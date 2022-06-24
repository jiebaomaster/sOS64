#include "trap.h"
#include "gate.h"
#include "printk.h"
#include "ptrace.h"
#include "SMP.h"

/*
        0
*/

void do_divide_error(struct pt_regs *regs, unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_divide_error(0),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx,"
               "CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        1
*/

void do_debug(struct pt_regs *regs, unsigned long error_code) {
  color_printk(
      RED, BLACK,
      "do_debug(1),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx,CPU:%#018lx\n",
      error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        2
*/

void do_nmi(struct pt_regs *regs, unsigned long error_code) {
  color_printk(
      RED, BLACK,
      "do_nmi(2),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx,CPU:%#018lx\n",
      error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        3
*/

void do_int3(struct pt_regs *regs, unsigned long error_code) {
  color_printk(
      RED, BLACK,
      "do_int3(3),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx,CPU:%#018lx\n",
      error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        4
*/

void do_overflow(struct pt_regs *regs, unsigned long error_code) {
  color_printk(
      RED, BLACK,
      "do_overflow(4),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx,CPU:%#018lx\n",
      error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        5
*/

void do_bounds(struct pt_regs *regs, unsigned long error_code) {
  color_printk(
      RED, BLACK,
      "do_bounds(5),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx,CPU:%#018lx\n",
      error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        6
*/

void do_undefined_opcode(struct pt_regs *regs, unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_undefined_opcode(6),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#"
               "018lx,CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        7
*/

void do_dev_not_available(struct pt_regs *regs, unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_dev_not_available(7),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#"
               "018lx,CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        8
*/

void do_double_fault(struct pt_regs *regs, unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_double_fault(8),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx,"
               "CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        9
*/

void do_coprocessor_segment_overrun(struct pt_regs *regs,
                                    unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_coprocessor_segment_overrun(9),ERROR_CODE:%#018lx,RSP:%#"
               "018lx,RIP:%#018lx,CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        10
*/

void do_invalid_TSS(struct pt_regs *regs, unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_invalid_TSS(10),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx,"
               "CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());

  if (error_code & 0x01)
    color_printk(
        RED, BLACK,
        "The exception occurred during delivery of an event external to the "
        "program,such as an interrupt or an earlier exception.\n");

  if (error_code & 0x02)
    color_printk(RED, BLACK, "Refers to a gate descriptor in the IDT;\n");
  else
    color_printk(RED, BLACK,
                 "Refers to a descriptor in the GDT or the current LDT;\n");

  if ((error_code & 0x02) == 0)
    if (error_code & 0x04)
      color_printk(RED, BLACK,
                   "Refers to a segment or gate descriptor in the LDT;\n");
    else
      color_printk(RED, BLACK, "Refers to a descriptor in the current GDT;\n");

  color_printk(RED, BLACK, "Segment Selector Index:%#010x\n",
               error_code & 0xfff8);

  while (1)
    ;
}

/*
        11
*/

void do_segment_not_present(struct pt_regs *regs, unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_segment_not_present(11),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%"
               "#018lx,CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());

  if (error_code & 0x01)
    color_printk(
        RED, BLACK,
        "The exception occurred during delivery of an event external to the "
        "program,such as an interrupt or an earlier exception.\n");

  if (error_code & 0x02)
    color_printk(RED, BLACK, "Refers to a gate descriptor in the IDT;\n");
  else
    color_printk(RED, BLACK,
                 "Refers to a descriptor in the GDT or the current LDT;\n");

  if ((error_code & 0x02) == 0)
    if (error_code & 0x04)
      color_printk(RED, BLACK,
                   "Refers to a segment or gate descriptor in the LDT;\n");
    else
      color_printk(RED, BLACK, "Refers to a descriptor in the current GDT;\n");

  color_printk(RED, BLACK, "Segment Selector Index:%#010x\n",
               error_code & 0xfff8);

  while (1)
    ;
}

/*
        12
*/

void do_stack_segment_fault(struct pt_regs *regs, unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_stack_segment_fault(12),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%"
               "#018lx,CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());

  if (error_code & 0x01)
    color_printk(
        RED, BLACK,
        "The exception occurred during delivery of an event external to the "
        "program,such as an interrupt or an earlier exception.\n");

  if (error_code & 0x02)
    color_printk(RED, BLACK, "Refers to a gate descriptor in the IDT;\n");
  else
    color_printk(RED, BLACK,
                 "Refers to a descriptor in the GDT or the current LDT;\n");

  if ((error_code & 0x02) == 0)
    if (error_code & 0x04)
      color_printk(RED, BLACK,
                   "Refers to a segment or gate descriptor in the LDT;\n");
    else
      color_printk(RED, BLACK, "Refers to a descriptor in the current GDT;\n");

  color_printk(RED, BLACK, "Segment Selector Index:%#010x\n",
               error_code & 0xfff8);

  while (1)
    ;
}

/*
        13
*/

void do_general_protection(struct pt_regs *regs, unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_general_protection(13),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#"
               "018lx,CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());

  if (error_code & 0x01)
    color_printk(
        RED, BLACK,
        "The exception occurred during delivery of an event external to the "
        "program,such as an interrupt or an earlier exception.\n");

  if (error_code & 0x02)
    color_printk(RED, BLACK, "Refers to a gate descriptor in the IDT;\n");
  else
    color_printk(RED, BLACK,
                 "Refers to a descriptor in the GDT or the current LDT;\n");

  if ((error_code & 0x02) == 0)
    if (error_code & 0x04)
      color_printk(RED, BLACK,
                   "Refers to a segment or gate descriptor in the LDT;\n");
    else
      color_printk(RED, BLACK, "Refers to a descriptor in the current GDT;\n");

  color_printk(RED, BLACK, "Segment Selector Index:%#010x\n",
               error_code & 0xfff8);

  while (1)
    ;
}

/*
        14
*/

void do_page_fault(struct pt_regs *regs, unsigned long error_code) {
  unsigned long cr2 = 0;

  __asm__ __volatile__("movq	%%cr2,	%0" : "=r"(cr2)::"memory");

  color_printk(RED, BLACK,
               "do_page_fault(14),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx,"
               "CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());

  if (!(error_code & 0x01))
    color_printk(RED, BLACK, "Page Not-Present,\t");

  if (error_code & 0x02)
    color_printk(RED, BLACK, "Write Cause Fault,\t");
  else
    color_printk(RED, BLACK, "Read Cause Fault,\t");

  if (error_code & 0x04)
    color_printk(RED, BLACK, "Fault in user(3)\t");
  else
    color_printk(RED, BLACK, "Fault in supervisor(0,1,2)\t");

  if (error_code & 0x08)
    color_printk(RED, BLACK, ",Reserved Bit Cause Fault\t");

  if (error_code & 0x10)
    color_printk(RED, BLACK, ",Instruction fetch Cause Fault");

  color_printk(RED, BLACK, "\n");

  color_printk(RED, BLACK, "CR2:%#018lx\n", cr2);

  while (1)
    ;
}

/*
        16
*/

void do_x87_FPU_error(struct pt_regs *regs, unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_x87_FPU_error(16),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#"
               "018lx,CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        17
*/

void do_alignment_check(struct pt_regs *regs, unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_alignment_check(17),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#"
               "018lx,CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        18
*/

void do_machine_check(struct pt_regs *regs, unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_machine_check(18),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#"
               "018lx,CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        19
*/

void do_SIMD_exception(struct pt_regs *regs, unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_SIMD_exception(19),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#"
               "018lx,CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

/*
        20
*/

void do_virtualization_exception(struct pt_regs *regs,
                                 unsigned long error_code) {
  color_printk(RED, BLACK,
               "do_virtualization_exception(20),ERROR_CODE:%#018lx,RSP:%#018lx,"
               "RIP:%#018lx,CPU:%#018lx\n",
               error_code, regs->rsp, regs->rip, SMP_cpu_id());
  while (1)
    ;
}

void sys_vector_init() {
  set_trap_gate(0, 1, divide_error); // 0 #DE 除 0 异常
  set_trap_gate(1, 1, debug);        // 1 #DB 调试异常
  set_intr_gate(2, 1, nmi);    // 2 NMI non-mask interrupt 不可屏蔽中断
  set_system_gate(3, 1, int3); // 3 BP 断点异常
  set_system_gate(4, 1, overflow);       // 4 #OF 溢出
  set_system_gate(5, 1, bounds);         // 5 #BR 对数组的引用超出边界
  set_trap_gate(6, 1, undefined_opcode); // 6 #UD 无效或未定义的操作码
  set_trap_gate(7, 1, dev_not_available); // 7 #NM 设备不可用(无数学协处理器)
  set_trap_gate(8, 1, double_fault); // 8 #DF 双重故障(有错误代码)
  set_trap_gate(9, 1, coprocessor_segment_overrun); // 9 协处理器跨段操作
  set_trap_gate(10, 1, invalid_TSS); // 10 #TS 无效TSS(有错误代码)
  set_trap_gate(11, 1, segment_not_present); // 11 #NP 段不存在(有错误代码)
  set_trap_gate(12, 1, stack_segment_fault); // 12 #SS 栈错误(有错误代码)
  set_trap_gate(13, 1, general_protection); // 13 #GP 常规保护(有错误代码)
  set_trap_gate(14, 1, page_fault);         // 14 #PF 页故障(有错误代码)
  // 15 Intel reserved. Do not use.
  set_trap_gate(16, 1, x87_FPU_error);   // 16 #MF 浮点处理单元错误
  set_trap_gate(17, 1, alignment_check); // 17 #AC 对齐检查
  set_trap_gate(18, 1, machine_check);   // 18 #MC 机器检查
  set_trap_gate(19, 1, SIMD_exception); // 19 #XM SIMD(单指令多数据)浮点异常
  set_trap_gate(20, 1, virtualization_exception); // 20 虚拟化异常
}