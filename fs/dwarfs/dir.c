#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/string.h>
#include <linux/posix_acl_xattr.h>
#include <linux/quotaops.h>
#include <linux/pagemap.h>
#include <linux/iversion.h>

/*
 * Make an empty dir after the inode has been instantiated in mkdir, and generate DOT & DOTDOT.
 * This must be done on a directory that has NO data, otherwise the 1st block is overwritten
 */
int dwarfs_make_empty_dir(struct inode *inode, struct inode *dir) {
  struct dwarfs_directory_entry *direntry = NULL;
  struct dwarfs_inode_info *dinode_i = DWARFS_INODE(inode);
  struct buffer_head *bh = NULL;
  char *blockaddr = NULL;
  int64_t first_blocknum = dwarfs_data_alloc(dir->i_sb, inode);

  if(first_blocknum < 0) {
    printk("Dwarfs: blocknum is error code: %lld\n", first_blocknum);
    return first_blocknum;
  }

  dinode_i->inode_data[0] = first_blocknum;
  if(!(bh =  sb_bread(dir->i_sb, dinode_i->inode_data[0]))) {
    printk("Dwarfs: Failed to grab buffer head!\n");
    return -ENOMEM;
  }

  blockaddr = bh->b_data;
  memset(blockaddr, 0, DWARFS_BLOCK_SIZE);
  direntry = (struct dwarfs_directory_entry *)blockaddr;
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

  inode->i_size = dir->i_sb->s_blocksize;
  dwarfs_write_buffer(&bh, dir->i_sb);
  return 0;
}

/*
 * Look up the file in dentry in directory dir.
 */
static struct dentry *dwarfs_lookup(struct inode *dir, struct dentry *dentry, unsigned flags) {
  int64_t ino;
  struct inode *inode = NULL;

  if(dentry->d_name.len > DWARFS_MAX_FILENAME_LEN || dentry->d_name.len <= 0) {
    printk("Dwarfs: Invalid DEntry name length: %u\n", dentry->d_name.len);
    return ERR_PTR(-ENAMETOOLONG);
  }
  ino = dwarfs_get_ino_by_name(dir, &dentry->d_name);
  if(ino) {
    inode = dwarfs_inode_get(dir->i_sb, ino);
    if(IS_ERR(inode)) {
      printk("Dwarfs: Error occured when geting inode: %ld\n", PTR_ERR(inode));
      return ERR_PTR(PTR_ERR(inode));
    }
  }
  return d_splice_alias(inode, dentry);
}

/*
 * Helper function to check if the root inode has data associated with it.
 * Only used to decide if it needs to be generated on mount or not
 */
