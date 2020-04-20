#ifndef __DWARFS_H__
#define __DWARFS_H__

#include <cstdint>

#define NO_FLAG 0x0 // 00000000 No special flags.
#define I_RES1 0x01 // 00000001
#define I_RES2 0x02 // 00000010
#define I_RES3 0x04 // 00000100
#define I_RES4 0x08 // 00001000
#define I_RES5 0x10 // 00010000
#define I_RES6 0x20 // 00100000
#define I_RES7 0x40 // 01000000
#define I_RES8 0x80 // 10000000

// Inode states
#define NEW_INODE 1

enum operating_systems {
    OS_LINUX = 1,
    OS_OSX = 2,
    OS_WIN = 3
};

enum inode_modes {
    I_DIR = 1,
    I_FILE = 2
};

static const uint32_t DWARFS_MAGIC = 0xDECAFBAD;
static const uint8_t DWARFS_MAX_NAME_LEN = 32;

static const int DWARFS_BLOCK_SIZE = 512; // blocksize in bytes

static const int DWARFS_SUPERBLOCK_BLOCKNUM = 0;
static const int DWARFS_INODE_BITMAP_BLOCKNUM = 1;
static const int DWARFS_DATA_BITMAP_BLOCKNUM = 2;
static const int DWARFS_FIRST_INODE_BLOCKNUM = 3;
static const int DWARFS_FIRST_DATA_BLOCKNUM = 8;
static const int DWARFS_SUPERBLOCK_PADDING = 376; /* Size was 136 without padding. Need 512 to fill block */

static const int DWARFS_NUMBLOCKS = 15; // Default number of block pointers in an inode

struct dwarfs_superblock {
    uint32_t dwarfs_magic; /* Magic number */

    /* FS structure and storage information */
    uint64_t dwarfs_blockc; /* number of blocks */
    uint64_t dwarfs_reserved_blocks; /* number of reserved blocks */
    uint64_t dwarfs_free_blocks_count; /* Number of free blocks in the volume */
    uint64_t dwarfs_free_inodes_count; /* Number of free inodes in the volume */
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
    uint64_t dwarfs_os; /* Which OS created the fs */

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
   // uint64_t inode_state;

    uint64_t inode_reserved1; /* Remove if not needed */
    uint64_t inode_blocks[DWARFS_NUMBLOCKS]; /* Pointers to data blocks */
    uint64_t inode_fragaddr; /* Fragment address */

    /* Linux thingies. Are these actually needed? Maybe remove! */
    uint8_t inode_fragnum; /* Fragment number */
    uint16_t inode_fragsize; /* Fragment size */
    uint16_t inode_padding1; /* Some padding */

    /* EXT2 has these, remove if no use is found */
    uint16_t inode_uid_high;
    uint16_t inode_gid_high;
    uint32_t inode_reserved2;

    // Padding to make size 256 (block_size divisible by sizeof(inode))
    char padding[24];
};
#endif