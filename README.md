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
