/*
 * mm.c
 *
 * This is the only file you should modify.
 * Explicit List Allocator
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* $begin mallocmacros */
/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
# define dbg_mm_checkheap(...) mm_checkheap(__VA_ARGS__)
#else
# define dbg_printf(...)
# define dbg_mm_checkheap(...)
#endif


#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)


/* Basic constants and macros */
#define WSIZE       4	/* word size (bytes) */
#define DSIZE       8	/* doubleword size (bytes) */
#define CHUNKSIZE   16	/* initial heap size (bytes) */
#define BLOCKSIZE   24  /* minimum block size */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(int *)(p))
#define PUT(p, val)  (*(int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((void *)(bp) - WSIZE)
#define FTRP(bp)       ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)  ((void *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))

/* Given block ptr bp, compute address of next and previous free blocks */
#define NEXT_FBLKP(bp)(*(void **)(bp + DSIZE))
#define PREV_FBLKP(bp)(*(void **)(bp))
/* $end mallocmacros */

/* Global variables */
static char *heap_listp = 0; /* Pointer to the first block */
static char *free_listp = 0;/* Pointer to the first free block */


/* function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void checkblock(void *bp);
static void insert_first(void *bp);
static void remove_block(void *bp);

/*
 * mm_init - Initialize the memory manager
 */
/* $begin mminit */
int mm_init(void)
{
    /* create the initial empty heap */
    if ((heap_listp = mem_sbrk(2*BLOCKSIZE)) == NULL)
        return -1;
    PUT(heap_listp, 0);                        /* alignment padding */
    
    PUT(heap_listp+WSIZE, PACK(BLOCKSIZE, 1));  /* prologue header */
    PUT(heap_listp+DSIZE, 0);                  /* previous ptr */
    PUT(heap_listp+WSIZE+DSIZE, 0);            /* next ptr */
    
    PUT(heap_listp+BLOCKSIZE, PACK(BLOCKSIZE, 1));  /* prologue footer */
    PUT(heap_listp+WSIZE+BLOCKSIZE, PACK(0, 1));   /* epilogue header */
    free_listp = heap_listp+DSIZE;
    
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}
/* $end mminit */

/*
 * malloc - Allocate a block with at least size bytes of payload
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size)
{
    size_t asize;      /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char *bp;
    /* if (heap_listp == 0){
     mm_init();
     } */
    
    /* Ignore spurious requests */
    if (size <= 0)
        return NULL;
    
    /* Adjust block size to include overhead and alignment reqs. */
    asize = MAX(ALIGN(size) + DSIZE, BLOCKSIZE);
    
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}
/* $end mmmalloc */

/*
 * free - Free a block
 */
/* $begin mmfree */
void mm_free(void *bp)
{
    if(!bp) return;
    
    size_t size = GET_SIZE(HDRP(bp));
    /*if (heap_listp == 0){
     mm_init();
     } */
    
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
    //mm_checkheap(0);
}

/* $end mmfree */

/*
 * realloc - naive implementation of realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;
    size_t asize = MAX(ALIGN(size) + DSIZE, BLOCKSIZE); //Must use adjusted size
    
    /* If size <= 0 then this is just free, and we return NULL. */
    if(size <= 0) {
        free(ptr);
        return 0;
    }
    
    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return malloc(size);
    }
    
    oldsize = GET_SIZE(HDRP(ptr));
    
    if (asize == oldsize)
        return ptr;
    
    if(asize <= oldsize)
    {
        size = asize;
        if(oldsize - size <= BLOCKSIZE)
            return ptr;
        PUT(HDRP(ptr), PACK(size, 1));
        PUT(FTRP(ptr), PACK(size, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(oldsize-size, 1));
        free(NEXT_BLKP(ptr));
        return ptr;
    }
    
    newptr = malloc(size);
    
    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }
    
    /* Copy the old data. */
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);
    
    /* Free the old block. */
    free(ptr);
    
    return newptr;
}

void *mm_calloc (size_t nmemb, size_t size)
{
    void *ptr;
    /*if (heap_listp == 0){
     mm_init();
     }*/
    
    ptr = mm_malloc(nmemb*size);
    bzero(ptr, nmemb*size);
    
    
    return ptr;
}

/*
 * checkheap - Minimal check of the heap for consistency
 */
void mm_checkheap(int verbose)
{
    void *bp = heap_listp;
    
    if (verbose)
        printf("Heap (%p):\n", heap_listp);
    
    if ((GET_SIZE(HDRP(heap_listp)) != BLOCKSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("Bad prologue header\n");
    checkblock(heap_listp);
    
    for (bp = free_listp; GET_ALLOC(HDRP(bp)) == 0; bp = NEXT_FBLKP(bp)) {
        if (verbose)
            printblock(bp);
        checkblock(bp);
    }
    
    if (verbose)
        printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");
}

/* The remaining routines are internal helper routines */

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    void *return_ptr;
    
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if (size < BLOCKSIZE) {
        size = BLOCKSIZE; //Must still allocate for blocksize if size is lesser
    }
    if ((long)(bp = mem_sbrk(size)) < 0)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */
    
    /* Coalesce if the previous block was free */
    return_ptr = coalesce(bp);
    //mm_checkheap(0);
    return return_ptr;
}
/* $end mmextendheap */

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    
    if ((csize - asize) >= BLOCKSIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        remove_block(bp); // block allocated must be removed
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        coalesce(bp); //new free block is adjoined with adjacent free blocks
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        remove_block(bp); //similarly removed
    }
}
/* $end mmplace */

/*
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize)
{
    /* completely different find_fit */
    void *ptr;
    for (ptr = free_listp; GET_ALLOC(HDRP(ptr)) == 0; ptr = NEXT_FBLKP(ptr)) { //moves through free list
        if (asize <= (size_t)GET_SIZE(HDRP(ptr))) { //finds one in size
            return ptr;
        }
    }
    return NULL;
}

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if (prev_alloc && next_alloc) {            /* Case 1 */
        insert_first(bp);
        return bp;
    }
    
    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_block(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }
    
    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        remove_block(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    
    else {                                     /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
        GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_block(PREV_BLKP(bp));
        remove_block(NEXT_BLKP(bp));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    
    insert_first(bp);
    
    return bp;
}

static void printblock(void *bp)
{
    int hsize, halloc, fsize, falloc;
    
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));
    
    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }
    printf("%p: header: [%d:%c] footer: [%d:%c]\n", bp,
               hsize, (halloc ? 'a' : 'f'),
               fsize, (falloc ? 'a' : 'f'));
}

static void checkblock(void *bp)
{
    if ((size_t)bp % 8)
        printf("Error: %p is not doubleword aligned\n", bp);
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
        printf("Error: header does not match footer\n");
}

static void insert_first(void *bp)
{
    NEXT_FBLKP(bp) = free_listp;
    PREV_FBLKP(free_listp) = bp;
    PREV_FBLKP(bp) = NULL;
    free_listp = bp;
}

static void remove_block(void *bp)
{
    if (PREV_FBLKP(bp))
        NEXT_FBLKP(PREV_FBLKP(bp)) = NEXT_FBLKP(bp);
    else
        free_listp = NEXT_FBLKP(bp);
    PREV_FBLKP(NEXT_FBLKP(bp)) = PREV_FBLKP(bp);
}
