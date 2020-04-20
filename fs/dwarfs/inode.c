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
        pr_err("Dwarfs: bad inode number %llu in dwarfs_getinode\n", ino);
        return ERR_PTR(-EINVAL);
    }
    offset = ino * DWARFS_SB(sb)->dwarfs_inodesize;
    block = DWARFS_FIRST_INODE_BLOCK + ((ino * DWARFS_SB(sb)->dwarfs_inodesize) / DWARFS_BLOCK_SIZE); // Assumption: integer division rounds down

    if(!(bh = sb_bread(sb, block))) {
        pr_err("Dwarfs: Error encountered during I/O in dwarfs_getdinode for ino %llu. Possibly bad block: %llu\n", ino, block);
        return ERR_PTR(-EIO);
    }
    *bhptr = bh;
    offset = ino % (DWARFS_BLOCK_SIZE / DWARFS_SB(sb)->dwarfs_inodesize);
    return (struct dwarfs_inode *)(bh->b_data + offset);

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
