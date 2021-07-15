all: system
	@echo 生成内核可执行代码 $< ...
	objcopy -I elf64-x86-64 -S -R ".eh_frame" -R ".comment" -O binary system kernel.bin

system: head.o main.o printk.o
	ld -b elf64-x86-64 -z muldefs -o system head.o main.o printk.o -T Kernel.lds

head.o:	head.S
	@echo 编译汇编文件 $< ...
	gcc -E head.S > head.s
	as --64 -o head.o head.s

main.o:	main.c
	@echo 编译内核代码 $< ...
	gcc	-mcmodel=large -fno-builtin -fno-stack-protector -m64 -c main.c

printk.o: printk.c
	gcc	-mcmodel=large -fno-builtin -fno-stack-protector -m64 -c printk.c

.PHONY:clean
clean:
	rm -rf *.bin *.s *.o system kernel.bin