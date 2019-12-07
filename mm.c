#include "mm.h"      // prototypes of functions implemented in this file

#include "memlib.h"  // mem_sbrk -- to extend the heap
#include <string.h>  // memcpy -- to copy regions of memory

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

/**
 * A block header uses 4 bytes for:
 * - a block size, multiple of 8 (so, the last 3 bits are always 0's)
 * - an allocated bit (stored as LSB, since the last 3 bits are needed)
 *
 * A block footer has the same format.
 * Check Figure 9.48(a) in the textbook.
 */
typedef int BlockHeader;

/**
 * Read the size field from a block header (or footer).
 *
 * @param bp address of the block header (or footer)
 * @return size in bytes
 */
static int get_size(BlockHeader *bp) {
    return (*bp) & ~7;  // discard last 3 bits
}

/**
 * Read the allocated bit from a block header (or footer).
 *
 * @param bp address of the block header (or footer)
 * @return allocated bit (either 0 or 1)
 */
static int get_allocated(BlockHeader *bp) {
    return (*bp) & 1;   // get last bit
}

/**
 * Write the size and allocated bit of a given block inside its header.
 *
 * @param bp address of the block header
 * @param size size in bytes (must be a multiple of 8)
 * @param allocated either 0 or 1
 */
static void set_header(BlockHeader *bp, int size, int allocated) {
    *bp = size | allocated;
}

/**
 * Write the size and allocated bit of a given block inside its footer.
 *
 * @param bp address of the block header
 * @param size size in bytes (must be a multiple of 8)
 * @param allocated either 0 or 1
 */
static void set_footer(BlockHeader *bp, int size, int allocated) {
    char *footer_addr = (char *)bp + get_size(bp) - 4;
    // the footer has the same format as the header
    set_header((BlockHeader *)footer_addr, size, allocated);
}


/**
 * Find the payload starting address given the address of a block header.
 *
 * The block header is 4 bytes, so the payload starts after 4 bytes.
 *
 * @param bp address of the block header
 * @return address of the payload for this block
 */
static char *get_payload_addr(BlockHeader *bp) {
    return (char *)(bp + 1);
}

/**
 * Find the header address of the previous block on the heap.
 *
 * @param bp address of a block header
 * @return address of the header of the previous block
 */
static BlockHeader *get_prev(BlockHeader *bp) {
    // move back by 4 bytes to find the footer of the previous block
    BlockHeader *previous_footer = bp - 1;
    int previous_size = get_size(previous_footer);
    char *previous_addr = (char *)bp - previous_size;
    return (BlockHeader *)previous_addr;
}

/**
 * Find the header address of the next block on the heap.
 *
 * @param bp address of a block header
 * @return address of the header of the next block
 */
static BlockHeader *get_next(BlockHeader *bp) {
    int this_size = get_size(bp);
    char *next_addr = (char*)bp + this_size;  // TODO: to implement, look at get_prev
    return (BlockHeader *)next_addr;
}

/**
 * In addition to the block header with size/allocated bit, a free block has
 * pointers to the headers of the previous and next blocks on the free list.
 *
 * Pointers use 4 bytes because this project is compiled with -m32.
 * Check Figure 9.48(b) in the textbook.
 */
typedef struct {
    BlockHeader header;
    BlockHeader *prev_free;
    BlockHeader *next_free;
} FreeBlockHeader;

/**
 * Find the header address of the previous **free** block on the **free list**.
 *
 * @param bp address of a block header (it must be a free block)
 * @return address of the header of the previous free block on the list
 */
static BlockHeader *get_prev_free(BlockHeader *bp) {
    FreeBlockHeader *fp = (FreeBlockHeader *)bp;
    return fp->prev_free;
}

/**
 * Find the header address of the next **free** block on the **free list**.
 *
 * @param bp address of a block header (it must be a free block)
 * @return address of the header of the next free block on the list
 */
static BlockHeader *get_next_free(BlockHeader *bp) {
    FreeBlockHeader *fp = (FreeBlockHeader *)bp;
    return fp->next_free;
}

/**
 * Set the pointer to the previous **free** block.
 *
 * @param bp address of a free block header
 * @param prev address of the header of the previous free block (to be set)
 */
static void set_prev_free(BlockHeader *bp, BlockHeader *prev) {
    FreeBlockHeader *fp = (FreeBlockHeader *)bp;
    fp->prev_free = prev;
}

/**
 * Set the pointer to the next **free** block.
 *
 * @param bp address of a free block header
 * @param next address of the header of the next free block (to be set)
 */
static void set_next_free(BlockHeader *bp, BlockHeader *next) {
    FreeBlockHeader *fp = (FreeBlockHeader *)bp;
    fp->next_free = next;
}

/* Pointer to the header of the first block on the heap */
static BlockHeader *heap_blocks;

