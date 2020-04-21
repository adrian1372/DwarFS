#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/aio.h>
#include <linux/mpage.h>
#include <linux/slab.h>

/*
const struct inode_operations dwarfs_file_inode_operations = {
    .setattr        = generic_setattr,
    .getattr        = generic_getattr,
    .update_time    = generic_update_time,
};
*/

struct dwarfs_inode *dwarfs_getdinode(struct super_block *sb, uint64_t ino, struct buffer_head **bhptr) {
    
    uint64_t block;
    uint64_t offset;
    struct buffer_head *bh;

    *bhptr = NULL;
    if((ino < DWARFS_FIRST_INODE && ino != DWARFS_ROOT_INUM) || ino > le64_to_cpu(DWARFS_SB(sb)->dfsb->dwarfs_inodec)) {
        printk("Dwarfs: bad inode number %llu in dwarfs_getdinode\n", ino);
        return ERR_PTR(-EINVAL);
    }
    offset = ino * DWARFS_SB(sb)->dwarfs_inodesize;
    block = DWARFS_FIRST_INODE_BLOCK + ((ino * DWARFS_SB(sb)->dwarfs_inodesize) / DWARFS_BLOCK_SIZE); // Assumption: integer division rounds down

    if(!(bh = sb_bread(sb, block))) {
        printk("Dwarfs: Error encountered during I/O in dwarfs_getdinode for ino %llu. Possibly bad block: %llu\n", ino, block);
        return ERR_PTR(-EIO);
    }
    *bhptr = bh;
    offset = ino % (DWARFS_BLOCK_SIZE / DWARFS_SB(sb)->dwarfs_inodesize);
    return (struct dwarfs_inode *)(bh->b_data + offset);

}

// Heavily based on EXT2, should probably be changed to be more original
struct inode *dwarfs_inode_get(struct super_block *sb, uint64_t ino) {
    struct inode *inode;
    struct dwarfs_inode *dinode;
    struct dwarfs_inode_info *dinode_info;
    struct buffer_head *bh;
    int i;
    uid_t uid;
    gid_t gid;

    printk("Dwarfs: in dwarfs_inode_get\n");

    inode = iget_locked(sb, ino);
    if(!inode) {
        printk("Dwarfs: Failed to get inode in iget!\n");
        return ERR_PTR(-ENOMEM);
    }
    if(!(inode->i_state & I_NEW)) {// inode already exists, nothing more to do
        printk("Dwarfs: Found existing inode, returning\n");
        return inode;
    }

    pr_debug("Dwarfs: inode at ino %llu does not exist, creating new!\n", ino);
    
    /* If it doesn't exist, we need to create it */
    dinode_info = DWARFS_INODE(inode);
    dinode = dwarfs_getdinode(inode->i_sb, ino, &bh);
    if(IS_ERR(dinode)) {
        printk("DwarFS: Got a bad inode of ino: %llu\n", ino);
        return ERR_PTR(-EFSCORRUPTED);
    }

    printk("Got dinode of size: %llu\n", dinode->inode_size);

    inode->i_mode = le16_to_cpu(dinode->inode_mode);
    uid = (uid_t)le16_to_cpu(dinode->inode_uid_high);
    gid = (gid_t)le16_to_cpu(dinode->inode_gid_high);

    printk("write uid & gid\n");
    i_uid_write(inode, uid);
    i_gid_write(inode, gid);
    set_nlink(inode, le64_to_cpu(dinode->inode_linkc));

    printk("Getting size and times\n");
    inode->i_size = le64_to_cpu(dinode->inode_size);
    inode->i_atime.tv_sec = (signed)le64_to_cpu(dinode->inode_atime);
    inode->i_ctime.tv_sec = (signed)le64_to_cpu(dinode->inode_ctime);
    inode->i_mtime.tv_sec = (signed)le64_to_cpu(dinode->inode_mtime);
    inode->i_atime.tv_nsec = inode->i_ctime.tv_nsec = inode->i_mtime.tv_nsec = 0;
    dinode_info->inode_dtime = le32_to_cpu(dinode->inode_dtime);
    
    printk("Checking inode validity\n");
    // Now we can check validity.
    // Among other things, check if the inode is deleted.
    if(inode->i_nlink == 0 && (inode->i_mode == 0 || dinode_info->inode_dtime)) {
        printk("Dwarfs: inode is stale (deleted!) at ino: %llu\n", ino);
        return ERR_PTR(-ESTALE);
    }


    printk("Setting blocks, flags etc.\n");
    inode->i_blocks = le64_to_cpu(dinode->inode_blocks);
    dinode_info->inode_flags = le64_to_cpu(dinode->inode_flags);
    dinode_info->inode_fragaddr = le64_to_cpu(dinode->inode_fragaddr);
    dinode_info->inode_fragnum = dinode->inode_fragnum;
    dinode_info->inode_fragsize = dinode->inode_fragsize;
    
    printk("Reading inode size\n");
    if(i_size_read(inode) < 0) {
        printk("Dwarfs: Couldnt read inode size: ino %llu\n", ino);
        return ERR_PTR(-EFSCORRUPTED);
    }

    printk("Setting dinode_info dtime etc.\n");
    dinode_info->inode_dtime = 0;
    dinode_info->inode_state = 0;
//    dinode_info->inode_block_group = (ino - 1) / DWARFS_SB(inode->i_sb)->dwarfs_inodes_per_group;
    dinode_info->inode_dir_start_lookup = 0;

    printk("Setting dinode_info data blocks");
    for(i = 0; i < DWARFS_NUMBLOCKS; i++)
        dinode_info->inode_data[i] = dinode->inode_blocks[i];
    
    printk("Setting inode operations\n");
    inode->i_op = &dwarfs_dir_inode_operations;
    inode->i_fop = &dwarfs_dir_operations;
    inode->i_mapping->a_ops = &dwarfs_aops;

    printk("brelse\n");
    brelse(bh);

    printk("Unlocking the new inode\n");
    unlock_new_inode(inode);

    printk("Returning\n");
    return inode;
}

int dwarfs_get_iblock(struct inode *inode, sector_t iblock, struct buffer_head *bh_result, int create) {
	map_bh(bh_result, inode->i_sb, iblock + DWARFS_INODE(inode)->inode_data[0]); /* !!!!TODO: Use more than data block 0! */
	return 0;
}

static int dwarfs_readpage(struct file *file, struct page *page) {
    return mpage_readpage(page, dwarfs_get_iblock);
}

static int dwarfs_readpages(struct file *file, struct address_space *mapping, struct list_head *pages, unsigned nr_pages) {
    return mpage_readpages(mapping, pages, nr_pages, dwarfs_get_iblock);
}

static ssize_t dwarfs_direct_io(struct kiocb *iocb, struct iov_iter *iter) {
    struct inode *inode = file_inode(iocb->ki_filp);
	return blockdev_direct_IO(iocb, inode, iter, dwarfs_get_iblock);
}


const struct address_space_operations dwarfs_aops = {
    .readpage		= dwarfs_readpage,
	.readpages		= dwarfs_readpages,
	.direct_IO      = dwarfs_direct_io,
};
