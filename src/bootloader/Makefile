#!Makefile
ASM = nasm

S_SOURCES = $(shell find . -name "*.asm")
S_OBJECTS = $(patsubst %.asm, %.bin, $(S_SOURCES))

all: $(S_OBJECTS)

%.bin: %.asm
	@echo 编译汇编文件 $< ...
	$(ASM) $(ASM_FLAGS) -o $@ $<

.PHONY:clean
clean:
	rm -rf $(S_OBJECTS)