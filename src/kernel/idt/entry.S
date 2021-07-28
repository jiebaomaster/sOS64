#include "linkage.h"

// 下面这些符号常量定义了，进程执行现场保存完毕后，栈中各寄存器相对于栈顶地址的偏移值
// 通过 栈顶指针+寄存器偏移 即可访问到所有保存的现场寄存器
R15 = 0x00
R14 = 0x08
R13 = 0x10
R12 = 0x18
R11 = 0x20
R10 = 0x28
R9  = 0x30
R8  = 0x38
RBX = 0x40
RCX = 0x48
RDX = 0x50
RSI = 0x58
RDI = 0x60
RBP = 0x68
DS  = 0x70
ES  = 0x78
RAX = 0x80
FUNC  = 0x88    // 处理函数的地址
                // 以下的寄存器在异常发生时由硬件自动压栈
ERRCODE = 0x90  // （如果有的话）错误代码
RIP = 0x98
CS  = 0xa0
RFLAGS  = 0xa8
                // 以下寄存器只会在发生特权级变化时被压栈
OLDRSP  = 0xb0  // 上一个任务的栈顶地址
OLDSS = 0xb8    // 上一个任务的栈段选择子

RESTORE_ALL:  // 恢复栈中保存的执行现场
  popq  %r15  // 恢复通用寄存器 r8~r15
  popq  %r14
  popq  %r13
  popq  %r12
  popq  %r11
  popq  %r10
  popq  %r9
  popq  %r8
  popq  %rbx  // 恢复通用寄存 rbx、rcx、rdx、rsi、rdi、rbp
  popq  %rcx
  popq  %rdx
  popq  %rsi
  popq  %rdi
  popq  %rbp
  popq  %rax          // ds 出栈
  movq  %rax,   %ds   //
  popq  %rax          // es 出栈
  movq  %rax,   %es   //
  popq  %rax  // 恢复通用寄存器 rax，若栈空间发送改变，则最后一个通用寄存器 rsp 由 iretq 恢复
  addq  $0x10,  %rsp  // 栈指针上移16B，即 FUNC 和 ERRCODE 出栈，
                      // 若异常会产生错误代码，则错误代码的压栈是硬件自动的，但是其出栈需要软件手动操作
  iretq               // 中断返回，自动弹出硬件保存的（SS、RSP）、RFLAGS、CS、RIP 寄存器

ret_from_exception:   // 异常返回
ENTRY(ret_from_intr)
  jmp RESTORE_ALL     // TODO 当前异常返回时只恢复现场，后期可以加上进程调度、进程信号处理等工作


// 借助 sysexit 指令执行系统调用的返回，内核空间 => 用户空间 P173
ENTRY(ret_system_call)
  movq  %rax, 0x80(%rsp) // 此时 rax=系统调用的返回值，因为后面恢复ds和es时会借助rax，所以这里先保存到栈中，等ds和es恢复完之后再从栈中恢复rax，这样就能将返回值传递给用户程序
  popq  %r15  // 恢复通用寄存器 r8~r15
  popq  %r14
  popq  %r13
  popq  %r12
  popq  %r11
  popq  %r10
  popq  %r9
  popq  %r8
  popq  %rbx  // 恢复通用寄存 rbx、rcx、rdx、rsi、rdi、rbp
  popq  %rcx  // sysexit 指令会将 rcx 加载到 rsp 中恢复3特权级的栈指针
  popq  %rdx  // sysexit 指令会将 rdx 加载到 rip 中恢复3特权级的指令指针
              // 上述两个寄存器需要在应用程序进入内核层之前进行特殊处理
  popq  %rsi
  popq  %rdi
  popq  %rbp
  popq  %rax          // ds 出栈
  movq  %rax,   %ds   //
  popq  %rax          // es 出栈
  movq  %rax,   %es   //
  popq  %rax
  addq  $0x38,  %rsp  // 平衡栈，剩余的 7 个执行现场寄存器出栈
  .byte 0x48
  sysexit

ENTRY(divide_error)   // 异常0 #DE 除 0 异常处理函数
  pushq $0            // #DE 不产生错误代码，为了确保所有异常处理函数的寄存器压栈顺序一致，压入0进行占位
  pushq %rax          // 以下三行代码，将真实处理函数的地址借助rax压栈，同时不改变rax的值
  leaq  do_divide_error(%rip),  %rax // rax=真实处理函数的地址
  xchgq %rax, (%rsp)  // 交换 rax 和 rsp指向的内存 的值

