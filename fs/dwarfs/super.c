#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/ktime.h>

#include "dwarfs.h"

/* Register FS module information */
MODULE_AUTHOR("Adrian S. Almenningen");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DwarFS filesystem for bachelor project Computer Science @ VU Amsterdam 2020");

/* Mounts the filesystem and returns the DEntry of the root directory */
static struct dentry *dwarfs_mount(struct file_system_type *type, int flags, char const *dev, void *data) {
    struct dentry *const entry = mount_bdev(type, flags, dev, data, dwarfs_generate_sb);

    if(IS_ERR(entry)) pr_err("Failed to mount DwarFS\n");
    else pr_debug("DwarFS mounted successfully\n");
    return entry; // root DEntry
}

/* Generate the Superblock when mounting the filesystem */
static int dwarfs_generate_sb(struct super_block *sb, void *data, int silent) {
    struct inode *root = NULL;
    struct timespec64 ts;

    /* Add the magic number and available super operations */
    sb->s_magic = DWARFS_MAGIC;
    sb->s_op = &dwarfs_super_operations;

    /* Create the Root inode */
    root = new_inode(sb);
    if(!root) {
        pr_err("Failed to allocate root iNode!\n");
        return -ENOMEM;
    }

    ktime_get_ts64(&ts);

    /* Define inode data. Currently using fictive data, as writing isn't implemented */
    root->i_ino = 0;
    root->i_sb = sb;
    root->i_atime = root->i_mtime = root->i_ctime = ts;
    inode_init_owner(root, NULL, S_IFDIR);

    /* Make the root DEntry for the superblock */
    sb->s_root = d_make_root(root);
    if(!sb->s_root) {
        pr_err("Failed to create Root!\n");
        return -ENOMEM;
    }
    
    return 0;
}

/* General DwarFS info */
static struct file_system_type dwarfs_type = {
    .owner      = THIS_MODULE,
    .name       = "dwarfs",
    .mount      = dwarfs_mount,
    .kill_sb    = kill_block_super,
};

/* Initialise the filesystem */
static int __init dwarfs_init(void) {
    int err = register_filesystem(&dwarfs_type);
    if(err != 0)
        pr_err("Encountered error code when registering DwarFS\n");
    return err;
}

/* Disassemble the filesystem */
static void __exit dwarfs_exit(void) {
    int err = unregister_filesystem(&dwarfs_type);
    if(err != 0)
        pr_err("Encountered error code when unregistering DwarFS\n");
}

// TODO: actually make this useful
/* Need to figure out what this is really meant to do */
static void dwarfs_put_super(struct super_block *sb) {
    pr_debug("Ayy Lmao super block destroyed\n");
}

static struct super_operations const dwarfs_super_operations = { .put_super = dwarfs_put_super, };

/* Let Linux know (I guess?) */
module_init(dwarfs_init);
module_exit(dwarfs_exit);
