#include "printk.h"
#include "lib.h"
#include "mm.h"
#include <stdarg.h>

/**
 * 重新映射 VBE 帧缓冲区的线形地址
 * 当前帧缓冲区占用了线性地址 0x3000000 开始的 16 MB，导致这个地址区间不可使用
 * 0xe0000000 在物理地址空间的空洞内，可以用来映射帧缓冲区
 */
void frame_buffer_init() {
  ////re init frame buffer;
  unsigned long i;
  unsigned long *tmp;
  unsigned long *tmp1;
  unsigned int *FB_addr = (unsigned int *)Phy_To_Virt(0xf1000000);

  Global_CR3 = Get_gdt();

  tmp = Phy_To_Virt((unsigned long *)((unsigned long)Global_CR3 & (~0xfffUL)) +
                    (((unsigned long)FB_addr >> PAGE_GDT_SHIFT) & 0x1ff));
  if (*tmp == 0) {
    unsigned long *newPDPT = kmalloc(PAGE_4K_SIZE, 0);
    set_mpl4t(tmp, mk_mpl4t(Virt_To_Phy(newPDPT), PAGE_KERNEL_GDT));
  }

  tmp = Phy_To_Virt((unsigned long *)(*tmp & (~0xfffUL)) +
                    (((unsigned long)FB_addr >> PAGE_1G_SHIFT) & 0x1ff));
  if (*tmp == 0) {
    unsigned long *newPDT = kmalloc(PAGE_4K_SIZE, 0);
    set_pdpt(tmp, mk_pdpt(Virt_To_Phy(newPDT), PAGE_KERNEL_Dir));
  }

  for (i = 0; i < Pos.FB_length; i += PAGE_2M_SIZE) {
    tmp1 = Phy_To_Virt(
        (unsigned long *)(*tmp & (~0xfffUL)) +
        (((unsigned long)((unsigned long)FB_addr + i) >> PAGE_2M_SHIFT) &
         0x1ff));

    unsigned long phy = 0xf1000000 + i;
    set_pdt(tmp1, mk_pdt(phy, PAGE_KERNEL_Page | PAGE_PWT | PAGE_PCD));
  }

  Pos.FB_addr = (unsigned int *)Phy_To_Virt(0xf1000000);

  flush_tlb();
}

/**
 * @brief 写帧缓存区，从而在屏幕的特定位置显示字符
 *
 * @param fb 帧缓存区的起始地址
 * @param Xsize 当前屏幕的分辨率，每一行的像素点数
 * @param x 当前字符第一个像素所在列
 * @param y 当前字符第一个像素所在行
 * @param FRcolor 字符颜色
 * @param BKcolor 字符背景色
 * @param font 待输出字符在 ASCII 码表中的索引
 */
static void putchar(unsigned int *fb, int Xsize, int x, int y, unsigned int FRcolor,
             unsigned int BKcolor, unsigned char font) {
  int i = 0, j = 0;
  unsigned int *addr = NULL; // 当前显示的像素点位置，一个像素点 32bit
  unsigned char *fontp = NULL;
  int testval = 0;

  fontp = font_ascii[font]; // fontp 初始化为待输出字符的位图矩阵的第一个
                            // Byte，即第一行

  // 遍历位图矩阵中的每一个位，在屏幕上位对应的像素点位置输出
  for (i = 0; i < 16; i++) {         // 16 行
    addr = fb + Xsize * (y + i) + x; // 当前行的起始像素点位置
    testval = 0x100;
    for (j = 0; j < 8; j++) { // 每行中的 8 列
      testval =
          testval >> 1; // 10000000b->00000001b，比较 fontp 的每一个二进制位
      if (*fontp & testval)
        *addr = FRcolor; // 位图中为 1 的像素处显示字符颜色
      else
        *addr = BKcolor; // 位图中为 0 的像素处显示字符背景颜色

      addr++; // 屏幕中的下一个像素点
    }
    fontp++; // 位图矩阵的下一个 Byte，即像素矩阵的下一行
  }
}

