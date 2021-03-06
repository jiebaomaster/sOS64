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

#### 处理器模式切换

处理器开机后默认运行在保护模式，在进入保护模式之前，需要准备一些必要的全局数据结构：
- GDT，LDT（可选）
- TSS（可选），如果想使用硬件多任务或者支持特权级的改变，则必须创建 TSS
- 页表，可选，如果要开启分页模式的话必须要有一个页目录表和页表

在这里，由于只是中间过渡（切换到保护模式后马上就再切换进入长模式），因此先不开启分页模式，也不用准备LDT，因此必须要准备的数据结构只剩下GDT和IDT。GDT中有两个描述符，一个32位代码段描述符，一个32位数据段描述符；而由于在进行模式切换前会使用cli指令（clear interrupt）屏蔽可屏蔽中断，因此这里的中段描述符表IDT只需要分配好空间即可，暂时不需要设置中段描述符。

从实模式切换到保护模式的步骤如下：

1. 关中断
2. 加载 gdt 和 idt
3. （若要使用分页可在这里开启）
4. 置位 CR0.PE，开启保护模式
5. 恢复中断

从保护模式切换到IA-32e模式的步骤如下：

1. 加载 64 位 gdt，并重置各段寄存器
2. 复位 CR0.PG，关闭分页
3. 置位 CR4.PAE，开启物理地址扩展功能（PAE）
4. 将页目录表（顶层页表PML4）的物理基地址加载到 CR3 寄存器中
5. 置位 IA32_EFER.LME，开启 IA-32e 模式
6. 置位 CR0.PG，开启分页机制


虚拟机运行效果如图：

![loader](image/loader_succ.png)