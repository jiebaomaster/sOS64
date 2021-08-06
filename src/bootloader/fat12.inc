RootDirSectors            equ   14  ; 根目录占用的扇区数
                                    ; (BPB_RootEntCnt * 32 + BPB_BytesPerSec - 1) / BPB_BytesPerSec = 14
SectorNumOfRootDirStart   equ   32  ; 根目录的起始扇区号
                                    ; BPB_RsvdSecCnt + BPB_FATSz16 * BPB_NumFATs = 25
SectorNumOfFAT1Start      equ   8   ; FAT1 表的起始扇区号，FAT1前只有一个保留扇区（引导扇区）

  BS_OEMName      db  'MINEboot'      ; 生产商名称
  BPB_BytesPerSec dw  0x200           ; 每扇区字节数 512
  BPB_SecPerClus  db  0x8             ; 每簇扇区数 8
  BPB_RsvdSecCnt  dw  0x1             ; 保留扇区数 1
  BPB_NumFATs     db  0x2             ; FAT 表的份数 2
  BPB_RootEntCnt  dw  0xe0            ; 根目录可容纳的目录项数 224
  BPB_TotSec16    dw  0x7d80          ; 总扇区数 32128
  BPB_Media       db  0xf0            ; 介质描述符
  BPB_FATSz16     dw  0xc             ; 每 FAT 扇区数 12
  BPB_SecPerTrk   dw  0x3d            ; 每磁道扇区数 61
  BPB_NumHeads    dw  0xff            ; 磁头数 255
  BPB_HiddSec     dd  0               ; 隐藏扇区数
  BPB_TotSec32    dd  0               ; 如果 BPB_TotSec16=0，则由这个值记录扇区数
  BS_DrvNum       db  0               ; int 13h 的驱动器号
  BS_Reserved1    db  0               ; 未使用
  BS_BootSig      db  29h             ; 扩展引导标记
  BS_VolID        dd  0               ; 卷序列号
  BS_VolLab       db  'boot loader'   ; 卷标
  BS_FileSysType  db  'FAT12   '      ; 文件系统类型