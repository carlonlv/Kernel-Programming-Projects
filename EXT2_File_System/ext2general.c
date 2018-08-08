#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include "ext2.h"
#include "ext2general.h"

unsigned char *read_image(char *disk_name) {
  int fd = open(disk_name, O_RDWR);
  unsigned char *disk = mmap(NULL, EXT2_NUM_BLOCKS * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(disk == MAP_FAILED) {
     return NULL;
  } else {
    return disk;
  }
}

struct ext2_super_block *get_super_block(unsigned char *disk) {
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + SUPERBLOCK_POS * EXT2_BLOCK_SIZE);
  return sb;
}

struct ext2_group_desc *get_group_descriptor(unsigned char *disk) {
  struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + GROUPDESCRIPTOR_POS * EXT2_BLOCK_SIZE);
  return gd;
}

struct ext2_inode *get_inode_table(unsigned char *disk) {
  struct ext2_group_desc *gd = get_group_descriptor(disk);
  struct ext2_inode *inode_tbl = (struct ext2_inode *) (disk + gd->bg_inode_table * EXT2_BLOCK_SIZE);
  return inode_tbl;
}

struct ext2_inode *get_root_inode(unsigned char *disk) {
  struct ext2_inode *root_inode = get_inode_table(disk) + TO_INDEX(ROOT_INODE_NUMBER);
  return root_inode;
}

struct ext2_dir_entry_2 *locate_file_by_name(unsigned char *disk, struct ext2_inode *inode, char *file_name) {
  for (int i = 0; i < TOTAL_BLOCK_COUNT; i++) {
    if (inode->i_block[i]) {
      int block_num = inode->i_block[i];
      struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_num);
      int current_position = 0;
      while (current_position < EXT2_BLOCK_SIZE) {
        if (dir->name_len == strlen(file_name) && (dir->inode != 0)) {
          char *print_name = malloc(sizeof(char) * (dir->name_len + 1));
          for (int u = 0; u < dir->name_len; u++) {
              print_name[u] = dir->name[u];
          }
          print_name[dir->name_len] = '\0';
          if ((strncmp(print_name, file_name, dir->name_len)) == 0 && (dir->inode != 0)) {
            free(print_name);
            return dir;
          }
          free(print_name);
        }
        current_position += dir->rec_len;
        dir = (void*) dir + dir->rec_len;
      }
    }
  }
  return NULL;
}

struct ext2_dir_entry_2 *find_dir_entry(unsigned char *disk, char *path) {
  path = path + sizeof(char);
  struct ext2_inode *root_inode = get_root_inode(disk);
  char *token = strtok_r(path, "/", &path);
  int found = 1;
  struct ext2_dir_entry_2 *current_file = locate_file_by_name(disk, root_inode, token);
  if (current_file == NULL) {
  	found = 0;
  } else {
  	struct ext2_inode *current_inode = get_inode_table(disk) + TO_INDEX(current_file->inode);
		while ((token = strtok_r(path, "/", &path)) && (found == 1)) {
		  current_file = locate_file_by_name(disk, current_inode, token);
		  if (current_file == NULL) {
		    found = 0;
		    break;
		  }
		  current_inode = get_inode_table(disk) + TO_INDEX(current_file->inode);
		}
  }
  if (found == 0) {
    return NULL;
  } else {
    return current_file;
  }
}

