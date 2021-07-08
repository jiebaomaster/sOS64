org 0x10000
  jmp Label_Start  

%include    "fat12.inc"

BaseOfKernelFile        equ   0x00      ;
OffsetOfKernelFile      equ   0x100000  ; 内核文件存储地址的偏移

BaseOfTmpKernelAddr     equ   0x00      ;
OffsetOfTmpKernelAddr   equ   0x7E00    ; 内核文件临时存储地址的偏移

MemoryStructBufferAddr  equ   0x7E00    ; 读取的内存物理地址信息存储地址

[SECTION gdt]
; 初始化进入保护模式时需要的临时全局描述符表
LABEL_GDT:            dd    0,0                     ; 第一项必须是 NULL 描述符
LABEL_DESC_CODE32:    dd    0x0000FFFF,0x00CF9A00   ; 代码段描述符
LABEL_DESC_DATA32:    dd    0x0000FFFF,0x00CF9200   ; 数据段描述符
                                                    ; base:limit=0x00000000:0xffffffff，即索引 0~4GB

GdtLen    equ   $ - LABEL_GDT   ; gdt 长度
GdtPtr    dw    GdtLen - 1      ; gdtr 的低 2 字节，保存 gdt 长度 - 1
          dd    LABEL_GDT       ; gdtr 的高 4 自己，保存 gdt 基地址

SelectorCode32    equ   LABEL_DESC_CODE32 - LABEL_GDT   ; 32位代码段描述符在gdt中的偏移
SelectorData32    equ   LABEL_DESC_DATA32 - LABEL_GDT   ; 32位数据段描述符在gdt中的偏移

[SECTION gdt64]
; 初始化进入长模式时需要的临时全局描述符表
LABEL_GDT64:            dq    0x0000000000000000      ; 第一项必须是 NULL 描述符
LABEL_DESC_CODE64:      dq    0x0020980000000000      ; 代码段描述符
LABEL_DESC_DATA64:      dq    0x0000920000000000      ; 数据段描述符

GdtLen64    equ   $ - LABEL_GDT64   ; gdt 长度
GdtPtr64    dw    GdtLen64 - 1      ; gdtr 的低 2 字节，保存 gdt 长度 - 1
            dd    LABEL_GDT64       ; gdtr 的高 4 自己，保存 gdt 基地址

SelectorCode64    equ   LABEL_DESC_CODE64 - LABEL_GDT64   ; 32位代码段描述符在gdt中的偏移
SelectorData64    equ   LABEL_DESC_DATA64 - LABEL_GDT64   ; 32位数据段描述符在gdt中的偏移

[SECTION .s16]  ; 定义一个节
[BITS 16]       ; 通知nasm编译器，以下代码运行在16位宽的处理器下

Label_Start:
  mov   ax,   cs
  mov   ds,   ax
  mov   es,   ax
  mov   ax,   0x00
  mov   ss,   ax
  mov   sp,   0x7c00

;======= display on screen : Start Loader......

  mov   ax,   1301h   ; 功能号 ah=13h 显示一行字符串
                      ; al=01h 字符串属性由 bl 提供，字符串长度由 cx 提供，光标移动至字符串尾端
  mov   bx,   000fh   ; bh=00h 页码，bl=0fh 黑底白字
  mov   dx,   0200h   ; dh=02h 游标坐标行号，dl=00h 游标坐标列号
  mov   cx,   12      ; cx=12 显示的字符串长度为 12
  push  ax
  mov   ax,   ds
  mov   es,   ax      ; 设置扩展段指针
                      ; es:bp 要显示字符串的内存地址
  pop   ax
  mov   bp,   StartLoaderMessage ; 用 bp 保存字符串的内存地址
  int   10h

;======= open address A20

  push  ax
  in    al,   92h         ; 读取 IO端口 92h
  or    al,   00000010b   ; 置位 92h 的第一位
  out   92h,  al
  pop   ax

  cli                     ; 关外部中断

  db    0x66              ; 在16位模式使用32位数据指令，需要加 0x66 前缀
  lgdt  [GdtPtr]          ; 开始保护模式前需要先设置临时全局描述表
  mov   eax,  cr0         ; 读 cr0
  or    eax,  1           ; 置位 cr0 的第 0 位，开启保护模式
  mov   cr0,  eax
  mov   ax,   SelectorData32
  mov   fs,   ax          ; 设置 fs 段寄存器，使其在实模式下能达到 4GB 的寻址能力
                          ; fs.base=0x00000000，fs.limit=0xffffffff
  mov   eax,  cr0         ; 读 cr0
  and   al,   11111110b   ; 清除 cr0 的第 0 位，关闭保护模式
  mov   cr0,  eax

  sti

