#ifndef __MM_H__
#define __MM_H__

#include "printk.h"

// 每个页表的页表项数目，每个页表4KB，每个页表项8B
#define PTRS_PER_PAGE 512
// 内核层的起始线性地址
#define PAGE_OFFSET ((unsigned long)0xffff800000000000)

#define PAGE_GDT_SHIFT 39
#define PAGE_1G_SHIFT 30
#define PAGE_2M_SHIFT 21
#define PAGE_4K_SHIFT 12

#define PAGE_2M_SIZE (1UL << PAGE_2M_SHIFT)
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
#define Phy_To_Virt ((unsigned long *)((unsigned long)(addr) + PAGE_OFFSET))

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
 * @brief 内存信息
 */
struct Global_Memory_Descriptor {
  struct E820 e820[32]; // BIOS 中断 E820 获取的物理内存空间信息数组
  unsigned long e820_length;  // BIOS 中断 E820 获取的物理内存空间信息数组长度
};

extern struct Global_Memory_Descriptor memory_management_struct;

// 初始化物理内存
void init_mm();

#endif