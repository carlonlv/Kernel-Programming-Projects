#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"
#include "ext2general.h"

int main(int argc, char **argv) {
    char *path;
    if (argc != 3) {
      fprintf(stderr, "Usage: %s <virtual disk name> '-a' <absolute path>\n", argv[0]);
      return EINVAL;
    }

    unsigned char *disk = read_image(argv[1]);
    if (disk == NULL) {
		  perror("mmap");
		  return EINVAL;
    }

    path = argv[2];

    if (path[0] != '/') {
		  fprintf(stderr, "Not an absolute path.\n");
		  return EINVAL;
    }

    // check if directory alreay exist
    char buf[strlen(path)];
		strcpy(buf, path);
		char *ptr_to_buf = buf;
    struct ext2_dir_entry_2 *dest_file = find_dir_entry(disk, ptr_to_buf);
    if(dest_file != NULL ){
        fprintf(stderr, "This destination file path already taken.\n");
    	return EEXIST;
    }

    // get the parent directry inode and make sure it's parent directory exist
    unsigned int parent_inodeNum = find_parent_inode(disk, path);
    if(parent_inodeNum == 0){
        fprintf(stderr, "parent directory does not exist.\n");
    	return ENOENT;
    }
    struct ext2_inode *parent_inode = get_inode_table(disk) + TO_INDEX(parent_inodeNum);

		char* filename = find_file_name_by_path(path);
		if (strlen(filename) > (EXT2_BLOCK_SIZE - sizeof(struct ext2_dir_entry_2 *))) {
			return ENAMETOOLONG;
		}

    // Initilize a new node and add a new block with "." and ".." directories.
    unsigned int new_inodeNum = find_free_inode(disk);
    if(new_inodeNum == 0){
        fprintf(stderr, "No more space for inode\n");
        return ENOSPC;
    }
    update_bitmap(disk, 'i', new_inodeNum, 1);
    struct ext2_inode *new_inode = get_inode_table(disk) + TO_INDEX(new_inodeNum);
    init_inode(new_inode,EXT2_S_IFDIR, EXT2_BLOCK_SIZE);
    new_inode->i_links_count = 2;

    unsigned int new_blockNum = find_free_block(disk);
     if(new_blockNum == 0){
        update_bitmap(disk, 'i', new_inodeNum, 0);
        fprintf(stderr, "No more space for block\n");
    	return ENOENT;
    }
    update_bitmap(disk, 'b', new_blockNum, 1);
    new_inode -> i_block[0] = new_blockNum;
    new_inode -> i_blocks += 1;

    struct ext2_dir_entry_2 *dir_entry = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * new_blockNum);
    dir_entry->inode = new_inodeNum;
    dir_entry->name_len = 1;
    dir_entry->rec_len = EXT2_BLOCK_SIZE;
    dir_entry->name[0] = '.';
    dir_entry->file_type = EXT2_FT_DIR;

    allocate_space_in_block(disk, 2, new_blockNum, parent_inodeNum, "..", EXT2_FT_DIR);
    parent_inode->i_links_count += 1;

    // Add the new direcotry to its parent's block.
    int found = 0;
    for(int i = 0; i < TOTAL_BLOCK_COUNT; i++){
        if (parent_inode->i_block[i]) {
            found = allocate_space_in_block(disk,strlen(filename), parent_inode -> i_block[i], new_inodeNum, filename, EXT2_FT_DIR);
            if(found){
            	break;
            }
        }
    }
    // if we cannot find space at existen block then we add new block
    if(!found){
        unsigned int new_block_Num = find_free_block(disk);
        update_bitmap(disk, 'b', new_block_Num, 1);

        int free_space = 0;
        int i;
        for(i = 0; i < TOTAL_BLOCK_COUNT; i++) {
            if (parent_inode->i_block[i] == 0){
                free_space = 1;
                break;
            }
        }

        if (free_space){
            parent_inode->i_block[i] = new_block_Num;
            parent_inode->i_blocks += 1;
            struct ext2_dir_entry_2 *parent_entry = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * new_block_Num);
            parent_entry->inode = new_inodeNum;
            parent_entry->name_len = strlen(filename);
            parent_entry->rec_len = EXT2_BLOCK_SIZE;
            for(int j=0;j<strlen(filename);j++){
               parent_entry->name[j] = filename[j];
            }
            parent_entry->file_type = EXT2_FT_DIR;
            found = 1;
        }
    }
    if (!found) {
    		//Release all occupied resourses, need to roll back
    		update_bitmap(disk, 'b', new_inodeNum, 0);
				update_bitmap(disk, 'i', new_inodeNum, 0);
				fprintf(stderr, "No space in parent directory.\n");
        return ENOSPC;
    } else {
        return 0;
    }
}
