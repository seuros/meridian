/**
 * \file fsw_reiserfs_disk.h
 * ReiserFS file system on-disk structures.
 */

/*
 * Copyright (c) 2006 Christoph Pfisterer
 * Portions Copyright (c) 1991-2006 by various Linux kernel contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
/**
** Modified for RefindPlus
** Copyright (c) 2024 - 2026 Dayo Akanji (sf.net/u/dakanji/profile)
**
** Modifications distributed under the preceding terms.
**/

#ifndef _FSW_REISERFS_DISK_H_
#define _FSW_REISERFS_DISK_H_

// types

typedef fsw_s8  __s8;
typedef fsw_u8  __u8;
typedef fsw_s16 __s16;
typedef fsw_u16 __u16;
typedef fsw_s32 __s32;
typedef fsw_u32 __u32;
typedef fsw_s64 __s64;
typedef fsw_u64 __u64;

typedef __u16   __le16;
typedef __u32   __le32;
typedef __u64   __le64;

#define le16_to_cpu(x) (x)
#define cpu_to_le16(x) (x)
#define le32_to_cpu(x) (x)
#define cpu_to_le32(x) (x)
#define le64_to_cpu(x) (x)
#define cpu_to_le64(x) (x)

#ifdef __GCC__
#define ATTR_PACKED __attribute__ ((__packed__))
#else
#define ATTR_PACKED
#endif


#pragma pack(push, 1)

/*
 * Disk Data Structures
 */

/***************************************************************************/
/*                             SUPER BLOCK                                 */
/***************************************************************************/

/*
 * Structure of super block on disk, a version of which in RAM is often accessed as REISERFS_SB(s)->s_rs
 * the version in RAM is part of a larger structure containing fields never written to disk.
 */
#define UNSET_HASH 0		// read_super will guess about, what hash names
		                    // in directories were sorted with
#define TEA_HASH  1
#define YURA_HASH 2
#define R5_HASH   3
#define DEFAULT_HASH R5_HASH

struct journal_params {
	__le32 jp_journal_1st_block;	/* where does journal start from on its
					                 * device */
	__le32 jp_journal_dev;	        /* journal device st_rdev */
	__le32 jp_journal_size;	        /* size of the journal */
	__le32 jp_journal_trans_max;	/* max number of blocks in a transaction. */
	__le32 jp_journal_magic;	    /* random value made on fs creation (this
					                 * was sb_journal_block_count) */
	__le32 jp_journal_max_batch;	    /* max number of blocks to batch into a
					                     * trans */
	__le32 jp_journal_max_commit_age;	/* in seconds, how old can an async
						                 * commit be */
	__le32 jp_journal_max_trans_age;	/* in seconds, how old can a transaction
						                 * be */
};

/* this is the super from 3.5.X, where X >= 10 */
struct reiserfs_super_block_v1 {
	__le32 s_block_count;	/* blocks count         */
	__le32 s_free_blocks;	/* free blocks count    */
	__le32 s_root_block;	/* root block number    */
	struct journal_params s_journal;
	__le16 s_blocksize;	            /* block size */
	__le16 s_oid_maxsize;	        /* max size of object id array, see
				                     * get_objectid() commentary  */
	__le16 s_oid_cursize;	        /* current size of object id array */
	__le16 s_umount_state;	        /* this is set to 1 when filesystem was
				                     * umounted, to 2 - when not */
	char s_magic[10];	            /* reiserfs magic string indicates that
				                     * file system is reiserfs:
				                     * "ReIsErFs" or "ReIsEr2Fs" or "ReIsEr3Fs"*/
	__le16 s_fs_state;              /* it is set to used by fsck to mark which
				                     * phase of rebuilding is done */
	__le32 s_hash_function_code;	/* indicate, what hash function is being use
					                 * to sort names in a directory*/
	__le16 s_tree_height;	        /* height of disk tree */
	__le16 s_bmap_nr;	            /* amount of bitmap blocks needed to address
				                     * each block of file system */
	__le16 s_version;	            /* this field is only reliable on filesystem
				                     * with non-standard journal */
	__le16 s_reserved_for_journal;	/* size in blocks of journal area on main
					                 * device, we need to keep after
					                 * making fs with non-standard journal */
} ATTR_PACKED;

