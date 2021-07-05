  org 0x7c00

BaseOfStack     equ   0x7c00
BaseOfLoader    equ   0x1000 ; loader 代码所在地址为 0x1000 << 4 + 0x00 = 0x10000
OffsetOfLoader  equ   0x00

RootDirSectors            equ   14  ; 根目录占用的扇区数
                                    ; (BPB_RootEntCnt * 32 + BPB_BytesPerSec - 1) / BPB_BytesPerSec = 14
SectorNumOfRootDirStart   equ   19  ; 根目录的起始扇区号
                                    ; BPB_RsvdSecCnt + BPB_FATSz16 * BPB_NumFATs = 19
SectorNumOfFAT1Start      equ   1   ; FAT1 表的起始扇区号，FAT1前只有一个保留扇区（引导扇区）
SectorBalance             equ   17  ; 

  jmp   short Label_Start
  nop
  BS_OEMName      db  'MINEboot'      ; 生产商名称
  BPB_BytesPerSec dw  512             ; 每扇区字节数
  BPB_SecPerClus  db  1               ; 每簇扇区数
  BPB_RsvdSecCnt  dw  1               ; 保留扇区数
  BPB_NumFATs     db  2               ; FAT 表的份数
  BPB_RootEntCnt  dw  224             ; 根目录可容纳的目录项数
  BPB_TotSec16    dw  2880            ; 总扇区数
  BPB_Media       db  0xf0            ; 介质描述符
  BPB_FATSz16     dw  9               ; 每 FAT 扇区数
  BPB_SecPerTrk   dw  18              ; 每磁道扇区数
  BPB_NumHeads    dw  2               ; 磁头数
  BPB_HiddSec     dd  0               ; 隐藏扇区数
  BPB_TotSec32    dd  0               ; 如果 BPB_TotSec16=0，则由这个值记录扇区数
  BS_DrvNum       db  0               ; int 13h 的驱动器号
  BS_Reserved1    db  0               ; 未使用
  BS_BootSig      db  29h             ; 扩展引导标记
  BS_VolID        dd  0               ; 卷序列号
  BS_VolLab       db  'boot loader'   ; 卷标
  BS_FileSysType  db  'FAT12   '      ; 文件系统类型

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
  push  ax
  mov   ax,   ds
  mov   es,   ax      ; 设置扩展段指针
                      ; es:bp 要显示字符串的内存地址
  pop   ax
  mov   bp,   StartBootMessage ; 用 bp 保存字符串的内存地址
  int   10h

;======= reset floppy
  
  xor   ah,   ah    ; 功能号 ah=00
  xor   dl,   dl    ; dl=00h 代表第一个软盘
  int   13h

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
  mov   cl,   1                 ; 读取的扇区数量
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
  mov   ax,   RootDirSectors
  and   di,   0ffe0h          ; di=找到的目录的地址，对齐0x20
  add   di,   01ah            ; DIR_FstClus 字段的偏移
  mov   cx,   word  [es:di]   ; 取出 DIR_FstClus 字段，即第一个簇号
  push  cx
  add   cx,   ax
  add   cx,   SectorBalance
  mov   ax,   BaseOfLoader    ; 设置 Func_ReadOneSector 参数
  mov   es,   ax              ;
  mov   bx,   OffsetOfLoader  ; ES:BX 目标缓冲区的起始地址
  mov   ax,   cx              ; 起始扇区号

Label_Go_On_Loading_File:
  push  ax
  push  bx
  mov   ah,   0eh
  mov   al,   '.'
  mov   bl,   0fh
  int   10h         ; 每读出一个簇，输出一个 “.”
  pop   bx
  pop   ax

  mov   cl,   1     ; 设置 Func_ReadOneSector 参数，读取一个簇
  call  Func_ReadOneSector
  pop   ax
  call  Func_GetFATEntry
  cmp   ax,   0fffh               ; FAT 表项内容为0xfff表示这是文件最后一个簇
  jz    Label_File_Loaded         ; loader 加载完成
  push  ax
  mov   dx,   RootDirSectors
  add   ax,   dx
  add   ax,   SectorBalance
  add   bx,   [BPB_BytesPerSec]   ; 地址+512
  jmp   Label_Go_On_Loading_File  ; 继续加载下一个簇

Label_File_Loaded:
  jmp   BaseOfLoader:OffsetOfLoader   ; loader 程序加载完成，跳转到 loader 执行

;======= read one sector from floppy
; arg1 AX 起始扇区号，逻辑扇区号，需要转换成 柱面/磁头/扇区 格式供中断使用
; arg2 CL 读入扇区数量
; arg3 ES:BX 目标缓冲区的起始地址
Func_ReadOneSector:
  push  bp
  mov   bp,   sp
  sub   esp,  2                 ; 栈上开辟 2 字节的空间
  mov   byte  [bp - 2],   cl    ; 需要读取的扇区数量入栈
  push  bx                      ; 目标缓冲区地址入栈
  mov   bl,   [BPB_SecPerTrk]   ; 每磁道扇区数
  div   bl                      ; ax/bl，al=商，ah=余数
  inc   ah                      ; ah++ 起始扇区号
  mov   cl,   ah                ; cl bit0-5 扇区号，bit6-7 磁道号（柱面号）的高 2 位
  mov   dh,   al                ; dh 磁头号
  shr   al,   1                 ; al = al >> 1 柱面号
  mov   ch,   al                ; ch 磁道号（柱面号）的低8位
  and   dh,   1                 ; dh = a & 1 磁头号
  pop   bx
  mov   dl,   [BS_DrvNum]       ; 驱动器号

Label_Go_On_Reading:
  mov   ah,   2                 ; 功能号 ah=02h
  mov   al,   byte  [bp - 2]    ; al 读取的扇区数
  int   13h
  jc    Label_Go_On_Reading     ; 读取成功时 CF=0，继续向下执行，否则跳转回去再次尝试读取
  add   esp,  2                 ; 平衡栈
  pop   bp
  ret

;=======	get FAT Entry 根据FAT表项索引出下一个簇号
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
  push	dx
  mov   bx,   8000h                 ; arg3 目标缓存区地址
  add   ax,   SectorNumOfFAT1Start  ; arg1 扇区号
  mov   cl,   2                     ; arg2 读入的扇区数量，这里读取2个扇区，可以应对FAT表项跨扇区的情况
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