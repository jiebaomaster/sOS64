#include "lib.h"
#include "printk.h"

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

  // 0x00ff0000 红色
  for (i = 0; i < 1440 * 20; i++) {
    *((char *)addr + 0) = (char)0x00;
    *((char *)addr + 1) = (char)0x00;
    *((char *)addr + 2) = (char)0xff;
    *((char *)addr + 3) = (char)0x00;
    addr += 1;
  }
  // 0x0000ff00 绿色
  for (i = 0; i < 1440 * 20; i++) {
    *((char *)addr + 0) = (char)0x00;
    *((char *)addr + 1) = (char)0xff;
    *((char *)addr + 2) = (char)0x00;
    *((char *)addr + 3) = (char)0x00;
    addr += 1;
  }
  // 0x000000ff 蓝色
  for (i = 0; i < 1440 * 20; i++) {
    *((char *)addr + 0) = (char)0xff;
    *((char *)addr + 1) = (char)0x00;
    *((char *)addr + 2) = (char)0x00;
    *((char *)addr + 3) = (char)0x00;
    addr += 1;
  }
  // 0x00ffffff 白色
  for (i = 0; i < 1440 * 20; i++) {
    *((char *)addr + 0) = (char)0xff;
    *((char *)addr + 1) = (char)0xff;
    *((char *)addr + 2) = (char)0xff;
    *((char *)addr + 3) = (char)0x00;
    addr += 1;
  }

  color_printk(YELLOW, BLACK, "Hello\t\t World!\n");
  printk("test int %-3d, str %s, 16# %#x, %%\n",10, "abcd", 16);

  i = 1 / 0; // 触发异常
  while (1)
    ;
}