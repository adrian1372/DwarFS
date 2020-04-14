obj-m := dwarfs.o
dwarfs-objs := super.o dir.o file.o inode.o

CFLAGS_super.o := -DDEBUG

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
    make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
