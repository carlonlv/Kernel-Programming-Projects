#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

struct frame *head;       //The frame that stores the head of the frame linkedlist
struct frame *tail;	  //The frame that stores the tail of the frame linkedlist

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {
    int evict = (head->pte->frame) >> PAGE_SHIFT;
    if (head != tail) {
        head = head->next;
        head->previous = NULL;
    } else {
        //The only page in the linked list got evicted (memsize = 1)
        head = NULL;
        tail = NULL;
    }
    coremap[evict].next = NULL;
    coremap[evict].previous = NULL;
	return evict;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {
    int frame_number = p->frame >> PAGE_SHIFT;
    if (coremap[frame_number].next == NULL && coremap[frame_number].previous == NULL) {
        //A new page is referenced
        if (head == NULL) {
            //The new page is the first page
            head = &(coremap[frame_number]);
            tail = &(coremap[frame_number]);
        } else {
            //The new page is the new tail
            tail->next = &(coremap[frame_number]);
            coremap[frame_number].previous = tail;
            coremap[frame_number].next = NULL;
            tail = &(coremap[frame_number]);
        }
    } else {
        //A page in physical memory is referenced
        if (head == tail) {
            //Do nothing here.
        } else if (head == &(coremap[frame_number])) {
            //Head is referenced, move it to tail
            head = head->next; //Already checked head->next is not NULL
            head->previous = NULL;
            tail->next = &(coremap[frame_number]);
            coremap[frame_number].previous = tail;
            coremap[frame_number].next = NULL;
            tail = &(coremap[frame_number]);
        } else if (tail == &(coremap[frame_number])) {
            //Tail is referenced, nothing to do here
        } else {
            //Middle is referenced 
            (coremap[frame_number].next)->previous = coremap[frame_number].previous;
            (coremap[frame_number].previous)->next = coremap[frame_number].next;            //Remove it from it's position
            tail->next = &(coremap[frame_number]);                                          //Add it to the tail
            coremap[frame_number].previous = tail;
            coremap[frame_number].next = NULL;
            tail = &(coremap[frame_number]);
        }
    }
	return;
}


/* Initialize any data structures needed for this
 * replacement algorithm
 */
void lru_init() {
	int i;
	head = NULL;
    	tail = NULL;
	for (i = 0; i < memsize; i++) {
		coremap[i].next = NULL;
		coremap[i].previous = NULL;
	}
}
