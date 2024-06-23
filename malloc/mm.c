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

/* Pointer to heap start */
static char* heap_start;

#ifdef DEBUG_MALLOC
static void mm_check();
#endif

static void* get_free_list(size_t size)
{
    int index = 0;
    if(size <= 32) index = 0;
    else if(size <= 64) index = 1;
    else if(size <= 128) index = 2;
    else if(size <= 256) index = 3;
    else if(size <= 512) index = 4;
    else if(size <= 1024) index = 5;
    else if(size <= 2048) index = 6;
    else if(size <= 4096) index = 7;
    else index = 8;

    return (heap_start + (index*DSIZE));
}

static void connect_node(void* prev_block, void* next_block, void* current_block)
{
    if(prev_block == NULL && next_block == NULL)
    {
        void* tmp_free_list_header = get_free_list(GET_SIZE(HDRP(current_block)));
        PUTP(tmp_free_list_header, 0);
        return;
    }

    if(prev_block == NULL)
    {
        void* tmp_free_list_header = get_free_list(GET_SIZE(HDRP(next_block)));
        PUTP(GET_PREV_FREE_BLOCK(next_block), 0);
        PUTP(tmp_free_list_header, (long long)next_block);
    } 
    else if(next_block == NULL)
    {
        PUTP(prev_block, 0);
    } 
    else 
    {
        PUTP(prev_block, (long long)next_block);
        PUTP(GET_PREV_FREE_BLOCK(next_block), (long long)prev_block);
    }
}

/*
* init 的情况
*   | (0 | 32] | (32 | 64] | (64 | 128] | (128 | 256 ] | (256 | 512] | (512 | 1024] | (1024 | 2048] | (2048 | 4096] | (4096 | ~] |
*   | 0 | 8/11 | 8/11 | 0/11 |
*/

/* 合并 free block */
static void* coalesce(void *bp) 
{
    char* block_header = HDRP(bp);
    char* next_block_header = HDRP(NEXT_BLKP(bp));
    char* prev_block_header = HDRP(PREV_BLKP(bp));
    size_t size = GET_SIZE(block_header);
    size_t prev_alloc = GET_ASIDE_ALLOC(block_header);
    size_t next_alloc = GET_ALLOC(next_block_header);

    void* free_list_header = get_free_list(size);

    if(prev_alloc && next_alloc) {                 /* Case 1 */
        PUT(block_header, PACK(size, 0x2));
        PUT(FTRP(bp), PACK(size, 0x2));
        PUT(next_block_header, PACK(GET_SIZE(next_block_header), 0x1));
    
        if(!GETP(free_list_header)) 
        {
            PUTP(bp, 0);
            PUTP(GET_PREV_FREE_BLOCK(bp), 0);
        } 
        else 
        {
            PUTP(GET_PREV_FREE_BLOCK(GETP(free_list_header)), (long long)bp);
            PUTP(bp, GETP(free_list_header));
            PUTP(GET_PREV_FREE_BLOCK(bp), 0);
        }

        PUTP(free_list_header, (long long)bp);
        return bp;
    } else if(prev_alloc && !next_alloc) {         /* Case 2 */
        char* next_block = NEXT_BLKP(bp);
        char* next_block_prev = (char*)GETP(GET_PREV_FREE_BLOCK(next_block));
        char* next_block_next = (char*)GETP(next_block);
        connect_node(next_block_prev, next_block_next, next_block);


        size += GET_SIZE(next_block_header);
        PUT(HDRP(bp), PACK(size, 0x2));
        PUT(FTRP(bp), PACK(size, 0x2));
        next_block_header = HDRP(NEXT_BLKP(bp));
        PUT(next_block_header, PACK(GET_SIZE(next_block_header), 0x1));
    } else if(!prev_alloc && next_alloc) {         /* Case 3 */
        char* prev_block = PREV_BLKP(bp);
        char* prev_block_prev = (char*)GETP(GET_PREV_FREE_BLOCK(prev_block));
        char* prev_block_next = (char*)GETP(prev_block);
        connect_node(prev_block_prev, prev_block_next, prev_block);

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
        if(prev_block_next == next_block) 
        {
            connect_node(prev_block_prev, next_block_next, prev_block);
        } else if(prev_block == next_block_next) {
            connect_node(next_block_prev, prev_block_next, next_block);
        } else {
            connect_node(prev_block_prev, prev_block_next, prev_block);
            connect_node(next_block_prev, next_block_next, next_block);
        }
        size += GET_SIZE(prev_block_header) +
            GET_SIZE(next_block_header);
        PUT(prev_block_header, PACK(size, 0x2));
        bp = PREV_BLKP(bp);
        PUT(FTRP(bp), PACK(size, 0x2));
        next_block_header = HDRP(NEXT_BLKP(bp));
        PUT(next_block_header, PACK(GET_SIZE(next_block_header), 0x1));
    }

    size_t new_size = GET_SIZE(HDRP(bp));
    free_list_header = get_free_list(new_size);
    if(!GETP(free_list_header)) 
    {
        PUTP(bp, 0);
        PUTP(GET_PREV_FREE_BLOCK(bp), 0);
    } 
    else 
    {
        PUTP(GET_PREV_FREE_BLOCK(GETP(free_list_header)), (long long)bp);
        PUTP(bp, GETP(free_list_header));
        PUTP(GET_PREV_FREE_BLOCK(bp), 0);
    }

    PUTP(free_list_header, (long long)bp);
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
    *   | (0 | 32] | (32 | 64] | (64 | 128] | (128 | 256 ] | (256 | 512] | (512 | 1024] | (1024 | 2048] | (2048 | 4096] | (4096 | ~] |
    *   | 0 | 8/11 | 8/11 | 0/11 |
    */
    size_t alloc = GET_ASIDE_ALLOC(HDRP(bp));
    PUT(HDRP(bp), PACK(size, alloc));
    PUT(FTRP(bp), PACK(size, alloc));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0x1));
    return coalesce(bp);
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if((heap_listp = mem_sbrk(22 * WSIZE)) == (void*)-1)
        return -1;

    /*
    *   | (0 | 32] | (32 | 64] | (64 | 128] | (128 | 256 ] | (256 | 512] | (512 | 1024] | (1024 | 2048] | (2048 | 4096] | (4096 | ~] |
    *   | 0 | 8/11 | 8/11 | 0/11 |
    */
    PUTP(heap_listp, 0);
    PUTP(heap_listp + DSIZE, 0);
    PUTP(heap_listp + (2*DSIZE), 0);
    PUTP(heap_listp + (3*DSIZE), 0);
    PUTP(heap_listp + (4*DSIZE), 0);
    PUTP(heap_listp + (5*DSIZE), 0);
    PUTP(heap_listp + (6*DSIZE), 0);
    PUTP(heap_listp + (7*DSIZE), 0);
    PUTP(heap_listp + (8*DSIZE), 0);

    PUT(heap_listp + (9*DSIZE), 0);
    PUT(heap_listp + ((9*DSIZE)+WSIZE), PACK(DSIZE, 0x3));
    PUT(heap_listp + (10*DSIZE), PACK(DSIZE, 0x3));
    PUT(heap_listp + ((10*DSIZE)+WSIZE), PACK(0, 0x3));

    heap_start = heap_listp;
    heap_listp += (10*DSIZE);

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

    connect_node(prev_block, next_block, bp);

    if(spare_block_size >= 3*DSIZE) 
    {
        PUT(HDRP(bp), PACK(asize, 0x3));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(spare_block_size, 0x2));        
        PUT(FTRP(NEXT_BLKP(bp)), PACK(spare_block_size, 0x2));

        char* free_block = NEXT_BLKP(bp);
        void* free_list_header = get_free_list(spare_block_size);
        if(!GETP(free_list_header)) 
        {
            PUTP(free_block, 0);
            PUTP(GET_PREV_FREE_BLOCK(free_block), 0);
        } 
        else 
        {
            PUTP(GET_PREV_FREE_BLOCK(GETP(free_list_header)), (long long)free_block);
            PUTP(free_block, GETP(free_list_header));
            PUTP(GET_PREV_FREE_BLOCK(free_block), 0);
        }

        PUTP(free_list_header, (long long)free_block);
    } else 
    {
        PUT(HDRP(bp), PACK(size, 0x3)); 
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(next_size, 0x3));
    }
}

