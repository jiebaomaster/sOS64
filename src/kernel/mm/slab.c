#include "mm.h"
#include "printk.h"

/**
 * @brief 使用 kmalloc 申请一块新的 Slab 并初始化
 */
struct Slab *get_new_Slab(unsigned long size) {
  // malloc slab
  struct Slab *slab = (struct Slab *)kmalloc(sizeof(struct Slab), 0);
  if (!slab) {
    printk_error("get_new_Slab => kmalloc <Slab> failed!\n");
    goto out;
  }
  memset(slab, 0, sizeof(struct Slab));

  // init slab
  list_init(&slab->list);
  slab->page = alloc_pages(ZONE_NORMAL, 1, 0);
  if (!slab->page) {
    printk_error("get_new_Slab => alloc_pages failed!\n");
    goto slab_out;
  }
  page_init(slab->page, PG_Kernel);

  slab->using_count = 0;
  slab->free_count = PAGE_2M_SIZE / size;
  slab->Vaddress = Phy_To_Virt(slab->page->PHY_address);
  slab->color_count = slab->free_count;
  slab->color_length =
      ((slab->color_count + sizeof(unsigned long) * 8 - 1) >> 6) << 3;
  // malloc bitmap
  slab->color_map = (unsigned long *)kmalloc(slab->color_length, 0);
  if (!slab->color_map) {
    printk_error("get_new_Slab => kmalloc <color_map> failed!\n");
    goto page_out;
  }
  memset(slab->color_map, 0xff, slab->color_length);
  for (int i = 0; i < slab->color_count; i++)
    *(slab->color_map + (i >> 6)) ^= 1UL << i % 64;

  return slab;

page_out:
  free_pages(slab->page, 1);
slab_out:
  kfree(slab);
out:
  return NULL;
}

/**
 * @brief 创建一个管理对象大小为 size 的内存池
 *
 * @param size 管理对象的大小
 * @param constructor 对象的构造函数
 * @param destructor 对象的析构函数
 * @return struct Slab_cache* 内存池地址
 */
struct Slab_cache *
slab_create(unsigned long size,
            void *(*constructor)(void *Vaddress, unsigned long arg),
            void *(*destructor)(void *Vaddress, unsigned long arg)) {
  struct Slab_cache *slab_cache = NULL;
  slab_cache = (struct Slab_cache *)kmalloc(sizeof(struct Slab_cache), 0);
  if (!slab_cache) {
    printk_error("slab_create => kmalloc <Slab_cache> failed!\n");
    goto out;
  }
  memset(slab_cache, 0, sizeof(struct Slab_cache));

  // init slab_cache
  slab_cache->size = SIZEOF_LONG_ALIGN(size);
  slab_cache->total_using = 0;
  slab_cache->total_free = 0;
  slab_cache->cache_dma_pool = NULL;
  slab_cache->constructor = constructor;
  slab_cache->destructor = destructor;

  slab_cache->cache_pool = get_new_Slab(slab_cache->size);
  if (!slab_cache->cache_pool) {
    printk_error("slab_create => get_new_Slab failed!\n");
    goto slab_cache_out;
  }
  slab_cache->total_free = slab_cache->cache_pool->free_count;

  return slab_cache;

slab_cache_out:
  kfree(slab_cache);
out:
  return NULL;
}

/**
 * @brief 释放一个 Slab，使用 kfree 释放 Slab 和 位图
 */
void __do_Slab_destory(struct Slab *slab) {
  list_del(&slab->list);  // 移除本项
  kfree(slab->color_map); // 释放位图
  page_clean(slab->page);
  free_pages(slab->page, 1); // 释放数据区
  kfree(slab);               // 释放 slab 本身
}

/**
 * @brief 销毁 slab_cache 内存池
 * @return 0=失败，1=成功
 */
unsigned long slab_destory(struct Slab_cache *slab_cache) {
  // 非空的内存池不能释放
  if (slab_cache->total_using != 0) {
    printk_error("slab_destory => slab_cache not empty!\n");
    return 0;
  }
  // 释放内存池中的所有 Slab
  struct Slab *pslab = slab_cache->cache_pool;
  struct Slab *tmp = NULL;
  while (!list_is_empty(&pslab->list)) {
    tmp = pslab;
    // pslab 指向链表的下一项
    pslab = container_of(list_next(&pslab->list), struct Slab, list);
    __do_Slab_destory(tmp);
  }
  // 移除链表中第一个 Slab
  kfree(pslab->color_map);
  page_clean(pslab->page);
  free_pages(pslab->page, 1);
  kfree(pslab);
  // 释放 Slab_cache
  kfree(slab_cache);
  return 1;
}

