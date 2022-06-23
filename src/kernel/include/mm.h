#ifndef __MM_H__
#define __MM_H__

#include "lib.h"
#include "list.h"

#if UEFI
#include "UEFI_boot_param_info.h"
#else
#include "BIOS_boot_param_info.h"
#endif

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

// 内核代码使用固定地址映射
// 物理地址的前 48MB 被映射到线性地址 0xffff800000000000 处，也就只有这 48MB
// 的内存空间可以使用下面的宏 内核层，虚拟地址 => 物理地址
#define Virt_To_Phy(addr) ((unsigned long)(addr)-PAGE_OFFSET)
// 内核层，物理地址 => 虚拟地址
#define Phy_To_Virt(addr)                                                      \
  ((unsigned long *)((unsigned long)(addr) + PAGE_OFFSET))

// 返回管理虚拟地址 kaddr 的物理地址空间的 struct Page 的虚拟地址
#define Virt_To_2M_Page(kaddr)                                                 \
  (memory_management_struct.pages_struct +                                     \
   (Virt_To_Phy(kaddr) >> PAGE_2M_SHIFT))
#define Phy_To_2M_Page(kaddr)                                                  \
  (memory_management_struct.pages_struct +                                     \
   ((unsigned long)(kaddr) >> PAGE_2M_SHIFT))

/**
 * 页表中表示该页表项的属性
 */
//	bit 63	Execution Disable:
#define PAGE_XD (1UL << 63)
//	bit 12	Page Attribute Table
#define PAGE_PAT (1UL << 12)
//	bit 8	Global Page:1,global;0,part
#define PAGE_Global (1UL << 8)
//	bit 7	Page Size:1,big page;0,small page;
#define PAGE_PS (1UL << 7)
//	bit 6	Dirty:1,dirty;0,clean;
#define PAGE_Dirty (1UL << 6)
//	bit 5	Accessed:1,visited;0,unvisited;
#define PAGE_Accessed (1UL << 5)
//	bit 4	Page Level Cache Disable
#define PAGE_PCD (1UL << 4)
//	bit 3	Page Level Write Through
#define PAGE_PWT (1UL << 3)
//	bit 2	User Supervisor:1,user and supervisor;0,supervisor;
#define PAGE_U_S (1UL << 2)
//	bit 1	Read Write:1,read and write;0,read;
#define PAGE_R_W (1UL << 1)
//	bit 0	Present:1,present;0,no present;
#define PAGE_Present (1UL << 0)
// 1,0
#define PAGE_KERNEL_GDT (PAGE_R_W | PAGE_Present)
// 1,0
#define PAGE_KERNEL_Dir (PAGE_R_W | PAGE_Present)
// 7,1,0
#define PAGE_KERNEL_Page (PAGE_PS | PAGE_R_W | PAGE_Present)
// 1,0
#define PAGE_USER_GDT (PAGE_U_S | PAGE_R_W | PAGE_Present)
// 2,1,0
#define PAGE_USER_Dir (PAGE_U_S | PAGE_R_W | PAGE_Present)
// 7,2,1,0
#define PAGE_USER_Page (PAGE_PS | PAGE_U_S | PAGE_R_W | PAGE_Present)

/**
 * 构建页表的辅助操作
 */
typedef struct {
  unsigned long pml4t;
} pml4t_t;
// 构建 mpl4t 页表的页表项，addr 下级页表的物理地址，attr 页表项属性
#define mk_mpl4t(addr, attr) ((unsigned long)(addr) | (unsigned long)(attr))
// 设置 mpl4t 页表的页表项 [mpl4tptr]=mpl4tval
#define set_mpl4t(mpl4tptr, mpl4tval) (*(mpl4tptr) = (mpl4tval))

typedef struct {
  unsigned long pdpt;
} pdpt_t;
#define mk_pdpt(addr, attr) ((unsigned long)(addr) | (unsigned long)(attr))
#define set_pdpt(pdptptr, pdptval) (*(pdptptr) = (pdptval))

