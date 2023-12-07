/*
 *  Copyright (C) 2023 CS416 Rutgers CS
 *	Tiny File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "rufs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here
struct superblock *sb;
bitmap_t inode_bitmap;
bitmap_t data_block_bitmap;
struct inode *inode_table;
//struct open_file_entry open_file_table[MAX_OPEN_FILES];
pthread_mutex_t bitmap_lock;
pthread_mutex_t inode_table_lock;


/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	
	// Step 2: Traverse inode bitmap to find an available slot

	// Step 3: Update inode bitmap and write to disk 

	return 0;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	
	// Step 2: Traverse data block bitmap to find an available slot

	// Step 3: Update data block bitmap and write to disk 

	return 0;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number

  // Step 2: Get offset of the inode in the inode on-disk block

  // Step 3: Read the block from disk and then copy into inode structure

	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	
	// Step 2: Get the offset in the block where this inode resides on disk

	// Step 3: Write inode to disk 

	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)

  // Step 2: Get data block of current directory from inode

  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure

	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way

	return 0;
}

/* 
 * Make file system
 */
int rufs_mkfs() {
    // Call dev_init() to initialize (Create) Diskfile
    dev_init(diskfile_path);

    // Write superblock information
    struct superblock *sb = malloc(sizeof(struct superblock));
    if (sb == NULL) {
        perror("Malloc failed for superblock1");
        return -1;
    }

    sb->magic_num = MAGIC_NUM;
    sb->max_inum = MAX_INUM;
    sb->max_dnum = MAX_DNUM;
    sb->i_bitmap_blk = 1; // 
    sb->d_bitmap_blk = 2; // 
    sb->i_start_blk = 3; // 
    sb->d_start_blk = 35; //

    // Write the superblock to the disk
    bio_write(0, sb);
    printf("Magic num is0: %u\n",sb->magic_num);

    // Initialize inode bitmap
    bitmap_t inode_bitmap = calloc(MAX_INUM / 8, sizeof(unsigned char));
    if (inode_bitmap == NULL) {
        perror("Malloc failed for inode bitmap");
        free(sb);
        return -1;
    }
    set_bitmap(inode_bitmap, 0); // Set the first bit for root directory inode
    bio_write(sb->i_bitmap_blk, inode_bitmap);

    // // Initialize data block bitmap
    bitmap_t data_block_bitmap = calloc(MAX_DNUM / 8, sizeof(unsigned char));
    if (data_block_bitmap == NULL) {
        perror("Malloc failed for data block bitmap");
        free(sb);
        free(inode_bitmap);
        return -1;
    }
    set_bitmap(data_block_bitmap, 0); // Set the first bit for the data block of the root directory
    bio_write(sb->d_bitmap_blk, data_block_bitmap);

    // // // Update bitmap information for root directory
    // // // This has been done above with set_bitmap calls for inode and data block bitmaps

    // // // Update inode for root directory
    // struct inode root_inode;
    // root_inode.ino = 0;
    // root_inode.valid = 1;
    // root_inode.size = 0; // Update size as needed
    // root_inode.type = S_IFDIR | 0755; // Define type (directory or file)
    // root_inode.link = 2; // Usually 2 links for root ('.' and '..')
    // // Initialize direct and indirect pointers
    // root_inode.direct_ptr[0] = sb->d_start_blk; // First data block

    // // Write root inode to disk
    // writei(0, &root_inode);

    // // Cleanup
    //free(sb);
    //free(inode_bitmap);
    //free(data_block_bitmap);

    return 0;
}
/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {
    // Check if the disk file exists
    if (access(diskfile_path, F_OK) != 0) {
      rufs_mkfs();
    } else {
        if (dev_open(diskfile_path) != 0) {
            fprintf(stderr, "Failed to open disk file\n");
            return NULL;
        }
        
        if (sb == NULL) {
        	printf("this shit is MEPTY");
          sb = malloc(sizeof(struct superblock));
        }
        bio_read(0, sb); // Assuming superblock is at block 0
		 		//printf("Magic num is: %u\n",sb1->magic_num);
    //     // Check if the superblock has the correct magic number
        if (sb->magic_num != MAGIC_NUM) {
            fprintf(stderr, "Invalid filesystem format\n");
            free(sb);
            return NULL;
        }

    // //     // Perform any other initialization based on the superblock contents
    // //     // For instance, loading bitmaps into memory if needed
    inode_bitmap = malloc(sb->max_inum / 8);
    if (inode_bitmap == NULL) {
        perror("Malloc failed for inode bitmap");
        free(inode_bitmap);
    }
    bio_read(sb->i_bitmap_blk, inode_bitmap);
    // // // Load data block bitmap into memory
    // data_block_bitmap = malloc(sb->max_dnum / 8);
    // if (data_block_bitmap == NULL) {
    //     perror("Malloc failed for data block bitmap");
    //     free(data_block_bitmap);
    // }
    //     // Cleanup
    //     free(sb);
    }

    return NULL;
}
static void rufs_destroy(void *userdata) {
    // Step 1: De-allocate in-memory data structures
    // Depending on your implementation, you might have allocated memory for in-memory data structures
    // such as bitmaps, caches, or other metadata. Make sure to free them here.
	if (sb != NULL) {
        free(sb);
        sb = NULL;
    }
    // Example:
    //  if (inode_bitmap != NULL) {
    //      free(inode_bitmap);
    //      inode_bitmap = NULL;
    // }
    // if (data_block_bitmap != NULL) {
    //     free(data_block_bitmap);
    //     data_block_bitmap = NULL;
    // }
    // Add similar free() calls for other in-memory structures you may have allocated

    // Step 2: Close diskfile
    dev_close();
}


static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode

		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);

	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

    return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	

	return 0;
}

static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.releasedir	= rufs_releasedir,
	.mkdir		= rufs_mkdir,
	.rmdir		= rufs_rmdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,
	.unlink		= rufs_unlink,

	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}

