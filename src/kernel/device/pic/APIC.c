#include "APIC.h"
#include "cpu.h"
#include "gate.h"
#include "interrupt.h"
#include "lib.h"
#include "linkage.h"
#include "mm.h"
#include "printk.h"
#include "ptrace.h"
#include "scheduler.h"
#include "SMP.h"

void IOAPIC_enable(unsigned long irq) {
  unsigned long value = 0;
  value = ioapic_rte_read((irq - 32) * 2 + 0x10);
  value = value & (~0x10000UL);
  ioapic_rte_write((irq - 32) * 2 + 0x10, value);
}

void IOAPIC_disable(unsigned long irq) {
  unsigned long value = 0;
  value = ioapic_rte_read((irq - 32) * 2 + 0x10);
  value = value | 0x10000UL;
  ioapic_rte_write((irq - 32) * 2 + 0x10, value);
}

unsigned long IOAPIC_install(unsigned long irq, void *arg) {
  struct IO_APIC_RET_entry *entry = (struct IO_APIC_RET_entry *)arg;
  ioapic_rte_write((irq - 32) * 2 + 0x10, *(unsigned long *)entry);

  return 1;
}

void IOAPIC_uninstall(unsigned long irq) {
  ioapic_rte_write((irq - 32) * 2 + 0x10, 0x10000UL);
}

void IOAPIC_level_ack(unsigned long irq) {
  __asm__ __volatile__("movq	$0x00,	%%rdx	\n\t"
                       "movq	$0x00,	%%rax	\n\t"
                       "movq 	$0x80b,	%%rcx	\n\t"
                       "wrmsr	\n\t" ::
                           : "memory");

  *ioapic_map.virtual_EOI_address = irq;
}

void IOAPIC_edge_ack(unsigned long irq) {
  __asm__ __volatile__("movq	$0x00,	%%rdx	\n\t"
                       "movq	$0x00,	%%rax	\n\t"
                       "movq 	$0x80b,	%%rcx	\n\t"
                       "wrmsr	\n\t" ::
                           : "memory");
}

void Local_APIC_edge_level_ack(unsigned long irq) {
  __asm__ __volatile__("movq	$0x00,	%%rdx	\n\t"
                       "movq	$0x00,	%%rax	\n\t"
                       "movq 	$0x80b,	%%rcx	\n\t"
                       "wrmsr	\n\t" ::
                           : "memory");
}

unsigned long ioapic_rte_read(unsigned char index) {
  unsigned long ret;

  *ioapic_map.virtual_index_address = index + 1;
  io_mfence();
  ret = *ioapic_map.virtual_data_address;
  ret <<= 32;
  io_mfence();

  *ioapic_map.virtual_index_address = index;
  io_mfence();
  ret |= *ioapic_map.virtual_data_address;
  io_mfence();

  return ret;
}

void ioapic_rte_write(unsigned char index, unsigned long value) {
  *ioapic_map.virtual_index_address = index;
  io_mfence();
  *ioapic_map.virtual_data_address = value & 0xffffffff;
  value >>= 32;
  io_mfence();

  *ioapic_map.virtual_index_address = index + 1;
  io_mfence();
  *ioapic_map.virtual_data_address = value & 0xffffffff;
  io_mfence();
}

