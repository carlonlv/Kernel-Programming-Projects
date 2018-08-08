#define TO_INDEX(NUMBER) ((NUMBER) - 1)
#define TO_NUMBER(INDEX) ((INDEX) + 1)

/** Opens and retuns a pointer to the beginning of the .img file.
 */
unsigned char *read_image(char *disk_name);

/** Returns a pointer to the super block
 */
struct ext2_super_block *get_super_block(unsigned char *disk);

/** Returns a pointer to the group descriptor
 */
struct ext2_group_desc *get_group_descriptor(unsigned char *disk);

/** Returns a pointer to the first inode of the inode table
 */
struct ext2_inode *get_inode_table(unsigned char *disk);

/** Returns a pointer to struct inode for the root inode
 */
struct ext2_inode *get_root_inode(unsigned char *disk);

/** Locate the file(directory entry) from its parent's inode and its name
 *  Note that: input name "." or ".." to locate self directory entry or
 *  parent directory entry
 *  Returns NULL if no such file with name provided exists in the parent inode
 */
struct ext2_dir_entry_2 *locate_file_by_name(unsigned char *disk, struct ext2_inode *inode, char *file_name);

/** Perform a path walk from root directory by the absolute path provided.
 *  Precondition: path must be an absolute path and path must not be root directory
 *  Need to check if path != "/" before use
 *  Returns NULL if no files for directory is found
 *  else returns a ext2_dir_entry_2 struct upon success
 *  Warning: Check if path is absolute before use!!
 */
struct ext2_dir_entry_2 *find_dir_entry(unsigned char *disk, char *path);

/** Update the specific bitmap with the specific block number to a specific value,
 *  Also updates metadatas in superblock and group descriptor correctly
 *  bitmap_type: b stands for block_bitmap, i stands for inode_bitmap
 *  block_num: the specific block you want to modify
 *  value: either change it to 0 or to 1
 */
void update_bitmap(unsigned char *disk, char bitmap_type, unsigned int num, unsigned int value);

/** Locate the index where the last '/' is
 *  Between index 0 and the returned index is the parent directory's path
 *  after the returned index is the filename of the file or directory or link of
 *  the given path.
 */
unsigned int last_slash_in_path(char *path);

/** Returns inode number of parent directory of the dir_entry provided with filepath equals
 *  to path.
 *  Returns 0 if parent directory does not exists
 */
unsigned int find_parent_inode(unsigned char *disk, char *path);

/** Check if the directory entry is named "." or ".."
 *  Returns 1 if the directory entry is external
 *  Returns 0 if the directory entry is named "." or ".."
 */
unsigned int check_if_external_entry(struct ext2_dir_entry_2 *dir);

/** Find the first place to fit in a struct for an directory entry in the block
 *  returns 0 if the block is full
 *  else properly assign all the attributes and returns 1
 *  Note: Need to assign value to the fields of newly returned struct ext2_dir_entry_2 except for name_len and rec_len
 *  Note: Do not pass in an empty block as an argument to this function
 */
int allocate_space_in_block(unsigned char *disk, unsigned char name_len, unsigned int block_num, unsigned int inode_num, char* file_name, unsigned char file_type);

/** Check the inode bitmap and find the first available inode
 *  Returns the free inode number
 *  Returns 0 if all the inodes are unavailable
 */
unsigned int find_free_inode(unsigned char *disk);

/** Check the block bitmap and find the first available block number
 *  Returns the block number upon success
 *  Returns 0 if all the blocks are unavailable
 */
unsigned int find_free_block(unsigned char *disk);

/** Initialize a new inode
 */
void init_inode(struct ext2_inode *inode,unsigned short mode,unsigned int size);

/** Parse the given path, and obtain the name the file at the end of the
 *  file path.
 */
char *find_file_name_by_path(char *path);

/** Free all the blocks of the given inode
 */
void free_all_blocks_of_inode(unsigned char *disk, struct ext2_inode *dir_entry_inode);

/** Read the content of symbolic link inode, and recursively find the file inode that it points to.
 */
struct ext2_inode *read_symbolic_link(unsigned char *disk, char *path);