/**
 * @brief
 * 将数值字母组成的字符串转换为整数值。连续读取字符串，直到读取到不是数值字母为止
 *
 * @param s
 * @return int 返回转换后的整数
 */
static int skip_atoi(const char **s) {
  int i = 0;

  while (is_digit(**s))
    i = 10 * i + *((*s)++) - '0';

  return i;
}

/**
 * @brief 将变量 num 转换为 base 进制数，输出每一位的字符到 str 中
 *
 * @param str
 * @param num 待转换变量
 * @param base 进制
 * @param size
 * @param precision
 * @param type
 * @return char*
 */
static char *number(char *str, long num, int base, int size, int precision,
                    int type) {
  char c, sign, tmp[50];
  const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  int i;

  if (type & SMALL) // 输出时使用小写字母
    digits = "0123456789abcdefghijklmnopqrstuvwxyz";
  if (type & LEFT) // 若需要右对齐，则不能使用 0 填充不足 size 的部分
    type &= ~ZEROPAD;
  if (base < 2 || base > 36) // 只支持输出 2~36 进制数
    return 0;

  c = (type & ZEROPAD) ? '0' : ' '; // 初始化填充用的字符
  sign = 0;
  if (type & SIGN && num < 0) { // 对于有符号的负数，要显示负号
    sign = '-';
    num = -num;
  } else // 否则符号由 PLUS 和 SPACE 决定
    sign = (type & PLUS) ? '+' : ((type & SPACE) ? ' ' : 0);

  if (sign)
    size--; // 负号占用一个宽度
  if (type & SPECIAL)
    if (base == 16) // 16 进制数 0x 占用两个宽度
      size -= 2;
    else if (base == 8) // 16 进制数 前导 0 占用一个宽度
      size--;

  i = 0;
  if (num == 0)
    tmp[i++] = '0';
  else // 将数值转换为字符串，数值的每一位在 tmp 中由低位到高位存储
    while (num != 0)
      tmp[i++] = digits[do_div(num, base)];

  if (i > precision)
    precision = i;
  size -= precision; // 去除数值部分，还需要输出多少字符
  if (!(type & (ZEROPAD + LEFT)))
    while (size-- > 0)
      *str++ = ' ';

  if (sign) // 输出符号
    *str++ = sign;

  if (type & SPECIAL) // 输出进制标志
    if (base == 8)
      *str++ = '0';
    else if (base == 16) {
      *str++ = '0';
      *str++ = digits[33]; // x,X
    }

  if (!(type & LEFT)) // 右对齐，在左边不足宽度处补字符 c
    while (size-- > 0)
      *str++ = c;

  while (i < precision--) // 输出精度要求的前导 0
    *str++ = '0';
  while (i-- > 0) // 倒叙输出字符串，即从高位到低位输出
    *str++ = tmp[i];
  while (size-- > 0) // 左对齐，在右边不足宽度处补空格
    *str++ = ' ';

  return str;
}

