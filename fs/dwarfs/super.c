#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/dax.h>
#include <linux/uidgid.h>
#include <linux/limits.h>

#include "dwarfs.h"

/* Register FS module information */
MODULE_AUTHOR("Adrian S. Almenningen");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DwarFS filesystem for bachelor project Computer Science @ VU Amsterdam 2020");

/* Read the superblock */
/*static struct dwarfs_superblock *dwarfs_read_superblock(struct super_block *sb) {
    struct dwarfs_superblock_info *dwarfsb = kzalloc(sizeof(struct dwarfs_superblock_info), GFP_NOFS); // Allocate memory for sb, GFP_NOFS blocks FS activity while allocating
    struct buffer_head *bh;
    struct dwarfs_superblock *ddsb;

    if(!dwarfsb) {
        printk("DwarFS failed to allocate superblock\n");
        return NULL;
    }
    bh = sb_bread(sb, 0);
    if(!bh) {
        printk("Couldn't read the superblock\n");
        kfree(dwarfsb);
        return NULL;
    }
    ddsb = (struct dwarfs_superblock *)bh->b_data;
    // Write to dwarfsb
    brelse(bh);
    if(ddsb->dwarfs_magic != DWARFS_MAGIC) {
        printk("Dwarfs got wrong magic number: 0x%x, expected: 0x%lx\n", ddsb->dwarfs_magic, DWARFS_MAGIC);
        kfree(dwarfsb);
        return NULL;
    }

    // DEBUG, remove in final version!!!!!
    printk("Dwarfs superblock:\n"
                "\tmagicnum        = 0x%x\n"
                "\tinode blocks    = %llu\n"
                "\tinode count     = %llu\n"
                "\treserved_blocks = %llu\n"
                "\tblocksize       = %llu\n"
                "\troot inode      = %llu\n",
                ddsb->dwarfs_magic, ddsb->dwarfs_blockc, ddsb->dwarfs_inodec,
                ddsb->dwarfs_reserved_blocks, ddsb->dwarfs_block_size, ddsb->dwarfs_root_inode);

    return ddsb;

}
*/


void dwarfs_superblock_sync(struct super_block *sb, struct dwarfs_superblock *dfsb, int wait) {
    mark_buffer_dirty(DWARFS_SB(sb)->dwarfs_bufferhead);
    if(wait)
        sync_dirty_buffer(DWARFS_SB(sb)->dwarfs_bufferhead);
}

void dwarfs_write_super(struct super_block *sb) {
    struct dwarfs_superblock *dfsb = DWARFS_SB(sb)->dfsb;
    dwarfs_superblock_sync(sb, dfsb, 1);
}

