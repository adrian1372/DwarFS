.PHONY: all clean rebuild prepare

obj-m := sizetest.o
sizetest-objs := sizes.o

CFLAGS_super.o := -DDEBUG

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

rebuild: clean all

prepare:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	make -C mkfs/
	mkfs/mkfs.dwarfs
	cp mkfs/image mkfs/fs
	mkdir mkfs/fs/mnt
	echo "Dwarfs filesystem prepared, run mount.sh as root to get started!"
