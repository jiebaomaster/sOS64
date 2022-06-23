#include "mm.h"
#include "printk.h"

struct Global_Memory_Descriptor memory_management_struct = {{0}, 0};

/**
 * @brief 初始化目标物理页的 page
 *
 * @param page 目标物理页
 * @param flags page.attribute
 * @return 0=失败，1=成功
 */
unsigned long page_init(struct Page *page, unsigned long flags) {
  page->attribute |= flags;

  if (!page->reference_count || (page->attribute & PG_Shared)) {
    page->reference_count++;
    page->zone_struct->total_pages_link++;
  }

  return 1;
}


unsigned long page_clean(struct Page *page) {
  page->reference_count--;
  page->zone_struct->total_pages_link--;

  // 引用归零，重置页属性
  if (!page->reference_count) {
    page->attribute &= PG_PTable_Maped;
  }

  return 1;
}

unsigned long get_page_attribute(struct Page *page) {
  if (page == NULL) {
    printk_error("get_page_attribute => ERROR: page == NULL\n");
    return 0;
  } else
    return page->attribute;
}

unsigned long set_page_attribute(struct Page *page, unsigned long flags) {
  if (page == NULL) {
    printk_error("set_page_attribute => ERROR: page == NULL\n");
    return 0;
  } else {
    page->attribute = flags;
    return 1;
  }
}

