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

#include "bkpfs.h"
#include "main.h"

#define METAFILE_SIZE 6
#define MAX_BACKUPS 999
#define BKP_LIMIT 10
#define MAX_BKP_NAME_EXT 8
#define EXT_SIZE 4

#define BKPM_CREATE 0x1
#define BKPM_READ 0x2
#define BKPM_UPDATE 0x4
#define BKPM_UPDATE_DEL_LATEST 0x8
#define BKPM_UPDATE_DEL_OLDEST 0x10
#define BKPM_UPDATE_DEL_ALL 0x20

#define BKP_EXT ".bkp"
#define BKPT_EXT ".bkpt"
#define BKP_META_EXT ".bkpm"

struct bkpinfo {
	long num_bkps;
	long latest_bkp;
};

/* Used for filldir implementation */
struct bkpfs_getdents_callback {
	struct dir_context ctx;
	struct dir_context *caller;
	struct super_block *sb;
	int filldir_called;
	int entries_written;
};

/* Takes in a file descriptor and creates a meta file
 * which stores the number of backups and the
 * latest backup's number in a six bit format
 * three bits for each piece of information.
 */
static int __bkpfs_create_meta(struct file *metafile)
{
	int len, err = 0;
	char *buf;
	mm_segment_t old_fs;
	loff_t pos = 0;

	buf = kmalloc(METAFILE_SIZE, GFP_KERNEL);
	if (!buf) {
		err = -ENOMEM;
		goto out;
	}
	old_fs = get_fs();
	set_fs(get_ds());

	memset(buf, '\0', METAFILE_SIZE);
	strncpy(buf, "000000", METAFILE_SIZE);
	len = vfs_write(metafile, (char __user *)buf, METAFILE_SIZE, &pos);
	if (len < 0) {
		err = len;
		goto out_fs;
	}
	if (len >= 0 && len != METAFILE_SIZE) {
		err = -EIO;
		goto out_fs;
	}
out_fs:
	set_fs(old_fs);
out:
	kfree(buf);
	return err;
}

/* A generic implementation of copying data from one file to
 * another given their file descriptors
 */
int __bkpfs_read_write(struct file *infile, struct file *outfile)
{
	char *buf;
	int i = 0, err = 0, num_pages, rem;
	loff_t i_pos = 0, o_pos = 0, file_size;
	ssize_t len = 0;
	mm_segment_t old_fs;

	// Allocate buffer memory
	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf) {
		err = -ENOMEM;
		goto out_buf;
	}

	old_fs = get_fs();
	set_fs(get_ds());
	file_size = infile->f_path.dentry->d_inode->i_size;
	// Read and write
	num_pages = file_size / PAGE_SIZE;
	while (i < num_pages) {
		len = vfs_read(infile, (char __user *)buf, PAGE_SIZE, &i_pos);
		if (len < 0) {
			err = len;
			goto out;
		}

		len = vfs_write(outfile, (char __user *)buf, PAGE_SIZE, &o_pos);
		if (len < 0) {
			err = len;
			goto out;
		}
		i++;
	}
	rem = file_size % PAGE_SIZE;
	if (rem) {
		len = vfs_read(infile, (char __user *)buf, rem, &i_pos);
		if (len < 0) {
			err = len;
			goto out;
		}

		len = vfs_write(outfile, (char __user *)buf, rem, &o_pos);
		if (len < 0) {
			err = len;
			goto out;
		}
	}
out:
	set_fs(old_fs);
out_buf:
	kfree(buf);
	return err;
}

/* A helper function that checks if a given string ends
 * with a given suffix.
 */
