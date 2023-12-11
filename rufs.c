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
/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {
    // Step 1: Read inode bitmap from disk
    // Assuming the inode bitmap fits within a single block
    unsigned char inode_bitmap_block[BLOCK_SIZE];
    if (bio_read(sb->i_bitmap_blk, inode_bitmap_block) < 0) {
        perror("Error reading inode bitmap block");
        return -1;
    }

    // Step 2: Traverse inode bitmap to find an available slot
    int available_inode = -1;
    for (int i = 0; i < sb->max_inum; i++) {
        if (get_bitmap(inode_bitmap_block, i) == 0) {
            available_inode = i;
            break;
        }
    }
    
    // No available inode was found
    if (available_inode == -1) {
        fprintf(stderr, "No available inodes\n");
        return -1;
    }

    // Step 3: Update inode bitmap and write to disk
    set_bitmap(inode_bitmap_block, available_inode);
    printf("bio_write for inode_bitmap in get_avail_ino\n");
    if (bio_write(sb->i_bitmap_blk, inode_bitmap_block) < 0) {
        perror("Error writing updated inode bitmap block");
        return -1;
    }
    printf("the available_inode in get_avail_ino is: %i\n",available_inode);
    return available_inode;
}


/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {
    // Step 1: Read data block bitmap from disk
    unsigned char data_block_bitmap_block[BLOCK_SIZE];
    if (bio_read(sb->d_bitmap_blk, data_block_bitmap_block) < 0) {
        perror("Error reading data block bitmap block");
        return -1;
    }

    // Step 2: Traverse data block bitmap to find an available slot
    int available_blockno = -1;
    for (int i = 0; i < sb->max_dnum; i++) {
        if (get_bitmap(data_block_bitmap_block, i) == 0) {
            available_blockno = i;
            break;
        }
    }
    
    // No available data block was found
    if (available_blockno == -1) {
        fprintf(stderr, "No available data blocks\n");
        return -1;
    }

    // Step 3: Update data block bitmap and write to disk
    set_bitmap(data_block_bitmap_block, available_blockno);
    printf("bio_write for data_block_bitmap in get_avail_blkno\n");
    if (bio_write(sb->d_bitmap_blk, data_block_bitmap_block) < 0) {
        perror("Error writing updated data block bitmap block");
        return -1;
    }

    return available_blockno;
}

int readi(uint16_t ino, struct inode *inode) {
    // Step 1: Get the inode's on-disk block number
    int block_num = sb->i_start_blk + ino / (BLOCK_SIZE / sizeof(struct inode));

    // Step 2: Get offset of the inode in the inode on-disk block
    int offset = (ino % (BLOCK_SIZE / sizeof(struct inode))) * sizeof(struct inode);

    // Step 3: Read the block from disk and then copy into inode structure
    unsigned char inode_block[BLOCK_SIZE];
    if (bio_read(block_num, inode_block) < 0) {
        perror("Error reading inode block");
        return -1;
    }
    memcpy(inode, inode_block + offset, sizeof(struct inode));
    
    return 0;
}

int writei(uint16_t ino, struct inode *inode) {
    // Step 1: Get the block number where this inode resides on disk
    int block_num = sb->i_start_blk + ino / (BLOCK_SIZE / sizeof(struct inode));

    // Step 2: Get the offset in the block where this inode resides on disk
    int offset = (ino % (BLOCK_SIZE / sizeof(struct inode))) * sizeof(struct inode);

    // Step 3: Write inode to disk
    unsigned char inode_block[BLOCK_SIZE];
    // Read the current block to not overwrite other inodes
    if (bio_read(block_num, inode_block) < 0) {
        perror("Error reading inode block for write");
        return -1;
    }
    // Copy inode into the correct position
    memcpy(inode_block + offset, inode, sizeof(struct inode));
    printf("bio_write for writei is in block_num: %d\n",block_num);
    if (bio_write(block_num, inode_block) < 0) {
        perror("Error writing inode block");
        return -1;
    }

    return 0;
}



