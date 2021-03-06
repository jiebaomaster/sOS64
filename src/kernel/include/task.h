#ifndef __TASK_H__
#define __TASK_H__

#include "list.h"
#include "mm.h"
#include "cpu.h"
#include "ptrace.h"

/* 各段在 GDT 中的偏移*/
#define KERNEL_CS (0x08)
#define KERNEL_DS (0x10)

#define USER_CS (0x28)
#define USER_DS (0x30)

/* do_fork 参数 clone_flags */
#define CLONE_FS (1 << 0) // 拷贝
#define CLONE_FILES (1 << 1) // 拷贝打开的文件
#define CLONE_SIGNAL (1 << 2) // 拷贝信号

// 内核线程栈的大小 32KB
#define STACK_SIZE 32768

// 链接脚本中定义的记录段地址的变量
extern char _text;
extern char _etext;
extern char _data;
extern char _edata;
extern char _rodata;
extern char _erodata;
extern char _bss;
extern char _ebss;
extern char _end;

extern unsigned long _stack_start; // 0 号进程的栈基地址，head.S
extern void system_call(); // 执行系统调用的进入，保存应用程序的执行现场
extern void ret_system_call(); // 执行系统调用的返回，从栈中恢复现场，entry.S

// 内存空间分布结构体，记录进程页表和各程序段信息
struct mm_struct {
  pml4t_t *pgd; // 顶级页表的指针

  unsigned long start_code, end_code;     // 代码段空间
  unsigned long start_data, end_data;     // 数据段空间
  unsigned long start_rodata, end_rodata; // 只读数据段空间
  unsigned long start_brk, end_brk; // 动态内存分配区（堆区域）
  unsigned long start_stack;        // 应用层栈基地址
};

// 进程切换时保留的状态信息
struct thread_struct {
  unsigned long rsp0; // 内核层栈基地址
  unsigned long rip;  // 内核层代码指针
  unsigned long rsp;  // 内核层当前栈指针

  unsigned long fs;
  unsigned long gs;

  unsigned long cr2;        // 缺页异常发生的地址
  unsigned long trap_nr;    // 产生异常时的异常号
  unsigned long error_code; // 异常的错误代码
};

/* task_struct->state */
#define TASK_RUNNING (1 << 0) // 运行态
#define TASK_INTERRUPTIBLE (1 << 1) // 阻塞，可中断
#define TASK_UNINTERRUPTIBLE (1 << 2) // 阻塞，不可中断
#define TASK_ZOMBLE (1 << 3) // 僵尸进程
#define TASK_STOPPED (1 << 4) // 停止

/* task_struct->flags */
// 内核线程
#define PF_KTHREAD (1UL << 0)
// 需要调度
#define NEED_SCHEDULE (1UL << 1)

// 进程控制块，记录和收集程序运行时的资源消耗信息，并维护程序运行的现场环境
struct task_struct {
  volatile long state; // 进程状态：运行态、停止态、可中断态等
  unsigned long flags; // 进程标志：进程、线程、内核线程
  /**
   * 抢占标志，记录持有自旋锁的数量，持有自旋锁期间不能进行抢占调度
   * 在中断返回阶段检查 preempt_count 判断是否允许调度，
   * 这会导致持有自旋锁时，即便是时间片用完也不会调度，但是下一次时钟
   * 中断到来时，时间片减为负，会再次触发调度
   */
  long preempt_count;
  long signal; // 进程持有的信号
  
  /* 上面成员的位置固定，见 entry.S */

  long cpu_id; // 进程当前所在处理器的编号

  struct mm_struct *mm;         // 进程的页表和各程序段信息
  struct thread_struct *thread; // 进程切换时保留的状态信息

  struct List list;    // 链接于运行队列，等待队列

  unsigned long
      addr_limit; // 进程地址空间范围
                  // 0x0000,0000,0000,0000 - 0x0000,7fff,ffff,ffff 应用层
                  // 0xffff,8000,0000,0000 - 0xffff,ffff,ffff,ffff 内核层

  long pid;      // 进程号
  long priority; // 进程优先级

  long vruntime; // 进程的虚拟运行时间
};



/**
 * 进程内核态栈和进程 task_struct 的联合体
 * 每一个进程都有独立的内核态栈，而进程的 task_struct 就放置在内核态栈的低地址空间内
 * 每个联合体对齐到 32KB，原因详见 get_current()
 * 
 * 高地址   -------   <--- 栈基地址，栈从高地址向低地址扩张
 *        |       |      |  
 *        |       |      | 内核态栈空间，大约 31KB
 *        |       |      |
 *        |-------|     ---
 *        |*******|      | task_struct 空间，大约 1KB
 * 低地址   -------   <---- task_struct 起始地址
 */
union task_union {
  struct task_struct task;
  unsigned long stack[STACK_SIZE / sizeof(unsigned long)];
} __attribute__((aligned(8))); // 按 8 字节对齐

struct mm_struct init_mm;
struct thread_struct init_thread;

