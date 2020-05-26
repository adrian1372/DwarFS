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
#include <linux/quotaops.h>
#include <linux/statfs.h>

#include "dwarfs.h"

/* Register FS module information */
MODULE_AUTHOR("Adrian S. Almenningen");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DwarFS filesystem for bachelor project Computer Science @ VU Amsterdam 2020");

static struct kmem_cache *dwarfs_inode_cacheptr;

static void dwarfs_init_once(void *ptr) {
    struct dwarfs_inode_info *dinode_i = (struct dwarfs_inode_info *)ptr;
    inode_init_once(&dinode_i->vfs_inode);
}

static int dwarfs_inode_cache_init(void) {
    dwarfs_inode_cacheptr = kmem_cache_create("dwarfs_dinode_cache", sizeof(struct dwarfs_inode_info), 0,
                            (SLAB_ACCOUNT | SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), dwarfs_init_once);
    if(!dwarfs_inode_cacheptr)
        return -ENOMEM;
    return 0;
}

static void dwarfs_inode_cache_fini(void) {
    rcu_barrier();
    kmem_cache_destroy(dwarfs_inode_cacheptr);
}

static struct inode *dwarfs_ialloc(struct super_block *sb) {
    struct dwarfs_inode_info *dinode_i = kmem_cache_alloc(dwarfs_inode_cacheptr, GFP_KERNEL);
    if(!dinode_i)
        return NULL;
    return &dinode_i->vfs_inode;
}

void dwarfs_ifree(struct inode *inode) {
    kmem_cache_free(dwarfs_inode_cacheptr, DWARFS_INODE(inode));
}

void dwarfs_superblock_sync(struct super_block *sb, struct dwarfs_superblock *dfsb, int wait) {
    struct dwarfs_superblock_info *dfsb_i = DWARFS_SB(sb);
    dfsb->dwarfs_free_blocks_count = dfsb_i->dwarfs_free_blocks_count;
    dfsb->dwarfs_free_inodes_count = dfsb_i->dwarfs_free_inodes_count;
    mark_buffer_dirty(dfsb_i->dwarfs_bufferhead);
    if(wait)
        sync_dirty_buffer(dfsb_i->dwarfs_bufferhead);
}

static int dwarfs_sync_fs(struct super_block *sb, int wait) {
    dquot_writeback_dquots(sb, -1);
    dwarfs_superblock_sync(sb, DWARFS_SB(sb)->dfsb, wait);
    return 0;
}

void dwarfs_write_super(struct super_block *sb) {
    struct dwarfs_superblock *dfsb = DWARFS_SB(sb)->dfsb;
    dwarfs_superblock_sync(sb, dfsb, 1);
}

