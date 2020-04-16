#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

const struct inode_operations dwarfs_file_inode_operations {
    .setattr        = generic_setattr,
    .getattr        = generic_getattr,
    .update_time    = generic_update_time,
};

/*

struct inode *dwarfs_inode_get(struct super_block *sb, uint64_t ino) {
    struct inode *inode;
    struct dwarfs_inode *dinode;
    struct dwarfs_inode_info *dinode_info;
    struct buffer_head *bh;

    uid_t uid;
    gid_t gid;

    inode = iget_locked(sb, ino);
    if(!inode)
        pr_err("Dwarfs: Failed to get inode in iget!\n");
        return ERR_PTR(-ENOMEM);
    if(!(inode->i_state) & I_NEW)) // inode already exists, nothing more to do
        return inode;
    
    dinode_info = DWARFS_INODE(inode);
    dinode = 
    
    return NULL;
}
*/