void init_memory() {
  int i, j;
  unsigned long TotalMem = 0; // 可用物理内存长度，单位字节
  unsigned long phymmend;     // 物理内存空间的结束地址
  // 在 loader 中由 BIOS 中断获取的物理内存空间信息被存储在线性地址
  // 0xffff800000007e00 处
#if UEFI
  struct E820 *p = boot_para_info->E820_Info.E820_Entry;
  int E820_Entry_count = boot_para_info->E820_Info.E820_Entry_count;
#else
  struct E820 *p = (struct E820 *)0xffff800000007e00;
  int E820_Entry_count = 32;
#endif

  printk("Display Physics Address MAP,Type(1:RAM,2:ROM or Reserved,3:ACPI "
         "Reclaim Memory,4:ACPI NVS Memory,Others:Undefine)\n");
  
  // 解析物理内存空间信息，保存到全局内存管理结构体中
  for (i = 0; i < E820_Entry_count; i++) {
    printk("Address:%#018lx\tLength:%#018lx\tType:%#010x\n", p->address,
           p->length, p->type);
    if (p->type == 1) // 统计可用物理内存长度
      TotalMem += p->length;
    // 保存当前物理内存段的信息
    memory_management_struct.e820[i].address += p->address;
    memory_management_struct.e820[i].length += p->length;
    memory_management_struct.e820[i].type = p->type;
    memory_management_struct.e820_length = i; // 更新内存空间数组长度

    p++;
    // type 1~4 有效，第一条无效的信息之后的信息都是无效的，跳出循环
    if (p->type > 4 || p->type < 1 || p->length == 0)
      break;
  }

  printk("OS Can Used Total RAM:%#018lx\n", TotalMem);

  TotalMem = 0;
  // 计算可用内存空间的物理页数
  for (i = 0; i <= memory_management_struct.e820_length; i++) {
    unsigned long start, end; // 可用内存地址范围，左闭右开
    if (memory_management_struct.e820[i].type != 1) // 只计算可用内存空间
      continue;

    // 起始地址对齐到 2M
    start = PAGE_2M_ALIGN(memory_management_struct.e820[i].address);
    // 终止地址对齐到 2M
    end = (memory_management_struct.e820[i].address +
           memory_management_struct.e820[i].length) &
          PAGE_2M_MASK;

    if (end <= start) // 该段空间不到一页
      continue;

    TotalMem += (end - start) >> PAGE_2M_SHIFT; // 累加可用内存页数
  }

  printk("OS Can Used Total 2M PAGEs:%#010x=%010d\n", TotalMem, TotalMem);

  // 物理内存空间的结束地址
  phymmend = memory_management_struct.e820[memory_management_struct.e820_length]
                 .address +
             memory_management_struct.e820[memory_management_struct.e820_length]
                 .length;

  /**
   * 下面开始给内存管理项的结构分配空间并初始化
   *
   * 物理内存空间布局：
   * | system | bits_map | pages | zones | blank | 任意使用的空间... |
   * 0        ^          ^       ^        预留空间              phymmend
   * ^ 表示对齐到 4KB
   *
   * 本系统采用 2MB 的页表，内存已知为 2GB，每个 struct page 为 40B
   * 则全局 struct page 数组占用的空间为 (4GB / 2MB) * 40B = 80KB
   * 而物理内存开始的 10MB 都被页表映射过了，所以初始化的过程中不会发生缺页中断
   */

  /* 初始化页映射位图 */

  // 页映射位图放置在内核程序的结束地址的对齐到 4KB 的上边界处
  memory_management_struct.bits_map =
      (unsigned long *)PAGE_4K_ALIGN(memory_management_struct.end_brk);
  // 计算物理页的数量，0~phymmend 中包含所有类型的物理内存段，都按 2M 划分
  memory_management_struct.bits_size = phymmend >> PAGE_2M_SHIFT;
  // 计算全局页映射位图所占内存大小，单位字节，对齐到 8 字节
  memory_management_struct.bits_length =
      (((unsigned long)(phymmend >> PAGE_2M_SHIFT) + sizeof(long) * 8 - 1) /
       8) &
      (~(sizeof(long) - 1));
  // 将页映射位图的所有位都置位，后面再将可用物理内存页对应的复位，这样就可以排除其他物理内存段
  memset(memory_management_struct.bits_map, 0xff,
         memory_management_struct.bits_length);

  /* 初始化 struct page 结构体数组 */

  // struct page 结构体数组放置在页映射位图之后
  memory_management_struct.pages_struct = (struct Page *)PAGE_4K_ALIGN(
      (unsigned long)memory_management_struct.bits_map +
      memory_management_struct.bits_length);
  // 计算物理页的数量，0~phymmend 中包含所有类型的物理内存段，都按 2M 划分
  memory_management_struct.pages_size = phymmend >> PAGE_2M_SHIFT;
  // 计算全局 struct page 结构体数组所占内存大小，单位字节，对齐到 8 字节
  memory_management_struct.pages_length =
      ((phymmend >> PAGE_2M_SHIFT) * sizeof(struct Page) + sizeof(long) - 1) &
      (~(sizeof(long) - 1));
  // 清零 struct page 结构体数组
  memset(memory_management_struct.pages_struct, 0x00,
         memory_management_struct.pages_length);

  /* 初始化 struct zone 结构体数组 */

  // struct zone 结构体数组放置在 struct page 结构体数组之后
  memory_management_struct.zones_struct = (struct Zone *)PAGE_4K_ALIGN(
      (unsigned long)memory_management_struct.pages_struct +
      memory_management_struct.pages_length);
  // struct zone 结构体数组的元素个数暂时不能确定
  memory_management_struct.zones_size = 0;
  // 分配内存空间时假设有 10 个zone
  memory_management_struct.zones_length =
      (10 * sizeof(struct Zone) + sizeof(long) - 1) & (~(sizeof(long) - 1));
  // 清零 struct zone 结构体数组
  memset(memory_management_struct.zones_struct, 0x00,
         memory_management_struct.zones_length);

  // 遍历所有的物理内存段，初始化 zone->page->bitmap
  for (i = 0; i <= memory_management_struct.e820_length; i++) {
    unsigned long start, end;
    struct Zone *z;
    struct Page *p;
    unsigned long *b;

    if (memory_management_struct.e820[i].type != 1)
      continue;

    start = PAGE_2M_ALIGN(memory_management_struct.e820[i].address);
    end = (memory_management_struct.e820[i].address +
           memory_management_struct.e820[i].length) &
          PAGE_2M_MASK;
    if (end <= start)
      continue;

    // 每个可用物理内存区域的地址边界对齐到 2M，就是当前 zone 的范围
    // 初始化该可用物理内存区域的管理结构体 zone
    z = memory_management_struct.zones_struct +
        memory_management_struct.zones_size;
    memory_management_struct.zones_size++; // 更新 zone 的数量

    z->zone_start_address = start;
    z->zone_end_address = end;
    z->zone_length = end - start;

    z->page_using_count = 0;
    z->page_free_count = (end - start) >> PAGE_2M_SHIFT;

    z->total_pages_link = 0;

    z->attribute = 0;
    z->GMD_struct = &memory_management_struct;

    z->pages_length = (end - start) >> PAGE_2M_SHIFT;
    z->pages_group = (struct Page *)(memory_management_struct.pages_struct +
                                     (start >> PAGE_2M_SHIFT));

    // 初始化当前 zone 中的所有物理页的管理结构体 page
    p = z->pages_group;
    for (j = 0; j < z->pages_length; j++, p++) {
      p->zone_struct = z;
      p->PHY_address = start + PAGE_2M_SIZE * j;
      p->attribute = 0;

      p->reference_count = 0;

      p->age = 0;
      // 页映射位图中相应的位复位，表示这一页是空闲可用的
      *(memory_management_struct.bits_map +
        ((p->PHY_address >> PAGE_2M_SHIFT) >> 6)) ^=
          1UL << (p->PHY_address >> PAGE_2M_SHIFT) % 64;
    }
  }

  // 物理内存空间的第一个页，管理0~2M，其中包括了多个物理内存段，且包含了内核代码。但他不属于
  // 任何一个 zone。特殊初始化这一页，使 zone 指针指向第一个 zone
  memory_management_struct.pages_struct->zone_struct =
      memory_management_struct.zones_struct;
  memory_management_struct.pages_struct->PHY_address = 0UL;
  set_page_attribute(memory_management_struct.pages_struct, PG_PTable_Maped | PG_Kernel_Init | PG_Kernel);
  memory_management_struct.pages_struct->reference_count = 1;
  memory_management_struct.pages_struct->age = 0;

  // 自此，所有的 zone 都已经被初始化了，zone 的数量已经确定
  // 更新全局 struct zone 结构体数组所占的内存大小，单位字节
  memory_management_struct.zones_length =
      (memory_management_struct.zones_size * sizeof(struct Zone) +
       sizeof(long) - 1) &
      (~(sizeof(long) - 1));

  // 输出 bitmap，page，zone 的统计信息
  printk("bits_map:%#018lx,bits_size:%#018lx,bits_length:%#018lx\n",
         memory_management_struct.bits_map, memory_management_struct.bits_size,
         memory_management_struct.bits_length);
  printk("pages_struct:%#018lx,pages_size:%#018lx,pages_length:%#018lx\n",
         memory_management_struct.pages_struct,
         memory_management_struct.pages_size,
         memory_management_struct.pages_length);
  printk("zones_struct:%#018lx,zones_size:%#018lx,zones_length:%#018lx\n",
         memory_management_struct.zones_struct,
         memory_management_struct.zones_size,
         memory_management_struct.zones_length);

  // 初始化全局 zone 索引
  ZONE_DMA_INDEX = 0;
  ZONE_NORMAL_INDEX = 0;
  ZONE_UNMAPED_INDEX = 0;
  // 遍历输出每一个zone的信息，并设置 ZONE_UNMAPED_INDEX
  for (i = 0; i < memory_management_struct.zones_size; i++) {
    struct Zone *z = memory_management_struct.zones_struct + i;
    printk("zone_start_address:%#018lx,zone_end_address:%#018lx,"
           "zone_length:%#018lx,pages_group:%#018lx,pages_length:%#018lx\n",
           z->zone_start_address, z->zone_end_address, z->zone_length,
           z->pages_group, z->pages_length);

    // 起始地址大于 4GB 的第一个 zone 是第一个未被页表映射的
    if (z->zone_start_address >= 0x100000000 && !ZONE_UNMAPED_INDEX)
      ZONE_UNMAPED_INDEX = i;
  }
  printk("ZONE_DMA_INDEX:%d\t ZONE_NORMAL_INDEX:%d\t ZONE_UNMAPED_INDEX:%d\n",
         ZONE_DMA_INDEX, ZONE_NORMAL_INDEX, ZONE_UNMAPED_INDEX);

  // 记录所有内存管理结构结束地址，从这之后的物理内存可以任意使用
  // 计算时 “+ sizeof(long) * 32” 预留了一段内存空间，防止越界访问
  memory_management_struct.end_of_struct =
      (unsigned long)((unsigned long)memory_management_struct.zones_struct +
                      memory_management_struct.zones_length +
                      sizeof(long) * 32) &
      (~(sizeof(long) - 1));

  printk("start_code:%#018lx,end_code:%#018lx,end_data:%#018lx,end_brk:%#018lx,"
         "end_of_struct:%#018lx\n",
         memory_management_struct.start_code, memory_management_struct.end_code,
         memory_management_struct.end_data, memory_management_struct.end_brk,
         memory_management_struct.end_of_struct);

  // 内核页面和内存管理页面 加起来的数量
  i = Virt_To_Phy(memory_management_struct.end_of_struct) >> PAGE_2M_SHIFT;
  // 初始化内存管理页面的页面属性
  for (j = 1; j <= i; j++) {
    struct Page *tmp_page = memory_management_struct.pages_struct + j;
    page_init(tmp_page, PG_PTable_Maped | PG_Kernel_Init | PG_Kernel);
    *(memory_management_struct.bits_map +
      ((tmp_page->PHY_address >> PAGE_2M_SHIFT) >> 6)) |=
        1UL << (tmp_page->PHY_address >> PAGE_2M_SHIFT) % 64;
    tmp_page->zone_struct->page_using_count++;
    tmp_page->zone_struct->page_free_count--;
  }

  // 输出1，2，3级页表地址
  Global_CR3 = Get_gdt();
  printk("Global_CR3\t:%#018lx\n", Global_CR3);
  printk("*Global_CR3\t:%#018lx\n", *Phy_To_Virt(Global_CR3) & (~0xff));
  printk("**Global_CR3\t:%#018lx\n",
         *Phy_To_Virt(*Phy_To_Virt(Global_CR3) & (~0xff)) & (~0xff));

  printk_info("1.memory_management_struct.bits_map:%#018lx\tzone_struct->page_"
              "using_count:%d\tzone_struct->page_free_count:%d\n",
              *memory_management_struct.bits_map,
              memory_management_struct.zones_struct->page_using_count,
              memory_management_struct.zones_struct->page_free_count);

  /**
   * 清除一致性页表映射，即页表中头几个物理地址 = 虚拟地址的页表项
   * 低端地址后面会映射给用户空间，内存初始化完毕后也不需要再保留一致性页表映射了
   * AP 的启动在 mm init 之后，仍然需要一致性映射
   */
  // for(i = 0; i < 10; i++) 
  //   *(Phy_To_Virt(Global_CR3) + i)= 0UL;
  // 刷新tlb，使页表项修改生效
  flush_tlb();
}

