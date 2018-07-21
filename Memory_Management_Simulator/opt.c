#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "sim.h"
#include "pagetable.h"

extern int debug;

extern struct frame *coremap;

struct vaddr {
	addr_t virtual_addr;
    struct vaddr *next;                             //This field points to the next struct vaddr in hashmap (which resolves conflict by chaining)
    struct vaddr *next_in_frame;                    //This field points to the next struct vaddr in coremap 
                                                    //if there is no next struct vaddr in coremap or it is not in coremap then this value is NULL
    int frame_number;                               //This field stores its frame number (coremap index) if it is in the frame, else -1
    struct occurance_sequence *occurance_head;
    struct occurance_sequence *occurance_tail;
};

struct occurance_sequence{
    int occurance;
    struct occurance_sequence *next;                
};

int hashmap_size;
struct vaddr **hashmap_ptr;             //The pointer to hashmap array that points to struct vaddr *
struct vaddr *coremap_head;             //The head of linked list that represents all the nodes inside of coremap
                                        //This linked list is sorted by the occurance in occurance_head, the largest number is stored in the coremap_head
struct vaddr **reference_string;        //The pointer to an array that represents entire reference string
int position;
 
/* This is the hash function used by our hashmap
 * Input: a virtual_address
 * Output: The index of hashmap (from 0 to hashmap_size - 1)
 */
int hash(addr_t virtual_address) {
    return virtual_address % hashmap_size;
}

/* Page to evict is chosen using the optimal (aka MIN) algorithm. 
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {
	int result = coremap_head->frame_number;
	coremap_head->frame_number = -1;    //This is not in frame any more.
	coremap_head = coremap_head->next_in_frame;  //Coremap head now points to the struct vaddr that has the second largest occurance in occurance_head
	return result;
}

/* This function adds new struct vaddr to the sorted Coremap linkedlist 
 * which is sorted by the occurance number in each struct vaddr's occurance_sequence
 * from biggest(or NULL) to the smallest
 */
void add_to_coremap(struct vaddr *virtual_address) {
    if (coremap_head == NULL || virtual_address->occurance_head == NULL) {
        virtual_address->next_in_frame = coremap_head;
        coremap_head = virtual_address;
    } else {
        if (coremap_head->occurance_head != NULL && (virtual_address->occurance_head->occurance > coremap_head->occurance_head->occurance)) {
            virtual_address->next_in_frame = coremap_head;
            coremap_head = virtual_address;
        } else {
            struct vaddr *current = coremap_head;
            //The coremap_head has a higher priority to be evicted, keep looking
            while (current->next_in_frame != NULL && (current->next_in_frame->occurance_head == NULL || current->next_in_frame->occurance_head->occurance > virtual_address->occurance_head->occurance)) {
                current = current->next_in_frame;
            }
            virtual_address->next_in_frame = current->next_in_frame;
            current->next_in_frame = virtual_address;
        }
    }
}

/* This function removes an existing struct vaddr from Coremap Linkedlist
 * And fix the pointers
 */