int __str_ends_with(const char *str, const char *suffix)
{
	int lenstr, lensuffix;

	if (!str || !suffix)
		return 0;
	lenstr = strlen(str);
	lensuffix = strlen(suffix);
	if (lensuffix > lenstr)
		return 0;
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

/* A helper function that checks if a given string begins
 * with the given prefix
 */
int __str_starts_with(const char *str, const char *prefix)
{
	int lenstr, lenprefix;

	if (!str || !prefix)
		return 0;
	lenstr = strlen(str);
	lenprefix = strlen(prefix);
	if (lenprefix > lenstr)
		return 0;
	return strncmp(prefix, str, lenprefix) == 0;
}

/* Checks if the given filename is a backup file name
 * used by bkpfs
 */
int __is_backup_file(const char *str)
{
	int lenstr;

	if (!str)
		return 0;
	lenstr = strlen(str);
	return strncmp(BKP_EXT, str + lenstr - (MAX_BKP_NAME_EXT - 1), 4) == 0;
}

/* Checks if the given file name is a valid file name to be
 * backed up. Excluded names are
 * 1. Backup files and backup metadata files
 * 2. Hidden files
 * 3. Vim temp files
 */
int __is_valid_filename(const char *file_name)
{
	// Check if it is a backup metadata file
	if (__str_ends_with(file_name, BKP_META_EXT))
		return 0;
	// Hidden files
	if (__str_starts_with(file_name, "."))
		return 0;
	// Check if it is a backup file
	if (__is_backup_file(file_name))
		return 0;
	// Exclude vim temp file
	if (strcmp(file_name, "4913") == 0)
		return 0;
	return 1;
}

/* Adds the backup extension to a given filename and
 * stores it in output.
 */
int __init_file_name(const char *base, const char *extension, char *output)
{
	int len_base, len_ext;

	if (!base || !extension)
		return 1;

	len_base = strlen(base);
	len_ext = strlen(extension);

	memset(output, '\0', len_base + MAX_BKP_NAME_EXT);
	strncpy(output, base, len_base);
	strncat(output, extension, len_ext);
	return 0;
}

/* Reads the metadata file and stores the output in info*/
int __bkpfs_read_meta(struct file *metafile, struct bkpinfo *info)
{
	int len, err = 0;
	char *buf;
	mm_segment_t old_fs;
	loff_t pos = 0;

	buf = kmalloc(METAFILE_SIZE + 1, GFP_KERNEL);
	if (!buf) {
		err = -ENOMEM;
		goto out;
	}
	old_fs = get_fs();
	set_fs(get_ds());

	memset(buf, '\0', METAFILE_SIZE + 1);
	len = vfs_read(metafile, (char __user *)buf, METAFILE_SIZE, &pos);
	if (len < 0) {
		err = len;
		goto out_fs;
	}
	if (len >= 0 && len != METAFILE_SIZE) {
		err = -EIO;
		goto out_fs;
	}
	err = kstrtol(buf + 3, 10, &info->latest_bkp);
	if (err)
		goto out_fs;
	buf[3] = '\0';
	err = kstrtol(buf, 10, &info->num_bkps);

out_fs:
	set_fs(old_fs);
out:
	kfree(buf);
	return err;
}

/* Updates the metadata file with attributes present in info*/
int __bkpfs_update_meta(struct file *metafile, struct bkpinfo *info)
{
	int len, err = 0;
	char *buf, *latest_bkp;
	mm_segment_t old_fs;
	loff_t pos = 0;

	buf = kmalloc(METAFILE_SIZE, GFP_KERNEL);
	if (!buf) {
		err = -ENOMEM;
		goto out;
	}
	latest_bkp = kmalloc(4, GFP_KERNEL);
	if (!latest_bkp) {
		err = -ENOMEM;
		goto out;
	}
	old_fs = get_fs();
	set_fs(get_ds());

	memset(buf, '\0', METAFILE_SIZE);
	// Update buf with attributes
	snprintf(buf, 4, "%03d", (int)info->num_bkps);
	snprintf(latest_bkp, 4, "%03d", (int)info->latest_bkp);
	strncat(buf, latest_bkp, 4);

	len = vfs_write(metafile, (char __user *)buf, METAFILE_SIZE, &pos);
	if (len < 0) {
		err = len;
		goto out_fs;
	}

out_fs:
	set_fs(old_fs);
out:
	kfree(latest_bkp);
	kfree(buf);
	return err;
}

/* Helper method to create a dentry for a backup file
 * The input file pointer is the original file and not the
 * backup
 */
struct dentry *__create_bkp_dentry(struct file *file,
				   char *bkp_name, struct path *bkp_path)
{
	int err = 0;
	struct file *lower_file;
	struct qstr this;
	struct dentry *lower_bkp_dentry, *lower_dir_dentry;
	struct path lower_path, lower_parent_path;
	struct vfsmount *lower_dir_mnt;

	lower_file = bkpfs_lower_file(file);
	// Populate negative dentry for bkp file
	lower_path = lower_file->f_path;
	lower_dir_dentry = lower_file->f_path.dentry->d_parent;
	bkpfs_get_lower_path(file->f_path.dentry->d_parent, &lower_parent_path);

	// Create a backup dentry and file open
	lower_dir_dentry = lower_file->f_path.dentry->d_parent;
	lower_dir_mnt = lower_parent_path.mnt;

	err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt,
			      bkp_name, 0, bkp_path);
	if (!err)
		return bkp_path->dentry;
	if (err && err != -ENOENT)
		goto out;

	this.name = bkp_name;
	this.len = strlen(bkp_name);
	this.hash = full_name_hash(lower_dir_dentry, this.name, this.len);
	lower_bkp_dentry = d_lookup(lower_dir_dentry, &this);
	if (lower_bkp_dentry)
		goto bkp_dcreate;

	lower_bkp_dentry = d_alloc(lower_dir_dentry, &this);
	if (!lower_bkp_dentry) {
		err = -ENOMEM;
		goto out;
	}
	d_add(lower_bkp_dentry, NULL);

bkp_dcreate:
	// Create backup file
	bkp_path->mnt = lower_path.mnt;
	bkp_path->dentry = lower_bkp_dentry;

	err = vfs_create(d_inode(lower_dir_dentry),
			 lower_bkp_dentry, 0700, 0);
	if (err) {
		err = -EIO;
		goto out;
	}
out:
	if (err)
		return ERR_PTR(err);
	return lower_bkp_dentry;
}

/* Looks up a backup file of a given file based on
 * the  backup number and returns the file pointer.
 * The input file pointer is of the original file
 * and not the backup
 */
struct file *__bkpfs_fetch_bkp(struct file *file, int bkpno)
{
	int err = 0;
	struct file *lower_file, *lower_bkp_file;
	struct path lower_path, lower_parent_path, lower_bkp_path;
	struct dentry *lower_dir_dentry;
	struct vfsmount *lower_dir_mnt;
	const unsigned char *file_name;
	char *bkp_name, *ext;

	lower_file = bkpfs_lower_file(file);
	lower_path = lower_file->f_path;
	bkpfs_get_lower_path(file->f_path.dentry->d_parent, &lower_parent_path);

	// Create a backup dentry and file open
	lower_dir_dentry = lower_file->f_path.dentry->d_parent;
	lower_dir_mnt = lower_parent_path.mnt;
	file_name = lower_file->f_path.dentry->d_name.name;

	// Do not create backup for an existing backup file
	if (!__is_valid_filename(file_name)) {
		err = -EINVAL;
		goto out_ignore;
	}

	// Fetch the backup file from bkpno
	bkp_name = kmalloc(strlen(file_name) + MAX_BKP_NAME_EXT, GFP_KERNEL);
	if (!bkp_name) {
		err = -ENOMEM;
		goto out_name;
	}
	__init_file_name(file_name, BKP_EXT, bkp_name);
	ext = kmalloc(EXT_SIZE, GFP_KERNEL);
	if (!ext) {
		err = -ENOMEM;
		goto out_name;
	}
	snprintf(ext, EXT_SIZE, "%03d", bkpno);
	strncat(bkp_name, ext, EXT_SIZE);

	// Need to check if the backup file exists
	err = vfs_path_lookup(lower_dir_dentry,
			      lower_dir_mnt, bkp_name,
			      0, &lower_bkp_path);
	if (err) {
		err = -EINVAL;
		goto out_name;
	}

	lower_bkp_file = dentry_open(&lower_bkp_path,
				     O_RDONLY, current_cred());
	path_put(&lower_bkp_path);
out_name:
	kfree(ext);
	kfree(bkp_name);
out_ignore:
	path_put(&lower_parent_path);
	if (err)
		return ERR_PTR(err);
	return lower_bkp_file;
}