;======= reset floppy

  xor   ah,   ah    ; 功能号 ah=00
  xor   dl,   dl    ; dl=00h 代表第一个软盘
  int   13h

;======= search kernel.bin

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

Label_Search_For_KernelBin:     ; 搜索当前扇区下的所有目录项
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
  jmp   Label_Search_For_KernelBin

Label_Goto_Next_Sector_In_Root_Dir:
  add   word  [SectorNo],   1          ; SectorNo++
  jmp   Lable_Search_In_Root_Dir_Begin ; 开始搜索下一个扇区

;======= display on screen : ERROR:No KERNEL Found

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

  jmp   $             ; 未找到 kernel 程序，原地死循环

;======= found kernel.bin name in root director struct

Label_FileName_Found:
  mov   ax,   RootDirSectors
  and   di,   0ffe0h          ; di=找到的目录的地址，对齐0x20
  add   di,   01ah            ; DIR_FstClus 字段的偏移
  mov   cx,   word  [es:di]   ; 取出 DIR_FstClus 字段，即第一个簇号
  push  cx
  add   cx,   ax
  add   cx,   SectorBalance
  mov   ax,   BaseOfTmpKernelAddr   ; 设置 Func_ReadOneSector 参数
  mov   es,   ax                    ;
  mov   bx,   OffsetOfTmpKernelAddr ; ES:BX 目标缓冲区的起始地址
  mov   ax,   cx                    ; 起始扇区号

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

;;;;;;;; 将读取到的临时空间的一簇 kernel 数据按字节转存到目标地址

  push  cx
  push  eax
  push  fs
  push  edi
  push  ds
  push  esi
  ; 设置下面循环拷贝数据用到的寄存器
  mov   cx,   200h ; 循环重复的次数 200h=512，即拷贝一个扇区
  mov   ax,   BaseOfKernelFile
  mov   fs,   ax
  mov   edi,  dword	[OffsetOfKernelFileCount]
  mov   ax,   BaseOfTmpKernelAddr
  mov   ds,   ax
  mov   esi,  OffsetOfTmpKernelAddr

Label_Mov_Kernel:	;------------------
  mov   al,   byte  [ds:esi]  ; 从缓冲区中读取一字节数据
  mov   byte  [fs:edi],   al  ; 转存数据到目标地址，此处借助上面 A20 部分设置的 fs 4GB 寻址
  inc   esi                   ; esi++ 缓冲区指针前进一个字节
  inc   edi                   ; edi++ 目标地址指针前进一个字节
  loop  Label_Mov_Kernel      ; cx--，cx!=0 时跳转到 Label_Mov_Kernel

  mov   eax,  0x1000
  mov   ds,   eax

  mov   dword [OffsetOfKernelFileCount],  edi ; 保存当前拷贝到缓冲区的哪个扇区的地址

  pop   esi
  pop   ds
  pop   edi
  pop   fs
  pop   eax
  pop   cx

;;;;;;;;

  call  Func_GetFATEntry
  cmp   ax,   0fffh               ; FAT 表项内容为0xfff表示这是文件最后一个簇
  jz    Label_File_Loaded         ; kernel 加载完成
  push  ax
  mov   dx,   RootDirSectors
  add   ax,   dx
  add   ax,   SectorBalance
  add   bx,   [BPB_BytesPerSec]   ; 地址+512
  jmp   Label_Go_On_Loading_File  ; 继续加载下一个簇

;======= 读取成功，通过写内存的方式在屏幕上输出一个字符 

Label_File_Loaded:
  mov   ax,   0B800h  ; 0B800h 开始是一段专门用于显示字符的内存空间
  mov   gs,   ax
  mov   ah,   0Fh     ; ah=字符属性，0000:1111 = 黑底:白字
  mov   al,   'G'     ; al=输出的字符
  mov   [gs:((80 * 0 + 39) * 2)], ax  ; 屏幕第 0 行, 第 39 列，输出字符

;======= 读取操作都完成了，关闭软盘驱动器

KillMotor:
  push  dx
  mov   dx,   03F2h
  mov   al,   0
  out   dx,   al  ; 向IO端口 03F2h 写 0，代表关闭全部软盘驱动器
  pop   dx

