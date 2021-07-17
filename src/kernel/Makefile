C_FLAGS = -mcmodel=large -fno-builtin -fno-stack-protector -m64 -c
LD_FLAGS = -b elf64-x86-64 -z muldefs -T Kernel.lds

all: system
	@echo 生成内核可执行代码 $< ...
	objcopy -I elf64-x86-64 -S -R ".eh_frame" -R ".comment" -O binary system kernel.bin

system: head.o main.o printk.o
	ld $(LD_FLAGS) -o system head.o main.o printk.o

head.o:	head.S
	@echo 编译汇编文件 $< ...
	gcc -E head.S > head.s
	as --64 -o head.o head.s

main.o:	main.c
	@echo 编译内核代码 $< ...
	gcc	$(C_FLAGS) main.c

printk.o: printk.c
	gcc	$(C_FLAGS) printk.c

.PHONY:clean
clean:
	rm -rf *.bin *.s *.o system kernel.bin