void IOAPIC_pagetable_remap() {
  unsigned long *tmp;
  unsigned char *IOAPIC_addr = (unsigned char *)Phy_To_Virt(0xfec00000);

  ioapic_map.physical_address = 0xfec00000;
  ioapic_map.virtual_index_address = IOAPIC_addr;
  ioapic_map.virtual_data_address = (unsigned int *)(IOAPIC_addr + 0x10);
  ioapic_map.virtual_EOI_address = (unsigned int *)(IOAPIC_addr + 0x40);

  Global_CR3 = Get_gdt();

  tmp = Phy_To_Virt(Global_CR3 +
                    (((unsigned long)IOAPIC_addr >> PAGE_GDT_SHIFT) & 0x1ff));
  if (*tmp == 0) {
    unsigned long *virtual = kmalloc(PAGE_4K_SIZE, 0);
    set_mpl4t(tmp, mk_mpl4t(Virt_To_Phy(virtual), PAGE_KERNEL_GDT));
  }

  color_printk(YELLOW, BLACK, "1:%#018lx\t%#018lx\n", (unsigned long)tmp,
               (unsigned long)*tmp);

  tmp = Phy_To_Virt((unsigned long *)(*tmp & (~0xfffUL)) +
                    (((unsigned long)IOAPIC_addr >> PAGE_1G_SHIFT) & 0x1ff));
  if (*tmp == 0) {
    unsigned long *virtual = kmalloc(PAGE_4K_SIZE, 0);
    set_pdpt(tmp, mk_pdpt(Virt_To_Phy(virtual), PAGE_KERNEL_Dir));
  }

  color_printk(YELLOW, BLACK, "2:%#018lx\t%#018lx\n", (unsigned long)tmp,
               (unsigned long)*tmp);

  tmp = Phy_To_Virt((unsigned long *)(*tmp & (~0xfffUL)) +
                    (((unsigned long)IOAPIC_addr >> PAGE_2M_SHIFT) & 0x1ff));
  set_pdt(tmp, mk_pdt(ioapic_map.physical_address,
                      PAGE_KERNEL_Page | PAGE_PWT | PAGE_PCD));

  color_printk(BLUE, BLACK, "3:%#018lx\t%#018lx\n", (unsigned long)tmp,
               (unsigned long)*tmp);

  color_printk(BLUE, BLACK, "ioapic_map.physical_address:%#010x\t\t\n",
               ioapic_map.physical_address);
  color_printk(BLUE, BLACK, "ioapic_map.virtual_address:%#018lx\t\t\n",
               (unsigned long)ioapic_map.virtual_index_address);

  flush_tlb();
}

