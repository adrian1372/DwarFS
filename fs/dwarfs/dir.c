#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/string.h>


uint64_t dwarfs_get_ino_by_name(struct inode *dir, const struct qstr *inode_name) {
  uint64_t ino;
  struct dwarfs_directory_entry *dirent;
  struct dwarfs_inode_info *di_i;
  int i;
  struct buffer_head *bh;

  di_i = DWARFS_INODE(dir);
  for(i = 0; i < DWARFS_NUMBLOCKS; i++) {
    if(di_i->inode_data[i] <= 0)
      break;
    
    bh = sb_bread(dir->i_sb, di_i->inode_data[i]);
    dirent = (struct dwarfs_directory_entry *)bh->b_data;
    while(dirent && dirent < ((struct dwarfs_directory_entry*)bh->b_data + (DWARFS_BLOCK_SIZE/sizeof(struct dwarfs_directory_entry)))) {
      if(dirent->filename && strnlen(dirent->filename, DWARFS_MAX_FILENAME_LEN) > 0) {
        printk("Checking: %s\n", dirent->filename);
        if(strncmp(dirent->filename, inode_name->name, DWARFS_MAX_FILENAME_LEN) == 0) {
          ino = dirent->inode;
          printk("Inode found at ino %llu\n", ino);
          return ino;
        }
      }
      dirent++;
    }
  }
  return 0;
}

static struct dentry *dwarfs_lookup(struct inode *dir, struct dentry *dentry, unsigned flags) {
  uint64_t ino;
  struct inode *inode;

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
  struct dwarfs_inode_info *dinode_i;
  struct buffer_head *bh;
  struct dwarfs_directory_entry *dirptr;

  dinode_i = DWARFS_INODE(inode);
  if(!dinode_i->inode_data[0]) {
    printk("Dwarfs root_inode_data[0] doesnt exist!\n");
    return 1;
  }

  if(!(bh = sb_bread(sb, 8))) {
    printk("Dwarfs rootdatablock not found!\n");
    return 2;
  }
  dirptr = (struct dwarfs_directory_entry *)bh->b_data;
  if(!dirptr) {
    printk("rootdatablock is NULL\n");
    return 3;
  }
  return 0;
}

/* Just create root for now */
// DEntries might be necessary
int dwarfs_create_dirdata(struct super_block *sb, struct inode *inode) {
  struct dwarfs_inode_info *dinode_i;
  struct buffer_head *bh;
  struct dwarfs_directory_entry *dirptr;
  struct dwarfs_directory_entry *blkstart;

  int blocknum = 8;

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

  return 0;
}

static int dwarfs_link(struct dentry *src, struct inode *inode, struct dentry *dest) {
  printk("Dwarfs: link not implemented!");
  return -ENOSYS;
}

static int dwarfs_unlink(struct inode *inode, struct dentry *dentry) {
  printk("Dwarfs: unlink not implemented!");
  return -ENOSYS;
}

static int dwarfs_symlink(struct inode *inode, struct dentry *dentry, const char *name) {
  printk("Dwarfs: symlink not implemented!");
  return -ENOSYS;
}

static int dwarfs_mkdir(struct inode *inode, struct dentry *dentry, umode_t mode) {
  printk("Dwarfs: mkdir not implemented!");
  return -ENOSYS;
}

static int dwarfs_rmdir (struct inode *inode, struct dentry *dentry) {
  printk("Dwarfs: rmdir not implemented!");
  return -ENOSYS;
}

static int dwarfs_mknod(struct inode *inode, struct dentry *dentry, umode_t mode, dev_t not_sure) {
  printk("Dwarfs: mknod not implemented!");
  return -ENOSYS;
}

static int dwarfs_rename(struct inode *inode, struct dentry *dentry, struct inode *newinode, struct dentry *newdentry, unsigned int unsure) {
  printk("Dwarfs: rename not implemented!");
  return -ENOSYS;
}

static int dwarfs_readlink(struct dentry *dentry, char __user *maybe_name, int unsure) {
  printk("Dwarfs: readlink not implemented!");
  return -ENOSYS;
}

static const char *dwarfs_get_link(struct dentry *dentry, struct inode *inode, struct delayed_call *wtf) {
  printk("Dwarfs: get_link not implemented!");
  return ERR_PTR(-ENOSYS);
}

static int dwarfs_permission(struct inode *inode, int unsure) {
  printk("Dwarfs: permission not implemented!");
  return -ENOSYS;
}

static int dwarfs_get_acl(struct inode *inode, int somenum) {
  printk("Dwarfs: get_acl not implemented!");
  return -ENOSYS;
}

static int dwarfs_setattr(struct dentry *dentry, struct iattr *iattr) {
  printk("Dwarfs: setattr not implemented!");
  return -ENOSYS;
}

static int dwarfs_getattr(const struct path *path, struct kstat *kstat, u32 somenum, unsigned int anothernum) {
  printk("Dwarfs: getattr not implemented!");
  return -ENOSYS;
}

static ssize_t dwarfs_listxattr(struct dentry *dentry, char *str, size_t size) {
  printk("Dwarfs: listxattr not implemented!");
  return -ENOSYS;
}

static void dwarfs_update_time(struct inode *inode, struct timespec *ts, int somenum) {
  printk("Dwarfs: update_time not implemented!");
}

static int dwarfs_atomic_open(struct inode *inode, struct dentry *dentry, struct file *file, unsigned open_flag, umode_t create_mode) {
  printk("Dwarfs: atomic_open not implemented!");
  return -ENOSYS;
}

static int dwarfs_dir_tmpfile(struct inode *inode, struct dentry *dentry, umode_t mode) {
  printk("Dwarfs: tmpfile not implemented!");
  return -ENOSYS;
}


const struct inode_operations dwarfs_dir_inode_operations = {
  //  .create       = dwarfs_create,
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

const struct file_operations dwarfs_dir_operations = {
    .llseek         = generic_file_llseek,
    .read           = generic_read_dir,
  //  .iterate_shared = dwarfs_iterate_shared,
  //  .unlocked_ioctl = dwarfs_ioctl,
  //  .fsync          = dwarfs_fsync,
};