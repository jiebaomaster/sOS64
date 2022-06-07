#!Makefile
TOPDIR := $(shell pwd)
SUBDIRS := $(TOPDIR)/bootloader $(TOPDIR)/kernel

ASM = nasm

BOOTIMG = a.img
DEVICE = $(dev)

all: $(patsubst %, _dir_%, $(SUBDIRS)) update_image

$(patsubst %, _dir_%, $(SUBDIRS)) : 
	$(MAKE) -C $(patsubst _dir_%, %, $@)

clean: $(patsubst %, _dir2_%,$(SUBDIRS))

$(patsubst %, _dir2_%, $(SUBDIRS)) :
	$(MAKE) -C $(patsubst _dir2_%, %, $@) clean

.PHONY:update_image
update_image:
	sudo dd if=./bootloader/vm/boot.bin of=$(BOOTIMG) bs=512 count=1 conv=notrunc
	sudo mount -o loop -t vfat $(BOOTIMG) /mnt
	sudo cp -f ./bootloader/vm/loader.bin /mnt
	sudo cp -f ./kernel/kernel.bin /mnt
	sync
	sudo umount /mnt

.PHONY:load
load:
	sudo dd if=./bootloader/pc/boot.bin of=$(DEVICE) bs=512 count=1 conv=notrunc
	sudo mount -o loop -t vfat $(DEVICE) /mnt
	sudo cp -f ./bootloader/pc/loader.bin /mnt
	sudo cp -f ./kernel/kernel.bin /mnt
	sync
	sudo umount /mnt

.PHONY:qemu
qemu:
	qemu-system-x86_64 -boot a -fda $(BOOTIMG) -m 2048

.PHONY:boch
boch:
	bochs -f .bochsrc