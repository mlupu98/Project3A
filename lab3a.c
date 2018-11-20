//
// Created by Matei Lupu on 11/17/18.
//
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <errno.h>

#include "ext2_fs.h"

#define superblockOffset 1024
#define groupOffset 1024
#define ext2BlockSize 1024

struct ext2_super_block superBuffer;
struct ext2_inode inodeBuffer;
struct ext2_group_desc *groupBuffer;

// Superblock Summary

void createSuperblockSummary(int fd, const char *path)
{
    int superblockFd = creat(path, S_IRWXU); // file owner has read, write and execute permissions

    int blockSize = 1024;
    int retSize = blockSize << superBuffer.s_log_block_size;

    pread(fd, &superBuffer, sizeof(struct ext2_super_block), superblockOffset);

    dprintf(superblockFd, "SUPERBLOCK,");

    dprintf(superblockFd, "%d,", superBuffer.s_blocks_count);

    dprintf(superblockFd, "%d,", superBuffer.s_inodes_count);

    dprintf(superblockFd, "%d,", retSize);

    dprintf(superblockFd, "%d,", superBuffer.s_inode_size);

    dprintf(superblockFd, "%d,", superBuffer.s_blocks_per_group);

    dprintf(superblockFd, "%d,", superBuffer.s_inodes_per_group);

    dprintf(superblockFd, "%d\n", superBuffer.s_first_ino);

    close(superblockFd);
}

// Group Summary

int createGroupSummary(int fd, const char *path) // returns number of froups
{
    int numberOfGroups = superBuffer.s_blocks_count / superBuffer.s_blocks_per_group + 1;

    int blockRemainder = superBuffer.s_blocks_count % superBuffer.s_blocks_per_group;
    int inodeRemainder = superBuffer.s_inodes_count % superBuffer.s_inodes_per_group;

    groupBuffer = malloc(numberOfGroups * sizeof(struct ext2_group_desc));

    int groupFd = creat(path, S_IRWXU);

    int i = 0;
    for (; i < numberOfGroups; i++)
    {
        pread(fd, &groupBuffer[i], sizeof(struct ext2_group_desc), superblockOffset + groupOffset + sizeof(struct ext2_group_desc) * i);

        dprintf(groupFd, "GROUP,");

        dprintf(groupFd, "%d,", i);

        if (blockRemainder == 0)
        {
            dprintf(groupFd, "%d,", superBuffer.s_blocks_per_group);
        }
        else
        {
            if ((i - 1) == 0)
            {
                dprintf(groupFd, "%d,", blockRemainder);
            }
            else
            {
                dprintf(groupFd, "%d,", superBuffer.s_blocks_per_group);
            }
        }

        if (inodeRemainder == 0)
        {
            dprintf(groupFd, "%d,", superBuffer.s_inodes_per_group);
        }
        else
        {
            if ((i - 1) == 0)
            {
                dprintf(groupFd, "%d,", inodeRemainder);
            }
            else
            {
                dprintf(groupFd, "%d,", superBuffer.s_inodes_per_group);
            }
        }

        dprintf(groupFd, "%d,", groupBuffer[i].bg_free_blocks_count);

        dprintf(groupFd, "%d,", groupBuffer[i].bg_free_inodes_count);

        dprintf(groupFd, "%d,", groupBuffer[i].bg_block_bitmap);

        dprintf(groupFd, "%d,", groupBuffer[i].bg_inode_bitmap);

        dprintf(groupFd, "%d\n", groupBuffer[i].bg_inode_table);
    }

    close(groupFd);

    return numberOfGroups;
}

// Free Block Summary

