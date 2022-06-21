#ifndef __TIMER_H__
#define __TIMER_H__

// 全局时钟计数器，记录定时器开始
volatile unsigned long jiffies = 0;

// 初始化定时器
void timer_init();

// 时钟中断处理函数
void do_timer();

#endif