/* Helper function to create, read or update a backup metadata
 * file based on the flags sent in. The input file pointer
 * is of the original file and not the backup
 */
static int __bkpfs_meta(struct file *file, int flag, struct bkpinfo *meta_info)
{
	int err = 0, mode_flag = O_RDWR;
	const unsigned char *file_name;
	char *bkp_name;
	struct qstr this;
	struct dentry *lower_bkp_dentry, *lower_dir_dentry;
	struct path lower_path, lower_parent_path, lower_bkp_path;
	struct file *lower_file, *lower_bkp_file = NULL;
	struct vfsmount *lower_dir_mnt;

	// Initial/Essential parameters
	lower_file = bkpfs_lower_file(file);
	lower_path = lower_file->f_path;
	bkpfs_get_lower_path(file->f_path.dentry->d_parent, &lower_parent_path);

	// Create a backup dentry and file open
	lower_dir_dentry = lower_file->f_path.dentry->d_parent;
	lower_dir_mnt = lower_parent_path.mnt;
	file_name = lower_file->f_path.dentry->d_name.name;

	// Do not create backup for an existing backup meta file
	if (!__is_valid_filename(file_name)) {
		err = -EINVAL;
		goto out_ignore;
	}

	// Initialize string for backup file name
	bkp_name = kmalloc(strlen(file_name) + MAX_BKP_NAME_EXT, GFP_KERNEL);
	if (!bkp_name) {
		err = -ENOMEM;
		goto out;
	}

	__init_file_name(file_name, BKP_META_EXT, bkp_name);

	// Need to check if the backup file exists before creating
	err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt,
			      bkp_name, 0, &lower_bkp_path);
	if (!err) {
		flag &= ~(BKPM_CREATE); // Reset create bit
		goto bkp_fopen;
	}
	if (err && err != -ENOENT)
		goto out;

	// Populate negative dentry for bkp file
	this.name = bkp_name;
	this.len = strlen(bkp_name);
	this.hash = full_name_hash(lower_dir_dentry, this.name, this.len);
	lower_bkp_dentry = d_lookup(lower_dir_dentry, &this);
	if (lower_bkp_dentry)
		goto bkp_fcreate;

	lower_bkp_dentry = d_alloc(lower_dir_dentry, &this);
	if (!lower_bkp_dentry) {
		err = -ENOMEM;
		goto out_path;
	}
	d_add(lower_bkp_dentry, NULL);
bkp_fcreate:
	// Create backup file
	lower_bkp_path.mnt = lower_path.mnt;
	lower_bkp_path.dentry = lower_bkp_dentry;

	err = vfs_create(d_inode(lower_dir_dentry),
			 lower_bkp_dentry, 0700, 0);
	if (err) {
		err = -EIO;
		goto out_path;
	}
bkp_fopen:
	// Create/Open a metadata file
	lower_bkp_file = dentry_open(&lower_bkp_path,
				     mode_flag, current_cred());

	if (IS_ERR(lower_bkp_file)) {
		err = PTR_ERR(lower_bkp_file);
		goto out_path;
	}

	/*
	 * Create a new metadata file if it does not exist,
	 * otherwise read the existing metadata file
	 */
	if (flag & BKPM_CREATE) {
		meta_info->num_bkps = 0;
		meta_info->latest_bkp = 0;
		err = __bkpfs_create_meta(lower_bkp_file);
	} else {
		err = __bkpfs_read_meta(lower_bkp_file, meta_info);
	}

	// If update flag is passed then update the meta file
	if (flag & BKPM_UPDATE) {
		if (meta_info->latest_bkp >= MAX_BACKUPS)
			meta_info->latest_bkp = meta_info->num_bkps;
		else
			meta_info->latest_bkp += 1;
		if (meta_info->num_bkps < maxbkpver)
			meta_info->num_bkps += 1;
		//meta_info->latest_bkp += 1;
		err = __bkpfs_update_meta(lower_bkp_file, meta_info);
	}
	if (flag & BKPM_UPDATE_DEL_LATEST) {
		meta_info->num_bkps -= 1;
		meta_info->latest_bkp -= 1;
		err = __bkpfs_update_meta(lower_bkp_file, meta_info);
	}
	if (flag & BKPM_UPDATE_DEL_OLDEST) {
		meta_info->num_bkps -= 1;
		err = __bkpfs_update_meta(lower_bkp_file, meta_info);
	}
	if (flag & BKPM_UPDATE_DEL_ALL) {
		meta_info->num_bkps = 0;
		meta_info->latest_bkp = 0;
		err = __bkpfs_update_meta(lower_bkp_file, meta_info);
	}

	if (lower_bkp_file)
		fput(lower_bkp_file);
out_path:
	path_put(&lower_bkp_path);
out:
	kfree(bkp_name);
out_ignore:
	path_put(&lower_parent_path);
	return err;
}

/* Helper function to find the latest backup file name
 *  given a file and its backup information. The output
 *  file name is stored in output.
 */
int __find_latest_bkp(const char *file_name, struct bkpinfo *info, char *output)
{
	int latest_bkp;
	char *ext;

	if (!info)
		return -EINVAL;

	__init_file_name(file_name, BKP_EXT, output);
	latest_bkp = (int)info->latest_bkp;
	ext = kmalloc(EXT_SIZE, GFP_KERNEL);
	if (!ext)
		return -ENOMEM;
	snprintf(ext, EXT_SIZE, "%03d", latest_bkp);
	strncat(output, ext, EXT_SIZE);
	return 0;
}

/* Helper function to find oldest backup's file name
 * given a file and its backup information. The output
 * file name is stored int outpou;
 */
int __find_oldest_bkp(const char *file_name, struct bkpinfo *info, char *output)
{
	int oldest_bkp;
	char *ext;

	if (!info)
		return -EINVAL;

	__init_file_name(file_name, BKP_EXT, output);
	oldest_bkp = (int)info->latest_bkp - (int)info->num_bkps + 1;
	ext = kmalloc(EXT_SIZE, GFP_KERNEL);
	if (!ext)
		return -ENOMEM;
	snprintf(ext, EXT_SIZE, "%03d", oldest_bkp);
	strncat(output, ext, EXT_SIZE);
	return 0;
}

