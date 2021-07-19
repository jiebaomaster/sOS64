#include "mm.h"
#include "lib.h"

void init_mm() {
  int i, j;
  unsigned long TotalMem = 0;
  struct E820 *p = NULL;

  color_printk(
      BLUE, BLACK,
      "Display Physics Address MAP,Type(1:RAM,2:ROM or Reserved,3:ACPI Reclaim "
      "Memory,4:ACPI NVS Memory,Others:Undefine)\n");
  p = (struct E820 *)0xffff800000007e00;

  for (i = 0; i < 32; i++) {
    printk("Address:%#018lx\tLength:%#018lx\tType:%#010x\n", p->address,
           p->length, p->type);
    unsigned long tmp = 0;
    if (p->type == 1)
      TotalMem += p->length;

    memory_management_struct.e820[i].address += p->address;
    memory_management_struct.e820[i].length += p->length;
    memory_management_struct.e820[i].type = p->type;
    memory_management_struct.e820_length = i;

    p++;
    if (p->type > 4)
      break;
  }

  color_printk(ORANGE, BLACK, "OS Can Used Total RAM:%#018lx\n", TotalMem);

  TotalMem = 0;
  // 计算可用内存空间的物理页数
  for (i = 0; i <= memory_management_struct.e820_length; i++) {
    unsigned long start, end; // 可用内存地址范围，左闭右开
    if (memory_management_struct.e820[i].type != 1) // 只计算可用内存空间
      continue;

    // 起始地址对齐到 2M
    start = PAGE_2M_ALIGN(memory_management_struct.e820[i].address);
    // 终止地址对齐到 2M
    end = (memory_management_struct.e820[i].address +
           memory_management_struct.e820[i].length) &
          PAGE_2M_MASK;

    if (end <= start) // 该段空间不到一页
      continue;

    TotalMem += (end - start) >> PAGE_2M_SHIFT; // 累加可用内存页数
  }

  color_printk(ORANGE, BLACK, "OS Can Used Total 2M PAGEs:%#010x=%010d\n",
               TotalMem, TotalMem);
}