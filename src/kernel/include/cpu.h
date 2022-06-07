#ifndef __CPU_H__
#define __CPU_H__

// 支持的最大 CPU 数
#define NR_CPUS 8

static inline void get_cpuid(unsigned int Mop, unsigned int Sop, unsigned int *a,
                      unsigned int *b, unsigned int *c, unsigned int *d) {
  __asm__ __volatile__("cpuid   \n\t"
                       : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                       : "0"(Mop), "2"(Sop));
}

// 获取处理器的固件信息，以进一步初始化处理器
void init_cpu();

#endif