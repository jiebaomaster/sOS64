#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include "ptrace.h"

// 中断控制接口，操作物理设备
typedef struct hw_int_type {
  void (*enable)(unsigned long irq); // 使能中断
  void (*disable)(unsigned long irq); // 禁用中断
	// 安装中断
  unsigned long (*install)(unsigned long irq, void *arg);
	// 卸载中断
  void (*uninstall)(unsigned long irq);
	// 应答中断
  void (*ack)(unsigned long irq);
} hw_int_controller;

typedef struct { // 处理中断所需信息
  hw_int_controller *controller; // 中断控制接口

  char *irq_name; // 中断名
  unsigned long parameter; // 中断处理函数的参数
	// 中断处理函数（上半部）
  void (*handler)(unsigned long nr, unsigned long parameter,
                  struct pt_regs *regs);
  unsigned long flags; // 自定义标志位
} irq_desc_T;

// 见 APIC.h 外部中断 32~55	I/O APIC，共 24 个
#define NR_IRQS 24
// 全局外部中断描述表
// 只有 24 项，所以用中断向量号索引时要减去 32
irq_desc_T interrupt_desc[NR_IRQS] = {0};
// 全局 IPI（Inter-Processor Interrupt）中断描述表，多核间通信
irq_desc_T SMP_IPI_desc[10] = {0};

// 外部中断处理函数指针数组
extern void (*interrupt[24])(void);
// IPI 中断处理函数指针数组
extern void (*SMP_interrupt[10])(void);

/**
 * @brief 在 interrupt_desc 中注册中断处理函数
 * 
 * @param irq 中断向量号
 */
int register_irq(unsigned long irq, void *arg,
                 void (*handler)(unsigned long nr, unsigned long parameter,
                                 struct pt_regs *regs),
                 unsigned long parameter, hw_int_controller *controller,
                 char *irq_name);

/**
 * @brief 在 interrupt_desc 中注销中断处理函数
 * 
 * @param irq 中断向量号
 */
int unregister_irq(unsigned long irq);

/**
 * @brief 统一的中断处理函数，所有的中断都先跳转到这里，再根据中断向量号分发给各自的真实处理函数
 *
 * @param regs 中断发生时栈顶指针，可访问到所有保存的现场寄存器
 * @param nr 中断向量号
 */
void do_IRQ(struct pt_regs *regs, unsigned long nr);

#endif