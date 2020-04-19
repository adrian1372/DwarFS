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

struct inode *dwarfs_inode_get(struct super_block *sb, uint64_t ino) {
    struct inode *inode;
    struct dwarfs_inode *dinode;
    struct dwarfs_inode_info *dinode_info;
    struct buffer_head *bh;
    int i;
    uid_t uid;
    gid_t gid;

    inode = iget_locked(sb, ino);
    if(!inode)
        pr_err("Dwarfs: Failed to get inode in iget!\n");
        return ERR_PTR(-ENOMEM);
    if(!(inode->i_state & I_NEW)) // inode already exists, nothing more to do
        return inode;

    pr_debug("Dwarfs: inode at ino %llu does not exist, creating new!\n", ino);
    
    /* If it doesn't exist, we need to create it */
    dinode_info = DWARFS_INODE(inode);
    dinode = dwarfs_geti(inode->i_sb, ino, &bh);
    if(IS_ERR(dinode)) {
        pr_err("DwarFS: Got a bad inode of ino: %llu\n", ino);
        return PTR_ERR(dinode);
    }

    inode->i_mode = le16_to_cpu(dinode->inode_mode);
    uid = (uid_t)le16_to_cpu(dinode->inode_uid_high);
    gid = (gid_t)le16_to_cpu(dinode->inode_gid_high);

    i_uid_write(inode, uid);
    i_gid_write(inode, gid);
    set_nlink(inode, le64_to_cpu(dinode->inode_linkc));

    inode->i_size = le64_to_cpu(dinode->inode_size);
    inode->i_atime.tv_sec = (signed)le64_to_cpu(dinode->inode_atime);
    inode->i_ctime.tv_sec = (signed)le64_to_cpu(dinode->inode_ctime);
    inode->i_mtime.tv_sec = (signed)le64_to_cpu(dinode->inode_mtime);
    inode->i_atime.tv_nsec = inode->i_ctime.tv_nsec = inode->i_mtime.tv_nsec = 0;
    dinode_info->inode_dtime = le32_to_cpu(dinode->inode_dtime);
    
    // Now we can check validity.
    // Among other things, check if the inode is deleted.
    if(inode->i_nlink == 0 && (inode->i_mode == 0 || dinode_info->inode_dtime)) {
        pr_err("Dwarfs: inode is stale (deleted!) at ino: %llu\n", ino);
        return ERR_PTR(-ESTALE);
    }

    inode->i_blocks = le64_to_cpu(dinode->inode_blocks);
    dinode_info->inode_flags = le64_to_cpu(dinode->inode_flags);
    // Set the flags in the inode
    dinode_info->inode_fragaddr = le64_to_cpu(dinode->inode_fragaddr);
    dinode_info->inode_fragnum = dinode->inode_fragnum;
    dinode_info->inode_fragsize = dinode->inode_fragsize;
    
    if(i_size_read(inode) < 0) {
        pr_err("Dwarfs: Couldnt read inode size: ino %llu\n", ino);
        return ERR_PTR(-EFSCORRUPTED);
    }

    dinode_info->inode_dtime = 0;
    dinode_info->inode_state = 0;
    dinode_info->inode_block_group = (ino - 1) / DWARFS_SB(inode->i_sb)->dwarfs_inodes_per_group;
    dinode_info->inode_dir_start_lookup = 0;

    for(i = 0; i < DWARFS_NUMBLOCKS; i++)
        dinode_info->inode_data[i] = dinode->inode_blocks[i];
    
    inode->i_op = &dwarfs_dir_inode_operations;
    inode->i_fop = &dwarfs_dir_operations;
    inode->i_mapping->a_ops = &dwarfs_aops;

    brelse(bh);
    unlock_new_inode(inode);
    return inode;
}

static int dwarfs_get_block(struct inode *inode, sector_t iblock, struct buffer_head *bh_result, int create)
{
	map_bh(bh_result, inode->i_sb, iblock + DWARFS_INODE(inode)->inode_data[0]); /* !!!!TODO: Use more than data block 0! */
	return 0;
}

static int dwarfs_readpage(struct file *file, struct page *page) {
    return mpage_readpage(page, dwarfs_get_block);
}

static int dwarfs_readpages(struct file *file, struct address_space *mapping, struct list_head *pages, unsigned nr_pages) {
    return mpage_readpages(mapping, pages, nr_pages, dwarfs_get_block);
}

static ssize_t dwarfs_direct_io(int rw, struct kiocb *iocb, struct iovec *iter, loff_t off, unsigned long nr_segs) {
	struct inode *inode = file_inode(iocb->ki_filp);
	return blockdev_direct_IO(/*rw,*/ iocb, inode, iter, dwarfs_get_block);
}


const struct address_space_operations dwarfs_aops = {
    .readpage		= dwarfs_readpage,
	.readpages		= dwarfs_readpages,
	.direct_IO      = dwarfs_direct_io,
};
