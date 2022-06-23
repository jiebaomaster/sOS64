#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include "lock/atomic.h"
#include "list.h"

typedef struct {
  struct List list;
  struct task_struct *tsk; // 指向正在等待信号量的进程
} semaphore_waiter;


// 信号量
typedef struct {
  atomic_T counter; // 信号量持有的资源数
  semaphore_waiter waiter_head; // 等待信号量的进程队列
} semaphore_T;

/**
 * @brief 初始化信号量
 * 
 * @param count 持有的资源数
 */
void semaphore_init(semaphore_T *semaphore, unsigned long count);

/**
 * @brief 释放一个资源
 */
void semaphore_up(semaphore_T *semaphore);

/**
 * @brief 获取一个资源
 */
void semaphore_down(semaphore_T *semaphore);

#endif