int dwarfs_rootdata_exists(struct super_block *sb, struct inode *inode) {
  struct dwarfs_inode_info *dinode_i = NULL;
  struct buffer_head *bh = NULL;
  struct dwarfs_directory_entry *dirptr = NULL;

  dinode_i = DWARFS_INODE(inode);
  if(!dinode_i->inode_data[0]) {
    printk("Dwarfs root_inode_data[0] doesnt exist!\n");
    return 0;
  }

  if(!(bh = sb_bread(sb, dinode_i->inode_data[0]))) {
    printk("Dwarfs rootdatablock not found!\n");
    return -EIO;
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

static int dwarfs_link(struct dentry *src, struct inode *dir, struct dentry *dest) {
  struct inode *srcnode = d_inode(src);
  int err;

  err = dquot_initialize(dir);
  if(err)
    return err;
  
  srcnode->i_ctime = current_time(srcnode);
  inode_inc_link_count(srcnode);
  ihold(srcnode);

  err = dwarfs_link_node(dest, srcnode);
  if(err) {
    inode_dec_link_count(srcnode);
    iput(srcnode);
    return err;
  }
  d_instantiate(dest, srcnode);
  return err;
}

/*
 * Helper function to find the directory entry in dir with the given name.
 * The buffer_head in bh should be NULL, otherwise the existing buffer_head may be lost.
 * On success, bh will contain the data block where the directory entry was found.
 */
static struct dwarfs_directory_entry *dwarfs_get_direntry(const char *name, struct inode *dir, struct buffer_head **bh) {
  struct dwarfs_directory_entry *currentry = NULL;
  struct dwarfs_inode_info *dir_dinode_i = DWARFS_INODE(dir);
  struct super_block *sb = dir->i_sb;
  int i;

  for(i = 0; i < dir->i_blocks; i++) {
    if(!dir_dinode_i->inode_data[i])
      continue;
    *bh = sb_bread(sb, dir_dinode_i->inode_data[i]);
    currentry = (struct dwarfs_directory_entry *)(*bh)->b_data;
    for( ; (char *)currentry < (*bh)->b_data + sb->s_blocksize; currentry++) {
      if(strncmp(currentry->filename, name, DWARFS_MAX_FILENAME_LEN) == 0) {
        return currentry;
      }
    }
    brelse(*bh);
  }
  printk("Dwarfs: couldn't find directory entry %s\n", name);
  return ERR_PTR(-ENOENT);
}

static inline void dwarfs_clear_direntry(struct dwarfs_directory_entry *direntry) {
  direntry->entrylen = 0;
  memset(direntry->filename, 0, DWARFS_MAX_FILENAME_LEN);
  direntry->namelen = 0;
  direntry->inode = 0;
}

/*
 * Unlinks the file specified by l_dentry from dir and resets the values of its directory entry to 0.
 * If this is the only known link to the file, deallocate it on the disk
 */
static int dwarfs_unlink(struct inode *dir, struct dentry *l_dentry) {
  struct inode *l_inode = d_inode(l_dentry);
  struct buffer_head *bh = NULL;
  struct dwarfs_directory_entry *l_direntry = dwarfs_get_direntry(l_dentry->d_name.name, dir, &bh);

  if(IS_ERR(l_direntry)) {
    printk("Dwarfs: l_direntry is an error code!\n");
    return PTR_ERR(l_direntry);
  }
  dwarfs_clear_direntry(l_direntry);
  dwarfs_write_buffer(&bh, dir->i_sb);
  l_inode->i_ctime = dir->i_ctime;
  if(l_inode->i_nlink == 1 || (S_ISDIR(l_inode->i_mode) && l_inode->i_nlink == 2)) {
    inode_dec_link_count(l_inode);
    dwarfs_inode_dealloc(l_inode->i_sb, l_inode->i_ino);
  }
  else inode_dec_link_count(l_inode);
  return 0;
}

/*
static int dwarfs_symlink(struct inode *dir, struct dentry *dentry, const char *name) {
  //struct super_block *sb = dir->i_sb;
  struct inode *newnode = NULL;
  int err, namelen;
  
  printk("Dwarfs: symlink\n");

  if((err = dquot_initialize(dir)))
    return err;
  
  newnode = dwarfs_create_inode(dir, &dentry->d_name, S_IRWXUGO | S_IFLNK);
  if(IS_ERR(newnode))
    return PTR_ERR(newnode);

  namelen = strnlen(name, DWARFS_MAX_FILENAME_LEN) + 1; // accounting for null-byte
  if(namelen < sizeof(DWARFS_INODE(newnode)->inode_data)) {
    newnode->i_op = &dwarfs_symlink_inode_operations;
    newnode->i_link = (char *)DWARFS_INODE(newnode)->inode_data;
    strncpy(newnode->i_link, name, namelen);
    newnode->i_size = namelen-1;
  }
  mark_inode_dirty(newnode);

  if((err = dwarfs_link_node(dentry, newnode))) {
    inode_dec_link_count(newnode);
    discard_new_inode(newnode);
    return err;
  }
  d_instantiate_new(dentry, newnode);
  return 0;
}
*/

static int dwarfs_symlink(struct inode *dir, struct dentry *dentry, const char *name) {
  printk("Dwarfs: symbolic links not supported!\n");
  return -EOPNOTSUPP;
}

/*
 * Create the inode for the new directory, allocate data and generate DOT/DOTDOT, link to dir.
 */
static int dwarfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode) {
  struct inode *newnode = NULL;
  int err;

  if((err = dquot_initialize(dir))) {
    printk("Dwarfs: Could not initialize quota operations\n");
    return err;
  }
  inode_inc_link_count(dir);
  newnode = dwarfs_create_inode(dir, &dentry->d_name, mode | S_IFDIR);
  if(IS_ERR(newnode)) {
    printk("Dwarfs: Failed to create new directory inode!\n");
    inode_dec_link_count(dir);
    return PTR_ERR(newnode);
  }
  newnode->i_fop = &dwarfs_dir_operations;
  newnode->i_op = &dwarfs_dir_inode_operations;
  newnode->i_mapping->a_ops = &dwarfs_aops;  

  inode_inc_link_count(newnode);

  if((err = dwarfs_make_empty_dir(newnode, dir))) {
    printk("Dwarfs: could not make empty directory!\n");
    goto cleanup;
  }
  if((err = dwarfs_link_node(dentry, newnode))) {
    printk("Dwarfs: Failed to link DEntry to iNode!\n");
    goto cleanup;
  }
  d_instantiate_new(dentry, newnode);
  return err;

cleanup:
  inode_dec_link_count(newnode);
  discard_new_inode(newnode);
  inode_dec_link_count(dir);
  return err;
}

static int dwarfs_check_dir_empty(struct inode *inode) {
  struct dwarfs_inode_info *dinode_i = DWARFS_INODE(inode);
  int i;
  struct buffer_head *bh = NULL;
  
  /*
   * We need to check that all data blocks don't have any data, as removing a reference
   * to a non-deleted file will cause the inode and allocated data to be lost.
   * There is one base-case, that the datablock pointed to is 0.
   * Otherwise, we need to read through it and check the directory entry structures.
   */
  for(i = 0; i < inode->i_blocks; i++) {
    struct dwarfs_directory_entry *direntry = NULL;
    if(dinode_i->inode_data[i] == 0)
      continue;
    bh = sb_bread(inode->i_sb, dinode_i->inode_data[i]);
    if(!bh || IS_ERR(bh)) {
      printk("Couldn't get directory buffer\n");
      return -EIO;
    }
    
    /*
     * We simply go through every direntry and make sure they're all empty.
     * The DOT and DOTDOT files still being there is acceptable, as they don't
     * store data of their own, and simply discarding them won't cause issues.
     */
    direntry = (struct dwarfs_directory_entry *)bh->b_data;
    for( ; (char*)direntry < bh->b_data + DWARFS_BLOCK_SIZE; direntry++) {
      if(direntry->inode != 0 && direntry->namelen > 0) {
        if(strncmp(direntry->filename, ".", DWARFS_MAX_FILENAME_LEN) != 0 && \
            strncmp(direntry->filename, "..", DWARFS_MAX_FILENAME_LEN) != 0) {
          printk("Dwarfs: found non-empty entry: %s, %llu\n", direntry->filename, direntry->inode);
          brelse(bh);
          return -ENOTEMPTY;
        }
      }
    }
    brelse(bh);
  }
  return 0;
}

/*
 * Mostly the same as unlink, except that we need to make sure the directory is empty
 * and account for DOT and DOTDOT also counting as links (otherwise data deallocation won't happen)
 */
static int dwarfs_rmdir(struct inode *dir, struct dentry *dentry) {
  struct inode *inode = d_inode(dentry);
  int err = 0;

  if((err = dwarfs_check_dir_empty(inode)) != 0) {
    printk("Dwarfs: directory isn't empty!\n");
    return err;
  }
  err = dwarfs_unlink(dir, dentry);
  if(!err) {
    inode_dec_link_count(inode);
    inode_dec_link_count(dir);
  }
  return err;
}

/*
 * Creates special files. Only pipes have been tested extensively,
 * though other types should work as well.
 */
static int dwarfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev) {
  struct inode *inode = NULL;
  int err = dquot_initialize(dir);

  if(err) return err;

  inode = dwarfs_create_inode(dir, &dentry->d_name, mode);
  if(IS_ERR(inode))
    return PTR_ERR(inode);
  
  init_special_inode(inode, inode->i_mode, dev);
  inode->i_op = &dwarfs_special_inode_operations;
  mark_inode_dirty(inode);

  err = dwarfs_link_node(dentry, inode);
  if(err) {
    inode_dec_link_count(inode);
    discard_new_inode(inode);
    return err;
  }
  d_instantiate_new(dentry, inode);
  return 0;
}

