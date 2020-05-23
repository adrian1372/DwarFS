#include <linux/buffer_head.h>
#include <linux/limits.h>

#include "dwarfs.h"

/*
 * Functions for allocating new space on the disk.
 * Handles both bitmaps, inodes and data blocks.
 */

static inline void dwarfs_flip_bitmap(unsigned long *bitmap, __le64 blocknum) {
    test_and_change_bit(blocknum, bitmap);
}

int64_t dwarfs_inode_alloc(struct super_block *sb) {
    struct buffer_head *bmbh = NULL;
    struct dwarfs_superblock *dfsb = DWARFS_SB(sb)->dfsb;
    int64_t ino = 0;
    int64_t bitmapblock = -1;
    printk("Dwarfs: inode_alloc\n");

    do {
        if(bmbh) brelse(bmbh);
        bitmapblock++;
        ino = 0;
        if(!(bmbh = sb_bread(sb, dfsb->dwarfs_inode_bitmap_start + bitmapblock))) {
            printk("Dwarfs: Unable to read inode bitmap\n");
            return -EIO;
        }
        ino = find_next_zero_bit_le((unsigned long *)bmbh->b_data, sb->s_blocksize, ino);
        if(ino + bitmapblock * sb->s_blocksize > dfsb->dwarfs_inodec) {
            printk("Dwarfs: No free inodes! %lld > %llu\n", ino + bitmapblock * sb->s_blocksize, dfsb->dwarfs_inodec);
            brelse(bmbh);
            return -ENOSPC;
        }
    } while(ino == sb->s_blocksize);
    dwarfs_flip_bitmap((unsigned long *)bmbh->b_data, ino);

    dwarfs_write_buffer(&bmbh, sb);
    printk("Dwarfs: allocated inode %lld\n", ino + sb->s_blocksize * bitmapblock);
    return ino + sb->s_blocksize * bitmapblock;
}

int dwarfs_inode_dealloc(struct super_block *sb, int64_t ino) {
    uint64_t bitmapblock = 0;
    struct buffer_head *bmbh = read_inode_bitmap(sb, ino, &bitmapblock);
    struct buffer_head *inodebh = NULL;
    struct dwarfs_inode *dinode = dwarfs_getdinode(sb, ino, &inodebh);
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

    dwarfs_flip_bitmap(bitmap, ino % sb->s_blocksize);
    dwarfs_write_buffer(&bmbh, sb);
    printk("Deallocated inode %lld\n", ino);
    return 0;
}

/*
 * Function to get the first available datablock and mark it busy in the bitmap.
 * This function returns the blocknum with the offset to account for the data blocks'
 * position in the filesystem. The caller may use the returned value as-is.
 */
int64_t dwarfs_data_alloc(struct super_block *sb, struct inode *inode) {
    struct buffer_head *bmbh = NULL;
    struct buffer_head *datbh = NULL;
    struct dwarfs_superblock *dfsb = DWARFS_SB(sb)->dfsb;
    unsigned long blocknum = 0;
    int64_t bitmapblock = -1;
    printk("Dwarfs: data_alloc");

    do {
        if(bmbh) brelse(bmbh);
        bitmapblock++;
        blocknum = 0;
        if(!(bmbh = sb_bread(sb, dfsb->dwarfs_data_bitmap_start + bitmapblock))) {
            printk("Dwarfs: unable to read data bitmap!\n");
            return -EIO;
        }
        blocknum = find_next_zero_bit_le((unsigned long *)bmbh->b_data, sb->s_blocksize, blocknum);
        if(blocknum * bitmapblock > dfsb->dwarfs_blockc) {
            printk("Dwarfs: Couldn't find any free data blocks!\n");
            brelse(bmbh);
            return -ENOSPC;
        }
    } while(blocknum > sb->s_blocksize);
    test_and_set_bit(blocknum, (unsigned long *)bmbh->b_data);
    dwarfs_write_buffer(&bmbh, sb);

    // zero-initalise the new block
    blocknum = blocknum + (bitmapblock * sb->s_blocksize) + dwarfs_datastart(sb);
    datbh = sb_bread(sb, blocknum);
    if(!datbh) {
        printk("Dwarfs: couldn't get BH for the new datablock: %lu\n", blocknum);
        return -EIO;
    }
    memset(datbh->b_data, 0, datbh->b_size);
    dwarfs_write_buffer(&datbh, sb);
    inode->i_blocks++;

    printk("Dwarfs: successfully allocated block %lu\n", blocknum);

    return (int64_t)blocknum; // Even more magic numbers to be changed.
}

