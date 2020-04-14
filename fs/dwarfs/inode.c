#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>

const struct inode_operations dwarfs_file_inode_operations {
    .setattr        = generic_setattr,
    .getattr        = generic_getattr,
    .update_time    = generic_update_time,
};
