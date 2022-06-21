#include "interrupt.h"
#include "gate.h"
#include "printk.h"
#include "lib.h"

/**
 * 初始化 8259A 中断控制器
 */
void init_8259A() {
  int i;
  // 初始化中断向量表
  for (i = 32; i < 56; i++)
    set_intr_gate(i, 0, interrupt[i - 32]);

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

  // 8259A-M/S OCW1 TODO 屏蔽除了键盘中断外的所有中断
  io_out8(0x21, 0xfd);
  io_out8(0xa1, 0xff);

  // 初始化中断处理之后开中断
  sti();
}

/**
 * @brief 统一的中断处理函数，所有的中断都先跳转到这里，再根据中断向量号分发给各自的真实处理函数
 *
 * @param regs 中断发生时栈顶指针，可访问到所有保存的现场寄存器
 * @param nr 中断向量号
 */
void do_IRQ(struct pt_regs * regs, unsigned long nr) {
  // TODO 当前中断处理时仅输出中断处理号
  printk("do_IRQ:%#08x\t", nr);

  // 读取键盘控制器的读写缓冲区，获取按键扫描码
  unsigned char x = io_in8(0x60);
	color_printk(RED,BLACK,"key code:%#08x\n",x);
  
  // 中断处理完毕后必须手动向中断控制器发送中断结束EOI指令，来复位ISR寄存器的对应位
  io_out8(0x20, 0x20);
}