;======= get memory address size type

  mov   ax,   1301h   ; 功能号 ah=13h 显示一行字符串
                      ; al=01h 字符串属性由 bl 提供，字符串长度由 cx 提供，光标移动至字符串尾端
  mov   bx,   000Fh   ; bh=00h 页码，bl=0fh 黑底白字
  mov   dx,   0400h   ; dh=04h 游标坐标行号4，dl=00h 游标坐标列号
  mov   cx,   24      ; cx=24 显示的字符串长度为 24
  push  ax
  mov   ax,   ds
  mov   es,   ax      ; 设置扩展段指针
                      ; es:bp 要显示字符串的内存地址
  pop   ax
  mov   bp,   StartGetMemStructMessage  ; 用 bp 保存待输出字符串的内存地址
  int   10h           ; 输出开始读取内存信息的提示消息

  mov   ebx,  0       ; ebx=0，ebx
  mov   ax,   0x00
  mov   es,   ax
  mov   di,   MemoryStructBufferAddr	; es:di 读取缓冲区地址

Label_Get_Mem_Struct:
  mov   eax,  0x0E820         ; 功能号 ax=E820h 从硬件查询系统地址映射信息
  mov   ecx,  20              ; struct 的大小
  mov   edx,  0x534D4150      ; 这是一个魔法数字
  int   15h
  jc    Label_Get_Mem_Fail    ; 读取成功时 CF=0，继续向下执行，否则跳转到错误分支
  add   di,   20              ; 读取下一个 struct

  cmp   ebx,  0
  jne   Label_Get_Mem_Struct  ; ebx!=0，表示还未遍历完，继续遍历，否则继续向下执行
  jmp   Label_Get_Mem_OK      ; 已遍历完，跳转到成功分支

Label_Get_Mem_Fail:
  mov   ax,   1301h   ; 功能号 ah=13h 显示一行字符串
                      ; al=01h 字符串属性由 bl 提供，字符串长度由 cx 提供，光标移动至字符串尾端
  mov   bx,   008Ch   ; bh=00h 页码，bl=8ch 字符闪烁、黑色背景、高亮、红色字体
  mov   dx,   0500h   ; dh=05h 游标坐标行号5，dl=00h 游标坐标列号
  mov   cx,   23      ; cx=23 显示的字符串长度为 23
  push  ax
  mov   ax,   ds
  mov   es,   ax      ; 设置扩展段指针
                      ; es:bp 要显示字符串的内存地址
  pop   ax
  mov   bp,   GetMemStructErrMessage  ; 用 bp 保存待输出字符串的内存地址
  int   10h           ; 输出读取内存信息的失败提示消息
  jmp   $             ; 失败后原地等待

Label_Get_Mem_OK:
  
  mov   ax,   1301h   ; 功能号 ah=13h 显示一行字符串
                      ; al=01h 字符串属性由 bl 提供，字符串长度由 cx 提供，光标移动至字符串尾端
  mov   bx,   000Fh   ; bh=00h 页码，bl=0fh 黑底白字
  mov   dx,   0600h   ; dh=06h 游标坐标行号6，dl=00h 游标坐标列号
  mov   cx,   29      ; cx=29 显示的字符串长度为 29
  push  ax
  mov   ax,   ds
  mov   es,   ax      ; 设置扩展段指针
                      ; es:bp 要显示字符串的内存地址
  pop   ax
  mov   bp,   GetMemStructOKMessage  ; 用 bp 保存待输出字符串的内存地址
  int   10h           ; 输出读取内存信息的成功提示消息
  
;=======	get SVGA information

  mov   ax,   1301h   ; 功能号 ah=13h 显示一行字符串
  mov   bx,   000Fh   ; bh=00h 页码，bl=0fh 黑底白字
  mov   dx,   0800h   ; dh=08h 游标坐标行号8，dl=00h 游标坐标列号
  mov   cx,   23      ; cx=23 显示的字符串长度为 23
  push  ax
  mov   ax,   ds
  mov   es,   ax      ; es:bp 要显示字符串的内存地址
  pop   ax
  mov   bp,   StartGetSVGAVBEInfoMessage  ; 用 bp 保存待输出字符串的内存地址
  int   10h           ; 输出开始读取 SVGA 提示消息

  mov   ax,   0x00
  mov   es,   ax
  mov   di,   0x8000  ; es:di=0x00:0x8000，读取到的信息保存的地址
  mov   ax,   4F00h   ; 功能号 ah=4Fh
  int   10h           ; 读取 SVGA 信息

  cmp   ax,   004Fh
  jz    .KO           ; ax=004Fh 则读取成功，跳转到成分支，否则继续执行输出错误信息
  