void Local_APIC_init() {
  unsigned int x, y;
  unsigned int a, b, c, d;

  // check APIC & x2APIC support
  get_cpuid(1, 0, &a, &b, &c, &d);
  // void get_cpuid(unsigned int Mop,unsigned int Sop,unsigned int * a,unsigned
  // int * b,unsigned int * c,unsigned int * d)
  color_printk(WHITE, BLACK,
               "CPUID\t01,eax:%#010x,ebx:%#010x,ecx:%#010x,edx:%#010x\n", a, b,
               c, d);

  if ((1 << 9) & d)
    color_printk(WHITE, BLACK, "HW support APIC&xAPIC\t");
  else
    color_printk(WHITE, BLACK, "HW NO support APIC&xAPIC\t");

  if ((1 << 21) & c)
    color_printk(WHITE, BLACK, "HW support x2APIC\n");
  else
    color_printk(WHITE, BLACK, "HW NO support x2APIC\n");

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

  color_printk(WHITE, BLACK, "eax:%#010x,edx:%#010x\t", x, y);

  if (x & 0xc00)
    color_printk(WHITE, BLACK, "xAPIC & x2APIC enabled\n");

#if VM
  // enable SVR[8]
  __asm__ __volatile__("movq 	$0x80f,	%%rcx	\n\t"
                       "rdmsr	\n\t"
                       "bts	$8,	%%rax	\n\t"
                      //  "bts	$12,%%rax\n\t"
                       "wrmsr	\n\t"
                       "movq 	$0x80f,	%%rcx	\n\t"
                       "rdmsr	\n\t"
                       : "=a"(x), "=d"(y)
                       :
                       : "memory");
#else 
  // enable SVR[8]
  __asm__ __volatile__("movq 	$0x80f,	%%rcx	\n\t"
                       "rdmsr	\n\t"
                       "bts	$8,	%%rax	\n\t"
                       "bts	$12,%%rax\n\t"
                       "wrmsr	\n\t"
                       "movq 	$0x80f,	%%rcx	\n\t"
                       "rdmsr	\n\t"
                       : "=a"(x), "=d"(y)
                       :
                       : "memory");
#endif

  color_printk(WHITE, BLACK, "eax:%#010x,edx:%#010x\t", x, y);

  if (x & 0x100)
    color_printk(WHITE, BLACK, "SVR[8] enabled\n");
  if (x & 0x1000)
    color_printk(WHITE, BLACK, "SVR[12] enabled\n");

  // get local APIC ID
  __asm__ __volatile__("movq $0x802,	%%rcx	\n\t"
                       "rdmsr	\n\t"
                       : "=a"(x), "=d"(y)
                       :
                       : "memory");

  color_printk(WHITE, BLACK, "eax:%#010x,edx:%#010x\tx2APIC ID:%#010x\n", x, y,
               x);

  // get local APIC version
  __asm__ __volatile__("movq $0x803,	%%rcx	\n\t"
                       "rdmsr	\n\t"
                       : "=a"(x), "=d"(y)
                       :
                       : "memory");

  color_printk(WHITE, BLACK,
               "local APIC Version:%#010x,Max LVT Entry:%#010x,SVR(Suppress "
               "EOI Broadcast):%#04x\t",
               x & 0xff, (x >> 16 & 0xff) + 1, x >> 24 & 0x1);

  if ((x & 0xff) < 0x10)
    color_printk(WHITE, BLACK, "82489DX discrete APIC\n");

  else if (((x & 0xff) >= 0x10) && ((x & 0xff) <= 0x15))
    color_printk(WHITE, BLACK, "Integrated APIC\n");

#if VM
  // mask all LVT
  __asm__ __volatile__(//"movq 	$0x82f,	%%rcx	\n\t" // CMCI
                       //"wrmsr	\n\t"
                       "movq 	$0x832,	%%rcx	\n\t" // Timer
                       "wrmsr	\n\t"
                       "movq 	$0x833,	%%rcx	\n\t" // Thermal Monitor
                       "wrmsr	\n\t"
                       "movq 	$0x834,	%%rcx	\n\t" // Performance Counter
                       "wrmsr	\n\t"
                       "movq 	$0x835,	%%rcx	\n\t" // LINT0
                       "wrmsr	\n\t"
                       "movq 	$0x836,	%%rcx	\n\t" // LINT1
                       "wrmsr	\n\t"
                       "movq 	$0x837,	%%rcx	\n\t" // Error
                       "wrmsr	\n\t"
                       :
                       : "a"(0x10000), "d"(0x00)
                       : "memory");
#else
  // mask all LVT
  __asm__ __volatile__("movq 	$0x82f,	%%rcx	\n\t" // CMCI
                       "wrmsr	\n\t"
                       "movq 	$0x832,	%%rcx	\n\t" // Timer
                       "wrmsr	\n\t"
                       "movq 	$0x833,	%%rcx	\n\t" // Thermal Monitor
                       "wrmsr	\n\t"
                       "movq 	$0x834,	%%rcx	\n\t" // Performance Counter
                       "wrmsr	\n\t"
                       "movq 	$0x835,	%%rcx	\n\t" // LINT0
                       "wrmsr	\n\t"
                       "movq 	$0x836,	%%rcx	\n\t" // LINT1
                       "wrmsr	\n\t"
                       "movq 	$0x837,	%%rcx	\n\t" // Error
                       "wrmsr	\n\t"
                       :
                       : "a"(0x10000), "d"(0x00)
                       : "memory");
#endif

  color_printk(GREEN, BLACK, "Mask ALL LVT\n");

  // TPR
  __asm__ __volatile__("movq 	$0x808,	%%rcx	\n\t"
                       "rdmsr	\n\t"
                       : "=a"(x), "=d"(y)
                       :
                       : "memory");

  color_printk(GREEN, BLACK, "Set LVT TPR:%#010x\t", x);

  // PPR
  __asm__ __volatile__("movq 	$0x80a,	%%rcx	\n\t"
                       "rdmsr	\n\t"
                       : "=a"(x), "=d"(y)
                       :
                       : "memory");

  color_printk(GREEN, BLACK, "Set LVT PPR:%#010x\n", x);
}

void IOAPIC_init() {
  int i;
  //	I/O APIC
  //	I/O APIC	ID
  *ioapic_map.virtual_index_address = 0x00;
  io_mfence();
  *ioapic_map.virtual_data_address = 0x0f000000;
  io_mfence();
  color_printk(GREEN, BLACK, "Get IOAPIC ID REG:%#010x,ID:%#010x\n",
               *ioapic_map.virtual_data_address,
               *ioapic_map.virtual_data_address >> 24 & 0xf);
  io_mfence();

  //	I/O APIC	Version
  *ioapic_map.virtual_index_address = 0x01;
  io_mfence();
  color_printk(GREEN, BLACK,
               "Get IOAPIC Version REG:%#010x,MAX redirection enties:%#08d\n",
               *ioapic_map.virtual_data_address,
               ((*ioapic_map.virtual_data_address >> 16) & 0xff) + 1);

  // 写中断定向投递寄存器 RTE，初始化 24 个
  for (i = 0x10; i < 0x40; i += 2)
    ioapic_rte_write(i, 0x10020 + ((i - 0x10) >> 1));

  color_printk(GREEN, BLACK,
               "I/O APIC Redirection Table Entries Set Finished.\n");
}

