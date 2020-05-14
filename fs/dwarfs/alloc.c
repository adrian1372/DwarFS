#include <linux/buffer_head.h>

#include "dwarfs.h"

/*
 * Functions for allocating new space on the disk.
 * Handles both bitmaps, inodes and data blocks.
 */

int64_t dwarfs_inode_alloc(struct super_block *sb) {
    struct buffer_head *bmbh = NULL;
    int64_t ino = 0;
    printk("Dwarfs: inode_alloc\n");

    if(!(bmbh = read_inode_bitmap(sb))) {
        printk("Dwarfs: Unable to read inode bitmap\n");
        return -EIO;
    }
    printk("Dwarfs bitmap buffer_head: %lu, %llu\n", bmbh->b_size, bmbh->b_blocknr);
    printk("Find next zero bit input: %lu\n", ((unsigned long *)bmbh->b_data)[0]);
    ino = find_next_zero_bit_le((unsigned long *)bmbh->b_data, DWARFS_BLOCK_SIZE, ino);
    if(!ino || ino >= DWARFS_SB(sb)->dfsb->dwarfs_inodec) {
        printk("Dwarfs: No free inodes! %lld > %llu\n", ino, DWARFS_SB(sb)->dfsb->dwarfs_inodec);
        brelse(bmbh);
        return -ENOSPC;
    }
    test_and_set_bit(ino, (unsigned long *)bmbh->b_data);
    printk("test and set bit output: %d\n", bmbh->b_data[0]);


    dwarfs_write_buffer(&bmbh, sb);
    return ino;
}

int dwarfs_inode_dealloc(struct super_block *sb, int64_t ino) {
    struct buffer_head *bmbh = read_inode_bitmap(sb);
    struct buffer_head *inodebh = NULL;
    struct dwarfs_inode *dinode = dwarfs_getdinode(sb, ino, &inodebh);
    int bitmapgroup;
    int offset;
    unsigned long *bitmap = (unsigned long *)bmbh->b_data;
    printk("Dwarfs: inode_dealloc\n");

    if(IS_ERR(bmbh))
        return PTR_ERR(bmbh);
    if(IS_ERR(dinode))
        return PTR_ERR(dinode);
    if(IS_ERR(inodebh))
        return PTR_ERR(inodebh);

    memset((char *)dinode, 0, sizeof(struct dwarfs_inode));
    dwarfs_write_buffer(&inodebh, sb);

    bitmapgroup = ino / (sizeof(unsigned long) * 8);
    offset = ino % (sizeof(unsigned long) * 8);
    bitmap[bitmapgroup] ^= 1 << offset;
    dwarfs_write_buffer(&bmbh, sb);
    return 0;
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
    if(((blocknum + 8) < 8) || blocknum > DWARFS_SB(sb)->dfsb->dwarfs_blockc) { // More magic numbers.. fix this! 8 -> first data block, 63 -> last FS block
        printk("Dwarfs: Couldn't find any free data blocks!\n");
        brelse(bh);
        return -ENOSPC;
    }
    test_and_set_bit(blocknum, (unsigned long *)bh->b_data);
    dwarfs_write_buffer(&bh, sb);   
    return blocknum + 8; // Even more magic numbers to be changed.
}

int dwarfs_data_dealloc(struct super_block *sb, struct inode *inode) {
    struct buffer_head *bmbh = read_data_bitmap(sb);
    struct buffer_head *databh = NULL;
    char *datablock = NULL;
    struct dwarfs_inode_info *dinode_i = DWARFS_INODE(inode);
    int bitmapgroup, offset, i;
    unsigned long *bitmap = (unsigned long *)bmbh->b_data;
    printk("Dwarfs: data_dealloc\n");

    if(S_ISLNK(inode->i_mode))
        return 0;
    if(IS_ERR(bmbh))
        return PTR_ERR(bmbh);
    if(IS_ERR(databh))
        return PTR_ERR(databh);
    if(IS_ERR(dinode_i))
        return PTR_ERR(dinode_i);

    for(i = 0; i < DWARFS_NUMBLOCKS; i++) { 
        int64_t blocknum = dinode_i->inode_data[i];

        if(blocknum == 0)
            continue;
        databh = sb_bread(sb, blocknum);
        datablock = (char *)databh->b_data;
        memset(datablock, 0, DWARFS_BLOCK_SIZE);
        dwarfs_write_buffer(&databh, sb);

        bitmapgroup = blocknum / (sizeof(unsigned long) * 8);
        offset = blocknum % (sizeof(unsigned long) * 8);
        bitmap[bitmapgroup] ^= 1 << offset;
    }
    dwarfs_write_buffer(&bmbh, sb);

    return 0;
}