#define SB_SIZE_V1 (sizeof (struct reiserfs_super_block_v1))

/* this is the on disk super block */
struct reiserfs_super_block {
    struct reiserfs_super_block_v1 s_v1;
    __le32 s_inode_generation;
    __le32 s_flags;    /* Right now used only by inode-attributes, if enabled */
    unsigned char s_uuid[16];   /* filesystem unique identifier */
    unsigned char s_label[16];  /* filesystem volume label */
    char s_unused[88];          /* zero filled by mkreiserfs and
                                 * reiserfs_convert_objectid_map_v1()
                                 * so any additions must be updated
                                 * there as well. */
} ATTR_PACKED;

#define SB_SIZE (sizeof (struct reiserfs_super_block))

#define REISERFS_VERSION_1 0
#define REISERFS_VERSION_2 2

// on-disk super block fields converted to cpu form
#define SB_DISK_SUPER_BLOCK(s) (REISERFS_SB(s)->s_rs)
#define SB_V1_DISK_SUPER_BLOCK(s) (&(SB_DISK_SUPER_BLOCK(s)->s_v1))
#define SB_BLOCKSIZE(s) \
        le32_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_blocksize))
#define SB_BLOCK_COUNT(s) \
        le32_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_block_count))
#define SB_FREE_BLOCKS(s) \
        le32_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_free_blocks))
#define SB_REISERFS_MAGIC(s) \
        (SB_V1_DISK_SUPER_BLOCK(s)->s_magic)
#define SB_ROOT_BLOCK(s) \
        le32_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_root_block))
#define SB_TREE_HEIGHT(s) \
        le16_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_tree_height))
#define SB_REISERFS_STATE(s) \
        le16_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_umount_state))
#define SB_VERSION(s) le16_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_version))
#define SB_BMAP_NR(s) le16_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_bmap_nr))

#define SB_ONDISK_JP(s) (&SB_V1_DISK_SUPER_BLOCK(s)->s_journal)
#define SB_ONDISK_JOURNAL_SIZE(s) \
         le32_to_cpu ((SB_ONDISK_JP(s)->jp_journal_size))
#define SB_ONDISK_JOURNAL_1st_BLOCK(s) \
         le32_to_cpu ((SB_ONDISK_JP(s)->jp_journal_1st_block))
#define SB_ONDISK_JOURNAL_DEVICE(s) \
         le32_to_cpu ((SB_ONDISK_JP(s)->jp_journal_dev))
#define SB_ONDISK_RESERVED_FOR_JOURNAL(s) \
         le16_to_cpu ((SB_V1_DISK_SUPER_BLOCK(s)->s_reserved_for_journal))

#define is_block_in_log_or_reserved_area(s, block) \
         block >= SB_JOURNAL_1st_RESERVED_BLOCK(s) \
         && block < SB_JOURNAL_1st_RESERVED_BLOCK(s) +  \
         ((!is_reiserfs_jr(SB_DISK_SUPER_BLOCK(s)) ? \
         SB_ONDISK_JOURNAL_SIZE(s) + 1 : SB_ONDISK_RESERVED_FOR_JOURNAL(s)))

				/* used by gcc */
#define REISERFS_SUPER_MAGIC 0x52654973
				/* used by file system utilities that
				   look at the superblock, etc. */
#define REISERFS_SUPER_MAGIC_STRING "ReIsErFs"
#define REISER2FS_SUPER_MAGIC_STRING "ReIsEr2Fs"
#define REISER2FS_JR_SUPER_MAGIC_STRING "ReIsEr3Fs"

/* ReiserFS leaves the first 64k unused, so that partition labels have
   enough space.  If someone wants to write a fancy bootloader that
   needs more than 64k, let us know, and this will be increased in size.
   This number must be larger than than the largest block size on any
   platform, or code will break.  -Hans */
#define REISERFS_DISK_OFFSET_IN_BYTES (64 * 1024)
#define REISERFS_FIRST_BLOCK unused_define
#define REISERFS_JOURNAL_OFFSET_IN_BYTES REISERFS_DISK_OFFSET_IN_BYTES

