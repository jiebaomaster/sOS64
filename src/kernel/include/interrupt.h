#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

void init_interrupt();

void do_IRQ(unsigned long regs, unsigned long nr);

#endif