/* Pointers to the headers of the first and last blocks on the free list */
static BlockHeader *free_headp;
static BlockHeader *free_tailp;

/**
 * Add a block at the beginning of the free list.
 *
 * @param bp address of the header of the block to add
 */
static void free_list_prepend(BlockHeader *bp) {
    //if free list is empty
    if(free_headp == NULL){
        free_headp = bp;
        free_tailp = bp;
        set_next_free(bp, NULL);
        set_prev_free(bp, NULL);
    }
    else{

        //if free list has 1 block
        if(get_next_free(free_headp) == NULL && get_prev_free(free_tailp) == NULL){
            set_prev_free(free_tailp, bp);
        }

        set_next_free(bp, free_headp);
        set_prev_free(free_headp, bp);
        free_headp = bp;
        set_prev_free(free_headp, NULL);
    }
}

/**
 * Add a block at the end of the free list.
 *
 * @param bp address of the header of the block to add
 */
static void free_list_append(BlockHeader *bp) {
    if(free_tailp == NULL){
        free_headp = bp;
        free_tailp = bp;
        set_next_free(bp, NULL);
        set_prev_free(bp,NULL);
    }
    else{

        //if free list has 1 block
        if(get_next_free(free_headp) == NULL && get_prev_free(free_tailp) == NULL){
            set_next_free(free_headp, bp);
        }

        set_next_free(free_tailp, bp);
        set_prev_free(bp, free_tailp);
        free_tailp = bp;
        set_next_free(free_tailp, NULL);
    }
}

/**
 * Remove a block from the free list.
 *
 * @param bp address of the header of the block to remove
 */
static void free_list_remove(BlockHeader *bp) {

    //if free_list has one block
    if(get_next_free(free_headp) == NULL && get_prev_free(free_headp) == NULL){
        free_headp = NULL;
        free_tailp = NULL;
    }
    
    //if block to remove is the head
    else if(free_headp == bp){
        free_headp = get_next_free(free_headp);
        set_prev_free(free_headp, NULL);
    }

    //if block to remove is tail
    else if(free_tailp == bp){
        free_tailp = get_prev_free(free_tailp);
        set_next_free(free_tailp, NULL);
    }

    //if block to remove is in the middle
    else{
          set_next_free(get_prev_free(bp), get_next_free(bp));
          set_prev_free(get_next_free(bp), get_prev_free(bp));
    }
    bp = NULL;
}

/**
 * Mark a block as free, coalesce with contiguous free blocks on the heap, add
 * the coalesced block to the free list.
 *
 * @param bp address of the block to mark as free
 * @return the address of the coalesced block
 */
static BlockHeader *free_coalesce(BlockHeader *bp) {


    // mark block as free
    int size = get_size(bp);
    set_header(bp, size, 0);
    set_footer(bp, size, 0);

    BlockHeader* next = get_next(bp);
    BlockHeader* prev = get_prev(bp);

    // check whether contiguous blocks are allocated
    int prev_alloc = get_allocated(get_prev(bp));
    int next_alloc = get_allocated(get_next(bp));

    //AA
     if (prev_alloc && next_alloc) {
        free_list_append(bp);
        return bp;
    }

    //AF
    else if (prev_alloc && !next_alloc) {
        free_list_remove(next);
        free_list_append(bp);
        set_header(bp, size + get_size(next), 0);
        set_footer(bp, size + get_size(next), 0);
        return bp;

    }

    //FA
    else if (!prev_alloc && next_alloc) {

        int m = get_size(prev);
        set_header(prev, m + size, 0);
        set_footer(prev, m + size, 0);
        return get_prev(bp);
    } 

    //FF
    else {
        int n1 = get_size(prev);
        int n2 = size;
        int n3 = get_size(next);

        set_header(prev, n1 + n2 + n3, 0);
        set_footer(prev, n1 + n2 + n3, 0);

        free_list_remove(next);
        return get_prev(bp);
    }
}

/**
 * Extend the heap with a free block of `size` bytes (multiple of 8).
 *
 * @param size number of bytes to allocate (a multiple of 8)
 * @return pointer to the header of the new free block
 */
static BlockHeader *extend_heap(int size) {

    // bp points to the beginning of the new block
    char *bp = mem_sbrk(size);
    if ((long)bp == -1)
        return NULL;

    // write header over old epilogue, then the footer
    BlockHeader *old_epilogue = (BlockHeader *)bp - 1;
    set_header(old_epilogue, size, 0);
    set_footer(old_epilogue, size, 0);

    // write new epilogue
    set_header(get_next(old_epilogue), 0, 1);

    // merge new block with previous one if possible
    return free_coalesce(old_epilogue);
}

