#include "SMP.h"
#include "cpu.h"
#include "lib.h"
#include "printk.h"
#include "gate.h"
#include "interrupt.h"
#include "task.h"
#include "scheduler.h"

extern int global_i;

// 同步 BSP 和 AP 的启动过程
// TODO 应该使用信号量而不是锁
spinlock_T SMP_lock;

/**
 * @brief IPI200 中断处理 BSP 转发来的时钟中断，进行进程调度
 */
void IPI_0x200(unsigned long nr, unsigned long parameter,
               struct pt_regs *regs) {
  update_cur_runtime();
}

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

  // 初始化 IPI 中断向量表，使用 tss.rsp0
  for (i = 200; i < 210; i++) {
    set_intr_gate(i, 0, SMP_interrupt[i - 200]);
  }
  memset(SMP_IPI_desc, 0, sizeof(irq_desc_T) * 10);
  // 注册 IPI200 中断处理 BSP 转发来的时钟中断，进行进程调度
  register_IPI(200, NULL, &IPI_0x200, NULL, NULL, "IPI 0x200");
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

  /* 初始化 AP 的 idle 进程的 PCB */
  current->state = TASK_RUNNING;
  current->flags = PF_KTHREAD;
  current->mm = &init_mm;
  
  list_init(&current->list);
  current->addr_limit = 0xffff800000000000;
  current->priority = 2;
  current->vruntime = 0;
  
  current->thread = (struct thread_struct*)(current + 1);
  memset(current->thread, 0, sizeof(struct thread_struct));
  current->thread->rsp0 = _stack_start;
  current->thread->rsp = _stack_start;
  current->thread->fs = KERNEL_DS;
  current->thread->gs = KERNEL_DS;
  init_task[SMP_cpu_id()] = current;

  // 加载属于 AP 的 TSS
  load_TR(10 + (global_i - 1) * 2);
  
  // AP 启动完毕后释放信号量，使得 BSP 启动下一个 AP
  spin_unlock(&SMP_lock);
  current->preempt_count = 0;

  sti();

  if (SMP_cpu_id() == 3)
    task_init();

  while(1)
    hlt();
}