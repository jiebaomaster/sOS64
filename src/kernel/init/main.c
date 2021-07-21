#include "gate.h"
#include "lib.h"
#include "mm.h"
#include "printk.h"
#include "trap.h"

// 链接脚本中定义的记录段地址的变量
extern char _text;
extern char _etext;
extern char _edata;
extern char _end;

void Start_Kernel(void) {
  int *addr = (int *)0xffff800000a00000; // 图像帧缓存的起始地址
  int i;

  // 初始化屏幕信息
  Pos.XResolution = 1440;
  Pos.YResolution = 900;

  Pos.XPosition = 0;
  Pos.YPosition = 0;

  Pos.XCharSize = 8;
  Pos.YCharSize = 16;

  Pos.FB_addr = (int *)0xffff800000a00000;
  Pos.FB_length = (Pos.XResolution * Pos.YResolution * 4);

  // 初始化 tss
  load_TR(8);
  // tss 中所有的栈指针都指向 0xffff800000007c00
  set_tss64(0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00,
            0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00,
            0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00,
            0xffff800000007c00);
  // 初始化异常处理
  sys_vector_init();

  memory_management_struct.start_code = (unsigned long)&_text;
  memory_management_struct.end_code = (unsigned long)&_etext;
  memory_management_struct.end_data = (unsigned long)&_edata;
  memory_management_struct.end_brk = (unsigned long)&_end;

  color_printk(RED, BLACK, "memory init \n");
  init_mm();


  // 测试物理页分配函数
  color_printk(RED, BLACK, "memory_management_struct.bits_map:%#018lx\n",
               *memory_management_struct.bits_map);
  color_printk(RED, BLACK, "memory_management_struct.bits_map:%#018lx\n",
               *(memory_management_struct.bits_map + 1));

  struct Page *page =
      alloc_pages(ZONE_NORMAL, 64, PG_PTable_Maped | PG_Active | PG_Kernel);

  for (i = 0; i <= 64; i++) {
    color_printk(INDIGO, BLACK, "page%d\tattribute:%#018lx\taddress:%#018lx\t",
                 i, (page + i)->attribute, (page + i)->PHY_address);
    i++;
    color_printk(INDIGO, BLACK, "page%d\tattribute:%#018lx\taddress:%#018lx\n",
                 i, (page + i)->attribute, (page + i)->PHY_address);
  }

  color_printk(RED, BLACK, "memory_management_struct.bits_map:%#018lx\n",
               *memory_management_struct.bits_map);
  color_printk(RED, BLACK, "memory_management_struct.bits_map:%#018lx\n",
               *(memory_management_struct.bits_map + 1));

  while (1)
    ;
}