#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/string.h>
#include <linux/posix_acl_xattr.h>
#include <linux/quotaops.h>
#include <linux/pagemap.h>
#include <linux/iversion.h>

// TODO: Remove the printk's. Some are above variable declarations, violating C90 standards.

int dwarfs_commit_chunk(struct page *pg, uint64_t offset, uint64_t n) {
  struct address_space *map = pg->mapping;
  struct inode *dir = map->host;
  printk("Dwarfs: commit_chunk\n");

  inode_inc_iversion(dir);
  block_write_end(NULL, map, offset, n, n, pg, NULL);
  if(offset + n > dir->i_size) {
    i_size_write(dir, offset+n);
    mark_inode_dirty(dir);
  }

  unlock_page(pg);
  return 0;
}

int dwarfs_make_empty_dir(struct inode *inode, struct inode *dir) {
  struct dwarfs_directory_entry *direntry = NULL;
  struct dwarfs_inode_info *dinode_i = DWARFS_INODE(inode);
  struct buffer_head *bh = NULL;
  char *blockaddr = NULL;
  int64_t first_blocknum = dwarfs_data_alloc(dir->i_sb);

  printk("Dwarfs: make_empty_dir\n");
  if(first_blocknum < 8 || first_blocknum >= 64) {
    printk("Dwarfs: Got an invalid block number: %lld\n", first_blocknum);
    return -EIO;
  }
  printk("Dwarfs: allocating data block: %lld\n", first_blocknum);
  dinode_i->inode_data[0] = first_blocknum;
  if(!(bh =  sb_bread(dir->i_sb, dinode_i->inode_data[0]))) {
    printk("Dwarfs: Failed to grab buffer head!\n");
    return -ENOMEM;
  }

  blockaddr = bh->b_data;
  memset(blockaddr, 0, DWARFS_BLOCK_SIZE);
  direntry =(struct dwarfs_directory_entry *)blockaddr;
  direntry->namelen = 1;
  direntry->entrylen = sizeof(struct dwarfs_directory_entry);
  direntry->inode = cpu_to_le64(inode->i_ino);
  strncpy(direntry->filename, ".\0\0", 4);
  direntry->filetype = 0;

  direntry++;
  direntry->namelen = 2;
  direntry->entrylen = sizeof(struct dwarfs_directory_entry);
  direntry->inode = cpu_to_le64(dir->i_ino);
  strncpy(direntry->filename, "..\0", 4);
  direntry->filetype = 0;

  dwarfs_write_buffer(&bh, dir->i_sb);
  dwarfs_sync_dinode(dir->i_sb, inode);
  return 0;
}

static struct dentry *dwarfs_lookup(struct inode *dir, struct dentry *dentry, unsigned flags) {
  int64_t ino;
  struct inode *inode = NULL;
  printk("Dwarfs: lookup\n");

  if(dentry->d_name.len > DWARFS_MAX_FILENAME_LEN || dentry->d_name.len <= 0) {
    printk("Dwarfs: Invalid DEntry name length: %u\n", dentry->d_name.len);
    return ERR_PTR(-ENAMETOOLONG);
  }
  printk("Dwarfs: Looking up: %s\n", dentry->d_name.name);
  ino = dwarfs_get_ino_by_name(dir, &dentry->d_name);
  inode = NULL;
  if(ino) {
    inode = dwarfs_inode_get(dir->i_sb, ino);
    if(IS_ERR(inode)) {
      printk("Dwarfs: Error occured when geting inode.\n");
      return ERR_PTR(PTR_ERR(inode));
    }
  }
  else printk("dwarfs: lookup yielded no result\n");
  d_add(dentry, inode);
  return dentry;
}

int dwarfs_rootdata_exists(struct super_block *sb, struct inode *inode) {
  struct dwarfs_inode_info *dinode_i = NULL;
  struct buffer_head *bh = NULL;
  struct dwarfs_directory_entry *dirptr = NULL;

  printk("Dwarfs: rootdata_exists\n");

  dinode_i = DWARFS_INODE(inode);
  if(!dinode_i->inode_data[0]) {
    printk("Dwarfs root_inode_data[0] doesnt exist!\n");
    return 0;
  }

  if(!(bh = sb_bread(sb, 8))) {
    printk("Dwarfs rootdatablock not found!\n");
    return 0;
  }
  dirptr = (struct dwarfs_directory_entry *)bh->b_data;
  if(!dirptr) {
    printk("rootdatablock is NULL\n");
    brelse(bh);
    return 0;
  }
  brelse(bh);
  return 1;
}

