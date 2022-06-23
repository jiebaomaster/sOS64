#include "lock/semaphore.h"
#include "lib.h"
#include "list.h"
#include "scheduler.h"
#include "task.h"

void semaphore_waiter_init(semaphore_waiter *waiter, struct task_struct *tsk) {
  list_init(&waiter->list);
  waiter->tsk = tsk;
}

/**
 * @brief 初始化信号量
 * 
 * @param count 持有的资源数
 */
void semaphore_init(semaphore_T *semaphore, unsigned long count) {
  atomic_set(&semaphore->counter, count);
  semaphore_waiter_init(&semaphore->waiter_head, NULL);
} 

void __up(semaphore_T *semaphore) {
  semaphore_waiter *waiter = container_of(list_next(&semaphore->waiter_head), semaphore_waiter, list);
  // 从等待队列中摘下
  list_del(&waiter->list);
  waiter->tsk->state = TASK_RUNNING;
  // 插入就绪队列
  enqueue_task(waiter->tsk);
}

/**
 * @brief 释放一个资源
 * FIXME 需要锁
 */
void semaphore_up(semaphore_T *semaphore) {
  if(list_is_empty(&semaphore->waiter_head.list)) {
    // 没有等待进程，递增资源计数
    atomic_inc(&semaphore->counter);
  } else {
    // 否则，唤醒等待进程
    __up(semaphore);
  }
}

void __down(semaphore_T *semaphore) {
  // 栈上对象，调度回来时会在这个函数中返回，对象并不会销毁
  semaphore_waiter waiter;
  semaphore_waiter_init(&waiter, current);
  
  current->state = TASK_UNINTERRUPTIBLE;
  // 插入到等待队列中
  list_add_to_before(&semaphore->waiter_head.list, &waiter.list);

  // 主动触发一次调度，因为当前进程进入等待了  
  schedule();
}

/**
 * @brief 获取一个资源
 * FIXME 需要锁
 */
void semaphore_down(semaphore_T *semaphore) {
  if(atomic_read(&semaphore->counter) > 0) {
    // 资源足够
    atomic_dec(&semaphore->counter);
  } else {
    // 资源不足，进入等待队列
    __down(semaphore);
  }
}