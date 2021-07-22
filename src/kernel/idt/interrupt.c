#include "interrupt.h"
#include "gate.h"
#include "printk.h"
#include "lib.h"
#include "linkage.h"

// 中断处理函数的中保存执行现场的部分
#define SAVE_ALL                                                               \
  "cld;			\n\t"                                                                \
  "pushq	%rax;		\n\t"                                                          \
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

// 定义中断处理函数指针数组
void (* interrupt[24])(void) = {
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

void init_interrupt() {
  int i;
  for (i = 32; i < 56; i++)
    set_intr_gate(i, 2, interrupt[i - 32]);

  printk("8259A init \n");

  // 8259A-maxter ICW1-4
  io_out8(0x20, 0x11);
  io_out8(0x21, 0x20); // ICW2，占用中断向量号 20h~27h
  io_out8(0x21, 0x04);
  io_out8(0x21, 0x01);

  // 8259A-slave ICW1-4
  io_out8(0xa0, 0x11);
  io_out8(0xa1, 0x28); // ICW2，占用中断向量号 28h~2fh
  io_out8(0xa1, 0x02);
  io_out8(0xa1, 0x01);

  // 8259A-M/S OCW1
  io_out8(0x21, 0x00);
  io_out8(0xa1, 0x00);

  // 初始化中断处理之后开中断
  sti();
}

/**
 * @brief 统一的中断处理函数，所有的中断都先跳转到这里，再根据中断向量号分发给各自的真实处理函数
 *
 * @param regs 中断发生时栈顶指针，可根据 entry.S 中定义的偏移量访问到所有保存的现场寄存器
 * @param nr 中断向量号
 */
void do_IRQ(unsigned long regs, unsigned long nr) {
  // TODO 当前中断处理时仅输出中断处理号
  printk("do_IRQ:%#08x\t", nr);
  // 中断处理完毕后必须手动向中断控制器发送中断结束EOI指令，来复位ISR寄存器的对应位
  io_out8(0x20, 0x20);
}
