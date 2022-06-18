/**
 * APU_boot.S 和内核一起编译，起始地址是 0xffff800000100000，但是实际运行在物理地址 0x20000 处，
 * 故该文件中不能使用绝对地址，对地址的访问全部要使用相对地址
*/
#include "linkage.h"

.balign 0x1000

.text
.code16

// 定义此文件代码链接后的首尾地址，用于在内核中拷贝这部分的代码到物理地址 0x20000 处
ENTRY(_APU_boot_start)

_APU_boot_base = .

  cli
  wbinvd
  
  mov %cs,  %ax // 当前 cs 由 IPI start-up 决定，本内核中为 0x2000
  mov %ax,  %ds
  mov %ax,  %es
  mov %ax,  %ss
  mov %ax,  %fs
  mov %ax,  %gs
  
  movl $(_APU_boot_tmp_Stack_end - _APU_boot_base), %esp

# get base address
  mov %cs,  %ax
  movzx %ax,  %esi // esi=0x2000
  shll $4,  %esi   // esi=0x20000 本文件代码的物理基地址

# set gdt and 32&64 code address
  leal (_APU_Code32 - _APU_boot_base)(%esi), %eax
  movl %eax,  _APU_Code32_vector - _APU_boot_base
  
  leal (_APU_Code64 - _APU_boot_base)(%esi), %eax
  movl %eax,  _APU_Code64_vector - _APU_boot_base

  leal (_APU_tmp_GDT - _APU_boot_base)(%esi), %eax
  movl %eax,  _APU_tmp_GDT + 2 - _APU_boot_base

# load idt gdt
  lidtl _APU_tmp_IDT - _APU_boot_base
  lgdtl _APU_tmp_GDT - _APU_boot_base

# 进入保护模式
  smsw  %ax
  bts   $0, %ax
  lmsw  %ax

# go to 32 code
  ljmpl *(_APU_Code32_vector - _APU_boot_base)

.code32
.balign 4
_APU_Code32:
# go to 64 code
  mov $0x10,  %ax
  mov %ax,    %ds
  mov %ax,    %es
  mov %ax,    %ss
  mov %ax,    %fs
  mov %ax,    %gs

  leal  (_APU_boot_tmp_Stack_end - _APU_boot_base)(%esi), %eax
  movl  %eax, %esp

# open PAE
  movl  %cr4, %eax
  bts   $5,   %eax // cr4.pae=1，开启物理地址扩展功能，进入长模式时必须
  movl  %eax, %cr4

# set page table 设置长模式的页表地址，UEFI 引导模式没有在 0x90000 处设置页表
#if UEFI
  movl  $0x101000, %eax
#else
  movl  $0x90000,  %eax
#endif
  movl  %eax, %cr3

# enable long mode
  movl  $0xC0000080,  %ecx
  rdmsr
  bts   $8, %eax // IA32_EFER.LME=1，激活 IA-32e 模式
  wrmsr 

# enable PE & paging
  movl  %cr0, %eax
  bts   $0,   %eax // cr0.PE=1，保险起见，再次开启保护模式
  bts   $31,  %eax // cr0.PG=1，开启分页
  movl  %eax, %cr0

  ljmp *(_APU_Code64_vector - _APU_boot_base)(%esi)

.code64
.balign 4
_APU_Code64:
# go to head.S
  movq  $0x20,  %rax
  movq  %rax,   %ds
  movq  %rax,   %es
  movq  %rax,   %fs
  movq  %rax,   %gs
  movq  %rax,   %ss

  // 跳转到 head.S，这里跳转到虚拟地址 1MB 处，需要确保页表中一致性映射还在
  movq  $0x100000,  %rax
  jmpq  *%rax

  hlt

.balign 4
_APU_tmp_IDT:
  .word 0
  .word 0,0

.balign 4
_APU_tmp_GDT:
  .short _APU_tmp_GDT_end - _APU_tmp_GDT - 1 // GDT 长度 0x00
  .long  _APU_tmp_GDT - _APU_boot_base       // GDT 基地址 0x02
  .short 0                  // 0x06
  .quad  0x00cf9a000000ffff // 32 代码段描述符 0x08
  .quad  0x00cf92000000ffff // 32 数据段描述符 0x10
  .quad  0x0020980000000000 // 64 代码段描述符 0x18
  .quad  0x0000920000000000 // 64 数据段描述符 0x20
_APU_tmp_GDT_end:

.balign 4
_APU_Code32_vector:                  // 32 位代码的远跳转地址
  .long _APU_Code32 - _APU_boot_base // 段内偏移
  .word 0x08,0                       // 段描述符

.balign 4
_APU_Code64_vector:
  .long _APU_Code64 - _APU_boot_base
  .word 0x18,0

.balign 4
_APU_boot_tmp_Stack_start:
  .org  0x400
_APU_boot_tmp_Stack_end:

ENTRY(_APU_boot_end)