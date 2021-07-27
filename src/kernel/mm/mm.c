#include "mm.h"
#include "lib.h"

struct Global_Memory_Descriptor memory_management_struct = {{0}, 0};

/**
 * @brief 初始化目标物理页的 page，并更新目标物理页所在 zone 的统计信息
 *
 * @param page 目标物理页
 * @param flags page.attribute
 * @return unsigned long
 */
unsigned long page_init(struct Page *page, unsigned long flags) {
  if (!page->attribute) { // 如果这个页面还没被初始化过
    // 页映射位图中相应的位置位，表示这一页已被占用
    *(memory_management_struct.bits_map +
      ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) |=
        1UL << (page->PHY_address >> PAGE_2M_SHIFT) % 64;
    page->attribute = flags;
    page->reference_count++;
    page->zone_struct->page_using_count++;
    page->zone_struct->page_free_count--;
    page->zone_struct->total_pages_link++;
  } else if ((page->attribute & PG_Referenced) ||
             (page->attribute & PG_K_Share_To_U) || (flags & PG_Referenced) ||
             (flags & PG_K_Share_To_U)) { // 设置一个已初始化的页面为共享的
    page->attribute |= flags;
    page->reference_count++;
    page->zone_struct->total_pages_link++;
  } else {
    *(memory_management_struct.bits_map +
      ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) |=
        1UL << (page->PHY_address >> PAGE_2M_SHIFT) % 64;
    page->attribute |= flags;
  }

  return 0;
}

void init_memory() {
  int i, j;
  unsigned long TotalMem = 0; // 可用物理内存长度，单位字节
  unsigned long phymmend;     // 物理内存空间的结束地址
  struct E820 *p = NULL;

  printk("Display Physics Address MAP,Type(1:RAM,2:ROM or Reserved,3:ACPI "
         "Reclaim Memory,4:ACPI NVS Memory,Others:Undefine)\n");
  // 在 loader 中由 BIOS 中断获取的物理内存空间信息被存储在线性地址
  // 0xffff800000007e00 处
  p = (struct E820 *)0xffff800000007e00;
  // 解析物理内存空间信息，保存到全局内存管理结构体中
  for (i = 0; i < 32; i++) {
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
  // 分配内存空间时假设有 5 个zone
  memory_management_struct.zones_length =
      (5 * sizeof(struct Zone) + sizeof(long) - 1) & (~(sizeof(long) - 1));
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
  memory_management_struct.pages_struct->attribute = 0;
  memory_management_struct.pages_struct->reference_count = 0;
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

  // TODO 全局zone索引需要在后期修正
  ZONE_DMA_INDEX = 0;
  ZONE_NORMAL_INDEX = 0;
  // TODO
  // 遍历输出每一个zone的信息，并设置 ZONE_UNMAPED_INDEX
  for (i = 0; i < memory_management_struct.zones_size; i++) {
    struct Zone *z = memory_management_struct.zones_struct + i;
    printk("zone_start_address:%#018lx,zone_end_address:%#018lx,"
           "zone_length:%#018lx,pages_group:%#018lx,pages_length:%#018lx\n",
           z->zone_start_address, z->zone_end_address, z->zone_length,
           z->pages_group, z->pages_length);

    // zone 的起始地址为 1G，该 zone 的物理内存页没有被页表映射
    if (z->zone_start_address == 0x100000000)
      ZONE_UNMAPED_INDEX = i;
  }

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
  // 初始化内核页面和内存管理页面的页面属性
  for (j = 0; j <= i; j++) {
    page_init(memory_management_struct.pages_struct + j,
              PG_PTable_Maped | PG_Kernel_Init | PG_Active | PG_Kernel);
  }

  // 输出1，2，3级页表地址
  Global_CR3 = Get_gdt();
  printk("Global_CR3\t:%#018lx\n", Global_CR3);
  printk("*Global_CR3\t:%#018lx\n", *Phy_To_Virt(Global_CR3) & (~0xff));
  printk("**Global_CR3\t:%#018lx\n",
         *Phy_To_Virt(*Phy_To_Virt(Global_CR3) & (~0xff)) & (~0xff));

  // 清除一致性页表映射，即页表中头几个物理地址 = 虚拟地址的页表项
  // 低端地址后面会映射给用户空间，内存初始化完毕后也不需要再保留一致性页表映射了
  *Phy_To_Virt(Global_CR3) = 0UL;
  // 刷新tlb，使页表项修改生效
  flush_tlb();
}

/**
 * @brief 从 zone_select 指定的物理内存区域中分配连续的 number 个物理页
 *
 * @param zone_select 指定可用的物理内存区域
 * @param number 需求的页数，最多一次分配 64 个物理页
 * @param page_flags 分配物理页后设置的页属性
 * @return struct Page* 返回第一页的 page 结构体地址
 */
struct Page *alloc_pages(int zone_select, int number,
                         unsigned long page_flags) {
  int i;
  unsigned long page = 0;

  int zone_start = 0;
  int zone_end = 0;

  // 设置备选的物理内存区域 zone_start ~ zone_end
  switch (zone_select) {
  case ZONE_DMA:
    zone_start = 0;
    zone_end = ZONE_DMA_INDEX;
    break;

  case ZONE_NORMAL:
    zone_start = ZONE_DMA_INDEX;
    zone_end = ZONE_NORMAL_INDEX;
    break;

  case ZONE_UNMAPED:
    zone_start = ZONE_UNMAPED_INDEX;
    zone_end = memory_management_struct.zones_size - 1;
    break;

  default:
    color_printk(RED, BLACK, "alloc_pages error zone_select index\n");
    return NULL;
    break;
  }

  // 遍历所有备选的 zone，从某一个 zone 中分配所有的物理页
  for (i = zone_start; i <= zone_end; i++) {
    struct Zone *z;
    unsigned long j;
    unsigned long start, end, length;
    unsigned long tmp;

    // 若当前 zone 中剩余的可用物理页数不能满足需求，跳转到下一个 zone
    if ((memory_management_struct.zones_struct + i)->page_free_count < number)
      continue;

    z = memory_management_struct.zones_struct + i;
    start = z->zone_start_address >> PAGE_2M_SHIFT;
    end = z->zone_end_address >> PAGE_2M_SHIFT;
    length = z->zone_length >> PAGE_2M_SHIFT;

    // start % 64 = 该 zone 的第一个 bit 在 bitsmap 中以 64 为步进单位的偏移
    // tmp 为下面外循环第一次遍历的位数，可使后续的循环都对齐到 64，方便以64步进
    tmp = 64 - start % 64;
    // 遍历bitsmap中的zone区域，每次外循环遍历一个 sizeof(unsigned long)=64位
    for (j = start; j <= end; j += j % 64 ? tmp : 64) {
      unsigned long *p = memory_management_struct.bits_map + (j >> 6); // 当前遍历到第几个64
      unsigned long shift = j % 64;
      unsigned long k;
      // 遍历当前步进长度中的每一个bit，直到从某个 bit 开始可以满足需求
      for (k = shift; k < 64 - shift; k++) {
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
        if (!(((*p >> k) | (*(p + 1) << (64 - k))) &
              (number == 64 ? 0xffffffffffffffffUL : ((1UL << number) - 1)))) {
          unsigned long l;
          page = j + k - 1;
          for (l = 0; l < number; l++) // 初始化每一个空闲的 page
            page_init(memory_management_struct.pages_struct + page + l,
                      page_flags);

          goto find_free_pages;
        }
      }
    }

    return NULL;

  find_free_pages:
    return (struct Page *)(memory_management_struct.pages_struct + page);
  }
}