;=======	Fail

  mov   ax,   1301h   ; 功能号 ah=13h 显示一行字符串
  mov   bx,   008Ch   ; bh=00h 页码，bl=8ch 字符闪烁、黑色背景、高亮、红色字体
  mov   dx,   0900h   ; dh=09h 游标坐标行号9，dl=00h 游标坐标列号
  mov   cx,   23      ; cx=23 显示的字符串长度为 23
  push  ax
  mov   ax,   ds
  mov   es,   ax      ; es:bp 要显示字符串的内存地址
  pop   ax
  mov   bp,   GetSVGAVBEInfoErrMessage  ; 用 bp 保存待输出字符串的内存地址
  int   10h           ; 输出读取 SVGA 失败的提示消息
  jmp   $             ; 失败后原地等待

.KO:
  mov   ax,   1301h   ; 功能号 ah=13h 显示一行字符串
  mov   bx,   000Fh   ; bh=00h 页码，bl=0fh 黑底白字
  mov   dx,   0A00h   ; dh=0Ah 游标坐标行号9，dl=00h 游标坐标列号
  mov   cx,   29      ; cx=29 显示的字符串长度为 29
  push  ax
  mov   ax,   ds
  mov   es,   ax      ; es:bp 要显示字符串的内存地址
  pop   ax
  mov   bp,   GetSVGAVBEInfoOKMessage  ; 用 bp 保存待输出字符串的内存地址
  int   10h           ; 输出读取 SVGA 成功的提示消息

;=======	Get SVGA Mode Info

  mov   ax,   1301h   ; 功能号 ah=13h 显示一行字符串
  mov   bx,   000Fh   ; bh=00h 页码，bl=0fh 黑底白字
  mov   dx,   0C00h   ; dh=0Ch 游标坐标行号12，dl=00h 游标坐标列号
  mov   cx,   24      ; cx=24 显示的字符串长度为 24
  push  ax
  mov   ax,   ds
  mov   es,   ax      ; es:bp 要显示字符串的内存地址
  pop   ax
  mov   bp,   StartGetSVGAModeInfoMessage  ; 用 bp 保存待输出字符串的内存地址
  int   10h           ; 输出开始读取 SVGA 显示模式的提示消息

  mov   ax,   0x00
  mov   es,   ax
  mov   si,   0x800e

  mov   esi,  dword [es:si]
  mov   edi,  0x8200

Label_SVGA_Mode_Info_Get:
  mov   cx,   word  [es:esi]

;=======	display SVGA mode information

  push  ax
  
  mov   ax,   00h
  mov   al,   ch
  call  Label_DispAL

  mov   ax,   00h
  mov   al,   cl	
  call  Label_DispAL
  
  pop   ax

;=======
  
  cmp   cx,   0FFFFh
  jz    Label_SVGA_Mode_Info_Finish

  mov   ax,   4F01h
  int   10h

  cmp   ax,   004Fh
  jnz   Label_SVGA_Mode_Info_FAIL	
  add   esi,  2
  add   edi,  0x100

  jmp   Label_SVGA_Mode_Info_Get
    
Label_SVGA_Mode_Info_FAIL:
  mov   ax,   1301h
  mov   bx,   008Ch
  mov   dx,   0D00h		;row 13
  mov   cx,   24
  push  ax
  mov   ax,   ds
  mov   es,   ax
  pop   ax
  mov   bp,   GetSVGAModeInfoErrMessage
  int   10h

Label_SET_SVGA_Mode_VESA_VBE_FAIL:
  jmp   $

Label_SVGA_Mode_Info_Finish:
  mov   ax,   1301h
  mov   bx,   000Fh
  mov   dx,   0E00h		;row 14
  mov   cx,   30
  push  ax
  mov   ax,   ds
  mov   es,   ax
  pop   ax
  mov   bp,   GetSVGAModeInfoOKMessage
  int   10h

;======= set the SVGA mode(VESA VBE)
; TODO 这里改变屏幕的分辨率后会使前面的输出不可见，后面用到再取消注释
  ; mov   ax,   4F02h
  ; mov   bx,   4180h    ; 显示模式 4180h，1440 列 900 行，物理地址 e0000000h，每像素 32bit
  ; int   10h

  ; cmp   ax,   004Fh
  ; jnc   Label_SET_SVGA_Mode_VESA_VBE_FAIL ; 若 ax!=004fh 表示设置显示模式失败，跳转到失败分支