int mm_init(void) {

    // init list of free blocks
    free_headp = NULL;
    free_tailp = NULL;

    // create empty heap of 4 x 4-byte words
    char *new_region = mem_sbrk(16);
    if ((long)new_region == -1)
        return -1;

    heap_blocks = (BlockHeader *)new_region;
    set_header(heap_blocks, 0, 0);      // skip 4 bytes for alignment
    set_header(heap_blocks + 1, 8, 1);  // allocate a block of 8 bytes as prologue
    set_footer(heap_blocks + 1, 8, 1);
    set_header(heap_blocks + 3, 0, 1);  // epilogue
    heap_blocks += 1;                   // point to the prologue header

    // TODO: extend heap with an initial heap size
    BlockHeader *extend = extend_heap(1<<11);
    if(extend == NULL){
        return -1;
    }
    // TODO: extend heap with an initial heap size
    return 0;
}

void mm_free(void *bp) {
 
    BlockHeader* bp_addr = (BlockHeader*) bp;
    BlockHeader* bp_head = bp_addr -1;

    if(get_allocated(bp_head) == 0)
        return;
    
    free_coalesce(bp_head);

}

/**
 * Find a free block with size greater or equal to `size`.
 *
 * @param size minimum size of the free block
 * @return pointer to the header of a free block or `NULL` if free blocks are
 *         all smaller than `size`.
 */
static BlockHeader *find_fit(int size) {
    BlockHeader * bp = free_headp;

    //going through free list to find block
    while(bp != NULL){
        if(get_size(bp) >= size){
            return bp;
        }
        bp = get_next_free(bp);
    }

    return NULL;
}

/**
 * Allocate a block of `size` bytes inside the given free block `bp`.
 *
 * @param bp pointer to the header of a free block of at least `size` bytes
 * @param size bytes to assign as an allocated block (multiple of 8)
 * @return pointer to the header of the allocated block
 */
static BlockHeader *place(BlockHeader *bp, int size) {

    //if bp is already allocated, then removing from free list
    if(get_allocated(bp) == 0){
        free_list_remove(bp);
    }


    int m = get_size(bp);
    int n = size;

    //if there is enough space to make an extra free block
    if(m - n >= 16){


        //if block size is large
        if(n > 100){

            //setting header and footer
            set_header(bp, m - n, 0);
            set_footer(bp, m - n, 0);

            //creating a new free block
            BlockHeader* next = get_next(bp);
            set_header(next, n, 1);
            set_footer(next, n, 1);

            //adding block to list
            free_list_append(bp);
            return next;
        }

        //if a small block
        else{

            //set new allocated block to n size
            set_header(bp, n, 1);
            set_footer(bp, n, 1);

            //creating a new free block
            BlockHeader* next = get_next(bp);
            set_header(next, m - n, 0);
            set_footer(next, m - n, 0);

            //adding block to list
            free_list_append(next);
            return bp;
        }
    }
    else{
        //giving the entire block
        set_header(bp, m, 1);
        set_footer(bp, m, 1);
    }
    return bp;



    // TODO: if current size is greater, use part and add rest to free list
    // TODO: return pointer to header of allocated block
    return NULL;
}

/**
 * Compute the required block size (including space for header/footer) from the
 * requested payload size.
 *
 * @param payload_size requested payload size
 * @return a block size including header/footer that is a multiple of 8
 */
static int required_block_size(int payload_size) {
    payload_size += 8;                    // add 8 for for header/footer
    return ((payload_size + 7) / 8) * 8;  // round up to multiple of 8
}

void *mm_malloc(size_t size) {
    // ignore spurious requests
    if (size == 0)
        return NULL;

    int required_size = required_block_size(size);


    BlockHeader* freeBlock = find_fit(required_size);

    //no freeBlock, must extend heap
    if(freeBlock == NULL){
        freeBlock = extend_heap(required_size);
    }

    freeBlock = place(freeBlock, required_size);

    return get_payload_addr(freeBlock);
}

void *mm_realloc(void *ptr, size_t size) {

    if (ptr == NULL) {
        // equivalent to malloc
        return mm_malloc(size);

    } else if (size == 0) {
        // equivalent to free
        mm_free(ptr);
        return NULL;

    } else {

        //getting bp
        BlockHeader* bp = ptr - 4;

        //finding current block size
        int m = get_size(bp);
        int n = required_block_size(size);

        if((m - n < 24) && (m - n >= 0)){
            return ptr;
        }      
       
       //reuse block
       else if(m - n >= 24){
            mm_malloc(size);
            memcpy(get_payload_addr(get_next(bp)), ptr, n);
            free_list_append(bp);
            return get_payload_addr(get_next(bp));
        }

        else{
            BlockHeader* free;

            if(get_size(get_next(bp)) == 0){

                if(n - m < 24){
                    n = 24 + m;
                    free = extend_heap(24);
                }
                else{
                    free = extend_heap(n - m);
                }

                free_list_remove(free);

                set_header(bp, n, 1);
                set_footer(bp, n, 1);

                return ptr;
            }
        }
    }
}