/**
 * @brief 从 zone_select 指定的物理内存区域中分配连续的 number 个物理页
 *
 * @param zone_select 指定可用的物理内存区域
 * @param number 需求的页数，最多一次分配 63 个物理页
 * @param page_flags 分配物理页后设置的页属性
 * @return struct Page* 返回第一页的 page 结构体地址
 */
struct Page *alloc_pages(int zone_select, int number,
                         unsigned long page_flags) {
  int i;
  unsigned long page = 0;
  unsigned long attribute = 0;

  int zone_start = 0;
  int zone_end = 0;

  if (number >= 64 || number <= 0) {
    printk_error("alloc_pages => ERROR: number is invalid\n");
    return NULL;
  }

  // 设置备选的物理内存区域 zone_start ~ zone_end
  switch (zone_select) {
  case ZONE_DMA:
    zone_start = 0;
    zone_end = ZONE_DMA_INDEX;
    attribute = PG_PTable_Maped;
    break;

  case ZONE_NORMAL:
    zone_start = ZONE_DMA_INDEX;
    zone_end = ZONE_NORMAL_INDEX;
    attribute = PG_PTable_Maped;
    break;

  case ZONE_UNMAPED:
    zone_start = ZONE_UNMAPED_INDEX;
    zone_end = memory_management_struct.zones_size - 1;
    attribute = 0;
    break;

  default:
    printk_error("alloc_pages => ERROR: zone_select index is invalid\n");
    return NULL;
    break;
  }

  // 遍历所有备选的 zone，从某一个 zone 中分配所有的物理页
  for (i = zone_start; i <= zone_end; i++) {
    struct Zone *z;
    unsigned long j;
    unsigned long start, end;
    unsigned long tmp;

    // 若当前 zone 中剩余的可用物理页数不能满足需求，跳转到下一个 zone
    if ((memory_management_struct.zones_struct + i)->page_free_count < number)
      continue;

    z = memory_management_struct.zones_struct + i;
    start = z->zone_start_address >> PAGE_2M_SHIFT;
    end = z->zone_end_address >> PAGE_2M_SHIFT;

    // start % 64 = 该 zone 的第一个 bit 在 bitsmap 中以 64 为步进单位的偏移
    // tmp 为下面外循环第一次遍历的位数，可使后续的循环都对齐到 64，方便以64步进
    tmp = 64 - start % 64;
    // 遍历bitsmap中的zone区域，每次外循环遍历一个 sizeof(unsigned long)=64位
    for (j = start; j <= end; j += j % 64 ? tmp : 64) {
      unsigned long *p = memory_management_struct.bits_map + (j >> 6); // 当前遍历到第几个64
      unsigned long shift = j % 64;

      unsigned long num = (1UL << number) - 1;
      unsigned long k;
      // 遍历当前步进长度中的每一个bit，直到从某个 bit 开始可以满足需求
      for (k = shift; k < 64; k++) {
        /**
         * 1. 相邻 64 位步进单元的拼接
         * bitsmap 中的每一个 64 从低位开始占用
         * 将第一个64的高位拼上第二个64的低位，组成一个64位的字符，保证一次检索最多64位
         * [    *p >> k    |  *(p + 1) << (64 - k) ]
         * [    *p >> k    |**********************]
         *                        *p 左移 k 位
         * [***************|  *(p + 1) << (64 - k) ]
         *  *(p+1) 右移 64-k 位
         * 
         * 2. (1UL << number) - 1 将 64 位单元的低 number 位置位
         * 
         * 3. (1 & 2) 若等于0，表示低 number 位全部为空，可以占用 
         */
        if (!((k ? ((*p >> k) | (*(p + 1) << (64 - k))) : *p) & (num))) {
          unsigned long l;
          page = j + k - shift;
          for (l = 0; l < number; l++) { // 初始化每一个空闲的 page
            struct Page *curPage =
                memory_management_struct.pages_struct + page + l;
            *(memory_management_struct.bits_map +
              ((curPage->PHY_address >> PAGE_2M_SHIFT) >> 6)) |=
                1UL << (curPage->PHY_address >> PAGE_2M_SHIFT) % 64;
            z->page_free_count--;
            z->page_using_count++;
            curPage->attribute = attribute;
          }

          goto find_free_pages;
        }
      }
    }

    printk_warn("alloc_pages => no page can alloc\n");
    return NULL;

  find_free_pages:
    return (struct Page *)(memory_management_struct.pages_struct + page);
  }
}

