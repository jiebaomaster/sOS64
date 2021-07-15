#ifndef __LIB_H__
#define __LIB_H__

#define NULL 0

/**
 * @brief 计算字符串长度
 * 
 * @param String 
 * @return int 
 */
static inline int strlen(char *String) {
  register int __res;
  __asm__ __volatile__("cld	\n\t"
                       "repne	\n\t"
                       "scasb	\n\t"
                       "notl	%0	\n\t"
                       "decl	%0	\n\t"
                       : "=c"(__res)
                       : "D"(String), "a"(0), "0"(0xffffffff)
                       :);
  return __res;
}

#endif