int vsprintf(char *buf, const char *fmt, va_list args) {
  char *str, *s;
  int flags;
  // field_width 指整体输出长度，precision 指数值部分的输出长度
  int field_width; // 要输出的字符的最小数目。如果输出的值短于该数，结果会用空格填充。如果输出的值长于该数，结果不会被截断
  int precision; //  要输出的数字的最小位数。如果写入的值短于该数，结果会用前导零来填充。如果写入的值长于该数，结果不会被截断。精度为
                 //  0 意味着不写入任何字符。
  int len, i;

  int qualifier; // "%" 后面格式字符前的格式修饰符，修饰整数格式

  // 遍历 fmt 字符串，处理所有 "%" 指定的格式化输出，输出内容到全局 buf 缓冲
  for (str = buf; *fmt; fmt++) {
    if (*fmt != '%') { // 当前字符不是 "%"，直接输出到缓冲区
      *str++ = *fmt;
      continue;
    }

    // % : flags : width : precision : qualifier : (fmt)
    // 1. 收集格式符的标志位
    flags = 0;
  repeat:
    fmt++;
    switch (*fmt) {
    case '-':
      flags |= LEFT; // 在 width 内左对齐，默认是右对齐
      goto repeat;
    case '+':
      flags |= PLUS; // 强制在结果之前显示加号或减号（+ 或 -）
      goto repeat;
    case ' ':
      flags |= SPACE; // 如果没有写入任何符号，则在该值前面插入一个空格
      goto repeat;
    case '#':
      flags |= SPECIAL; // 与 o、x 或 X 说明符一起使用时，非零值前面会分别显示
                        // 0、0x 或 0X
      goto repeat;
    case '0':
      flags |= ZEROPAD; // 不足 width 的部分使用 0 填充，默认是空格
      goto repeat;
    }

    // 2. 获取输出字符宽度
    field_width = -1;
    if (is_digit(*fmt)) // 宽度在 format 字符串中指定，直接读出数字
      field_width = skip_atoi(&fmt);
    else if (*fmt == '*') { // 宽度在 format 字符串中未指定，则由扩展参数给出
      fmt++;
      field_width = va_arg(args, int);
      if (field_width < 0) { // 扩展参数为负数，将负号看做 flags
        field_width = -field_width;
        flags |= LEFT;
      }
    }

    // 3. 获取输出字符的精度
    precision = -1;
    if (*fmt == '.') {
      fmt++;
      if (is_digit(*fmt)) {
        precision = skip_atoi(&fmt);
      } else if (*fmt == '*') {
        fmt++;
        precision = va_arg(args, int);
      }

      if (precision < 0)
        precision = 0;
    }

    // 4. 处理格式修饰符，支持 h,l,L,Z 修饰整数格式
    qualifier = -1;
    if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' || *fmt == 'Z') {
      qualifier = *fmt;
      fmt++;
    }

    // 5. 根据前面获取的信息和格式符进行输出
    // 格式符支持 c,s,o,p,x,X,d,i,u,n,%
    switch (*fmt) {
    case 'c':              // 输出一个字符
      if (!(flags & LEFT)) // 右对齐，在左边不足宽度处补空格
        while (--field_width > 0)
          *str++ = ' ';
      *str++ = (unsigned char)va_arg(args, int);
      while (--field_width > 0) // 左对齐，在右边不足宽度处补空格
        *str++ = ' ';
      break;

    case 's': // 输出一个字符串
      s = va_arg(args, char *);
      if (!s)
        s = '\0';
      len = strlen(s);
      if (precision < 0)
        precision = len;
      else if (len > precision)
        len = precision; // 精度限制了字符串的输出长度

      if (!(flags & LEFT)) // 右对齐，在左边不足宽度处补空格
        while (len < field_width--)
          *str++ = ' ';
      for (i = 0; i < len; i++) // 输出指定长度的字符串
        *str++ = *s++;
      while (len < field_width--) // 左对齐，在右边不足宽度处补空格
        *str++ = ' ';
      break;

    case 'o': // 按 8 进制输出无符号整数
      if (qualifier == 'l')
        str = number(str, va_arg(args, unsigned long), 8, field_width,
                     precision, flags);
      else
        str = number(str, va_arg(args, unsigned int), 8, field_width, precision,
                     flags);
      break;

    case 'p': // 输出指针地址
      if (field_width == -1) {
        field_width = 2 * sizeof(void *);
        flags |= ZEROPAD;
      }

      str = number(str, (unsigned long)va_arg(args, void *), 16, field_width,
                   precision, flags);
      break;

    case 'x': // 按 16 进制小写输出无符号整数
      flags |= SMALL;
    case 'X': // 按 16 进制大写输出无符号整数
      if (qualifier == 'l')
        str = number(str, va_arg(args, unsigned long), 16, field_width,
                     precision, flags);
      else
        str = number(str, va_arg(args, unsigned int), 16, field_width,
                     precision, flags);
      break;

    case 'd':
    case 'i': // d 和 i 都表示输出有符号 10 进制数
      flags |= SIGN;
    case 'u': // 输出无符号 10 进制数
      if (qualifier == 'l')
        str = number(str, va_arg(args, unsigned long), 10, field_width,
                     precision, flags);
      else
        str = number(str, va_arg(args, unsigned int), 10, field_width,
                     precision, flags);
      break;

    case 'n': // 将当前已格式化的字符串长度赋值给参数
      if (qualifier == 'l') {
        long *ip = va_arg(args, long *);
        *ip = (str - buf);
      } else {
        int *ip = va_arg(args, int *);
        *ip = (str - buf);
      }
      break;

    case '%': // %% 输出一个 %
      *str++ = '%';
      break;

    default: // % 之后跟着不被支持的格式符，直接当做字符串输出
      *str++ = '%';
      if (*fmt)
        *str++ = *fmt;
      else
        fmt--;
      break;
    }
  }
  *str = '\0';
  return str - buf; // 返回格式化后的字符串长度
}