/**
 * @brief 释放从 page 开始的 number 个物理页
 */
void free_pages(struct Page *page, int number) {
  if (!page) {
    printk_error("free_pages => ERROR: page is invalid\n");
    return;
  }

  if (number >= 64 || number <= 0) {
    printk_error("free_pages => ERROR: number is invalid\n");
    return;
  }
  int i;
  for (i = 0; i < number; i++, page++) {
    *(memory_management_struct.bits_map +
      ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) &=
        ~(1UL << (page->PHY_address >> PAGE_2M_SHIFT) % 64);
    page->zone_struct->page_free_count++;
    page->zone_struct->page_using_count--;
    page->attribute = 0;
  }
}

/**
 * @brief 为 kmalloc 使用的内存池申请新的 Slab
 * 
 * @param size 
 * @return struct Slab* 
 */
struct Slab *kmalloc_create(unsigned long size) {
  int i;
  struct Slab *pSlab = NULL;
  struct Page *page = NULL;
  unsigned long *page_address = NULL;
  long slabAndMapSize = 0;

  switch (size) {
  case 32:
  case 64:
  case 128:
  case 256:
  case 512:
    /**
     * 分配空间存储 32～512B 的小尺寸对象，这些对象的内存尺寸较小而位图占用空间大，
     * 将 Slab、位图、数据存储区都放在一个页面中，避免额外分配空间（嵌套调用 kmalloc)
     * 保证只要有可用物理页，即可分配出内存对象
     * 
     * 页面：低地址 -> 高地址
     * [  对象数据存储区   |  struct Slab | bitmap ]
     */
    page = alloc_pages(ZONE_NORMAL, 1, 0);
    if (!page) {
      printk_error("kmalloc_create => alloc_pages failed!\n");
      return NULL;
    }
    page_init(page, PG_Kernel);

    page_address = Phy_To_Virt(page->PHY_address);
    slabAndMapSize = sizeof(struct Slab) + PAGE_2M_SIZE / size / 8;

    pSlab = (struct Slab *)((unsigned char *)page_address + PAGE_2M_SIZE -
                            slabAndMapSize);
    pSlab->color_map =
        (unsigned long *)((unsigned char *)pSlab + sizeof(struct Slab));
    pSlab->free_count =
        (PAGE_2M_SIZE - (PAGE_2M_SIZE / size / 8) - sizeof(struct Slab)) / size;
    pSlab->color_length =
        ((pSlab->color_count + sizeof(unsigned long) * 8 - 1) >> 6) << 3;
    pSlab->using_count = 0;
    pSlab->color_count = pSlab->free_count;
    memset(pSlab->color_map, 0xff, pSlab->color_length);
    for (i = 0; i < pSlab->color_count; i++)
      *(pSlab->color_map + (i >> 6)) ^= 1UL << i % 64;

    pSlab->Vaddress = page_address;
    pSlab->page = page;
    list_init(&pSlab->list);

    break;
  case 1024: // 1KB
  case 2048:
  case 4096:
  case 8192:
  case 16384:
  case 32768:
  case 131072: // 128KB
  case 262144:
  case 524288:
  case 1048576: // 1MB
    // 大型对象使用 kmalloc 递归申请 Slab 和 位图 的内存
    pSlab = get_new_Slab(size);
    if (!pSlab) {
      printk_error("kmalloc_create => get_new_Slab failed!\n");
      return NULL;
    }
    break;
  default:
    printk_error("kmalloc_create => ERROR: wrong size:%08d\n", size);
    return NULL;
    break;
  }

