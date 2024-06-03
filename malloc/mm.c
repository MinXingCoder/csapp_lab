    /*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE   (1<<12) /* Extend heap by this amout (bytes) */

#define MAX(x, y)   ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)      (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

#define GETP(p)         (*(long long*)(p))
#define PUTP(p, val)    (*(long long*)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)         (GET(p) & ~0x7)
#define GET_ALLOC(p)        (GET(p) & 0x1)
#define GET_ASIDE_ALLOC(p)  (GET(p) & 0x2)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)    ((char*)(bp) - WSIZE)
#define FTRP(bp)    ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

#define GET_PREV_FREE_BLOCK(p)  ((char*)(p) + DSIZE)

/* Private global variables */
static char* heap_listp;
/* Pointer to first free block */
static char* heap_free_root;

/* 合并 free block */
static void* coalesce(void *bp) 
{
    char* block_header = HDRP(bp);
    char* next_block_header = HDRP(NEXT_BLKP(bp));
    char* prev_block_header = HDRP(PREV_BLKP(bp));
    size_t size = GET_SIZE(block_header);
    
    size_t prev_alloc = GET_ASIDE_ALLOC(block_header);
    size_t next_alloc = GET_ALLOC(next_block_header);

    if(prev_alloc && next_alloc) {                 /* Case 1 */
        PUT(block_header, PACK(size, 0x2));
        PUT(FTRP(bp), PACK(size, 0x2));
        PUT(next_block_header, PACK(GET_SIZE(next_block_header), 0x1));
    
        PUTP(bp, GETP(heap_free_root));
        PUTP(GET_PREV_FREE_BLOCK(bp), (long long)heap_free_root);
        PUTP(GET_PREV_FREE_BLOCK(GETP(heap_free_root)), (long long)bp);
        PUTP(heap_free_root, (long long)bp);
        return bp;
    } else if(prev_alloc && !next_alloc) {         /* Case 2 */
        char* next_block = NEXT_BLKP(bp);
        char* next_block_prev = (char*)GETP(GET_PREV_FREE_BLOCK(next_block));
        char* next_block_next = (char*)GETP(next_block);
        PUTP(next_block_prev, (long long)next_block_next);
        PUTP(GET_PREV_FREE_BLOCK(next_block_next), (long long)next_block_prev);

        size += GET_SIZE(next_block_header);
        PUT(HDRP(bp), PACK(size, 0x2));
        PUT(FTRP(bp), PACK(size, 0x2));
        next_block_header = HDRP(NEXT_BLKP(bp));
        PUT(next_block_header, PACK(GET_SIZE(next_block_header), 0x1));
    } else if(!prev_alloc && next_alloc) {         /* Case 3 */
        char* prev_block = PREV_BLKP(bp);
        char* prev_block_prev = (char*)GETP(GET_PREV_FREE_BLOCK(prev_block));
        char* prev_block_next = (char*)GETP(prev_block);
        PUTP(prev_block_prev, (long long)prev_block_next);
        PUTP(GET_PREV_FREE_BLOCK(prev_block_next), (long long)prev_block_prev);

        size += GET_SIZE(prev_block_header);
        PUT(prev_block_header, PACK(size, 0x2));
        bp = PREV_BLKP(bp);
        PUT(FTRP(bp), PACK(size, 0x2));
        PUT(next_block_header, PACK(GET_SIZE(next_block_header), 0x1));
    } else {                                       /* Case 4 */
        char* prev_block = PREV_BLKP(bp);
        char* next_block = NEXT_BLKP(bp);
        char* next_block_next = (char*)GETP(next_block);
        char* next_block_prev = (char*)GETP(GET_PREV_FREE_BLOCK(next_block));
        char* prev_block_next = (char*)GETP(prev_block);
        char* prev_block_prev = (char*)GETP(GET_PREV_FREE_BLOCK(prev_block));
        if((next_block_prev != prev_block) && (next_block_next != prev_block))
        {
            PUTP(next_block_prev, (long long)next_block_next);
            PUTP(GET_PREV_FREE_BLOCK(next_block_next), (long long)next_block_prev);
            PUTP(prev_block_prev, (long long)prev_block_next);
            PUTP(GET_PREV_FREE_BLOCK(prev_block_next), (long long)prev_block_prev);
        } else if(next_block_prev == prev_block){
            PUTP(prev_block_prev, (long long)next_block_next);
            PUTP(GET_PREV_FREE_BLOCK(next_block_next), (long long)prev_block_prev);
        } else {
            PUTP(next_block_prev, (long long)prev_block_next);
            PUTP(GET_PREV_FREE_BLOCK(prev_block_next), (long long)next_block_prev);
        }

        size += GET_SIZE(prev_block_header) +
            GET_SIZE(next_block_header);
        PUT(prev_block_header, PACK(size, 0x2));
        bp = PREV_BLKP(bp);
        PUT(FTRP(bp), PACK(size, 0x2));
        next_block_header = HDRP(NEXT_BLKP(bp));
        PUT(next_block_header, PACK(GET_SIZE(next_block_header), 0x1));
    }

    PUTP(bp, GETP(heap_free_root));
    PUTP(GET_PREV_FREE_BLOCK(bp), (long long)heap_free_root);
    PUTP(GET_PREV_FREE_BLOCK(GETP(heap_free_root)), (long long)bp);
    PUTP(heap_free_root, (long long)bp);
    return bp;
}

