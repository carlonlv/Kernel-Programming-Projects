#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"
#include "ext2general.h"

int main(int argc, char **argv) {

  //Check for correct arguments and flag
  if (argc != 3 && argc != 4) {
			fprintf(stderr, "Usage: %s <virtual disk name> '-a' <absolute path>\n", argv[0]);
      return EINVAL;
  }

  int flag = 0;
  char *path;
  if (argc == 4) {
    if (strlen(argv[2]) != 2 || argv[2][0] != '-' || argv[2][1] != 'a') {
    	fprintf(stderr, "Usage: %s <virtual disk name> '-a' <absolute path>\n", argv[0]);
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

  unsigned char *disk = read_image(argv[1]);
  if (disk == NULL) {
    perror("mmap");
    return EINVAL;
  }

	int is_root = 0;
	struct ext2_dir_entry_2 *dest_file = NULL;
	if (strlen(path) == 1) {

		is_root = 1;
	} else {
		
		char buf[strlen(path)];
		strcpy(buf, path);
		char *ptr_to_buf = buf;
		dest_file = find_dir_entry(disk, ptr_to_buf);

		if (dest_file == NULL) {

			fprintf(stderr, "No such file or directory\n");
			return ENOENT;
		}
	}

  if ((dest_file == NULL && is_root == 1) || (dest_file->file_type == EXT2_FT_DIR)) {

  	struct ext2_inode *dir_inode;
		if (is_root) {
			dir_inode = get_root_inode(disk);
		} else {
			dir_inode = get_inode_table(disk) + TO_INDEX(dest_file->inode);
		}

    if (flag) {

		  for (int i = 0; i < TOTAL_BLOCK_COUNT; i++) {
		    if (dir_inode->i_block[i]) {
		        struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * dir_inode->i_block[i]);
		        int current_position = 0;
		        while (current_position < EXT2_BLOCK_SIZE) {
		          if (dir->inode) {
		            char *print_name = malloc(sizeof(char) * (dir->name_len + 1));
		            for (int u = 0; u < dir->name_len; u++) {
		                print_name[u] = dir->name[u];
		            }
		            print_name[dir->name_len] = '\0';
		            printf("%s\n", print_name);
		            free(print_name);
		          }
		          current_position += dir->rec_len;
		          dir = (void*) dir + dir->rec_len;
		        }
		    }
		  }
		} else {

		  for (int i = 0; i < TOTAL_BLOCK_COUNT; i++) {
		    if (dir_inode->i_block[i]) {
		        struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * dir_inode->i_block[i]);
		        int current_position = 0;
		        while (current_position < EXT2_BLOCK_SIZE) {
		          if (dir->inode != 0 && (check_if_external_entry(dir) == 1)) {
		            char *print_name = malloc(sizeof(char) * (dir->name_len + 1));
		            for (int u = 0; u < dir->name_len; u++) {
		                print_name[u] = dir->name[u];
		            }
		            print_name[dir->name_len] = '\0';
		            printf("%s\n", print_name);
		            free(print_name);
		          }
		          current_position += dir->rec_len;
		          dir = (void*) dir + dir->rec_len;
		        }
		    }
		  }
		}
  } else {

    char *print_name = malloc(sizeof(char) * (dest_file->name_len + 1));
    for (int u = 0; u < dest_file->name_len; u++) {
        print_name[u] = dest_file->name[u];
    }
    print_name[dest_file->name_len] = '\0';
    printf("%s\n", print_name);
    free(print_name);
  }
  return 0;
}
