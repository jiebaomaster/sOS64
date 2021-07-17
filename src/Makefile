#!Makefile
TOPDIR := $(shell pwd)
SUBDIRS := $(TOPDIR)/bootloader $(TOPDIR)/kernel

ASM = nasm

BOOTIMG = a.img

all: $(patsubst %, _dir_%, $(SUBDIRS)) update_image

$(patsubst %, _dir_%, $(SUBDIRS)) : 
	$(MAKE) -C $(patsubst _dir_%, %, $@)

clean: $(patsubst %, _dir2_%,$(SUBDIRS))

$(patsubst %, _dir2_%, $(SUBDIRS)) :
	$(MAKE) -C $(patsubst _dir2_%, %, $@) clean

.PHONY:update_image
update_image:
	dd if=./bootloader/boot.bin of=$(BOOTIMG) bs=512 count=1 conv=notrunc
	sudo mount -o loop -t vfat $(BOOTIMG) /mnt
	cp ./bootloader/loader.bin /mnt
	cp ./kernel/kernel.bin /mnt
	sync
	umount /mnt

.PHONY:qemu
qemu:
	qemu-system-x86_64 -boot a -fda $(BOOTIMG) -m 2048