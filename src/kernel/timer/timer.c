#include "timer.h"
#include "lib.h"
#include "mm.h"
#include "printk.h"
#include "softirq.h"

// 全局定时器链表头
struct timer timerList;

// 初始化定时器
void timer_init() {
  jiffies = 0;
  // 注册时钟中断为第 0 个软中断
  register_softirq(0, &do_timer, NULL);
  init_timer(&timerList, NULL, NULL, -1UL);
}

/**
 * @brief 定时器软中断处理函数，下半部，每次时钟中断到达时触发。
 * 触发并删除所有到达的定时器
 */
void do_timer(void *data) {
  struct timer *tmp =
      container_of(list_next(&timerList.list), struct timer, list);

  while (!list_is_empty(&timerList.list) && tmp->expire_jiffies <= jiffies) {
    del_timer(tmp);
    tmp->func(tmp->data);
    tmp = container_of(list_next(&timerList.list), struct timer, list);
  }

  printk("(HPET:%ld)", jiffies);
}

/**
 * @brief 初始化一个定时器
 *
 * @param timer 定时器信息结构体
 * @param func 定时器处理方法
 * @param data 处理方法的参数
 * @param expire_jiffies 期望执行时间
 */
void init_timer(struct timer *timer, void (*func)(void *), void *data,
                unsigned long expire_jiffies) {
  list_init(&timer->list);
  timer->expire_jiffies = expire_jiffies;
  timer->func = func;
  timer->data = data;
}

/**
 * @brief 添加一个定时器
 */
void add_timer(struct timer *timer) {
  struct timer *tmp =
      container_of(list_next(&timerList.list), struct timer, list);
  if (!list_is_empty(&timerList.list)) {
    // 找到第一个比 timer 晚到达的定时器
    while (tmp->expire_jiffies < timer->expire_jiffies) {
      tmp = container_of(list_next(&tmp->list), struct timer, list);
    }
  }
  list_add_to_before(&tmp->list, &timer->list);
}

/**
 * @brief 删除一个定时器
 */
void del_timer(struct timer *timer) { 
  list_del(&timer->list); 
}