/*
 * Copyright (c) 1998-2017 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2017 Stony Brook University
 * Copyright (c) 2003-2017 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _WRAPFS_H_
#define _WRAPFS_H_

#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/seq_file.h>
#include <linux/statfs.h>
#include <linux/fs_stack.h>
#include <linux/magic.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/xattr.h>
#include <linux/exportfs.h>
#include <linux/crc32.h>
#include <linux/sort.h>

/* the file system name */
#define WRAPFS_NAME "KWAL"

/* wrapfs root inode number */
#define WRAPFS_ROOT_INO     1

/* useful for tracking code reachability */
#define UDBG printk(KERN_DEFAULT "DBG:%s:%s:%d\n", __FILE__, __func__, __LINE__)

/* operations vectors defined in specific files */
extern const struct file_operations wrapfs_main_fops;
extern const struct file_operations wrapfs_dir_fops;
extern const struct inode_operations wrapfs_main_iops;
extern const struct inode_operations wrapfs_dir_iops;
extern const struct inode_operations wrapfs_symlink_iops;
extern const struct super_operations wrapfs_sops;
extern const struct dentry_operations wrapfs_dops;
extern const struct address_space_operations wrapfs_aops, wrapfs_dummy_aops;
extern const struct vm_operations_struct wrapfs_vm_ops;
extern const struct export_operations wrapfs_export_ops;
extern const struct xattr_handler *wrapfs_xattr_handlers[];

extern int wrapfs_init_inode_cache(void);
extern void wrapfs_destroy_inode_cache(void);
extern int wrapfs_init_dentry_cache(void);
extern void wrapfs_destroy_dentry_cache(void);
extern int new_dentry_private_data(struct dentry *dentry);
extern void free_dentry_private_data(struct dentry *dentry);
extern struct dentry *wrapfs_lookup(struct inode *dir, struct dentry *dentry,
				    unsigned int flags);
extern struct inode *wrapfs_iget(struct super_block *sb,
				 struct inode *lower_inode);
extern int wrapfs_interpose(struct dentry *dentry, struct super_block *sb,
			    struct path *lower_path);

#include <linux/hashtable.h>
//#define KWAL_HASH_BITS (16)
#define KWAL_HASH_BITS (8)

#define KWAL_NUM  (4)

#define KiB(x)	( (x) * 1024L )
#define MiB(x)	( KiB(x) * 1024L )

#define KWAL_SIZE	(MiB(256))
//#define KWAL_SIZE	(MiB(64))
#define GUARD_SIZE	(MiB(10))

//#define INDEX_BG_LEN	(MiB(128)/KiB(4))


//#define COMMIT_LAT
//#define KWAL_BUG

//#define F2FS_REMAP
#define EXT4_EXT_REMAP
//#define EXT4_IND_REMAP
//#define COPY_CP
//#define SEL_REMAP
//#define CORSE_SEL

//#define SEQ_META
//#define NO_META

#define FINEWC_DETECT
#define RR_ISO

#define REMAP_VALIDITY (60)
#define FORCE_REMAP_VALIDITY (200)
#define FORCE_COPY_VALIDITY (60)

//#define MIN_REMAP (4)
//#define MIN_REMAP (16)
//#define MIN_REMAP (64)
#define MIN_REMAP (256)

//#define WARPFS_DEBUG

#ifdef WARPFS_DEBUG
#define wrapfs_debug(a...)						\
	do {								\
		trace_printk(a);				\
	} while (0)
#else
#define wrapfs_debug(fmt, ...)	no_printk(fmt, ##__VA_ARGS__)
#endif



struct redirection_entry {
    struct hlist_node node;
	struct list_head next_entry; // cl->dirty_listhead or filp->traversing_listhead
	struct list_head next_version; // linked list for duplicated entries entry->next_version
	struct list_head neighbour; // cl->kwal_list[KWAL_NUM]
	struct list_head gc_entry; // target_ct->gc_list
	struct list_head next_active; // cl->active_entry_head
	int dirt;
	int valid;
	unsigned long org_block;
	unsigned long new_block;
	int endt;
	u32 cs;
#ifdef FINEWC_DETECT
	int is_conflict;
	struct staging_entry *p_staging;
#endif
};

