#include "gate.h"
#include "lib.h"
#include "mm.h"
#include "printk.h"
#include "trap.h"
#include "task.h"
#include "cpu.h"
#include "SMP.h"

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
  Pos.XResolution = 1440;
  Pos.YResolution = 900;

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

  // 为 BSP 加载 tss
  load_TR(10);
  // 设置 BSP 的 TSS
  set_tss64(TSS64_TABLE, _stack_start, _stack_start, _stack_start,
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
  Local_APIC_init();
  #else
  init_8259A();
  #endif

  // color_printk(RED, BLACK, "keyboard init \n");
  // keyboard_init();

  // color_printk(RED, BLACK, "task_init \n");
  // task_init();

  SMP_init();

	struct INT_CMD_REG icr_entry;
  // 准备 INIT IPI
  icr_entry.vector = 0x00;
	icr_entry.deliver_mode =  APIC_ICR_IOAPIC_INIT;
	icr_entry.dest_mode = ICR_IOAPIC_DELV_PHYSICAL;
	icr_entry.deliver_status = APIC_ICR_IOAPIC_Idle;
	icr_entry.res_1 = 0;
	icr_entry.level = ICR_LEVEL_DE_ASSERT;
	icr_entry.trigger = APIC_ICR_IOAPIC_Edge;
	icr_entry.res_2 = 0;
	icr_entry.dest_shorthand = ICR_ALL_EXCLUDE_Self;
	icr_entry.res_3 = 0;
	icr_entry.destination.x2apic_destination = 0x00;

	wrmsr(0x830, *(unsigned long*)&icr_entry); // INIT IPI
  
  // 每个 AP 需要有独立的栈和 TSS 才能独立处理任务
  // 为 AP 的 init 进程申请栈的内存空间，用 _stack_start 在 head.S 中给 AP 传递栈顶地址
  // BSP 和 AP 在 head.S 中都用 _stack_start 设置栈
  _stack_start = (unsigned long)kmalloc(STACK_SIZE, 0) + STACK_SIZE;
  // 为 AP 的 tss 申请内存空间
  unsigned int* tss = (unsigned int*)kmalloc(128, 0);
  set_tss_descriptor(12, tss);
  set_tss64(tss, _stack_start, _stack_start, _stack_start, _stack_start, _stack_start, _stack_start,_stack_start,_stack_start,_stack_start,_stack_start);
  
  // 准备 Start-up IPI
  icr_entry.vector = 0x20;
  icr_entry.deliver_mode = ICR_Start_up;

  wrmsr(0x830, *(unsigned long*)&icr_entry); // Start-up IPI
  wrmsr(0x830, *(unsigned long*)&icr_entry); // Start-up IPI

  while (1)
    ;
}