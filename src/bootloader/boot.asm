; 内存地址空间
;|----------------------|
;|  100000 ~ END  |
;|     KERNEL  |
;|----------------------|
;|  E0000 ~ 100000  |
;| Extended System BIOS |
;|----------------------|
;|  C0000 ~ Dffff  |
;|     Expansion Area   |
;|----------------------|
;|  A0000 ~ bffff  |
;|   Legacy Video Area  |
;|----------------------|
;|  9f000 ~ A0000  |
;|   BIOS reserve  |
;|----------------------|
;|  90000 ~ 9f000  |
;|   kernel tmpbuf  |
;|----------------------|
;|  10000 ~ 90000  |
;|     LOADER  |
;|----------------------|
;|  8000 ~ 10000  |
;|    VBE info  |
;|----------------------|
;|  7e00 ~ 8000  |
;|    mem info  |
;|----------------------|
;|  7c00 ~ 7e00  |
;|   MBR (BOOT)  |
;|----------------------|
;|  0000 ~ 7c00  |
;|   BIOS Code  |
;|----------------------|

  
  org 0x7c00

BaseOfStack     equ   0x7c00
BaseOfLoader    equ   0x1000 ; loader 代码所在地址为 0x1000 << 4 + 0x00 = 0x10000
OffsetOfLoader  equ   0x00

  jmp   short Label_Start
  nop
  %include    "fat12.inc"

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

  mov   ax,   1301h   ; 功能号 ah=13h 显示一行字符串
                      ; al=01h 字符串属性由 bl 提供，字符串长度由 cx 提供，光标移动至字符串尾端
  mov   bx,   000fh   ; bh=00h 页码，bl=0fh 黑底白字
  mov   dx,   0000h   ; dh=00h 游标坐标行号，dl=00h 游标坐标列号
  mov   cx,   10      ; cx=10 显示的字符串长度为 10
  mov   bp,   StartBootMessage ; 用 bp 保存字符串的内存地址
  int   10h

;======= search loader.bin

  mov   word  [SectorNo],   SectorNumOfRootDirStart ; 搜索的起始扇区号=根目录起始扇区号

Lable_Search_In_Root_Dir_Begin:           ; 搜索所有根目录占用的所有扇区
  cmp   word  [RootDirSizeForLoop],   0   ; RootDirSizeForLoop=0 表示根目录中的所有扇区都搜索完成
  jz    Label_No_LoaderBin                ; 若在根目录的所有扇区中都没找到 loader，输出错误信息
  dec   word  [RootDirSizeForLoop]        ; RootDirSizeForLoop--
  mov   ax,   00h
  mov   es,   ax                ;
  mov   bx,   8000h             ; 目标缓冲区地址 es:bx=0x00:0x8000
  mov   ax,   [SectorNo]        ; 待读取的扇区号
  mov   cx,   1                 ; 读取的扇区数量
  call  Func_ReadOneSector      ; 读取第 SectorNo 个扇区到内存中
  mov   si,   LoaderFileName
  mov   di,   8000h             ; 存放目标扇区数据的缓冲区地址
  cld                           ; 设置 DF=0，下面字符串比较时地址递增
  mov   dx,   10h               ; dx=每个扇区可容纳的目录项个数 512 / 32 = 16 = 10h

Label_Search_For_LoaderBin:     ; 搜索当前扇区下的所有目录项
  cmp   dx,   0                 ; dx=0 表示当前扇区下的所有目录项都搜索完成
  jz    Label_Goto_Next_Sector_In_Root_Dir ; 若当前扇区没找到，跳转到下一个扇区
  dec   dx                      ; dx--
  mov   cx,   11                ; 目标文件名长度，包括文件名和扩展名，不包含分隔符 “.”