/* the spot for the super in versions 3.5 - 3.5.10 (inclusive) */
#define REISERFS_OLD_DISK_OFFSET_IN_BYTES (8 * 1024)

// reiserfs internal error code (used by search_by_key adn fix_nodes))
#define CARRY_ON                          (0)
#define REPEAT_SEARCH                    (-1)
#define IO_ERROR                         (-2)
#define NO_DISK_SPACE                    (-3)
#define NO_BALANCING_NEEDED              (-4)
#define NO_MORE_UNUSED_CONTIGUOUS_BLOCKS (-5)
#define QUOTA_EXCEEDED                   (-6)

typedef __u32 b_blocknr_t;
typedef __le32 unp_t;

struct unfm_nodeinfo {
	unp_t unfm_nodenum;
	unsigned short unfm_freespace;
};

/* there are two formats of keys: 3.5 and 3.6
 */
#define KEY_FORMAT_3_5 0
#define KEY_FORMAT_3_6 1

/* there are two stat datas */
#define STAT_DATA_V1 0
#define STAT_DATA_V2 1



/** this says about version of key of all items (but stat data) the
    object consists of */
#define get_inode_item_key_version( inode )                                    \
    ((REISERFS_I(inode)->i_flags & i_item_key_version_mask) ? KEY_FORMAT_3_6 : KEY_FORMAT_3_5)

#define set_inode_item_key_version( inode, version )                           \
         ({ if ((version)==KEY_FORMAT_3_6)                                     \
                REISERFS_I(inode)->i_flags |= i_item_key_version_mask;         \
            else                                                               \
                REISERFS_I(inode)->i_flags &= ~i_item_key_version_mask; })

#define get_inode_sd_version(inode)                                            \
    ((REISERFS_I(inode)->i_flags & i_stat_data_version_mask) ? STAT_DATA_V2 : STAT_DATA_V1)

#define set_inode_sd_version(inode, version)                                   \
         ({ if ((version)==STAT_DATA_V2)                                       \
                REISERFS_I(inode)->i_flags |= i_stat_data_version_mask;        \
            else                                                               \
                REISERFS_I(inode)->i_flags &= ~i_stat_data_version_mask; })

/*
 * values for s_umount_state field
 */
#define REISERFS_VALID_FS    1
#define REISERFS_ERROR_FS    2

//
// there are 5 item types currently
//
#define TYPE_STAT_DATA  0
#define TYPE_INDIRECT   1
#define TYPE_DIRECT     2
#define TYPE_DIRENTRY   3
#define TYPE_MAXTYPE    3
#define TYPE_ANY       15 // FIXME: comment is required

/***************************************************************************/
/*                       KEY & ITEM HEAD                                   */
/***************************************************************************/

//
// directories use this key as well as old files
//
struct offset_v1 {
	__le32 k_offset;
	__le32 k_uniqueness;
} ATTR_PACKED;

struct offset_v2 {
	__le64 v;
} ATTR_PACKED;

/* Key of an item determines its location in the S+tree, and
   is composed of 4 components */
struct reiserfs_key {
	__le32 k_dir_id;	/* packing locality: by default parent
                         * directory object id */
	__le32 k_objectid;	/* object identifier */
	union {
		struct offset_v1 k_offset_v1;
		struct offset_v2 k_offset_v2;
	} ATTR_PACKED u;
} ATTR_PACKED;

struct in_core_key {
	__u32 k_dir_id;		/* packing locality: by default parent
                         * directory object id */
	__u32 k_objectid;	/* object identifier */
	__u64 k_offset;
	__u8 k_type;
};

struct cpu_key {
	struct in_core_key on_disk_key;
	int version;
	int key_length;		/* 3 in all cases but direct2indirect and
                         * indirect2direct conversion */
};

/* Our function for comparing keys can compare keys of different
   lengths.  It takes as a parameter the length of the keys it is to
   compare.  These defines are used in determining what is to be passed
   to it as that parameter. */
#define REISERFS_FULL_KEY_LEN     4
#define REISERFS_SHORT_KEY_LEN    2