#ifdef FINEWC_DETECT
struct conflict_entry {
	struct hlist_node conflict_node; // cl->conflict_tree
	unsigned long org_block;
	u64 dirtybitmap; // need spinlock
};

struct staging_entry {
	struct hlist_node staging_node; // cl->staging_tree
	unsigned long org_block;
	unsigned long new_block;
	struct redirection_entry *p_redentry;
	u64 dirtybitmap;
};
#endif

struct commit_tree_list {
	struct inode* inode;
	unsigned long ino;
	int length;
	int remap_length;
	int written_length;
#ifdef FINEWC_DETECT
    DECLARE_HASHTABLE(staging_hash, KWAL_HASH_BITS);
    DECLARE_HASHTABLE(conflict_hash, KWAL_HASH_BITS);
#endif
	struct list_head gc_list;
    struct list_head dirty_listhead;
    struct list_head active_entry_head;
	struct list_head list;
    DECLARE_HASHTABLE(redirection_hash, KWAL_HASH_BITS);
    struct list_head kwal_list[KWAL_NUM];
};

/* file private data */
struct wrapfs_file_info {
	struct file *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
    
	DECLARE_HASHTABLE(redirection_hash,KWAL_HASH_BITS);
    struct list_head traversing_listhead;
	struct rw_semaphore redirection_tree_lock;
	int written_kwals[KWAL_NUM];
	unsigned long redir_len;
	int is_tx;
	int start_time;
	int read_cache_time; // check rr
	int read_cache_blk; // check rr
};

/* wrapfs inode data in memory */
struct wrapfs_inode_info {
	struct inode *lower_inode;
	struct inode vfs_inode;
	int active_txs;
	int time;
#ifdef COPY_CP
	struct file *i_file;
#endif
	//DJ
	loff_t kwal_isize;
	struct commit_tree_list * commit_tree;
	struct rw_semaphore redirection_tree_lock;		
	struct mutex atomic_mutex;
};

/* wrapfs dentry data in memory */
struct wrapfs_dentry_info {
	spinlock_t lock;	/* protects lower_path */
	struct path lower_path;
};


struct kwal_node{
    atomic_t inflight_txs;
    struct rw_semaphore on_io;
    struct rw_semaphore cp_mutex;
    int on_remap;
    int remap_ready;
    int num;
	unsigned long long last_block; // consuming first, increasing latter
	unsigned long long meta_last_block;
	unsigned long long index_block;
	int remained;
	spinlock_t last_block_lock;
	void* kwal_file;
};

struct kwal_info {
	struct list_head per_inode_commit_tree;
    
    struct rw_semaphore commit_tree_lock;
	struct mutex atomic_mutex;	
#if (KWAL_NUM <= 2)
	struct rw_semaphore big_kwal_mutex;
#endif
    spinlock_t curr_kwal_lock;
    spinlock_t curr_index_lock;
    spinlock_t remap_lock;

    wait_queue_head_t remap_done;
        
    int remap_trigger;

    struct kwal_node *to_remap_kwal;
	struct kwal_node *curr_kwal;
//    struct kwal_node *kwal_index;
//    struct kwal_node *kwal_indexes[2];
    struct kwal_node *kwals[KWAL_NUM];
    struct kwal_node *commit_kwal;
    
};

/* wrapfs super-block data in memory */
struct wrapfs_sb_info {
	struct super_block *lower_sb;
    
	struct kwal_info kwal_info;
};

static inline unsigned long set_kwal_blk(unsigned long blk, int kwal_num){
		//    wrapfs_debug("%lu\n",(blk | (((unsigned long)kwal_num)<<28)));
		return (blk | (((unsigned long)kwal_num)<<28));
}

static inline int get_kwal_num(unsigned long offset){
		//    wrapfs_debug("%lu\n",(offset>>28) & 0xF);
		return ((offset>>28) & 0xF);
}

