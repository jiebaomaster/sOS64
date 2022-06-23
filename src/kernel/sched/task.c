#include "task.h"
#include "scheduler.h"
#include "gate.h"
#include "mm.h"
#include "printk.h"




/**
 * @brief 一段应用程序，模拟 1 号进程的应用程序部分
 * 
 */
void user_level_func() {
  long ret = 0;
  char string[] = "hello world!\n";

  // printk("user_level_func task is running\n");

  __asm__ __volatile__ (
      "leaq sysexit_return_address(%%rip), %%rdx \n\t" // rdx=rip
      "movq %%rsp,  %%rcx \n\t" // rcx=rsp，以上两句保存应用程序的现场
      "sysenter \n\t"           // 执行系统调用
      "sysexit_return_address:  \n\t"
      : "=a"(ret) // ret=rax
      : "0"(1),"D"(string)   // rax=1，调用 1 号系统调用；rdi=string，用寄存器传递字符串地址给系统调用
      : "memory");

  // printk("user_level_func task called sysenter, ret:%ld\n", ret);

  while (1)
    ;
}

/**
 * @brief 搭建应用程序的执行环境
 * 
 * @param regs 
 * @return unsigned long 
 */
unsigned long do_execve(struct pt_regs *regs) {
  // 搭建应用程序的执行环境，P173
  regs->rdx = 0x800000; // RIP，do_execve返回时，切换到这里执行
  regs->rcx = 0xa00000; // RSP
  regs->rax = 1;
  regs->ds = 0;
  regs->es = 0;
  color_printk(RED, BLACK, "do_execve task is running\n");

  /* 在页表中映射虚拟地址 0x800000 给 init 进程的用户层部分使用 */
  unsigned long addr = 0x800000;
  unsigned long *tmp;
  unsigned long *virtual = NULL;
  struct Page *p = NULL;
  Global_CR3 = Get_gdt();

  tmp = Phy_To_Virt((unsigned long *)((unsigned long)Global_CR3 & (~0xfffUL)) +
                    ((addr >> PAGE_GDT_SHIFT) & 0x1ff));

  virtual = kmalloc(PAGE_4K_SIZE, 0);
  set_mpl4t(tmp, mk_mpl4t(Virt_To_Phy(virtual), PAGE_USER_GDT));

  tmp = Phy_To_Virt((unsigned long *)(*tmp & (~0xfffUL)) +
                    ((addr >> PAGE_1G_SHIFT) & 0x1ff));
  virtual = kmalloc(PAGE_4K_SIZE, 0);
  set_pdpt(tmp, mk_pdpt(Virt_To_Phy(virtual), PAGE_USER_Dir));

  tmp = Phy_To_Virt((unsigned long *)(*tmp & (~0xfffUL)) +
                    ((addr >> PAGE_2M_SHIFT) & 0x1ff));
  p = alloc_pages(ZONE_NORMAL, 1, PG_PTable_Maped);
  set_pdt(tmp, mk_pdt(p->PHY_address, PAGE_USER_Page));

  flush_tlb();

  if(!(current->flags & PF_KTHREAD))
    current->addr_limit = 0xffff800000000000;

  // 将应用程序拷贝到线性地址处
  memcpy(user_level_func, (void *)0x800000, 1024);

  return 0;
}

/**
 * @brief 1号进程的入口函数，跳转到用户空间
 *
 * @param arg
 * @return unsigned long
 */
unsigned long init(unsigned long arg) {
  struct pt_regs *regs = NULL;

  color_printk(RED, BLACK, "init task is running, arg:%#018lx\n", arg);

  current->thread->rip = (unsigned long)ret_system_call;
  current->thread->rsp =
      (unsigned long)current + STACK_SIZE - sizeof(struct pt_regs);
  regs = (struct pt_regs *)current->thread->rsp;
  current->flags = 0;

  // 参考 switch_to，采用 push+jmp 的方法跳转到 ret_system_call 处执行，
  // 在 ret_system_call 中恢复栈中的执行现场，从而进入用户空间
  __asm__ __volatile__("movq %1, %%rsp \n\t"
                       "pushq %2 \n\t"
                       "jmp do_execve \n\t" 
                       ::"D"(regs),"m"(current->thread->rsp),
                        "m"(current->thread->rip)
                       : "memory");

  return 1;
}

/**
 * @brief 创建一个新进程
 *
 * @param regs 新进程的执行现场
 * @param clone_flags
 * @param stack_start
 * @param stack_size
 * @return unsigned long
 */