/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {
    // Step 1: Call readi() to get the inode using ino (inode number of current directory)
    struct inode dir_inode;
    if (readi(ino, &dir_inode) < 0) {
        perror("Error reading inode in dir_find");
        return -1;
    }

    // Check if inode is a directory
    if ((dir_inode.vstat.st_mode & S_IFDIR) == 0) {
        fprintf(stderr, "Inode %d is not a directory\n", ino);
        return -1;
    }

    // Step 2: Get data block of current directory from inode
    for (int i = 0; i < 16; ++i) {  // Assuming 16 direct pointers for simplicity
        if (dir_inode.direct_ptr[i] == 0) {
            continue; // This block pointer is null, skip it
        }

        // Read the directory's data block
        char block[BLOCK_SIZE];
        if (bio_read(dir_inode.direct_ptr[i], block) < 0) {
            perror("Error reading directory block in dir_find");
            continue; // Skip to the next block
        }

        // Step 3: Read directory's data block and check each directory entry.
        struct dirent *current_dirent = (struct dirent *)block;
        for (int j = 0; j < BLOCK_SIZE / sizeof(struct dirent); ++j) {
            if (current_dirent[j].valid == 0) {
                continue; // This directory entry is not valid, skip it
            }

            // If the name matches, then copy directory entry to dirent structure
            if (strncmp(current_dirent[j].name, fname, name_len) == 0 && current_dirent[j].len == name_len) {
                memcpy(dirent, &current_dirent[j], sizeof(struct dirent));
                return 0;  // Success, entry found
            }
        }
    }

    return -1;  // Entry not found
}



