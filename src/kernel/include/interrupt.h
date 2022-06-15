#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include "ptrace.h"

void (* interrupt[24])(void);

void do_IRQ(struct pt_regs * regs, unsigned long nr);

#endif