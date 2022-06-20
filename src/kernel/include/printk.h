#ifndef __PRINTK_H__
#define __PRINTK_H__

#include "font.h"
#include "spinlock.h"
#include <stdarg.h>

#define ZEROPAD 1  /* pad with zero 0扩展 */
#define SIGN 2     /* unsigned/signed long */
#define PLUS 4     /* show plus 正负号 */
#define SPACE 8    /* space if plus 在整数前显示一个空格 */
#define LEFT 16    /* left justified 左对齐 */
#define SPECIAL 32 /* 0x 16进制数 */
#define SMALL 64   /* use 'abcdef' instead of 'ABCDEF' 使用小写字母 */

// 判断一个字符是不是数值字母
#define is_digit(c) ((c) >= '0' && (c) <= '9')

#define WHITE 0x00ffffff  //白
#define BLACK 0x00000000  //黑
#define RED 0x00ff0000    //红
#define ORANGE 0x00ff8000 //橙
#define YELLOW 0x00ffff00 //黄
#define GREEN 0x0000ff00  //绿
#define BLUE 0x000000ff   //蓝
#define INDIGO 0x0000ffff //靛
#define PURPLE 0x008000ff //紫

struct position { // 屏幕信息

  // 分辨率
  int XResolution; // 每一行的像素点数
  int YResolution; // 每一列的像素点数

  // 光标位置
  int XPosition; // 当前行的第几个字符
  int YPosition; // 当前列的第几个字符

  // 字符位图矩阵大小
  int XCharSize; // 字符位图矩阵每一行的像素点数
  int YCharSize; // 字符位图矩阵每一列的像素点数

  unsigned int *FB_addr;   // 帧缓存区起始地址，FB = Frame Buffer
  unsigned long FB_length; // 帧缓存区容量，单位 byte

  spinlock_T printk_lock; // 打印操作锁
};

extern struct position Pos;
extern unsigned char font_ascii[256][16];

// n /= base，返回余数部分 __res
#define do_div(n, base)                                                        \
  ({                                                                           \
    int __res;                                                                 \
    __asm__("divq %%rcx" : "=a"(n), "=d"(__res) : "0"(n), "1"(0), "c"(base));  \
    __res;                                                                     \
  })

/**
 * @brief 格式化字符串 fmt，处理其中的 "%"，输出到 buf 中
 *
 * @param buf 输出缓冲区
 * @param fmt 待格式化字符串
 * @param args 扩展参数
 * @return int 返回格式化后的字符串长度
 */
int vsprintf(char *buf, const char *fmt, va_list args);

/**
 * @brief 在屏幕上打印自定义前后景色的格式化字符串
 *
 * @param FRcolor 字体颜色
 * @param BKcolor 背景颜色
 * @param fmt 待格式化字符串，含有 "%" 格式化输出和 "\" 转义字符
 * @param ... 扩展参数
 * @return int
 */
int color_printk(unsigned int FRcolor, unsigned int BKcolor, const char *fmt,
                 ...);

// 黑底白字输出
#define printk(fmt, args...) color_printk(WHITE, BLACK, fmt, ##args)
#define printk_debug(fmt, args...) color_printk(BLUE, BLACK, fmt, ##args)
#define printk_info(fmt, args...) color_printk(ORANGE, BLACK, fmt, ##args)
#define printk_warn(fmt, args...) color_printk(YELLOW, BLACK, fmt, ##args)
#define printk_error(fmt, args...) color_printk(RED, BLACK, fmt, ##args)

/**
 * 重新映射 VBE 帧缓冲区的线形地址
 * 当前帧缓冲区占用了线性地址 0x3000000 开始的 16 MB，导致这个地址区间不可使用
 * 0xe0000000 在物理地址空间的空洞内，可以用来映射帧缓冲区
 */
void frame_buffer_init();

#endif