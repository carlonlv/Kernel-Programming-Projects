CC = gcc
CFLAGS = -Wall
DEBUG = -g
EXE = readimage ext2gerneral.o ext2_ls ext2_rm ext2_mkdir ext2_ln ext2_cp 
UTILS = ext2general.o ext2general.h ext2.h

all: $(EXE)

readimage: readimage.c ext2.h
	$(CC) $(CFLAGS) $(DEBUG) $^ -o $@

ext2_ls : ext2_ls.c $(UTILS)
	$(CC) $(CFLAGS) $(DEBUG) $^ -o $@

ext2_rm : ext2_rm.c $(UTILS)
	$(CC) $(CFLAGS) $(DEBUG) $^ -o $@

ext2_ln : ext2_ln.c $(UTILS)
	$(CC) $(CFLAGS) $(DEBUG) $^ -o $@

ext2_mkdir : ext2_mkdir.c $(UTILS)
	$(CC) $(CFLAGS) $(DEBUG) $^ -o $@

ext2_cp : ext2_cp.c $(UTILS)
	$(CC) $(CFLAGS) $(DEBUG) $^ -o $@

ext2gerneral.o : ext2general.c ext2general.h ext2.h
	$(CC) $(CFLAGS) $(DEBUG) -c $^

.PHONY: clean

clean :
	rm -f *.o $(EXE) *.h.gch

