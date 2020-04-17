#include <iostream>
#include <fstream>
#include "dwarfs.h"

int main(int argc, char **argv) {
    struct dwarfs_superblock sb;
    sb.dwarfs_magic = DWARFS_MAGIC;
    sb.dwarfs_blockc = 64;
    sb.dwarfs_reserved_blocks = 2;
    sb.dwarfs_free_blocks_count = 62;
    sb.dwarfs_free_inodes_count = 0;
    sb.dwarfs_data_start_block = DWARFS_FIRST_DATA_BLOCKNUM;
    sb.dwarfs_block_size = DWARFS_BLOCK_SIZE;
    sb.dwarfs_root_inode = 2;
    sb.dwarfs_inodec = 1;
    sb.dwarfs_blocks_per_group = 1;
    sb.dwarfs_frags_per_group = 1;
    sb.dwarfs_inodes_per_group = 1;
    sb.dwarfs_wtime = 0;
    sb.dwarfs_mtime = 0;
    sb.dwarfs_def_resgid = 0;
    sb.dwarfs_def_resuid = 0;
    sb.dwarfs_version_num = 1;
    sb.dwarfs_os = 1;

    for(char c : sb.padding)
        c = 10;

    std::ofstream imgfile("image", std::ios::binary | std::ios::out);
    imgfile.write((char*)&sb, sizeof(struct dwarfs_superblock));
    return 0;

}