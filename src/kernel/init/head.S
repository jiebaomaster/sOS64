.section  .text

.globl  _start

_start:
  mov   $0x10,  %ax
  mov   %ax,    %ds
  mov   %ax,    %es
  mov   %ax,    %fs
  mov   %ax,    %ss
  mov   $0x7E00,  %esp

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
  movq  $0x7E00,  %rsp
  
//====== load cr3

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
  movq  $0xffff800000007E00,  %rsp

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
  leaq  TSS64_TABLE(%rip),  %rdx
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
  movq  %rax, 64(%rdi)
  shrq  $32,  %rdx
  movq  %rdx, 72(%rdi)

//  mov   $0x40,%ax // 0x40 为 TSS 描述符在 GDT 中的偏移量
//  ltr   %ax       // 加载 TSS 描述符选择子到 tr 寄存器中

  movq  go_to_kernel(%rip), %rax
  pushq $0x08
  pushq %rax
  lretq           // 调用 Start_Kernel 函数  

go_to_kernel:
  .quad Start_Kernel

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

//====== init page
.align  8

.org  0x1000
//====== PML4T 39-47
__PML4E:
  .quad   0x102007  // 下一级页表物理地址 0x102000
  .fill   255,8,0
  .quad   0x102007  // 下一级页表物理地址 0x102000
  .fill   255,8,0

.org  0x2000
//====== PDPT 30-38
__PDPTE:
  .quad   0x103003  // 下一级页表物理地址 0x103000
  .fill   511,8,0

.org  0x3000
//====== PDT 21-29，采用 2MB 大小的物理页，页内偏移占 21 位
// 映射内核代码，前 5 个页表项将物理地址前 10MB 映射到线性地址 0 和 0xffff800000000000
// 映射图像帧缓存，后 8 个页表项将物理地址前 0xe0000000 开始的 16MB 映射到线性地址 0xa00000 和 0xffff800000a00000
__PDE:
  .quad   0x000083
  .quad   0x200083
  .quad   0x400083
  .quad   0x600083
  .quad   0x800083
  .quad   0xe0000083  /*0x a00000*/
  .quad   0xe0200083
  .quad   0xe0400083
  .quad   0xe0600083
  .quad   0xe0800083
  .quad   0xe0a00083
  .quad   0xe0c00083
  .quad   0xe0e00083
  .fill   499,8,0

//====== GDT_TABLE

.section  .data

.globl  GDT_TABLE

GDT_TABLE:
  .quad   0x0000000000000000    /*0	NULL descriptor		       	00*/
  .quad   0x0020980000000000    /*1	KERNEL	Code	64-bit	Segment	08*/
  .quad   0x0000920000000000    /*2	KERNEL	Data	64-bit	Segment	10*/
  .quad   0x0020f80000000000    /*3	USER	Code	64-bit	Segment	18*/
  .quad   0x0000f20000000000    /*4	USER	Data	64-bit	Segment	20*/
  .quad   0x00cf9a000000ffff    /*5	KERNEL	Code	32-bit	Segment	28*/
  .quad   0x00cf92000000ffff    /*6	KERNEL	Data	32-bit	Segment	30*/
  .fill   10,8,0                /*8-9 TSS (jmp one segment <7>) in long-mode 128bit 40*/
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

//======= TSS64_TABLE

.globl  TSS64_TABLE

TSS64_TABLE: // 64 位模式下每个 TSS 占 104 字节，故该表只含有一个 TSS，详见书本 P232
  .fill   13,8,0
TSS64_END:

TSS64_POINTER:
TSS64_LIMIT:    .word TSS64_END - TSS64_TABLE - 1
TSS64_BASE:     .quad TSS64_TABLE