  return pSlab;
}

/**
 * @brief 基于内存池的通用内存申请
 *
 * @param size 申请的内存大小
 * @param gfp_flags 控制内存分配
 * @return void* 申请到内存的起始虚拟地址
 */
void *kmalloc(unsigned long size, unsigned long gfp_flags) {
  // 申请内存大小不能超过 1MB
  if (size > 1048576) {
    printk_error("kmalloc => ERROR: kmalloc size too big:%08d\n", size);
    return NULL;
  }

  // 1. size 向上取整，找到可以适合的内存池
  struct Slab *pSlab = NULL;
  int i;
  for (i = 0; i < 16; i++) {
    if (kmalloc_cache_size[i].size >= size) {
      pSlab = kmalloc_cache_size[i].cache_pool;
      break;
    }
  }
  
  // 2. 在内存池中寻找合适的 Slab
  if (kmalloc_cache_size[i].total_free != 0) {
    // 内存池中还有空闲，遍历找到还有空闲的 Slab
    do {
      if (pSlab->free_count == 0) {
        pSlab = container_of(list_next(&pSlab->list), struct Slab, list);
        continue;
      }
      break;
    } while (pSlab != kmalloc_cache_size[i].cache_pool);
  } else {
    // 内存池中没有空闲了，申请新的 Slab
    pSlab = kmalloc_create(kmalloc_cache_size[i].size);
    if (pSlab == NULL) {
      printk_warn("kmalloc => kmalloc_create get Null!\n");
      return NULL;
    }

    kmalloc_cache_size[i].total_free += pSlab->color_count;
    printk("kmalloc => kmalloc_create size:%#010x\n",
           kmalloc_cache_size[i].size);
    // 链接进内存池的 Slab 链表
    list_add_to_before(&kmalloc_cache_size[i].cache_pool->list, &pSlab->list);
  }

  // 3. 在 slab 中进行分配
  void *obj = __do_slab_malloc(&kmalloc_cache_size[i], pSlab, NULL);
  if (obj)
    return obj;

  printk_error("kmalloc => ERROR: no memory can alloc!\n");
  return NULL;
}

