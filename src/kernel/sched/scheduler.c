#include "scheduler.h"
#include "printk.h"
#include "lib.h"
#include "list.h"
#include "timer.h"
#include "SMP.h"

// 全局进程调度器
struct scheduler task_scheduler[NR_CPUS];

/**
 * @brief 更新时间片和虚拟运行时间
 */
void update_cur_runtime() {
  // 根据进程优先级对时间片和虚拟运行时间进行修改，优先级越高的进程时间过得越慢
  switch (current->priority) {
  case 0:
  case 1:
    task_scheduler[SMP_cpu_id()].CPU_exec_task_jiffies--;
    current->vruntime += 1;
    break;
  case 2:
  default:
    task_scheduler[SMP_cpu_id()].CPU_exec_task_jiffies -= 2;
    current->vruntime += 2;
    break;
  }

  // 如果当前进程的时间片用完了，标记需要进行调度，在中断返回前进行进程调度
  if (task_scheduler[SMP_cpu_id()].CPU_exec_task_jiffies <= 0)
    current->flags |= NEED_SCHEDULE;
}

/**
 * @brief 获取下一个可运行进程
 */
struct task_struct *pick_next_task() {
  // 执行队列为空，返回 idle
  if (list_is_empty(&task_scheduler[SMP_cpu_id()].runqueue.list)) {
    return init_task[SMP_cpu_id()];
  }
  // 否则返回运行队列中第一个，即虚拟运行时间最小的
  struct task_struct *tsk =
      container_of(list_next(&task_scheduler[SMP_cpu_id()].runqueue.list),
                   struct task_struct, list);
  list_del(&tsk->list);
  task_scheduler[SMP_cpu_id()].running_task_count--;

  return tsk;
}

/**
 * @brief 将 tsk 插入运行队列
 */
void enqueue_task(struct task_struct *tsk) {
  if (tsk == init_task[SMP_cpu_id()]) // idle 进程不进行插入
    return;

  struct task_struct *tmp =
      container_of(list_next(&task_scheduler[SMP_cpu_id()].runqueue.list),
                   struct task_struct, list);
  if (!list_is_empty(&task_scheduler[SMP_cpu_id()].runqueue.list)) {
    // 找到第一个虚拟运行时间更大的
    while (tmp->vruntime < tsk->vruntime) {
      tmp = container_of(list_next(&tmp->list), struct task_struct, list);
    }
  }
  list_add_to_before(&tmp->list, &tsk->list);
  task_scheduler[SMP_cpu_id()].running_task_count++;
}

// 初始化进程时间片
static void init_timeslice(struct task_struct *tsk) {
  if (!task_scheduler[SMP_cpu_id()].CPU_exec_task_jiffies) {
    switch (tsk->priority) {
    case 0:
    case 1:
      task_scheduler[SMP_cpu_id()].CPU_exec_task_jiffies =
          4 / task_scheduler[SMP_cpu_id()].running_task_count;
      break;
    case 2:
    default:
      task_scheduler[SMP_cpu_id()].CPU_exec_task_jiffies =
          4 / task_scheduler[SMP_cpu_id()].running_task_count * 3;
      break;
    }
  }
}

/**
 * @brief 进行一次进程调度
 */
void schedule() {
  // 复位
  current->flags &= ~NEED_SCHEDULE;

  struct task_struct *tsk = pick_next_task();
  printk_info("#schedule: %d#\n", jiffies);

  if (current->vruntime >= tsk->vruntime) {
    // 找到虚拟运行时间更小的进程，切换

    // 如果当前进程还是可运行的，说明是由于时间片用完导致的调度
    // 需要将当前进程插入回队列
    if(current->state == TASK_RUNNING)
      enqueue_task(current);

    init_timeslice(tsk);

    // 切换到 tsk
    switch_to(current, tsk);
  } else {
    // 不存在虚拟运行时间更小的进程，重置时间片，继续执行 current

    enqueue_task(tsk);
    init_timeslice(current);
  }
}

/**
 * @brief 初始化进程调度器
 */
void scheduler_init() {
  int i;
  memset(&task_scheduler, 0, sizeof(struct scheduler) * NR_CPUS);

  for(i = 0; i < NR_CPUS; i++) {
    list_init(&task_scheduler[i].runqueue.list);
    task_scheduler[i].runqueue.vruntime = 0x7fffffffffffffff;
    // 初始队列为空，但是 idle 也算一个可运行进程
    task_scheduler[i].running_task_count = 1;
    task_scheduler[i].CPU_exec_task_jiffies = 4;
  }

}