unsigned long do_fork(struct pt_regs *regs, unsigned long clone_flags,
                      unsigned long stack_start, unsigned long stack_size) {
  struct task_struct *tsk = NULL;
  struct thread_struct *thd = NULL;
  struct Page *p = NULL;

  printk("alloc_pages,bitmap:%#018lx\n", *memory_management_struct.bits_map);
  // TODO 目前还没有实现堆内存分配，直接分配一页内存给新进程
  p = alloc_pages(ZONE_NORMAL, 1, PG_PTable_Maped | PG_Kernel);
  printk("alloc_pages,bitmap:%#018lx\n", *memory_management_struct.bits_map);

  /**
   * 分配给新进程的物理页的空间布局：
   * 低地址   -------   <--- tsk
   *        |*******|      | 新进程的 task_struct
   *        |-------|     --- thd
   *        |#######|      | 新进程的 thread_struct
   *        |-------|     ---
   *        |       |      |
   *        |       |      | 新进程栈空间
   *        |$$$$$$$|      | 执行现场 regs 被拷贝到栈底
   *        |-------|  <---- 栈底 (unsigned long)tsk + STACK_SIZE
   *        |       | 
   *        |       |
   *        |       |
   *         ......
   *        |       |
   * 高地址   -------
   */

  /* 初始化新进程的 task_struct */
  tsk = (struct task_struct *)Phy_To_Virt(p->PHY_address);
  printk("struct task_struct address:%#018lx\n", (unsigned long)tsk);
  memset(tsk, 0, sizeof(*tsk));
  *tsk = *current; // 新分配的进程控制块拷贝父进程的控制块信息，空间信息也相同

  list_init(&tsk->list);
  tsk->priority = 2;
  tsk->pid++;
  tsk->preempt_count = 0;
  tsk->state = TASK_UNINTERRUPTIBLE;
  /* 初始化新进程的 thread_struct */
  // 在 task_struct 的后面存放 thread_struct
  thd = (struct thread_struct *)(tsk + 1);
  memset(thd, 0, sizeof(thd));
  tsk->thread = thd;
  // 拷贝执行现场 regs 到栈中，用于后面从栈中构建进程的执行环境
  memcpy(regs,
         (void *)((unsigned long)tsk + STACK_SIZE - sizeof(struct pt_regs)),
         sizeof(struct pt_regs));

  thd->rsp0 = (unsigned long)tsk + STACK_SIZE;
  thd->rip = regs->rip;
  thd->rsp = (unsigned long)tsk + STACK_SIZE - sizeof(struct pt_regs);
  thd->fs = KERNEL_DS;
  thd->gs = KERNEL_DS;

  // 若新进程是一个应用层进程，则入口地址改为统一的系统调用返回入口，
  // 因为应用层通过系统调用进入内核层的 do_fork
  if (!(tsk->flags & PF_KTHREAD))
    thd->rip = regs->rip = (unsigned long)ret_system_call;

  tsk->state = TASK_RUNNING;
  // 将新进程加入运行队列
  enqueue_task(tsk);

  return 0;
}

/**
 * @brief 内核线程执行完之后调用该退出函数，回收线程资源
 * 
 * @param code 内核线程函数的返回值
 * @return unsigned long 
 */
unsigned long do_exit(unsigned long code) {
  color_printk(RED, BLACK, "exit task is running, arg:%#018lx\n", code);

  // TODO 线程回收比较复杂，后期实现，目前先原地等待
  while (1)
    ;
}

/**
 * @brief 统一的系统调用入口函数，从栈的 rax 寄存器中读取系统调用向量号，
 *        并调用相应的处理程序
 * 
 * @param regs 当前栈顶指针，应用程序的执行环境
 * @return unsigned long 
 */
unsigned long system_call_function(struct pt_regs * regs) {
  return system_call_table[regs->rax](regs);
}

/**
 * 内核线程的入口
 * 从栈中恢复执行现场，运行内核线程函数，最后退出进程
 */
extern void kernel_thread_func(void);
__asm__("kernel_thread_func:	\n\t"
        " popq  %r15  \n\t" /* 1. 恢复通用寄存器 */
        " popq  %r14  \n\t"
        " popq  %r13  \n\t"
        " popq  %r12  \n\t"
        " popq  %r11  \n\t"
        " popq  %r10  \n\t"
        " popq  %r9 \n\t"
        " popq  %r8 \n\t"
        " popq  %rbx  \n\t"
        " popq  %rcx  \n\t"
        " popq  %rdx  \n\t"
        " popq  %rsi  \n\t"
        " popq  %rdi  \n\t"
        " popq  %rbp  \n\t"
        " popq  %rax  \n\t"
        " movq  %rax, %ds \n\t"
        " popq  %rax  \n\t"
        " movq  %rax, %es \n\t"
        " popq  %rax  \n\t"
        " addq  $0x38, %rsp \n\t" /* pt_regs 中剩余 7 个成员出栈*/
        /////////////////////////////////
        " movq  %rdx, %rdi  \n\t" /* 内核线程的参数 */
        " callq *%rbx \n\t" /* 2. 调用内核线程执行函数 */
        " movq  %rax, %rdi  \n\t" /* 内核线程函数的返回值 */
        " callq do_exit \n\t"     /* 3. 调用线程结束函数 */
        );

