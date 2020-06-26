#include <iostream>
#include <fstream>
#include "dwarfs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <ctime>
#include <math.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>

/*
 * Builds a file system on the given device (partition). Make sure the partition passed to this
 * program is empty; no attempts are made to check for other file systems that are present, and
 * running mkfs.dwarfs on a partition with an existing file system is extremely likely to lead to
 * corruption of any data present!
 */

int main(int argc, char **argv) {
    struct dwarfs_superblock sb;
    struct dwarfs_inode inode_blank;
    struct dwarfs_inode inode_root;
    size_t size;
    size_t totalblocks, datablocks, metadatablocks, inodeblocks, inodebitmapblocks, databitmapblocks;
    int inodeperblock = DWARFS_BLOCK_SIZE / sizeof(struct dwarfs_inode);
    int fd;

    if(argc < 2) {
	std::cout << "Usage: # mkfs.dwarfs <device>\n";
	return 0;
    }

    printf("%s\n", argv[1]);

    fd = open(argv[1], O_RDONLY);
    if(fd < 0) {
        std::cout << "An error occured while getting device fd\n";
	return fd;
    }

    ioctl(fd, BLKGETSIZE64, &size);
    if(!size) {
	std::cout << "Couldn't determine device size... are you running as root?\n";
	return -2;
    }

    printf("Size: %lu\n", size);

    std::cout << "Creating DwarFS filesystem on device " << argv[1] << std::endl;

    totalblocks = size / DWARFS_BLOCK_SIZE;
    metadatablocks = totalblocks / 5; // 1/5 of blocks for metadata
    datablocks = totalblocks - metadatablocks;

    databitmapblocks = ceil(datablocks / (DWARFS_BLOCK_SIZE * 8));
    metadatablocks -= (databitmapblocks + 1);

    inodebitmapblocks = ceil(metadatablocks / ((DWARFS_BLOCK_SIZE / inodeperblock) * 8));
    inodeblocks = metadatablocks - inodebitmapblocks;

    std::cout << "Volume layout:\n" \
            << "Superblock:             1\n" \
            << "Inode bitmap blocks:    " << inodebitmapblocks << std::endl \
            << "Data bitmap blocks:     " << databitmapblocks << std::endl \
            << "Inode blocks:           " << inodeblocks << std::endl \
            << "Data blocks:            " << datablocks << std::endl;
    

    // Fill the SB
    sb.dwarfs_magic = DWARFS_MAGIC;
    sb.dwarfs_blockc = datablocks;
    sb.dwarfs_reserved_blocks = 0;
    sb.dwarfs_free_blocks_count = sb.dwarfs_blockc - 1; // reserve one data block for the root inode
    sb.dwarfs_inode_start_block = 1 + inodebitmapblocks + databitmapblocks;
    sb.dwarfs_data_start_block = sb.dwarfs_inode_start_block + inodeblocks;
    sb.dwarfs_block_size = DWARFS_BLOCK_SIZE;
    sb.dwarfs_root_inode = 2;
    sb.dwarfs_inodec = inodeperblock * inodeblocks;
    sb.dwarfs_free_inodes_count = sb.dwarfs_inodec - 3; // reserve the root node
    sb.dwarfs_data_bitmap_start = sb.dwarfs_inode_bitmap_start + inodebitmapblocks;
    sb.dwarfs_inode_bitmap_start = 1;
    sb.dwarfs_wtime = 0;
    sb.dwarfs_mtime = 0;
    sb.dwarfs_def_resgid = 0;
    sb.dwarfs_def_resuid = 0;
    sb.dwarfs_version_num = 1;
    sb.dwarfs_os = operating_systems::OS_LINUX;

    for(char c : sb.padding)
        c = 10;

    std::ofstream imgfile(argv[1], std::ios::binary | std::ios::out);
    imgfile.write((char*)&sb, sizeof(struct dwarfs_superblock));
    
    std::cout << "Wrote superblock!" << std::endl;

    char emptyblock[DWARFS_BLOCK_SIZE];
    for(char c : emptyblock)
        c = 0;
    unsigned long *firstlong = (unsigned long*)emptyblock; // 00000111, reserve inodes 0, 1 and 2.
    firstlong[0] = 7;

    imgfile.write(emptyblock, DWARFS_BLOCK_SIZE);
    firstlong[0] = 0;
    for(int i = 1; i < inodebitmapblocks; i++)
        imgfile.write(emptyblock, DWARFS_BLOCK_SIZE);
    std::cout << "Wrote inode bitmap, size: " << inodebitmapblocks << std::endl;

    for(int i = 0; i < databitmapblocks; i++)
        imgfile.write(emptyblock, DWARFS_BLOCK_SIZE);
    std::cout << "Wrote data bitmap, size: " << databitmapblocks << std::endl;

    // Fill the iNode
    inode_blank.inode_mode = 0;
    inode_blank.inode_size = sizeof(struct dwarfs_inode);
    inode_blank.inode_uid = 0;
    inode_blank.inode_gid = 0;
    inode_blank.inode_dtime = 0;
    inode_blank.inode_blockc = 0;
    inode_blank.inode_linkc = 0;
    inode_blank.inode_flags = 0;

    inode_root = inode_blank;
    inode_root.inode_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    inode_root.inode_atime = inode_blank.inode_ctime = inode_blank.inode_mtime = time(NULL);

    std::cout << "Inode size: " << sizeof(struct dwarfs_inode) << std::endl;

    for(uint64_t block : inode_blank.inode_blocks)
        block = 0;
    for(uint64_t block : inode_root.inode_blocks)
        block = 0;

    for(char c : inode_blank.padding)
        c = 10;

    int numnodes = sb.dwarfs_inodec;

    for(int i = 0; i < numnodes; i++) {
        if(i == 2)
            imgfile.write((char*)&inode_root, sizeof(struct dwarfs_inode));
        else
            imgfile.write((char*)&inode_blank, sizeof(struct dwarfs_inode));   
    }

        
/*
    std::cout << "Wrote " << numnodes << " inodes" << std::endl;

    int numdatablocks = sb.dwarfs_blockc;
    for(int i = 0; i < numdatablocks; i++) {
        imgfile.write(emptyblock, DWARFS_BLOCK_SIZE);
    }
    std::cout << "Wrote " << numdatablocks << " empty data blocks" << std::endl;
  */
    return 0;
}