/**
 * @brief 在 slab_cache 中的 slab 中分配一个对象
 * 
 * @param arg 对象构造函数的参数
 * @return void* 初始化后的对象
 */
void *__do_slab_malloc(struct Slab_cache *slab_cache, struct Slab *slab,
                       unsigned long arg) {
  for (int j = 0; j < slab->color_count; j++) {
    // 这一个 64 位 全都是 1，直接跳到下一个，加速比较
    if (*(slab->color_map + (j >> 6)) == 0xffffffffffffffffUL) {
      j += 63;
      continue;
    }
    // 找到第一个 0
    if ((*(slab->color_map + (j >> 6)) & (1UL << (j % 64))) == 0) {
      // 占用这一位
      *(slab->color_map + (j >> 6)) |= 1UL << (j % 64);

      slab->using_count++;
      slab->free_count--;

      slab_cache->total_using++;
      slab_cache->total_free--;

      if (slab_cache->constructor) { // 如果有构造函数就调用
        return slab_cache->constructor(
            (char *)slab->Vaddress + slab_cache->size * j, arg);
      } else {
        return (void *)((char *)slab->Vaddress + slab_cache->size * j);
      }
    }
  }
  return NULL;
}

/**
 * @brief 向 slab_cache 内存池申请一个对象
 *
 * @param arg 对象构造函数的参数
 * @return void* 初始化后的对象
 */
void *slab_malloc(struct Slab_cache *slab_cache, unsigned long arg) {
  struct Slab *pSlab = slab_cache->cache_pool;
  void *obj = NULL;

  if (slab_cache->total_free == 0) {
    // 内存池中没有空闲的块了，申请新的 Slab 并分配
    struct Slab *tmpSlab = get_new_Slab(slab_cache->size);
    if (!tmpSlab) {
      printk_error("slab_malloc => get_new_Slab failed!\n");
      return NULL;
    }

    list_add_to_behind(&slab_cache->cache_pool->list, &tmpSlab->list);
    slab_cache->total_free += tmpSlab->color_count;

    obj = __do_slab_malloc(slab_cache, tmpSlab, arg);
    if (obj)
      return obj;

    // 分配失败，释放新申请的 Slab
    __do_Slab_destory(tmpSlab);
  } else {
    // 内存池中还有空闲块，查找一个有空闲块的 Slab 并分配
    do {
      if (pSlab->free_count == 0) { // 跳过所有满的 Slab
        pSlab = container_of(list_next(&pSlab->list), struct Slab, list);
        continue;
      }
      obj = __do_slab_malloc(slab_cache, pSlab, arg);
      if (obj)
        return obj;
    } while (pSlab != slab_cache->cache_pool);
  }

  // 分配失败
  printk_error("slab_malloc => alloc ERROR!\n");
  return NULL;
}

/**
 * @brief 将 address 内存块归还到 slab_cache 内存池，并调用对象的析构函数
 * 
 * @return 0=失败，1=成功
 */
unsigned long slab_free(struct Slab_cache *slab_cache, void *address,
                        unsigned long arg){
  struct Slab* pSlab = slab_cache->cache_pool;
  int index;
  do {
    // 找到 address 地址所在的 Slab
    if (pSlab->Vaddress > address ||
        pSlab->Vaddress + PAGE_2M_SIZE <= address) {
      pSlab = container_of(list_next(&pSlab->list), struct Slab, list);
      continue;
    }

    // 1. 复位位图中对应位
    index = (address - pSlab->Vaddress) / slab_cache->size;
    *(pSlab->color_map + (index >> 6)) ^= 1UL << index % 64;

    // 2. 维护计数器
    pSlab->using_count--;
    pSlab->free_count++;
    slab_cache->total_using--;
    slab_cache->total_free++;

    // 3. 调用析构函数
    if (slab_cache->destructor) {
      slab_cache->destructor(address, arg);
    }

    // 4. 若 Slab 已空闲 且 内存池中剩余可分配内存充足，释放 Slab
    // 条件二 空闲内存数量超过一个 Slab 的 1.5 倍，也确保了内存池中至少有 2 个 Slab
    if (pSlab->using_count == 0 &&
        (slab_cache->total_free >= pSlab->color_count * 3 / 2)) {
      if (pSlab == slab_cache->cache_pool) {
        slab_cache->cache_pool = container_of(list_next(&pSlab->list), struct Slab, list);
      }
      slab_cache->total_free -= pSlab->color_count;
      __do_Slab_destory(pSlab);
    }
  } while (pSlab != slab_cache->cache_pool);

  // 没有找到 address 地址所在的 Slab，分配失败
  printk_error("slab_free => ERROR: address not in slab!\n");
  return 0;
}

/**
 * @brief 初始化 kmalloc 使用的内存池 kmalloc_cache_size
 * 
 * @return 0=失败，1=成功
 */
