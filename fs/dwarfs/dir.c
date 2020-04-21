#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>


static struct dentry *dwarfs_lookup(struct inode *dir, struct dentry *dentry, unsigned flags) {
  printk("Dwarfs: dwarfs_lookup not implemented yet!\n");
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
int dwarfs_create_dirdata(struct super_block *sb, struct inode *inode) {
  struct dwarfs_inode_info *dinode_i;
  struct buffer_head *bh;
  struct dwarfs_directory_entry *dirptr;
  struct dwarfs_directory_entry *blkstart;

  int blocknum = 8;

  dinode_i = DWARFS_INODE(inode);
  bh = sb_bread(sb, blocknum); // Just read first data block for right now
  
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