int dir_add(struct inode *dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {
    if (dir_inode == NULL || fname == NULL) {
        return -1;  // Invalid arguments
    }
    char block[BLOCK_SIZE];
    struct dirent *entries;
    int new_block_allocated = 0;
    // Step 1: Read dir_inode's data block and check each directory entry of dir_inode
    for (int i = 0; i < 16; i++) {  // Assuming 16 direct pointers
        if (dir_inode->direct_ptr[i] == 0) {
            // Allocate a new data block for this directory if it does not exist
            int new_block = get_avail_blkno();
            if (new_block == -1) {
                return -1;  // No space available
            }
            dir_inode->direct_ptr[i] = new_block;
            memset(block, 0, BLOCK_SIZE);  // Zero out the new block
            new_block_allocated = 1;
        } else {
            // Read the existing block
            bio_read(dir_inode->direct_ptr[i], block);
        }

        entries = (struct dirent *)block;
        // Step 2: Check if fname (directory name) is already used in other entries
        int found_free_spot = 0;
        for (int j = 0; j < BLOCK_SIZE / sizeof(struct dirent); j++) {
            if (entries[j].valid && strncmp(entries[j].name, fname, name_len) == 0) {
                return -1;  // Entry with this name already exists
            }
            if (!entries[j].valid && !found_free_spot) {
                // Found a free spot for the new directory entry
                entries[j].ino = f_ino;
                entries[j].valid = 1;
                strncpy(entries[j].name, fname, name_len);
                entries[j].name[name_len] = '\0';  // Null terminate the name
                entries[j].len = name_len;
                found_free_spot = 1;
                break;
            }
        }

        if (found_free_spot) {
            // Step 3: Write directory entry
            printf("bio_write for dir_add\n");
            bio_write(dir_inode->direct_ptr[i], block);
            if (new_block_allocated) {
                dir_inode->size += BLOCK_SIZE;  // Update size if a new block was added
            }
            return 0;
        }
    }

    return -1;  // No space found to add the new entry
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
	printf("get_node_by_path called for path: %s\n", path);
    if (path == NULL || inode == NULL) {
        return -1; // Invalid arguments
    }

    if (path[0] != '/') {
        return -1; // Path must start with '/'
    }

    // Start with the provided inode (usually the root inode)
    if (readi(ino, inode) < 0) {
        perror("Error reading initial inode");
        return -1;
    }

    // Tokenize the path
    char *path_copy = strdup(path); // Duplicate the path as strtok modifies the string
    if (path_copy == NULL) {
        perror("strdup failed");
        return -1;
    }

    char *token = strtok(path_copy, "/");
    while (token != NULL) {
        if (S_ISDIR(inode->vstat.st_mode)) {
            // Current inode is a directory, look for the next component in this directory
            struct dirent entry;
            int found = dir_find(inode->ino, token, strlen(token), &entry);
            if (found == 0) {
                // Entry found, read its inode
                if (readi(entry.ino, inode) < 0) {
                    perror("Error reading inode in get_node_by_path");
                    free(path_copy);
                    return -1;
                }
            } else {
                // Entry not found in current directory
                free(path_copy);
                return -1;
            }
        } else {
            // Current inode is not a directory but we still have path components left
            free(path_copy);
            return -1;
        }

        token = strtok(NULL, "/"); // Get next component
    }

    free(path_copy);
    return 0; // Inode of the final path component is in 'inode'
}


/* 
 * Make file system
 */
int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);

    // Write superblock information
    sb = malloc(sizeof(struct superblock));
    if (sb == NULL) {
        perror("Malloc failed for superblock");
        return -1;
    }
    sb->magic_num = MAGIC_NUM;
    sb->max_inum = MAX_INUM;
    sb->max_dnum = MAX_DNUM;
    sb->i_bitmap_blk = 1; // 
    sb->d_bitmap_blk = 2; // 
    sb->i_start_blk = 3; // 
    sb->d_start_blk = 35; //
    printf("bio_write for sb\n");
    bio_write(0, sb);
	// write superblock information

	// initialize inode bitmap
    inode_bitmap = calloc(MAX_INUM / 8, sizeof(unsigned char));
    if (inode_bitmap == NULL) {
        perror("Malloc failed for inode bitmap");
        free(sb);
        return -1;
    }
    set_bitmap(inode_bitmap, 0); // Set the first bit for root directory inode
    printf("bio_write for inode_bitmap\n");
    bio_write(sb->i_bitmap_blk, inode_bitmap);
	// initialize data block bitmap
    data_block_bitmap = calloc(MAX_DNUM / 8, sizeof(unsigned char));
    if (data_block_bitmap == NULL) {
        perror("Malloc failed for data block bitmap");
        free(sb);
        free(inode_bitmap);
        return -1;
    }
	// update bitmap information for root directory
    set_bitmap(data_block_bitmap, 0); // Set the first bit for the data block of the root directory
    printf("bio_write for data_block_bitmap\n");
    bio_write(sb->d_bitmap_blk, data_block_bitmap);
	// update inode for root directory
	struct inode root_inode;
    memset(&root_inode, 0, sizeof(struct inode)); // Initialize all fields to zero
    root_inode.ino = 0;               // Inode number for root directory
    root_inode.valid = 1;             // Validity of the inode
    root_inode.size = 0;              // Initial size (update as needed)
    root_inode.type = S_IFDIR | 0755; // Directory type with permissions
    root_inode.link = 2;              // '.' and '..' entries for directories

    // The first data block is reserved for the root directory
    root_inode.direct_ptr[0] = sb->d_start_blk;

    // The rest of the direct and indirect pointers are initialized to 0
    // by the memset call above.

    // Set vstat
    struct stat *st = &(root_inode.vstat);
    st->st_mode = S_IFDIR | 0755; // Directory with permissions
    st->st_nlink = 2;             // Number of links
    st->st_uid = getuid();        // Owner user ID
    st->st_gid = getgid();        // Owner group ID
    st->st_size = root_inode.size;// Size of the directory (usually a number of entries * size of an entry)
    st->st_mtime = time(NULL);    // Last modification time


    // Write root inode to disk
    if (writei(root_inode.ino, &root_inode) < 0) {
        perror("Failed to write root inode to disk");
        free(sb);
        free(inode_bitmap);
        free(data_block_bitmap);
        return -1;
    }

	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs
	u_char block[BLOCK_SIZE];
	if (access(diskfile_path, F_OK) != 0) {
      rufs_mkfs();
    } else {
        if (dev_open(diskfile_path) != 0) {
            fprintf(stderr, "Failed to open disk file\n");
            return NULL;
        }
        
        if (sb == NULL) {
          sb = malloc(sizeof(struct superblock));
        }
        bio_read(0, block); // Assuming superblock is at block 0
        memcpy(sb,block,sizeof(struct superblock));
        printf("Magic num is: %u\n",sb->magic_num);
		//Check if the superblock has the correct magic number
        if (sb->magic_num != MAGIC_NUM) {
            fprintf(stderr, "Invalid filesystem format\n");
            free(sb);
            return NULL;
        }
    }
    return 0;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	if (sb != NULL) {
        free(sb);
        sb = NULL;
    }

	// Step 2: Close diskfile
	dev_close();

}

static int rufs_getattr(const char *path, struct stat *stbuf) {
	printf("rufs_getattr called for path: %s\n", path);
    if (path == NULL || stbuf == NULL) {
        return -ENOENT; // No such file or directory
    }

    // Initialize stbuf to zero
    memset(stbuf, 0, sizeof(struct stat));

    // Step 1: Call get_node_by_path() to get inode from path
    struct inode inode;
    int res = get_node_by_path(path, 0, &inode); // Assuming ROOT_INODE is the inode number of the root directory
    if (res != 0) {
        return -ENOENT; // No such file or directory
    }

    // Step 2: Fill attribute of file into stbuf from inode
    memcpy(stbuf, &inode.vstat, sizeof(struct stat));
    
    return 0;
}