/* Just create root for now */
// DEntries might be necessary
/*
int dwarfs_create_dirdata(struct super_block *sb, struct inode *inode) {
  struct dwarfs_inode_info *dinode_i = NULL;
  struct buffer_head *bh = NULL;
  struct dwarfs_directory_entry *dirptr = NULL;
  struct dwarfs_directory_entry *blkstart = NULL;
  struct buffer_head *bmbh = read_data_bitmap(sb);
  int blocknum = 8;

  printk("Dwarfs: create_dirdata\n");

  test_and_set_bit(0, (unsigned long *)bmbh->b_data); // Statically choosing 1st data block for root. Definitely change this down the line

  dinode_i = DWARFS_INODE(inode);
  bh = sb_bread(sb, blocknum); // Just read first data block for right now

  dirptr = (struct dwarfs_directory_entry *)bh->b_data;
  blkstart = dirptr;

  dirptr->inode = DWARFS_ROOT_INUM;
  dirptr->entrylen = 1;
  dirptr->namelen = 1;
  dirptr->filetype = 0;
  strcpy(dirptr->filename, ".");

  dirptr++;
  dirptr->inode = DWARFS_ROOT_INUM;
  dirptr->entrylen = 1;
  dirptr->namelen = 2;
  dirptr->filetype = 0;
  strcpy(dirptr->filename, "..");

  dinode_i->inode_data[0] = blocknum;
  dwarfs_write_buffer(&bh, sb);

  return 0;
}
*/

static int dwarfs_link(struct dentry *src, struct inode *inode, struct dentry *dest) {
  printk("Dwarfs: link not implemented!\n");
  return -ENOSYS;
}

static int dwarfs_unlink(struct inode *inode, struct dentry *dentry) {
  printk("Dwarfs: unlink not implemented!\n");
  return -ENOSYS;
}

static int dwarfs_symlink(struct inode *inode, struct dentry *dentry, const char *name) {
  printk("Dwarfs: symlink not implemented!\n");
  return -ENOSYS;
}

static int dwarfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode) {
  struct inode *newnode = NULL;
  int err;

  printk("Dwarfs: mkdir\n");

  if((err = dquot_initialize(dir))) {
    printk("Dwarfs: Could not initialize quota operations\n");
    return err;
  }
  inode_inc_link_count(dir);
  newnode = dwarfs_create_inode(dir, &dentry->d_name, mode | S_IFDIR);
  if(IS_ERR(newnode)) {
    printk("Dwarfs: Failed to create new directory inode!\n");
    return PTR_ERR(newnode);
  }
  newnode->i_fop = &dwarfs_dir_operations;
  newnode->i_op = &dwarfs_dir_inode_operations;
  newnode->i_mapping->a_ops = &dwarfs_aops;  

  inode_inc_link_count(newnode);

  if((err = dwarfs_make_empty_dir(newnode, dir))) {
    printk("Dwarfs: could not make empty directory!\n");
    inode_dec_link_count(newnode);
    discard_new_inode(newnode);
    inode_dec_link_count(dir);
    return err;
  }
  if((err = dwarfs_link_node(dentry, newnode))) {
    printk("Dwarfs: Failed to link DEntry to iNode!\n");
    inode_dec_link_count(newnode);
    discard_new_inode(newnode);
    inode_dec_link_count(dir);
    return err;
  }
  d_instantiate_new(dentry, newnode);
  return err;
}

static int dwarfs_rmdir (struct inode *inode, struct dentry *dentry) {
  // Use rmfile, but recursively?
  printk("Dwarfs: rmdir not implemented!\n");
  return -ENOSYS;
}

static int dwarfs_mknod(struct inode *inode, struct dentry *dentry, umode_t mode, dev_t not_sure) {
  printk("Dwarfs: mknod not implemented!\n");
  return -ENOSYS;
}

static int dwarfs_rename(struct inode *inode, struct dentry *dentry, struct inode *newinode, struct dentry *newdentry, unsigned int unsure) {
  printk("Dwarfs: rename not implemented!\n");
  return -ENOSYS;
}

static int dwarfs_readlink(struct dentry *dentry, char __user *maybe_name, int unsure) {
  printk("Dwarfs: readlink not implemented!\n");
  return -ENOSYS;
}

static const char *dwarfs_get_link(struct dentry *dentry, struct inode *inode, struct delayed_call *wtf) {
  printk("Dwarfs: get_link not implemented!\n");
  return ERR_PTR(-ENOSYS);
}

static int dwarfs_permission(struct inode *inode, int unsure) {
  printk("Dwarfs: permission not implemented!\n");
  return -ENOSYS;
}

static int dwarfs_get_acl(struct inode *inode, int somenum) {
  printk("Dwarfs: get_acl not implemented!\n");
  return -ENOSYS;
}

static int dwarfs_setattr(struct dentry *dentry, struct iattr *iattr) {
  int err;
  struct inode *inode = d_inode(dentry);

  printk("Dwarfs: setattr\n");

  err = setattr_prepare(dentry, iattr);
  if(err) {
    printk("An error was encountered during setattr_prepare!\n");
    return err;
  }
  setattr_copy(inode, iattr);
  if(iattr->ia_valid & ATTR_MODE) {
    err = posix_acl_chmod(inode, inode->i_mode);
  }
  mark_inode_dirty(inode);
  return err;
}

