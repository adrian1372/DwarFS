#ifndef _DWARFS_H
#define _DWARFS_H

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/uidgid.h>
#include <linux/buffer_head.h>

#define EFSCORRUPTED EUCLEAN
#define EEXISTS 17 // Couldn't figure out where this is defined

#define DWARFS_SUPERBLOCK_PADDING 3960 // 4096 - sizeof(dwarfs_superblock)
static const int DWARFS_BLOCK_SIZE = 4096; /* Size per block in bytes. TODO: experiment with different sizes */

/*
 * Superblock code
 */

static const unsigned long DWARFS_MAGIC = 0xDECAFBAD; /* Because copious amounts of caffeine is the only reason this is progressing at all */
static const unsigned long DWARFS_SUPERBLOCK_BLOCKNUM = 0; /* Default to 0, find out if this should be dynamic! */
static const uint64_t DWARFS_INODE_BITMAP_BLOCK = 1;
static const uint64_t DWARFS_DATA_BITMAP_BLOCK = 2;
static const uint64_t DWARFS_FIRST_INODE_BLOCK = 3; // sb -> i_bitmap -> d_bitmap -> inodes

extern struct file_system_type dwarfs_type;
extern const struct super_operations dwarfs_super_operations;


/* Actual DwarFS superblock */
struct dwarfs_superblock {
    __le32 dwarfs_magic; /* Magic number */

    /* FS structure and storage information */
    __le64 dwarfs_blockc; /* number of blocks */
    __le64 dwarfs_reserved_blocks; /* number of reserved blocks */
    __le64 dwarfs_free_blocks_count; /* Number of free blocks in the volume */
    __le64 dwarfs_free_inodes_count; /* Number of free inodes in the volume (let's aim to never let this be zero) */
    __le64 dwarfs_data_start_block; /* Block at which data storage starts */
    __le64 dwarfs_block_size; /* Size of disk blocks */
    __le64 dwarfs_root_inode; /* root inode */
    __le64 dwarfs_inodec; /* Number of inodes */
    __le64 dwarfs_blocks_per_group; /* Number of blocks in a disk group */
    __le64 dwarfs_frags_per_group; /* Number of fragments in a disk group */
    __le64 dwarfs_inodes_per_group; /* Number of inodes in a disk group */

    /* Time data */
    __le64 dwarfs_wtime; /* Time of last write */
    __le64 dwarfs_mtime; /* Time of last mount */

    /* General FS & volume information */
    __le16 dwarfs_def_resuid; /* Default user ID for reserved blocks */
    __le16 dwarfs_def_resgid; /* Default group ID for reserved blocks */
    __le64 dwarfs_version_num; /* Versions might not matter ... backwards/forwards compatibility???? */
    __le64 dwarfs_os; /* Which OS created the fs */

    char padding[DWARFS_SUPERBLOCK_PADDING];
};

/* DwarFS superblock in memory */
struct dwarfs_superblock_info {
    uint64_t dwarfs_fragsize; /* Size of fragmentations in bytes */
    uint64_t dwarfs_inodes_per_block; /* inodes per block */
    uint64_t dwarfs_frags_per_group; /* Fragmentations per group */
    uint64_t dwarfs_blocks_per_group; /* Blocks per group */
    uint64_t dwarfs_inodes_per_group; /* iNodes per FS group */
    uint64_t dwarfs_inode_tableblocks_per_group; /* iNode table blocks per FS group */
    uint64_t dwarfs_grpdescriptor_count; /* Total number of group descriptors */
    uint64_t dwarfs_descriptor_per_block; /* Group descriptors per block */
    uint64_t dwarfs_groupc; /* Number of groups in FS */
    uint64_t dwarfs_sb_blocknum; /* block number of the superblock */

    struct buffer_head *dwarfs_bufferhead; /* Buffer Head containing the superblock */
    struct dwarfs_superblock *dfsb; /* Super block in the buffer */
    struct buffer_head **dwarfs_group_desc; /* Group descriptors */
    struct dax_device *dwarfs_dax_device; /* Direct Access Device */

    uint32_t dwarfs_inodesize; /* Size of inodes */
    uint64_t dwarfs_first_inum; /* First inode number (ID) */
    uint64_t dwarfs_dir_count; /* Number of directories */
    kgid_t dwarfs_resgid; /* GID of reserved blocks */
    kuid_t dwarfs_resuid; /* UID of reserved blocks */

};

