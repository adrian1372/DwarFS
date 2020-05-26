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
    struct dwarfs_superblock_info *dfsb_i = DWARFS_SB(sb);
    struct dwarfs_superblock *dfsb = dfsb_i->dfsb;
    int64_t ino = 0;
    int64_t bitmapblock = -1;

    mutex_lock_interruptible(&dfsb_i->dwarfs_bitmap_lock);
    do {
        if(bmbh) brelse(bmbh);
        bitmapblock++;
        ino = 0;
        if(!(bmbh = sb_bread(sb, dfsb->dwarfs_inode_bitmap_start + bitmapblock))) {
            printk("Dwarfs: Unable to read inode bitmap\n");
            mutex_unlock(&dfsb_i->dwarfs_bitmap_lock);
            return -EIO;
        }
        ino = find_next_zero_bit_le((unsigned long *)bmbh->b_data, sb->s_blocksize, ino);
        if(ino + bitmapblock * sb->s_blocksize > dfsb->dwarfs_inodec) {
            printk("Dwarfs: No free inodes! %lld > %llu\n", ino + bitmapblock * sb->s_blocksize, dfsb->dwarfs_inodec);
            brelse(bmbh);
            mutex_unlock(&dfsb_i->dwarfs_bitmap_lock);
            return -ENOSPC;
        }
    } while(ino >= sb->s_blocksize);
    dwarfs_flip_bitmap((unsigned long *)bmbh->b_data, ino);
    dfsb_i->dwarfs_free_inodes_count--;

    dwarfs_write_buffer(&bmbh, sb);
    mutex_unlock(&dfsb_i->dwarfs_bitmap_lock);

    printk("Dwarfs: allocated inode %lld\n", ino + sb->s_blocksize * bitmapblock);
    return ino + sb->s_blocksize * bitmapblock;
}

int dwarfs_inode_dealloc(struct super_block *sb, int64_t ino) {
    uint64_t bitmapblock = 0;
    struct buffer_head *bmbh;
    struct buffer_head *inodebh = NULL;
    struct dwarfs_inode *dinode = dwarfs_getdinode(sb, ino, &inodebh);
    struct dwarfs_superblock_info *dfsb_i = DWARFS_SB(sb);
    unsigned long *bitmap = NULL;
    int64_t err = 0;

    mutex_lock_interruptible(&dfsb_i->dwarfs_bitmap_lock);

    bmbh = read_inode_bitmap(sb, ino, &bitmapblock);
    bitmap = (unsigned long *)bmbh->b_data;

    if(IS_ERR(bmbh)) {
        err = PTR_ERR(bmbh);
    }
    if(IS_ERR(dinode)) {
        err = PTR_ERR(dinode);
    }
    if(IS_ERR(inodebh)) {
        err = PTR_ERR(inodebh);
    }
    if(err) goto outerr;

    dwarfs_flip_bitmap(bitmap, ino % sb->s_blocksize);
    dfsb_i->dwarfs_free_inodes_count++;
    dwarfs_write_buffer(&bmbh, sb);
    mutex_unlock(&dfsb_i->dwarfs_bitmap_lock);

    memset((char *)dinode, 0, sizeof(struct dwarfs_inode));
    dwarfs_write_buffer(&inodebh, sb);


    return 0;

outerr:
    mutex_unlock(&dfsb_i->dwarfs_bitmap_lock);
    return err;
}

/*
 * Function to get the first available datablock and mark it busy in the bitmap.
 * This function returns the blocknum with the offset to account for the data blocks'
 * position in the filesystem. The caller may use the returned value as-is.
 */
int64_t dwarfs_data_alloc(struct super_block *sb, struct inode *inode) {
    struct buffer_head *bmbh = NULL;
    struct buffer_head *datbh = NULL;
    struct dwarfs_superblock_info *dfsb_i = DWARFS_SB(sb);
    struct dwarfs_superblock *dfsb = dfsb_i->dfsb;
    unsigned long blocknum = 0;
    int64_t bitmapblock = -1;

    mutex_lock_interruptible(&dfsb_i->dwarfs_bitmap_lock);
    do {
        bitmapblock++;
        blocknum = 0;
        if(!(bmbh = sb_bread(sb, dfsb->dwarfs_data_bitmap_start + bitmapblock))) {
            printk("Dwarfs: unable to read data bitmap!\n");
            mutex_unlock(&dfsb_i->dwarfs_bitmap_lock);
            return -EIO;
        }
        blocknum = find_next_zero_bit_le((unsigned long *)bmbh->b_data, sb->s_blocksize, blocknum);
        if(blocknum * bitmapblock > dfsb->dwarfs_blockc) {
            printk("Dwarfs: Couldn't find any free data blocks!\n");
            brelse(bmbh);
            mutex_unlock(&dfsb_i->dwarfs_bitmap_lock);
	    return -ENOSPC;
        }
	if(blocknum < sb->s_blocksize)
	    break;
	brelse(bmbh);
    } while(blocknum >= sb->s_blocksize);
    test_and_set_bit(blocknum, (unsigned long *)bmbh->b_data);
    dfsb_i->dwarfs_free_blocks_count--;
    dwarfs_write_buffer(&bmbh, sb);
    mutex_unlock(&dfsb_i->dwarfs_bitmap_lock);

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

    return (int64_t)blocknum;
}

