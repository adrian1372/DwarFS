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
#include <linux/ktime.h>
#include <linux/writeback.h>

static int dwarfs_get_numblocks(struct inode *inode, uint64_t maxblocks, ino_t ino, int create) {
    int err, i;
    int n = 0;
    struct dwarfs_inode_info *dinode_i = DWARFS_INODE(inode);
    uint64_t indirblockno;
    struct buffer_head *indirbh = NULL;
    __le64 *blocknums = NULL;

    for(i = 0; i < DWARFS_NUMBLOCKS-1; i++) {
        if(dinode_i->inode_data[i] > 0) n++;
    }
    if(dinode_i->inode_data[DWARFS_INODE_INDIR]) {
        printk("Dwarfs: checking indirect blocks\n");
        indirblockno = dinode_i->inode_data[DWARFS_INODE_INDIR];
        for( ;; ) {
            indirbh = sb_bread(inode->i_sb, indirblockno);
            if(!indirbh)
                return -EIO;
            blocknums = (__le64 *)indirbh->b_data;

            for(i = 0; i < (inode->i_sb->s_blocksize / sizeof(__le64) - 1); i++) {
                if(blocknums[i]) n++;
            }
            indirblockno = blocknums[inode->i_sb->s_blocksize / sizeof(__le64) - 1];
            brelse(indirbh);
            if(!indirblockno)
                break;
        }
    }
    return n;
}

static int __dwarfs_iwrite(struct inode *inode, bool sync) {
    struct dwarfs_inode_info *dinode_i = DWARFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh;
    struct dwarfs_inode *dinode = dwarfs_getdinode(sb, inode->i_ino, &bh);
    int i;
    uid_t uid = i_uid_read(inode);
    gid_t gid = i_gid_read(inode);

    printk("Dwarfs: iwrite\n");

    if(IS_ERR(dinode))
        return PTR_ERR(dinode);

    if(dinode_i->inode_state & DWARFS_INODE_NEW)
        memset(dinode, 0, DWARFS_SB(sb)->dwarfs_inodesize);

    dinode->inode_mode = cpu_to_le16(inode->i_mode);
    dinode->inode_uid = cpu_to_le16(fs_high2lowuid(uid));
    dinode->inode_gid = cpu_to_le16(fs_high2lowgid(gid));
    dinode->inode_linkc = cpu_to_le64(inode->i_nlink);
    dinode->inode_size = cpu_to_le64(inode->i_size);
    dinode->inode_atime = cpu_to_le64(inode->i_atime.tv_sec);
    dinode->inode_ctime = cpu_to_le64(inode->i_ctime.tv_sec);
    dinode->inode_mtime = cpu_to_le64(inode->i_mtime.tv_sec);
    dinode->inode_dtime = dinode_i->inode_dtime;
    dinode->inode_flags = dinode_i->inode_flags;
    dinode->inode_fragaddr = dinode_i->inode_fragaddr;
    dinode->inode_fragsize = dinode_i->inode_fragsize;

    // Need to check for BLOCK dev and CHAR dev.
    for(i = 0; i < DWARFS_NUMBLOCKS; i++) {
        dinode->inode_blocks[i] = dinode_i->inode_data[i];
    }
    mark_buffer_dirty(bh);
    if(sync)
        sync_dirty_buffer(bh);
    dinode_i->inode_state &= ~DWARFS_INODE_NEW;
    brelse(bh);

    return 0;
}

int dwarfs_iwrite(struct inode *inode, struct writeback_control *wbc) {
   return __dwarfs_iwrite(inode, wbc->sync_mode == WB_SYNC_ALL);
}

