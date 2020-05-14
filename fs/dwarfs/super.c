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

static struct kmem_cache *dwarfs_inode_cacheptr;

static void dwarfs_init_once(void *ptr) {
    struct dwarfs_inode_info *dinode_i = (struct dwarfs_inode_info *)ptr;
    printk("dwarfs: init_once: %lu\n", dinode_i->vfs_inode.i_ino);
    inode_init_once(&dinode_i->vfs_inode);
}

static int dwarfs_inode_cache_init(void) {
    dwarfs_inode_cacheptr = kmem_cache_create("dwarfs_dinode_cache", sizeof(struct dwarfs_inode_info), 0,
                            (SLAB_ACCOUNT | SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), dwarfs_init_once);
    printk("dwarfs: dwarfs_inode_cache_init\n");
    if(!dwarfs_inode_cacheptr)
        return -ENOMEM;
    return 0;
}

static void dwarfs_inode_cache_fini(void) {
    printk("dwarfs: inode_cache_fini\n");
    rcu_barrier();
    kmem_cache_destroy(dwarfs_inode_cacheptr);
}

static struct inode *dwarfs_ialloc(struct super_block *sb) {
    struct dwarfs_inode_info *dinode_i = kmem_cache_alloc(dwarfs_inode_cacheptr, GFP_KERNEL);
    printk("dwarfs: ialloc\n");
    if(!dinode_i)
        return NULL;
   // inode_set_iversion(&dinode_i->vfs_inode, 1);
    printk("Dwarfs: Allocated an inode!\n");
    return &dinode_i->vfs_inode;
}

void dwarfs_ifree(struct inode *inode) {
    printk("Dwarfs: freeing inode!\n");
    kmem_cache_free(dwarfs_inode_cacheptr, DWARFS_INODE(inode));
}

void dwarfs_superblock_sync(struct super_block *sb, struct dwarfs_superblock *dfsb, int wait) {
    printk("Dwarfs: superblock_sync\n");
    mark_buffer_dirty(DWARFS_SB(sb)->dwarfs_bufferhead);
    if(wait)
        sync_dirty_buffer(DWARFS_SB(sb)->dwarfs_bufferhead);
}

void dwarfs_write_super(struct super_block *sb) {
    struct dwarfs_superblock *dfsb = DWARFS_SB(sb)->dfsb;
    printk("Dwarfs: write_super\n");
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
    if(dfsb_i->dwarfs_inodes_per_block <= 0) {
        printk("Dwarfs: inodes per block = 0!\n");
        return -EINVAL;
    }

    sb->s_op = &dwarfs_super_operations;
    dfsb_i->dwarfs_bufferhead = bh;

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
        printk("Dwarfs: dwarfs root inode now has data!\n");
    }
    printk("Writing super\n");
    dwarfs_write_super(sb);
    printk("Returning from fill_super\n");
    return 0;
}

/* Mounts the filesystem and returns the DEntry of the root directory */
struct dentry *dwarfs_mount(struct file_system_type *type, int flags, char const *dev, void *data) {
    struct dentry *const entry = mount_bdev(type, flags, dev, data, dwarfs_fill_super);
    printk("Dwarfs: mount\n");

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
    printk("Dwarfs: init\n");

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
    printk("Dwarfs: exit\n");
    if(err != 0)
        printk("Encountered error code when unregistering DwarFS\n");
    
    dwarfs_inode_cache_fini();
}

/* Destroy the superblock when unmounting */
void dwarfs_put_super(struct super_block *sb) {
    struct dwarfs_superblock *dwarfsb = NULL;
    struct dwarfs_superblock_info *dwarfsb_i = DWARFS_SB(sb);
    printk("dwarfs_put_super\n");
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
    //    printk("Freeing dwarfsb\n");
    //    kfree(dwarfsb);
        printk("Syncing superblock\n");
        dwarfs_superblock_sync(sb, dwarfsb, 1);
    }
    sb->s_fs_info = NULL;
    printk("Freeing dwarfsb_i\n");
    kfree(dwarfsb_i);
    printk("DwarFS superblock destroyed successfully.\n");
}

struct super_operations const dwarfs_super_operations = {
    .put_super      = dwarfs_put_super,
    .alloc_inode    = dwarfs_ialloc,
    .free_inode     = dwarfs_ifree,
    .evict_inode    = dwarfs_ievict,
    .write_inode    = dwarfs_iwrite,
};

/* Let Linux know (I guess?) */
module_init(dwarfs_init);
module_exit(dwarfs_exit);