typedef struct {
  unsigned long pdt;
} pdt_t;
#define mk_pdt(addr, attr) ((unsigned long)(addr) | (unsigned long)(attr))
#define set_pdt(pdtptr, pdtval) (*(pdtptr) = (pdtval))

typedef struct {
  unsigned long pt;
} pt_t;
#define mk_pt(addr, attr) ((unsigned long)(addr) | (unsigned long)(attr))
#define set_pt(ptptr, ptval) (*(ptptr) = (ptval))

// cr3 寄存器保存顶级页表地址
unsigned long *Global_CR3 = NULL;

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

/* page 结构体中的页表属性 page.attribute */
// 1=经过页表映射的页
#define PG_PTable_Maped (1 << 0)
// 1=内核初始化程序
#define PG_Kernel_Init (1 << 1)
// 1=设备寄存器/内存 0=物理内存空间
#define PG_Device (1 << 2)
// 内核层的页
#define PG_Kernel (1 << 3)
// 1=共享页
#define PG_Shared (1 << 4)

struct Page {                    // 物理内存页
  struct Zone *zone_struct;      // 指向该页所属的 zone
  unsigned long PHY_address;     // 页的物理地址
  unsigned long attribute;       // 页属性
  unsigned long reference_count; // 引用次数，一个页面可以被映射到多个线性地址
  unsigned long age;             // 该页的创建时间
};

/**
 * @brief 表示一个可用的物理内存区域（可用物理内存段，取自 BIOS 中断 E820）
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
int ZONE_NORMAL_INDEX = 0;  // low 4GB RAM ,was mapped in pagetable
int ZONE_UNMAPED_INDEX = 0; // above 4GB RAM,unmapped in pagetable

////alloc_pages zone_select
//
#define ZONE_DMA	(1 << 0)
//
#define ZONE_NORMAL	(1 << 1)
//
#define ZONE_UNMAPED	(1 << 2)

extern struct Global_Memory_Descriptor memory_management_struct;

// 初始化物理内存
void init_memory();

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
 * @brief 初始化目标物理页的 page
 * 
 * @param page 目标物理页
 * @param flags page.attribute
 * @return 0=失败，1=成功
 */
unsigned long page_init(struct Page * page,unsigned long flags);

/**
 * @brief 
 * 
 * @param page 
 * @return 0=失败，1=成功
 */
unsigned long page_clean(struct Page* page);

/**
 * @brief 从 zone_select 指定的物理内存区域中分配连续的 number 个物理页
 * 
 * @param zone_select 指定从哪个物理内存区域分配
 * @param number 需求的页数
 * @param page_flags 分配物理页后设置的页属性
 * @return struct Page* 返回第一个 page 结构体的虚拟地址
 */
struct Page* alloc_pages(int zone_select, int number, unsigned long page_flags);

/**
 * @brief 释放从 page 开始的 number 个物理页
 */
void free_pages(struct Page *page, int number);

/**
 * 管理 Slab 内存池中的一个物理页
 */
struct Slab {
  struct List list; // 属于内存池的所有

  struct Page *page; // 所管理物理页的 page
  void *Vaddress;    // 所管理物理页的虚拟地址

  unsigned long using_count; // 已用个数
  unsigned long free_count;  // 可用个数

  unsigned long color_length; // 位图本身的大小（字节）
  unsigned long color_count;  // 位图管理的对象个数，即 Slab 容量
  unsigned long *color_map;   // 位图地址
};

/**
 * 对象大小为 size 的内存池
 * 包括多个大小为一个物理页的 struct Slab
 */
struct Slab_cache {
  unsigned long size;          // 内存池的对象大小
  unsigned long total_using;   // 可用个数
  unsigned long total_free;    // 已用个数
  struct Slab *cache_pool;     // 内存池链表
  struct Slab *cache_dma_pool; // DMA内存池

