#include "linkage.h"

.section  .text

.globl  _start

#if UEFI
_start:
  mov   %ss,    %ax
  mov   %ax,    %ds
  mov   %ax,    %es
  mov   %ax,    %fs
  mov   %ax,    %ss
  mov   $0x7E00,  %esp

  movq  $0x101000,  %rax
  movq  %rax,       %cr3
//====== load GDTR

  lgdt  GDT_POINTER(%rip)

//====== load IDTR

  lidt  IDT_POINTER(%rip)
  
  mov   $0x10,  %ax
  mov   %ax,    %ds
  mov   %ax,    %es
  mov   %ax,    %fs
  mov   %ax,    %gs
  mov   %ax,    %ss
  
  movq  _stack_start(%rip),  %rsp

#else 
_start:
	mov	$0x10,	%ax
	mov	%ax,	%ds
	mov	%ax,	%es
	mov	%ax,	%fs
	mov	%ax,	%ss
	mov	$0x7E00,	%esp

//=======	load GDTR

	lgdt	GDT_POINTER(%rip)

//=======	load	IDTR

	lidt	IDT_POINTER(%rip)

	mov	$0x10,	%ax
	mov	%ax,	%ds
	mov	%ax,	%es
	mov	%ax,	%fs
	mov	%ax,	%gs
	mov	%ax,	%ss

	movq	$0x7E00,	%rsp

#endif
  
//====== load cr3 设置页表

  movq  $0x101000, %rax
  movq  %rax,   %cr3
  movq  switch_seg(%rip), %rax
  pushq $0x08     // 返回地址压栈
  pushq %rax      // 函数入口地址压栈
  lretq           // 调用 entry64 函数，GAS不支持远跳转指令，需要使用函数调用切换段

//====== 64bit mode code

switch_seg:
  .quad entry64

entry64:
  movq  $0x10,  %rax
  movq  %rax,    %ds
  movq  %rax,    %es
  movq  %rax,    %gs
  movq  %rax,    %ss
  // _stack_start 在 BSP 启动后更新为属于 AP 的独立栈地址
  movq  _stack_start(%rip),  %rsp
  
  movq  $0x1b,   %rcx // 根据 IA32_APIC_BASE.BSP[8] 判断是否是 AP
  rdmsr
  bt    $8,      %rax
  jnc   start_smp // 是 AP，走 AP 启动路线

setup_IDT: // 初始化 IDT，全部指向一个临时的处理函数 ignore_int
  leaq  ignore_int(%rip), %rdx // 接下去的多行代码初始化了一个指向 ignore_int 的中断描述符
  movq  $(0x08 << 16),    %rax // 且该中断描述符的低 8 字节存储在 rax，高 8 字节存储在 rdx
  movw  %dx,	%ax
  movq  $(0x8E00 << 32),  %rcx		
  addq  %rcx, %rax
  movl  %edx, %ecx
  shrl  $16,  %ecx
  shlq  $48,  %rcx
  addq  %rcx, %rax
  shrq  $32,  %rdx
  leaq  IDT_TABLE(%rip),  %rdi // rdi=IDT表的地址
  mov   $256, %rcx      // rcx=256，循环次数
rp_sidt:                // repeat set idt
  movq  %rax, (%rdi)    // *rdi=rax，初始化当前 idt 项的低 8 字节
  movq  %rdx, 8(%rdi)   // *(rdi+8)=rdx，初始化当前 idt 项的高 8 字节
  addq  $0x10,%rdi      // rdi+=16B，一个 idt 项占 16B，即指向下一个 idt 项
  dec   %rcx            // rcx--
  jne   rp_sidt         // 如果rcx!=0，继续循环，即重复 256 次

setup_TSS64: // 初始化 TSS，发生特权级的切换时，需要从 TSS 内读取栈空间信息
  leaq  init_tss(%rip),  %rdx
  xorq  %rax, %rax
  xorq  %rcx, %rcx
  movq  $0x89,%rax
  shlq  $40,  %rax
  movl  %edx, %ecx
  shrl  $24,  %ecx
  shlq  $56,  %rcx
  addq  %rcx, %rax
  xorq  %rcx, %rcx
  movl  %edx, %ecx
  andl  $0xffffff,  %ecx
  shlq  $16,  %rcx
  addq  %rcx, %rax
  addq  $103, %rax
  leaq  GDT_TABLE(%rip),  %rdi
  movq  %rax, 80(%rdi)
  shrq  $32,  %rdx
  movq  %rdx, 88(%rdi)

//  mov   $0x50,%ax // 0x50 为 TSS 描述符在 GDT 中的偏移量
//  ltr   %ax       // 加载 TSS 描述符选择子到 tr 寄存器中

  movq  go_to_kernel(%rip), %rax
  pushq $0x08
  pushq %rax
  lretq           // 调用 Start_Kernel 函数  

go_to_kernel:
  .quad Start_Kernel


// AP 的处理路径，只需加载操作系统的各类描述符表，不应该重复执行各模块的初始化
start_smp:
  movq  go_to_smp_kernel(%rip), %rax
  pushq $0x08
  pushq %rax
  lretq   // 调用 Start_SMP 函数  

go_to_smp_kernel:
  .quad Start_SMP

//====== ignore_int

