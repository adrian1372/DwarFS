#ifndef _DWARFS_H
#define _DWARFS_H

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/uidgid.h>

#define EFSCORRUPTED EUCLEAN

#define DWARFS_SUPERBLOCK_PADDING 376 // 512 - sizeof(dwarfs_superblock)
static const int DWARFS_BLOCK_SIZE = 512; /* Size per block in bytes. TODO: experiment with different sizes */

/*
 * Superblock code
 */

static const unsigned long DWARFS_MAGIC = 0xDECAFBAD; /* Because copious amounts of caffeine is the only reason this is progressing at all */
static const unsigned long DWARFS_SUPERBLOCK_BLOCKNUM = 0; /* Default to 0, does this have to be dynamic??? */
static const uint64_t DWARFS_FIRST_INODE_BLOCK = 3;

extern struct file_system_type dwarfs_type;
extern int dwarfs_fill_super(struct super_block *sb, void *data, int silent);
extern struct dentry *dwarfs_mount(struct file_system_type *type, int flags, char const *dev, void *data);
extern const struct super_operations dwarfs_super_operations;
extern void dwarfs_put_super(struct super_block *sb);

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
    __le64 dwarfs_os; /* Which OS created the fs (Can probably be deleted, no one outside Linux would use this) */

    /* Add padding to fill the block? */
    // Answer is a resounding yes! Need to fill the block
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
#define DWARFS_NEW_INODE 1

extern struct inode *dwarfs_inode_get(struct super_block *sb, uint64_t ino);
extern struct dwarfs_inode *dwarfs_getdinode(struct super_block *sb, uint64_t ino, struct buffer_head **bhptr);

extern const struct inode_operations dwarfs_file_inode_operations;
#define DWARFS_NUMBLOCKS 15 /* Subject to change */
#define DWARFS_INODE_PADDING 24 /* Subject to change as inode size and blocksize changes */
#define DWARFS_ROOT_INUM 2
#define DWARFS_FIRST_INODE DWARFS_ROOT_INUM+1 // First unreserved inode
/* Disk inode */
struct dwarfs_inode {
    __le16 inode_mode; /* Dir, file, etc. */
    __le64 inode_size; /* Size of the iNode */
    
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

    __le64 inode_reserved1; /* Not sure what this is, gotten from ext2. REMOVE IF NOT USED */
    __le64 inode_blocks[DWARFS_NUMBLOCKS]; /* Pointers to data blocks */
    __le64 inode_fragaddr; /* Fragment address */

    /* Linux thingies. Are these actually needed? Maybe remove! */
    uint8_t inode_fragnum; /* Fragment number */
    __le16 inode_fragsize; /* Fragment size */
    __le16 inode_padding1; /* Some padding */

    /* Not sure what any of these are for. Gotten from EXT2. */
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

    uint64_t inode_dir_start_lookup;
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

struct dwarfs_directory_entry {
    __le64 inode; /* inum */
    __le64 entrylen; /* length of the entry */
    uint8_t namelen; /* Length of the name */
    uint8_t filetype; /* Filetype (directory, normal, etc.) */
    char filename[]; /* File name */
};

//static struct dentry *dwarfs_lookup(struct inode *dir, struct dentry *dentry, unsigned flags);

/* Operations */

/* File */
extern const struct file_operations dwarfs_file_operations;

/* inode */
extern const struct inode_operations dwarfs_file_inode_operations;
extern const struct inode_operations dwarfs_dir_inode_operations;
extern const struct address_space_operations dwarfs_aops;

/* Directory */
extern const struct file_operations dwarfs_dir_operations;

#endif