/* Helper function to delete a backup given the parent directory
 * path and the backup's file name.
 */
int __remove_bkp(struct path lower_parent_path, char *bkp_name)
{
	int err = 0;
	struct dentry *lower_dir_dentry, *lower_bkp_dentry;
	struct path lower_bkp_path;
	struct vfsmount *lower_dir_mnt;

	lower_dir_dentry = lower_parent_path.dentry;
	lower_dir_mnt = lower_parent_path.mnt;
	err = vfs_path_lookup(lower_dir_dentry,
			      lower_dir_mnt, bkp_name,
			      0, &lower_bkp_path);
	if (err) {
		pr_info("file for deletion not found\n");
		err = 0;
		goto out;
	}
	lower_bkp_dentry = lower_bkp_path.dentry;
	dget(lower_bkp_dentry);
	inode_lock(lower_dir_dentry->d_inode);
	err = vfs_unlink(lower_dir_dentry->d_inode, lower_bkp_dentry, NULL);
	if (err == -EBUSY && lower_bkp_dentry->d_flags & DCACHE_NFSFS_RENAMED)
		err = 0;

	inode_unlock(lower_dir_dentry->d_inode);
	dput(lower_bkp_dentry);

	path_put(&lower_bkp_path);
out:
	return err;
}

/* Helper function to remove all backups associated
 * with the file based on the info from the metadata
 */
int __bkpfs_remove_all_bkps(struct file *file, struct bkpinfo *info)
{
	int i, oldest_bkp, err = 0;
	char *bkp_name, *ext;
	const unsigned char *file_name;
	struct file *lower_file;
	struct path lower_parent_path;

	lower_file = bkpfs_lower_file(file);
	file_name = lower_file->f_path.dentry->d_name.name;
	oldest_bkp = (int)info->latest_bkp - (int)info->num_bkps + 1;
	bkpfs_get_lower_path(file->f_path.dentry->d_parent, &lower_parent_path);

	bkp_name = kmalloc(strlen(file_name) + MAX_BKP_NAME_EXT, GFP_KERNEL);
	if (!bkp_name) {
		err = -ENOMEM;
		goto out;
	}
	ext = kmalloc(EXT_SIZE, GFP_KERNEL);
	if (!ext) {
		err = -ENOMEM;
		goto out;
	}
	// Delete backups starting from the oldest
	for (i = oldest_bkp; i <= (int)info->latest_bkp; i++) {
		__init_file_name(file_name, BKP_EXT, bkp_name);
		memset(ext, '\0', EXT_SIZE);
		snprintf(ext, EXT_SIZE, "%03d", i);
		strncat(bkp_name, ext, EXT_SIZE);
		err = __remove_bkp(lower_parent_path, bkp_name);
		if (err)
			goto out;
	}
out:
	kfree(ext);
	kfree(bkp_name);
	path_put(&lower_parent_path);
	return err;
}

/* Renames a backup and takes in the old and new dentries */
int bkpfs_rename_bkp(struct dentry *lower_old_dentry,
		     struct dentry *lower_new_dentry)
{
	int err = 0;
	struct dentry *trap;
	struct dentry *lower_dir_dentry = NULL;

	lower_dir_dentry = dget_parent(lower_old_dentry);

	trap = lock_rename(lower_dir_dentry, lower_dir_dentry);

	err = vfs_rename(d_inode(lower_dir_dentry), lower_old_dentry,
			 d_inode(lower_dir_dentry), lower_new_dentry,
			NULL, 0);

	unlock_rename(lower_dir_dentry, lower_dir_dentry);
	dput(lower_dir_dentry);
	return err;
}

/* This function is invoked when the maximum backup
 * limit is reached which is 999. This is a design decision
 * based on how the metadata file is structured (3 bits for
 * storing number of backups). Once the limit is reached
 * all the backup files are renamed to start from 1 to N in
 * the same order as they were created.
 */
int __reset_all_bkps(struct file *file, struct bkpinfo *info)
{
	int err = 0, oldest_bkp, i, new_file_num = 0;
	struct file *lower_file;
	const unsigned char *file_name;
	char *new_bkp_name, *ext;
	struct dentry *new_bkp_dentry;
	struct path new_bkp_path;
	struct file *old_bkp_file;

	lower_file = bkpfs_lower_file(file);
	file_name = lower_file->f_path.dentry->d_name.name;
	new_bkp_name = kmalloc(strlen(file_name) + MAX_BKP_NAME_EXT,
			       GFP_KERNEL);

	if (!new_bkp_name) {
		err = -ENOMEM;
		goto out;
	}
	ext = kmalloc(EXT_SIZE, GFP_KERNEL);
	if (!ext) {
		err = -ENOMEM;
		goto out_name;
	}
	oldest_bkp = (int)info->latest_bkp - (int)info->num_bkps + 1;
	for (i = oldest_bkp; i <= (int)info->latest_bkp; i++) {
		old_bkp_file = __bkpfs_fetch_bkp(file, i);
		if (IS_ERR(old_bkp_file)) {
			err = PTR_ERR(old_bkp_file);
			goto out_ext;
		}

		new_file_num += 1;
		__init_file_name(file_name, BKP_EXT, new_bkp_name);
		snprintf(ext, EXT_SIZE, "%03d", new_file_num);
		strncat(new_bkp_name, ext, EXT_SIZE);

		new_bkp_dentry = __create_bkp_dentry(file,
						     new_bkp_name,
						     &new_bkp_path);
		if (IS_ERR(new_bkp_dentry)) {
			err = PTR_ERR(new_bkp_dentry);
			goto out_ext;
		}

		err = bkpfs_rename_bkp(old_bkp_file->f_path.dentry,
				       new_bkp_dentry);
		if (err)
			goto out_ext;

		// Cleanup
		if (old_bkp_file)
			fput(old_bkp_file);
	}
	// Update info
	info->latest_bkp = new_file_num;

out_ext:
	path_put(&new_bkp_path);
	kfree(ext);
out_name:
	kfree(new_bkp_name);
out:
	return err;
}

