#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>
#include "disk.h"
#include "fs.h"

#define BLOCK_SIZE 4096
#define FAT_EOC 0xFFFF

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
	char fileName[FS_FILENAME_LEN];
	uint32_t fileSize;
	uint16_t dataBlockIndex;
	uint8_t padding[10];
};

struct __attribute__((packed)) rootDirectory {
	struct rootEntry rootEntry[FS_FILE_MAX_COUNT];
};

struct __attribute__((packed)) fileEntry {
	char fileName[FS_FILENAME_LEN];
	uint8_t offset;
};

struct __attribute__((packed)) fileDirectory {
	struct fileEntry fileEntry[FS_OPEN_MAX_COUNT];
	uint8_t numFilesOpen;
};


/* Global Variables */
struct superBlock super;
struct FAT fat;
struct rootDirectory root;
struct fileDirectory open_files;

/* TODO: Phase 1 */
int fs_mount(const char *diskname)
{
	printf("\n MOUNT THIS syntax \n");
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
	} else if (super.numFATBlocks + 1 != super.rootIndex) {								// Incorrect fat block start index
 		return -1;
	} else if (super.rootIndex + 1 != super.dataIndex) {								// Incorrect data block start index
		return -1;
	}

	/* FAT Array Mapping */
	fat.arr = (uint16_t*)malloc(super.numFATBlocks * BLOCK_SIZE * sizeof(uint16_t));
	void *buf = (void*)malloc(BLOCK_SIZE);
	for(int i = 1; i < super.rootIndex; i++) {
		if(block_read(i, buf) == -1) {
			return -1;
		} else {
			memcpy(fat.arr + (i-1)*BLOCK_SIZE, buf, BLOCK_SIZE);
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
	} else if (block_write(super.rootIndex, &root) == -1) {		// Root directory can't be written to
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
	if (filename == NULL || strlen(filename) > FS_FILENAME_LEN) {
		return -1;
	}


	/* Traverse root directory before creation (Check if file exists and if max file count not exceeded) */
	int fileCount = 0;
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (strcmp((char*)root.rootEntry[i].fileName, filename) == 0) {
			return -1; 
		} 
		if (root.rootEntry[i].fileName[0] != '\0') { // Non empty entry found, increment file count
			fileCount++;										
		}
	}

	/* Max File Count Exceeded */
	if (fileCount >= FS_FILE_MAX_COUNT) {
		return -1;
	}

	/* Create File */
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (root.rootEntry[i].fileName[0] == '\0') {								// Empty entry found
			memcpy(root.rootEntry[i].fileName, filename, FS_FILENAME_LEN); 		// Copy file name
			root.rootEntry[i].fileSize = 0; 									// Set root dir size to 0
			root.rootEntry[i].dataBlockIndex = FAT_EOC;  							// first data block starts from 0xFFFF
			block_write(super.rootIndex, &root);
			break;
		}
	}

	return 0;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	if (filename == NULL) {
		return -1;
	}

	uint16_t index = FAT_EOC;
	bool fileFound = false;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (strcmp((char*)root.rootEntry[i].fileName, filename) == 0) {
			fileFound = true;
			index = root.rootEntry[i].dataBlockIndex;

			/* Destroy Root Entry */
			root.rootEntry[i].fileName[0] = '\0';
			root.rootEntry[i].fileSize = 0;
			root.rootEntry[i].dataBlockIndex = FAT_EOC;
			block_write(super.rootIndex, &root);
			break;
		}
	}

	if (!fileFound) {
		return -1;
	}

	while (index != FAT_EOC) {
		uint16_t next = fat.arr[index];
		fat.arr[index] = 0;
		index = next;
	}

	return 0;
}

int fs_ls(void)
{
	printf("FS ls:\n");
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (root.rootEntry[i].fileName[0] != '\0') {
			struct rootEntry entry = root.rootEntry[i];
			printf("File Name: %s, Size: %i, Data Block Index: %i\n", (char*)entry.fileName, entry.fileSize, entry.dataBlockIndex);
		}
	}
	return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	if (filename == NULL || open_files.numFilesOpen == FS_OPEN_MAX_COUNT) {
		return -1;
	}

	bool fileFound = false;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (strcmp((char*)root.rootEntry[i].fileName, filename) == 0) {
			fileFound = true;
		}
	}

	if (!fileFound) {
		return -1;
	}

	for (int fd = 0; fd < FS_OPEN_MAX_COUNT; fd++) {
		if (root.rootEntry[fd].fileName[0] != '\0'){	
			strcpy(open_files.fileEntry[fd].fileName, filename);
			open_files.numFilesOpen++;
			open_files.fileEntry[fd].offset = 0; 
			return fd;
		}
	}

	return -1;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	if (fd < 0 || fd > 31 || open_files.numFilesOpen == 0 || open_files.fileEntry[fd].fileName[0] == '\0') { // Out of bounds and File Existence Check
		return -1;
	}

	open_files.fileEntry[fd].fileName[0] = '\0';
	open_files.fileEntry[fd].offset = 0;
	open_files.numFilesOpen--;
	return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	if (fd < 0 || fd > 31 || open_files.numFilesOpen == 0 || open_files.fileEntry[fd].fileName[0] == '\0') { // Out of bounds and File Existence Check
		return -1;
	}

	char *openFileName = open_files.fileEntry[fd].fileName;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (strcmp(root.rootEntry[i].fileName, openFileName) == 0) {
			return root.rootEntry[i].fileSize;
		}
	}
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	if (fd < 0 || fd > 31 || offset < 0 || offset > root.rootEntry[fd].fileSize) {
		return -1;
	}

	open_files.fileEntry[fd].offset = 0;
	return 0;
}

