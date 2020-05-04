#include <linux/buffer_head.h>

#include "dwarfs.h"

/*
 * Functions for allocating new space on the disk.
 * Handles both bitmaps, inodes and data blocks.
 */

int64_t dwarfs_inode_alloc(struct super_block *sb) {
    struct buffer_head *bmbh = NULL;
    int64_t ino = 2; // Temporary hack, 0 and 1 don't seem to be properly reserved
    printk("Dwarfs: inode_alloc\n");

    if(!(bmbh = read_inode_bitmap(sb))) {
        printk("Dwarfs: Unable to read inode bitmap\n");
        return -EIO;
    }
    ino = find_next_zero_bit_le(bmbh->b_data, DWARFS_BLOCK_SIZE, ino);
    if(!ino || ino >= dfsb->dwarfs_inodec) {
        printk("Dwarfs: No free inodes!\n");
        brelse(bmbh);
        return -ENOSPC;
    }
    test_and_set_bit(ino, (unsigned long *)bmbh->b_data);

    dwarfs_write_buffer(&bmbh, sb);
    return ino;
}

/*
 * Function to get the first available datablock and mark it busy in the bitmap.
 * This function returns the blocknum with the offset to account for the data blocks'
 * position in the filesystem. The caller may use the returned value as-is.
 * SUBJECT TO CHANGE
 */
int64_t dwarfs_data_alloc(struct super_block *sb) {
    struct buffer_head *bh = NULL;
    int64_t blocknum = 0;
    printk("Dwarfs: data_alloc");

    if(!(bh = read_data_bitmap(sb))) {
        printk("Dwarfs: unable to read data bitmap!\n");
        return -EIO;
    }
    blocknum = find_next_zero_bit_le(bh->b_data, DWARFS_BLOCK_SIZE, blocknum);
    if(!blocknum || (blocknum + 8) > 63) { // More magic numbers.. fix this! 8 -> first data block, 63 -> last FS block
        printk("Dwarfs: Couldn't find any free data blocks!\n");
        brelse(bh);
        return -ENOSPC;
    }
    test_and_set_bit(blocknum, (unsigned long *)bh->b_data);
    dwarfs_write_buffer(&bh, sb);   
    return blocknum + 8; // Even more magic numbers to be changed.
}