;======= init IDT,GDT and goto protect mode
  
  cli                   ; 关外部中断
  db    0x66
  lgdt  [GdtPtr]        ; 重新加载 gdt 指针
  db    0x66
  lidt  [IDT_POINTER]   ; （可选）重新加载 idt 指针
  mov   eax,  cr0
  or    eax,  1
  mov   cr0,  eax       ; 设置 cr0 的第 1 位，打开保护模式
  jmp   dword SelectorCode32:GO_TO_TMP_Protect  ; 通过一个远跳转指令将保护模式的代码端选择子SelectorCode32
                                                ; 加载到 cs，便可运行保护模式的代码段
  
[SECTION .s32]
[BITS 32]

GO_TO_TMP_Protect:

;======= goto tmp long mode

  mov   ax,   0x10        ; 初始化各个段寄存器
  mov   ds,   ax
  mov   es,   ax
  mov   fs,   ax
  mov   ss,   ax
  mov   esp,  7E00h

  call  support_long_mode ; 判断是否支持长模式
  test  eax,  eax         ; eax & eax = 0 即 eax = 0 时，设置 eflags.zf=1
  jz    no_support        ; eflags.zf=1 表示系统不支持长模式，跳转到失败分支

;======= init temporary page table 0x90000
  ; 长模式下的临时页表，支持 256TB（2^48B）的线性地址空间
  ; 这里 PDT[7]=1，故采用的是 2MB 大小的物理页，所以总共有三级页表
  ; 每一级表项的大小为 8B，因为 Intel CPU 采用小端序，所以要先存放表项的低 4B，再存放高 4B
  ; 将虚拟地址 0x0000000000000000和 0x0000040000000000（128TB）
  ; 开始的 6*2MB=12MB 的地址空间映射到物理地址 0x0000000000000000 开始的 12MB
  ; PML4T 39-47
  mov   dword [0x90000],    0x91007 ; 第 0 项
  mov   dword [0x90004],    0x00000
  mov   dword [0x90800],    0x91007 ; 第 256 项
  mov   dword [0x90804],    0x00000
  ; PDPT 30-38
  mov   dword [0x91000],    0x92007
  mov   dword [0x91004],    0x00000
  ; PDT 21-29
  mov   dword [0x92000],    0x000083
  mov   dword [0x92004],    0x000000
  mov   dword [0x92008],    0x200083
  mov   dword [0x9200c],    0x000000
  mov   dword [0x92010],    0x400083
  mov   dword [0x92014],    0x000000
  mov   dword [0x92018],    0x600083
  mov   dword [0x9201c],    0x000000
  mov   dword [0x92020],    0x800083
  mov   dword [0x92024],    0x000000
  mov   dword [0x92028],    0xa00083
  mov   dword [0x9202c],    0x000000

;======= load GDTR

  db    0x66
  lgdt  [GdtPtr64]
  mov   ax,   0x10
  mov   ds,   ax
  mov   es,   ax
  mov   fs,   ax
  mov   gs,   ax
  mov   ss,   ax
  mov   esp,  7E00h

;======= open PAE

  mov   eax,  cr4
  bts   eax,  5     ; cr4.pae=1，开启物理地址扩展功能，进入长模式时必须
  mov   cr4,  eax
  
;======= load cr3

  mov   eax,  0x90000
  mov   cr3,  eax   ; 设置长模式的页表地址

;======= enable long-mode

  mov   ecx,  0C0000080h  ; 读 msr 寄存器组的寄存器地址，0C0000080h 表示 IA32_EFER 寄存器
  rdmsr             ; 读 msr 寄存器组到 eax
  bts   eax,  8     ; IA32_EFER.LME=1，激活 IA-32e 模式
  wrmsr             ; 写 msr

;======= open PE and paging

  mov   eax,  cr0
  bts   eax,  0     ; cr0.PE=1，保险起见，再次开启保护模式
  bts   eax,  31    ; cr0.PG=1，开启分页
  mov   cr0,  eax

  jmp SelectorCode64:OffsetOfKernelFile ; 长跳转到内核代码，正式进入64位IA-32e模式