/* Helper function for creating a backup file*/
static int __bkpfs_create_bkp(struct file *file, struct bkpinfo *info)
{
	int err = 0;
	const unsigned char *file_name;
	char *bkp_name, *ext;
	struct dentry *lower_bkp_dentry, *lower_dir_dentry;
	struct path lower_bkp_path, lower_path;
	struct path lower_parent_path;
	struct file *lower_file, *lower_bkp_file = NULL;

	// Initial/essetial parameters
	lower_file = bkpfs_lower_file(file);
	lower_path = lower_file->f_path;
	bkpfs_get_lower_path(file->f_path.dentry->d_parent, &lower_parent_path);

	// Create a backup dentry and file open
	lower_dir_dentry = lower_file->f_path.dentry->d_parent;
	file_name = lower_file->f_path.dentry->d_name.name;

	// Do not create backup for an existing backup file
	if (!__is_valid_filename(file_name))
		goto out_ignore;

	// Initialize string for backup file name
	bkp_name = kmalloc(strlen(file_name) + MAX_BKP_NAME_EXT, GFP_KERNEL);
	if (!bkp_name) {
		err = -ENOMEM;
		goto out_name;
	}

	__init_file_name(file_name, BKP_EXT, bkp_name);
	ext = kmalloc(EXT_SIZE, GFP_KERNEL);
	if (!ext) {
		err = -ENOMEM;
		goto out_name;
	}
	snprintf(ext, EXT_SIZE, "%03d", (int)info->latest_bkp + 1);
	strncat(bkp_name, ext, EXT_SIZE);

	lower_bkp_dentry = __create_bkp_dentry(file, bkp_name, &lower_bkp_path);
	if (IS_ERR(lower_bkp_dentry)) {
		err = PTR_ERR(lower_bkp_dentry);
		goto out;
	}

	// Copy main file contents to backup file
	lower_bkp_file = dentry_open(&lower_bkp_path,
				     O_WRONLY | O_CREAT | O_TRUNC,
				     current_cred());

	if (IS_ERR(lower_bkp_file)) {
		err = PTR_ERR(lower_bkp_file);
		goto out;
	}

	// Create a copy of the original file
	err = __bkpfs_read_write(lower_file, lower_bkp_file);
	if (err) {
		//Need to remove the backup file created
		if (!lower_bkp_dentry)
			lower_bkp_dentry = lower_bkp_path.dentry;
		dget(lower_bkp_dentry);
		inode_lock(lower_dir_dentry->d_inode);
		err = vfs_unlink(lower_dir_dentry->d_inode,
				 lower_bkp_dentry, NULL);
		inode_unlock(lower_dir_dentry->d_inode);
		dput(lower_bkp_dentry);
	}

	if (lower_bkp_file)
		fput(lower_bkp_file);
out:
	path_put(&lower_bkp_path);
out_name:
	kfree(ext);
	kfree(bkp_name);
out_ignore:
	path_put(&lower_parent_path);
	return err;
}

/* Helper function for creating a temp file when a restore
 * is called
 */
static int __bkpfs_create_temp_bkp(struct file *file, int bkpno)
{
	int err = 0;
	const unsigned char *file_name;
	char *temp_name, *ext;
	struct dentry *lower_bkpt_dentry;
	struct path lower_bkpt_path;
	struct file *lower_file;
	struct file *lower_bkpt_file = NULL;
	struct file *lower_bkp_file = NULL;

	// Initial/essetial parameters
	lower_file = bkpfs_lower_file(file);
	file_name = lower_file->f_path.dentry->d_name.name;

	// Do not create backup for an existing backup file
	if (!__is_valid_filename(file_name))
		goto out_ignore;

	// Initialize string for backup file name
	temp_name = kmalloc(strlen(file_name) + MAX_BKP_NAME_EXT, GFP_KERNEL);
	if (!temp_name) {
		err = -ENOMEM;
		goto out_name;
	}

	__init_file_name(file_name, BKPT_EXT, temp_name);

	lower_bkpt_dentry = __create_bkp_dentry(file,
						temp_name,
						&lower_bkpt_path);
	if (IS_ERR(lower_bkpt_dentry)) {
		err = PTR_ERR(lower_bkpt_dentry);
		goto out;
	}

	// Copy main file contents to backup file
	lower_bkpt_file = dentry_open(&lower_bkpt_path,
				      O_WRONLY | O_CREAT | O_TRUNC,
				      current_cred());

	if (IS_ERR(lower_bkpt_file)) {
		err = PTR_ERR(lower_bkpt_file);
		goto out;
	}

	// Fetch the backup file
	lower_bkp_file = __bkpfs_fetch_bkp(file, bkpno);
	if (IS_ERR(lower_bkp_file)) {
		err = PTR_ERR(lower_bkp_file);
		goto out_file;
	}
	// Create a copy of the original file
	err = __bkpfs_read_write(lower_bkp_file, lower_bkpt_file);

	if (lower_bkp_file)
		fput(lower_bkp_file);

out_file:
	if (lower_bkpt_file)
		fput(lower_bkpt_file);
out:
	path_put(&lower_bkpt_path);
out_name:
	kfree(ext);
	kfree(temp_name);
out_ignore:
	return err;
}

/* Helper function to read a backup file's contents*/
int __bkpfs_read_bkp(struct file *file, char *buf, loff_t *pos)
{
	int err = 0, len;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(get_ds());

	memset(buf, '\0', PAGE_SIZE);
	len = vfs_read(file, (char __user *)buf, PAGE_SIZE - 1, pos);
	if (len < 0)
		err = len;
	set_fs(old_fs);
	return err;
}

static ssize_t bkpfs_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;

	lower_file = bkpfs_lower_file(file);
	err = vfs_read(lower_file, buf, count, ppos);
	/* update our inode atime upon a successful lower read */
	if (err >= 0)
		fsstack_copy_attr_atime(d_inode(dentry),
					file_inode(lower_file));

	return err;
}

static ssize_t bkpfs_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *ppos)
{
	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;

	lower_file = bkpfs_lower_file(file);
	err = vfs_write(lower_file, buf, count, ppos);
	/* update our inode times+sizes upon a successful lower write */
	if (err >= 0) {
		// Update flag to notify a write has occurred
		BKPFS_F(file)->is_write = 1;
		fsstack_copy_inode_size(d_inode(dentry),
					file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(dentry),
					file_inode(lower_file));
	}
	return err;
}

