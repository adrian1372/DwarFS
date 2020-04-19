#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>


static struct dentry *dwarfs_lookup(struct inode *dir, struct dentry *dentry, unsigned flags)
{
	return NULL;
}

const struct inode_operations dwarfs_dir_inode_operations = {
    .lookup     = dwarfs_lookup,
};

const struct file_operations dwarfs_dir_operations = {
    .llseek     = generic_file_llseek,
    .read       = generic_read_dir,
  //  .iterate    = generic_read_dir,
};