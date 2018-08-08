#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <math.h>
#include "ext2.h"
#include "ext2general.h"

unsigned int have_enough_blocks(unsigned char *disk, struct stat *file_info);

void copy_data_to_block(unsigned char *disk, unsigned int block_num, char *buffer, unsigned int bytes_read);

int main(int argc, char **argv) {

	//check if argument is valid
	if (argc != 4) {
    fprintf(stderr, "Usage: %s <virtual disk name> '-a' <absolute path>\n", argv[0]);
    return EINVAL;
  }

  unsigned char *disk = read_image(argv[1]);
  if (disk == NULL) {
		perror("mmap\n");
		return EINVAL;
  }

	char *native_path = argv[2];
	struct stat buf;
	if(lstat(native_path, &buf) < 0) {
		perror("lstat\n");
		return EINVAL;
	}

	if (!S_ISREG(buf.st_mode)) {
		fprintf(stderr, "The source file path provided is not a regular file.\n");
		return EINVAL;
	}

	if (have_enough_blocks(disk, &buf) == 0) {
		fprintf(stderr, "File too large.\n");
		return ENOSPC;
	}

	FILE *native_file = fopen(native_path, "r");
	if(native_file == NULL){
		fprintf(stderr, "The source file path does not exist.\n");
		return ENOENT;
	}

	// destination path
	char *dest_path = argv[3];

	//check if user does not provide an absolute path.
	if (dest_path[0] != '/') {
		fprintf(stderr, "Not an absolute path.\n");
		return EINVAL;
	}

	struct ext2_dir_entry_2 *dest_dir_entry = NULL;
	int flag = 0;
	if (strlen(dest_path) == 1) {
		flag = 1;
	} else {
		// check if directory exist
		char buf[strlen(dest_path)];
		strcpy(buf, dest_path);
		char *ptr_to_buf = buf;
	  dest_dir_entry = find_dir_entry(disk, ptr_to_buf);
	  if (dest_dir_entry == NULL) {
	  	fprintf(stderr, "The destination file path does not exist.\n");
			return ENOENT;
	  }
	}
	
	struct ext2_inode *dest_dir_entry_inode = NULL;
	if (flag) {
		dest_dir_entry_inode = get_root_inode(disk);
	} else {
		dest_dir_entry_inode = get_inode_table(disk) + TO_INDEX(dest_dir_entry->inode);
	}
	
	char *native_file_name;
	if(flag == 0){
	  if (dest_dir_entry == NULL) {
			//Check if the parent directory exists
			unsigned int parent_inodeNum = find_parent_inode(disk, dest_path);
			if (parent_inodeNum) {
				dest_dir_entry_inode = get_inode_table(disk) + TO_INDEX(parent_inodeNum);
				native_file_name = find_file_name_by_path(dest_path);
			} else {
				fprintf(stderr, "The parent directory does not exist.\n");			
				return ENOENT;
			}
		} else {
	    // check if destination path is a directory
	    if (dest_dir_entry->file_type == EXT2_FT_DIR) {
	    	dest_dir_entry_inode = get_inode_table(disk) + TO_INDEX(dest_dir_entry->inode);
			  native_file_name = find_file_name_by_path(native_path);
			  if (strlen(native_file_name) > (EXT2_BLOCK_SIZE - sizeof(struct ext2_dir_entry_2 *))) {
					return ENAMETOOLONG;
				}
			  char path[PATH_MAX];
			  strcat(path, dest_path);
			  strcat(path, "/");
			  strcat(path, native_file_name);
			  
			  dest_dir_entry = find_dir_entry(disk, path);
			  if (dest_dir_entry) {
			  	flag = -1;	  	
			  }
		  } else {
			  dest_dir_entry_inode = get_inode_table(disk) + TO_INDEX(find_parent_inode(disk, dest_path));
			  native_file_name = find_file_name_by_path(dest_path);
			  flag = -1;
		  }
		}
	} else {
		native_file_name = find_file_name_by_path(native_path);
		if (strlen(native_file_name) > (EXT2_BLOCK_SIZE - sizeof(struct ext2_dir_entry_2 *))) {
			return ENAMETOOLONG;
		}
		char path[PATH_MAX];
	  strcpy(path, dest_path);
	  path[strlen(dest_path)] = '\0';
	  strcat(path, "/");
	  strcat(path, native_file_name);
	  dest_dir_entry = find_dir_entry(disk, path);
	  if (dest_dir_entry) {
	  	flag = -1;	  	
	  }
	}

	// Locate the file inode
	unsigned int new_inodeNum = 0;
	if (flag == -1) {
		new_inodeNum = dest_dir_entry->inode;
		struct ext2_inode *dir_entry_inode = get_inode_table(disk) + TO_INDEX(dest_dir_entry->inode);
		free_all_blocks_of_inode(disk, dir_entry_inode);
	} else {
		new_inodeNum = find_free_inode(disk);
		if(new_inodeNum == 0){
      fprintf(stderr, "No more free inodes\n");
      return ENOSPC;
		}
		update_bitmap(disk, 'i', new_inodeNum, 1);
	}
	
  struct ext2_inode *new_file_entry_inode = get_inode_table(disk) + TO_INDEX(new_inodeNum);
  init_inode(new_file_entry_inode, EXT2_S_IFREG, buf.st_size);

	//copy file data into inode
	char buffer[EXT2_BLOCK_SIZE];
	unsigned int bytes_read = fread(buffer, 1, EXT2_BLOCK_SIZE, native_file);
	int block_index = 0;
	int block_num;
	unsigned char *block_to_write;
	while (block_index < DIRECT_BLOCK_COUNT && bytes_read > 0) {
		//Write to direct blocks
		block_num = find_free_block(disk);
		update_bitmap(disk, 'b', block_num, 1);
		new_file_entry_inode->i_block[block_index] = block_num;
		new_file_entry_inode->i_blocks += 1;
		block_index += 1;
		block_to_write = disk + block_num * EXT2_BLOCK_SIZE;
		memcpy(block_to_write, buffer, bytes_read);
		bytes_read = fread(buffer, 1, EXT2_BLOCK_SIZE, native_file);
	}
	if (bytes_read > 0) {
		//Write to indirect blocks
		block_num = find_free_block(disk);
		update_bitmap(disk, 'b', block_num, 1);
		new_file_entry_inode->i_block[INDIRECT_BLOCK] = block_num;
		new_file_entry_inode->i_blocks += 1;
		unsigned int *ptr_block = (unsigned int *)(disk + block_num * EXT2_BLOCK_SIZE);
		int offset_in_ptr_block = 0;
		while ((offset_in_ptr_block < NUM_OF_PTRS_IN_BLOCK) && (bytes_read > 0)) {
			block_num = find_free_block(disk);
			update_bitmap(disk, 'b', block_num, 1);
			new_file_entry_inode->i_blocks += 1;
			ptr_block[offset_in_ptr_block] = block_num;
			
			copy_data_to_block(disk, block_num, buffer, bytes_read);
			
			offset_in_ptr_block++;
			bytes_read = fread(buffer, 1, EXT2_BLOCK_SIZE, native_file);
		}
		
		if (bytes_read > 0) {
			//Write to double indirect blocks
			block_num = find_free_block(disk);
			update_bitmap(disk, 'b', block_num, 1);
			new_file_entry_inode->i_block[DBLE_INDIRECT_BLOCK] = block_num;
			new_file_entry_inode->i_blocks += 1;
			unsigned int *ptr_ptr_block = (unsigned int *)(disk + block_num * EXT2_BLOCK_SIZE);
			int offset_in_ptr_ptr_block = 0;
			while (offset_in_ptr_ptr_block < NUM_OF_PTRS_IN_BLOCK && bytes_read > 0) {
				block_num = find_free_block(disk);
				update_bitmap(disk, 'b', block_num, 1);
				new_file_entry_inode->i_blocks += 1;
				ptr_ptr_block[offset_in_ptr_ptr_block] = block_num;

				ptr_block = (unsigned int *)(disk + block_num * EXT2_BLOCK_SIZE);
				offset_in_ptr_block = 0;
				while (offset_in_ptr_block < NUM_OF_PTRS_IN_BLOCK && bytes_read > 0) {
					block_num = find_free_block(disk);
					update_bitmap(disk, 'b', block_num, 1);
					new_file_entry_inode->i_blocks += 1;
					ptr_block[offset_in_ptr_block] = block_num;
					copy_data_to_block(disk, block_num, buffer, bytes_read);
					offset_in_ptr_block++;
					bytes_read = fread(buffer, 1, EXT2_BLOCK_SIZE, native_file);
				}

				offset_in_ptr_ptr_block++;
				bytes_read = fread(buffer, 1, EXT2_BLOCK_SIZE, native_file);
			}
			
			if (bytes_read > 0) {
				//Write to triple indirect blocks
				block_num = find_free_block(disk);
				update_bitmap(disk, 'b', block_num, 1);
				new_file_entry_inode->i_block[TPLE_INDIRECT_BLOCK] = block_num;
				new_file_entry_inode->i_blocks += 1;
				unsigned int *ptr_ptr_ptr_block = (unsigned int *)(disk + block_num * EXT2_BLOCK_SIZE);
				int offset_in_ptr_ptr_ptr_block = 0;
				while (offset_in_ptr_ptr_ptr_block < NUM_OF_PTRS_IN_BLOCK && bytes_read > 0) {
					block_num = find_free_block(disk);
					update_bitmap(disk, 'b', block_num, 1);
					new_file_entry_inode->i_blocks += 1;
					ptr_ptr_ptr_block[offset_in_ptr_ptr_ptr_block] = block_num;

					ptr_ptr_block = (unsigned int *)(disk + block_num * EXT2_BLOCK_SIZE);
					offset_in_ptr_ptr_block = 0;
					while (offset_in_ptr_ptr_block < NUM_OF_PTRS_IN_BLOCK && bytes_read > 0) {
						block_num = find_free_block(disk);
						update_bitmap(disk, 'b', block_num, 1);
						new_file_entry_inode->i_blocks += 1;
						ptr_ptr_block[offset_in_ptr_ptr_block] = block_num;

						ptr_block = (unsigned int *)(disk + block_num * EXT2_BLOCK_SIZE);
						offset_in_ptr_block = 0;
						while (offset_in_ptr_block < NUM_OF_PTRS_IN_BLOCK && bytes_read > 0) {
							block_num = find_free_block(disk);
							update_bitmap(disk, 'b', block_num, 1);
							new_file_entry_inode->i_blocks += 1;
							ptr_block[offset_in_ptr_block] = block_num;
							copy_data_to_block(disk, block_num, buffer, bytes_read);
							offset_in_ptr_block++;
							bytes_read = fread(buffer, 1, EXT2_BLOCK_SIZE, native_file);
						}

						offset_in_ptr_ptr_block++;
						bytes_read = fread(buffer, 1, EXT2_BLOCK_SIZE, native_file);
					}

					offset_in_ptr_ptr_ptr_block++;
					bytes_read = fread(buffer, 1, EXT2_BLOCK_SIZE, native_file);
				}
			}
		}
	}
		
	fclose(native_file);
	
	if (flag != -1) {
		int found = 0;
		for (int i = 0; i < TOTAL_BLOCK_COUNT; i++) {
			if (dest_dir_entry_inode->i_block[i]) {
				 found = allocate_space_in_block(disk, strlen(native_file_name), dest_dir_entry_inode->i_block[i], new_inodeNum, native_file_name, EXT2_FT_REG_FILE);
				 if (found) {
					 break;
				 }
			}
		}
		if (!found) {
			int new_blockNum = find_free_block(disk);
			if (new_blockNum == 0) {
				//Release all occupied resourses, need to roll back
				free_all_blocks_of_inode(disk, new_file_entry_inode);
				update_bitmap(disk, 'i', new_inodeNum, 0);
				fprintf(stderr, "No more free blocks\n");
				return ENOSPC;
			}
			update_bitmap(disk, 'b', new_blockNum, 1);
			struct ext2_dir_entry_2 *new_file_entry = (struct ext2_dir_entry_2 *)(disk + EXT2_BLOCK_SIZE * new_blockNum);
			new_file_entry->rec_len = EXT2_BLOCK_SIZE;
			new_file_entry->name_len = strlen(native_file_name);
			new_file_entry->inode = new_inodeNum;
			new_file_entry->file_type = EXT2_FT_REG_FILE;
			for (int j = 0; j < strlen(native_file_name); j++) {
				new_file_entry->name[j] = native_file_name[j];
			}
			for (int i = 0; i < TOTAL_BLOCK_COUNT; i++) {
				if (dest_dir_entry_inode->i_block[i] == 0) {
					 dest_dir_entry_inode->i_block[i] = new_blockNum;
					 dest_dir_entry_inode->i_blocks += 1;
					 break;
				}
			}
		}
	}
	return 0;
}

