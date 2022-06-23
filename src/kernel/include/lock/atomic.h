#ifndef __ATOMIC_H__
#define __ATOMIC_H__

// 原子变量
typedef struct {
  volatile long value;
} atomic_T;

#define atomic_read(atomic) ((atomic)->value)
#define atomic_set(atomic, val) (((atomic)->value) = (val))

/**
 * @brief 原子变量 atomic 加 value
 */
static inline void atomic_add(atomic_T *atomic, long value) {
  __asm__ __volatile__("lock addq %1, %0 \n\t"
                       : "=m"(atomic->value)
                       : "r"(value)
                       : "memory");
}

/**
 * @brief 原子变量 atomic 减 value
 */
static inline void atomic_sub(atomic_T *atomic, long value) {
  __asm__ __volatile__("lock	subq	%1,	%0	\n\t"
                       : "=m"(atomic->value)
                       : "r"(value)
                       : "memory");
}

/**
 * @brief 原子变量 atomic 自增 1
 */
static inline void atomic_inc(atomic_T *atomic) {
  __asm__ __volatile__("lock	incq	%0	\n\t"
                       : "=m"(atomic->value)
                       : "m"(atomic->value)
                       : "memory");
}

/**
 * @brief 原子变量 atomic 自减 1
 */
static inline void atomic_dec(atomic_T *atomic) {
  __asm__ __volatile__("lock	decq	%0	\n\t"
                       : "=m"(atomic->value)
                       : "m"(atomic->value)
                       : "memory");
}

/**
 * @brief 原子变量 atomic 按位“或”操作，set 1 位
 */
static inline void atomic_set_mask(atomic_T *atomic, long mask) {
  __asm__ __volatile__("lock	orq	%1,	%0	\n\t"
                       : "=m"(atomic->value)
                       : "r"(mask)
                       : "memory");
}

/**
 * @brief 原子变量 atomic 按位“与”操作，clear 1 位
 */
static inline void atomic_clear_mask(atomic_T *atomic, long mask) {
  __asm__ __volatile__("lock	andq	%1,	%0	\n\t"
                       : "=m"(atomic->value)
                       : "r"(~(mask))
                       : "memory");
}

#endif