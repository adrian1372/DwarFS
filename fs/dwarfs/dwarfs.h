#ifndef _DWARFS_H
#define _DWARFS_H

#include <linux/fs.h>
#include <linux/types.h>

static const unsigned long DWARFS_MAGIC = 0x1DEADFAD;

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
extern const struct file_system_type dwarfs_type;
static int dwarfs_generate_sb(struct super_block *sb, void *data, int somenum);
static struct dentry *dwarfs_mount(struct file_system_type *type, int flags, char const *dev, void *data);
static const struct super_operations dwarfs_super_operations;
static void dwarfs_put_super(struct super_block *sb);

#endif