error_code: // 保存现场执行环境
  pushq %rax  // 保存通用寄存器 rax
  movq  %es,  %rax  // 保存 es
  pushq %rax        //
  movq  %ds,  %rax  // 保存 ds
  pushq %rax        //
  xorq  %rax, %rax  // rax 清理

  pushq %rbp  // 保存通用寄存 rbx、rcx、rdx、rsi、rdi、rbp
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  pushq %rbx
  pushq %r8   // 保存恢复通用寄存器 r8~r15
  pushq %r9
  pushq %r10
  pushq %r11
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15

  cld         // 清方向标志位
  movq  ERRCODE(%rsp),  %rsi  // 处理函数的参数2，错误代码
  movq  FUNC(%rsp), %rdx      // 处理函数的参数3，处理函数的地址

  movq  $0x10,  %rdi // 重新加载内核数据段，因为处理函数属于内核段
  movq  %rdi, %ds
  movq  %rdi, %es

  movq  %rsp, %rdi    // 处理函数的参数1，当前栈顶指针，
                      // 通过 栈顶指针+文件开头定义的寄存器偏移 即可访问到所有保存的现场寄存器

  callq *%rdx         // 调用真正的中断处理函数
  jmp   ret_from_exception // 处理函数执行完毕，跳转到通用的异常返回代码

ENTRY(debug)
  pushq $0
  pushq %rax
  leaq  do_debug(%rip), %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(nmi)  // 不可屏蔽中断，是一个外部中断，执行中断处理过程
  pushq %rax
  cld
  pushq %rax

  pushq %rax
  movq  %es,  %rax
  pushq %rax
  movq  %ds,  %rax
  pushq %rax
  xorq  %rax, %rax

  pushq %rbp
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  pushq %rbx
  pushq %r8
  pushq %r9
  pushq %r10
  pushq %r11
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15

  movq  $0x10,  %rdx  // 重新加载内核数据段，因为处理函数属于内核段
  movq  %rdx, %ds
  movq  %rdx, %es

  movq  $0,   %rsi    // 处理函数的参数2，占位的错误代码，中断不会产生错误代码
  movq  %rsp, %rdi    // 处理函数的参数1，当前栈顶指针

  callq	do_nmi

  jmp	RESTORE_ALL


ENTRY(int3)
  pushq $0
  pushq %rax
  leaq  do_int3(%rip),  %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(overflow)
  pushq $0
  pushq %rax
  leaq  do_overflow(%rip),  %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(bounds)
  pushq $0
  pushq %rax
  leaq  do_bounds(%rip),  %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(undefined_opcode)
  pushq $0
  pushq %rax
  leaq  do_undefined_opcode(%rip),  %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(dev_not_available)
  pushq $0
  pushq %rax
  leaq  do_dev_not_available(%rip), %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(double_fault)
  pushq %rax  // 该异常会产生错误代码，不用压入0占位
  leaq  do_double_fault(%rip),  %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(coprocessor_segment_overrun)
  pushq $0
  pushq %rax
  leaq  do_coprocessor_segment_overrun(%rip), %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(invalid_TSS)
  pushq %rax
  leaq  do_invalid_TSS(%rip), %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(segment_not_present)
  pushq %rax
  leaq  do_segment_not_present(%rip), %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(stack_segment_fault)
  pushq %rax
  leaq  do_stack_segment_fault(%rip), %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(general_protection)
  pushq %rax
  leaq  do_general_protection(%rip),  %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(page_fault)
  pushq %rax
  leaq  do_page_fault(%rip),  %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(x87_FPU_error)
  pushq $0
  pushq %rax
  leaq  do_x87_FPU_error(%rip), %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(alignment_check)
  pushq %rax
  leaq  do_alignment_check(%rip), %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(machine_check)
  pushq $0
  pushq %rax
  leaq  do_machine_check(%rip), %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(SIMD_exception)
  pushq $0
  pushq %rax
  leaq  do_SIMD_exception(%rip),  %rax
  xchgq %rax, (%rsp)
  jmp   error_code

ENTRY(virtualization_exception)
  pushq $0
  pushq %rax
  leaq  do_virtualization_exception(%rip),  %rax
  xchgq %rax, (%rsp)
  jmp   error_code