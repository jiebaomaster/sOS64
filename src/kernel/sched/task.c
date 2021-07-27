#include "task.h"

/**
 * @brief 
 * 
 * @param arg 
 * @return unsigned long 
 */
unsigned long init(unsigned long arg) {
  color_printk(RED, BLACK, "init task is running, arg:%#018lx\n", arg);

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
  p = alloc_pages(ZONE_NORMAL, 1, PG_PTable_Maped | PG_Active | PG_Kernel);
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

  // 初始化新进程的 task_struct
  tsk = (struct task_struct *)Phy_To_Virt(p->PHY_address);
  printk("struct task_struct address:%#018lx\n", (unsigned long)tsk);
  memset(tsk, 0, sizeof(*tsk));
  *tsk = *current; // 新分配的进程控制块拷贝父进程的控制块信息，空间信息也相同

  list_init(&tsk->list);
  // 将新进程加入全局进程链表
  list_add_to_before(&init_task_union.task.list, &tsk->list);
  tsk->pid++;
  tsk->state = TASK_UNINTERRUPTIBLE;
  // 初始化新进程的 thread_struct
  thd = (struct thread_struct *)(tsk + 1);
  tsk->thread = thd;
  // 拷贝执行现场 regs 到栈中，用于后面从栈中构建进程的执行环境
  memcpy(regs,
         (void *)((unsigned long)tsk + STACK_SIZE - sizeof(struct pt_regs)),
         sizeof(struct pt_regs));

  thd->rsp0 = (unsigned long)tsk + STACK_SIZE;
  thd->rip = regs->rip;
  thd->rsp = (unsigned long)tsk + STACK_SIZE - sizeof(struct pt_regs);

  // 若新进程是一个应用层进程，则入口地址改为中断返回地址，因为应用层通过系统调用进入内核层
  // 的 do_fork，应该通过中断返回回到应用层
  if (!(tsk->flags & PF_KTHREAD))
    thd->rip = regs->rip = (unsigned long)ret_from_intr;

  tsk->state = TASK_RUNNING;

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
  // 内核线程的
  regs.rip = (unsigned long)kernel_thread_func;

  return do_fork(&regs, flags, 0, 0);
}

/**
 * @brief 做进程切换前的最后准备
 * 
 * @param prev 当前进程的控制块
 * @param next 将要执行进程的控制块
 */
void __switch_to(struct task_struct *prev, struct task_struct *next) {
  // 更新当前 CPU 的内核栈基地址
  init_tss[0].rsp0 = next->thread->rsp0;

  set_tss64(init_tss[0].rsp0, init_tss[0].rsp1, init_tss[0].rsp2,
            init_tss[0].ist1, init_tss[0].ist2, init_tss[0].ist3,
            init_tss[0].ist4, init_tss[0].ist5, init_tss[0].ist6,
            init_tss[0].ist7);

  // 保存、设置段选择子
  __asm__ __volatile__("movq %%fs, %0 \n\t" : "=a"(prev->thread->fs));
  __asm__ __volatile__("movq %%gs, %0 \n\t" : "=a"(prev->thread->gs));
  __asm__ __volatile__("movq %0, %%fs \n\t" ::"a"(next->thread->fs));
  __asm__ __volatile__("movq %0, %%gs \n\t" ::"a"(next->thread->gs));

  printk("prev->thread->rsp0:%#018lx\n", prev->thread->rsp0);
  printk("next->thread->rsp0:%#018lx\n", next->thread->rsp0);
  printk("next->thread->rip:%#018lx\n", next->thread->rip);
  printk("next->thread->rsp:%#018lx\n", next->thread->rsp);
}

void task_init() {
  struct task_struct *p = NULL;

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

  set_tss64(init_thread.rsp0, init_tss[0].rsp1, init_tss[0].rsp2,
            init_tss[0].ist1, init_tss[0].ist2, init_tss[0].ist3,
            init_tss[0].ist4, init_tss[0].ist5, init_tss[0].ist6,
            init_tss[0].ist7);

  init_tss[0].rsp0 = init_thread.rsp0;
  list_init(&init_task_union.task.list);

  // 创建 1 号进程
  kernel_thread(init, 10, 0);

  // 当前进程状态设置为正在执行
  init_task_union.task.state = TASK_RUNNING;

  // 1 号进程为当前 0 号进程的下一个进程
  p = container_of(list_next(&current->list), struct task_struct, list);

  // 切换到 1 号进程
  switch_to(current, p);
}