ignore_int:
  cld
  /* 保存通用寄存器的值 */
  pushq %rax
  pushq %rbx
  pushq %rcx
  pushq %rdx
  pushq %rbp
  pushq %rdi
  pushq %rsi

  pushq %r8
  pushq %r9
  pushq %r10
  pushq %r11
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15

  /* 保存 ds 和 es */
  movq  %es,  %rax
  pushq %rax
  movq  %ds,  %rax
  pushq %rax

  /* ds 和 es 设置成内核数据段 */
  movq  $0x10,  %rax
  movq  %rax,   %ds
  movq  %rax,   %es

  leaq  int_msg(%rip),  %rax  // rax=字符串地址
  pushq %rax                  // rax入栈，接下去准备函数调用的参数
  movq  %rax,   %rdx          // 参数3，字符串地址
  movq  $0x00000000,  %rsi    // 参数2，背景色黑色
  movq  $0x00ff0000,  %rdi    // 参数1，前景色红色
  movq  $0,     %rax          // rax存储返回值
  callq color_printk          // 调用格式化输出函数
  addq  $0x8,   %rsp          // 平衡栈操作，rax出栈

Loop: // 原地死循环
  jmp   Loop

  /* 恢复 ds 和 es */
  popq  %rax
  movq  %rax,   %ds
  popq  %rax
  movq  %rax,   %es
  /* 恢复通用寄存器 */
  popq  %r15
  popq  %r14
  popq  %r13
  popq  %r12
  popq  %r11
  popq  %r10
  popq  %r9
  popq  %r8
  popq  %rsi
  popq  %rdi
  popq  %rbp
  popq  %rdx
  popq  %rcx
  popq  %rbx
  popq  %rax
  iretq

int_msg:
  .asciz  "Unknown interrupt or fault at RIP\n"

// 0 号进程的栈基地址，定义在 task.h 中
ENTRY(_stack_start)
  .quad init_task_union + 32768

//====== init page
.align  8

.org  0x1000
//====== PML4T 39-47
__PML4E:
  .quad   0x102003  // 偏移 0，一致性页表映射，下一级页表物理地址 0x102000
  .fill   255,8,0
  .quad   0x102003  // 下一级页表物理地址 0x102000
  .fill   255,8,0

.org  0x2000
//====== PDPT 30-38
__PDPTE:
  .quad   0x103003  // 下一级页表物理地址 0x103000
  .fill   511,8,0

.org  0x3000
//====== PDT 21-29，采用 2MB 大小的物理页，页内偏移占 21 位
// 映射内核代码，前 24 个页表项将物理地址前 48MB 映射到线性地址 0 和 0xffff800000000000
__PDE:
  .quad	0x000083	
	.quad	0x200083
	.quad	0x400083
	.quad	0x600083
	.quad	0x800083		/* 0x800083 */
	.quad	0xa00083
	.quad	0xc00083
	.quad	0xe00083
	.quad	0x1000083
	.quad	0x1200083
	.quad	0x1400083
	.quad	0x1600083
	.quad	0x1800083
	.quad	0x1a00083
	.quad	0x1c00083
	.quad	0x1e00083
	.quad	0x2000083
	.quad	0x2200083
	.quad	0x2400083
	.quad	0x2600083
	.quad	0x2800083
	.quad	0x2a00083
	.quad	0x2c00083
	.quad	0x2e00083

// 映射图像帧缓存，后 8 个页表项将物理地址前 0xf1000000 开始的 16MB 映射到线性地址 0x3000000 和 0xffff800003000000
// UEFI 中不需要手动映射了
	.quad	0xe0000083		/* 线性地址 0x3000000 */
	.quad	0xe0200083
	.quad	0xe0400083
	.quad	0xe0600083
	.quad	0xe0800083
	.quad	0xe0a00083
	.quad	0xe0c00083
	.quad	0xe0e00083
	.fill	488,8,0

//====== GDT_TABLE

.section  .data

.globl  GDT_TABLE
// P173，P180 sysexit/sysenter 指令对 GDT 中段描述符的排列有要求
GDT_TABLE:
  .quad   0x0000000000000000    /*0	NULL descriptor		       	    00*/
  .quad   0x0020980000000000    /*1	KERNEL	Code	64-bit	Segment	08*/
  .quad   0x0000920000000000    /*2	KERNEL	Data	64-bit	Segment	10*/
  .quad   0x0000000000000000    /*3	USER	  Code	32-bit	Segment	18*/
  .quad   0x0000000000000000    /*4	USER	  Data	32-bit	Segment	20*/
  .quad   0x0020f80000000000    /*5	USER	  Code	64-bit	Segment	28*/
  .quad   0x0000f20000000000    /*6	USER	  Data	64-bit	Segment	30*/
  .quad   0x00cf9a000000ffff    /*7	KERNEL	Code	32-bit	Segment	38*/
  .quad   0x00cf92000000ffff    /*8	KERNEL	Data	32-bit	Segment	40*/
  .fill   100,8,0               /*10-11 TSS (jmp one segment <9>) in long-mode 128bit 50*/
GDT_END:

GDT_POINTER:
GDT_LIMIT:    .word GDT_END - GDT_TABLE - 1
GDT_BASE:     .quad GDT_TABLE

//====== IDT_TABLE

.globl  IDT_TABLE

IDT_TABLE:
  .fill   512,8,0
IDT_END:

IDT_POINTER:
IDT_LIMIT:    .word IDT_END - IDT_TABLE - 1
IDT_BASE:     .quad IDT_TABLE
