#ifndef __LIB_H__
#define __LIB_H__

#define NULL 0

// 开中断
#define sti() __asm__ __volatile__("sti	\n\t" ::: "memory")
// 关中断
#define cli() __asm__ __volatile__("cli	\n\t" ::: "memory")
// 空操作
#define nop() __asm__ __volatile__("nop	\n\t")
// 停机指令
#define hlt() __asm__ __volatile__("hlt	\n\t")
// 读写屏障
#define io_mfence() __asm__ __volatile__("mfence	\n\t" ::: "memory")

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
 * @brief 从 Form 处开始的内存空间拷贝 Num 个字节到 To 开始的内存空间
 * 
 * @param From 源地址
 * @param To 目的地址
 * @param Num 拷贝的字节数
 * @return void* 
 */
static inline void *memcpy(void *From, void *To, long Num) {
  int d0, d1, d2;
  __asm__ __volatile__("cld	\n\t"
                       "rep	\n\t"
                       "movsq	\n\t"
                       "testb	$4,%b4	\n\t"
                       "je	1f	\n\t"
                       "movsl	\n\t"
                       "1:\ttestb	$2,%b4	\n\t"
                       "je	2f	\n\t"
                       "movsw	\n\t"
                       "2:\ttestb	$1,%b4	\n\t"
                       "je	3f	\n\t"
                       "movsb	\n\t"
                       "3:	\n\t"
                       : "=&c"(d0), "=&D"(d1), "=&S"(d2)
                       : "0"(Num / 8), "q"(Num), "1"(To), "2"(From)
                       : "memory");
  return To;
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

/**
 * @brief 读取 MSR 寄存器 address 地址处的值
 * 
 * @param address 
 * @return unsigned long 
 */
static inline unsigned long rdmsr(unsigned long address) {
  unsigned int tmp0 = 0;
  unsigned int tmp1 = 0;
  __asm__ __volatile__("rdmsr	\n\t"
                       : "=d"(tmp0), "=a"(tmp1)
                       : "c"(address)
                       : "memory");
  return (unsigned long)tmp0 << 32 | tmp1;
}

/**
 * @brief 写 MSR 寄存器
 * 
 * @param address 
 * @param value 待写入的值
 */
static inline void wrmsr(unsigned long address, unsigned long value) {
  __asm__ __volatile__("wrmsr	\n\t" ::"d"(value >> 32),
                       "a"(value & 0xffffffff), "c"(address)
                       : "memory");
}

/**
 * @brief 返回 rflags 寄存器
 */
static inline unsigned long get_rflags() {
  unsigned long tmp = 0;
  __asm__ __volatile__("pushfq	\n\t"
                       "movq	(%%rsp), %0	\n\t"
                       "popfq	\n\t"
                       : "=r"(tmp)::"memory");
  return tmp;
}

#endif