int color_printk(unsigned int FRcolor, unsigned int BKcolor, const char *fmt,
                 ...) {
  int i = 0;
  int count = 0;
  int tabSpace = 0; // 当前光标距离下一个制表位需要填充的空格数量
  va_list args;
  va_start(args, fmt);

  // 1. 先处理源字符串中的 "%"
  i = vsprintf(buf, fmt, args);

  va_end(args);

  // 2. 再处理字符串中的转义字符 \n \b \t
  // 逐个字符检查是否是转义字符，若是则进行特殊处理，否则将字符输出到屏幕上
  for (count = 0; count < i || tabSpace; count++) {

    if (tabSpace > 0) {
      // 还需要输出更多制表符产生的空格
      count--; // 平衡这次循环的 count++
      goto label_tab;
    }

    if ((unsigned char)*(buf + count) == '\n') {
      Pos.YPosition++;   // 下一行
      Pos.XPosition = 0; // 下一行的第一个字符
    } else if ((unsigned char)*(buf + count) == '\b') { // backspace
      Pos.XPosition--; // 光标回退一个字符
      if (Pos.XPosition < 0) {
        // 横轴光标位置为负，则光标跳转到上一行的末尾
        Pos.XPosition = (Pos.XResolution / Pos.XCharSize - 1) * Pos.XCharSize;
        Pos.YPosition--;
        if (Pos.YPosition < 0) // 纵轴光标位置为负，则光标跳转到最后一行的末尾
          Pos.YPosition = (Pos.YResolution / Pos.YCharSize - 1) * Pos.YCharSize;
      }
      // 在上一个字符位置输出空格，则从视觉上删除了上一个字符
      putchar(Pos.FB_addr, Pos.XResolution, Pos.XPosition * Pos.XCharSize,
              Pos.YPosition * Pos.YCharSize, FRcolor, BKcolor, ' ');
    } else if ((unsigned char)*(buf + count) == '\t') {
      // 计算当前光标距离下一个制表位需要填充的空格数量，一个制表位占用 8
      // 个显示字符
      tabSpace = ((Pos.XPosition + 8) & ~(8 - 1)) - Pos.XPosition;

    label_tab: // 输出制表符产生的空格
      tabSpace--;
      putchar(Pos.FB_addr, Pos.XResolution, Pos.XPosition * Pos.XCharSize,
              Pos.YPosition * Pos.YCharSize, FRcolor, BKcolor, ' ');
      Pos.XPosition++;
    } else { // 输出普通字符

      putchar(Pos.FB_addr, Pos.XResolution, Pos.XPosition * Pos.XCharSize,
              Pos.YPosition * Pos.YCharSize, FRcolor, BKcolor,
              (unsigned char)*(buf + count));
      Pos.XPosition++;
    }

    if (Pos.XPosition >= (Pos.XResolution / Pos.XCharSize)) {
      // 光标移动到行尾了，跳到下一行
      Pos.XPosition = 0;
      Pos.YPosition++;
    }
    if (Pos.YPosition >= (Pos.YResolution / Pos.YCharSize)) {
      // 光标移动到列尾了，直接跳转到第一行，简单覆盖
      // TODO 这边可以优化为屏幕滚动
      Pos.YPosition = 0;
    }
  }
  return i;
}