;======= test support long mode or not
; 判断是否支持 IA-32e 模式
; 返回值在 eax 中，eax=0 则不支持，否则支持 
support_long_mode:
  mov   eax,  0x80000000  ; cpuid 指令的输入参数，0x80000000h 表示查询处理器支持的最大扩展功能号
  cpuid                   ; 监测处理器支持的功能，eax=0x80000000h 时返回值存储在 eax
  cmp   eax,  0x80000001  
  setnb al                ; eax < 0x80000001 时 cf=1，设置 al=0
  jb    support_long_mode_done  ; 若 cpuid 返回小于 0x80000001h，则肯定不支持长模式，跳转到返回
  mov   eax,  0x80000001  ; cpuid 指令的输入参数，eax=0x80000001h 时返回值保存在 edx，
                          ; 且其第 29 位表示了 cpu 是否支持长模式
  cpuid
  bt    edx,  29          ; 将 edx 的第 29 位传到 eflags.CF
  setc  al                ; 若 eflags.CF=1，设置 al=1

support_long_mode_done:
  movzx eax,  al          ; 8 位的 al 前面补 0 再传给 eax，eax 作为返回值
  ret

no_support:
  jmp   $      

[SECTION .s16lib]
[BITS 16]

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

;======= display num in al 打印一个16进制数
; arg1 AL = 要显示的16进制数

Label_DispAL:
  push  ecx
  push  edx
  push  edi
  
  mov   edi,  [DisplayPosition]
  mov   ah,   0Fh     ; 设置字体颜色属性，黑底白字
  mov   dl,   al      ; al => dl 保存 al 的低 4 位
  shr   al,   4       ; al >> 4 右移 4 位
  mov   ecx,  2       ; ecx=0 用于循环计数，AL有8位，可以显示两个16进制字符

.begin:
  and   al,   0Fh     ; 前面右移了，所以这里是取 al 的高 4 位，即第一个16进制字符
                      ; 先显示高 4 位代表的字符，再显示低 4 位
  cmp   al,   9       ; 与 9 比较
  ja    .1            ; al > 9，需要显示 'A'~'F'，跳转到 .1 做特殊处理
  add   al,   '0'     ; al=0~9，显示的字符为 '0' + al
  jmp   .2

.1:
  sub   al,   0Ah     ; al-=0Ah，计算与 'A' 的偏移
  add   al,   'A'     ; al='A'~'F'，显示的字符为 'A' + al

.2:
  mov   [gs:edi], ax  ; gs:edi=B800h:DisplayPosition，即向显存空间写一个字符
  add   edi,  2       ; edi+=2 执行显存中下一个字符的地址
  mov   al,   dl      ; dl=>al 显示低 4 位代表的第二个字符
  loop  .begin        ; cx--，cx!=0 时跳转到 .begin，

  mov   [DisplayPosition],  edi ; 保存当前写入的显存内存地址，下次调用从这里继续显示

  pop   edi
  pop   edx
  pop   ecx
  ret

;======= tmp IDT

IDT:
  times 0x50  dq  0 ; 50 个 64 位的中断描述符
IDT_END:

IDT_POINTER:
  dw  IDT_END - IDT - 1 ; idtr 低 2 字节，idt 长度 - 1
  dd  IDT               ; idtr 高 4 字节，idt 地址

;======= tmp variable

RootDirSizeForLoop  dw    RootDirSectors
SectorNo            dw    0
Odd                 db    0
OffsetOfKernelFileCount dd  OffsetOfKernelFile
DisplayPosition     dd    0

;=======	display messages

StartLoaderMessage:   db    "Start Loader"    
NoLoaderMessage:      db    "ERROR:No KERNEL Found"
LoaderFileName:       db    "KERNEL  BIN",0

StartGetMemStructMessage:   db    "Start Get Memory Struct."
GetMemStructErrMessage:     db    "Get Memory Struct ERROR"
GetMemStructOKMessage:      db    "Get Memory Struct SUCCESSFUL!"

StartGetSVGAVBEInfoMessage: db    "Start Get SVGA VBE Info"
GetSVGAVBEInfoErrMessage:   db    "Get SVGA VBE Info ERROR"
GetSVGAVBEInfoOKMessage:    db    "Get SVGA VBE Info SUCCESSFUL!"

StartGetSVGAModeInfoMessage:db    "Start Get SVGA Mode Info"
GetSVGAModeInfoErrMessage:  db    "Get SVGA Mode Info ERROR"
GetSVGAModeInfoOKMessage:   db    "Get SVGA Mode Info SUCCESSFUL!"