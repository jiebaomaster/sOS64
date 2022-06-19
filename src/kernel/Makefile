# 引导方式 BIOS/UEFI
BOOTLOADER := BIOS
# 启动平台 VM/PC
PLATFORM := VM
# 支持的中断控制器 APIC/8259
PIC := APIC
C_SOURCES_ORIGIN = $(shell find . -name "*.c")
PIC_SOURCES = $(shell find "./device/pic" -name "*.c")
TARGET_PIC_SOURCES = $(shell find "./device/pic" -name "*$(PIC).c")
TARGET_PIC_OBJECTS = $(patsubst %.c, %.o, $(TARGET_PIC_SOURCES))

C_SOURCES = $(filter-out $(PIC_SOURCES), $(C_SOURCES_ORIGIN))
C_OBJECTS = $(patsubst %.c, %.o, $(C_SOURCES))
S_SOURCES = $(shell find . -name "*.S")
S_OBJECTS = $(patsubst %.S, %.o, $(S_SOURCES))

ROOT_INCLUDE_PATH =  $(shell pwd)/include

CC = gcc
C_FLAGS = -mcmodel=large -fno-builtin -fno-stack-protector -m64 -c -I $(ROOT_INCLUDE_PATH) -D$(PIC) -D$(BOOTLOADER) -D$(PLATFORM)
ASM = as
ASM_FLAGS = --64
LD_FLAGS = -b elf64-x86-64 -z muldefs -T Kernel.lds

all: system
	@echo 生成内核可执行代码 $< ...
	objcopy -I elf64-x86-64 -S -R ".eh_frame" -R ".comment" -O binary system kernel.bin

system: $(C_OBJECTS) $(S_OBJECTS) $(TARGET_PIC_OBJECTS)
	ld $(LD_FLAGS) -o system init/head.o $(C_OBJECTS) $(TARGET_PIC_OBJECTS) $(filter-out %head.o, $(S_OBJECTS))

.c.o:
	@echo 编译代码文件 $< ...
	$(CC) $(C_FLAGS) $< -o $@

.S.o:
	@echo 编译汇编文件 $< ...
	$(CC) -I $(ROOT_INCLUDE_PATH) -E $< > $(patsubst %.S, %.s, $<) -D$(BOOTLOADER) -D$(PLATFORM)
	$(ASM) $(ASM_FLAGS) -o $@ $(patsubst %.S, %.s, $<)

.PHONY:clean
clean:
	rm -rf */*.bin */*.s */*.o */*/*.o system kernel.bin