/**
 * @brief 新建一个内核线程
 * 
 * @param fn 内核线程执行的函数
 * @param arg 传给内核线程函数的参数
 * @param flags 
 * @return int 
 */
int kernel_thread(unsigned long (*fn)(unsigned long), unsigned long arg,
                  unsigned long flags) {
  struct pt_regs regs;
  memset(&regs, 0, sizeof(regs));

  // 构造一个内核线程的执行现场，只要从栈中恢复该现场即可创建正确的内核线程执行环境
  regs.rbx = (unsigned long)fn; // 保存内核线程执行的函数地址
  regs.rdx = (unsigned long)arg; // 保存内核线程函数的参数

  regs.ds = KERNEL_DS;
  regs.es = KERNEL_DS;
  regs.cs = KERNEL_CS;
  regs.ss = KERNEL_DS;
  regs.rflags = (1 << 9);
  // 内核线程的统一入口
  regs.rip = (unsigned long)kernel_thread_func;

  return do_fork(&regs, flags, 0, 0);
}

/**
 * @brief 做进程切换前的最后准备，此时 rsp 已经是 next 的栈了
 * 
 * @param prev 当前进程的控制块
 * @param next 将要执行进程的控制块
 */
void __switch_to(struct task_struct *prev, struct task_struct *next) {
  // 更新当前 CPU 的内核栈基地址
  init_tss[0].rsp0 = next->thread->rsp0;

  set_tss64(TSS64_TABLE, init_tss[0].rsp0, init_tss[0].rsp1, init_tss[0].rsp2,
            init_tss[0].ist1, init_tss[0].ist2, init_tss[0].ist3,
            init_tss[0].ist4, init_tss[0].ist5, init_tss[0].ist6,
            init_tss[0].ist7);

  // 保存、设置段选择子
  __asm__ __volatile__("movq %%fs, %0 \n\t" : "=a"(prev->thread->fs));
  __asm__ __volatile__("movq %%gs, %0 \n\t" : "=a"(prev->thread->gs));
  __asm__ __volatile__("movq %0, %%fs \n\t" ::"a"(next->thread->fs));
  __asm__ __volatile__("movq %0, %%gs \n\t" ::"a"(next->thread->gs));

  // 每个进程都有独立的内核栈，设置 sysenter 使用的内核栈
  wrmsr(0x175, next->thread->rsp0);

  printk("prev->thread->rsp0:%#018lx\t", prev->thread->rsp0);
  printk("prev->thread->rsp:%#018lx\n", prev->thread->rsp);
  printk("next->thread->rsp0:%#018lx\t", next->thread->rsp0);
  printk("next->thread->rsp:%#018lx\n", next->thread->rsp);
}

void task_init() {
  struct task_struct *tsk = NULL;

  // 初始化 0 号进程的内存空间结构体
  init_mm.pgd = (pml4t_t *)Global_CR3;
  init_mm.start_code = memory_management_struct.start_code;
  init_mm.end_code = memory_management_struct.end_code;
  init_mm.start_data = (unsigned long)&_data;
  init_mm.end_data = memory_management_struct.end_data;
  init_mm.start_rodata = (unsigned long)&_rodata;
  init_mm.end_rodata = (unsigned long)&_erodata;
  init_mm.start_brk = 0;
  init_mm.end_brk = memory_management_struct.end_brk;
  init_mm.start_stack = _stack_start;

  // P173 P180 设置 0 特权级下的代码段选择子
  wrmsr(0x174, KERNEL_CS);
  // P180 sysenter 进入内核层载入的 ESP
	wrmsr(0x175, current->thread->rsp0);
	// P180 sysenter 进入内核层载入的 EIP
  wrmsr(0x176, (unsigned long)system_call);

  set_tss64(TSS64_TABLE, init_thread.rsp0, init_tss[0].rsp1, init_tss[0].rsp2,
            init_tss[0].ist1, init_tss[0].ist2, init_tss[0].ist3,
            init_tss[0].ist4, init_tss[0].ist5, init_tss[0].ist6,
            init_tss[0].ist7);

  init_tss[0].rsp0 = init_thread.rsp0;
  list_init(&init_task_union.task.list);

  // 创建 1 号进程
  kernel_thread(init, 10, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);

  // 当前进程 idle 状态设置为正在执行
  init_task_union.task.state = TASK_RUNNING;
  init_task_union.task.preempt_count = 0;
}