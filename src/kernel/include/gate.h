#ifndef __GATE_H__
#define __GATE_H__

/**
 * @brief gdt 表中保存的描述符
 */
struct desc_struct {
  unsigned char x[8];
};

/**
 * @brief idt 表中保存的门描述符
 * 每个门描述符占 16 个字节
 */
struct gate_struct {
  unsigned char x[16];
};

extern struct desc_struct GDT_TABLE[]; // head.S 中定义的 gdt 表
extern struct gate_struct IDT_TABLE[]; // head.S 中定义的 idt 表

// 初始化 idt 中的各种门
#define _set_gate(gate_selector_addr, addr, ist, code_addr)                    \
  do {                                                                         \
    unsigned long __d0, __d1;                                                  \
    __asm__ __volatile__("movw	%%dx,	%%ax	\n\t"                                \
                         "andq	$0x7,	%%rcx	\n\t"                               \
                         "addq	%4,	%%rcx	\n\t"                                 \
                         "shlq	$32,	%%rcx	\n\t"                                \
                         "addq	%%rcx,	%%rax	\n\t"                              \
                         "xorq	%%rcx,	%%rcx	\n\t"                              \
                         "movl	%%edx,	%%ecx	\n\t"                              \
                         "shrq	$16,	%%rcx	\n\t"                                \
                         "shlq	$48,	%%rcx	\n\t"                                \
                         "addq	%%rcx,	%%rax	\n\t"                              \
                         "movq	%%rax,	%0	\n\t"                                 \
                         "shrq	$32,	%%rdx	\n\t"                                \
                         "movq	%%rdx,	%1	\n\t"                                 \
                         : "=m"(*((unsigned long *)(gate_selector_addr))),     \
                           "=m"(*(1 + (unsigned long *)(gate_selector_addr))), \
                           "=&a"(__d0), "=&d"(__d1)                            \
                         : "i"(addr << 8), "3"((unsigned long *)(code_addr)),  \
                           "2"(0x8 << 16), "c"(ist)                            \
                         : "memory");                                          \
  } while (0)

/**
 * @brief 初始化一个中断门
 *
 * @param n    该中断门在 idt 表中的索引，即中断号
 * @param ist  处理函数所使用的栈在 tss 中的索引
 * @param addr 中断处理函数的起始地址
 */
static inline void set_intr_gate(unsigned int n, unsigned char ist,
                                 void *addr) {
  _set_gate(IDT_TABLE + n, 0x8E, ist, addr); // P,DPL=0,TYPE=E
}

static inline void set_trap_gate(unsigned int n, unsigned char ist,
                                 void *addr) {
  _set_gate(IDT_TABLE + n, 0x8F, ist, addr); // P,DPL=0,TYPE=F
}

static inline void set_system_gate(unsigned int n, unsigned char ist,
                                   void *addr) {
  _set_gate(IDT_TABLE + n, 0xEF, ist, addr); // P,DPL=3,TYPE=F
}

/**
 * @brief 在 GDT 中偏移为 n 的地方设置 tss 描述符
 * 
 * @param n GDT 中的偏移
 * @param addr tss 的地址
 */
inline void set_tss_descriptor(unsigned int n, void *addr) {
  unsigned long limit = 103;
  *(unsigned long *)(GDT_TABLE + n) =
      (limit & 0xffff) | (((unsigned long)addr & 0xffff) << 16) |
      (((unsigned long)addr >> 16 & 0xff) << 32) | ((unsigned long)0x89 << 40) |
      ((limit >> 16 & 0xf) << 48) |
      (((unsigned long)addr >> 24 & 0xff) << 56); /////89 is attribute
  *(unsigned long *)(GDT_TABLE + n + 1) =
      ((unsigned long)addr >> 32 & 0xffffffff) | 0;
}

// 加载 TSS 描述符选择子到 tr 寄存器中
// n 为 tss 描述符在 GDT 中的偏移
#define load_TR(n)                                                             \
  do {                                                                         \
    __asm__ __volatile__("ltr	%%ax" : : "a"(n << 3) : "memory");             \
  } while (0)

/**
 * @brief 设置 TSS 中的各个栈指针，
 * 64 位模式下的 tss 不再保存和还原程序的执行现场环境，它只负责不同特权级间的栈切换工作
 * 
 * @param tss TSS 的地址 
 * @param rsp0~2 特权级 0，1，2 的栈
 * @param ist1~7 给不同的中断处理函数准备的栈
 */
void set_tss64(unsigned int *tss, unsigned long rsp0, unsigned long rsp1,
               unsigned long rsp2, unsigned long ist1, unsigned long ist2,
               unsigned long ist3, unsigned long ist4, unsigned long ist5,
               unsigned long ist6, unsigned long ist7) {
  // 特权级 0，1，2 的栈指针
  *(unsigned long *)(tss + 1) = rsp0;
  *(unsigned long *)(tss + 3) = rsp1;
  *(unsigned long *)(tss + 5) = rsp2;

  // 给不同的中断处理函数准备的栈指针
  *(unsigned long *)(tss + 9) = ist1;
  *(unsigned long *)(tss + 11) = ist2;
  *(unsigned long *)(tss + 13) = ist3;
  *(unsigned long *)(tss + 15) = ist4;
  *(unsigned long *)(tss + 17) = ist5;
  *(unsigned long *)(tss + 19) = ist6;
  *(unsigned long *)(tss + 21) = ist7;
}

#endif