uint16_t dataBlockIndex(int fd, uint16_t offset)
{
	char* name = open_files.fileEntry[fd].fileName;
	uint16_t index;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (strcmp(root.rootEntry[i].fileName, name) == 0) {
			index = root.rootEntry[i].dataBlockIndex;
			break;
		}
	}
	return index + offset;
}

uint16_t findOpenFAT()
{
	uint16_t index;
	for (int i = 0; i < super.numDataBlocks; i++) {
		if(fat.arr[i] == 0) {
			index = i;
			return index;
		}
	}
	return -1;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	char* name = open_files.fileEntry[fd].fileName;
	if (fd < 0 || fd > 31 || open_files.numFilesOpen == 0 || name[0] == '\0') { // Out of bounds and File Existence Check
		return -1;
	} else if (count <= 0 || buf == NULL) {
		return 0;
	}

	/* Find File in Root Directory */
	bool fileFound = false;
	struct rootEntry root_block;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (strcmp(root.rootEntry[i].fileName, name) == 0) {
			fileFound = true;
			root_block = root.rootEntry[i];
			break;
		}
	}

	/* File Not Found */
	if (!fileFound) {
		return -1;
	}

	/* File Empty: No Allocated Blocks, Find First Availiable Block in FAT */
	uint8_t startIndex;
	if (fs_stat(fd) == 0) {
		startIndex = findOpenFAT();
		if (startIndex == -1) {
			return -1;
		}
		root_block.dataBlockIndex = startIndex;
	}

	startIndex = root_block.dataBlockIndex;
	uint8_t actualIndex = startIndex + super.dataIndex;
	uint8_t offset = open_files.fileEntry[fd].offset;

	void *bounce = (void*)malloc(BLOCK_SIZE);
	block_read((size_t)actualIndex, bounce);
	size_t block_offset = offset % BLOCK_SIZE;
	bool increase = false;
	int byte_count = 0;

	for (int i = 0; i < count; i++) {
		open_files.fileEntry[fd].offset = offset;
		if (offset >= fs_stat(fd)) {
			increase = true;
		}

		if (block_offset >= BLOCK_SIZE) {
			block_offset = 0;
			if (increase) {
				uint16_t fatIndex = findOpenFAT();
				if (fatIndex == -1) {
					return byte_count;
				}
				fat.arr[startIndex] = fatIndex;
				startIndex = fatIndex;
			} else {
				startIndex = dataBlockIndex(offset, startIndex);
			}
			uint16_t actualIndex = startIndex + super.dataIndex;
            block_read((size_t )actualIndex, bounce);
		}
		
		memcpy(bounce + block_offset, buf + i, 1);
		byte_count++;
		uint16_t actualIndex = startIndex + super.dataIndex;
		block_write((size_t )actualIndex, bounce);
		
		if (increase) {
			root_block.fileSize++;
		}
		block_offset++;
		offset++;
	}
	return byte_count;

}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
		/* TODO: Phase 4 */
	char* name = open_files.fileEntry[fd].fileName;
	if (fd < 0 || fd > 31 || open_files.numFilesOpen == 0 || name[0] == '\0') { // Out of bounds and File Existence Check
		return -1;
	} else if (count <= 0 || buf == NULL || fs_stat(fd) == 0) {
		return 0;
	}

	uint8_t offset = open_files.fileEntry[fd].offset;

	/* Find File in Root Directory */
	bool fileFound = false;
	uint8_t startIndex;
	struct rootEntry root_block;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (strcmp(root.rootEntry[i].fileName, name) == 0) {
			fileFound = true;
			root_block = root.rootEntry[i];
			startIndex = root_block.dataBlockIndex;
			break;
		}
	}

	/* File Not Found */
	if (!fileFound) {
		return -1;
	}

	int bytes = 0;
	uint8_t dataIndex = dataBlockIndex(offset, startIndex);
	uint8_t actualIndex = startIndex + super.dataIndex;
	void *bounce = (void*)malloc(BLOCK_SIZE);
	block_read((size_t)actualIndex, bounce);
	size_t block_offset = offset % BLOCK_SIZE;

	for (int i = 0; i < count; i++, bytes++) {
		open_files.fileEntry[fd].offset = offset;

		if (offset >= fs_stat(fd)) {
			return bytes;
		}

		if (block_offset >= BLOCK_SIZE) {
			block_offset = 0;
			dataIndex = dataBlockIndex(offset, startIndex);
			if (dataIndex == -1) {
				return bytes;
			}
			uint16_t actualIndex = dataIndex + super.dataIndex;
            block_read((size_t)actualIndex, bounce);
		}
		memcpy(buf + i, bounce + block_offset, 1);
	}
	return bytes;
}

