#ifndef __SOFTIRQ_H__
#define __SOFTIRQ_H__

// 全局软中断状态，64 位，某一位为 1，表示存在某一类的软中断未处理
unsigned long softirq_status = 0;

// 时钟中断使用 softirq_status 的第一位
#define TIMER_SIRQ (1<<0)

// 中断下半部，软中断
struct softirq {
  void (*action)(void *data); // 软中断处理函数
  void* data; // 软中断处理函数的参数
};

// 全局软中断向量表，只有 64 项，对应与 softirq_status 的每一位
struct softirq softirq_vector[64] = {0};

/**
 * @brief 注册一个软中断
 * 
 * @param nr 中断向量号
 * @param action 软中断处理函数
 * @param data 软中断处理函数的参数
 */
void register_softirq(int nr, void (*action)(void *data), void* data);

/**
 * @brief 注销一个软中断
 * 
 * @param nr 中断向量号
 */
void unregister_softirq(int nr);


void set_softirq_status(unsigned long status);
unsigned long get_softirq_status();

// 初始化软中断
void softirq_init();

/**
 * @brief 处理系统中当前存在的所有软中断
 */
void do_softirq();

#endif