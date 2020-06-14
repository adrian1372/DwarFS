#include "structs.h"

/* Register FS module information */
MODULE_AUTHOR("Adrian S. Almenningen");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DwarFS filesystem for bachelor project Computer Science @ VU Amsterdam 2020");

static int __init sizes_init(void) {
    printk( "DWARFS structures: \n"
            "Superblock:    %d\n"
            "inode:         %d\n"
            "dentry:        %d\n\n", sizeof(struct dwarfs_superblock), sizeof(struct dwarfs_inode), sizeof(struct dwarfs_directory_entry));

    printk( "EXT4 structures: \n"
            "Superblock:    %d\n"
            "inode:         %d\n"
            "dentry:        %d\n", sizeof(struct ext4_super_block), sizeof(struct ext4_inode), sizeof(struct ext4_dir_entry));
    
    return 0;
}

static void __exit sizes_exit(void) {
    printk("Removed sizes module\n");
    return;
}


module_init(sizes_init);
module_exit(sizes_exit);