/* Generate the Superblock when mounting the filesystem */
int dwarfs_fill_super(struct super_block *sb, void *data, int silent) {

    struct dax_device *dax;
    struct inode *root;
    struct buffer_head *bh;
    struct dwarfs_superblock *dfsb;
    struct dwarfs_superblock_info *dfsb_i;
    uint64_t logical_sb_blocknum;
    uint64_t offset = 0;
    unsigned long blocksize;

    dax = fs_dax_get_by_bdev(sb->s_bdev);
    dfsb_i = kzalloc(sizeof(struct dwarfs_superblock_info), GFP_KERNEL);
    if(!dfsb_i) {
        printk("DwarFS failed to allocate superblock information structure!\n");
        return -ENOMEM;
    }

    sb->s_fs_info = dfsb_i;
    dfsb_i->dwarfs_sb_blocknum = DWARFS_SUPERBLOCK_BLOCKNUM;
    dfsb_i->dwarfs_dax_device = dax;

    /* 
     * Making sure that the physical disk's block size isn't
     * larger than the filesystem's defined block size.
     * If so, use the disk's blocksize. Otherwise, default.
     */
    blocksize = sb_min_blocksize(sb, DWARFS_BLOCK_SIZE);
    if(!blocksize) {
        printk("Dwarfs failed to set blocksize!\n");
        return -EINVAL;
    }

    /* If the blocksize isn't the same as default, calculate offset. */
    if(blocksize != DWARFS_BLOCK_SIZE) {
        logical_sb_blocknum = (DWARFS_SUPERBLOCK_BLOCKNUM * DWARFS_BLOCK_SIZE) / blocksize;
        offset = (DWARFS_SUPERBLOCK_BLOCKNUM * DWARFS_BLOCK_SIZE) % blocksize;
    } else logical_sb_blocknum = DWARFS_SUPERBLOCK_BLOCKNUM;

    bh = sb_bread(sb, logical_sb_blocknum);
    if(!bh) {
        printk("Dwarfs failed to read superblock!\n");
        return -EINVAL;
    }
    dfsb = (struct dwarfs_superblock*)(((char*)bh->b_data) + offset);
    dfsb_i->dfsb = dfsb;
    sb->s_magic = le64_to_cpu(dfsb->dwarfs_magic);

    if(sb->s_magic != DWARFS_MAGIC) {
        printk("Dwarfs got wrong magic number: 0x%lx, expected: 0x%lx\n", sb->s_magic, DWARFS_MAGIC);
        return -EINVAL;
    }
    else printk("Dwarfs got correct magicnum: 0x%lx\n", sb->s_magic);

    if(sb->s_blocksize != blocksize) {
        printk("Dwarfs blocksize mismatch: %lu vs %lu\n", sb->s_blocksize, blocksize);
    }

    dfsb_i->dwarfs_resgid = make_kgid(&init_user_ns, le16_to_cpu(dfsb->dwarfs_def_resgid));
    dfsb_i->dwarfs_resuid = make_kuid(&init_user_ns, le16_to_cpu(dfsb->dwarfs_def_resuid));

    sb->s_maxbytes = 512; /* TODO: Make this a defined const or dynamic */
    sb->s_max_links = 512; /* TODO: Make defined const or dynamic */

    dfsb_i->dwarfs_inodesize = 256;
    dfsb_i->dwarfs_first_inum = DWARFS_FIRST_INODE;
    dfsb_i->dwarfs_inodes_per_block = sb->s_blocksize / dfsb_i->dwarfs_inodesize;
    if(dfsb_i->dwarfs_inodes_per_block <= 0) {
        printk("Dwarfs: inodes per block = 0!\n");
        return -EINVAL;
    }

    dfsb_i->dwarfs_bufferhead = bh;

    root = dwarfs_inode_get(sb, DWARFS_ROOT_INUM);
    
    if(IS_ERR(root)) {
        printk("Dwarfs got error code when getting the root node!\n");
        return PTR_ERR(root);
    }
    if(!S_ISDIR(root->i_mode) || !root->i_blocks || !root->i_size) {
        iput(root);
        printk("Dwarfs: Root node corrupt!\n");
        return -EINVAL;
    }

    sb->s_root = d_make_root(root);
    if(!sb->s_root) {
        printk("Dwarfs: Couldn't get root inode!\n");
        return -ENOMEM;
    }
    dwarfs_write_super(sb);
    return 0;
}

/* Mounts the filesystem and returns the DEntry of the root directory */
struct dentry *dwarfs_mount(struct file_system_type *type, int flags, char const *dev, void *data) {
    struct dentry *const entry = mount_bdev(type, flags, dev, data, dwarfs_fill_super);

    if(IS_ERR(entry)) printk("Failed to mount DwarFS\n");
    else printk("DwarFS mounted successfully\n");
    return entry; // root DEntry
}

/* General DwarFS info */
struct file_system_type dwarfs_type = {
    .owner      = THIS_MODULE,
    .name       = "dwarfs",
    .mount      = dwarfs_mount,
    .kill_sb    = kill_block_super,
};

/* Initialise the filesystem */
static int __init dwarfs_init(void) {
    int err = register_filesystem(&dwarfs_type);
    if(err != 0)
        printk("Encountered error code when registering DwarFS\n");
    return err;
}

/* Disassemble the filesystem */
static void __exit dwarfs_exit(void) {
    int err = unregister_filesystem(&dwarfs_type);
    if(err != 0)
        printk("Encountered error code when unregistering DwarFS\n");
}

/* Destroy the superblock when unmounting */
void dwarfs_put_super(struct super_block *sb) {
    struct dwarfs_superblock *dwarfsb = DWARFS_SB(sb)->dfsb;
    if(dwarfsb)
        kfree(dwarfsb);
    sb->s_fs_info = NULL;
    printk("DwarFS superblock destroyed successfully.\n");
}

struct super_operations const dwarfs_super_operations = { .put_super = dwarfs_put_super, };

/* Let Linux know (I guess?) */
module_init(dwarfs_init);
module_exit(dwarfs_exit);