static int dwarfs_getattr(const struct path *path, struct kstat *kstat, u32 req_mask, unsigned int query_flags) {
  struct inode *inode = NULL;
  struct dwarfs_inode_info *dinode_i = NULL;
  uint64_t flags;

  printk("Dwarfs: getattr\n");

  inode = d_inode(path->dentry);
  dinode_i = DWARFS_INODE(inode);
  flags = dinode_i->inode_flags & FS_FL_USER_VISIBLE;
  // Check for flags and add to kstat

  generic_fillattr(inode, kstat);
  return 0;
}

static ssize_t dwarfs_listxattr(struct dentry *dentry, char *str, size_t size) {
  printk("Dwarfs: listxattr not implemented!\n");
  return -ENOSYS;
}

static void dwarfs_update_time(struct inode *inode, struct timespec *ts, int somenum) {
  printk("Dwarfs: update_time not implemented!\n");
}

static int dwarfs_atomic_open(struct inode *inode, struct dentry *dentry, struct file *file, unsigned open_flag, umode_t create_mode) {
  printk("Dwarfs: atomic_open not implemented!\n");
  return -ENOSYS;
}

static int dwarfs_dir_tmpfile(struct inode *inode, struct dentry *dentry, umode_t mode) {
  printk("Dwarfs: tmpfile not implemented!\n");
  return -ENOSYS;
}

static int dwarfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
  printk("Dwarfs: create\n");
  return -ENOSYS;
}

const struct inode_operations dwarfs_dir_inode_operations = {
  .create       = dwarfs_create,
  .lookup       = dwarfs_lookup,
  .link         = dwarfs_link,
  .unlink       = dwarfs_unlink,
  .symlink      = dwarfs_symlink,
  .mkdir        = dwarfs_mkdir,
  .rmdir        = dwarfs_rmdir,
  .mknod        = dwarfs_mknod,
  .rename       = dwarfs_rename,
//  .get_link     = dwarfs_get_link,
//  .readlink     = dwarfs_readlink,
//  .permission   = dwarfs_permission,
  .setattr      = dwarfs_setattr,
  .getattr      = dwarfs_getattr,
//  .listxattr    = dwarfs_listxattr,
//  .update_time  = dwarfs_update_time,
//  .atomic_open  = dwarfs_atomic_open,
  .tmpfile      = dwarfs_dir_tmpfile,
};

//static ssize_t dwarfs_read_dir(struct file *file, char __user *buf, size_t size, loff_t *pos) {
//  printk("Dwarfs: read_dir\n");
//  return -ENOSYS;
//}

static int dwarfs_read_dir(struct file *file, struct dir_context *ctx) {
  struct inode *inode = file_inode(file);
  struct dwarfs_inode_info *dinode_i = DWARFS_INODE(inode);
  struct super_block *sb = inode->i_sb;
  struct buffer_head *bh = NULL;
  struct dwarfs_directory_entry *dirent = NULL;
  char *limit = NULL;

  printk("Dwarfs: reading directory\n");

  // For now, only reading first block
  bh = sb_bread(sb, dinode_i->inode_data[0]);
  if(!bh) {
    printk("Dwarfs: Failed to get inode data buffer\n");
    return -EIO;
  }
  dirent = (struct dwarfs_directory_entry *)bh->b_data;
  limit = (char *)dirent + DWARFS_BLOCK_SIZE;
  while((char *)dirent <= limit) {
    if(dirent->entrylen == 0) {
      printk("Dwarfs: encountered dirent of size 0; ignoring\n");
      dirent++;
      continue;
    }
    else if(dirent->namelen > 0) printk("Dwarfs: name encountered: %s\n", dirent->filename);
    if(dirent->inode) {
      unsigned char d_type = DT_UNKNOWN;

      if(!dir_emit(ctx, dirent->filename, dirent->namelen, le64_to_cpu(dirent->inode), d_type)) {
        brelse(bh);
        return 0;
      }
    }
    ctx->pos += sizeof(struct dwarfs_directory_entry);
    dirent++;
  }
  printk("Dwarfs: exiting read\n");
  return 0;
}

static long dwarfs_ioctl(struct file *fileptr, unsigned int cmd, unsigned long arg) {
  printk("Dwarfs: ioctl\n");
  return -ENOSYS;
}

static int dwarfs_fsync(struct file *file, loff_t start, loff_t end, int datasync) {
  printk("Dwarfs: fsync\n");
  return -ENOSYS;
}

const struct file_operations dwarfs_dir_operations = {
  .llseek         = generic_file_llseek,
  .read           = generic_read_dir,
  .iterate_shared = dwarfs_read_dir,
  .unlocked_ioctl = dwarfs_ioctl,
  .fsync          = dwarfs_fsync,
};