static void* find_fit(size_t asize) 
{
    char* free_list_header = (char*)get_free_list(asize);

    char* free_list = (char*)GETP(free_list_header);
    while(free_list) 
    {
        if(GET_SIZE(HDRP(free_list)) >= asize)
        {
            return free_list;
        }

        free_list = (char*)GETP(free_list); 
    }

    char* free_list_end = heap_listp - (2*DSIZE);
    free_list_header += DSIZE;
    while(free_list_header <= free_list_end)
    {
        free_list = (char*)GETP(free_list_header);
        while(free_list) 
        {
            if(GET_SIZE(HDRP(free_list)) >= asize)
            {
                return free_list;
            }

            free_list = (char*)GETP(free_list);
        }

        free_list_header += DSIZE;
    }

    return NULL;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    //++g_test_count;

    size_t asize;
    size_t extendsize; 
    char* bp;

    if(size == 0)
        return NULL;
    
    if(size <= 3*DSIZE - WSIZE)
        asize = 3 * DSIZE;
    else 
        asize = ALIGN(size + WSIZE);
    

    if((bp = find_fit(asize)) != NULL) 
    {
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

#ifdef DEBUG_MALLOC
void mm_check()
{
    fprintf(stderr, "header list:\n");

    char* tmp_list_header = heap_start, *tmp_list_end = heap_start + (9*DSIZE);
    while(tmp_list_header < tmp_list_end)
    {
        fprintf(stderr, "%p ", (void*)GETP(tmp_list_header));
        tmp_list_header += DSIZE;
    }

    fprintf(stderr, "\n");

    tmp_list_header = heap_listp;

    while(1)
    {
        size_t alloc = GET_ALLOC(HDRP(tmp_list_header));
        size_t size = GET_SIZE(HDRP(tmp_list_header));
        
        if(!alloc)
            fprintf(stderr, "alloc: %lu, aside alloc: %u, size: %lu, pointer: %p, next: %p, prev: %p\t", alloc, GET_ASIDE_ALLOC(HDRP(tmp_list_header)), size, tmp_list_header, (void*)GETP(tmp_list_header),
                (void*)GETP(GET_PREV_FREE_BLOCK(tmp_list_header)));
        else 
            fprintf(stderr, "alloc: %lu, aside alloc: %u, size: %lu, pointer: %p\t", alloc, GET_ASIDE_ALLOC(HDRP(tmp_list_header)), size, tmp_list_header);
        if(size == 0)
        {
            fprintf(stderr, "\n");
            return;
        }

        tmp_list_header = NEXT_BLKP(tmp_list_header);
    }
    
    fprintf(stderr, "\n");
}
#endif