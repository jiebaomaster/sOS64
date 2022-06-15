#include "gate.h"
#include "lib.h"
#include "mm.h"
#include "printk.h"
#include "trap.h"
#include "task.h"
#include "cpu.h"

#if APIC
#include "APIC.h"
#else
#include "8259A.h"
#endif

extern char _text;
extern char _etext;
extern char _data;
extern char _edata;
extern char _rodata;
extern char _erodata;
extern char _bss;
extern char _ebss;
extern char _end;

#if UEFI
#include "UEFI_boot_param_info.h"
#endif

#if UEFI
// UEFI 传递的信息放在物理地址 0x60000 处
struct KERNEL_BOOT_PARAMETER_INFORMATION *boot_para_info =
    (struct KERNEL_BOOT_PARAMETER_INFORMATION *)0xffff800000060000;

void init_Pos() {
  Pos.XResolution = boot_para_info->Graphics_Info.HorizontalResolution;
  Pos.YResolution = boot_para_info->Graphics_Info.VerticalResolution;

  Pos.XPosition = 0;
  Pos.YPosition = 0;

  Pos.XCharSize = 8;
  Pos.YCharSize = 16;

  Pos.FB_addr = (int *)0xffff800003000000;
  Pos.FB_length = boot_para_info->Graphics_Info.FrameBufferSize;
}
#else
void init_Pos() {
  Pos.XResolution = 1024;
  Pos.YResolution = 768;

  Pos.XPosition = 0;
  Pos.YPosition = 0;

  Pos.XCharSize = 8;
  Pos.YCharSize = 16;

  Pos.FB_addr = (int *)0xffff800003000000;
  Pos.FB_length =
      (Pos.XResolution * Pos.YResolution * 4 + PAGE_4K_SIZE - 1) & PAGE_4K_MASK;
}
#endif

void Start_Kernel(void) {
  int i;

	memset((void*)&_bss,0,(unsigned long)&_end-(unsigned long)&_bss);


  // 初始化屏幕信息
  void init_Pos();

  // 初始化 tss
  load_TR(10);
  // tss 中所有的栈指针都指向 0xffff800000007c00
  set_tss64(0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00,
            0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00,
            0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00,
            0xffff800000007c00);
  // 初始化异常处理
  sys_vector_init();
  init_cpu();

  memory_management_struct.start_code = (unsigned long)&_text;
  memory_management_struct.end_code = (unsigned long)&_etext;
  memory_management_struct.end_data = (unsigned long)&_edata;
  memory_management_struct.end_brk = (unsigned long)&_end;

  color_printk(RED, BLACK, "memory init \n");
  init_memory();

  color_printk(RED, BLACK, "slab init \n");
  kmalloc_slab_init();

  color_printk(RED, BLACK, "frame buffer init \n");
  frame_buffer_init();
  color_printk(WHITE, BLACK, "frame_buffer_init() is OK \n");

  color_printk(RED, BLACK, "pagetable init \n");
  pagetable_init();

  color_printk(RED, BLACK, "interrupt init \n");
  #if APIC
  APIC_IOAPIC_init();
  #else
  init_8259A();
  #endif
    

  // color_printk(RED, BLACK, "task_init \n");
  // task_init();

  while (1)
    ;
}