static int dwarfs_rename(struct inode *dir, struct dentry *dentry, struct inode *newdir, struct dentry *newdentry, unsigned int flags) {

  struct dwarfs_directory_entry *dirent = NULL;
  struct dwarfs_directory_entry *dotdotdirent = NULL;
  struct buffer_head *direntbh, *dotdotbh = NULL;
  struct inode *inode = d_inode(dentry);
  int err;

  printk("Dwarfs: rename\n");

  // Only supporting NOREPLACE
  if(flags & ~RENAME_NOREPLACE)
    return -EINVAL;
  
  if((err = dquot_initialize(inode)))
    return err;
  
  dirent = dwarfs_get_direntry(dentry->d_name.name, dir, &direntbh);
  if(IS_ERR(dirent))
    return PTR_ERR(dirent);

  dwarfs_link_node(newdentry, inode);
  if(S_ISDIR(inode->i_mode)) { // need to update DOTDOT
    dotdotdirent = dwarfs_get_direntry("..", inode, &dotdotbh);
    dotdotdirent->inode = newdir->i_ino;
    dwarfs_write_buffer(&dotdotbh, dir->i_sb);
    inode_dec_link_count(dir);
    inode_inc_link_count(newdir);
  }

  inode->i_ctime = current_time(inode);
  mark_inode_dirty(inode);
  dwarfs_clear_direntry(dirent);
  dwarfs_write_buffer(&direntbh, dir->i_sb);
  return 0;
}