/** Checks how many blocks will the file need
 *	Returns 1 if enough blocks is left
 *  else returns 0
 */
unsigned int have_enough_blocks(unsigned char *disk, struct stat *file_info) {
	unsigned int num_blocks_needed;
	struct ext2_group_desc *gd = get_group_descriptor(disk);

	if (file_info->st_size < DIRECT_BLOCK_COUNT * EXT2_BLOCK_SIZE) {
		num_blocks_needed = (file_info->st_size / EXT2_BLOCK_SIZE);
		if (file_info->st_size % EXT2_BLOCK_SIZE) {
			num_blocks_needed += 1;
		}
	} else if (file_info->st_size < (DIRECT_BLOCK_COUNT * EXT2_BLOCK_SIZE + NUM_OF_PTRS_IN_BLOCK * EXT2_BLOCK_SIZE)) {
		num_blocks_needed = DBLE_INDIRECT_BLOCK + (file_info->st_size - DIRECT_BLOCK_COUNT * EXT2_BLOCK_SIZE) / EXT2_BLOCK_SIZE;
		if ((file_info->st_size - DIRECT_BLOCK_COUNT * EXT2_BLOCK_SIZE) % EXT2_BLOCK_SIZE) {
			num_blocks_needed += 1;
		} 
	}else if (file_info->st_size < (DIRECT_BLOCK_COUNT * EXT2_BLOCK_SIZE + NUM_OF_PTRS_IN_BLOCK * EXT2_BLOCK_SIZE + pow(NUM_OF_PTRS_IN_BLOCK, 2) * EXT2_BLOCK_SIZE)){
		num_blocks_needed = TPLE_INDIRECT_BLOCK + NUM_OF_PTRS_IN_BLOCK;
		unsigned int end_blocks_needed = (file_info->st_size - DIRECT_BLOCK_COUNT * EXT2_BLOCK_SIZE - NUM_OF_PTRS_IN_BLOCK * EXT2_BLOCK_SIZE) / EXT2_BLOCK_SIZE;
    if ((file_info->st_size - DIRECT_BLOCK_COUNT * EXT2_BLOCK_SIZE - NUM_OF_PTRS_IN_BLOCK * EXT2_BLOCK_SIZE) % EXT2_BLOCK_SIZE) {
      end_blocks_needed += 1;
		}
		unsigned int ptr_blocks_needed = end_blocks_needed / NUM_OF_PTRS_IN_BLOCK;
		if (end_blocks_needed % NUM_OF_PTRS_IN_BLOCK) {
			ptr_blocks_needed += 1;
		}
		num_blocks_needed += ptr_blocks_needed + end_blocks_needed;
	} else if (file_info->st_size < (DIRECT_BLOCK_COUNT * EXT2_BLOCK_SIZE + NUM_OF_PTRS_IN_BLOCK * EXT2_BLOCK_SIZE + pow(NUM_OF_PTRS_IN_BLOCK, 2) * EXT2_BLOCK_SIZE + pow(NUM_OF_PTRS_IN_BLOCK, 3) * EXT2_BLOCK_SIZE)) {
		num_blocks_needed = TOTAL_BLOCK_COUNT + (2 * NUM_OF_PTRS_IN_BLOCK) + pow(NUM_OF_PTRS_IN_BLOCK, 2);
		unsigned int end_blocks_needed = (file_info->st_size - DIRECT_BLOCK_COUNT * EXT2_BLOCK_SIZE - NUM_OF_PTRS_IN_BLOCK * EXT2_BLOCK_SIZE / EXT2_BLOCK_SIZE - pow(NUM_OF_PTRS_IN_BLOCK, 2) * EXT2_BLOCK_SIZE) / EXT2_BLOCK_SIZE;
		if ((file_info->st_size - DIRECT_BLOCK_COUNT * EXT2_BLOCK_SIZE - NUM_OF_PTRS_IN_BLOCK * EXT2_BLOCK_SIZE - (int)(pow(NUM_OF_PTRS_IN_BLOCK, 2) * EXT2_BLOCK_SIZE)) % EXT2_BLOCK_SIZE) {
			end_blocks_needed += 1;
		}
		unsigned int ptr_blocks_needed = end_blocks_needed / NUM_OF_PTRS_IN_BLOCK;
		if (end_blocks_needed % NUM_OF_PTRS_IN_BLOCK) {
			ptr_blocks_needed += 1;
		}
		unsigned int ptr_ptr_blocks_needed = ptr_blocks_needed / NUM_OF_PTRS_IN_BLOCK;
		if (ptr_ptr_blocks_needed % NUM_OF_PTRS_IN_BLOCK) {
			ptr_ptr_blocks_needed += 1;
		}
		num_blocks_needed += ptr_blocks_needed + end_blocks_needed + ptr_ptr_blocks_needed;
	} else {
		return 0;
	}
	if (gd->bg_free_blocks_count >= num_blocks_needed) {
		return 1;
	} else {
		return 0;
	}
}

void copy_data_to_block(unsigned char *disk, unsigned int block_num, char *buffer, unsigned int bytes_read) {
	unsigned char *block_to_write = disk + block_num * EXT2_BLOCK_SIZE;
	memcpy(block_to_write, buffer, bytes_read);
}
