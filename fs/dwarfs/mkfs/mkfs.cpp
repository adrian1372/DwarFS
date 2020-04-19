#include <iostream>
#include <fstream>
#include "dwarfs.h"

int main(int argc, char **argv) {
    struct dwarfs_superblock sb;
    struct dwarfs_inode inode_blank;

    // Fill the SB
    sb.dwarfs_magic = DWARFS_MAGIC;
    sb.dwarfs_blockc = 0;
    sb.dwarfs_reserved_blocks = 0;
    sb.dwarfs_free_blocks_count = 0;
    sb.dwarfs_free_inodes_count = 0;
    sb.dwarfs_data_start_block = DWARFS_FIRST_DATA_BLOCKNUM;
    sb.dwarfs_block_size = DWARFS_BLOCK_SIZE;
    sb.dwarfs_root_inode = 2;
    sb.dwarfs_inodec = 0;
    sb.dwarfs_blocks_per_group = 0;
    sb.dwarfs_frags_per_group = 0;
    sb.dwarfs_inodes_per_group = 0;
    sb.dwarfs_wtime = 0;
    sb.dwarfs_mtime = 0;
    sb.dwarfs_def_resgid = 0;
    sb.dwarfs_def_resuid = 0;
    sb.dwarfs_version_num = 1;
    sb.dwarfs_os = operating_systems::OS_LINUX;

    for(char c : sb.padding)
        c = 10;

    std::ofstream imgfile("image", std::ios::binary | std::ios::out);
    imgfile.write((char*)&sb, sizeof(struct dwarfs_superblock));
    
    std::cout << "Wrote superblock!" << std::endl;

    char emptyblock[DWARFS_BLOCK_SIZE];
    for(char c : emptyblock)
        c = 0;

    imgfile.write(emptyblock, DWARFS_BLOCK_SIZE);
    std::cout << "Wrote inode bitmap!" << std::endl;

    imgfile.write(emptyblock, DWARFS_BLOCK_SIZE);
    std::cout << "Wrote data bitmap!" << std::endl;

    // Fill the iNode
    inode_blank.inode_mode = inode_modes::I_DIR;
    inode_blank.inode_size = 0;
    inode_blank.inode_uid = 0;
    inode_blank.inode_gid = 0;
    inode_blank.inode_atime = 0;
    inode_blank.inode_ctime = 0;
    inode_blank.inode_mtime = 0;
    inode_blank.inode_dtime = 0;
    inode_blank.inode_blockc = 0;
    inode_blank.inode_linkc = 0;
    inode_blank.inode_flags = I_NEW;
    inode_blank.inode_reserved1 = 0;
    inode_blank.inode_fragaddr = 0;
    inode_blank.inode_fragnum = 0;
    inode_blank.inode_fragsize = 0;
    inode_blank.inode_padding1 = 10;
    inode_blank.inode_uid_high = 0;
    inode_blank.inode_gid_high = 0;
    inode_blank.inode_reserved2 = 0;

    for(uint64_t block : inode_blank.inode_blocks)
        block = 0;

    for(char c : inode_blank.padding)
        c = 10;

    int numnodes = (DWARFS_FIRST_DATA_BLOCKNUM - DWARFS_FIRST_INODE_BLOCKNUM) * (DWARFS_BLOCK_SIZE / sizeof(struct dwarfs_inode));

    for(int i = 0; i < numnodes; i++)
        imgfile.write((char*)&inode_blank, sizeof(struct dwarfs_inode));

    std::cout << "Wrote " << numnodes << " inodes" << std::endl;

    int numdatablocks = 55;
    for(int i = 0; i < 55; i++) {
        imgfile.write(emptyblock, DWARFS_BLOCK_SIZE);
    }
    std::cout << "Wrote " << numdatablocks << " empty data blocks" << std::endl;
    return 0;
}