/* The result of the key compare */
#define FIRST_GREATER             1
#define KEYS_IDENTICAL            0
#define SECOND_GREATER           -1

#define KEY_FOUND                 1
#define KEY_NOT_FOUND             0

#define KEY_SIZE (sizeof (struct reiserfs_key))
#define SHORT_KEY_SIZE (sizeof (__u32) + sizeof (__u32))

/* return values for search_by_key and clones */
#define ITEM_FOUND                1
#define ITEM_NOT_FOUND            0

#define ENTRY_FOUND               1
#define ENTRY_NOT_FOUND           0

#define DIRECTORY_NOT_FOUND      -1
#define REGULAR_FILE_FOUND       -2
#define DIRECTORY_FOUND          -3

#define BYTE_FOUND                1
#define BYTE_NOT_FOUND            0
#define FILE_NOT_FOUND           -1

#define POSITION_FOUND            1
#define POSITION_NOT_FOUND        0

// return values for reiserfs_find_entry and search_by_entry_key
#define NAME_NOT_FOUND            0
#define NAME_FOUND                1
#define GOTO_PREVIOUS_ITEM        2
#define NAME_FOUND_INVISIBLE      3

/*  Everything in the filesystem is stored as a set of items.  The
    item head contains the key of the item, its free space (for
    indirect items) and specifies the location of the item itself
    within the block.  */

struct item_head {
	/* Everything in the tree is found by searching for it based on
	 * its key.*/
	struct reiserfs_key ih_key;
	union {
		/* The free space in the last unformatted node of an
		   indirect item if this is an indirect item.  This
		   equals 0xFFFF iff this is a direct item or stat data
		   item. Note that the key, not this field, is used to
		   determine the item type, and thus which field this
		   union contains. */
		__le16 ih_free_space_reserved;
		/* If this is a directory item, this field equals the
		   number of directory entries in the directory item. */
		__le16 ih_entry_count;
	} ATTR_PACKED u;
	__le16 ih_item_len;	        /* total size of the item body */
	__le16 ih_item_location;	/* an offset to the item body
					             * within the block */
	__le16 ih_version;	        /* 0 for all old items, 2 for new
                                 * ones. Highest bit is set by fsck
                                 * temporary, cleaned after all done */
} ATTR_PACKED;
/* size of item header     */
#define IH_SIZE (sizeof (struct item_head))

#define ih_free_space(ih)            le16_to_cpu((ih)->u.ih_free_space_reserved)
#define ih_version(ih)               le16_to_cpu((ih)->ih_version)
#define ih_entry_count(ih)           le16_to_cpu((ih)->u.ih_entry_count)
#define ih_location(ih)              le16_to_cpu((ih)->ih_item_location)
#define ih_item_len(ih)              le16_to_cpu((ih)->ih_item_len)

#define unreachable_item(ih) (ih_version(ih) & (1 << 15))

#define get_ih_free_space(ih) (ih_version (ih) == KEY_FORMAT_3_6 ? 0 : ih_free_space (ih))

/* these operate on indirect items, where you've got an array of ints
** at a possibly unaligned location.  These are a noop on ia32
**
** p is the array of __u32, i is the index into the array, v is the value
** to store there.
*/
#define get_block_num(p, i) le32_to_cpu(get_unaligned((p) + (i)))

//
// in old version uniqueness field shows key type
//
#define V1_SD_UNIQUENESS 0
#define V1_INDIRECT_UNIQUENESS 0xfffffffe
#define V1_DIRECT_UNIQUENESS 0xffffffff
#define V1_DIRENTRY_UNIQUENESS 500
#define V1_ANY_UNIQUENESS 555	// FIXME: comment is required

#define is_direntry_le_key(version,key) (le_key_k_type (version, key) == TYPE_DIRENTRY)
#define is_direct_le_key(version,key) (le_key_k_type (version, key) == TYPE_DIRECT)
#define is_indirect_le_key(version,key) (le_key_k_type (version, key) == TYPE_INDIRECT)
#define is_statdata_le_key(version,key) (le_key_k_type (version, key) == TYPE_STAT_DATA)