void update_bitmap(unsigned char *disk, char bitmap_type, unsigned int num, unsigned int value) {
	if (num != 0) {
		int bitmap_byte = TO_INDEX(num) / 8;
		int bitmap_bit = TO_INDEX(num) % 8;
		unsigned char *bitmap;
		if (bitmap_type == 'b') {
		  bitmap = disk + get_group_descriptor(disk)->bg_block_bitmap * EXT2_BLOCK_SIZE;
		} else {
		  bitmap = disk + get_group_descriptor(disk)->bg_inode_bitmap * EXT2_BLOCK_SIZE;
		}
		if (value) {
		  if (!((bitmap[bitmap_byte] >> bitmap_bit) & 1)) {
		    if (bitmap_type == 'b') {
		      get_super_block(disk)->s_free_blocks_count -= 1;
		      get_group_descriptor(disk)->bg_free_blocks_count -= 1;
		    } else {
		      get_super_block(disk)->s_free_inodes_count -= 1;
		      get_group_descriptor(disk)->bg_free_inodes_count -= 1;
		    }
		  }
		  bitmap[bitmap_byte] = bitmap[bitmap_byte] | (1 << bitmap_bit);
		} else {
		  if ((bitmap[bitmap_byte] >> bitmap_bit) & 1) {
		    if (bitmap_type == 'b') {
		      get_super_block(disk)->s_free_blocks_count += 1;
		      get_group_descriptor(disk)->bg_free_blocks_count += 1;
		    } else {
		      get_super_block(disk)->s_free_inodes_count += 1;
		      get_group_descriptor(disk)->bg_free_inodes_count += 1;
		    }
		  }
		  bitmap[bitmap_byte] = bitmap[bitmap_byte] & ~(1 << bitmap_bit);
		}
	}
}

unsigned int last_slash_in_path(char *path) {
  int i;
  if (path[strlen(path) - 1] == '/') {
    for (i = strlen(path) - 2; i >= 0; i--) {
      if (path[i] == '/') {
        break;
      }
    }
  } else {
    for (i = strlen(path) - 1; i >= 0; i--) {
      if (path[i] == '/') {
        break;
      }
    }
  }
  return i;
}

unsigned int find_parent_inode(unsigned char *disk, char *path) {
  char buffer[strlen(path)];
  strcpy(buffer, path);
  buffer[last_slash_in_path(path)] = '\0';
  char *parent_path = buffer;
	if (strlen(parent_path) == 0) {
		return ROOT_INODE_NUMBER;
	}
  struct ext2_dir_entry_2 *parent_entry = find_dir_entry(disk, parent_path);
  if (parent_entry == NULL) {
    return 0;
  } else {
    return parent_entry->inode;
  }
}


unsigned int check_if_external_entry(struct ext2_dir_entry_2 *dir) {
  char *print_name = malloc(sizeof(char) * (dir->name_len + 1));
  for (int u = 0; u < dir->name_len; u++) {
      print_name[u] = dir->name[u];
  }
  print_name[dir->name_len] = '\0';
  if (strncmp(print_name, ".", dir->name_len + 1) == 0) {
    free(print_name);
    return 0;
  } else if (strncmp(print_name, "..", dir->name_len + 1) == 0) {
    free(print_name);
    return 0;
  } else {
    free(print_name);
    return 1;
  }
}

