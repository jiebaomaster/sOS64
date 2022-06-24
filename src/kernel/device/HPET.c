#include "HPET.h"
#include "APIC.h"
#include "lib.h"
#include "printk.h"
#include "time.h"
#include "mm.h"
#include "timer.h"
#include "softirq.h"
#include "scheduler.h"

hw_int_controller HPET_int_controller = {.enable = IOAPIC_enable,
                                         .disable = IOAPIC_disable,
                                         .install = IOAPIC_install,
                                         .uninstall = IOAPIC_uninstall,
                                         .ack = IOAPIC_edge_ack};

// 时钟中断，上半部
void HPET_handler(unsigned long nr, unsigned long parameter,
                  struct pt_regs *regs) {
  jiffies++;
  
  // 如果存在到期的定时器，标记需要处理定时器软中断
  if (container_of(list_next(&timerList.list), struct timer, list)
          ->expire_jiffies <= jiffies)
    set_softirq_status(TIMER_SIRQ);

  // BSP 进程调度统计
  update_cur_runtime();

  // BSP 广播 IPI 给所有 AP，模拟时钟中断，驱动 AP 上的进程调度
  struct INT_CMD_REG icr_entry;
  memset(&icr_entry, 0, sizeof(struct INT_CMD_REG));
  icr_entry.vector = 0xc8;
  icr_entry.dest_shorthand = ICR_ALL_EXCLUDE_Self;
  icr_entry.trigger = APIC_ICR_IOAPIC_Edge;
  icr_entry.dest_mode = ICR_IOAPIC_DELV_PHYSICAL;
  icr_entry.deliver_mode = APIC_ICR_IOAPIC_Fixed;
  wrmsr(0x830, *(unsigned long *)&icr_entry);
}

/**
 * 初始化 HPET 驱动程序
 */
void HPET_init() {
  struct IO_APIC_RET_entry entry;

  // init I/O APIC IRQ2 => 34 号中断 HPET Timer 0
  entry.vector = 34;
  entry.deliver_mode = APIC_ICR_IOAPIC_Fixed;
  entry.dest_mode = ICR_IOAPIC_DELV_PHYSICAL;
  entry.deliver_status = APIC_ICR_IOAPIC_Idle;
  entry.polarity = APIC_IOAPIC_POLARITY_HIGH;
  entry.irr = APIC_IOAPIC_IRR_RESET;
  entry.trigger = APIC_ICR_IOAPIC_Edge;
  entry.mask = APIC_ICR_IOAPIC_Masked;
  entry.reserved = 0;

  entry.destination.physical.reserved1 = 0;
  entry.destination.physical.phy_dest = 0;
  entry.destination.physical.reserved2 = 0;
  // 注册定时中断处理函数
  register_irq(34, &entry, &HPET_handler, NULL, &HPET_int_controller, "HPET");
	

  unsigned char * HPET_addr = (unsigned char *)Phy_To_Virt(0xfed00000);
  color_printk(RED, BLACK, "HPET - GCAP_ID:<%#018lx>\n",
               *(unsigned long *)HPET_addr);
  // 配置 GEN_CONF，使定时器 0 向 IOAPIC IRQ2 引脚发送中断请求 
  *(unsigned long *)(HPET_addr + 0x10) = 3;
  io_mfence();

  // 周期性，边缘触发
  *(unsigned long *)(HPET_addr + 0x100) = 0x004c;
  io_mfence();

  // 每秒触发一次，
#if VM
  *(unsigned long *)(HPET_addr + 0x108) = 14318179;
#else
  // intel 400 芯片组时间精度为 41.666667ns
  *(unsigned long *)(HPET_addr + 0x108) = 23999999;
#endif
  io_mfence();

  /**
   * 定时器 0 之后会用来管理 RTC 时间，在获取 RTC 时间之后立马开始计数
   * 以缩短 RTC 时间和定时器 0 的计数时间间隔
   */
  // 获取 CMOS 中的 RTC 寄存器中的时间
  get_cmos_time(&time);

  // 向主计数器 MIAN_CNT 写入 0
  *(unsigned long *)(HPET_addr + 0xf0) = 0;
  io_mfence();

  color_printk(RED, BLACK,
               "year%#010x,month:%#010x,day:%#010x,hour:%#010x,mintue:%#010x,"
               "second:%#010x\n",
               time.year, time.month, time.day, time.hour, time.minute,
               time.second);
}