int dwarfs_data_dealloc_indirect(struct super_block *sb, struct inode *inode) {
    int i, j, blockc, blockpos;
    struct buffer_head *bmbh = NULL;
    struct buffer_head *ptrbh = NULL;
    struct buffer_head *databh = NULL;
    __le64 *buf = NULL;
    __le64 blocknum;
    unsigned long *bitmap = NULL;
    struct dwarfs_superblock_info *dfsb_i = DWARFS_SB(sb);

    // Figure out how many linked list levels we're deallocating.
    blockc = dwarfs_divround(inode->i_blocks - DWARFS_INODE_INDIR, (sb->s_blocksize / sizeof(__le64)) - 1);

    if(S_ISLNK(inode->i_mode))
        return 0;

    /*
     * We possibly need to dealloc multiple levels of the linked list, so
     * for each level, dealloc 511 data pointers, then dealloc the pointer to
     * this level of the linked list, before moving on to the next level and repeating.
     */
    blockpos = DWARFS_INODE(inode)->inode_data[DWARFS_INODE_INDIR];
    for(i = 0; i < blockc; i++) {
        ptrbh = sb_bread(sb, blockpos);
        if(IS_ERR(ptrbh) || !ptrbh)
            return -EIO;
        buf = (__le64 *)ptrbh->b_data;
        for(j = 0; j < (sb->s_blocksize / sizeof(__le64)) - 1; j++) {
            if(buf[j] == 0 || buf[j] > DWARFS_SB(sb)->dfsb->dwarfs_blockc + dwarfs_datastart(sb))
                continue;
            blocknum = buf[j];
            databh = sb_bread(sb, blocknum);
            if(IS_ERR(databh) || !databh)
                return -EIO;

            memset(databh->b_data, 0, databh->b_size);
            dwarfs_write_buffer(&databh, sb);

            bmbh = read_data_bitmap(sb, blocknum, NULL);
            bitmap = (unsigned long *)bmbh->b_data;
            blocknum -= dwarfs_datastart(sb); // account for the position in the bitmap
            dwarfs_flip_bitmap(bitmap, blocknum % sb->s_blocksize);
            dfsb_i->dwarfs_free_blocks_count++;
            dwarfs_write_buffer(&bmbh, sb);
            bmbh = NULL;
            bitmap = NULL;
        }
        bmbh = read_data_bitmap(sb, blockpos, NULL);
        bitmap = (unsigned long *)bmbh->b_data;
        blockpos -= dwarfs_datastart(sb);
        dwarfs_flip_bitmap(bitmap, blockpos % sb->s_blocksize); // Dealloc the pointer to the list
        dfsb_i->dwarfs_free_blocks_count++;
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
    struct dwarfs_superblock_info *dfsb_i = DWARFS_SB(sb);
    struct dwarfs_inode_info *dinode_i = DWARFS_INODE(inode);
    int i;
    unsigned long *bitmap = NULL;

    if(S_ISLNK(inode->i_mode))
        return 0;
    if(IS_ERR(dinode_i))
        return PTR_ERR(dinode_i);

    /*
     * Direct blocks are simple, except that index 14 might point to a
     * linked list and must be handled separately
     */
    mutex_lock_interruptible(&dfsb_i->dwarfs_bitmap_lock);
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
            brelse(bmbh);
            mutex_unlock(&dfsb_i->dwarfs_bitmap_lock);
	    printk("Couldn't get databh\n");
            return -EIO;
        }
        datablock = (char *)databh->b_data;
        memset(datablock, 0, DWARFS_BLOCK_SIZE);
        dwarfs_write_buffer(&databh, sb);
        dinode_i->inode_data[i] = 0;

        blocknum -= dwarfs_datastart(sb); // account for the position in the bitmap
        dwarfs_flip_bitmap(bitmap, blocknum % sb->s_blocksize);
        dfsb_i->dwarfs_free_blocks_count++;
        dwarfs_write_buffer(&bmbh, sb);
        bitmap = NULL;
    }
    mutex_unlock(&dfsb_i->dwarfs_bitmap_lock);
    inode->i_blocks = 0;
    return 0;
}