static int bkpfs_filldir(struct dir_context *ctx,
			 const char *lower_name,
			 int lower_namelen, loff_t offset,
			 u64 ino, unsigned int d_type)
{
	struct bkpfs_getdents_callback *buf =
		container_of(ctx, struct bkpfs_getdents_callback, ctx);
	int rc = 0;

	buf->filldir_called++;
	if (!__is_valid_filename(lower_name))
		goto out;

	buf->caller->pos = buf->ctx.pos;
	rc = !dir_emit(buf->caller, lower_name, lower_namelen, ino, d_type);
	if (!rc)
		buf->entries_written++;
out:
	return rc;
}

static int bkpfs_readdir(struct file *file, struct dir_context *ctx)
{
	int err;
	struct file *lower_file = NULL;
	struct dentry *dentry = file->f_path.dentry;
	struct inode *inode = file_inode(file);
	struct bkpfs_getdents_callback buf = {
		.ctx.actor = bkpfs_filldir,
		.caller = ctx,
		.sb = inode->i_sb,
	};

	lower_file = bkpfs_lower_file(file);
	err = iterate_dir(lower_file, &buf.ctx);
	ctx->pos = buf.ctx.pos;
	file->f_pos = lower_file->f_pos;
	if (buf.filldir_called && !buf.entries_written)
		goto out;
	if (err >= 0)		/* copy the atime */
		fsstack_copy_attr_atime(d_inode(dentry),
					file_inode(lower_file));
out:
	return err;
}

static long bkpfs_unlocked_ioctl(struct file *file,
				 unsigned int cmd,
				 unsigned long arg)
{
	int flag = 0, oldest, newest, bkpno;
	long err = 0;
	const unsigned char *file_name;
	char *bkp_name;
	struct file *lower_file, *bkp_file;
	struct bkpinfo info;
	struct path lower_parent_path;
	query_arg_t *q1;

	lower_file = bkpfs_lower_file(file);
	file_name = lower_file->f_path.dentry->d_name.name;
	bkpfs_get_lower_path(file->f_path.dentry->d_parent, &lower_parent_path);

	bkp_name = kmalloc(strlen(file_name) + MAX_BKP_NAME_EXT, GFP_KERNEL);
	if (!bkp_name) {
		err = -ENOMEM;
		goto out;
	}
	q1 = kmalloc(sizeof(query_arg_t), GFP_KERNEL);
	if (!q1) {
		err = -ENOMEM;
		goto out;
	}
	switch (cmd) {
	case QUERY_LIST_VER:
		if (!access_ok(VERIFY_WRITE,
			       arg, sizeof(query_arg_t))) {
			err = -EFAULT;
			goto out;
		}

		// Read metadata file
		flag |= BKPM_READ;
		err = __bkpfs_meta(file, flag, &info);
		if (err)
			goto out;

		q1->num_bkps = (int)info.num_bkps;
		q1->latest_bkp = (int)info.latest_bkp;

		if (copy_to_user((query_arg_t *)arg,
				 q1, sizeof(query_arg_t))) {
			err = -EACCES;
			goto out;
		}
		goto out;

	case QUERY_DELETE_VER:
		if (!access_ok(VERIFY_READ, arg, sizeof(query_arg_t))) {
			err = -EFAULT;
			goto out;
		}
		if (copy_from_user(q1,
				   (query_arg_t *)arg,
				   sizeof(query_arg_t))) {
			err = -EACCES;
			goto out;
		}

		// Read metadata file
		flag |= BKPM_READ;
		err = __bkpfs_meta(file, flag, &info);
		if (err)
			goto out;

		if (info.num_bkps == 0)
			goto out;
		if (q1->delete_ver & DEL_LATEST) {
			err = __find_latest_bkp(file_name, &info, bkp_name);
			err = __remove_bkp(lower_parent_path, bkp_name);

			// Update the metadata file
			flag = 0; //Reset
			flag |= BKPM_UPDATE_DEL_LATEST;
			err = __bkpfs_meta(file, flag, &info);
		} else if (q1->delete_ver & DEL_OLDEST) {
			err = __find_oldest_bkp(file_name, &info, bkp_name);
			err = __remove_bkp(lower_parent_path, bkp_name);

			// Update the metadata file
			flag = 0; //Reset
			flag |= BKPM_UPDATE_DEL_OLDEST;
			err = __bkpfs_meta(file, flag, &info);
		} else if (q1->delete_ver & DEL_ALL) {
			err = __bkpfs_remove_all_bkps(file, &info);

			// Update the metadata file
			flag = 0; //Reset
			flag |= BKPM_UPDATE_DEL_ALL;
			err = __bkpfs_meta(file, flag, &info);
		} else {
			pr_info("Invalid delete option\n");
		}

		goto out;

	case QUERY_VIEW_VER:
		if (!access_ok(VERIFY_WRITE, arg, sizeof(query_arg_t))) {
			err = -EFAULT;
			goto out;
		}
		if (copy_from_user(q1,
				   (query_arg_t *)arg,
				   sizeof(query_arg_t))) {
			err = -EACCES;
			goto out;
		}

		// Read metadata file
		flag |= BKPM_READ;
		err = __bkpfs_meta(file, flag, &info);
		if (err)
			goto out;
		if (info.num_bkps == 0)
			goto out;
		if (q1->version == VIEW_NEW)
			bkpno = (int)info.latest_bkp;
		else if (q1->version == VIEW_OLD)
			bkpno = (int)(info.latest_bkp - info.num_bkps + 1);
		else
			bkpno = q1->version;
		bkp_file = __bkpfs_fetch_bkp(file, bkpno);
		if (IS_ERR(bkp_file)) {
			err = PTR_ERR(bkp_file);
			goto out;
		}
		err = __bkpfs_read_bkp(bkp_file, q1->buf, &q1->offset);
		if (err)
			goto out;
		if (copy_to_user((query_arg_t *)arg,
				 q1, sizeof(query_arg_t))) {
			err = -EACCES;
			goto out;
		}
		if (bkp_file)
			fput(bkp_file);

		goto out;

	case QUERY_RESTORE_VER:
		if (!access_ok(VERIFY_READ, arg, sizeof(query_arg_t))) {
			err = -EFAULT;
			goto out;
		}
		if (copy_from_user(q1,
				   (query_arg_t *)arg,
				   sizeof(query_arg_t))) {
			err = -EACCES;
			goto out;
		}

		flag |= BKPM_READ;
		err = __bkpfs_meta(file, flag, &info);
		if (err)
			goto out;

		oldest = (int)info.latest_bkp - (int)info.num_bkps + 1;
		newest = (int)info.latest_bkp;

		if (info.num_bkps == 0)
			goto out;
		if (q1->version == RESTORE_NEW)
			err = __bkpfs_create_temp_bkp(file, newest);
		else if (q1->version == RESTORE_OLD)
			err = __bkpfs_create_temp_bkp(file, oldest);
		else
			if (q1->version <= newest && q1->version >= oldest)
				err = __bkpfs_create_temp_bkp(file,
							      q1->version);

		goto out;
	}

	err = -ENOTTY;
	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->unlocked_ioctl)
		err = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);

	/* some ioctls can change inode attributes (EXT2_IOC_SETFLAGS) */
	if (!err)
		fsstack_copy_attr_all(file_inode(file),
				      file_inode(lower_file));
	goto out_ioctl;