Label_Cmp_FileName:             ; 比较当前目录项
  cmp   cx,   0                 ; cx=0，全部字符都对比完成，即找到了 loader.bin 的目录项
  jz    Label_FileName_Found    ; 跳转到搜索成功分支
  dec   cx                      ; cx--
  lodsb                         ; 从si指定地址读取一字节到al，且si++
  cmp   al,   byte  [es:di]     ; 比较字符串
  jz    Label_Go_On             ; 当前字符相同，继续比较下一个字符
  jmp   Label_Different         ; 当前字符不相同，跳转到下一个目录项

Label_Go_On:
  inc   di                      ; di++，缓冲区地址+1
  jmp   Label_Cmp_FileName

Label_Different:
  and   di,   0ffe0h             ; 将 di 对齐到 0x20
  add   di,   20h                ; di+=32，下一个目录项
  mov   si,   LoaderFileName
  jmp   Label_Search_For_LoaderBin

Label_Goto_Next_Sector_In_Root_Dir:
  add   word  [SectorNo],   1          ; SectorNo++
  jmp   Lable_Search_In_Root_Dir_Begin ; 开始搜索下一个扇区

;======= display on screen : ERROR:No LOADER Found

Label_No_LoaderBin:
  mov   ax,   1301h   ; 功能号 ah=13h 显示一行字符串
                      ; al=01h 字符串属性由 bl 提供，字符串长度由 cx 提供，光标移动至字符串尾端
  mov   bx,   008ch   ; bh=00h 页码，bl=8ch 字符闪烁、黑色背景、高亮、红色字体
  mov   dx,   0100h   ; dh=10h 游标坐标行号，dl=00h 游标坐标列号
  mov   cx,   21      ; cx=21 显示的字符串长度为 21
  push  ax
  mov   ax,   ds
  mov   es,   ax      ; 设置扩展段指针
                      ; es:bp 要显示字符串的内存地址
  pop   ax
  mov   bp,   NoLoaderMessage ; 用 bp 保存字符串的内存地址
  int   10h

  jmp   $             ; 未找到loader程序，原地死循环

;======= found loader.bin name in root director struct

Label_FileName_Found:
  mov   cx,   [BPB_SecPerClus] ; cx=8
  and   di,   0ffe0h          ; di=找到的目录的地址，对齐0x20
  add   di,   01ah            ; DIR_FstClus 字段的偏移
  mov   ax,   word  [es:di]   ; 取出 DIR_FstClus 字段，即第一个簇号，存储空间的最小分配单位为簇
  push  ax
  sub   ax,   2 ; ax-=2，得实际簇号，FAT 相关，排除保留项 FAT[0],FAT[1]
  mul   cl      ; ax*=8，得文件的第一个扇区的逻辑扇区号偏移
  mov   cx,   RootDirSectors ; cx=14
  add   cx,   ax ; 加上根目录占用的扇区数
  ;add   cx,   SectorBalance
  add   cx,   SectorNumOfRootDirStart ; 加上根目录的起始扇区号
                              ; cx 最终得到文件的第一个扇区的逻辑扇区号
  mov   ax,   BaseOfLoader    ; 设置 Func_ReadOneSector 参数
  mov   es,   ax              ;
  mov   bx,   OffsetOfLoader  ; ES:BX 目标缓冲区的起始地址
  mov   ax,   cx              ; 起始扇区号

Label_Go_On_Loading_File:
  push  ax
  push  bx
  mov   ah,   0eh
  mov   al,   '.'
  mov   bx,   0fh
  int   10h         ; 每读出一个簇，输出一个 “.”
  pop   bx
  pop   ax

  mov   cx,   [BPB_SecPerClus]    ; 设置 Func_ReadOneSector 参数，读取一个簇
  call  Func_ReadOneSector
  pop   ax
  call  Func_GetFATEntry
  cmp   ax,   0fffh               ; FAT 表项内容为0xfff表示这是文件最后一个簇
  jz    Label_File_Loaded         ; loader 加载完成
  push  ax

  mov   cx,   [BPB_SecPerClus]
  sub   ax,   2
  mul   cl

  mov   dx,   RootDirSectors
  add   ax,   dx
  ;add   ax,   SectorBalance
  add   ax,   SectorNumOfRootDirStart
  add   bx,   0x1000
  jmp   Label_Go_On_Loading_File  ; 继续加载下一个簇

