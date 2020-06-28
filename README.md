# DwarFS File System

<b>DISCLAIMER:</b> DwarFS was developed as part of a bachelor project at VU Amsterdam. This system is not warrantied to be completely stable and durable. DwarFS should not in its current state be used as a replacement for other file systems to store important data. Usage of DwarFS is at the users' own risk; VU Amsterdam and the developer are not liable for any problems, damage or loss of data that may occur during installation or usage of DwarFS.

## Description
DwarFS is a minuscule, written-from-scratch file system created with simplicity in mind. The system was created as part of a bachelor project in computer science, to investigate the performance differences between modern general-purpose file systems and a small, special-purpose file system. DwarFS has specifically been made to only perform simple read/write operations as quickly as possible.

In its current state, DwarFS is very experimental, and should not be used as a root file system to replace the state-of-the-art file systems, or as storage for important data. DwarFS does, however, provide a skeleton for further development towards a file system fitting your specific needs.


### Features
DwarFS currently has simple read/write I/O, as well as some system calls implemented. Most importantly, `open`, `stat`, `fsync`, `close`, `read` and `write`. Any programs or scripts that only require these commands should work correctly on DwarFS. DwarFS also has some support for special files, however only FIFO pipes have been properly tested for correct behavior.


### Requirements
DwarFS has not been tested extensively on various setups and Linux distributions. It is only guaranteed to work under the following setup:
* Ubuntu 20.04
* Linux kernel 5.4.X
* GCC 9.3.0

Some kernel updates make changes to the data structures used by DwarFS, and thus older versions of the kernel might not be compatible with DwarFS. Furthermore, DwarFS might not build on other Linux distributions than Ubuntu, due to the method for building kernel modules differing. If such is the case, consult your distribution's documentation on out-of-tree module compilation.

## Usage

This section follows the convention that terminal commands preceded by `$` should be run as a normal user, while commands preceded by `#` must be run as super-user.

### Setup
To test DwarFS without the need for physical hardware, it is possible to mount the system on a `loop device` or a `ram block device`. To create a ram block device, install the `brd` kernel module.
```
# modprobe brd rd_nr=X rd_size=Y
```
`rd_nr` is the amount of ram block devices to create, while `rd_size` is the size of each ram block device. Installing the `brd` module this way will create `/dev/ramX` devices, on which DwarFS can be mounted by following the instructions in the following subsections.


### Installation
DwarFS can be installed as a normal kernel module, and mounted as any other file system. To install the system, clone the repository and run 
```
$ make
# insmod dwarfs.ko
```
in the `dwarfs` directory. The installation can then be verified with `lsmod` and `dmesg`. If DwarFS is in the list of installed modules, and the kernel buffer doesn't contain error messages related to DwarFS, the installation was successful. 

### Mkfs
DwarFS comes with a `mkfs` utility for creating the DwarFS file system on a device. To use this utility, enter the `mkfs` directory and run `make`. Then, to create the DwarFS file system on device `DEV`, run:
```
# ./mkfs.dwarfs DEV
```

mkfs.dwarfs will then add the structures needed to run the file system on the device, and print statistics for the amount of inodes, data blocks and bitmaps that have been allocated.What is an example of a disclaimer?

<b>WARNING:</b> mkfs should <b>NEVER</b> be run on a partition that may contain data you cannot afford to lose. The utility makes no effort to search for existing file systems on the partition/device given to it, and any existing files <b>WILL</b> be irreversibly corrupted/lost.


### Mounting
DwarFS can be mounted using the `mount` tool. To mount a DwarFS file system on device DEV to mount point MNT:
```
# mount DEV MNT -t dwarfs
```
Make sure that `mkfs.dwarfs` has been run on the partition before attempting to mount it. If DwarFS cannot find its magic number, it will cancel the mount process.


### Uninstall
To uninstall DwarFS from your system, first unmount the file system with
```
# umount MNT
```
Where MNT is the mount point of the DwarFS partition. After unmounting the system, the DwarFS module can be uninstalled with 
```
# rmmod dwarfs
```

## Tests
For the sake of reproducibility, all tests used in the thesis are supplied here. To get the sizes of the disk structures of DwarFS and Ext4, a kernel module that prints their sizes during installation was created. This can be found under `dwarfs/tests/structures`. The module should be installed in the same way as the Dwarfs kernel module, and its output can be found in `dmesg`. After the results have been printed to `dmesg`, the sizetest can be uninstalled with `rmmod`.

The programs for testing the system call latencies can be found under `dwarfs/tests/check-syscalls`. Each .c file contains further information and usage about its test(s). Note that the path to the file to run the tests on has been hard-coded. To specify where the tests should be run, change the `path` variable at the start of the test you want to run.


### FIO
The rest of the tests were performed through `fio` jobs. The tests running on loop devices were performed through variations of:
```
$ fio --name=test --rw=X --size=Y --numjobs=Z --bs=I
```

X is the type of I/O that should be run. For the read bandwidth tests, \emph{randread} was used, while \emph{randwrite} was used for the write bandwidth tests. Y is the size of the files, e.g. 10G for the initial bandwidth tests, or 64M for the scalability tests. Z is the number of instances of the job that should run in paralell. For the sequential tests this number is 1, while for the scalabiliy tests this was powers of two between 1 and 32. I is the buffer size that is used for the job (e.g. 128k or 1M).

To drop the caches between each FIO job, so that repeatedly working on the same file does not end up skewing the results, use the following commands:
```
# echo 1 > /proc/sys/vm/drop_caches
# echo 2 > /proc/sys/vm/drop_caches
# echo 3 > /proc/sys/vm/drop_caches
```

For the tests with small files, the jobs had to be run as time-based and with the O\_DIRECT flag set. Thus, the fio command is slightly different:
```
$ fio --name=test --rw=randread --runtime=180 --numjobs=X --size=Y --bs=Z \
  --time_based=1 --direct=1
```
X is the amount of threads to use for the test, Y is the size of the file to be read by each thread, and Z is the buffer size to use in the test. This command will open the test file(s) with O\_DIRECT and read for 3 minutes (180 seconds).