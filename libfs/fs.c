#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define BLOCK_SIZE 4096
#define FAT_EOC 0xFFFF
#define MAX_FILES 128

struct __attribute__((packed)) superBlock {
    char signature[8]; 			// Signature (must be equal to “ECS150FS”)
    uint16_t totalBlocks;		// Total amount of blocks of virtual disk
    uint16_t rootIndex;			// Root directory block index
    uint16_t dataIndex;			// Data block start index
    uint16_t numDataBlocks;		// Amount of data blocks
    uint8_t numFATBlocks;		// Number of blocks for FAT
    char padding[4079];			// Unused/Padding
};

struct __attribute__((packed)) FAT {
	uint16_t *arr;
};

struct __attribute__((packed)) rootEntry {
	char fileName[16];
	uint32_t fileSize;
	uint16_t dataBlockIndex;
	uint8_t padding[10];
};

struct __attribute__((packed)) rootDirectory {
	struct rootEntry rootEntry[MAX_FILES];
};


/* Global Variables */
struct superBlock super;
struct FAT fat;
struct rootDirectory root;

/* TODO: Phase 1 */
int fs_mount(const char *diskname)
{
	/* ECS150 Disk Format Error Check */
	if (block_disk_open(diskname) == -1) {												// Disk can't be opened
		return -1;
	} else if (block_read(0, &super) == -1) {											// Superblock can't be read
		return -1;
	} else if (1 + super.numFATBlocks + 1 + super.numDataBlocks != super.totalBlocks) { // Incorrect total blocks
		return -1;
	} else if (super.totalBlocks != block_disk_count()) {								// Block count off
		return -1;
	} else if (memcmp("ECS150FS", super.signature, 8) != 0) {   						// Incorrect Signature
        return -1;
	} else if (super.numFatBlocks + 1 != super.rootIndex) {								// Incorrect fat block start index
 		return -1;
	} else if (super.rootIndex + 1 != super.dataIndex) {								// Incorrect data block start index
		return -1;
	}

	/* FAT Array Mapping */
	fat.arr = (uint16_t*)malloc(super.numFatBlocks * BLOCK_SIZE * sizeof(uint16_t));
	void *buf = (void*)malloc(BLOCK_SIZE);
	for(int i = 1; i < super.rootIndex; i++) {
		if(block_read(i, buf) == -1) {
			return -1;
		} else {
			memcpy(fat.arr + (i-1)*BLOCK_SIZE, buffer, BLOCK_SIZE);
		}
	}

	if (fat.arr[0] != FAT_EOC) {
		return -1; 
	}

	/* Meta Information */
	if (block_read(super.rootIndex, &root) == -1) {
		return -1;
	}

	/* Sucessful Mount! */
	return 0; 
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	if (block_write(0, &super) == -1) {							// Superblock can't be read
		return -1;
	} else if (block_write(super_rootIndex, &root) == -1) {		// Root directory can't be written to
		return -1;
	}

	for(int i = 1; i < super.rootIndex; i++) {
		if (block_write(i, fat.arr + (i-1) * BLOCK_SIZE) == -1){
			return -1;
		}
	}

	if (block_disk_close() == -1) {
		return -1;
	}

	/* Sucessful Unmount! */
	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	printf("FS Info:\n");
	printf("Total # of Blocks: %i\n", super.totalBlocks);
	printf("Total # of FAT Blocks: %i\n", super.numFATBlocks);
	printf("Total # of Data Blocks: %i\n", super.numDataBlocks);
	printf("Root Directory Index: %i\n", super.rootIndex);
	printf("Data Block Start Index: %i\n", super.dataIndex);

	return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */

}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

