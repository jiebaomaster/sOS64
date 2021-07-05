#!Makefile

ASM = nasm

BOOTIMG = a.img

S_SOURCES = $(shell find . -name "*.asm")
S_OBJECTS = $(patsubst %.asm, %.bin, $(S_SOURCES))

all: $(S_OBJECTS) update_image

%.bin: %.asm
	@echo 编译汇编文件 $< ...
	$(ASM) $(ASM_FLAGS) -o $@ $<

.PHONY:clean
clean:
	rm $(S_OBJECTS)

.PHONY:update_image
update_image:
	dd if=boot.bin of=$(BOOTIMG) bs=512 count=1 conv=notrunc
	mkdir _kernel
	sudo mount -o loop -t vfat $(BOOTIMG) ./_kernel
	cp loader.bin ./_kernel
	cp kernel.bin ./_kernel
	sync
	umount _kernel
	rmdir _kernel

.PHONY:qemu
qemu:
	qemu-system-x86_64 -boot a -fda $(BOOTIMG) -m 2048