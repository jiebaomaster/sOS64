#include "interrupt.h"
#include "gate.h"
#include "printk.h"
#include "lib.h"
#include "linkage.h"

// 中断处理函数的中保存执行现场的部分
#define SAVE_ALL                                                               \
  "cld;			\n\t"                                                                \
  "pushq	%rax;		\n\t"    /* FUNC 占位，为了共用异常退出代码 */     \
  "pushq	%rax;		\n\t"                                                          \
  "movq	%es,	%rax;	\n\t"                                                       \
  "pushq	%rax;		\n\t"                                                          \
  "movq	%ds,	%rax;	\n\t"                                                       \
  "pushq	%rax;		\n\t"                                                          \
  "xorq	%rax,	%rax;	\n\t"                                                      \
  "pushq	%rbp;		\n\t"                                                          \
  "pushq	%rdi;		\n\t"                                                          \
  "pushq	%rsi;		\n\t"                                                          \
  "pushq	%rdx;		\n\t"                                                          \
  "pushq	%rcx;		\n\t"                                                          \
  "pushq	%rbx;		\n\t"                                                          \
  "pushq	%r8;		\n\t"                                                           \
  "pushq	%r9;		\n\t"                                                           \
  "pushq	%r10;		\n\t"                                                          \
  "pushq	%r11;		\n\t"                                                          \
  "pushq	%r12;		\n\t"                                                          \
  "pushq	%r13;		\n\t"                                                          \
  "pushq	%r14;		\n\t"                                                          \
  "pushq	%r15;		\n\t"                                                          \
  "movq	$0x10,	%rdx;	\n\t"                                                     \
  "movq	%rdx,	%ds;	\n\t"                                                       \
  "movq	%rdx,	%es;	\n\t"

#define IRQ_NAME2(nr) nr##_interrupt(void)
#define IRQ_NAME(nr) IRQ_NAME2(IRQ##nr)

// 定义一个中断处理函数，函数原型为 void IRQ##nr##_interrupt(void)
// 保存现场 =》统一处理 do_IRQ =》中断返回 ret_from_intr
#define Build_IRQ(nr)                                                          \
  void IRQ_NAME(nr);                                                           \
  __asm__( SYMBOL_NAME_STR(IRQ)#nr"_interrupt: \n\t"                          \
          "pushq $0x00    \n\t" /* 中断不会产生错误代码，压入 0 占位 */ \
          SAVE_ALL /* 保存执行现场 */ \
          "movq %rsp, %rdi  \n\t"  /* 处理函数参数1，当前栈顶指针 */ \
          "leaq ret_from_intr(%rip), %rax  \n\t" /* 获取返回地址 */ \
          "pushq %rax   \n\t" /* 将返回地址压栈，配合jmp模拟调用指令 */ \
          "movq $"#nr", %rsi    \n\t" /* 处理函数参数2，中断向量号 */ \
          "jmp  do_IRQ  \n\t");

// 定义 0x20~0x37 号中断处理函数
Build_IRQ(0x20)
Build_IRQ(0x21)
Build_IRQ(0x22)
Build_IRQ(0x23)
Build_IRQ(0x24)
Build_IRQ(0x25)
Build_IRQ(0x26)
Build_IRQ(0x27)
Build_IRQ(0x28)
Build_IRQ(0x29)
Build_IRQ(0x2a)
Build_IRQ(0x2b)
Build_IRQ(0x2c)
Build_IRQ(0x2d)
Build_IRQ(0x2e)
Build_IRQ(0x2f)
Build_IRQ(0x30)
Build_IRQ(0x31)
Build_IRQ(0x32)
Build_IRQ(0x33)
Build_IRQ(0x34)
Build_IRQ(0x35)
Build_IRQ(0x36)
Build_IRQ(0x37)
// 定义 0xc8~0xd1 号 IPI 中断处理函数
Build_IRQ(0xc8)
Build_IRQ(0xc9)
Build_IRQ(0xca)
Build_IRQ(0xcb)
Build_IRQ(0xcc)
Build_IRQ(0xcd)
Build_IRQ(0xce)
Build_IRQ(0xcf)
Build_IRQ(0xd0)
Build_IRQ(0xd1)