  // 对象的构造函数和析构函数
  void *(*constructor)(void *Vaddress, unsigned long arg);
  void *(*destructor)(void *Vaddress, unsigned long arg);
};

// 通用内存管理 kmalloc/kfree 使用的内存池
struct Slab_cache kmalloc_cache_size[16] = {
    {32, 0, 0, NULL, NULL, NULL, NULL},
    {64, 0, 0, NULL, NULL, NULL, NULL},
    {128, 0, 0, NULL, NULL, NULL, NULL},
    {256, 0, 0, NULL, NULL, NULL, NULL},
    {512, 0, 0, NULL, NULL, NULL, NULL},
    {1024, 0, 0, NULL, NULL, NULL, NULL}, // 1KB
    {2048, 0, 0, NULL, NULL, NULL, NULL},
    {4096, 0, 0, NULL, NULL, NULL, NULL}, // 4KB
    {8192, 0, 0, NULL, NULL, NULL, NULL},
    {16384, 0, 0, NULL, NULL, NULL, NULL},
    {32768, 0, 0, NULL, NULL, NULL, NULL},
    {65536, 0, 0, NULL, NULL, NULL, NULL},  // 64KB
    {131072, 0, 0, NULL, NULL, NULL, NULL}, // 128KB
    {262144, 0, 0, NULL, NULL, NULL, NULL},
    {524288, 0, 0, NULL, NULL, NULL, NULL},
    {1048576, 0, 0, NULL, NULL, NULL, NULL}, // 1MB
};

// 向上取整到 long 的整数倍
#define SIZEOF_LONG_ALIGN(size) ((size + sizeof(long)-1) & ~(sizeof(long)-1))
// 向上取整到 int 的整数倍
#define SIZEOF_INT_ALIGN(size) ((size + sizeof(int)-1) & ~(sizeof(int)-1))

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
            void *(*destructor)(void *Vaddress, unsigned long arg));

/**
 * @brief 销毁 slab_cache 内存池
 * @return 0=失败，1=成功
 */
unsigned long slab_destory(struct Slab_cache* slab_cache);

/**
 * @brief 向 slab_cache 内存池申请一个对象
 * 
 * @param arg 对象构造函数的参数
 * @return void* 初始化后的对象
 */
void* slab_malloc(struct Slab_cache* slab_cache, unsigned long arg);

/**
 * @brief 将 address 内存块归还到 slab_cache 内存池，并调用对象的析构函数
 * 
 * @return 0=失败，1=成功
 */
unsigned long slab_free(struct Slab_cache *slab_cache, void *address,
                        unsigned long arg);

/**
 * @brief 初始化 kmalloc 使用的内存池 kmalloc_cache_size
 * 
 * @return 0=失败，1=成功
 */
unsigned long kmalloc_slab_init();

/**
 * @brief 通用内存申请
 *
 * @param size 申请的内存大小
 * @param gfp_flags 控制内存分配
 * @return void* 申请到内存的起始虚拟地址
 */
void *kmalloc(unsigned long size, unsigned long gfp_flags);

/**
 * @brief 释放内存
 *
 * @param address 待释放内存的起始地址
 * @return 0=失败，1=成功
 */
unsigned long kfree(void *address);

/**
 * @brief 使用 kmalloc 申请一块新的 Slab 并初始化
 * 
 * @param size Slab 管理的对象大小
 */
struct Slab *get_new_Slab(unsigned long size);

/**
 * @brief 在 slab_cache 中的 slab 中分配一个对象
 * 
 * @param arg 对象构造函数的参数
 * @return void* 初始化后的对象
 */
void *__do_slab_malloc(struct Slab_cache *slab_cache, struct Slab *slab, unsigned long arg);

/**
 * @brief 释放一个 Slab，使用 kfree 释放 Slab 和 位图
 */
void __do_Slab_destory(struct Slab *slab);

/**
 * 将所有物理页全部映射到线性地址空间内
 */
void pagetable_init();

#endif