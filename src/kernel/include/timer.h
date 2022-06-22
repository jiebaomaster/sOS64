#ifndef __TIMER_H__
#define __TIMER_H__

#include "list.h"

// 全局时钟计数器，记录 HPET 定时器产生的中断次数
// 通过 RTC time 和 jiffies 可以推算当前时间
volatile unsigned long jiffies = 0;

// 初始化定时器子系统
void timer_init();

// 时钟中断处理函数
void do_timer();

// 定时器，链表结构
struct timer {
  struct List list;
  unsigned long expire_jiffies;
  void (*func)(void* data);
  void *data;
};

// 全局定时器链表头
extern struct timer timerList;

/**
 * @brief 初始化一个定时器
 * 
 * @param timer 定时器信息结构体
 * @param func 定时器处理方法
 * @param data 处理方法的参数
 * @param expire_jiffies 期望执行时间
 */
void init_timer(struct timer *timer, void (*func)(void *), void *data,
                unsigned long expire_jiffies);

/**
 * @brief 添加一个定时器
 */
void add_timer(struct timer *timer);

/**
 * @brief 删除一个定时器
 */
void del_timer(struct timer *timer);
#endif