#ifndef __SMP_H__
#define __SMP_H__

// APU_boot 代码的起始、终止地址
extern unsigned char _APU_boot_start[];
extern unsigned char _APU_boot_end[];

/**
 * 多核处理器的初始化
 */
void SMP_init();

/**
 * APU 的启动路径，只需加载操作系统的各类描述符表，不应该重复执行各模块的初始化
 */
void Start_SMP();

#endif