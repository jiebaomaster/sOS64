# sOS64

## 参考

[《一个64位操作系统的设计与实现》学习笔记](https://github.com/yifengyou/The-design-and-implementation-of-a-64-bit-os)

## 实验记录

### bochs 安装

1. 下载[bochs-2.6.10](https://sourceforge.net/projects/bochs/files/bochs/2.6.10/)或者直接使用`helper/`目录下的压缩包
2. 运行安装脚本`sh ./helper/bochs-install.sh`，可能会有一些依赖错误

### 第一个引导程序

``` shell
# 在 sOS64/src 目录下，编译
make
# 新建一个软盘 =>1=>fd=>1.44M=>a.img
bximage
# 引导程序写入软盘
dd if=boot.bin of=a.img bs=512 count=1 conv=notrunc
# 运行虚拟机 =>6=>c
bochs -f .bochsrc
```

虚拟机运行效果如图：

![first_boot](image/first_boot.png)

### 加载程序

[操作系统的设计与实现——BootLoader引导加载程序](https://www.sunxiaokong.xyz/2020-08-08/lzx-babyos-1/)

Loader引导加载程序的任务主要有三个：检测硬件信息、处理器模式切换、向内核传递数据。这些工作能够让内核初始化之后正常运行。

- **检测硬件信息** Loader引导加载程序需要检测的硬件信息很多，主要是通过 BIOS 中断服务程序来获取和检测硬件信息 由于 BIOS 在上电自检出的大部分信息只能在实模式下获取，而且内核运行于非实模式下，那么就必须在进入内核程序前将这些信息检测出来，再作为参数提供给内核程序使用在这些硬件信息中，最重要的莫过于物理地址空间信息，只有正确解析出物理地址间信息，才能知道ROM RAM 设备寄存器间和内存空洞等资源的物理地址范围，进而将其交给内存管理单元模块加以维护。

- **处理器模式切换** 从起初 BIOS 运行的实模式（ real mode ），到 32 位操作系统使用的保护模式（ protect mode ），再到 64 位操作系统使用的 IA-32e 模式（ long mode ，长模式）， Loader引导加载程序必须历经这三个模式，才能使处理器运行于 64 位的 IA-32e 模式。

- **向内核传递数据** 这里的数据一部分是控制信息，这部分是纯软件的，比如启动模式之类的东西，另一部分是硬件数据信息，通常是指 Loader 引导加载程序检测出的硬件数据信息 Loader 引导加载程序将这些数据信息多半都保存在固定的内存地址中，并将数据起始内存地址和数据长度作为参数传递给内核，以供内核程序在初始化时分析 配置和使用，典型的数据信息有内存信息、VBE 信息等

``` shell
sudo make
make qemu
```

虚拟机运行效果如图：

![loader](image/loader.png)