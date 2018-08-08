#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include "ext2.h"
#include "ext2general.h"

/** Locate the dir entry with the given name inside the parent directory's inode
 *  Returns -1 if dir entry with such name is not found.
 */
int locate_dir_entry_in_block(unsigned char *disk, struct ext2_inode *parent_inode, struct ext2_dir_entry_2 *file_entry) {
  for (int i = 0; i < TOTAL_BLOCK_COUNT; i++) {
    if (parent_inode->i_block[i]) {
      struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * parent_inode->i_block[i]);
      int current_position = 0;
      while (current_position < EXT2_BLOCK_SIZE) {
        if ((dir->name_len == file_entry->name_len)) {
        	int same_name = 1;
        	for (int i = 0; i < dir->name_len; i++) {
        		if (dir->name[i] == file_entry->name[i]) {
        			same_name = 0;
        		}
        	}
        	if (same_name) {
        		break;
        	}
        }
        current_position += dir->rec_len;
        dir = (void*) dir + dir->rec_len;
      }
      return i;
    }
  }
  return -1;
}

/** Check if the block in the parent inode contains only the dir entry that is about to be deleted.
 *  Unallocate the block if it does.
 */
void update_parent_inode_block(unsigned char *disk, struct ext2_inode *parent_inode, int index) {
  struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * parent_inode->i_block[index]);
  int count = 0;
  int current_position = 0;
  while (current_position < EXT2_BLOCK_SIZE) {
    if (dir->inode != 0) {
      count++;
    }
    current_position += dir->rec_len;
    dir = (void*) dir + dir->rec_len;
  }
  if (count == 1) {
    //There is only one directory_entry in this block and it is about to be deleted
    update_bitmap(disk, 'b', parent_inode->i_block[index], 0); //Free the empty block
    parent_inode->i_block[index] = 0; //Set it to unused
    parent_inode->i_blocks -= 1;
  }
}

/** Unallocates all the blocks and the inode for the file that is about to be deleted.
 */
void remove_file(unsigned char *disk, struct ext2_dir_entry_2 *dir_entry, char *path) {
  struct ext2_inode *dir_entry_inode = get_inode_table(disk) + TO_INDEX(dir_entry->inode);
  dir_entry_inode->i_links_count -= 1;
  if (dir_entry_inode->i_links_count == 0) {
    free_all_blocks_of_inode(disk, dir_entry_inode);
    //Free self inode
    update_bitmap(disk, 'i', dir_entry->inode, 0);
    //Set delete time
    dir_entry_inode->i_dtime = time(NULL);
    //Mark size as 0
    dir_entry_inode->i_size = 0;
  }
	//Fix parent directory inode's block
  struct ext2_inode *parent_inode = get_inode_table(disk) + TO_INDEX(find_parent_inode(disk, path));
  int index = locate_dir_entry_in_block(disk, parent_inode, dir_entry);
  update_parent_inode_block(disk, parent_inode, index);
  //Invalidate struct inode number
	dir_entry->inode = 0;
}

/** Precondition: Only . and .. are the only directory entries left.
 *
 */
void remove_empty_directory(unsigned char *disk, struct ext2_dir_entry_2 *empty_directory, char *path) {
  //Fix parent directory inode's blocks
  struct ext2_inode *parent_inode = get_inode_table(disk) + TO_INDEX(find_parent_inode(disk, path));
  int index = locate_dir_entry_in_block(disk, parent_inode, empty_directory);
  update_parent_inode_block(disk, parent_inode, index);
  parent_inode->i_links_count -= 1;
  //Free self inode
  update_bitmap(disk, 'i', empty_directory->inode, 0);
  
  struct ext2_inode *empty_directory_inode = get_inode_table(disk) + TO_INDEX(empty_directory->inode);
  //Set delete time
  empty_directory_inode->i_dtime = time(NULL);
  //Mark size as 0
  empty_directory_inode->i_size = 0;
  //Free all the direct blocks
  for (int i = 0; i < TOTAL_BLOCK_COUNT; i++) {
    update_bitmap(disk, 'b', empty_directory_inode->i_block[i], 0);
  }
  //Invalidate struct inode number
  empty_directory->inode = 0;
}

void remove_directory_or_file_or_link(unsigned char *disk, struct ext2_dir_entry_2 *dir_entry, char *path) {
	if (dir_entry->file_type == EXT2_FT_REG_FILE || dir_entry->file_type == EXT2_FT_SYMLINK) {
	  remove_file(disk, dir_entry, path);
	} else {
		struct ext2_inode *inode = get_inode_table(disk) + TO_INDEX(dir_entry->inode);
		inode->i_links_count -= 2;
		for (int i = 0; i < TOTAL_BLOCK_COUNT; i++) {
		  if (inode->i_block[i]) {
		    struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * inode->i_block[i]);
		    int current_position = 0;
		    while (current_position < EXT2_BLOCK_SIZE) {
		      if (dir->inode != 0 && (check_if_external_entry(dir) == 1)) {
				    char *print_name = malloc(sizeof(char) * (dir->name_len + 1));
				    for (int u = 0; u < dir->name_len; u++) {
				        print_name[u] = dir->name[u];
				    }
				    print_name[dir->name_len] = '\0';
				    char entry_path[PATH_MAX];
				    strcpy(entry_path, path);
				    entry_path[strlen(path)] = '\0';
				    strcat(entry_path, "/");
				    strcat(entry_path, dir->name);
				    free(print_name);
				    remove_directory_or_file_or_link(disk, dir, entry_path);
		      }

		      current_position += dir->rec_len;
		      dir = (void*) dir + dir->rec_len;
		    }
		  }
		}
		remove_empty_directory(disk, dir_entry, path);
	}
}

int main(int argc, char **argv) {

  //Check for correct arguments and flag
  if (argc != 3 && argc != 4) {
      fprintf(stderr, "Usage: %s <virtual disk name> '-r' <absolute path>\n", argv[0]);
      return EINVAL;
  }

  int flag = 0;
  char *path;
  if (argc == 4) {
    if (strlen(argv[2]) != 2 || argv[2][0] != '-' || argv[2][1] != 'r') {
      fprintf(stderr, "Usage: %s <virtual disk name> '-r' <absolute path>\n", argv[0]);
      return EINVAL;
    } else {
      flag = 1;
      path = argv[3];
    }
  } else {
    path = argv[2];
  }

  //The user does not provide an absolute path.
  if (path[0] != '/') {
    fprintf(stderr, "Not an absolute path.\n");
    return EINVAL;
  }

	if (strlen(path) == 1) {
		fprintf(stderr, "Can not delete root directory.\n");
		return EINVAL;
	}

  unsigned char *disk = read_image(argv[1]);
  if (disk == NULL) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }
	
	char buf[strlen(path)];
	strcpy(buf, path);
	char *ptr_to_buf = buf;
  struct ext2_dir_entry_2 *dest_file = find_dir_entry(disk, ptr_to_buf);
  if (dest_file == NULL) {

    fprintf(stderr, "No such file or directory.\n");
    return ENOENT;
  }

  if (dest_file->file_type == EXT2_FT_DIR) {

    if (flag) {
      //Recursively remove all files inside
      remove_directory_or_file_or_link(disk, dest_file, path);
    } else {

      fprintf(stderr, "Can not remove directory.\n");
      return EISDIR;
    }
  } else {

    remove_directory_or_file_or_link(disk, dest_file, path);
  }
	return 0;
}