void dwarfs_ievict(struct inode *inode) {
    bool delete = false;
    printk("dwarfs: ievict: %lu\n", inode->i_ino);
    
    if(inode->i_nlink == 0 && !is_bad_inode(inode)) {
        dquot_initialize(inode);
        delete = true;
    }

    if(!S_ISDIR(inode->i_mode)) {
        truncate_inode_pages_final(&inode->i_data);
    }

    printk("Checking delete\n");
    if(delete) {
        printk("start_intwrite\n");
        sb_start_intwrite(inode->i_sb);
        printk("dwarfs_inode\n");
        DWARFS_INODE(inode)->inode_dtime = ktime_get_real_seconds();
        printk("mark_inode_dirty\n");
        mark_inode_dirty(inode);
        printk("iwrite\n");
        __dwarfs_iwrite(inode, inode_needs_sync(inode));
        printk("Wrote the inode\n");

        inode->i_size = 0;
        if(inode->i_blocks || DWARFS_INODE(inode)->inode_data)
            dwarfs_data_dealloc(inode->i_sb, inode);
        printk("Deallocated data\n");
    }

    printk("Invalidate_inode_buffers\n");
    invalidate_inode_buffers(inode);
    printk("clear_inode\n");
    clear_inode(inode);
    printk("Cleared the inode\n");

    if(delete) {
     //   dwarfs_ifree(inode);
        sb_end_intwrite(inode->i_sb);
    }
    printk("Done!\n");
}

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
    dinode->inode_mode = inode->i_mode;
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

    printk("dwarfs: get_ino_by_name\n");

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
  //  struct buffer_head *dirbh = NULL;
    struct super_block *sb = NULL;
    struct inode *newnode = NULL;
  //  struct dwarfs_directory_entry *newdirentry = NULL;
    struct dwarfs_inode_info *dinode_i = NULL;
    struct dwarfs_superblock_info *dfsb_i = NULL;
    struct dwarfs_superblock *dfsb = NULL;
    int64_t ino = 2;
    int err;

    printk("dwarfs: create_inode\n");

    sb = dir->i_sb;
    dfsb_i = DWARFS_SB(sb);
    dfsb = dfsb_i->dfsb;

    if(S_ISDIR(mode)) {
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
    dinode_i->inode_state = DWARFS_INODE_NEW;

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
    printk("Passed iget_locked\n");
    if(!(inode->i_state & I_NEW)) {// inode already exists, nothing more to do
        printk("Dwarfs: Found existing inode, returning\n");
        return inode;
    }
    
    /* If it doesn't exist, we need to create it */
    dinode_info = DWARFS_INODE(inode);
    dinode = dwarfs_getdinode(inode->i_sb, ino, &bh);
    if(IS_ERR(dinode)) {
        printk("DwarFS: Got a bad inode of ino: %llu\n", ino);
        return ERR_PTR(-EFSCORRUPTED);
    }

    inode->i_mode = dinode->inode_mode;
    if(S_ISDIR(inode->i_mode))
        printk("Dwarfs: we got a directory mane\n");
    if(S_ISREG(inode->i_mode)) {
        printk("Dwarfs: inode_get: regular file!\n");
    }
    uid = (uid_t)le16_to_cpu(dinode->inode_uid_high);
    gid = (gid_t)le16_to_cpu(dinode->inode_gid_high);

    i_uid_write(inode, uid);
    i_gid_write(inode, gid);
    set_nlink(inode, le64_to_cpu(dinode->inode_linkc));

    inode->i_size = le64_to_cpu(dinode->inode_size);
    inode->i_atime.tv_sec = (signed)le64_to_cpu(dinode->inode_atime);
    inode->i_ctime.tv_sec = (signed)le64_to_cpu(dinode->inode_ctime);
    inode->i_mtime.tv_sec = (signed)le64_to_cpu(dinode->inode_mtime);
    inode->i_atime.tv_nsec = inode->i_ctime.tv_nsec = inode->i_mtime.tv_nsec = 0;
    dinode_info->inode_dtime = le32_to_cpu(dinode->inode_dtime);
    
    // Now we can check validity.
    // Among other things, check if the inode is deleted.
    if(inode->i_ino == DWARFS_ROOT_INUM) {
        if(inode->i_nlink == 0) {
            set_nlink(inode, 1);
            dinode->inode_linkc = 1;
        }

    }
    if(inode->i_nlink == 0 && (inode->i_mode == 0 || dinode_info->inode_dtime)) {
        printk("Dwarfs: inode is stale (deleted!) at ino: %llu\n", ino);
        return ERR_PTR(-ESTALE);
    }

    inode->i_blocks = le64_to_cpu(dinode->inode_blockc); // Turns out this is a count and not block ptrs
    dinode_info->inode_flags = le64_to_cpu(dinode->inode_flags);
    dinode_info->inode_fragaddr = le64_to_cpu(dinode->inode_fragaddr);
    dinode_info->inode_fragnum = dinode->inode_fragnum;
    dinode_info->inode_fragsize = dinode->inode_fragsize;
    
    if(i_size_read(inode) < 0) {
        printk("Dwarfs: Couldnt read inode size: ino %llu\n", ino);
        return ERR_PTR(-EFSCORRUPTED);
    }

    dinode_info->inode_dtime = 0;
    dinode_info->inode_state = 0;
//    dinode_info->inode_block_group = (ino - 1) / DWARFS_SB(inode->i_sb)->dwarfs_inodes_per_group;
    dinode_info->inode_dir_start_lookup = 0;

    for(i = 0; i < DWARFS_NUMBLOCKS; i++) {
        dinode_info->inode_data[i] = (dinode->inode_blocks[i] < 64 ? dinode->inode_blocks[i] : 0);
    }

    if(S_ISDIR(inode->i_mode)) {
        inode->i_op = &dwarfs_dir_inode_operations;
        inode->i_fop = &dwarfs_dir_operations;
    }
    else if(S_ISREG(inode->i_mode)) {
        inode->i_op = &dwarfs_file_inode_operations;
        inode->i_fop = &dwarfs_file_operations;
    }
    else if(S_ISLNK(inode->i_mode)) { // Only fast symlinks!
        inode->i_op = &dwarfs_symlink_inode_operations;
        inode->i_link = (char *)dinode_info->inode_data;
    }
    else {
        inode->i_op = &dwarfs_special_inode_operations;
        if(dinode->inode_blocks[0]) init_special_inode(inode, inode->i_mode, old_decode_dev(le64_to_cpu(dinode->inode_blocks[0])));
        else init_special_inode(inode, inode->i_mode, new_decode_dev(le64_to_cpu(dinode->inode_blocks[1])));
    }
    inode->i_mapping->a_ops = &dwarfs_aops;

    brelse(bh);

    unlock_new_inode(inode);

    return inode;
}

