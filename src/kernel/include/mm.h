#ifndef __MM_H__
#define __MM_H__

#include "lib.h"
#include "printk.h"

// 每个页表的页表项数目，每个页表4KB，每个页表项8B
#define PTRS_PER_PAGE 512
// 内核层的起始线性地址
#define PAGE_OFFSET ((unsigned long)0xffff800000000000)

#define PAGE_GDT_SHIFT 39
#define PAGE_1G_SHIFT 30
#define PAGE_2M_SHIFT 21
#define PAGE_4K_SHIFT 12

// 2M 页所占的字节数
#define PAGE_2M_SIZE (1UL << PAGE_2M_SHIFT)
// 4K 页所占的字节数
#define PAGE_4K_SIZE (1UL << PAGE_4K_SHIFT)

// 屏蔽低于 2M 的数值
#define PAGE_2M_MASK (~(PAGE_2M_SIZE - 1))
// 屏蔽低于 4K 的数值
#define PAGE_4K_MASK (~(PAGE_4K_SIZE - 1))

// 将地址 addr 对齐到 2M 的上边界
#define PAGE_2M_ALIGN(addr)                                                    \
  (((unsigned long)(addr) + PAGE_2M_SIZE - 1) & PAGE_2M_MASK)
// 将地址 addr 对齐到 4K 的上边界
#define PAGE_4K_ALIGN(addr)                                                    \
  (((unsigned long)(addr) + PAGE_4K_SIZE - 1) & PAGE_4K_MASK)

// 只有物理地址的前 10MB 被映射到线性地址0xffff800000000000处，也就只有这 10MB
// 的内存空间可以使用下面的宏 内核层，虚拟地址 => 物理地址
#define Virt_To_Phy(addr) ((unsigned long)(addr)-PAGE_OFFSET)
// 内核层，物理地址 => 虚拟地址
#define Phy_To_Virt(addr)                                                      \
  ((unsigned long *)((unsigned long)(addr) + PAGE_OFFSET))

// cr3 寄存器保存顶级页表地址
unsigned long *Global_CR3 = NULL;

/**
 * @brief 由 BIOS 中断 E820 获取的物理内存空间信息结构体
 * 物理内存空间信息保存在线性地址 0xffff800000007e00 处
 *
 * 物理内存空间的类型：
 * 1:RAM,
 * 2:ROM or Reserved,
 * 3:ACPI Reclaim Memory,
 * 4:ACPI NVS Memory,
 * Others:Undefine
 */
struct E820 {
  unsigned long address; // 起始地址
  unsigned long length;  // 内存空间长度
  unsigned int type;     // 内存类型
} __attribute__((packed));

/**
 * @brief 全局内存信息
 */
struct Global_Memory_Descriptor {
  struct E820 e820[32]; // BIOS 中断 E820
                        // 获取的物理内存空间信息数组，包含所有类型的内存空间
  unsigned long e820_length; // BIOS 中断 E820 获取的物理内存空间信息数组长度

  unsigned long *bits_map; // 物理页映射位图，标志页是否被分配，0=free
  unsigned long bits_size; // 物理页的数量
  unsigned long bits_length; // 物理页映射位图占用内存空间长度，单位字节

  struct Page *pages_struct; // 全局 struct page 结构体数组指针
  unsigned long pages_size;  // 全局 struct page 结构体数量
  unsigned long
      pages_length; // 全局 struct page 结构体数组占用内存空间长度，单位字节

  struct Zone *zones_struct; // 全局 struct zone 结构体数组指针
  unsigned long zones_size;  // 全局 struct zone 结构体数量
  unsigned long
      zones_length; // 全局 struct zone 结构体数组占用内存空间长度，单位字节
                    // 下面 4 个属性由链接脚本提供
  unsigned long start_code; // 内核程序的代码段起始地址
  unsigned long end_code;   // 内核程序的代码段结束地址
  unsigned long end_data;   // 内核程序的数据段结束地址
  unsigned long end_brk;    // 内核程序的结束地址

  unsigned long
      end_of_struct; // 内存页管理结构（包括位图、page数组、zone数组）的结束地址，
                     // 从这之后的物理内存可以任意使用
};

