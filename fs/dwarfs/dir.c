#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <string.h>


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
    while(dirent) {
      if(strncmp(dirent->filename, inode_name->name, DWARFS_MAX_FILENAME_LEN) {
        ino = dirent->inode;
        printk("Inode found at ino %llu\n", ino);
        return ino;
      }
    }
  }
  return 0;
}

static struct dentry *dwarfs_lookup(struct inode *dir, struct dentry *dentry, unsigned flags) {
  uint64_t ino;
  struct inode *inode;

  if(dentry->d_name.len > DWARFS_MAX_FILENAME_LEN || dentry->d_name.len <= 0) {
    printk("Dwarfs: Invalid DEntry name length: %llu\n", dentry->d_name.len);
    return ERR_PTR(-ENAMETOOLONG);
  }
  printk("Dwarfs: Looking up: %s\n", dentry->d_name.name);
  ino = dwarfs_get_ino_by_name(dir, &dentry->d_name);// Get the inum somehow
  if(ino) {
    inode = dwarfs_inode_get(dir->i_sb, ino);
    if(IS_ERR(inode)) {
      printk("Dwarfs: Couldn't find target inode\n");
      return PTR_ERR(inode);
    }
    d_add(dentry, inode);
  }

  return NULL;
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


const struct inode_operations dwarfs_dir_inode_operations = {
    .lookup     = dwarfs_lookup,
};

const struct file_operations dwarfs_dir_operations = {
    .llseek     = generic_file_llseek,
    .read       = generic_read_dir,
  //  .iterate    = generic_read_dir,
};