/* Generate the Superblock when mounting the filesystem */
int dwarfs_fill_super(struct super_block *sb, void *data, int silent) {

    struct inode *root = NULL;
    struct buffer_head *bh = NULL;
    struct dwarfs_superblock *dfsb = NULL;
    struct dwarfs_superblock_info *dfsb_i = NULL;
    uint64_t logical_sb_blocknum;
    uint64_t offset = 0;
    unsigned long blocksize;
    printk("Dwarfs: fill_super\n");

    dfsb_i = kzalloc(sizeof(struct dwarfs_superblock_info), GFP_KERNEL);
    if(!dfsb_i) {
        printk("DwarFS failed to allocate superblock information structure!\n");
        return -ENOMEM;
    }

    sb->s_fs_info = dfsb_i;
    dfsb_i->dwarfs_sb_blocknum = DWARFS_SUPERBLOCK_BLOCKNUM;

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
    printk("Dwarfs: BLOCKSIZE: %lu\n", blocksize);

    /* If the blocksize isn't the same as default, calculate offset. */
    if(blocksize != DWARFS_BLOCK_SIZE) {
        logical_sb_blocknum = (DWARFS_SUPERBLOCK_BLOCKNUM * DWARFS_BLOCK_SIZE) / blocksize;
        offset = (DWARFS_SUPERBLOCK_BLOCKNUM * DWARFS_BLOCK_SIZE) % blocksize;
    } else logical_sb_blocknum = DWARFS_SUPERBLOCK_BLOCKNUM;

    bh = sb_bread(sb, logical_sb_blocknum);
    printk("Dwarfs: SB_BLOCK: %llu. Blocksize: %lu\n", logical_sb_blocknum, sb->s_blocksize);
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

    sb->s_maxbytes = 4294967296; // 4 GB max filesize
    sb->s_max_links = 512;

    dfsb_i->dwarfs_inodesize = sizeof(struct dwarfs_inode);
    dfsb_i->dwarfs_first_inum = DWARFS_FIRST_INODE;
    dfsb_i->dwarfs_inodes_per_block = sb->s_blocksize / dfsb_i->dwarfs_inodesize;
    dfsb_i->dwarfs_free_inodes_count = dfsb->dwarfs_free_inodes_count;
    dfsb_i->dwarfs_free_blocks_count = dfsb->dwarfs_free_blocks_count;
    if(dfsb_i->dwarfs_inodes_per_block <= 0) {
        printk("Dwarfs: inodes per block = 0!\n");
        return -EINVAL;
    }

    sb->s_op = &dwarfs_super_operations;
    dfsb_i->dwarfs_bufferhead = bh;

    mutex_init(&dfsb_i->dwarfs_bitmap_lock);

    root = dwarfs_inode_get(sb, DWARFS_ROOT_INUM);
    
    if(IS_ERR(root)) {
        printk("Dwarfs got error code when getting the root node!\n");
        return PTR_ERR(root);
    }
    if(!S_ISDIR(root->i_mode) /* || !root->i_blocks || !root->i_size */) {
        if(!S_ISDIR(root->i_mode)) printk("Not a DIR!\n");
        if(!root->i_blocks) printk("No blocks detected!\n");
        if(!root->i_size) printk("Size = 0!\n");

        iput(root);
        printk("Dwarfs: Root node corrupt!\n");
        return -EINVAL;
    }

    printk("Making root\n");
    sb->s_root = d_make_root(root);
    if(!sb->s_root) {
        printk("Dwarfs: Couldn't get root inode!\n");
        return -ENOMEM;
    }
    printk("Checking if data block 0 of root inode exists\n");
    if(!dwarfs_rootdata_exists(sb, root)) {
        printk("Creating block 0 of root inode\n");
        dwarfs_make_empty_dir(root, root);
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
    int err;

    err = dwarfs_inode_cache_init();
    if(err != 0) {
        printk("Dwarfs: failed to initialise inode cache!\n");
        return err;
    }
    err = register_filesystem(&dwarfs_type);
    if(err != 0) {
        dwarfs_inode_cache_fini();
        printk("Encountered error code when registering DwarFS\n");
    }
    return err;
}

/* Disassemble the filesystem */
static void __exit dwarfs_exit(void) {
    int err = unregister_filesystem(&dwarfs_type);
    if(err != 0)
        printk("Encountered error code when unregistering DwarFS\n");
    
    dwarfs_inode_cache_fini();
}

/* Destroy the superblock when unmounting */
void dwarfs_put_super(struct super_block *sb) {
    struct dwarfs_superblock *dwarfsb = NULL;
    struct dwarfs_superblock_info *dwarfsb_i = DWARFS_SB(sb);
    if(!sb) {
        printk("superblock is already destroyed!\n");
        return;
    }
    if(!dwarfsb_i) {
        printk("s_fs_info is NULL!\n");
        return;
    }	
    dwarfsb = dwarfsb_i->dfsb;
    if(dwarfsb) {
        dwarfs_superblock_sync(sb, dwarfsb, 1);
    }
    sb->s_fs_info = NULL;
    kfree(dwarfsb_i);
    printk("DwarFS superblock destroyed successfully.\n");
}

static int dwarfs_statfs(struct dentry *dentry, struct kstatfs *stat) {
    struct super_block *sb = dentry->d_sb;
    struct dwarfs_superblock_info *dfsb_i = DWARFS_SB(sb);
    struct dwarfs_superblock *dfsb = dfsb_i->dfsb;

    stat->f_type = DWARFS_MAGIC;
    stat->f_bsize = sb->s_blocksize;
    stat->f_blocks = dfsb->dwarfs_blockc;
    stat->f_files = dfsb->dwarfs_inodec;
    stat->f_namelen = DWARFS_MAX_FILENAME_LEN; // or is this FS namelen?

    // Quick little hack to make sure stuff's up-to-date here
    mutex_lock_interruptible(&dfsb_i->dwarfs_bitmap_lock);
    stat->f_bfree = dfsb_i->dwarfs_free_blocks_count;
    dfsb->dwarfs_free_blocks_count = dfsb_i->dwarfs_free_blocks_count;
    stat->f_ffree = dfsb_i->dwarfs_free_inodes_count;
    dfsb->dwarfs_free_inodes_count = dfsb_i->dwarfs_free_inodes_count;
    mutex_unlock(&dfsb_i->dwarfs_bitmap_lock);
    stat->f_bavail = stat->f_bfree;

    /* Seems like even the guys writing the manual pages don't know wtf f_fsid is supposed to be, so ignoring.... */

    return 0;
}

struct super_operations const dwarfs_super_operations = {
    .put_super      = dwarfs_put_super,
    .alloc_inode    = dwarfs_ialloc,
    .free_inode     = dwarfs_ifree,
    .evict_inode    = dwarfs_ievict,
    .write_inode    = dwarfs_iwrite,
    .sync_fs        = dwarfs_sync_fs,
    .statfs         = dwarfs_statfs,
};

/* Let Linux know (I guess?) */
module_init(dwarfs_init);
module_exit(dwarfs_exit);
