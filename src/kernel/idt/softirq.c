#include "softirq.h"
#include "lib.h"

/**
 * @brief 注册一个软中断
 *
 * @param nr 中断向量号
 * @param action 软中断处理函数
 * @param data 软中断处理函数的参数
 */
void register_softirq(int nr, void (*action)(void *data), void *data) {
  softirq_vector[nr].action = action;
  softirq_vector[nr].data = data;
}

/**
 * @brief 注销一个软中断
 *
 * @param nr 中断向量号
 */
void unregister_softirq(int nr) {
  softirq_vector[nr].action = NULL;
  softirq_vector[nr].data = NULL;
}

void set_softirq_status(unsigned long status) { softirq_status |= status; }
unsigned long get_softirq_status() { return softirq_status; }

// 初始化软中断
void softirq_init() {
  softirq_status = 0;
  memset(softirq_vector, 0, sizeof(struct softirq) * 64);
}

/**
 * @brief 处理系统中当前存在的所有软中断，下半部
 */
void do_softirq() {
  int i;
  
  /**
   * 此时还在中断处理上下文中，且中断关闭
   * 中断上半部在 关中断 环境下执行，中断下半部在 开中断 环境下执行
   * 开中断允许嵌套中断，提高了中断响应速度
   */
  // 开中断
  sti();
  // 处理所有软中断
  while (softirq_status) {
    for (i = 0; i < 64; i++) {
      if (softirq_status & (1 << i)) {
        // 调用软中断处理函数
        softirq_vector[i].action(softirq_vector[i].data);
        softirq_status &= ~(1 << i); // 复位
      }
    }
  }

  cli();
}