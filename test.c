#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include "ext2_fs.h"

#define SUPER_OFFSET 1024

int ext2fd;
struct ext2_super_block* sbp;
struct ext2_group_desc* gdp;
unsigned int block_size;
unsigned int num_groups;

void process_superblock();
void process_groups();
void process_free_blocks();
void process_free_inodes();
void process_inodes();
void format_time();
void process_directory(struct ext2_inode* inode, int inode_number);
void process_indirect(int inode_number, int block_num, int logical_offset, int level);

int main(int argc, char* argv[argc+1])
{
    if (argc != 2) {
        fprintf(stderr, "Error: Incorrect number of arguments.\n");
        exit(1);
    }

    char* filename = argv[1];
    ext2fd = open(filename, O_RDONLY);
    if (ext2fd == -1) {
        fprintf(stderr, "Error: Cannot open file.\n");
        exit(1);
    }

    process_superblock();
    process_groups();
    process_free_blocks();
    process_free_inodes();
    process_inodes();
}

/*
 * Process the superblock information.
 *
 * Places information into block_size global variable.
 * Prints out superblock information.
 */
void process_superblock()
{
    sbp = malloc(sizeof(struct ext2_super_block));
    pread(ext2fd, sbp, sizeof(struct ext2_super_block), SUPER_OFFSET);
    block_size = EXT2_MIN_BLOCK_SIZE << sbp->s_log_block_size;
    printf("SUPERBLOCK,%u,%u,%u,%u,%u,%u,%u\n",
            sbp->s_blocks_count,
            sbp->s_inodes_count,
            block_size,
            sbp->s_inode_size,
            sbp->s_blocks_per_group,
            sbp->s_inodes_per_group,
            sbp->s_first_ino);
}

/*
 * Process group information
 *
 * Gets the number of groups as well as the remainder blocks and inodes.
 * Allocates for and reads in group descriptor table.
 * Ouputs group information for each group.
 */
void process_groups()
{
    num_groups = ceil(sbp->s_blocks_count * 1.0 / sbp->s_blocks_per_group);
    unsigned int rem_blocks = sbp->s_blocks_count % sbp->s_blocks_per_group;
    unsigned int rem_inodes = sbp->s_inodes_count % sbp->s_inodes_per_group;

    gdp = malloc(num_groups * sizeof(struct ext2_group_desc));
    pread(ext2fd, gdp, sizeof(struct ext2_group_desc), SUPER_OFFSET + block_size);

    for (size_t i = 0; i < num_groups; i++) {
        bool output_remaining_blocks = (i == num_groups - 1 && rem_blocks != 0);
        bool output_remaining_inodes = (i == num_groups - 1 && rem_inodes != 0);
        printf("GROUP,%zu,%u,%u,%u,%u,%u,%u,%u\n",
                    i,
                    output_remaining_blocks ? rem_blocks : sbp->s_blocks_per_group,
                    output_remaining_inodes ? rem_inodes : sbp->s_inodes_per_group,
                    gdp[i].bg_free_blocks_count,
                    gdp[i].bg_free_inodes_count,
                    gdp[i].bg_block_bitmap,
                    gdp[i].bg_inode_bitmap,
                    gdp[i].bg_inode_table);
    }
}

/*
 * Processes the free block entries for each group by scanning the free block bitmap
 * 1 --> allocated block
 * 0 --> free block
 * byte location is calculated using bitmap, blocksize, and offset
 */
void process_free_blocks()
{
    for (size_t i = 0; i < num_groups; i++) {
        for (size_t j = 0; j < block_size; j++) {
            uint8_t byte_entry;
            pread(ext2fd, &byte_entry, 1, (gdp[i].bg_block_bitmap * block_size) + j);
            for (size_t k = 0; k < 8; k++)
                if (((byte_entry & (1 << k))) == 0)  // use k to create mask
                    printf("BFREE,%lu\n", i * sbp->s_blocks_per_group + (j * 8) + (k + 1));
        }
    }
}

/*
 * Processes the free inode entries for each group by scanning the free inode bitmap
 * 1 --> allocated inode
 * 2 --> free inode
 * byte location is calculated using bitmap, blocksize, and offset
 */
void process_free_inodes()
{
    for (size_t i = 0; i < num_groups; i++) {
        for (size_t j = 0; j < block_size; j++) {
            uint8_t byte_entry;
            pread(ext2fd, &byte_entry, 1, (gdp[i].bg_inode_bitmap * block_size) + j);
            for (size_t k = 0; k < 8; k++)
                if (((byte_entry & (1 << k)) == 0))  // use k to create mask
                    printf("IFREE,%lu\n", i * sbp->s_inodes_per_group + (j * 8) + (k + 1));
        }
    }
}

/*
 * Processes and prints information about allocated inodes for each group
 */