int dwarfs_setattr(struct dentry *dentry, struct iattr *iattr) {
  int err;
  struct inode *inode = d_inode(dentry);

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

int dwarfs_getattr(const struct path *path, struct kstat *kstat, u32 req_mask, unsigned int query_flags) {
  struct inode *inode = NULL;
  struct dwarfs_inode_info *dinode_i = NULL;
  uint64_t flags;

  inode = d_inode(path->dentry);
  dinode_i = DWARFS_INODE(inode);
  flags = dinode_i->inode_flags & FS_FL_USER_VISIBLE;
  // Check for flags and add to kstat

  generic_fillattr(inode, kstat);
  return 0;
}

static int dwarfs_dir_tmpfile(struct inode *inode, struct dentry *dentry, umode_t mode) {
  printk("Dwarfs: tmpfile not supported!\n");
  return -EOPNOTSUPP;
}

/*
 * Function for creation of regular files.
 * Simply creates the inode and links it to the given directory, before instantiating the dentry.
 */
static int dwarfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
  struct inode *newnode = NULL;
  int err;

  if((err = dquot_initialize(dir))) {
    printk("Dwarfs: Could not initialize quota operations\n");
    return err;
  }

  newnode = dwarfs_create_inode(dir, &dentry->d_name, mode);
  if(IS_ERR(newnode)) {
    printk("Dwarfs: Failed to create new regfile inode!\n");
    return PTR_ERR(newnode);
  }
  newnode->i_fop = &dwarfs_file_operations;
  newnode->i_op = &dwarfs_file_inode_operations;
  newnode->i_mapping->a_ops = &dwarfs_aops;  

  if((err = dwarfs_link_node(dentry, newnode))) {
    printk("Dwarfs: Failed to link DEntry to iNode!\n");
    inode_dec_link_count(newnode);
    discard_new_inode(newnode);
    return err;
  }
  d_instantiate_new(dentry, newnode);
  return err;
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
  .setattr      = dwarfs_setattr,
  .getattr      = dwarfs_getattr,
  .tmpfile      = dwarfs_dir_tmpfile,
};

/*
 * Function for reading a directory, one block at a time
 */
static int dwarfs_read_dir(struct file *file, struct dir_context *ctx) {
  struct inode *inode = file_inode(file);
  struct dwarfs_inode_info *dinode_i = DWARFS_INODE(inode);
  struct super_block *sb = inode->i_sb;
  struct buffer_head *bh = NULL;
  struct dwarfs_directory_entry *dirent = NULL;
  char *limit = NULL;
  int i = ctx->pos / sb->s_blocksize;

  if(!dinode_i->inode_data[i]) return 0;
  bh = sb_bread(sb, dinode_i->inode_data[i]);
  if(!bh) {
    printk("Dwarfs: Failed to get inode data buffer\n");
    return -EIO;
  }
  dirent = (struct dwarfs_directory_entry *)bh->b_data;
  limit = (char *)dirent + DWARFS_BLOCK_SIZE;
  while((char *)dirent < limit && ctx->pos < inode->i_size) {
    if(dirent->entrylen == 0) {
      ctx->pos += sizeof(struct dwarfs_directory_entry);
      dirent++;
      continue;
    }
    if(dirent->inode) {
      unsigned char d_type = DT_UNKNOWN;

      if(!dir_emit(ctx, dirent->filename, dirent->namelen, le64_to_cpu(dirent->inode), d_type)) {
        printk("Dwarfs: !dir_emit in read_dir: %s\n", dirent->filename);
        brelse(bh);
        return 0;
      }
    }
    ctx->pos += sizeof(struct dwarfs_directory_entry);
    dirent++;
  }
  brelse(bh);
  return 0;
}

static long dwarfs_ioctl(struct file *fileptr, unsigned int cmd, unsigned long arg) {
  printk("Dwarfs: ioctl not supported!\n");
  return -EOPNOTSUPP;
}

static int dwarfs_fsync(struct file *file, loff_t start, loff_t end, int sync) {
  return generic_file_fsync(file, start, end, sync);
}

const struct file_operations dwarfs_dir_operations = {
  .llseek         = generic_file_llseek,
  .read           = generic_read_dir,
  .iterate_shared = dwarfs_read_dir,
  .unlocked_ioctl = dwarfs_ioctl,
  .fsync          = dwarfs_fsync,
};
