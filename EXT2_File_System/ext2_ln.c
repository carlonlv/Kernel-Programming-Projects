#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"
#include "ext2general.h"


int main(int argc, char **argv) {

	char *file_path;
	char *dest_path;

	// Checking if the number of argument error.
	if (argc != 4 && argc != 5){
			fprintf(stderr, "Usage: <virtual disk name> '-s' <absolute path> <absolute path>\n");
			return EINVAL;
	}

	// Load the arguments of paths into this funciton.
	if (argc == 4){
		file_path = argv[2];
		dest_path = argv[3];
	}else if (argc == 5){
		// Check if the flag input is correct.
		if (strlen(argv[2]) != 2 || argv[2][0] != '-' || argv[2][1] != 's'){
			fprintf(stderr, "Usage: <virtual disk name> '-s' <absolute path> <absolute path>\n");
			return EINVAL;
		} else {
			file_path = argv[3];
			dest_path = argv[4];
		}
	}

	// Checking if both paths provided by user are absolute paths.
	if (file_path[0] != '/' || dest_path[0] != '/') {
			fprintf(stderr, "Usage: <virtual disk name> '-s' <absolute path> <absolute path>\n");
			return EINVAL;
	}

	// Find the path of the destination directory.
	if (strlen(dest_path) == 1) {
		fprintf(stderr, "The destination path refers to root directory.\n");
		return EISDIR;
	}

	unsigned char *disk = read_image(argv[1]);
	// Check if the Disk Image provided is valid.
	if (disk == NULL) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	// Find directory_entry for the source file.
	char buf[strlen(file_path)];
	strcpy(buf, file_path);
	char *ptr_to_buf = buf;
	struct ext2_dir_entry_2 *file = find_dir_entry(disk, ptr_to_buf);
	// Checking if source file exists
	if (file == NULL){
		fprintf(stderr, "No such source file.\n");
		return ENOENT;
	}

	char dir_path[strlen(dest_path)];
	strncpy(dir_path, dest_path, last_slash_in_path(dest_path) + 1);
	dir_path[last_slash_in_path(dest_path) + 1] = '\0';

	// An indicator to whether the destination directory is a root directory.
	int is_root = 0;

	// Check if the destination directory is a root directory.
	struct ext2_dir_entry_2 *dir = NULL;
	if (strlen(dir_path) == 1) {
		is_root = 1;
	} else {
		strcpy(buf, dir_path);
		ptr_to_buf = buf;
		dir = find_dir_entry(disk, ptr_to_buf);
	}

	// Checking if both destination directory and source file exist.
	if (is_root != 1){
		if (dir == NULL){
			fprintf(stderr, "No such destionation directory.\n");
			return ENOENT;
		}
	}

	// Checking if both arguments refer to correct file formats.
	if (file->file_type == EXT2_FT_DIR) {
		fprintf(stderr, "%s refers to a directory, it has to be a file.\n", file_path);
		return EISDIR;
	}
	if (is_root != 1){
		if (dir->file_type != EXT2_FT_DIR){
			fprintf(stderr, "%s does not refer to a directory, it has to be a directory.\n", dir_path);
			return EISDIR;
		}
	}

	// Checking if the Link with the same name already exists in the destination directory.
	strcpy(buf, dest_path);
	ptr_to_buf = buf;
	if (find_dir_entry(disk, ptr_to_buf) != NULL){
		fprintf(stderr, "The destination path already taken.\n");
		return EEXIST;
	}
	// An indicator for space allocation in blocks.
	int found = 0;
	// The name of the link.
	char *filename = find_file_name_by_path(dest_path);
	if (strlen(filename) > (EXT2_BLOCK_SIZE - sizeof(struct ext2_dir_entry_2 *))) {
		return ENAMETOOLONG;
	}
	// The INODE of the source file.
	struct ext2_inode *file_inode = get_inode_table(disk) + TO_INDEX(file->inode);
	if (argc == 4){
		//The case where the souce file inode is a symbolic link
		if (file_inode->i_mode == EXT2_S_IFLNK) {
			//Recursively read its content until the original file inode is reached
			file_inode = read_symbolic_link(disk, file_path);
			if (file_inode == NULL) {
				fprintf(stderr, "The file that one of the symbolic link files has been removed.\n");
				return ENOENT;
			}
		}
		// Obtain INODE of destination directory.
		struct ext2_inode *dir_inode = NULL;
		if (is_root != 1){
			dir_inode = get_inode_table(disk) + TO_INDEX(dir->inode);
		} else {
			dir_inode = get_root_inode(disk);
		}
		// Increment link count for source file, since creating a hard link.
		file_inode->i_links_count++;
		// Allocate the hardlink.
		for(int i = 0; i < TOTAL_BLOCK_COUNT; i++){
        if (dir_inode->i_block[i]) {
            found = allocate_space_in_block(disk, strlen(filename), dir_inode->i_block[i], file->inode, filename, EXT2_FT_REG_FILE);
            if(found){
                break;
            }
        }
    }
		// If there is no space found in the provided blocks.
		if(!found){
				// Find a new block.
        unsigned int new_block_Num = find_free_block(disk);
				update_bitmap(disk, 'b', new_block_Num, 1);
				if (new_block_Num == 0) {
					//Release all occupied resourses, need to roll back
					file_inode->i_links_count--;
					fprintf(stderr, "No space in blocks\n");
				} else {
					int free_space = 0;
		      int i;
		      for(i = 0; i < TOTAL_BLOCK_COUNT; i++) {
		          if (dir_inode->i_block[i] == 0){
								// Found available block.
		              free_space = 1;
		              break;
		          }
		      }

		      if (free_space){
							// Set the fields to be a hardlink.
		          dir_inode->i_block[i] = new_block_Num;
		          dir_inode->i_blocks += 1;
		          struct ext2_dir_entry_2 *link_entry = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * new_block_Num);
		          link_entry->inode = file->inode;
		          link_entry->rec_len = EXT2_BLOCK_SIZE;
		          for(int j=0;j<strlen(filename);j++){
		             link_entry->name[j] = filename[j];
		          }
		          link_entry->file_type = file->file_type;
		          found = 1;
		      }
				}
    }

	} else {
	  struct ext2_inode *dir_inode = NULL;
		if (is_root != 1){
			dir_inode = get_inode_table(disk) + TO_INDEX(dir->inode) * sizeof(struct ext2_inode);
		} else {
			dir_inode = get_root_inode(disk);
		}
		// Allocate a new inode for the link and update the bitmap.
		unsigned int link_inode_num = find_free_inode(disk);
		if(link_inode_num == 0){
        fprintf(stderr, "No more space for inode\n");
        return ENOSPC;
    }
		update_bitmap(disk, 'i', link_inode_num, 1);

		// Initialize the Inode for the link.
		struct ext2_inode *link_inode = get_inode_table(disk) + TO_INDEX(link_inode_num);
    init_inode(link_inode, EXT2_S_IFLNK, strlen(file_path));

		// Allocate a new block for the link and update bitmap.
		unsigned int link_block_num = find_free_block(disk);
	  if(link_block_num == 0){
	  	update_bitmap(disk, 'i', link_inode_num, 0);
	    fprintf(stderr, "No more space for block\n");
			return ENOSPC;
		}
		update_bitmap(disk, 'b', link_block_num, 1);

		// Store Absolute path of the file into block pointed to by Symlink's inode.
		if (strlen(file_path) > EXT2_BLOCK_SIZE){
			fprintf(stderr, "Path of link is too long.\n");
			return ENOSPC;
		}
		unsigned char *block_to_write = disk + link_block_num * EXT2_BLOCK_SIZE;
		memcpy(block_to_write, file_path, strlen(file_path));

		//Add the block to the new inode
		link_inode->i_block[0] = link_block_num;
		link_inode->i_blocks++;

		// Put the symlink into the parent directory.
		for(int i = 0; i < TOTAL_BLOCK_COUNT; i++){
        if (dir_inode->i_block[i]) {
            found = allocate_space_in_block(disk, strlen(filename), dir_inode->i_block[i], link_inode_num, filename, EXT2_FT_SYMLINK);
	          if(found){
	              break;
	          }
        }
    }
		if(!found){
        unsigned int new_block_Num = find_free_block(disk);
				if (new_block_Num == 0) {
					//Release all occupied resourses, need to roll back
					update_bitmap(disk, 'b', link_block_num, 0);
					update_bitmap(disk, 'i', link_inode_num, 0);
					fprintf(stderr, "No space in parent directory.\n");
				} else {
					update_bitmap(disk, 'b', new_block_Num, 1);
		      int free_space = 0;
		      int i;
		      for(i = 0; i < TOTAL_BLOCK_COUNT; i++) {
		          if (dir_inode->i_block[i] == 0){
		              free_space = 1;
		              break;
		          }
		      }

		      if (free_space){
		          dir_inode->i_block[i] = new_block_Num;
		          dir_inode->i_blocks += 1;
		          struct ext2_dir_entry_2 *link_entry = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * new_block_Num);
		          link_entry->name_len = strlen(filename);
		          link_entry->inode = link_inode_num;
		          link_entry->rec_len = EXT2_BLOCK_SIZE;
		          for(int j = 0; j < strlen(filename); j++){
		             link_entry->name[j] = filename[j];
		          }
		          link_entry->file_type = file->file_type;
		          found = 1;
		      }
				}
    }
  }
	if (found) {
		return 0;
	} else {
		return ENOSPC;
	}
}
