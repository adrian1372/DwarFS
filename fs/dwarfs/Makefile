.PHONY: all clean rebuild

obj-m := dwarfs.o
dwarfs-objs := super.o dir.o inode.o alloc.o

CFLAGS_super.o := -DDEBUG

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

rebuild: clean all