int allocate_space_in_block(unsigned char *disk, unsigned char name_len, unsigned int block_num, unsigned int inode_num, char* file_name, unsigned char file_type) {
  struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_num);
  struct ext2_dir_entry_2 *prev = dir;
  unsigned short current_position = dir->rec_len;
  dir = (void*) dir + dir->rec_len;
  unsigned short new_rec_len_per_four_bytes = (sizeof(struct ext2_dir_entry_2 *) + sizeof(char) * prev->name_len) / 4;
  if ((sizeof(struct ext2_dir_entry_2 *) + sizeof(char) * prev->name_len) % 4 != 0) {
    new_rec_len_per_four_bytes += 1;
  }
  if (prev->inode == 0 && (prev->rec_len >= new_rec_len_per_four_bytes * 4)) {
  	prev->name_len = name_len;
  	prev->inode = inode_num;
  	for (int i = 0; i < name_len; i++) {
  		prev->name[i] = file_name[i];
  	}
  	prev->file_type = file_type;
  	return 1;
  }
  while (current_position < EXT2_BLOCK_SIZE) {
		new_rec_len_per_four_bytes = (sizeof(struct ext2_dir_entry_2 *) + sizeof(char) * dir->name_len) / 4;
		if ((sizeof(struct ext2_dir_entry_2 *) + sizeof(char) * dir->name_len) % 4 != 0) {
		  new_rec_len_per_four_bytes += 1;
		}
    if (dir->inode == 0 && (dir->rec_len >= new_rec_len_per_four_bytes * 4)) {
      dir->name_len = name_len;
      dir->inode = inode_num;
      for (int i = 0; i < name_len; i++) {
				dir->name[i] = file_name[i];
			}
			dir->file_type = file_type;
      return 1;
    }
    current_position += dir->rec_len;
    dir = (void*) dir + dir->rec_len;
    prev = (void*) prev + prev->rec_len;
  }
  unsigned short rec_len_needed = (2 * sizeof(struct ext2_dir_entry_2 *) + sizeof(char) * name_len + sizeof(char) * prev->name_len) / 4;
  if ((2 * sizeof(struct ext2_dir_entry_2 *) + sizeof(char) * name_len + sizeof(char) * prev->name_len) % 4) {
  	rec_len_needed += 1;
  }
  if (prev->rec_len >= rec_len_needed * 4) {
  	new_rec_len_per_four_bytes = (sizeof(struct ext2_dir_entry_2 *) + sizeof(char) * prev->name_len) / 4;
		if ((sizeof(struct ext2_dir_entry_2 *) + sizeof(char) * prev->name_len) % 4 != 0) {
		  new_rec_len_per_four_bytes += 1;
		}
    unsigned short remainder = prev->rec_len - new_rec_len_per_four_bytes * 4;
    prev->rec_len = new_rec_len_per_four_bytes * 4;
    dir = (void*)prev + prev->rec_len;
    dir->rec_len = remainder;
    dir->name_len = name_len;
    dir->inode = inode_num;
    for (int i = 0; i < name_len; i++) {
  		dir->name[i] = file_name[i];
  	}
  	dir->file_type = file_type;
	  return 1;
  }
  return 0;
}

unsigned int find_free_inode(unsigned char *disk) {
	struct ext2_super_block *sb = get_super_block(disk);
	struct ext2_group_desc *gd = get_group_descriptor(disk);
	unsigned char *inode_bit_map = disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap;
	for (int i = 0; i < sb->s_inodes_count; i++) {
		if (i >= EXT2_GOOD_OLD_FIRST_INO) {
			int bit_map_byte = i / 8;
      int bit_order = i % 8;
      if (!((inode_bit_map[bit_map_byte] >> bit_order) & 1)) {
      	return TO_NUMBER(i);
      }
		}
	}
	return 0;
}

unsigned int find_free_block(unsigned char *disk) {
	struct ext2_super_block *sb = get_super_block(disk);
	struct ext2_group_desc *gd = get_group_descriptor(disk);
	unsigned char *inode_bit_map = disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap;
	for (int i = 0; i < sb->s_blocks_count; i++) {
		int bit_map_byte = i / 8;
    int bit_order = i % 8;
    if (!((inode_bit_map[bit_map_byte] >> bit_order) & 1)) {
    	return TO_NUMBER(i);
    }
	}
	return 0;
}

void init_inode(struct ext2_inode *inode,unsigned short mode,unsigned int size){
    time_t current_time = time(NULL);
    inode->i_mode = mode;
    inode->i_uid = 0;
    inode->i_size = size;
    inode->i_ctime = current_time;
    inode->i_dtime = 0;
    inode->i_gid = 0;
    inode->i_links_count = 1;
    inode-> i_blocks = 0;
    inode->osd1 = 0;
    inode->i_generation = 0;
    inode->i_file_acl = 0;
    inode->i_dir_acl = 0;
    inode->i_faddr = 0;
    for(int i = 0; i < TOTAL_BLOCK_COUNT; i++){
          inode->i_block[i] = 0;
    }
}

char *find_file_name_by_path(char *path){
  unsigned int index = last_slash_in_path(path) + 1;
	path = path + index;
	return path;
}

