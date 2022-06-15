#ifndef __APIC_H__
#define __APIC_H__

#include "interrupt.h"
#include "linkage.h"
#include "ptrace.h"

/**
 * @brief 统一的中断处理函数，所有的中断都先跳转到这里，再根据中断向量号分发给各自的真实处理函数
 *
 * @param regs 中断发生时栈顶指针，可访问到所有保存的现场寄存器
 * @param nr 中断向量号
 */ 
void do_IRQ(struct pt_regs *regs, unsigned long nr);

/**
 * 初始化 APIC 中断控制器
 */
void APIC_IOAPIC_init();
void Local_APIC_init();

#endif