/*

*/

void APIC_IOAPIC_init() {
  //	init trap abort fault
  int i;
  
  // 映射间接访问寄存器
  IOAPIC_pagetable_remap();
  // 初始化中断向量表，使用 tss.rsp0
  for (i = 32; i < 56; i++) {
    set_intr_gate(i, 0, interrupt[i - 32]);
  }

  // mask 8259A
  color_printk(GREEN, BLACK, "MASK 8259A\n");
  io_out8(0x21, 0xff);
  io_out8(0xa1, 0xff);

  // enable IMCR
  io_out8(0x22, 0x70);
  io_out8(0x23, 0x01);

  // init local apic
  Local_APIC_init();

  // init ioapic
  IOAPIC_init();

  /**
   * https://blog.csdn.net/defrag257/article/details/111939545
   * 芯片组相关的OIC寄存器（较新版本叫IOAC寄存器），实际上是用于BIOS上电初始化的，操作系统不用写这个寄存器！
   * 没有标准化、每个芯片组都不一样的寄存器，一般不是为操作系统编程而设计的，而是为BIOS上电初始化、厂商驱动程序而设计的。
   * 此外IMCR寄存器可选存在于早期基于MP的系统中，对于现代的基于ACPI的系统，不存在IMCR寄存器，但强行写这个寄存器也没事。
   * 操作系统只需要屏蔽8259A（使用8259A的IMR，或屏蔽LINT0和RTE0）、初始化I/O APIC的ID寄存器和24个RTE寄存器即可。
   */
  // get RCBA address
  //  io_out32(0xcf8,0x8000f8f0);
  //  x = io_in32(0xcfc);
  //  color_printk(RED,BLACK,"Get RCBA Address:%#010x\n",x);
  //  x = x & 0xffffc000;
  //  color_printk(RED,BLACK,"Get RCBA Address:%#010x\n",x);

  // //get OIC address
  // if(x > 0xfec00000 && x < 0xfee00000)
  // {
  // 	p = (unsigned int *)Phy_To_Virt(x + 0x31feUL);
  // }

  // //enable IOAPIC
  // x = (*p & 0xffffff00) | 0x100;
  // io_mfence();
  // *p = x;
  // io_mfence();

  memset(interrupt_desc, 0, sizeof(irq_desc_T) * NR_IRQS);

  // open IF eflages
  sti();
}

/**
 * @brief 统一的中断处理函数，所有的中断都先跳转到这里，再根据中断向量号分发给各自的真实处理函数。
 * 以 0x80 为界，以下为外部中断 IOAPIC，以上为内部中断 Local APIC（包括 IPI)
 *
 * @param regs 中断发生时栈顶指针，可访问到所有保存的现场寄存器
 * @param nr 中断向量号
 */
void do_IRQ(struct pt_regs *regs, unsigned long nr) {
  switch (nr & 0x80) {
  case 0x00: { // 外部中断
    irq_desc_T *irq = &interrupt_desc[nr - 32];

    // 调用实际的中断处理函数
    if (irq->handler != NULL)
      irq->handler(nr, irq->parameter, regs);
    // 调用中断应答函数
    if (irq->controller != NULL && irq->controller->ack != NULL)
      irq->controller->ack(nr);
  } break;

  case 0x80: // 核间中断
    // printk_info("SMP IPI:%d, CPU:%d\n",nr,SMP_cpu_id());
    Local_APIC_edge_level_ack(nr);
    {
      irq_desc_T *ipi = &SMP_IPI_desc[nr - 200];
      if (ipi->handler)
        ipi->handler(nr, ipi->parameter, regs);
    }
    break;
  default:
    printk_error("do_IRQ receive:%d\n", nr);
    break;
  }
}