/* page.attribute */
// 经过页表映射的页
#define PG_PTable_Maped (1 << 0)
// 内核初始化程序
#define PG_Kernel_Init (1 << 1)
// 引用
#define PG_Referenced (1 << 2)
//
#define PG_Dirty (1 << 3)
// 使用中的页
#define PG_Active (1 << 4)
//
#define PG_Up_To_Date (1 << 5)
//
#define PG_Device (1 << 6)
// 内核层的页
#define PG_Kernel (1 << 7)
// 共享
#define PG_K_Share_To_U (1 << 8)
//
#define PG_Slab (1 << 9)

struct Page {                    // 物理内存页
  struct Zone *zone_struct;      // 指向该页所属的 zone
  unsigned long PHY_address;     // 页的物理地址
  unsigned long attribute;       // 页属性
  unsigned long reference_count; // 引用次数，一个页面可以被映射到多个线性地址
  unsigned long age;             // 该页的创建时间
};

/**
 * @brief 表示一个可用的物理内存区域（可用物理内存段）
 */
struct Zone {
  struct Page *pages_group; // 本区域包含的物理页的 struct page 数组指针
  unsigned long pages_length; // 本区域包含的物理页数量

  unsigned long zone_start_address; // 本区域的起始页对齐地址
  unsigned long zone_end_address;   // 本区域的结束页对齐地址
  unsigned long zone_length; // 本区域经过页对齐后的地址长度
  unsigned long attribute;   // 该区域空间的属性

  struct Global_Memory_Descriptor *GMD_struct; // 指向全局内存信息结构体

  unsigned long page_using_count; // 本区域已使用物理页数量
  unsigned long page_free_count;  // 本区域空闲物理页数量

  unsigned long
      total_pages_link; // 本区域物理页被引用的数量，一个物理页可以和多个线性地址对应，
                        // 故 total_pages_link >= page_using_count
};

// 每种类型的 zone 在全局 zone 数组中的索引
int ZONE_DMA_INDEX = 0;
int ZONE_NORMAL_INDEX = 0;  // low 1GB RAM ,was mapped in pagetable
int ZONE_UNMAPED_INDEX = 0; // above 1GB RAM,unmapped in pagetable

////alloc_pages zone_select
//
#define ZONE_DMA	(1 << 0)
//
#define ZONE_NORMAL	(1 << 1)
//
#define ZONE_UNMAPED	(1 << 2)

extern struct Global_Memory_Descriptor memory_management_struct;

/**
 * @brief 初始化目标物理页的 page，并更新目标物理页所在 zone 的统计信息
 * 
 * @param page 目标物理页
 * @param flags page.attribute
 * @return unsigned long 
 */
unsigned long page_init(struct Page * page,unsigned long flags);

// 初始化物理内存
void init_mm();

// 页表项会被 tlb 缓存，需要强制刷新 tlb 才能使页表项更新成功
// 重新加载 cr3 即可迫使 tlb 自动刷新
#define flush_tlb()                                                            \
  do {                                                                         \
    unsigned long tmpreg;                                                      \
    __asm__ __volatile__("movq	%%cr3,	%0	\n\t"                                 \
                         "movq	%0,	%%cr3	\n\t"                                 \
                         : "=r"(tmpreg)                                        \
                         :                                                     \
                         : "memory");                                          \
  } while (0)

// 获取当前 cr3 寄存器的值
static inline unsigned long *Get_gdt() {
  unsigned long *tmp;
  __asm__ __volatile__("movq	%%cr3,	%0	\n\t" : "=r"(tmp) : : "memory");
  return tmp;
}

/**
 * @brief 从 zone_select 指定的物理内存区域中分配连续的 number 个物理页
 * 
 * @param zone_select 指定从哪个物理内存区域分配
 * @param number 需求的页数
 * @param page_flags 分配物理页后设置的页属性
 * @return struct Page* 返回第一页的 page 结构体地址
 */
struct Page* alloc_pages(int zone_select, int number, unsigned long page_flags);

#endif