out:
	kfree(q1);
	kfree(bkp_name);
	path_put(&lower_parent_path);

out_ioctl:
	return err;
}

#ifdef CONFIG_COMPAT
static long bkpfs_compat_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;

	lower_file = bkpfs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->compat_ioctl)
		err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);

out:
	return err;
}
#endif

static int bkpfs_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err = 0;
	bool willwrite;
	struct file *lower_file;
	const struct vm_operations_struct *saved_vm_ops = NULL;

	/* this might be deferred to mmap's writepage */
	willwrite = ((vma->vm_flags | VM_SHARED | VM_WRITE) == vma->vm_flags);

	/*
	 * File systems which do not implement ->writepage may use
	 * generic_file_readonly_mmap as their ->mmap op.  If you call
	 * generic_file_readonly_mmap with VM_WRITE, you'd get an -EINVAL.
	 * But we cannot call the lower ->mmap op, so we can't tell that
	 * writeable mappings won't work.  Therefore, our only choice is to
	 * check if the lower file system supports the ->writepage, and if
	 * not, return EINVAL (the same error that
	 * generic_file_readonly_mmap returns in that case).
	 */
	lower_file = bkpfs_lower_file(file);
	if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
		err = -EINVAL;
		pr_info(KERN_ERR "bkpfs: lower file system does not "
		       "support writeable mmap\n");
		goto out;
	}

	/*
	 * find and save lower vm_ops.
	 *
	 * XXX: the VFS should have a cleaner way of finding the lower vm_ops
	 */
	if (!BKPFS_F(file)->lower_vm_ops) {
		err = lower_file->f_op->mmap(lower_file, vma);
		if (err) {
			pr_info(KERN_ERR "bkpfs: lower mmap failed %d\n", err);
			goto out;
		}
		saved_vm_ops = vma->vm_ops; /* save: came from lower ->mmap */
	}

	/*
	 * Next 3 lines are all I need from generic_file_mmap.  I definitely
	 * don't want its test for ->readpage which returns -ENOEXEC.
	 */
	file_accessed(file);
	vma->vm_ops = &bkpfs_vm_ops;

	file->f_mapping->a_ops = &bkpfs_aops; /* set our aops */
	if (!BKPFS_F(file)->lower_vm_ops) /* save for our ->fault */
		BKPFS_F(file)->lower_vm_ops = saved_vm_ops;

out:
	return err;
}

static int bkpfs_open(struct inode *inode, struct file *file)
{
	int err = 0, flag;
	const unsigned char *file_name;
	struct file *lower_file = NULL;
	struct path lower_path;
	struct inode *lower_inode;
	struct bkpinfo info;

	/* don't open unhashed/deleted files */
	if (d_unhashed(file->f_path.dentry)) {
		err = -ENOENT;
		goto out_err;
	}

	file->private_data =
		kzalloc(sizeof(struct bkpfs_file_info), GFP_KERNEL);
	if (!BKPFS_F(file)) {
		err = -ENOMEM;
		goto out_err;
	}

	/* open lower object and link bkpfs's file struct to lower's */
	bkpfs_get_lower_path(file->f_path.dentry, &lower_path);
	lower_file = dentry_open(&lower_path, file->f_flags, current_cred());
	path_put(&lower_path);
	if (IS_ERR(lower_file)) {
		err = PTR_ERR(lower_file);
	} else {
		bkpfs_set_lower_file(file, lower_file);
		lower_inode = bkpfs_lower_inode(inode);
		file_name = lower_file->f_path.dentry->d_name.name;

		// Check for regular file and valid file name
		if (S_ISREG(lower_inode->i_mode) &&
		    __is_valid_filename(file_name)) {
			flag |= BKPM_CREATE;
			flag |= BKPM_READ;
			err = __bkpfs_meta(file, flag, &info);
		}
		if (!err)
			goto out;
	}
	lower_file = bkpfs_lower_file(file);
	if (lower_file) {
		bkpfs_set_lower_file(file, NULL);
		fput(lower_file); /* fput calls dput for lower_dentry */
	}

out:
	if (err)
		kfree(BKPFS_F(file));
	else
		fsstack_copy_attr_all(inode, bkpfs_lower_inode(inode));
out_err:
	return err;
}

static int bkpfs_flush(struct file *file, fl_owner_t id)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = bkpfs_lower_file(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->flush) {
		filemap_write_and_wait(file->f_mapping);
		err = lower_file->f_op->flush(lower_file, id);
	}

	return err;
}

