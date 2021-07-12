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
  pushq $0x08     // 返回值地址压栈
  pushq %rax      // 函数入口地址压栈
  lretq           // 调用 entry64 函数，GAS不支持远跳转指令，需要使用函数调用切换段

//====== 64bit mode code

switch_seg:
  .quad entry64

entry64:
  mov   $0x10,  %rax
  mov   %rax,    %ds
  mov   %rax,    %es
  mov   %rax,    %gs
  mov   %rax,    %ss
  movq  $0xffff800000007E00,  %rsp

  movq  go_to_kernel(%rip), %rax
  pushq $0x08
  pushq %rax
  lretq           // 调用 Start_Kernel 函数  

go_to_kernel:
  .quad Start_Kernel

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
// 前 5 个页表项将物理地址前 10MB 映射到线性地址 0 和 0xffff800000000000
// 后 8 个页表项将物理地址前 0xe0000000开始的 16MB 映射到线性地址 0xa00000 和 0xffff800000a00000
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

TSS64_TABLE:
  .fill   13,8,0
TSS64_END:

TSS64_POINTER:
TSS64_LIMIT:    .word TSS64_END - TSS64_TABLE - 1
TSS64_BASE:     .quad TSS64_TABLE