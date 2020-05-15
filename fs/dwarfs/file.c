#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/quotaops.h>
#include <linux/aio.h>

/* This doesn't work at all, keep out of the Makefile */

int dwarfs_fiemap(struct inode *inode, struct fiemap_extent_info *finfo, uint64_t start, uint64_t len) {
  return generic_block_fiemap(inode, finfo, start, len, dwarfs_get_iblock);
}

const struct inode_operations dwarfs_file_inode_operations = {
    .setattr        = dwarfs_setattr,
    .getattr        = dwarfs_getattr,
//    .get_acl        = dwarfs_getacl,
//    .set_acl        = dwarfs_setacl,
    .fiemap         = dwarfs_fiemap,
};

ssize_t dwarfs_file_read_iter (struct kiocb * iocb, struct iov_iter * iter) {
  printk("Dwarfs: file_read_iter\n");
  return generic_file_read_iter(iocb, iter);
}

ssize_t dwarfs_file_write_iter(struct kiocb *iocb, struct iov_iter *iter) {
  printk("Dwarfs: file_write_iter\n");
  return generic_file_write_iter(iocb, iter);
}

loff_t dwarfs_file_llseek(struct file *file, loff_t offset, int whence) {
  printk("Dwarfs: generic_file_llseek\n");
  return generic_file_llseek(file, offset, whence);
}

int dwarfs_fsync(struct file *file, loff_t start, loff_t end, int sync) {
  printk("Dwarfs: fsync\n");
  return generic_file_fsync(file, start, end, sync);
}

const struct file_operations dwarfs_file_operations = {
    .llseek             = dwarfs_file_llseek,
    .read_iter          = dwarfs_file_read_iter,
    .write_iter         = dwarfs_file_write_iter,
  //  .release          = generic_release,
  //  .unlocked_ioctl   = generic_ioctl,
    .fsync              = dwarfs_fsync,
    .splice_read        = generic_file_splice_read,
    .splice_write       = iter_file_splice_write,
    .open               = dquot_file_open,
    .get_unmapped_area  = thp_get_unmapped_area,
};