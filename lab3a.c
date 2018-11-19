//
// Created by Matei Lupu on 11/17/18.
//
#include <stdio.h>
#include <ext2_fs.h>

#define superblockOffset 1024
#define groupOffset     1024

struct ext2_super_block superBuffer;
struct ext2_inode       inodeBuffer;
struct ext2_group_desc  groupBuffer;

void createSuperblockSummary(int fd, const char* path)
{
    int superblockFd = creat( path, S_IRWXU ); // file owner has read, write and execute permissions

    pread( fd, &superBuffer, sizeof(ext2_super_block), superblockOffset )

    dprintf(superblockFd, "SUPERBLOCK,");

    dprintf(superblockFd, "%d,", superBuffer.s_blocks_count;

    dprintf(superblockFd, "%d,", superBuffer.s_inodes_count);

    dprintf(superblockFd, "%d,", superBuffer.s_log_block_size);

    dprintf(superblockFd, "%d,", superBuffer.s_inode_size);

    dprintf(superblockFd, "%d,", superBuffer.s_blocks_per_group);

    dprintf(superblockFd, "%d,", superBuffer.s_inodes_per_group);

    dprintf(superblockFd, "%d\n,", superBuffer.s_first_ino);

    close(superblockFd);

}

int createGroupSummary(int fd, const char* path) // returns number of froups
{
    int numberOfGroups = superBuffer.s_blocks_count / superBuffer.s_blocks_per_group + 1;

    int blockRemainder = superBuffer.s_blocks_count % superBuffer.s_blocks_per_group;
    int inodeRemainder = superBuffer.s_inodes_count % superBuffer.s_inodes_per_group;

    groupBuffer = malloc(numberOfGroups*sizeof(struct ext2_group_desc));

    int groupFd = creat( path, S_IRWXU );

    int i = 0;
    for(; i < numberOfGroups; i++)
    {
        pread( fd, &groupBuffer[i], sizeof(ext2_group_desc), superblockOffset + groupOffset + sizeof(struct ext2_group_desc)*i )

        dprintf(groupFd, "GROUP,");

        dprintf(groupFd, "%d,", i);

        if( blockRemainder == 0 )
        {
            dprintf(groupFd, "%d,", superBuffer.s_blocks_per_group);
        }
        else
        {
            if( (i-1) == 0 )
            {
                dprintf(groupFd, "%d,", blockRemainder);
            }
        }

        if( inodeRemainder == 0 )
        {
            dprintf(groupFd, "%d,", superBuffer.s_inodes_per_group);
        }
        else
        {
            if( (i-1) == 0 )
            {
                dprintf(groupFd, "%d,", inodeRemainder);
            }
        }

        dprintf(groupFd, "%d,", groupBuffer[i].bg_free_blocks_count);

        dprintf(groupFd, "%d,", groupBuffer[i].bg_free_inodes_count);

        dprintf(groupFd, "%d,", groupBuffer[i].bg_block_bitmap);

        dprintf(groupFd, "%d,", groupBuffer[i].bg_inode_bitmap);

        dprintf(groupFd, "%d\n,", groupBuffer[i].bg_inode_table);

    }

    close(groupFd);
}

void createFreeSummary(int fd, const char* groupPath, const char* inodePath int numOfGroups)
{
    int freeGroupFd = creat( groupPath, S_IRWXU );
    int freeInodeFd = creat( inodePath, S_IRWXU );

    uint8_t uint8Buffer;

    int i = 0;
    for(; i < numOfGroups; i++ )
    {
        int blockSize = 1024 << superBuffer.s_log_block_size;

        int j = 0;
        for(; j < blockSize; j++)
        {
            int bitmapLocation = EXT2_MIN_BLOCK_SIZE << superBuffer.s_log_block_size;

            pread( fd, &uint8Buffer, 1, groupBuffer[i].bg_block_bitmap * bitmapLocation + j );

            int k = 0;
            for(; k < 8; k++)
            {
                if( (uint8Buffer & 1 << k) == 0 ) // 0 means free
                {
                    dprintf( freeGroupFd, "BFREE,");

                    int blockNumber = i*superBuffer.s_blocks_per_group + j*8 + k + 1;
                    dprintf( freeGroupFd, "%d\n,", blockNumber);
                }
            }

            pread( fd, &uint8Buffer, 1, groupBuffer[i].bg_inode_bitmap * bitmapLocation + j );

            k = 0;
            for(; k < 8; k++)
            {
                if( (uint8Buffer & 1 << k) == 0 ) // 0 means free
                {
                    dprintf( freeInodeFd, "IFREE,");

                    int inodeNumber = i*superBuffer.s_inodes_per_group + j*8 + k + 1;
                    dprintf( freeInodeFd, "%d\n,", inodeNumber);
                }
            }

        }

    }

    close(freeGroupFd);
    close(freeInodeFd);
}

void createInodeSummary(int fd, const char* path, numOfGroups)
{

}



int main( int argc, char* argv[])
{

    const char* superPath      = "superCSV.csv";
    const char* groupPath      = "groupCSV.csv";
    const char* freeGroupPath  = "freeGCSV.csv";
    const char* freeInodePath  = "freeICSV.csv";
    const char* inodePath      = "inodeCSV.csv";

    char* filename;

    if(argv[1] != NULL)
    {
        char *filename = argv[1];
    }
    else
    {
        fprintf(stderr, "Error: need to pass filesystem image as parameter");
        exit(1);
    }

    int fd = open(filename, O_RDONLY);
    if( fd < 0 )
    {
        fprintf( stderr, "Could not open file with specified image" );
        exit(1);
    }


    createSuperblockSummary(fd, superPath);
    int numberOfGroups = createGroupSummary(fd, groupPath);
    createFreeSummary(fd, freeGroupPath, freeInodePath, numberOfGroups);
//  createInodeSummary(fd, inodePath, numberOfGroups);

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