void createFreeSummary(int fd, const char *groupPath, const char *inodePath, int numOfGroups)
{
    int freeGroupFd = creat(groupPath, S_IRWXU);
    int freeInodeFd = creat(inodePath, S_IRWXU);

    uint8_t uint8Buffer;

    int i = 0;
    for (; i < numOfGroups; i++)
    {
        int blockSize = 1024 << superBuffer.s_log_block_size;

        int j = 0;
        for (; j < blockSize; j++)
        {
            int bitmapLocation = EXT2_MIN_BLOCK_SIZE << superBuffer.s_log_block_size;

            pread(fd, &uint8Buffer, 1, groupBuffer[i].bg_block_bitmap * bitmapLocation + j);

            int k = 0;
            for (; k < 8; k++)
            {
                if ((uint8Buffer & 1 << k) == 0) // 0 means free
                {
                    dprintf(freeGroupFd, "BFREE,");

                    int blockNumber = i * superBuffer.s_blocks_per_group + j * 8 + k + 1;
                    dprintf(freeGroupFd, "%d\n", blockNumber);
                }
            }

            pread(fd, &uint8Buffer, 1, groupBuffer[i].bg_inode_bitmap * bitmapLocation + j);

            k = 0;
            for (; k < 8; k++)
            {
                if ((uint8Buffer & 1 << k) == 0) // 0 means free
                {
                    dprintf(freeInodeFd, "IFREE,");

                    int inodeNumber = i * superBuffer.s_inodes_per_group + j * 8 + k + 1;
                    dprintf(freeInodeFd, "%d\n", inodeNumber);
                }
            }
        }
    }

    close(freeGroupFd);
    close(freeInodeFd);
}

void formatTime(uint32_t t, char *buf)
{
    time_t timer = t;
    struct tm *ts = gmtime(&timer);
    strftime(buf, 80, "%m/%d/%y %H:%M:%S", ts);
}

void processIndirect(int fd, int inodeFd, int inodeNum, int blockNum, int offset, int level)
{
    uint32_t indirectBlock[ext2BlockSize];
    pread(fd, &indirectBlock, ext2BlockSize, blockNum * ext2BlockSize);

    int lim = (ext2BlockSize / sizeof(uint32_t));
    for (int i = 0; i < lim; i++)
    {
        while (indirectBlock[i] == 0)
        {
            dprintf(inodeFd, "INDIRECT, %u, %u %u, %u, %u\n", inodeNum, level, offset, blockNum, indirectBlock[i]);

            if (level > 1)
                processIndirect(fd, inodeFd, inodeNum, indirectBlock[i], offset + i, level - 1);
        }
    }
}

void processDirectory(int fd, int inodeFd, struct ext2_inode inode, int inodeNum)
{
    unsigned char block[ext2BlockSize]; // TODO: Check blocksize
    struct ext2_dir_entry *dir;
    unsigned int j = 0;

    for (int i = 0; i < EXT2_NDIR_BLOCKS; i++)
    {
        if (inode.i_block[i] == 0)
            return;
        pread(fd, block, ext2BlockSize, inode.i_block[i] * ext2BlockSize);
        dir = (struct ext2_dir_entry *)block;
        while ((j < inode.i_size) && dir->file_type)
        {
            if (dir->inode > 0 && dir->name_len > 0)
            {
                char fileName[EXT2_NAME_LEN + 1];
                strncpy(fileName, dir->name, dir->name_len);
                fileName[dir->name_len] = 0;
                dprintf(inodeFd, "DIRENT,%u,%u,%u,%u,%u,'%s'\n",
                        inodeNum,
                        j,
                        dir->inode,
                        dir->rec_len,
                        dir->name_len,
                        fileName);
                j += dir->rec_len;
                dir = (void *)dir + dir->rec_len;
            }
        }
    }

    // Each call represents a different level.
    if (inode.i_block[EXT2_IND_BLOCK] != 0)
        processIndirect(fd, inodeFd, inodeNum, inode.i_block[EXT2_IND_BLOCK], 12, 1);
    if (inode.i_block[EXT2_DIND_BLOCK] != 0)
        processIndirect(fd, inodeFd, inodeNum, inode.i_block[EXT2_DIND_BLOCK], 12 + 256, 2);
    if (inode.i_block[EXT2_TIND_BLOCK] != 0)
        processIndirect(fd, inodeFd, inodeNum, inode.i_block[EXT2_TIND_BLOCK], 12 + 256 + (256 * 256), 3);
}

