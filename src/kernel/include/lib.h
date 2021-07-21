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

/**
 * @brief 将地址 Address 开始的 Count 个字符，设置为 C
 * 
 * @param Address 
 * @param C 
 * @param Count 
 * @return void* 
 */
static inline void *memset(void *Address, unsigned char C, long Count) {
  int d0, d1;
  unsigned long tmp = C * 0x0101010101010101UL;
  __asm__ __volatile__("cld	\n\t"
                       "rep	\n\t"
                       "stosq	\n\t"
                       "testb	$4, %b3	\n\t"
                       "je	1f	\n\t"
                       "stosl	\n\t"
                       "1:\ttestb	$2, %b3	\n\t"
                       "je	2f\n\t"
                       "stosw	\n\t"
                       "2:\ttestb	$1, %b3	\n\t"
                       "je	3f	\n\t"
                       "stosb	\n\t"
                       "3:	\n\t"
                       : "=&c"(d0), "=&D"(d1)
                       : "a"(tmp), "q"(Count), "0"(Count / 8), "1"(Address)
                       : "memory");
  return Address;
}
#endif