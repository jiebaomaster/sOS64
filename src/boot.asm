  org 0x7c00

BaseOfStack equ 0x7c00

Label_Start:

  mov   ax,   cs
  mov   ds,   ax
  mov   es,   ax
  mov   ss,   ax
  mov   sp,   BaseOfStack

;======= clear screen

  mov   ax,   0600h   ; 功能号 ah=06，上移显示窗口
                      ; al=00 使用清屏功能，此时bx、cx、dx参数实际不起作用
  mov   bx,   0700h
  mov   cx,   0
  mov   dx,   0184fh
  int   10h

;======= set focus

  mov   ax,   0200h   ; 功能号 ah=02，设置光标位置
  mov   bx,   0000h   ; bh=00h 页码
  mov   dx,   0000h   ; dh=00h 光标所在列数，dl=00h 光标所在行数
  int   10h

;======= display on screen : Start Booting......

  mov   ax,   1301h   ; 功能号 ah=12h 显示一行字符串
                      ; al=01h 字符串属性由 bl 提供，字符串长度由 cx 提供，光标移动至字符串尾端
  mov   bx,   000fh   ; bh=00h 页码，bl=0fh
  mov   dx,   0000h   ; dh=00h 游标坐标行号，dl=00h 游标坐标列号
  mov   cx,   10      ; cx=10 显示的字符串长度为 10
  push  ax
  mov   ax,   ds
  mov   es,   ax      ; 设置扩展段指针
                      ; es:bp 要显示字符串的内存地址
  pop   ax
  mov   bp,   StarBootMessage ; 用 bp 保存字符串的内存地址
  int   10h

;======= reset floppy
  
  xor   ah,   ah    ; 功能号 ah=00
  xor   dl,   dl    ; dl=00h 代表第一个软盘
  int   13h

  jmp   $           ; 原地死循环

StarBootMessage: db "Start Boot"

; ====== fill zero until whole sector

  times 510 - ($ - $$)  db  0 ; ($ - $$) 表示当前行地址减本节起始地址，此程序只有一节，则可得前面程序所得的机器码长度
                              ; 引导扇区为 512 字节，去掉扇区最后的两个字节大小的关键字，前面的程序需要占 510 字节
                              ; 使用 times 指令重复定义 0，填充剩余的空间
  dw    0xaa55    ; 引导扇区应该以 0x55 0xaa 结尾，因为 Intel 处理器以小端模式存储数据，所以这里要反一下