/**
 * @brief 释放内存
 *
 * @param address 待释放内存的起始地址
 * @return 0=失败，1=成功
 */
unsigned long kfree(void *address) {
  int i;
  struct Slab * pSlab = NULL;
  unsigned long page_base_addr = (unsigned long)address & PAGE_2M_MASK;
  for(i = 0; i < 16; i++) {
    pSlab = kmalloc_cache_size[i].cache_pool;
    do {
      // 找到 page_base_addr 地址所在的 Slab
      if ((unsigned long)pSlab->Vaddress != page_base_addr) {
        pSlab = container_of(list_next(&pSlab->list), struct Slab, list);
        continue;
      }

      // 1. 复位位图中对应位
      int index = (address - pSlab->Vaddress) / kmalloc_cache_size[i].size;
      *(pSlab->color_map + (index >> 6)) ^= 1UL << (index % 64);
      
      // 2. 维护计数器
      pSlab->free_count++;
      pSlab->using_count--;
      kmalloc_cache_size[i].total_free++;
      kmalloc_cache_size[i].total_using--;

      // 3. 若 Slab 已空闲 且 内存池中剩余可分配内存充足，释放 Slab
      // 条件二 空闲内存数量超过一个 Slab 的 1.5 倍，也确保了内存池中至少有 2 个 Slab
      // 条件三 保证第一个 Slab 不被删除，因为第一个 Slab 是静态申请的
      if ((pSlab->using_count == 0) &&
          (kmalloc_cache_size[i].total_free >= pSlab->color_count * 3 / 2) &&
          (pSlab != kmalloc_cache_size[i].cache_pool)) {
        switch (kmalloc_cache_size[i].size) {
        case 32:
        case 64:
        case 128:
        case 256:
        case 512:
          // 小型对象的 Slab、位图、数据存储区都放在一个页面中，只需释放 page 即可
          list_del(&pSlab->list);
          kmalloc_cache_size[i].total_free -= pSlab->color_count;

          page_clean(pSlab->page);
          free_pages(pSlab->page, 1);
          break;
        default:
          // 大型对象使用 kfree 递归释放
          kmalloc_cache_size[i].total_free -= pSlab->color_count;
          __do_Slab_destory(pSlab);
          break;
        }
      }
      return 1;
    } while (pSlab != kmalloc_cache_size[i].cache_pool);
  }
  
  printk_error("kfree => ERROR: cant't free memory:%#018lx!\n", address);
  return 0;
}

