#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/quotaops.h>

/* This doesn't work at all, keep out of the Makefile */

const struct inode_operations dwarfs_file_inode_operations = {
    .setattr        = dwarfs_setattr,
    .getattr        = dwarfs_getattr,
//    .get_acl        = dwarfs_getacl,
//    .set_acl        = dwarfs_setacl,
//    .fiemap         = dwarfs_fiemap,
};

const struct file_operations dwarfs_file_operations = {
    .llseek         = generic_file_llseek,
    .read_iter      = generic_file_read_iter,
    .write_iter     = generic_file_write_iter,
    .mmap           = generic_file_mmap,
  //  .release        = generic_file_release,
  //  .unlocked_ioctl = generic_ioctl,
  //  .fsync          = generic_file_fsync,
    .splice_read    = generic_file_splice_read,
    .splice_write   = iter_file_splice_write,
    .open           = dquot_file_open,
    .get_unmapped_area  = thp_get_unmapped_area,
};