// Recursion time?
__le64 dwarfs_get_indirect_blockno(struct inode *inode, sector_t offset, int create) {
    struct buffer_head *indirbh = NULL;
    struct super_block *sb = inode->i_sb;
    int depth = 1;
    int i;
    __le64 *blocknums = NULL;
    __le64 nextblock, ret;
    unsigned nextptrloc = (sb->s_blocksize / sizeof(__le64)) - 1;

    printk("Dwarfs: get_indirect_blockno: %llu\n", offset);

    while(offset > 510) { // Sorry, but the data block is in a different castle
        BUG_ON(offset > 200000);
        printk("Dwarfs: offset is too high: %llu\n", offset);
        depth++;
        offset -= nextptrloc;
    }

    nextblock = DWARFS_INODE(inode)->inode_data[DWARFS_INODE_INDIR];
    if(!nextblock) {
        if(!create)
            return -EFBIG;
        DWARFS_INODE(inode)->inode_data[DWARFS_INODE_INDIR] = dwarfs_data_alloc(sb);
        nextblock = DWARFS_INODE(inode)->inode_data[DWARFS_INODE_INDIR];
    }
    for(i = 1; i < depth; i++) { // Traverse the lists until we get to the desired depth
        indirbh = sb_bread(sb, nextblock);
        blocknums = (__le64 *)indirbh->b_data;
        nextblock = blocknums[nextptrloc];
        if(!nextblock) { // Need to allocate next list
            if(!create) {
                brelse(indirbh);
                return -EFBIG;
            }               
            blocknums[nextptrloc] = dwarfs_data_alloc(sb);
            nextblock = blocknums[nextptrloc];
        }
        blocknums = NULL;
        brelse(indirbh);
    }
    indirbh = sb_bread(sb, nextblock); // The block we actually want
    if(!indirbh)
        return -EIO;    
    blocknums = (__le64 *)indirbh->b_data;
    if(!(blocknums[offset])) {
        if(!create) {
            brelse(indirbh);
            return -EFBIG;
        }
        blocknums[offset] = dwarfs_data_alloc(sb);
    }
    ret = blocknums[offset];
    blocknums = NULL;
    brelse(indirbh);
    printk("Dwarfs: returning block: %llu at depth %d\n", ret, depth);
    return ret;
}

