#ifndef __PTRACE_H__

#define __PTRACE_H__

// 传给中断异常处理函数的栈顶指针，栈中存储的执行现场寄存器
// 为方便访问定义结构体描述栈中各寄存器的位置
struct pt_regs {
  // 各通用寄存器
  unsigned long r15;
  unsigned long r14;
  unsigned long r13;
  unsigned long r12;
  unsigned long r11;
  unsigned long r10;
  unsigned long r9;
  unsigned long r8;
  unsigned long rbx;
  unsigned long rcx;
  unsigned long rdx;
  unsigned long rsi;
  unsigned long rdi;
  unsigned long rbp;
  unsigned long ds;
  unsigned long es;
  unsigned long rax;
  unsigned long func; // 处理函数的地址，只在异常中有效
  // 以下的寄存器在异常发生时由硬件自动压栈
  unsigned long errcode; // （如果有的话）错误代码
  unsigned long rip;
  unsigned long cs;
  unsigned long rflags;
  // 以下寄存器只会在发生特权级变化时由硬件自动压栈
  unsigned long rsp; // 上一个任务的栈顶地址
  unsigned long ss;  // 上一个任务的栈段选择子
};

#endif