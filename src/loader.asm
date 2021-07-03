  org 0x10000

  mov   ax,   cs
  mov   ds,   ax
  mov   es,   ax
  mov   ax,   0x00
  mov   ss,   ax
  mov   sp,   0x7c00
;======= display on screen : Start Loader......

  mov   ax,   1301h   ; 功能号 ah=13h 显示一行字符串
                      ; al=01h 字符串属性由 bl 提供，字符串长度由 cx 提供，光标移动至字符串尾端
  mov   bx,   000fh   ; bh=00h 页码，bl=0fh 白底黑字
  mov   dx,   0200h   ; dh=02h 游标坐标行号，dl=00h 游标坐标列号
  mov   cx,   12      ; cx=12 显示的字符串长度为 12
  push  ax
  mov   ax,   ds
  mov   es,   ax      ; 设置扩展段指针
                      ; es:bp 要显示字符串的内存地址
  pop   ax
  mov   bp,   StartLoaderMessage ; 用 bp 保存字符串的内存地址
  int   10h

  jmp   $

;======= display messages

StartLoaderMessage:   db    "Start Loader"    
