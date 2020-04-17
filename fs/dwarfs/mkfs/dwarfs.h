#ifndef __DWARFS_H__
#define __DWARFS_H__

#include <cstdint>

static const uint32_t DWARFS_MAGIC = 0xDECAFBAD;
static const uint8_t DWARFS_MAX_NAME_LEN = 32;

static const int DWARFS_BLOCK_SIZE = 512; // blocksize in bytes

static const int DWARFS_SUPERBLOCK_BLOCKNUM = 0;
static const int DWARFS_INODE_BITMAP_BLOCKNUM = 1;
static const int DWARFS_DATA_BITMAP_BLOCKNUM = 2;
static const int DWARFS_FIRST_INODE_BLOCKNUM = 3;
static const int DWARFS_FIRST_DATA_BLOCKNUM = 8;
static const int DWARFS_SUPERBLOCK_PADDING = 376; /* Size was 136 without padding. Need 512 */

static const int DWARFS_NUMBLOCKS = 15; // Default number of block pointers in an inode

struct dwarfs_superblock {
    uint32_t dwarfs_magic; /* Magic number */

    /* FS structure and storage information */
    uint64_t dwarfs_blockc; /* number of blocks */
    uint64_t dwarfs_reserved_blocks; /* number of reserved blocks */
    uint64_t dwarfs_free_blocks_count; /* Number of free blocks in the volume */
    uint64_t dwarfs_free_inodes_count; /* Number of free inodes in the volume (let's aim to never let this be zero) */
    uint64_t dwarfs_data_start_block; /* Block at which data storage starts */
    uint64_t dwarfs_block_size; /* Size of disk blocks */
    uint64_t dwarfs_root_inode; /* root inode */
    uint64_t dwarfs_inodec; /* Number of inodes */
    uint64_t dwarfs_blocks_per_group; /* Number of blocks in a disk group */
    uint64_t dwarfs_frags_per_group; /* Number of fragments in a disk group */
    uint64_t dwarfs_inodes_per_group; /* Number of inodes in a disk group */

    /* Time data */
    uint64_t dwarfs_wtime; /* Time of last write */
    uint64_t dwarfs_mtime; /* Time of last mount */

    /* General FS & volume information */
    uint16_t dwarfs_def_resuid; /* Default user ID for reserved blocks */
    uint16_t dwarfs_def_resgid; /* Default group ID for reserved blocks */
    uint64_t dwarfs_version_num; /* Versions might not matter ... backwards/forwards compatibility???? */
    uint64_t dwarfs_os; /* Which OS created the fs (Can probably be deleted, no one outside Linux would use this) */

    /* Add padding to fill the block? */
    // Answer is yes!
    char padding[376];
};

struct dwarfs_inode {
    uint16_t inode_mode; /* Dir, file, etc. */
    uint64_t inode_size; /* Size of the iNode */
    
    /* Owner */
    uint16_t inode_uid; /* Owner user ID */
    uint16_t inode_gid; /* iNode Group ID */

    /* Time Management */
    uint64_t inode_atime; /* Time last accessed */
    uint64_t inode_ctime; /* Time of creation */
    uint64_t inode_mtime; /* Time of last modification */
    uint64_t inode_dtime; /* Time deleted */

    /* Disk info */
    uint64_t inode_blockc; /* Number of used blocks */
    uint64_t inode_linkc; /* Number of links */
    uint64_t inode_flags; /* File flags (Remove this if no flags get implemented!) */

    uint64_t inode_reserved1; /* Not sure what this is, gotten from ext2. REMOVE IF NOT USED */
    uint64_t inode_blocks[DWARFS_NUMBLOCKS]; /* Pointers to data blocks */
    uint64_t inode_fragaddr; /* Fragment address */

    /* Linux thingies. Are these actually needed? Maybe remove! */
    uint8_t inode_fragnum; /* Fragment number */
    uint16_t inode_fragsize; /* Fragment size */
    uint16_t inode_padding1; /* Some padding */

    /* Not sure what any of these are for. Gotten from EXT2. */
    uint16_t inode_uid_high;
    uint16_t inode_gid_high;
    uint32_t inode_reserved2;
};
#endif