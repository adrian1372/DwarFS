#ifndef _DWARFS_H
#define _DWARFS_H

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/uidgid.h>

static const int DWARFS_BLOCK_SIZE = 512; /* Size per block in bytes. TODO: experiment with different sizes */
static const unsigned long DWARFS_MAGIC = 0xDECAFBAD; /* Because caffeine is the only reason this is progressing at all */
static const unsigned long DWARFS_SUPERBLOCK_BLOCKNUM = 1; /* Default to 1, does this have to be dynamic??? */

typedef unsigned long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short

struct dwarfs_superblock {
    __le64 dwarfs_magic; /* Magic number */

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
};

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

static inline struct dwarfs_superblock_info *DWARFS_SB(struct super_block *sb)
{
	return (dwarfs_superblock_info *)sb->s_fs_info;
}

/* cache.c */
//extern void fat_cache_inval_inode(struct inode *inode);
//extern int fat_get_cluster(struct inode *inode, int cluster,
//			   int *fclus, int *dclus);
//extern int fat_get_mapped_cluster(struct inode *inode, sector_t sector,
//				  sector_t last_block,
//				  unsigned long *mapped_blocks, sector_t *bmap);
//extern int fat_bmap(struct inode *inode, sector_t sector, sector_t *phys,
//		    unsigned long *mapped_blocks, int create, bool from_bmap);

/* fat/dir.c */
//extern const struct file_operations fat_dir_operations;
//extern int fat_search_long(struct inode *inode, const unsigned char *name,
//			   int name_len, struct fat_slot_info *sinfo);
//extern int fat_dir_empty(struct inode *dir);
//extern int fat_subdirs(struct inode *dir);
//extern int fat_scan(struct inode *dir, const unsigned char *name,
//		    struct fat_slot_info *sinfo);
//extern int fat_scan_logstart(struct inode *dir, int i_logstart,
//			     struct fat_slot_info *sinfo);
//extern int fat_get_dotdot_entry(struct inode *dir, struct buffer_head **bh,
//				struct msdos_dir_entry **de);
//extern int fat_alloc_new_dir(struct inode *dir, struct timespec64 *ts);
//extern int fat_add_entries(struct inode *dir, void *slots, int nr_slots,
//			   struct fat_slot_info *sinfo);
//extern int fat_remove_entries(struct inode *dir, struct fat_slot_info *sinfo);

/* fat/file.c */
//extern long fat_generic_ioctl(struct file *filp, unsigned int cmd,
//			      unsigned long arg);

extern const struct file_operations dwarfs_file_operations;
extern const struct inode_operations dwarfs_file_inode_operations;

//extern int fat_setattr(struct dentry *dentry, struct iattr *attr);
//extern void fat_truncate_blocks(struct inode *inode, loff_t offset);
//extern int fat_getattr(const struct path *path, struct kstat *stat,
//		       u32 request_mask, unsigned int flags);
//extern int fat_file_fsync(struct file *file, loff_t start, loff_t end,
//			  int datasync);

/* fat/inode.c */
//extern int fat_block_truncate_page(struct inode *inode, loff_t from);
//extern void fat_attach(struct inode *inode, loff_t i_pos);
//extern void fat_detach(struct inode *inode);
//extern struct inode *fat_iget(struct super_block *sb, loff_t i_pos);
//extern struct inode *fat_build_inode(struct super_block *sb,
//			struct msdos_dir_entry *de, loff_t i_pos);
//extern int fat_sync_inode(struct inode *inode);
//extern int fat_fill_super(struct super_block *sb, void *data, int silent,
//			  int isvfat, void (*setup)(struct super_block *));
//extern int fat_fill_inode(struct inode *inode, struct msdos_dir_entry *de);

//extern int fat_flush_inodes(struct super_block *sb, struct inode *i1,
//			    struct inode *i2);
//static inline unsigned long fat_dir_hash(int logstart)
//{
//	return hash_32(logstart, FAT_HASH_BITS);
//}
//extern int fat_add_cluster(struct inode *inode);

/* super.c */
static struct file_system_type dwarfs_type;
static int dwarfs_generate_sb(struct super_block *sb, void *data, int somenum);
static struct dentry *dwarfs_mount(struct file_system_type *type, int flags, char const *dev, void *data);
static const struct super_operations dwarfs_super_operations;
static void dwarfs_put_super(struct super_block *sb);

#endif
