#include "structs.h"
#include <linux/module.h>
#include <linux/init.h>

/* Register FS module information */
MODULE_AUTHOR("Adrian S. Almenningen");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DwarFS filesystem for bachelor project Computer Science @ VU Amsterdam 2020");

static int __init sizes_init(void) {
    printk( "DWARFS structures: \n"
            "Superblock:    %ld\n"
            "inode:         %ld\n"
            "dentry:        %ld\n\n", sizeof(struct dwarfs_superblock), sizeof(struct dwarfs_inode), sizeof(struct dwarfs_directory_entry));

    printk( "EXT4 structures: \n"
            "Superblock:    %ld\n"
            "inode:         %ld\n"
            "dentry:        %ld\n", sizeof(struct ext4_super_block), sizeof(struct ext4_inode), sizeof(struct ext4_dir_entry));
    
    return 0;
}

static void __exit sizes_exit(void) {
    printk("Removed sizes module\n");
}


module_init(sizes_init);
module_exit(sizes_exit);