#define INIT_TASK(task) \
{ \
  .state = TASK_INTERRUPTIBLE, \
  .flags = PF_KTHREAD, \
  .preempt_count = 0, \
  .signal = 0, \
  .cpu_id = 0, \
  .mm = &init_mm, \
  .thread = &init_thread, \
  .addr_limit = 0xffff800000000000, \
  .pid = 0, \
  .priority = 2, \
  .vruntime = 0 \
}

// BSP 的栈空间
extern union task_union init_task_union;
// 所有 CPU 的 idle 进程的 PCB，其中 BSP 采用静态创建，AP 的 PCB 在 BSP 中动态创建
extern struct task_struct *init_task[NR_CPUS];

extern struct mm_struct init_mm;
extern struct thread_struct init_thread;

struct tss_struct {
  unsigned int reserved0;
  unsigned long rsp0;
  unsigned long rsp1;
  unsigned long rsp2;
  unsigned long reserved1;
  unsigned long ist1;
  unsigned long ist2;
  unsigned long ist3;
  unsigned long ist4;
  unsigned long ist5;
  unsigned long ist6;
  unsigned long ist7;
  unsigned long reserved2;
  unsigned short reserved3;
  unsigned short iomapbaseaddr;
}__attribute((packed));

#define INIT_TSS { \
  .reserved0 = 0, \
  .rsp0 = (unsigned long)(init_task_union.stack + STACK_SIZE / sizeof(unsigned long)), \
  .rsp1 = (unsigned long)(init_task_union.stack + STACK_SIZE / sizeof(unsigned long)), \
  .rsp2 = (unsigned long)(init_task_union.stack + STACK_SIZE / sizeof(unsigned long)), \
  .reserved1 = 0, \
  .ist1 = 0xffff800000007c00, \
  .ist2 = 0xffff800000007c00, \
  .ist3 = 0xffff800000007c00, \
  .ist4 = 0xffff800000007c00, \
  .ist5 = 0xffff800000007c00, \
  .ist6 = 0xffff800000007c00, \
  .ist7 = 0xffff800000007c00, \
  .reserved2 = 0, \
  .reserved3 = 0, \
  .iomapbaseaddr = 0 \
}

// 为每一个 CPU 设置一个 TSS
extern struct tss_struct init_tss[NR_CPUS];

/**
 * @brief 由当前的栈顶指针获取当前执行进程的 task_struct
 * 当前执行进程的内核态栈的栈顶指针必定在 rsp 寄存器中，而 task_struct 在栈的低地址处
 * 联合体对齐到 32KB，故当前栈顶地址按 32KB 下边界对齐后就是当前进程的 task_struct 起始地址
 *
 * 32767（32KB-1）取反=0xffffffffffff8000，
 * 再与 rsp 的值逻辑与，即将 rsp 按 32KB 下边界对齐
 * 
 * @return struct task_struct* 当前执行进程的 task_struct
 */
static inline struct task_struct * get_current() {
  struct task_struct *current = NULL;
  __asm__ __volatile__ ("andq %%rsp,%0 \n\t":"=r"(current):"0"(~32767UL));
  return current;
}

#define current get_current()

// 同 get_current()
#define GET_CURRENT \
  "movq %rsp, %rbx \n\t" \
  "andq $-32768, %rbx \n\t"

// 进程切换 prev => next
#define switch_to(prev, next)                                                  \
  do {                                                                         \
    __asm__ __volatile__("pushq %%rbp \n\t"                                    \
                         "pushq %%rax \n\t"                                    \
                         "movq %%rsp, %0 \n\t" /* 保存prev的栈指针 */            \
                         "movq %2, %%rsp \n\t" /* 切换到next的栈*/               \
                         "leaq 1f(%%rip), %%rax \n\t" /* 保存 prev 继续执行的地址*/\
                         "movq	%%rax,	%1	\n\t" /* prev->thread->rip = &1 */ \
                         "pushq %3 \n\t" /* 压入 next 的入口地址，jmp 返回时进入 */ \
                         "jmp __switch_to \n\t"                                \
                         "1: \n\t" /* prev继续执行的地址，下次调度到prev从这里开始执行 */\
                         "popq %%rax \n\t"                                     \
                         "popq %%rbp \n\t"                                     \
                         : "=m"(prev->thread->rsp), "=m"(prev->thread->rip)    \
                         : "m"(next->thread->rsp), "m"(next->thread->rip),     \
                           "D"(prev), "S"(next) /* prev和next保存在rdi和rsi中，传参给__switch_to */ \
                         : "memory");                                          \
  } while(0)

/**
 * @brief 1号进程的入口函数，跳转到用户空间
 */
unsigned long init(unsigned long arg);

unsigned long do_fork(struct pt_regs *regs, unsigned long clone_flags,
                      unsigned long stack_start, unsigned long stack_size);

/**
 * @brief BSP 中对 idle 进程的初始化
 */
void task_init();


#define MAX_SYSTEM_CALL_NR 128 // 最多 128 个系统调用

// 系统调用函数指针类型
typedef unsigned long (*system_call_t)(struct pt_regs *regs);

extern system_call_t system_call_table[MAX_SYSTEM_CALL_NR];

#endif