Label_File_Loaded:
  jmp   BaseOfLoader:OffsetOfLoader   ; loader 程序加载完成，跳转到 loader 执行

;======= read one sector from floppy
; INT 13h，AH=42h 磁盘读取扩展操作 P250
; arg1 AX 逻辑扇区号，从 0 开始
; arg2 CX 读入扇区数量
; arg3 ES:BX 目标缓冲区的起始地址
Func_ReadOneSector:
                      ; 在栈中构建调用参数
  push  dword 00h     ; 64 位的传输缓冲区地址扩展
  push  dword eax     ; 扇区起始号
  push  word  es      ; 传输缓冲区地址（段）
  push  word  bx      ; 传输缓冲区地址（偏移）
  push  word  cx      ; 传输的扇区数
  push  word  10h     ; 参数列表的长度
  mov   ah,   42h     ; 功能号，逻辑块寻址功能
  mov   dl,   00h     ; DL 磁盘驱动器号，00h 代表第一个软盘驱动器
  mov   si,   sp      ; DS:SI，栈中参数的地址
  int   13h
  add   sp,   10h     ; 平衡栈 sp+=16B
  ret

;=======  get FAT Entry 根据FAT表项索引出下一个簇号
; arg1 AX = FAT Entry Number
; ret AX = Next FAT Entry Number
Func_GetFATEntry:
  push  es
  push  bx
  push  ax
  mov   ax,   00
  mov   es,   ax
  pop   ax              ; ax=FAT 表项号
  mov   byte  [Odd],  0
  mov   bx,   3         ;
  mul   bx              ; ax=ax*3
  mov   bx,   2         ;
  div   bx              ; ax=ax/2 ax=商，dx=余数
  cmp   dx,   0
  jz    Label_Even      ; 余数为0则跳转，FAT号为偶数
  mov   byte  [Odd],  1 ; FAT 号为奇数

Label_Even:
  xor   dx,   dx
  mov   bx,   [BPB_BytesPerSec]
  div   bx              ; ax=ax/bx 商ax为FAT表项的偏移扇区号，余数dx为扇区内偏移
  push  dx
  mov   bx,   8000h                 ; arg3 目标缓存区地址
  add   ax,   SectorNumOfFAT1Start  ; arg1 扇区号
  mov   cx,   2                     ; arg2 读入的扇区数量，这里读取2个扇区，可以应对FAT表项跨扇区的情况
  call  Func_ReadOneSector          ; 读取扇区
  
  pop   dx
  add   bx,   dx          ; FAT 表项所在地址
  mov   ax,   [es:bx]     ; 从缓冲区读取16位
  cmp   byte  [Odd],    1
  jnz   Label_Even_2      ; 表项号是偶数则跳转
  shr   ax,   4           ; 表项号是奇数，高12位才是表项的数据，右移4位，舍弃低4位

Label_Even_2:
  and   ax,   0fffh       ; 表项号是偶数，低12位才是表项的数据，取低12位
  pop   bx
  pop   es
  ret
  
;======= tmp variable

RootDirSizeForLoop  dw    RootDirSectors
SectorNo            dw    0
Odd                 db    0

;======= display messages

StartBootMessage:   db    "Start Boot"
NoLoaderMessage:    db    "ERROR:No LOADER Found"
LoaderFileName:     db    "LOADER  BIN",0

; ====== fill zero until whole sector

  times 510 - ($ - $$)  db  0 ; ($ - $$) 表示当前行地址减本节起始地址，此程序只有一节，则可得前面程序所得的机器码长度
                              ; 引导扇区为 512 字节，去掉扇区最后的两个字节大小的关键字，前面的程序需要占 510 字节
                              ; 使用 times 指令重复定义 0，填充剩余的空间
  dw    0xaa55    ; 引导扇区应该以 0x55 0xaa 结尾，因为 Intel 处理器以小端模式存储数据，所以这里要反一下