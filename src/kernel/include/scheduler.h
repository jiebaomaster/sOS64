#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include "task.h"

// 进程调度器
struct scheduler {
  long running_task_count; // 运行队列长度
  long CPU_exec_task_jiffies; // 当前进程剩余时间片数量
  // 运行队列队头，按进程虚拟运行时间大小排序，优先选择虚拟运行时间最短的运行
  struct task_struct runqueue; 
};

// 全局进程调度器
extern struct scheduler task_scheduler;

/**
 * @brief 更新时间片和虚拟运行时间
 */
void update_cur_runtime();

/**
 * @brief 将 tsk 插入运行队列
 */
void enqueue_task(struct task_struct *tsk);

/**
 * @brief 进行一次进程调度
 */
void schedule();

/**
 * @brief 初始化进程调度器
 */
void scheduler_init();


#endif