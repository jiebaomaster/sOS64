#!Makefile
TOPDIR := $(shell pwd)

ASM = nasm

BOOTIMG = a.img

S_SOURCES = $(shell find . -name "*.asm")
S_OBJECTS = $(patsubst %.asm, %.bin, $(S_SOURCES))

all: $(S_OBJECTS) kernel.bin update_image

%.bin: %.asm
	@echo 编译汇编文件 $< ...
	$(ASM) $(ASM_FLAGS) -o $@ $<

kernel.bin:
	make -C $(TOPDIR)/kernel

.PHONY:clean
clean:
	rm -rf $(S_OBJECTS)
	make -C $(TOPDIR)/kernel clean 

.PHONY:update_image
update_image:
	dd if=boot.bin of=$(BOOTIMG) bs=512 count=1 conv=notrunc
	sudo mount -o loop -t vfat $(BOOTIMG) /mnt
	cp loader.bin /mnt
	cp ./kernel/kernel.bin /mnt
	sync
	umount /mnt

.PHONY:qemu
qemu:
	qemu-system-x86_64 -boot a -fda $(BOOTIMG) -m 2048