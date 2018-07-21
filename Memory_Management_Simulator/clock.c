#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

int oldest_page;
/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int clock_evict() {
	int found = 0;
	pgtbl_entry_t *protential_victim;
	while (found == 0) {
		oldest_page = (oldest_page + 1) % memsize;
		protential_victim = coremap[oldest_page].pte;
		if ((protential_victim->frame & PG_REF) == 0) {
			/*Check if the protential_victim's reference bit is 0.*/
			found = 1;
		} else {
			protential_victim->frame = protential_victim->frame & ~PG_REF; /*Change the reference bit to 0 and give it second chance.*/
		}
	}
	return oldest_page;
}

/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {
	/*Reference bit for each page is properly updated in pagetable.c so nothing should be done here.*/
	p->frame = p->frame | PG_REF; /*But just in case.*/
	return;
}

/* Initialize any data structures needed for this replacement
 * algorithm.
 */
void clock_init() {
	oldest_page = -1; /*This is to make sure the first one to be evicted always starts from 0.*/
}