//
// item header has version.
//
#define is_direntry_le_ih(ih) is_direntry_le_key (ih_version (ih), &((ih)->ih_key))
#define is_direct_le_ih(ih) is_direct_le_key (ih_version (ih), &((ih)->ih_key))
#define is_indirect_le_ih(ih) is_indirect_le_key (ih_version(ih), &((ih)->ih_key))
#define is_statdata_le_ih(ih) is_statdata_le_key (ih_version (ih), &((ih)->ih_key))

#define is_direntry_cpu_key(key) (cpu_key_k_type (key) == TYPE_DIRENTRY)
#define is_direct_cpu_key(key) (cpu_key_k_type (key) == TYPE_DIRECT)
#define is_indirect_cpu_key(key) (cpu_key_k_type (key) == TYPE_INDIRECT)
#define is_statdata_cpu_key(key) (cpu_key_k_type (key) == TYPE_STAT_DATA)

/* are these used ? */
#define is_direntry_cpu_ih(ih) (is_direntry_cpu_key (&((ih)->ih_key)))
#define is_direct_cpu_ih(ih) (is_direct_cpu_key (&((ih)->ih_key)))
#define is_indirect_cpu_ih(ih) (is_indirect_cpu_key (&((ih)->ih_key)))
#define is_statdata_cpu_ih(ih) (is_statdata_cpu_key (&((ih)->ih_key)))

#define I_K_KEY_IN_ITEM(p_s_ih, p_s_key, n_blocksize) \
    ( ! COMP_SHORT_KEYS(p_s_ih, p_s_key) && \
          I_OFF_BYTE_IN_ITEM(p_s_ih, k_offset (p_s_key), n_blocksize) )

/* maximal length of item */
#define MAX_ITEM_LEN(block_size) (block_size - BLKH_SIZE - IH_SIZE)
#define MIN_ITEM_LEN 1

/* object identifier for root dir */
#define REISERFS_ROOT_OBJECTID 2
#define REISERFS_ROOT_PARENT_OBJECTID 1

/*
 * Picture represents a leaf of the S+tree
 *  ______________________________________________________
 * |      |  Array of     |                   |           |
 * |Block |  Object-Item  |      F r e e      |  Objects- |
 * | head |  Headers      |     S p a c e     |   Items   |
 * |______|_______________|___________________|___________|
 */

/* Header of a disk block.  More precisely, header of a formatted leaf
   or internal node, and not the header of an unformatted node. */
struct block_head {
	__le16 blk_level;	    /* Level of a block in the tree. */
	__le16 blk_nr_item;	    /* Number of keys/items in a block. */
	__le16 blk_free_space;	/* Block free space in bytes. */
	__le16 blk_reserved;
	/* dump this in v4/planA */
	struct reiserfs_key blk_right_delim_key;	/* kept only for compatibility */
};

#define BLKH_SIZE                     (sizeof (struct block_head))
#define blkh_level(p_blkh)            (le16_to_cpu((p_blkh)->blk_level))
#define blkh_nr_item(p_blkh)          (le16_to_cpu((p_blkh)->blk_nr_item))
#define blkh_free_space(p_blkh)       (le16_to_cpu((p_blkh)->blk_free_space))
#define blkh_reserved(p_blkh)         (le16_to_cpu((p_blkh)->blk_reserved))
#define blkh_right_delim_key(p_blkh)  ((p_blkh)->blk_right_delim_key)

/*
 * values for blk_level field of the struct block_head
 */

#define FREE_LEVEL 0		/* when node gets removed from the tree its
				   blk_level is set to FREE_LEVEL. It is then
				   used to see whether the node is still in the
				   tree */

#define DISK_LEAF_NODE_LEVEL  1	/* Leaf node level. */

/* Given the buffer head of a formatted node, resolve to the block head of that node. */
#define B_BLK_HEAD(p_s_bh)            ((struct block_head *)((p_s_bh)->b_data))
/* Number of items that are in buffer. */
#define B_NR_ITEMS(p_s_bh)            (blkh_nr_item(B_BLK_HEAD(p_s_bh)))
#define B_LEVEL(p_s_bh)               (blkh_level(B_BLK_HEAD(p_s_bh)))
#define B_FREE_SPACE(p_s_bh)          (blkh_free_space(B_BLK_HEAD(p_s_bh)))