void remove_from_coremap(struct vaddr *virtual_address) {
    if (coremap_head->virtual_addr == virtual_address->virtual_addr) {
        //Existing struct vaddr is the head of coremap
        coremap_head = coremap_head->next_in_frame;
    } else {
        struct vaddr *current = coremap_head;
        while (current->next_in_frame->virtual_addr != virtual_address->virtual_addr) {
            current = current->next_in_frame;
        }
        current->next_in_frame = current->next_in_frame->next_in_frame;
        virtual_address->next_in_frame = NULL;
    }
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref(pgtbl_entry_t *p) {
    struct vaddr *referenced_vaddr = reference_string[position];
    position += 1;     //Move position to the next reference
    if (referenced_vaddr->occurance_head != NULL) {
        free(referenced_vaddr->occurance_head);
        referenced_vaddr->occurance_head = referenced_vaddr->occurance_head->next; //Make sure the occurance_head always stores the position of next occurance
    }
    if (referenced_vaddr->frame_number == -1) {
        //The newly referenced struct vaddr is not in coremap
        int frame = p->frame >> PAGE_SHIFT;
        referenced_vaddr->frame_number = frame;
        add_to_coremap(referenced_vaddr);
    } else {
        //The referenced struct is in coremap;
        remove_from_coremap(referenced_vaddr);
        add_to_coremap(referenced_vaddr);
    }
	return;
}

/* This function is a helper function of vaddr_record, it adds the new_occurance to the 
 * tail of the occurance_sequence.
 */
void add_occurance_sequence(struct vaddr *add_to, int position) {
    if (add_to->occurance_head == NULL) {
        add_to->occurance_tail = malloc(sizeof(struct occurance_sequence));
        add_to->occurance_tail->occurance = position;
        add_to->occurance_tail->next = NULL;
        add_to->occurance_head = add_to->occurance_tail;
    } else {
        add_to->occurance_tail->next = malloc(sizeof(struct occurance_sequence));
        add_to->occurance_tail->next->occurance = position;
        add_to->occurance_tail->next->next = NULL;
        add_to->occurance_tail = add_to->occurance_tail->next;
    }
}

/* Check if the vaddr_number already exists in hashmap. (Resolves conflict in hashmap by chaining)
 * if the vaddr_number exists in hasmap, add occurance_sequence and return a pointer to it
 * else create a new struct vaddr and add occurance_sequence and return it
 */
struct vaddr *vaddr_record(int position, addr_t vaddr_number, int index) {
    if (hashmap_ptr[index] == NULL) {
        struct vaddr *new_vaddr = malloc(sizeof(struct vaddr));
        new_vaddr->virtual_addr = vaddr_number;
        new_vaddr->next = NULL;
        new_vaddr->next_in_frame = NULL;
        new_vaddr->frame_number = -1;
        new_vaddr->occurance_head = NULL;
	    new_vaddr->occurance_tail = NULL;
        add_occurance_sequence(new_vaddr, position);
        hashmap_ptr[index] = new_vaddr;
        return hashmap_ptr[index];
    } else {
        int found = 0;
        struct vaddr *current = hashmap_ptr[index];
        while (current->next != NULL) {
            if (current->virtual_addr == vaddr_number) {
                found = 1;
                break;
            }
            current = current->next;
        }
        if (current->virtual_addr == vaddr_number) {
            found = 1;
        }
        //The case where struct vaddr with vaddr_number does not exist in hashmap
        if (found == 0) {
            struct vaddr *new_vaddr = malloc(sizeof(struct vaddr));
            new_vaddr->virtual_addr = vaddr_number;
            new_vaddr->next = NULL;
            new_vaddr->next_in_frame = NULL;
            new_vaddr->frame_number = -1;
            new_vaddr->occurance_head = NULL;
	        new_vaddr->occurance_tail = NULL;
            add_occurance_sequence(new_vaddr, position);
            current->next = new_vaddr;
            return new_vaddr;
        } else {
            add_occurance_sequence(current, position);
            return current;
        }
    }
}

/* This function is used to parse the buffer read by fgets
 * Replaces new line character to '\0'
 */
void replace_newline_char(char *buffer, int size) {
    for (int i = 0; i < size; i++) {
        if (buffer[i] == '\n') {
            buffer[i] = '\0';
            break;
        }
    }
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init() {
    //hashmap_ptr = NULL;
    coremap_head = NULL;
    //reference_string = NULL;
    hashmap_size = memsize;         //Initialize the size of hasmap can be any number, but is determinant of running time since we solve collisions by chaining
    position = 0;
    
    //Malloc a hashmap array of size hashmap_size and initialize every entry to NULL
    hashmap_ptr = malloc(sizeof(struct vaddr *) * hashmap_size);
    //struct vaddr *hashmap[hashmap_size];
    //hashmap_ptr = hashmap;
    for (int i = 0; i < hashmap_size; i++) {
        hashmap_ptr[i] = NULL;
    }
    
    //Open the tracefile and generate reference string and struct vaddr in hashmap for each virtual address read
    FILE *tracefile_fp;
    if((tracefile_fp = fopen(tracefile, "r")) == NULL) {
		perror("Error opening tracefile.\n");
		exit(1);
    }
    
    //First read the entire file to retrieve the line number
    int line_number = 0;
    char buffer[20];
    while(fgets(buffer, 20, tracefile_fp) != NULL) {
        line_number += 1;
    }
    
    //Malloc a reference string array of size line_number and initialize every entry to NULL
    reference_string = malloc(sizeof(struct vaddr *) * line_number);
    for (int i = 0; i < line_number; i++) {
        reference_string[i] = NULL;
    }
    
    char type;
    addr_t virtual_addr;
    int index;
    
    fseek(tracefile_fp, 0, SEEK_SET);              //Reset the file pointer to the start of the file
    while (fgets(buffer, 20, tracefile_fp) != NULL) {
        replace_newline_char(buffer, 20);
        sscanf(buffer, "%c %lx", &type, &virtual_addr);
        index = hash(virtual_addr);
        struct vaddr *new_vaddr = vaddr_record(position, virtual_addr, index);
        reference_string[position] = new_vaddr;     //Store the address of existing or new struct vaddr to reference_string
        position += 1;
    }
    position = 0;   //Reset position, now reference_string[position] stores pointer to the start of reference_string
    
    fclose(tracefile_fp);
}