void process_inodes()
{
    struct ext2_inode inode;
    uint16_t file_type;

    for (unsigned int i = 0; i < num_groups; i++) {
        uint32_t inode_table_offset = block_size * gdp[i].bg_inode_table;

        for (unsigned int inode_number = 1; inode_number < sbp->s_inodes_count; inode_number++) {
            // read in inode
           //  pread(ext2fd, &inode, sizeof(struct ext2_inode), inode_table_offset + (inode_number * sizeof(struct ext2_inode)));
            pread(ext2fd, &inode, sizeof(struct ext2_inode), inode_table_offset + ((inode_number - 1) * sizeof(struct ext2_inode)));

            // if inode is valid
            if (inode.i_mode != 0 && inode.i_links_count != 0) {

                // assign file type
                file_type = inode.i_mode;
                if (file_type & 0x8000) file_type = 'f';
                else if (file_type & 0x4000) file_type = 'd';
                else if (file_type & 0xA000) file_type = 's';
                else file_type = '?';

                // get last change time, modification time, and access time
                char last_change_time[100];
                char modification_time[100];
                char access_time[100];
                format_time(inode.i_ctime, last_change_time);
                format_time(inode.i_mtime, modification_time);
                format_time(inode.i_atime, access_time);

                // print first 12 inode details
                printf("INODE,%u,%c,%o,%u,%u,%u,%s,%s,%s,%d,%d",
                            inode_number,
                            file_type,
                            inode.i_mode & 0xFFF,
                            inode.i_uid,
                            inode.i_gid,
                            inode.i_links_count,
                            last_change_time,
                            modification_time,
                            access_time,
                            inode.i_size,
                            inode.i_blocks);

                for (int k = 0; k < EXT2_N_BLOCKS; k++) {
                    printf(",%u", inode.i_block[k]);
                }
                printf("\n");

                if (file_type == 'd')
                    process_directory(&inode, inode_number);

                if (inode.i_block[EXT2_IND_BLOCK] != 0)
                    process_indirect(inode_number, inode.i_block[EXT2_IND_BLOCK], 12, 1);
                if (inode.i_block[EXT2_DIND_BLOCK] != 0) {}
                    process_indirect(inode_number, inode.i_block[EXT2_DIND_BLOCK], 12 + 256, 2);
                if (inode.i_block[EXT2_TIND_BLOCK] != 0) {}
                    process_indirect(inode_number, inode.i_block[EXT2_TIND_BLOCK], 12 + 256 + (256 * 256), 3);

            }
        }
    }
}

void format_time(uint32_t t, char* buf)
{
    time_t timer = t;
    struct tm* ts = gmtime(&timer);
    strftime(buf, 80, "%m/%d/%y %H:%M:%S", ts);
}

/*
 * Process directory inode
 * inode is guarenteed of directory type
 */
void process_directory(struct ext2_inode* inode, int inode_number)
{
    unsigned char block[block_size];
    struct ext2_dir_entry* dep;
    unsigned int position = 0;

    for (int i = 0; i < EXT2_NDIR_BLOCKS; i++) {

        // termination. directory entry is not active
        if (inode->i_block[i] == 0)
            return;

        pread(ext2fd, block, block_size, inode->i_block[i] * block_size);
        dep = (struct ext2_dir_entry*) block;

        while ((position < inode->i_size) && dep->file_type) {
            if (dep->inode > 0 && dep->name_len > 0) {
                char file_name[EXT2_NAME_LEN + 1];
                strncpy(file_name, dep->name, dep->name_len);
                file_name[dep->name_len] = 0;
                printf("DIRENT,%u,%u,%u,%u,%u,'%s'\n",
                                inode_number,
                                position,
                                dep->inode,
                                dep->rec_len,
                                dep->name_len,
                                file_name);
                position += dep->rec_len;
                dep = (void *) dep + dep->rec_len;
            }
        }
    }

    if (inode->i_block[EXT2_IND_BLOCK] != 0)
        process_indirect(inode_number, inode->i_block[EXT2_IND_BLOCK], 12, 1);
    if (inode->i_block[EXT2_DIND_BLOCK] != 0)
				process_indirect(inode_number, inode->i_block[EXT2_DIND_BLOCK], 12 + 256, 2);
    if (inode->i_block[EXT2_TIND_BLOCK] != 0)
				process_indirect(inode_number, inode->i_block[EXT2_TIND_BLOCK], 12 + 256 + (256 * 256), 3);
}

/*
 * Processes the indirect block of inode given the correct logical offset
 */
void process_indirect(int inode_number, int block_num, int logical_offset, int level)
{
    uint32_t indirect_block[block_size];
    pread(ext2fd, &indirect_block, block_size, block_num * block_size);
    int pointer_capacity = block_size / sizeof(uint32_t);

    for (int i = 0; i < pointer_capacity; i++) {
        if (indirect_block[i] == 0)
            continue;

        printf("INDIRECT,%u,%u,%u,%u,%u\n",
                    inode_number,
                    level,
                    logical_offset + i,
                    block_num,
                    indirect_block[i]);

				if (level > 1)
					process_indirect(inode_number, indirect_block[i], logical_offset + i, level-1);
    }
}