static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }    
    /*
    * init 的情况
    *   | 0 | 24/11 | next | pointer | prev | pointer | 24/11 | 0/11 | next | pointer | prev | pointer |
    */
    bp -= 4*WSIZE;
    char* prev_block = (char*)GETP(GET_PREV_FREE_BLOCK(bp));

    size_t alloc = GET_ASIDE_ALLOC(HDRP(bp));
    PUT(HDRP(bp), PACK(size, alloc));
    PUT(FTRP(bp), PACK(size, alloc));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0x1));

    PUTP(NEXT_BLKP(bp), 0);
    PUTP(GET_PREV_FREE_BLOCK(NEXT_BLKP(bp)), (long long)prev_block);
    PUTP(prev_block, (long long)NEXT_BLKP(bp));
    return coalesce(bp);
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    /*
    *   | WSIZE | WSIZE | WSIZE | WSIZE | WSIZE | WSIZE | WSIZE | WSIZE | WSIZE | WSIZE | WSIZE | WSIZE |
    */
    if((heap_listp = mem_sbrk(12 * WSIZE)) == (void*)-1)
        return -1;

    /*
    *   | 0 | 24/11 | next | pointer | prev | pointer | 24/11 | 0/11 | next | pointer | prev | pointer |
    */
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(3*DSIZE, 0x3));
    PUTP(heap_listp + (2*WSIZE), (long long)(heap_listp + (8*WSIZE)));
    PUTP(heap_listp + (4*WSIZE), 0);
    PUT(heap_listp + (6*WSIZE), PACK(3*DSIZE, 0x3));
    PUT(heap_listp + (7*WSIZE), PACK(0, 0x3));
    PUTP(heap_listp + (8*WSIZE), 0);
    PUTP(heap_listp + (10*WSIZE), (long long)(heap_listp + (2*WSIZE)));

    heap_listp += (2*WSIZE);
    heap_free_root = heap_listp;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void place(void *bp, size_t asize)
{
    size_t size = GET_SIZE(HDRP(bp));
    size_t spare_block_size = size - asize;
    
    char* next_block = (char*)GETP(bp);
    char* prev_block = (char*)GETP(GET_PREV_FREE_BLOCK(bp));

    if(spare_block_size >= 3*DSIZE) 
    {
        PUT(HDRP(bp), PACK(asize, 0x3));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(spare_block_size, 0x2));        
        PUT(FTRP(NEXT_BLKP(bp)), PACK(spare_block_size, 0x2));

        char* free_block = NEXT_BLKP(bp);
        PUTP(free_block, (long long)next_block);
        PUTP(GET_PREV_FREE_BLOCK(free_block), (long long)prev_block);
        PUTP(prev_block, (long long)free_block);
        PUTP(GET_PREV_FREE_BLOCK(next_block), (long long)free_block);
    } else {
        PUT(HDRP(bp), PACK(size, 0x3)); 
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(next_size, 0x3));

        PUTP(prev_block, (long long)next_block);
        PUTP(GET_PREV_FREE_BLOCK(next_block), (long long)prev_block);
    }
}

static void* find_fit(size_t asize) 
{
    char* tmp_heap_listp = (char*)GETP(heap_free_root);
    size_t min_size = (size_t)-1;
    char* min_heap_listp = NULL;

    while(1) 
    {
        size_t size = GET_SIZE(HDRP(tmp_heap_listp));
        if(size == 0)
            break;
        
        if(size >= asize) 
        {
            if(size < min_size) {
                min_size = size;
                min_heap_listp = tmp_heap_listp;
            }
        }

        tmp_heap_listp = (char*)GETP(tmp_heap_listp);
    }

    if(min_size < (size_t)-1) {
        return min_heap_listp;
    }

    return NULL;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize; 
    char* bp;

    if(size == 0)
        return NULL;
    
    if(size <= 3*DSIZE - WSIZE)
        asize = 3 * DSIZE;
    else 
        asize = ALIGN(size + WSIZE);

    if((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if (size == 0) 
    {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL) 
    {
        return mm_malloc(size);
    }

    oldsize = GET_SIZE(HDRP(ptr)) - WSIZE;

    if(oldsize >= size) return ptr;
    newptr = mm_malloc(size);

    if (!newptr) 
    {
        return 0;
    }


    if (size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);
    return newptr;
}