int dwarfs_get_iblock(struct inode *inode, sector_t iblock, struct buffer_head *bh_result, int create) {
    __le64 resultblock;
    printk("dwarfs: get_iblock: %llu\n", iblock);
    if(!create) printk("Dwarfs: !create\n");
    if(iblock < DWARFS_INODE_INDIR) { // iblock <= 13 means we're using a direct block
        if(DWARFS_INODE(inode)->inode_data[iblock] <= 0) {
            if(create)
                DWARFS_INODE(inode)->inode_data[iblock] = dwarfs_data_alloc(inode->i_sb); // TODO: allocate multiple blocks at once
            else return -EFBIG; // Might not be the right error code, but this means we're trying to write to a non-allocated sector without wanting to alloc
        }
        resultblock = DWARFS_INODE(inode)->inode_data[iblock];
    } else { // We're using indirect blocks!
        printk("%llu - %u = %llu\n", iblock, DWARFS_INODE_INDIR, iblock - (DWARFS_INODE_INDIR));
        resultblock = dwarfs_get_indirect_blockno(inode, iblock - (DWARFS_INODE_INDIR), create);
    }
    printk("Dwarfs: mapping block %llu\n", resultblock);
    map_bh(bh_result, inode->i_sb, resultblock);
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

static int dwarfs_writepage(struct page *pg, struct writeback_control *wbc) {
    printk("Dwarfs: writepage\n");
    return block_write_full_page(pg, dwarfs_get_iblock, wbc);
}

static int dwarfs_writepages(struct address_space *mapping, struct writeback_control *wbc) {
    printk("Dwarfs: writepages\n");
    return mpage_writepages(mapping, wbc, dwarfs_get_iblock);
}

static int dwarfs_write_begin(struct file *file, struct address_space *mapping, loff_t offset,
                unsigned int len, unsigned int flags, struct page **pagelist, void **fsdata) {
    printk("Dwarfs: write_begin\n");
    return block_write_begin(mapping, offset, len, flags, pagelist, dwarfs_get_iblock);
}

static int dwarfs_write_end(struct file *file, struct address_space *mapping, loff_t offset,
                unsigned int len, unsigned int copied, struct page *pg, void *fsdata) {
    printk("Dwarfs: write_end\n");
    return generic_write_end(file, mapping, offset, len, copied, pg, fsdata);
}

static sector_t dwarfs_bmap(struct address_space *mapping, sector_t block) {
    printk("Dwarfs: bmap\n");
    return generic_block_bmap(mapping, block, dwarfs_get_iblock);
}

const struct address_space_operations dwarfs_aops = {
    .readpage		= dwarfs_readpage,
	.readpages		= dwarfs_readpages,
    .writepage      = dwarfs_writepage,
    .writepages     = dwarfs_writepages,
	.direct_IO      = dwarfs_direct_io,
    .write_begin    = dwarfs_write_begin,
    .write_end      = dwarfs_write_end,
    .bmap           = dwarfs_bmap,
    .migratepage    = buffer_migrate_page,
    .is_partially_uptodate  = block_is_partially_uptodate,
    .error_remove_page      = generic_error_remove_page,
};