/* Get right delimiting key. -- little endian */
#define B_PRIGHT_DELIM_KEY(p_s_bh)   (&(blk_right_delim_key(B_BLK_HEAD(p_s_bh))

/* Does the buffer contain a disk leaf. */
#define B_IS_ITEMS_LEVEL(p_s_bh)     (B_LEVEL(p_s_bh) == DISK_LEAF_NODE_LEVEL)

/* Does the buffer contain a disk internal node */
#define B_IS_KEYS_LEVEL(p_s_bh)      (B_LEVEL(p_s_bh) > DISK_LEAF_NODE_LEVEL \
                                            && B_LEVEL(p_s_bh) <= MAX_HEIGHT)

/***************************************************************************/
/*                             STAT DATA                                   */
/***************************************************************************/

//
// old stat data is 32 bytes long. We are going to distinguish new one by
// different size
//
struct stat_data_v1 {
	__le16 sd_mode;		/* file type, permissions */
	__le16 sd_nlink;	/* number of hard links */
	__le16 sd_uid;		/* owner */
	__le16 sd_gid;		/* group */
	__le32 sd_size;		/* file size */
	__le32 sd_atime;	/* time of last access */
	__le32 sd_mtime;	/* time file was last modified  */
	__le32 sd_ctime;	/* time inode (stat data) was last changed (except changes to sd_atime and sd_mtime) */
	union {
		__le32 sd_rdev;
		__le32 sd_blocks;	/* number of blocks file uses */
	} ATTR_PACKED u;
	__le32 sd_first_direct_byte;	/* first byte of file which is stored
					   in a direct item: except that if it
					   equals 1 it is a symlink and if it
					   equals ~(__u32)0 there is no
					   direct item.  The existence of this
					   field really grates on me. Let's
					   replace it with a macro based on
					   sd_size and our tail suppression
					   policy.  Someday.  -Hans */
} ATTR_PACKED;

#define SD_V1_SIZE              (sizeof (struct stat_data_v1))
#define stat_data_v1(ih)        (ih_version (ih) == KEY_FORMAT_3_5)
#define sd_v1_mode(sdp)         (le16_to_cpu((sdp)->sd_mode))
#define set_sd_v1_mode(sdp,v)   ((sdp)->sd_mode = cpu_to_le16(v))
#define sd_v1_nlink(sdp)        (le16_to_cpu((sdp)->sd_nlink))
#define set_sd_v1_nlink(sdp,v)  ((sdp)->sd_nlink = cpu_to_le16(v))
#define sd_v1_uid(sdp)          (le16_to_cpu((sdp)->sd_uid))
#define set_sd_v1_uid(sdp,v)    ((sdp)->sd_uid = cpu_to_le16(v))
#define sd_v1_gid(sdp)          (le16_to_cpu((sdp)->sd_gid))
#define set_sd_v1_gid(sdp,v)    ((sdp)->sd_gid = cpu_to_le16(v))
#define sd_v1_size(sdp)         (le32_to_cpu((sdp)->sd_size))
#define set_sd_v1_size(sdp,v)   ((sdp)->sd_size = cpu_to_le32(v))
#define sd_v1_atime(sdp)        (le32_to_cpu((sdp)->sd_atime))
#define set_sd_v1_atime(sdp,v)  ((sdp)->sd_atime = cpu_to_le32(v))
#define sd_v1_mtime(sdp)        (le32_to_cpu((sdp)->sd_mtime))
#define set_sd_v1_mtime(sdp,v)  ((sdp)->sd_mtime = cpu_to_le32(v))
#define sd_v1_ctime(sdp)        (le32_to_cpu((sdp)->sd_ctime))
#define set_sd_v1_ctime(sdp,v)  ((sdp)->sd_ctime = cpu_to_le32(v))
#define sd_v1_rdev(sdp)         (le32_to_cpu((sdp)->u.sd_rdev))
#define set_sd_v1_rdev(sdp,v)   ((sdp)->u.sd_rdev = cpu_to_le32(v))
#define sd_v1_blocks(sdp)       (le32_to_cpu((sdp)->u.sd_blocks))
#define set_sd_v1_blocks(sdp,v) ((sdp)->u.sd_blocks = cpu_to_le32(v))
#define sd_v1_first_direct_byte(sdp) \
                                (le32_to_cpu((sdp)->sd_first_direct_byte))