unsigned long kmalloc_slab_init() {
  struct Slab *pSlab = NULL;
  // 记录可用内存的起点
  unsigned long origin_end_of_struct = memory_management_struct.end_of_struct;

  ////////// 1. 从全局物理内存中分配空间给内存池的 Slab，并初始化
  for (int i = 0; i < 16; i++) {
    // 从全局物理内存的可用内存部分划一块分配给内存池存放 <struct Slab，bitmap>
    pSlab = (struct Slab *)memory_management_struct.end_of_struct;
    kmalloc_cache_size[i].cache_pool = pSlab;
    memory_management_struct.end_of_struct =
        memory_management_struct.end_of_struct + sizeof(struct Slab) +
        sizeof(long) * 10;
    // 初始化 Slab
    list_init(&pSlab->list);
    pSlab->using_count = 0;
    pSlab->free_count = PAGE_2M_SIZE / kmalloc_cache_size[i].size;
    pSlab->color_length = ((PAGE_2M_SIZE / kmalloc_cache_size[i].size +
                            sizeof(unsigned long) * 8 - 1) >> 6) << 3;
    pSlab->color_count = pSlab->free_count;
    pSlab->color_map = (unsigned long *)memory_management_struct.end_of_struct;
    memory_management_struct.end_of_struct =
        (unsigned long)(memory_management_struct.end_of_struct +
                        pSlab->color_length + sizeof(long) * 10) &
        (~(sizeof(long) - 1));
    memset(pSlab->color_map, 0xff, pSlab->color_length);
    for (int i = 0; i < pSlab->color_count; i++)
      *(pSlab->color_map + (i >> 6)) ^= 1UL << i % 64;

    kmalloc_cache_size[i].total_free = pSlab->color_count;
    kmalloc_cache_size[i].total_using = 0;
  }

  ////////// 2. 初始化 Slab 占用物理内存的 page
  unsigned long cur_end_of_struct =
      Virt_To_Phy(memory_management_struct.end_of_struct) >> PAGE_2M_SHIFT;
  struct Page *page = NULL;
  for (unsigned long i =
           PAGE_2M_ALIGN(Virt_To_Phy(origin_end_of_struct)) >> PAGE_2M_SHIFT;
       i <= cur_end_of_struct; i++) {
    page = memory_management_struct.pages_struct + i;
    // 在位图中占用指定的 page
    *(memory_management_struct.bits_map +
      ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) |=
        1UL << (page->PHY_address >> PAGE_2M_SHIFT) % 64;
    page->zone_struct->page_using_count++;
    page->zone_struct->page_free_count--;
    page_init(page, PG_PTable_Maped | PG_Kernel_Init | PG_Kernel);
  }

  printk_info(
      "2. memory_management_struct.bits_map:%#018lx\t zone_struct->page_"
      "using_count:%d\t zone_struct->page_free_count:%d\n",
      *memory_management_struct.bits_map,
      memory_management_struct.zones_struct->page_using_count,
      memory_management_struct.zones_struct->page_free_count);

  ////////// 3. 从全局物理内存中分配空间给内存池的 Slab 管理的数据存储区，并初始化
  unsigned long *cache_area = NULL;
  for (int i = 0; i < 16; i++) {
    cache_area = (unsigned long *)((memory_management_struct.end_of_struct +
                                    PAGE_2M_SIZE * i + PAGE_2M_SIZE - 1) &
                                   PAGE_2M_MASK);
    page = Virt_To_2M_Page(cache_area);

    *(memory_management_struct.bits_map +
      ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) |=
        1UL << (page->PHY_address >> PAGE_2M_SHIFT) % 64;
    page->zone_struct->page_using_count++;
    page->zone_struct->page_free_count--;

    page_init(page, PG_PTable_Maped | PG_Kernel_Init | PG_Kernel);

    kmalloc_cache_size[i].cache_pool->page = page;
    kmalloc_cache_size[i].cache_pool->Vaddress = cache_area;
  }

  printk_info(
      "3. memory_management_struct.bits_map:%#018lx\t zone_struct->page_"
      "using_count:%d\t zone_struct->page_free_count:%d\n",
      *memory_management_struct.bits_map,
      memory_management_struct.zones_struct->page_using_count,
      memory_management_struct.zones_struct->page_free_count);

  printk_info(
      "start_code:%#018lx, end_code:%#018lx, end_data:%#018lx, "
      "end_brk:%#018lx, "
      "end_of_struct:%#018lx\n",
      memory_management_struct.start_code, memory_management_struct.end_code,
      memory_management_struct.end_data, memory_management_struct.end_brk,
      memory_management_struct.end_of_struct);

  return 1;
}