// Inode Summary
void createInodeSummary(int fd, const char *path, int numOfGroups)
{
    int inodeFd = creat(path, S_IRWXU);
    uint16_t fileType;
    for (int i = 0; i < numOfGroups; i++)
    {
        for (unsigned int j = 1; j < superBuffer.s_inodes_count; j++)
        {
            pread(fd, &inodeBuffer, sizeof(struct ext2_inode), superblockOffset + 4 * groupOffset + sizeof(struct ext2_inode) * j);

            if (inodeBuffer.i_mode != 0 && inodeBuffer.i_links_count != 0)
            {
                fileType = inodeBuffer.i_mode;
                if (fileType & 0x8000)
                    fileType = 'f';
                else if (fileType & 0x4000)
                    fileType = 'd';
                else if (fileType & 0xA000)
                    fileType = 's';
                else
                    fileType = '?';

                // Get Time from Inode
                char lastChangeTime[100];
                char modificationTime[100];
                char accessTime[100];
                formatTime(inodeBuffer.i_ctime, lastChangeTime);
                formatTime(inodeBuffer.i_mtime, modificationTime);
                formatTime(inodeBuffer.i_atime, accessTime);

                dprintf(inodeFd, "INODE,%u,%c,%o,%u,%u,%u,%s,%s,%s,%d,%d",
                        j,
                        fileType,
                        inodeBuffer.i_mode & 0xFFF,
                        inodeBuffer.i_uid,
                        inodeBuffer.i_gid,
                        inodeBuffer.i_links_count,
                        lastChangeTime,
                        modificationTime,
                        accessTime,
                        inodeBuffer.i_size,
                        inodeBuffer.i_blocks);

                for (int k = 0; k < EXT2_N_BLOCKS; k++)
                {
                    dprintf(inodeFd, ",%u", inodeBuffer.i_block[k]);
                }
                dprintf(inodeFd, "\n"); // or printf?

                if (fileType == 'd')
                    processDirectory(fd, inodeFd, inodeBuffer, j);

                // Each call is different directory.
                if (inodeBuffer.i_block[EXT2_IND_BLOCK] != 0)
                    processIndirect(fd, inodeFd, j, inodeBuffer.i_block[EXT2_IND_BLOCK], 12, 1);
                if (inodeBuffer.i_block[EXT2_DIND_BLOCK] != 0)
                {
                    processIndirect(fd, inodeFd, j, inodeBuffer.i_block[EXT2_DIND_BLOCK], 12 + 256, 2);
                }
                if (inodeBuffer.i_block[EXT2_TIND_BLOCK] != 0)
                {
                    processIndirect(fd, inodeFd, j, inodeBuffer.i_block[EXT2_TIND_BLOCK], 12 + 256 + (256 * 256), 3);
                }
            }
        }
    }
}

// Change this code.
void printCSV(const char *filename)
{
    int fd = open(filename, O_RDONLY);
    char buf[1024];
    int len;
    while ((len = read(fd, buf, 1024)) > 0)
    {
        write(1, buf, len);
    }
    close(fd);
}

int main(int argc, char *argv[])
{

    const char *superPath = "superCSV.csv";
    const char *groupPath = "groupCSV.csv";
    const char *freeGroupPath = "freeGCSV.csv";
    const char *freeInodePath = "freeICSV.csv";
    const char *inodePath = "inodeCSV.csv";

    char *filename;

    if (argc != 2)
    {
        fprintf(stderr, "Error: Incorrect number of arguments.\n");
        exit(1);
    }

    if (argv[1] != NULL)
    {
        filename = argv[1];
    }
    else
    {
        fprintf(stderr, "Error: need to pass filesystem image as parameter");
        exit(1);
    }

    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "Could not open file with specified image");
        exit(1);
    }

    createSuperblockSummary(fd, superPath);
    int numberOfGroups = createGroupSummary(fd, groupPath);
    createFreeSummary(fd, freeGroupPath, freeInodePath, numberOfGroups);
    createInodeSummary(fd, inodePath, numberOfGroups);

    printCSV(superPath);
    printCSV(groupPath);
    printCSV(freeGroupPath);
    printCSV(freeInodePath);
    printCSV(inodePath);

    close(fd);

    //    SUPERBLOCK
    //    total number of blocks (decimal)
    //    total number of i-nodes (decimal)
    //    block size (in bytes, decimal)
    //    i-node size (in bytes, decimal)
    //    blocks per group (decimal)
    //    i-nodes per group (decimal)
    //    first non-reserved i-node (decimal)

    //    GROUP
    //    group number (decimal, starting from zero)
    //    total number of blocks in this group (decimal)
    //    total number of i-nodes in this group (decimal)
    //    number of free blocks (decimal)
    //    number of free i-nodes (decimal)
    //    block number of free block bitmap for this group (decimal)
    //    block number of free i-node bitmap for this group (decimal)
    //    block number of first block of i-nodes in this group (decimal)

    return 0;
}