static inline unsigned long get_kwal_blk(unsigned long blk){
		return blk & 0x0FFFFFFFUL;
}
/*
 * inode to private data
 *
 * Since we use containers and the struct inode is _inside_ the
 * wrapfs_inode_info structure, WRAPFS_I will always (given a non-NULL
 * inode pointer), return a valid non-NULL pointer.
 */
static inline struct wrapfs_inode_info *WRAPFS_I(const struct inode *inode)
{
	return container_of(inode, struct wrapfs_inode_info, vfs_inode);
}

/* dentry to private data */
#define WRAPFS_D(dent) ((struct wrapfs_dentry_info *)(dent)->d_fsdata)

/* superblock to private data */
#define WRAPFS_SB(super) ((struct wrapfs_sb_info *)(super)->s_fs_info)

/* file to private Data */
#define WRAPFS_F(file) ((struct wrapfs_file_info *)((file)->private_data))

/* file to lower file */
static inline struct file *wrapfs_lower_file(const struct file *f)
{
	return WRAPFS_F(f)->lower_file;
}

static inline void wrapfs_set_lower_file(struct file *f, struct file *val)
{
	WRAPFS_F(f)->lower_file = val;
}

/* inode to lower inode. */
static inline struct inode *wrapfs_lower_inode(const struct inode *i)
{
	return WRAPFS_I(i)->lower_inode;
}

static inline void wrapfs_set_lower_inode(struct inode *i, struct inode *val)
{
	WRAPFS_I(i)->lower_inode = val;
}

/* superblock to lower superblock */
static inline struct super_block *wrapfs_lower_super(
	const struct super_block *sb)
{
	return WRAPFS_SB(sb)->lower_sb;
}

static inline void wrapfs_set_lower_super(struct super_block *sb,
					  struct super_block *val)
{
	WRAPFS_SB(sb)->lower_sb = val;
}

/* path based (dentry/mnt) macros */
static inline void pathcpy(struct path *dst, const struct path *src)
{
	dst->dentry = src->dentry;
	dst->mnt = src->mnt;
}
/* Returns struct path.  Caller must path_put it. */
static inline void wrapfs_get_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&WRAPFS_D(dent)->lock);
	pathcpy(lower_path, &WRAPFS_D(dent)->lower_path);
	path_get(lower_path);
	spin_unlock(&WRAPFS_D(dent)->lock);
	return;
}
static inline void wrapfs_put_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	path_put(lower_path);
	return;
}
static inline void wrapfs_set_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&WRAPFS_D(dent)->lock);
	pathcpy(&WRAPFS_D(dent)->lower_path, lower_path);
	spin_unlock(&WRAPFS_D(dent)->lock);
	return;
}
static inline void wrapfs_reset_lower_path(const struct dentry *dent)
{
	spin_lock(&WRAPFS_D(dent)->lock);
	WRAPFS_D(dent)->lower_path.dentry = NULL;
	WRAPFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&WRAPFS_D(dent)->lock);
	return;
}
static inline void wrapfs_put_reset_lower_path(const struct dentry *dent)
{
	struct path lower_path;
	spin_lock(&WRAPFS_D(dent)->lock);
	pathcpy(&lower_path, &WRAPFS_D(dent)->lower_path);
	WRAPFS_D(dent)->lower_path.dentry = NULL;
	WRAPFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&WRAPFS_D(dent)->lock);
	path_put(&lower_path);
	return;
}

/* locking helpers */
static inline struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir = dget_parent(dentry);
	inode_lock_nested(d_inode(dir), I_MUTEX_PARENT);
	return dir;
}

static inline void unlock_dir(struct dentry *dir)
{
	inode_unlock(d_inode(dir));
	dput(dir);
}


#include <linux/remap.h>

#include <linux/pagemap.h>

#include <linux/delay.h>

#define KWAL_MAGIC 0x887

int kwal_init(struct super_block *sb);


#endif	/* not _WRAPFS_H_ */