/**
 * 将所有物理页全部映射到线性地址空间内
 */
void pagetable_init() {
  unsigned long * tmp = NULL;
  Global_CR3 = Get_gdt();

  // 打印当前系统中各级页表的起始地址（虚拟地址，物理地址）
  tmp = (unsigned long *)(((unsigned long)Phy_To_Virt(
                              (unsigned long)Global_CR3 & (~0xfffUL))) +
                          8 * 256);
  // printk("1:%#018lx,%#018lx\t\t\n", (unsigned long)tmp, *tmp);
  tmp = Phy_To_Virt(*tmp & (~0xfffUL));
  // printk("2:%#018lx,%#018lx\t\t\n", (unsigned long)tmp, *tmp);
  tmp = Phy_To_Virt(*tmp & (~0xfffUL));
  // printk("3:%#018lx,%#018lx\t\t\n", (unsigned long)tmp, *tmp);

  // 把 normal zone 中的所有物理页全部映射到线性地址空间内，
  // 则内核层可以访问到全部物理页，即内核层不会发生缺页中断

  // 遍历所有 zone
  unsigned long i, j;
  for(i = 0; i < memory_management_struct.zones_size; i++) {
    if(ZONE_UNMAPED_INDEX && i == ZONE_UNMAPED_INDEX) break;

    struct Zone* z = memory_management_struct.zones_struct + i;
    struct Page* p = z->pages_group;

    // 遍历 zone 下的所有 page
    for(j = 0; j < z->pages_length; j++, p++) {
      // tmp 指向 PML4T 中 page 对应的页表项
      tmp = (unsigned long*)(((unsigned long)Phy_To_Virt((unsigned long)Global_CR3 & (~0xfffUL))) + (((unsigned long)Phy_To_Virt(p->PHY_address) >> PAGE_GDT_SHIFT) & 0x1ff) * 8);
      if(*tmp == 0) { // 页表项为空，分配下级页表 PDPT，并设置 PML4T 中对应的页表项
        unsigned long * newPDPT = kmalloc(PAGE_4K_SIZE, 0);
        set_mpl4t(tmp, mk_mpl4t(Virt_To_Phy(newPDPT), PAGE_KERNEL_GDT));
      }

      // tmp 指向 PDPT 中 page 对应的页表项
      tmp = (unsigned long *)((unsigned long)Phy_To_Virt(*tmp & (~ 0xfffUL)) + (((unsigned long)Phy_To_Virt(p->PHY_address) >> PAGE_1G_SHIFT) & 0x1ff) * 8);
      if (*tmp == 0) {
        unsigned long *newPDT = kmalloc(PAGE_4K_SIZE, 0);
        set_pdpt(tmp, mk_pdpt(Virt_To_Phy(newPDT), PAGE_KERNEL_Dir));
      }

      // tmp 指向 PDT 中 page 对应的页表项
      tmp = (unsigned long *)((unsigned long)Phy_To_Virt(*tmp & (~ 0xfffUL)) + (((unsigned long)Phy_To_Virt(p->PHY_address) >> PAGE_2M_SHIFT) & 0x1ff) * 8);
      // 设置最下级的页表项
      set_pdt(tmp, mk_pdt(p->PHY_address, PAGE_KERNEL_Page));

      // debug 信息
      // if (j % 50 == 0)
      // printk_debug("@:%#018lx, %#018lx\t\n", (unsigned long)tmp, *tmp);
    }
  }
  
  flush_tlb();
}