static inline struct dwarfs_superblock_info *DWARFS_SB(struct super_block *sb) {
	return (struct dwarfs_superblock_info *)sb->s_fs_info;
}

/*
 * iNode code
 */


#define NO_FLAG 0x0 // 00000000 No special flags.
#define I_RES1 0x01 // 00000001
#define I_RES2 0x02 // 00000010
#define I_RES3 0x04 // 00000100
#define I_RES4 0x08 // 00001000
#define I_RES5 0x10 // 00010000
#define I_RES6 0x20 // 00100000
#define I_RES7 0x40 // 01000000
#define I_RES8 0x80 // 10000000

// Inode states
#define DWARFS_INODE_NEW 1

extern const struct inode_operations dwarfs_file_inode_operations;

#define DWARFS_NUMBLOCKS 15 /* Total block ptrs in an inode */
#define DWARFS_INODE_INDIR DWARFS_NUMBLOCKS-1

#define DWARFS_INODE_PADDING 24 /* Subject to change as inode size and blocksize changes */
#define DWARFS_ROOT_INUM 2
#define DWARFS_FIRST_INODE DWARFS_ROOT_INUM+1 // First unreserved inode
/* Disk inode */
struct dwarfs_inode {
    __le16 inode_mode; /* Filetype and access bits */
    __le64 inode_size; /* Size of the iNode... might be filesize */
    
    /* Owner */
    __le16 inode_uid; /* Owner user ID */
    __le16 inode_gid; /* iNode Group ID */

    /* Time Management */
    __le64 inode_atime; /* Time last accessed */
    __le64 inode_ctime; /* Time of creation */
    __le64 inode_mtime; /* Time of last modification */
    __le64 inode_dtime; /* Time deleted */

    /* Disk info */
    __le64 inode_blockc; /* Number of used blocks */
    __le64 inode_linkc; /* Number of links */
    __le64 inode_flags; /* File flags (Remove this if no flags get implemented!) */
    
    __le64 inode_reserved1;
    __le64 inode_blocks[DWARFS_NUMBLOCKS]; /* Pointers to data blocks */

    /*
     * The following can be replaced with padding in the future
     * unless removing these and padding makes the inode a smaller
     * blocksize-divisible size
     */
    __le64 inode_fragaddr;
    uint8_t inode_fragnum; /* Fragment number */
    __le16 inode_fragsize; /* Fragment size */
    __le16 inode_padding1; /* Some padding */
    __le16 inode_uid_high;
    __le16 inode_gid_high;
    __le32 inode_reserved2;

    uint8_t padding[DWARFS_INODE_PADDING];
};

/* Memory inode */
struct dwarfs_inode_info {
    uint32_t inode_fragaddr;
    uint8_t inode_fragnum;
    uint8_t inode_fragsize;
    uint64_t inode_dtime;
    uint64_t inode_block_group;
    uint64_t inode_state;
    uint64_t inode_flags;

    __le64 inode_data[DWARFS_NUMBLOCKS];

    int64_t inode_dir_start_lookup;
    struct inode vfs_inode;
};

static inline struct dwarfs_inode_info *DWARFS_INODE(struct inode *inode) {
    return container_of(inode, struct dwarfs_inode_info, vfs_inode);
}

/*
 * File code
 */

/*
 * Directory code
 */

#define DWARFS_MAX_FILENAME_LEN 24

struct dwarfs_directory_entry {
    __le64 inode; /* inum */
    __le64 entrylen; /* length of the entry */
    uint8_t namelen; /* Length of the name */
    uint8_t filetype; /* Filetype (directory, normal, etc.) */
    char filename[DWARFS_MAX_FILENAME_LEN]; /* File name */
};

//static struct dentry *dwarfs_lookup(struct inode *dir, struct dentry *dentry, unsigned flags);

/* Function declarations */
/* super.c */
extern int dwarfs_fill_super(struct super_block *sb, void *data, int silent);
extern struct dentry *dwarfs_mount(struct file_system_type *type, int flags, char const *dev, void *data);
extern void dwarfs_put_super(struct super_block *sb);
extern void dwarfs_superblock_sync(struct super_block *sb, struct dwarfs_superblock *dfsb, int wait);
extern void dwarfs_write_super(struct super_block *sb);
extern void dwarfs_ifree(struct inode *inode);

