#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

// 自旋锁
typedef struct {
  __volatile__ unsigned long lock; // 1=unlock, 0=lock
} spinlock_T;

// 初始化自旋锁
static inline void spin_init(spinlock_T *lock) { lock->lock = 1; }

// 自旋锁上锁
static inline void spin_lock(spinlock_T *lock) {
  __asm__ __volatile__("1: \n\t"
                       "lock decq %0 \n\t" // lock 原子减 1
                       "jns 3f \n\t" // lock >= 0 获取锁成功，跳到 3
                       "2: \n\t"
                       "pause \n\t"
                       "cmpq $0, %0 \n\t"
                       "jle 2b \n\t" // if(lock <= 0) 跳到 2，继续判断
                       "jmp 1b \n\t" // 否则，跳到 1，尝试获取锁
                       "3: \n\t"
                       : "=m"(lock->lock)
                       :
                       : "memory"
  );
}

// 自旋锁解锁
static inline void spin_unlock(spinlock_T* lock) {
  __asm__ __volatile__("movq $1, %0 \n\t" // 给 lock 置 1
                       : "=m"(lock->lock)
                       :
                       : "memory");
}

#endif