#ifndef __LIB_H__
#define __LIB_H__

#define NULL 0

// 开中断
#define sti() __asm__ __volatile__("sti	\n\t" ::: "memory")

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
 * @param Address 起始地址
 * @param C 写入的字符
 * @param Count 写入的字符数
 * @return void* 起始地址
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

/**
 * @brief 从 io 端口 port 读取一个字节
 *
 * @param port 目标 io 端口
 * @return unsigned char 读取到的字节
 */
static inline unsigned char io_in8(unsigned short port) {
  unsigned char ret = 0;
  __asm__ __volatile__("inb	%%dx,	%0	\n\t"
                       "mfence			\n\t"
                       : "=a"(ret)
                       : "d"(port)
                       : "memory");
  return ret;
}

/**
 * @brief 从 io 端口 port 读取 4 个字节
 *
 * @param port 目标 io 端口
 * @return unsigned char 读取到的数据
 */
static inline unsigned int io_in32(unsigned short port) {
  unsigned int ret = 0;
  __asm__ __volatile__("inl	%%dx,	%0	\n\t"
                       "mfence			\n\t"
                       : "=a"(ret)
                       : "d"(port)
                       : "memory");
  return ret;
}

/**
 * @brief 向 io 端口 port 写入 1 个字节
 *
 * @param port 目标 io 端口
 * @return void
 */
static inline void io_out8(unsigned short port, unsigned char value) {
  __asm__ __volatile__("outb	%0,	%%dx	\n\t"
                       "mfence			\n\t"
                       :
                       : "a"(value), "d"(port)
                       : "memory");
}

/**
 * @brief 向 io 端口 port 写入 4 个字节
 *
 * @param port 目标 io 端口
 * @return void
 */
static inline void io_out32(unsigned short port, unsigned int value) {
  __asm__ __volatile__("outl	%0,	%%dx	\n\t"
                       "mfence			\n\t"
                       :
                       : "a"(value), "d"(port)
                       : "memory");
}
#endif