static int rufs_opendir(const char *path, struct fuse_file_info *fi) {
    // Step 1: Call get_node_by_path() to get inode from path
    struct inode dir_inode;
    int res = get_node_by_path(path, 0, &dir_inode);
    if (res != 0) {
        return -ENOENT;  // No such file or directory
    }

    // Step 2: Check if it's a directory
    if (!S_ISDIR(dir_inode.vstat.st_mode)) {
        return -ENOTDIR;  // Not a directory
    }

    return 0;
}


static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    // Step 1: Call get_node_by_path() to get inode from path
    printf("I am in rufs_readdir with path : %s\n",path);
    struct inode dir_inode;
    int res = get_node_by_path(path, 0, &dir_inode);
    if (res != 0) {
        return -ENOENT;  // No such file or directory
    }
    //printf("is it getting here?");

    // Step 2: Read directory entries from its data blocks, and copy them to filler
    filler(buffer, ".", NULL, 0);
    filler(buffer, "..", NULL, 0);
    char block[BLOCK_SIZE];
    struct dirent *entry;
    printf("inode number of dir_inode in rufs_readdir is : %d\n",dir_inode.ino);
    for (int i = 0; i < 16; i++) {  // Assuming 16 direct pointers
        if (dir_inode.direct_ptr[i] == 0) {
            continue;  // Empty pointer, skip
        }
        printf("what [i] is doing a bio_read of: %d?\n",i);
        bio_read(dir_inode.direct_ptr[i], block);
        entry = (struct dirent *)block;

        for (int j = 0; j < BLOCK_SIZE / sizeof(struct dirent); j++) {
            if (entry[j].valid) {
            	printf("dirtent mame is %s\n",entry[j].name);
            	if (strcmp(entry[j].name, "\\") != 0) {
            		printf("is it not root? \n");
                	filler(buffer, entry[j].name, NULL, 0);
            	}
            }
        }
    }
    printf("outside of loops\n");

    // Handle indirect pointers if necessary
    // ...

    return 0;
}



static int rufs_mkdir(const char *path, mode_t mode) {
    if (path == NULL) {
        return -ENOENT; // No such file or directory
    }

    // Step 1: Use dirname() and basename() to separate parent directory path and target directory name
    char *path_copy = strdup(path); // Duplicate path since dirname and basename may modify it
    if (path_copy == NULL) {
        return -ENOMEM; // Out of memory
    }

    char *parent_path = dirname(path_copy);
    char *dir_name = basename(path_copy);
    printf("parent_pathis: %s and dir_name is : %s\n",parent_path,dir_name);

    // Step 2: Call get_node_by_path() to get inode of parent directory
    struct inode parent_inode;
    int res = get_node_by_path(parent_path, 0, &parent_inode);
    if (res != 0) {
        free(path_copy);
        return -ENOENT; // Parent directory not found
    }

    // Step 3: Call get_avail_ino() to get an available inode number
    uint16_t new_ino = get_avail_ino();
    printf("available inode in rufs_mkdir is: %d\n", new_ino);
    if (new_ino == 0) {
        free(path_copy);
        return -ENOSPC; // No space left on device
    }

    // Step 4: Call dir_add() to add directory entry of target directory to parent directory
    res = dir_add(&parent_inode, new_ino, dir_name, strlen(dir_name));
    printf("did it return from dir_add call?");
    if (res != 0) {
        free(path_copy);
        return res; // Propagate the error from dir_add
    }

    // Step 5: Update inode for target directory
    struct inode new_dir_inode;
    memset(&new_dir_inode, 0, sizeof(struct inode));
    new_dir_inode.ino = new_ino;
    new_dir_inode.valid = 1;
    new_dir_inode.size = 0;
    new_dir_inode.vstat.st_mode = S_IFDIR | mode;
    new_dir_inode.vstat.st_nlink = 2; // '.' and '..'
    new_dir_inode.vstat.st_uid = getuid();
    new_dir_inode.vstat.st_gid = getgid();
    new_dir_inode.vstat.st_mtime  = time(NULL);

    // Step 6: Call writei() to write inode to disk
    res = writei(new_ino, &new_dir_inode);
    if (res != 0) {
        free(path_copy);
        return -EIO; // Input/output error
    }

    free(path_copy);
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