void free_all_blocks_of_inode(unsigned char *disk, struct ext2_inode *dir_entry_inode) {
	//Free all the direct blocks
  for (int i = 0; i < DIRECT_BLOCK_COUNT; i++) {
    update_bitmap(disk, 'b', dir_entry_inode->i_block[i], 0);
  }
  //Free indirect blocks
  if (dir_entry_inode->i_block[INDIRECT_BLOCK]) {
    unsigned int *current_pos = (unsigned int *) (disk + EXT2_BLOCK_SIZE * dir_entry_inode->i_block[INDIRECT_BLOCK]);
    for (int i = 0; i < NUM_OF_PTRS_IN_BLOCK; i++) {
      update_bitmap(disk, 'b', current_pos[i], 0);
    }
    update_bitmap(disk, 'b', dir_entry_inode->i_block[INDIRECT_BLOCK], 0);
  }
  //Free double indirect Blocks
  if (dir_entry_inode->i_block[DBLE_INDIRECT_BLOCK]) {
    unsigned int *current_ptr_pos = (unsigned int *) (disk + EXT2_BLOCK_SIZE * dir_entry_inode->i_block[DBLE_INDIRECT_BLOCK]);
    for (int i = 0; i < NUM_OF_PTRS_IN_BLOCK; i++) {
      unsigned int *current_pos = (unsigned int *) (disk + EXT2_BLOCK_SIZE * (current_ptr_pos[i]));
      for (int j = 0; j < NUM_OF_PTRS_IN_BLOCK; j++) {
        update_bitmap(disk, 'b', current_pos[j], 0);
      }
      update_bitmap(disk, 'b', current_ptr_pos[i], 0);
    }
    update_bitmap(disk, 'b', dir_entry_inode->i_block[DBLE_INDIRECT_BLOCK], 0);
  }
  //Free triple indirect Blocks
  if (dir_entry_inode->i_block[TPLE_INDIRECT_BLOCK]) {
    int max_ptr_ptr_count = EXT2_BLOCK_SIZE / sizeof(unsigned int);
    unsigned int *current_ptr_ptr_pos = (unsigned int *) (disk + EXT2_BLOCK_SIZE * dir_entry_inode->i_block[TPLE_INDIRECT_BLOCK]);
    for (int i = 0; i < max_ptr_ptr_count; i++) {
      unsigned int *current_ptr_pos = (unsigned int *) (disk + EXT2_BLOCK_SIZE * (current_ptr_ptr_pos[i]));
      for (int j = 0; j < NUM_OF_PTRS_IN_BLOCK; j++) {
        int max_blocks_num = EXT2_BLOCK_SIZE / sizeof(unsigned int);
        unsigned int *current_pos = (unsigned int *) (disk + EXT2_BLOCK_SIZE * (current_ptr_pos[i]));
        for (int k = 0; k < max_blocks_num; k++) {
          update_bitmap(disk, 'b', current_pos[k], 0);
        }
        update_bitmap(disk, 'b', current_ptr_pos[j], 0);
      }
      update_bitmap(disk, 'b', current_ptr_ptr_pos[i], 0);
    }
    update_bitmap(disk, 'b', dir_entry_inode->i_block[TPLE_INDIRECT_BLOCK], 0);
  }
}

struct ext2_inode *read_symbolic_link(unsigned char *disk, char *path) {
	char buf[strlen(path)];
	strcpy(buf, path);
	char *ptr_to_buf = buf;
	struct ext2_dir_entry_2 *target = find_dir_entry(disk, ptr_to_buf);
  if (target == NULL) {
    return NULL;
  }
	struct ext2_inode *target_inode = get_inode_table(disk) + TO_INDEX(target->inode);
	if (target_inode->i_mode == EXT2_S_IFLNK) {
		char new_path[PATH_MAX];
		memcpy(new_path, (unsigned char *)(disk + EXT2_BLOCK_SIZE * target_inode->i_block[0]), target_inode->i_size);
		new_path[target_inode->i_size] = '\0';
		char *ptr_to_new_path = new_path;
		return read_symbolic_link(disk, ptr_to_new_path);
	} else {
		return target_inode;
	}
}