int dwarfs_data_dealloc_indirect(struct super_block *sb, struct inode *inode) {
    int i, j, blockc, blockpos;
    struct buffer_head *bmbh = NULL;
    struct buffer_head *ptrbh = NULL;
    struct buffer_head *databh = NULL;
    __le64 *buf = NULL;
    __le64 blocknum;
    unsigned long *bitmap = NULL;

    printk("Dwarfs: data dealloc indirect\n");

    // Figure out how many linked list levels we're deallocating.
    blockc = dwarfs_divround(inode->i_blocks - DWARFS_INODE_INDIR, (sb->s_blocksize / sizeof(__le64)) - 1);

    if(S_ISLNK(inode->i_mode))
        return 0;

    /*
     * We possibly need to dealloc multiple levels of the linked list, so
     * for each level, dealloc 511 data pointers, then dealloc the pointer to
     * this level of the linked list, before moving on to the next level and repeating.
     */
    printk("blockc: %d\n", blockc);
    blockpos = DWARFS_INODE(inode)->inode_data[DWARFS_INODE_INDIR];
    for(i = 0; i < blockc; i++) {
        ptrbh = sb_bread(sb, blockpos);
        if(IS_ERR(ptrbh) || !ptrbh)
            return -EIO;
        buf = (__le64 *)ptrbh->b_data;
        for(j = 0; j < (sb->s_blocksize / sizeof(__le64)) - 1; j++) {
            printk("Entered for loop: %llu\n", buf[j]);
            if(buf[j] == 0 || buf[j] > DWARFS_SB(sb)->dfsb->dwarfs_blockc + dwarfs_datastart(sb))
                continue;
            blocknum = buf[j];
            printk("Dwarfs: deallocating block %lld (bm: %lld)\n", blocknum, blocknum-dwarfs_datastart(sb));
            databh = sb_bread(sb, blocknum);
            if(IS_ERR(databh) || !databh)
                return -EIO;

            memset(databh->b_data, 0, databh->b_size);
            dwarfs_write_buffer(&databh, sb);

            bmbh = read_data_bitmap(sb, blocknum, NULL);
            bitmap = (unsigned long *)bmbh->b_data;
            blocknum -= dwarfs_datastart(sb); // account for the position in the bitmap
            dwarfs_flip_bitmap(bitmap, blocknum % sb->s_blocksize);
            dwarfs_write_buffer(&bmbh, sb);
            bmbh = NULL;
            bitmap = NULL;
            printk("Deallocated: %llu\n", blocknum);
        }
        bmbh = read_data_bitmap(sb, blockpos, NULL);
        bitmap = (unsigned long *)bmbh->b_data;
        blockpos -= dwarfs_datastart(sb);
        dwarfs_flip_bitmap(bitmap, blockpos % sb->s_blocksize); // Dealloc the pointer to the list
        dwarfs_write_buffer(&bmbh, sb);
        bmbh = NULL;
        bitmap = NULL;

        blockpos = buf[(sb->s_blocksize / sizeof(__le64)) - 1];
        memset(ptrbh->b_data, 0, ptrbh->b_size); // level done, set all ptrs 0
        dwarfs_write_buffer(&ptrbh, sb);
    }
    return 0;
}

int dwarfs_data_dealloc(struct super_block *sb, struct inode *inode) {
    struct buffer_head *bmbh = NULL;
    struct buffer_head *databh = NULL;
    char *datablock = NULL;
    struct dwarfs_inode_info *dinode_i = DWARFS_INODE(inode);
    int i;
    unsigned long *bitmap = NULL;
    printk("Dwarfs: data_dealloc\n");

    if(S_ISLNK(inode->i_mode))
        return 0;
    if(IS_ERR(dinode_i))
        return PTR_ERR(dinode_i);

    /*
     * Direct blocks are simple, except that index 14 might point to a
     * linked list and must be handled separately
     */
    for(i = 0; i < DWARFS_NUMBLOCKS; i++) {
        int64_t blocknum = dinode_i->inode_data[i];

        if(blocknum == 0)
            continue;
        bmbh = read_data_bitmap(sb, blocknum, NULL);
        bitmap = (unsigned long *)bmbh->b_data;

        if(i == DWARFS_NUMBLOCKS-1) { // indirect block
            dwarfs_data_dealloc_indirect(sb, inode);
            dinode_i->inode_data[i] = 0;
            break;
        }
        databh = sb_bread(sb, blocknum);
        if(!databh) {
            printk("Couldn't get databh\n");
            return -EIO;
        }
        datablock = (char *)databh->b_data;
        memset(datablock, 0, DWARFS_BLOCK_SIZE);
        dwarfs_write_buffer(&databh, sb);
        dinode_i->inode_data[i] = 0;

        blocknum -= dwarfs_datastart(sb); // account for the position in the bitmap
        dwarfs_flip_bitmap(bitmap, blocknum % sb->s_blocksize);
        dwarfs_write_buffer(&bmbh, sb);
        bitmap = NULL;
        printk("Dwarfs: deallocated block %lld\n", blocknum);
    }
    inode->i_blocks = 0;
    printk("Dwarfs: return from data_dealloc\n");
    return 0;
}