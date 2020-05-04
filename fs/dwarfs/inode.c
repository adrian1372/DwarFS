#include "dwarfs.h"
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/aio.h>
#include <linux/mpage.h>
#include <linux/slab.h>
#include <linux/cred.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/quotaops.h>

/*
const struct inode_operations dwarfs_file_inode_operations = {
    .setattr        = generic_setattr,
    .getattr        = generic_getattr,
    .update_time    = generic_update_time,
};
*/

int dwarfs_sync_dinode(struct super_block *sb, struct inode *inode) {
    struct dwarfs_inode_info *dinode_i = DWARFS_INODE(inode);
    struct buffer_head *bhptr = NULL;
    struct dwarfs_inode *dinode = dwarfs_getdinode(sb, inode->i_ino, &bhptr);
    int offset = inode->i_ino % (DWARFS_BLOCK_SIZE / DWARFS_SB(sb)->dwarfs_inodesize);
    int i;

    printk("Dwarfs: sync_dinode\n");
    
    if(((struct dwarfs_inode *)bhptr->b_data + offset) != dinode) {
        printk("Something weird happened! bh: %p, dinode: %p\n", ((struct dwarfs_inode *)bhptr->b_data + offset), dinode);
        return -EIO;
    }
    for(i = 0; i < DWARFS_NUMBLOCKS; i++) {
        struct buffer_head *debugbh = NULL;
        struct dwarfs_directory_entry *debugdirent = NULL;
        if(!dinode_i->inode_data[i]) continue;
        printk("Dwarfs: writing inode_block[%d] (%llu) to dinode.\n", i, dinode_i->inode_data[i]);
        dinode->inode_blocks[i] = dinode_i->inode_data[i];
        debugbh = sb_bread(sb, dinode->inode_blocks[i]);
        debugdirent = (struct dwarfs_directory_entry *)debugbh->b_data;
        for( ; (char*)debugdirent < (char*)debugbh->b_data + DWARFS_BLOCK_SIZE; debugdirent++) {
            printk ("Entry:     %s\n"
                    "namelen:   %d\n"
                    "entrylen:  %lld\n"
                    "inode:     %lld\n",
                    debugdirent->filename, debugdirent->namelen, debugdirent->entrylen, debugdirent->inode);
        }
    }
    dinode->inode_dtime = dinode_i->inode_dtime;

    dwarfs_write_buffer(&bhptr, sb);
    return 0;
}

int dwarfs_link_node(struct dentry *dentry, struct inode *inode) {
    struct inode *dirnode = d_inode(dentry->d_parent);
    int namelen = dentry->d_name.len;
    struct buffer_head *bh = NULL;
    struct dwarfs_directory_entry *direntry = NULL;
    char *address, *endaddress = NULL;
    int error = 0;
    
    printk("Dwarfs: dwarfs_link_node\n");
    /*
     * For now, a dir is only one block.
     * Needs future expansion (a loop) to
     * read all other blocks as well.
     */
    bh = sb_bread(dirnode->i_sb, DWARFS_INODE(dirnode)->inode_data[0]);
    printk("Dwarfs: linking inode %lu to %lu\n", inode->i_ino, dirnode->i_ino);
    if(IS_ERR(bh)) {
        printk("Dwarfs: couldn't get the node data buffer_head!\n");
        return PTR_ERR(bh);
    }
    printk("Dwarfs: got buffer_head for node %lu\n", dirnode->i_ino);
    //endaddress = address + (inode->i_size > DWARFS_BLOCK_SIZE ? DWARFS_BLOCK_SIZE : inode->i_size);
    address = (char*)bh->b_data;
    endaddress = address + DWARFS_BLOCK_SIZE;
    direntry = (struct dwarfs_directory_entry *)address;
    while((char *)direntry < endaddress) {
        printk("Dwarfs: evaluating direntry: %s (%llu)\n", direntry->filename, direntry->inode);
        if(direntry->entrylen == 0) {
            printk("Dwarfs: encountered a direntry of size 0!\n");
            goto post_loop;
        }
        if(strncmp(dentry->d_name.name, direntry->filename, DWARFS_MAX_FILENAME_LEN) == 0) {
            printk("DwarFS: File of name %s already exists!\n", dentry->d_name.name);
            error = -EEXISTS;
            goto error_unlock;
        }
        if(!direntry->inode) {
            printk("Dwarfs: Found entry without inode!\n");
            goto post_loop;
        }
        if(strnlen(direntry->filename, DWARFS_MAX_FILENAME_LEN) == 0) {
            printk("Dwarfs: Found entry with length 0 name!\n");
            goto post_loop;
        }
        direntry++;
    }
    brelse(bh);
    printk("Dwarfs: Block is full!\n");
    return -ENOSPC;

post_loop:
    if(direntry->inode) { // At the moment, this should never be true.
        struct dwarfs_directory_entry *direntry2 = (struct dwarfs_directory_entry *)((char *) \
                direntry + sizeof(struct dwarfs_directory_entry));
        printk("Dwarfs: Direntry has an inode, hopping to next entry slot\n");
        direntry2->entrylen = sizeof(struct dwarfs_directory_entry);
        direntry = direntry2;
    }
    direntry->namelen = namelen;
    strncpy(direntry->filename, dentry->d_name.name, DWARFS_MAX_FILENAME_LEN);
    direntry->inode = cpu_to_le64(inode->i_ino);
    direntry->filetype = 0;
    direntry->entrylen = sizeof(struct dwarfs_directory_entry);
    dwarfs_write_buffer(&bh, dirnode->i_sb);
    dirnode->i_mtime = dirnode->i_ctime = current_time(dirnode);
    DWARFS_INODE(dirnode)->inode_flags &= ~FS_BTREE_FL; // From ext2. Wtf does this mean
    mark_inode_dirty(dirnode);
    return 0;
error_unlock:
    brelse(bh);
    return error;
}

