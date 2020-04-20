#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>

/* This doesn't work at all, keep out of the Makefile */

const struct file_operations dwarfs_file_operations = {
    .llseek         = generic_file_llseek,
    .read           = new_sync_read,
    .read_iter      = generic_file_read_iter,
    .write_iter     = generic_file_write_iter,
    .mmap           = generic_file_mmap,
    .release        = generic_file_release,
    .unlocked_ioctl = generic_ioctl,
    .compat_ioctl   = compat_ptr_ioctl,
    .fsync          = generic_file_fsync,
    .splice_read    = generic_file_splice_read,
    .splice_write   = iter_file_splice_write,
    .fallocate      = generic_allocate,
};