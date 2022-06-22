#include "SMP.h"
#include "cpu.h"
#include "lib.h"
#include "printk.h"
#include "gate.h"
#include "interrupt.h"
#include "task.h"

extern int global_i;

/**
 * 多核处理器的初始化
 */
void SMP_init() {
  int i;
  unsigned int a, b, c, d;

  for (i = 0;; i++) {
    get_cpuid(0xb, i, &a, &b, &c, &d);
    if ((c >> 8 & 0xff) == 0)
      break;

    printk("local APIC ID Package_../Core_2/SMT_1,type(%x) Width:%#010x,num of "
           "logical processor(%x)\n",
           c >> 8 & 0xff, a & 0x1f, b & 0xff);
  }

  printk("x2APIC ID level:%#010x\tx2APIC ID the current logical "
         "processor:%#010x\n",
         c & 0xff, d);

  printk_info("SMP copy byte:%#010x\n", _APU_boot_end - _APU_boot_start);
  // 将 APU Boot 代码拷贝到物理地址 0x20000 处
  memcpy(_APU_boot_start, (unsigned char *)0xffff800000020000, _APU_boot_end - _APU_boot_start);
  spin_init(&SMP_lock);

  // 初始化 IPI 中断向量表
  for (i = 200; i < 210; i++) {
    set_intr_gate(i, 2, SMP_interrupt[i - 200]);
  }
  memset(SMP_IPI_desc, 0, sizeof(irq_desc_T) * 10);
}

/**
 * APU 的启动路径，只需加载操作系统的各类描述符表，不应该重复执行各模块的初始化
 */
void Start_SMP(void) {
  unsigned int x, y;

  color_printk(RED, YELLOW, "APU starting......\n");


  // enable xAPIC & x2APIC
  __asm__ __volatile__("movq 	$0x1b,	%%rcx	\n\t"
                       "rdmsr	\n\t"
                       "bts	$10,	%%rax	\n\t"
                       "bts	$11,	%%rax	\n\t"
                       "wrmsr	\n\t"
                       "movq 	$0x1b,	%%rcx	\n\t"
                       "rdmsr	\n\t"
                       : "=a"(x), "=d"(y)
                       :
                       : "memory");

  if (x & 0xc00)
    color_printk(RED, YELLOW, "xAPIC & x2APIC enabled\n");

  // enable SVR[8]
  __asm__ __volatile__(
      "movq 	$0x80f,	%%rcx	\n\t"
      "rdmsr	\n\t"
      "bts	$8,	%%rax	\n\t"
      //				"bts	$12,	%%rax\n\t"
      "wrmsr	\n\t"
      "movq 	$0x80f,	%%rcx	\n\t"
      "rdmsr	\n\t"
      : "=a"(x), "=d"(y)
      :
      : "memory");

  if (x & 0x100)
    color_printk(RED, YELLOW, "SVR[8] enabled\n");
  if (x & 0x1000)
    color_printk(RED, YELLOW, "SVR[12] enabled\n");

  // get local APIC ID
  __asm__ __volatile__("movq $0x802,	%%rcx	\n\t"
                       "rdmsr	\n\t"
                       : "=a"(x), "=d"(y)
                       :
                       : "memory");

  color_printk(RED, YELLOW, "x2APIC ID:%#010x\n", x);

  // 清空 AP 的 PCB
  memset(current, 0, sizeof(struct task_struct));

  // 加载属于 AP 的 TSS
  load_TR(10 + (global_i - 1) * 2);
  
  // AP 启动完毕后释放信号量，使得 BSP 启动下一个 AP
  spin_unlock(&SMP_lock);
  
  sti();

  while(1)
    hlt();
}