/* inode.c */
extern struct inode *dwarfs_inode_get(struct super_block *sb, int64_t ino);
extern struct dwarfs_inode *dwarfs_getdinode(struct super_block *sb, int64_t ino, struct buffer_head **bhptr);
extern uint64_t dwarfs_get_ino_by_name(struct inode *dir, const struct qstr *inode_name);
extern struct inode *dwarfs_create_inode(struct inode *dir, const struct qstr *namestr, umode_t mode);
extern struct dwarfs_inode *dwarfs_getdinode(struct super_block *sb, int64_t ino, struct buffer_head **bhptr);
extern struct inode *dwarfs_inode_get(struct super_block *sb, int64_t ino);
extern int dwarfs_get_iblock(struct inode *inode, sector_t iblock, struct buffer_head *bh_result, int create);
extern int dwarfs_link_node(struct dentry *dentry, struct inode *inode);
extern int dwarfs_sync_dinode(struct super_block *sb, struct inode *inode);
extern void dwarfs_ievict(struct inode *inode);
extern int dwarfs_iwrite(struct inode *inode, struct writeback_control *wbc);

/* dir.c */
extern int dwarfs_commit_chunk(struct page *pg, uint64_t offset, uint64_t n);
extern int dwarfs_make_empty_dir(struct inode *inode, struct inode *dir);
extern int dwarfs_getattr(const struct path *path, struct kstat *kstat, u32 req_mask, unsigned int query_flags);
extern int dwarfs_setattr(struct dentry *dentry, struct iattr *iattr);

/* alloc.c */
extern int64_t dwarfs_inode_alloc(struct super_block *sb);
extern int dwarfs_inode_dealloc(struct super_block *sb, int64_t ino);
extern int64_t dwarfs_data_alloc(struct super_block *sb, struct inode *inode);
extern int dwarfs_data_dealloc(struct super_block *sb, struct inode *inode);

/* Operations */

/* Symlink */
static const struct inode_operations dwarfs_symlink_inode_operations = {
    .setattr = dwarfs_setattr,
    .getattr = dwarfs_getattr,
    .get_link = simple_get_link,
};

static const struct inode_operations dwarfs_slow_symlink_inode_operations = {
    .setattr = dwarfs_setattr,
    .getattr = dwarfs_getattr,
    .get_link = simple_get_link,
};

/* File */
extern const struct file_operations dwarfs_file_operations;

/* inode */
extern const struct inode_operations dwarfs_file_inode_operations;
extern const struct inode_operations dwarfs_dir_inode_operations;
extern const struct address_space_operations dwarfs_aops;

/* Directory */
extern const struct file_operations dwarfs_dir_operations;
//extern int dwarfs_create_dirdata(struct super_block *sb, struct inode *inode);
extern int dwarfs_rootdata_exists(struct super_block *sb, struct inode *inode);

/* Special */
static const struct inode_operations dwarfs_special_inode_operations = {
    .getattr    = dwarfs_getattr,
    .setattr    = dwarfs_setattr,
};


/* General helper functions */
static inline void dwarfs_write_buffer(struct buffer_head **bh, struct super_block *sb) {
    printk("dwarfs_write_buffer\n");
    mark_buffer_dirty(*bh);
    if(sb->s_flags & SB_SYNCHRONOUS)
        sync_dirty_buffer(*bh);
    brelse(*bh);
}

static inline int dwarfs_divround(int divisor, int dividend) {
    return (divisor % dividend) > 0 ? (divisor / dividend)+1 : (divisor / dividend);
}

/* Function to count number of inode blocks */
static inline int dwarfs_inodeblocksnum(struct super_block *sb) {
    return dwarfs_divround(DWARFS_SB(sb)->dfsb->dwarfs_inodec * sizeof(struct dwarfs_inode), sb->s_blocksize);
}

/*
 * Function to get the block where data starts.
 * After all the inodes, every block will be reserved for file/dir data.
 */
static inline int dwarfs_datastart(struct super_block *sb) {
 return DWARFS_FIRST_INODE_BLOCK + dwarfs_inodeblocksnum(sb);
}

static inline struct buffer_head *read_inode_bitmap(struct super_block *sb) {
    return sb_bread(sb, DWARFS_INODE_BITMAP_BLOCK);
}

static inline struct buffer_head *read_data_bitmap(struct super_block *sb) {
    return sb_bread(sb, DWARFS_DATA_BITMAP_BLOCK);
}

#endif