// 定义中断处理函数指针数组
void (* interrupt[NR_IRQS])(void) = {
  IRQ0x20_interrupt,
  IRQ0x21_interrupt,
  IRQ0x22_interrupt,
  IRQ0x23_interrupt,
  IRQ0x24_interrupt,
  IRQ0x25_interrupt,
  IRQ0x26_interrupt,
  IRQ0x27_interrupt,
  IRQ0x28_interrupt,
  IRQ0x29_interrupt,
  IRQ0x2a_interrupt,
  IRQ0x2b_interrupt,
  IRQ0x2c_interrupt,
  IRQ0x2d_interrupt,
  IRQ0x2e_interrupt,
  IRQ0x2f_interrupt,
  IRQ0x30_interrupt,
  IRQ0x31_interrupt, 
  IRQ0x32_interrupt,
  IRQ0x33_interrupt,
  IRQ0x34_interrupt,
  IRQ0x35_interrupt,
  IRQ0x36_interrupt,
  IRQ0x37_interrupt,
};

void (*SMP_interrupt[NR_IPI])(void) = {
  IRQ0xc8_interrupt, 
  IRQ0xc9_interrupt, 
  IRQ0xca_interrupt, 
  IRQ0xcb_interrupt,
  IRQ0xcc_interrupt, 
  IRQ0xcd_interrupt, 
  IRQ0xce_interrupt, 
  IRQ0xcf_interrupt,
  IRQ0xd0_interrupt, 
  IRQ0xd1_interrupt,
};

/**
 * @brief 在 interrupt_desc 中注册中断处理函数
 * 
 * @param irq 中断向量号
 * @param arg 安装中断的参数，可以是 struct IO_APIC_RET_entry
 * @param handler 中断处理函数
 * @param parameter 中断处理函数的参数
 * @param controller 中断控制接口函数
 * @param irq_name 中断名称
 */
int register_irq(unsigned long irq, void *arg,
                 void (*handler)(unsigned long nr, unsigned long parameter,
                                 struct pt_regs *regs),
                 unsigned long parameter, hw_int_controller *controller,
                 char *irq_name) {
  irq_desc_T *p = &interrupt_desc[irq - 32];
  
  p->controller = controller;
  p->handler = handler;
  p->parameter = parameter;
  p->irq_name = irq_name;
  p->flags = 0;

  p->controller->install(irq, arg);
  p->controller->enable(irq);

  return 1;
}

/**
 * @brief 在 interrupt_desc 中注销中断处理函数
 * 
 * @param irq 中断向量号
 */
int unregister_irq(unsigned long irq) {
  irq_desc_T *p = &interrupt_desc[irq - 32];
  
  p->controller->disable(irq);
  p->controller->uninstall(irq);
  
  p->controller = NULL;
  p->handler = NULL;
  p->parameter = NULL;
  p->irq_name = NULL;
  p->flags = 0;
  
  return 1;
}

/**
 * @brief 在 SMP_IPI_desc 中注册中断处理函数
 * 
 * @param irq 中断向量号
 * @param arg 安装中断的参数，可以是 struct IO_APIC_RET_entry
 * @param handler 中断处理函数
 * @param parameter 中断处理函数的参数
 * @param controller 中断控制接口函数，IPI 不用操作硬件设备，该参数应为 NULL
 * @param irq_name 中断名称
 */
int register_IPI(unsigned long irq, void *arg,
                 void (*handler)(unsigned long nr, unsigned long parameter,
                                 struct pt_regs *regs),
                 unsigned long parameter, hw_int_controller *controller,
                 char *irq_name) {
  irq_desc_T *p = &SMP_IPI_desc[irq - 200];

  p->controller = NULL;
  p->irq_name = irq_name;
  p->parameter = parameter;
  p->flags = 0;
  p->handler = handler;

  return 1;
}

/**
 * @brief 在 SMP_IPI_desc 中注销中断处理函数
 * 
 * @param irq 中断向量号
 */
int unregister_IPI(unsigned long irq) {
  irq_desc_T *p = &SMP_IPI_desc[irq - 200];

  p->controller = NULL;
  p->irq_name = NULL;
  p->parameter = NULL;
  p->flags = 0;
  p->handler = NULL;

  return 1;
}
