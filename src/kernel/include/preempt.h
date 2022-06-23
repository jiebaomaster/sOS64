#ifndef __PREEMPT_H__
#define __PREEMPT_H__

#include "task.h"

// 允许抢占
#define preempt_enable() \
do { \
  current->preempt_count--; \
} while(0)

// 禁止抢占
#define preempt_disable() \
do { \
  current->preempt_count++; \
} while(0)

#endif