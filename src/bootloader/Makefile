BOOTLOADERS := $(shell ls -d */)

all: $(patsubst %, _dir_%, $(BOOTLOADERS))

$(patsubst %, _dir_%, $(BOOTLOADERS)) : 
	$(MAKE) -C $(patsubst _dir_%, %, $@)

clean: $(patsubst %, _dir2_%,$(BOOTLOADERS))

$(patsubst %, _dir2_%, $(BOOTLOADERS)) :
	$(MAKE) -C $(patsubst _dir2_%, %, $@) clean