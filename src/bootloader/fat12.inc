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