uint64_t dwarfs_get_ino_by_name(struct inode *dir, const struct qstr *inode_name) {
  int64_t ino;
  struct dwarfs_directory_entry *dirent = NULL;
  struct dwarfs_inode_info *di_i = NULL;
  int i;
  struct buffer_head *bh = NULL;

  di_i = DWARFS_INODE(dir);
  for(i = 0; i < DWARFS_NUMBLOCKS; i++) {
    if(di_i->inode_data[i] <= 0)
      break;
    
    bh = sb_bread(dir->i_sb, di_i->inode_data[i]);
    dirent = (struct dwarfs_directory_entry *)bh->b_data;
    while(dirent && dirent < ((struct dwarfs_directory_entry *)bh->b_data + (DWARFS_BLOCK_SIZE/sizeof(struct dwarfs_directory_entry)))) {
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

struct inode *dwarfs_create_inode(struct inode *dir, const struct qstr *namestr, umode_t mode) {
    struct buffer_head *dirbh = NULL;
    struct super_block *sb = NULL;
    struct inode *newnode = NULL;
    struct dwarfs_directory_entry *newdirentry = NULL;
    struct dwarfs_inode_info *dinode_i = NULL;
    struct dwarfs_superblock_info *dfsb_i = NULL;
    struct dwarfs_superblock *dfsb = NULL;
    int64_t ino = 2;
    int err;

    sb = dir->i_sb;
    dfsb_i = DWARFS_SB(sb);
    dfsb = dfsb_i->dfsb;

    if((mode & S_IFMT) == S_IFDIR) {
        printk("Dwarfs: create_inode is creating directory: %s\n", namestr->name);
    }

    if(!(newnode = new_inode(sb))) {
        printk("Dwarfs: Failed to create new inode!\n");
        return ERR_PTR(-ENOMEM);
    }
    dinode_i = DWARFS_INODE(newnode);
    ino = dwarfs_inode_alloc(sb);

    inode_init_owner(newnode, dir, mode);
    newnode->i_mode = mode;
    newnode->i_ino = ino;
    newnode->i_blocks = 0;
    newnode->i_mtime = newnode->i_atime = newnode->i_ctime = current_time(newnode);
    memset(dinode_i->inode_data, 0, sizeof(dinode_i->inode_data));
    dinode_i->inode_flags = 0; // This needs to be implemented still
    dinode_i->inode_fragaddr = 0;
    dinode_i->inode_fragnum = 0;
    dinode_i->inode_fragsize = 0;
    dinode_i->inode_dtime = 0;
    dinode_i->inode_block_group = 0;
    dinode_i->inode_dir_start_lookup = 0;
    dinode_i->inode_state = DWARFS_NEW_INODE;

    if(insert_inode_locked(newnode) < 0) {
        printk("Dwarfs: Couldn't create new node, inum already in use: %llu\n", ino);
        return ERR_PTR(-EIO);
    }

    if((err = dquot_initialize(newnode))) {
        printk("Dwarfs: couldnt initialise quota operations\n");
        make_bad_inode(newnode);
        iput(newnode);
        return ERR_PTR(err);
    }
    if((err = dquot_alloc_inode(newnode))) {
        printk("Dwarfs: Couldn't alloc new inode!\n");
        make_bad_inode(newnode);
        iput(newnode);
        return ERR_PTR(err);
    }
    mark_inode_dirty(newnode);
    printk("Dwarfs: Successfully allocated inode: %lu\n", newnode->i_ino);
    return newnode;
}

struct dwarfs_inode *dwarfs_getdinode(struct super_block *sb, int64_t ino, struct buffer_head **bhptr) {
    
    uint64_t block;
    uint64_t offset;
    struct buffer_head *bh = NULL;

    printk("Dwarfs: getdinode\n");

    *bhptr = NULL;
    if((ino < DWARFS_FIRST_INODE && ino != DWARFS_ROOT_INUM) || ino > le64_to_cpu(DWARFS_SB(sb)->dfsb->dwarfs_inodec)) {
        printk("Dwarfs: bad inode number %llu in dwarfs_getdinode\n", ino);
        return ERR_PTR(-EINVAL);
    }
    block = DWARFS_FIRST_INODE_BLOCK + ((ino * DWARFS_SB(sb)->dwarfs_inodesize) / DWARFS_BLOCK_SIZE); // Assumption: integer division rounds down

    if(!(bh = sb_bread(sb, block))) {
        printk("Dwarfs: Error encountered during I/O in dwarfs_getdinode for ino %llu. Possibly bad block: %llu\n", ino, block);
        return ERR_PTR(-EIO);
    }
    *bhptr = bh;
    offset = ino % (DWARFS_BLOCK_SIZE / DWARFS_SB(sb)->dwarfs_inodesize);
    return (struct dwarfs_inode *)bh->b_data + offset;
}

// Heavily based on EXT2, should probably be changed to be more original
struct inode *dwarfs_inode_get(struct super_block *sb, int64_t ino) {
    struct inode *inode = NULL;
    struct dwarfs_inode *dinode = NULL;
    struct dwarfs_inode_info *dinode_info = NULL;
    struct buffer_head *bh = NULL;
    int i;
    uid_t uid;
    gid_t gid;

    printk("Dwarfs: in dwarfs_inode_get\n");

    inode = iget_locked(sb, ino);
    if(!inode) {
        printk("Dwarfs: Failed to get inode in iget!\n");
        return ERR_PTR(-ENOMEM);
    }
    if(!(inode->i_state & I_NEW)) {// inode already exists, nothing more to do
        printk("Dwarfs: Found existing inode, returning\n");
        return inode;
    }

    pr_debug("Dwarfs: inode at ino %llu does not exist, creating new!\n", ino);
    
    /* If it doesn't exist, we need to create it */
    dinode_info = DWARFS_INODE(inode);
    dinode = dwarfs_getdinode(inode->i_sb, ino, &bh);
    if(IS_ERR(dinode)) {
        printk("DwarFS: Got a bad inode of ino: %llu\n", ino);
        return ERR_PTR(-EFSCORRUPTED);
    }

    printk("Got dinode of size: %llu\n", dinode->inode_size);

    inode->i_mode = (ino == DWARFS_ROOT_INUM ? S_IFDIR : le16_to_cpu(dinode->inode_mode)); // TODO: mkfs sets root's inode to S_IFDIR
    if(inode->i_mode == S_IFDIR)
        printk("Dwarfs: we got a directory mane\n");
    uid = (uid_t)le16_to_cpu(dinode->inode_uid_high);
    gid = (gid_t)le16_to_cpu(dinode->inode_gid_high);

    printk("write uid & gid\n");
    i_uid_write(inode, uid);
    i_gid_write(inode, gid);
    set_nlink(inode, le64_to_cpu(dinode->inode_linkc));

    printk("Getting size and times\n");
    inode->i_size = le64_to_cpu(dinode->inode_size);
    inode->i_atime.tv_sec = (signed)le64_to_cpu(dinode->inode_atime);
    inode->i_ctime.tv_sec = (signed)le64_to_cpu(dinode->inode_ctime);
    inode->i_mtime.tv_sec = (signed)le64_to_cpu(dinode->inode_mtime);
    inode->i_atime.tv_nsec = inode->i_ctime.tv_nsec = inode->i_mtime.tv_nsec = 0;
    dinode_info->inode_dtime = le32_to_cpu(dinode->inode_dtime);
    
    printk("Checking inode validity\n");
    // Now we can check validity.
    // Among other things, check if the inode is deleted.
    if(inode->i_nlink == 0 && (inode->i_mode == 0 || dinode_info->inode_dtime)) {
        printk("Dwarfs: inode is stale (deleted!) at ino: %llu\n", ino);
        return ERR_PTR(-ESTALE);
    }


    printk("Setting blocks, flags etc.\n");
    inode->i_blocks = le64_to_cpu(dinode->inode_blocks);
    dinode_info->inode_flags = le64_to_cpu(dinode->inode_flags);
    dinode_info->inode_fragaddr = le64_to_cpu(dinode->inode_fragaddr);
    dinode_info->inode_fragnum = dinode->inode_fragnum;
    dinode_info->inode_fragsize = dinode->inode_fragsize;
    
    printk("Reading inode size\n");
    if(i_size_read(inode) < 0) {
        printk("Dwarfs: Couldnt read inode size: ino %llu\n", ino);
        return ERR_PTR(-EFSCORRUPTED);
    }

    printk("Setting dinode_info dtime etc.\n");
    dinode_info->inode_dtime = 0;
    dinode_info->inode_state = 0;
//    dinode_info->inode_block_group = (ino - 1) / DWARFS_SB(inode->i_sb)->dwarfs_inodes_per_group;
    dinode_info->inode_dir_start_lookup = 0;

    printk("Setting dinode_info data blocks");
    for(i = 0; i < DWARFS_NUMBLOCKS; i++) {
        dinode_info->inode_data[i] = (dinode->inode_blocks[i] < 64 ? dinode->inode_blocks[i] : 0);
        printk("Dwarfs: inode block %d: %llu\n", i, dinode_info->inode_data[i]);
    }
    
    printk("Setting inode operations\n");
    inode->i_op = &dwarfs_dir_inode_operations;
    inode->i_fop = &dwarfs_dir_operations;
    inode->i_mapping->a_ops = &dwarfs_aops;

    printk("brelse\n");
    brelse(bh);

    printk("Unlocking the new inode\n");
    unlock_new_inode(inode);

    printk("Returning\n");
    return inode;
}

int dwarfs_get_iblock(struct inode *inode, sector_t iblock, struct buffer_head *bh_result, int create) {
	printk("Dwarfs: get_iblock\n");
    map_bh(bh_result, inode->i_sb, iblock + DWARFS_INODE(inode)->inode_data[0]); /* !!!!TODO: Use more than data block 0! */
	return 0;
}

static int dwarfs_readpage(struct file *file, struct page *page) {
    printk("Dwarfs: readpage\n");
    return mpage_readpage(page, dwarfs_get_iblock);
}

static int dwarfs_readpages(struct file *file, struct address_space *mapping, struct list_head *pages, unsigned nr_pages) {
    printk("Dwarfs: readpages\n");
    return mpage_readpages(mapping, pages, nr_pages, dwarfs_get_iblock);
}

static ssize_t dwarfs_direct_io(struct kiocb *iocb, struct iov_iter *iter) {
    struct inode *inode = file_inode(iocb->ki_filp);
    printk("Dwarfs: direct_io\n");
	return blockdev_direct_IO(iocb, inode, iter, dwarfs_get_iblock);
}


const struct address_space_operations dwarfs_aops = {
    .readpage		= dwarfs_readpage,
	.readpages		= dwarfs_readpages,
	.direct_IO      = dwarfs_direct_io,
};