#define set_sd_v1_first_direct_byte(sdp,v) \
                                ((sdp)->sd_first_direct_byte = cpu_to_le32(v))

/*
#include <linux/ext2_fs.h>
*/

/* inode flags stored in sd_attrs (nee sd_reserved) */

/* we want common flags to have the same values as in ext2,
   so chattr(1) will work without problems */
#define REISERFS_IMMUTABLE_FL EXT2_IMMUTABLE_FL
#define REISERFS_APPEND_FL    EXT2_APPEND_FL
#define REISERFS_SYNC_FL      EXT2_SYNC_FL
#define REISERFS_NOATIME_FL   EXT2_NOATIME_FL
#define REISERFS_NODUMP_FL    EXT2_NODUMP_FL
#define REISERFS_SECRM_FL     EXT2_SECRM_FL
#define REISERFS_UNRM_FL      EXT2_UNRM_FL
#define REISERFS_COMPR_FL     EXT2_COMPR_FL
#define REISERFS_NOTAIL_FL    EXT2_NOTAIL_FL

/* persistent flags that file inherits from the parent directory */
#define REISERFS_INHERIT_MASK ( REISERFS_IMMUTABLE_FL |	\
				REISERFS_SYNC_FL |	\
				REISERFS_NOATIME_FL |	\
				REISERFS_NODUMP_FL |	\
				REISERFS_SECRM_FL |	\
				REISERFS_COMPR_FL |	\
				REISERFS_NOTAIL_FL )

/* Stat Data on disk (reiserfs version of UFS disk inode minus the
   address blocks) */
struct stat_data {
	__le16 sd_mode;		/* file type, permissions */
	__le16 sd_attrs;	/* persistent inode flags */
	__le32 sd_nlink;	/* number of hard links */
	__le64 sd_size;		/* file size */
	__le32 sd_uid;		/* owner */
	__le32 sd_gid;		/* group */
	__le32 sd_atime;	/* time of last access */
	__le32 sd_mtime;	/* time file was last modified  */
	__le32 sd_ctime;	/* time inode (stat data) was last changed (except changes to sd_atime and sd_mtime) */
	__le32 sd_blocks;
	union {
		__le32 sd_rdev;
		__le32 sd_generation;
		//__le32 sd_first_direct_byte;
		/* first byte of file which is stored in a
		   direct item: except that if it equals 1
		   it is a symlink and if it equals
		   ~(__u32)0 there is no direct item.  The
		   existence of this field really grates
		   on me. Let's replace it with a macro
		   based on sd_size and our tail
		   suppression policy? */
	} ATTR_PACKED u;
} ATTR_PACKED;
//
// this is 44 bytes long
//
#define SD_SIZE (sizeof (struct stat_data))
#define SD_V2_SIZE              SD_SIZE
#define stat_data_v2(ih)        (ih_version (ih) == KEY_FORMAT_3_6)
#define sd_v2_mode(sdp)         (le16_to_cpu((sdp)->sd_mode))
#define set_sd_v2_mode(sdp,v)   ((sdp)->sd_mode = cpu_to_le16(v))
/* sd_reserved */
/* set_sd_reserved */
#define sd_v2_nlink(sdp)        (le32_to_cpu((sdp)->sd_nlink))
#define set_sd_v2_nlink(sdp,v)  ((sdp)->sd_nlink = cpu_to_le32(v))
#define sd_v2_size(sdp)         (le64_to_cpu((sdp)->sd_size))
#define set_sd_v2_size(sdp,v)   ((sdp)->sd_size = cpu_to_le64(v))
#define sd_v2_uid(sdp)          (le32_to_cpu((sdp)->sd_uid))
#define set_sd_v2_uid(sdp,v)    ((sdp)->sd_uid = cpu_to_le32(v))
#define sd_v2_gid(sdp)          (le32_to_cpu((sdp)->sd_gid))
#define set_sd_v2_gid(sdp,v)    ((sdp)->sd_gid = cpu_to_le32(v))
#define sd_v2_atime(sdp)        (le32_to_cpu((sdp)->sd_atime))
#define set_sd_v2_atime(sdp,v)  ((sdp)->sd_atime = cpu_to_le32(v))
#define sd_v2_mtime(sdp)        (le32_to_cpu((sdp)->sd_mtime))
#define set_sd_v2_mtime(sdp,v)  ((sdp)->sd_mtime = cpu_to_le32(v))
#define sd_v2_ctime(sdp)        (le32_to_cpu((sdp)->sd_ctime))
#define set_sd_v2_ctime(sdp,v)  ((sdp)->sd_ctime = cpu_to_le32(v))
#define sd_v2_blocks(sdp)       (le32_to_cpu((sdp)->sd_blocks))
#define set_sd_v2_blocks(sdp,v) ((sdp)->sd_blocks = cpu_to_le32(v))
#define sd_v2_rdev(sdp)         (le32_to_cpu((sdp)->u.sd_rdev))
#define set_sd_v2_rdev(sdp,v)   ((sdp)->u.sd_rdev = cpu_to_le32(v))
#define sd_v2_generation(sdp)   (le32_to_cpu((sdp)->u.sd_generation))
#define set_sd_v2_generation(sdp,v) ((sdp)->u.sd_generation = cpu_to_le32(v))
#define sd_v2_attrs(sdp)         (le16_to_cpu((sdp)->sd_attrs))
#define set_sd_v2_attrs(sdp,v)   ((sdp)->sd_attrs = cpu_to_le16(v))

