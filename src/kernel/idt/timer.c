#include "timer.h"
#include "lib.h"
#include "printk.h"
#include "softirq.h"

// 初始化定时器
void timer_init() {
  jiffies = 0;
  // 注册时钟中断为第 0 个软中断
  register_softirq(0, &do_timer, NULL);
}

// 时钟软中断处理函数，下半部
void do_timer(void *data) { 
  printk_info("(HPET:%ld)", jiffies); 
}