/* release all lower object references & free the file info structure */
static int bkpfs_file_release(struct inode *inode, struct file *file)
{
	struct file *lower_file;
	int flag = 0, err = 0, i;
	long temp_count;
	struct bkpinfo info;
	const unsigned char *file_name;
	char *bkp_name;
	struct path lower_parent_path;

	pr_info("file_release entered\n");
	lower_file = bkpfs_lower_file(file);

	// Need to be able to read file to create a backup
	lower_file->f_mode |= (FMODE_READ | FMODE_CAN_READ);
	lower_file->f_mode &= ~FMODE_WRITE;

	// Check if metadata file exists, if not create one
	file_name = lower_file->f_path.dentry->d_name.name;
	bkpfs_get_lower_path(file->f_path.dentry->d_parent, &lower_parent_path);

	// Initialize string for backup file name
	bkp_name = kmalloc(strlen(file_name) + MAX_BKP_NAME_EXT, GFP_KERNEL);
	if (!bkp_name) {
		err = -ENOMEM;
		goto out;
	}

	if (BKPFS_F(file)->is_write && __is_valid_filename(file_name)) {
		flag |=  BKPM_CREATE; // Create
		flag |= BKPM_READ; // Read
		err = __bkpfs_meta(file, flag, &info);

		if (err)
			goto out;

		// Reset if latest_bkp reaches MAX_BACKUPS
		if (info.latest_bkp >= MAX_BACKUPS) {
			err = __reset_all_bkps(file, &info);
			if (err)
				goto out;

			// Update metafile
			flag = 0; // flag reset
			flag |= BKPM_UPDATE; // update
			err = __bkpfs_meta(file, flag, &info);
			if (err)
				goto out;
		}
		pr_info("maxbkpver= %ld\n", maxbkpver);
		// When number of backups exceeds max, we delete the oldest one
		if (info.num_bkps >= maxbkpver) {
			pr_info("num_bkp greater than maxbkpver\n");
			temp_count = info.num_bkps;
			for (i = 0; i <= (temp_count - maxbkpver); i++) {
				err = __find_oldest_bkp(file_name,
							&info, bkp_name);
				err = __remove_bkp(lower_parent_path, bkp_name);
				if (err)
					break;
				info.num_bkps -= 1;
			}
			path_put(&lower_parent_path);
			if (err) {
				pr_info("error in loop\n");
				goto out;
			}
			pr_info("after loop, info is %ld, %ld\n",
				info.num_bkps, info.latest_bkp);
		}
		// Create a backup before releasing file
		err = __bkpfs_create_bkp(file, &info);
		if (err)
			goto out;

		// Update metafile
		flag = 0; // flag reset
		flag |= BKPM_UPDATE; // update
		err = __bkpfs_meta(file, flag, &info);

		if (err)
			goto out;

		// Reset the flag
		BKPFS_F(file)->is_write = 0;
	}

	// File release code
	if (lower_file) {
		bkpfs_set_lower_file(file, NULL);
		fput(lower_file);
	}
out:
	kfree(BKPFS_F(file));
	kfree(bkp_name);
	return err;
}

static int bkpfs_fsync(struct file *file, loff_t start, loff_t end,
		       int datasync)
{
	int err;
	struct file *lower_file;
	struct path lower_path;
	struct dentry *dentry = file->f_path.dentry;

	err = __generic_file_fsync(file, start, end, datasync);
	if (err)
		goto out;
	lower_file = bkpfs_lower_file(file);
	bkpfs_get_lower_path(dentry, &lower_path);
	err = vfs_fsync_range(lower_file, start, end, datasync);
	bkpfs_put_lower_path(dentry, &lower_path);
out:
	return err;
}

static int bkpfs_fasync(int fd, struct file *file, int flag)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = bkpfs_lower_file(file);
	if (lower_file->f_op && lower_file->f_op->fasync)
		err = lower_file->f_op->fasync(fd, lower_file, flag);

	return err;
}

/*
 * Bkpfs cannot use generic_file_llseek as ->llseek, because it would
 * only set the offset of the upper file.  So we have to implement our
 * own method to set both the upper and lower file offsets
 * consistently.
 */
static loff_t bkpfs_file_llseek(struct file *file, loff_t offset, int whence)
{
	int err;
	struct file *lower_file;

	err = generic_file_llseek(file, offset, whence);
	if (err < 0)
		goto out;

	lower_file = bkpfs_lower_file(file);
	err = generic_file_llseek(lower_file, offset, whence);

out:
	return err;
}

/*
 * Bkpfs read_iter, redirect modified iocb to lower read_iter
 */
ssize_t
bkpfs_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = bkpfs_lower_file(file);
	if (!lower_file->f_op->read_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->read_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode atime as needed */
	if (err >= 0 || err == -EIOCBQUEUED)
		fsstack_copy_attr_atime(d_inode(file->f_path.dentry),
					file_inode(lower_file));
out:
	return err;
}

/*
 * Bkpfs write_iter, redirect modified iocb to lower write_iter
 */
ssize_t
bkpfs_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = bkpfs_lower_file(file);
	if (!lower_file->f_op->write_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->write_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode times/sizes as needed */
	if (err >= 0 || err == -EIOCBQUEUED) {
		fsstack_copy_inode_size(d_inode(file->f_path.dentry),
					file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(file->f_path.dentry),
					file_inode(lower_file));
	}
out:
	return err;
}

const struct file_operations bkpfs_main_fops = {
	.llseek		= generic_file_llseek,
	.read		= bkpfs_read,
	.write		= bkpfs_write,
	.unlocked_ioctl	= bkpfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= bkpfs_compat_ioctl,
#endif
	.mmap		= bkpfs_mmap,
	.open		= bkpfs_open,
	.flush		= bkpfs_flush,
	.release	= bkpfs_file_release,
	.fsync		= bkpfs_fsync,
	.fasync		= bkpfs_fasync,
	.read_iter	= bkpfs_read_iter,
	.write_iter	= bkpfs_write_iter,
};

/* trimmed directory options */
const struct file_operations bkpfs_dir_fops = {
	.llseek		= bkpfs_file_llseek,
	.read		= generic_read_dir,
	.iterate	= bkpfs_readdir,
	.unlocked_ioctl	= bkpfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= bkpfs_compat_ioctl,
#endif
	.open		= bkpfs_open,
	.release	= bkpfs_file_release,
	.flush		= bkpfs_flush,
	.fsync		= bkpfs_fsync,
	.fasync		= bkpfs_fasync,
};