/***************************************************************************/
/*                      DIRECTORY STRUCTURE                                */
/***************************************************************************/
/*
   Picture represents the structure of directory items
   ________________________________________________
   |  Array of     |   |     |        |       |   |
   | directory     |N-1| N-2 | ....   |   1st |0th|
   | entry headers |   |     |        |       |   |
   |_______________|___|_____|________|_______|___|
                    <----   directory entries         ------>

 First directory item has k_offset component 1. We store "." and ".."
 in one item, always, we never split "." and ".." into differing
 items.  This makes, among other things, the code for removing
 directories simpler. */
#define SD_OFFSET  0
#define SD_UNIQUENESS 0
#define DOT_OFFSET 1
#define DOT_DOT_OFFSET 2
#define DIRENTRY_UNIQUENESS 500

#define FIRST_ITEM_OFFSET 1

/*
   Q: How to get key of object pointed to by entry from entry?

   A: Each directory entry has its header. This header has deh_dir_id and deh_objectid fields, those are key
      of object, entry points to */

/* NOT IMPLEMENTED:
   Directory will someday contain stat data of object */

struct reiserfs_de_head {
	__le32 deh_offset;	/* third component of the directory entry key */
	__le32 deh_dir_id;	/* objectid of the parent directory of the object, that is referenced
				   by directory entry */
	__le32 deh_objectid;	/* objectid of the object, that is referenced by directory entry */
	__le16 deh_location;	/* offset of name in the whole item */
	__le16 deh_state;	/* whether 1) entry contains stat data (for future), and 2) whether
				   entry is hidden (unlinked) */
} ATTR_PACKED;

/*
 * Picture represents an internal node of the reiserfs tree
 *  ______________________________________________________
 * |      |  Array of     |  Array of         |  Free     |
 * |block |    keys       |  pointers         | space     |
 * | head |      N        |      N+1          |           |
 * |______|_______________|___________________|___________|
 */

/***************************************************************************/
/*                      DISK CHILD                                         */
/***************************************************************************/
/* Disk child pointer: The pointer from an internal node of the tree
   to a node that is on disk. */
struct disk_child {
	__le32 dc_block_number;	/* Disk child's block number. */
	__le16 dc_size;		/* Disk child's used space.   */
	__le16 dc_reserved;
};

#define MAX_HEIGHT 5		/* maximal height of a tree. do not change this without changing JOURNAL_PER_BALANCE_CNT */


#pragma pack(pop)
#endif
