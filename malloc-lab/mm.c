#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    "6조",
    "BANG",
    "BANG",
    "",
    ""};

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<10)
#define MAX(x, y)((x)>(y) ? (x) : (y))

#define PACK(size, alloc)((size) | (alloc))

#define GET(p)  (*(unsigned int*)(p))
#define PUT(p, val)(*(unsigned int*)(p) = (val))

#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_PREV_ALLOC(p) ((GET(p) & 0x2) >> 1)

#define SET_PREV_ALLOC(p, prev_alloc) PUT(p, (GET(p) & ~0x2) | ((prev_alloc) << 1))

#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

static void* heap_listp;

#define NUM_LISTS 20
static void* segregated_lists[NUM_LISTS];


#define GET_PTR(p)  (*(void **)(p))
#define PUT_PTR(p, val) (*(void **)(p) = (val))

#define PRED_PTR(bp) ((char *)(bp))
#define SUCC_PTR(bp) ((char *)(bp) + sizeof(void *))

#define GET_PRED(bp) (GET_PTR(PRED_PTR(bp)))
#define GET_SUCC(bp) (GET_PTR(SUCC_PTR(bp)))

#define P_SIZE (sizeof(void *))
#define MIN_BLOCK_SIZE (DSIZE + 2 * P_SIZE)


static int get_list_index(size_t size) {
    if (size <= 24) return 0;
    else if (size <= 32) return 1;
    else if (size <= 48) return 2;
    else if (size <= 64) return 3;
    else if (size <= 96) return 4;
    else if (size <= 128) return 5;
    else if (size <= 192) return 6;
    else if (size <= 256) return 7;
    else if (size <= 384) return 8;
    else if (size <= 512) return 9;
    else if (size <= 768) return 10;
    else if (size <= 1024) return 11;
    else if (size <= 1536) return 12;
    else if (size <= 2048) return 13;
    else if (size <= 3072) return 14;
    else if (size <= 4096) return 15;
    else if (size <= 8192) return 16;
    else if (size <= 16384) return 17;
    else if (size <= 32768) return 18;
    else return 19;
}

static void insert_free_block(void *bp){
    size_t size = GET_SIZE(HDRP(bp));
    int index = get_list_index(size);

    void *head = segregated_lists[index];

    if (head != NULL) {
        PUT_PTR(PRED_PTR(head), bp);
    }
    PUT_PTR(SUCC_PTR(bp), head);
    PUT_PTR(PRED_PTR(bp), NULL);
    segregated_lists[index] = bp;
}

static void remove_free_block(void *bp){
    size_t size = GET_SIZE(HDRP(bp));
    int index = get_list_index(size);

    void *pred = GET_PRED(bp);
    void *succ = GET_SUCC(bp);

    if (pred)
        PUT_PTR(SUCC_PTR(pred), succ);
    else
        segregated_lists[index] = succ;

    if (succ)
        PUT_PTR(PRED_PTR(succ), pred);
}

static void* coalesce(void* bp){
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    void* next_bp = NEXT_BLKP(bp);
    void* prev_bp = NULL;
    if (!prev_alloc) {
        prev_bp = PREV_BLKP(bp);
    }

    if (prev_alloc && next_alloc) {
        insert_free_block(bp);
        SET_PREV_ALLOC(HDRP(next_bp), 0);
        return bp;
    }
    else if (prev_alloc && !next_alloc){
        remove_free_block(next_bp);
        size += GET_SIZE(HDRP(next_bp));
        PUT(HDRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp)) << 1 | 0));
        PUT(FTRP(bp), PACK(size, 0));
        void* next_next_bp = NEXT_BLKP(bp);
        SET_PREV_ALLOC(HDRP(next_next_bp), 0);
    }
    else if (!prev_alloc && next_alloc){
        remove_free_block(prev_bp);
        size += GET_SIZE(HDRP(prev_bp));
        PUT(HDRP(prev_bp), PACK(size, GET_PREV_ALLOC(HDRP(prev_bp)) << 1 | 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = prev_bp;
        SET_PREV_ALLOC(HDRP(next_bp), 0);
    }
    else {
        remove_free_block(prev_bp);
        remove_free_block(next_bp);
        size += GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp));
        PUT(HDRP(prev_bp), PACK(size, GET_PREV_ALLOC(HDRP(prev_bp)) << 1 | 0));
        PUT(FTRP(next_bp), PACK(size, 0));
        bp = prev_bp;
        void* next_next_bp = NEXT_BLKP(bp);
        SET_PREV_ALLOC(HDRP(next_next_bp), 0);
    }
    insert_free_block(bp);
    return bp;
}

static void* extend_heap(size_t words){
    char* bp;
    size_t size;
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1) return NULL;

    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));

    PUT(HDRP(bp), PACK(size, prev_alloc << 1 | 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0 << 1 | 1));

    return coalesce(bp);
}

static void* find_fit(size_t asize){
    int index = get_list_index(asize);
    void* bp;

    for (int i = index; i < NUM_LISTS; i++) {
        for (bp = segregated_lists[i]; bp != NULL; bp = GET_SUCC(bp)) {
            if (GET_SIZE(HDRP(bp)) >= asize) {
                return bp;
            }
        }
    }

    return NULL;
}

static void place (void* bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    remove_free_block(bp);

    if((csize - asize) >= MIN_BLOCK_SIZE){
        PUT(HDRP(bp), PACK(asize, prev_alloc << 1 | 1));

        void *next_bp = NEXT_BLKP(bp);
        PUT(HDRP(next_bp), PACK(csize - asize, 1 << 1 | 0));
        PUT(FTRP(next_bp), PACK(csize - asize, 0));
        insert_free_block(next_bp);

        void* next_next_bp = NEXT_BLKP(next_bp);
        SET_PREV_ALLOC(HDRP(next_next_bp), 0);

    }else{
        PUT(HDRP(bp), PACK(csize, prev_alloc << 1 | 1));
        void* next_bp = NEXT_BLKP(bp);
        SET_PREV_ALLOC(HDRP(next_bp), 1);
    }
}

int mm_init(void){
    memset(segregated_lists, 0, NUM_LISTS * sizeof(void*));

    if((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1) return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1 << 1 | 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0, 1 << 1 | 1));
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    return 0;
}

void *mm_malloc(size_t size){
    size_t asize;
    size_t extendsize;
    char* bp;

    if(size == 0) return NULL;

    if (size <= (MIN_BLOCK_SIZE - DSIZE))
        asize = MIN_BLOCK_SIZE;
    else
        asize = ALIGN(size + WSIZE);

    asize = MAX(asize, MIN_BLOCK_SIZE);

    if ((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;

    place(bp, asize);
    return bp;
}

void mm_free(void *ptr){
    size_t size = GET_SIZE(HDRP(ptr));
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, prev_alloc << 1 | 0));
    PUT(FTRP(ptr), PACK(size, 0));

    void* next_bp = NEXT_BLKP(ptr);
    SET_PREV_ALLOC(HDRP(next_bp), 0);

    coalesce(ptr);
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    size_t oldsize;
    size_t asize;

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    oldsize = GET_SIZE(HDRP(oldptr));

    if (size <= (MIN_BLOCK_SIZE - DSIZE))
        asize = MIN_BLOCK_SIZE;
    else
        asize = ALIGN(size + WSIZE);
    asize = MAX(asize, MIN_BLOCK_SIZE);

    if (asize <= oldsize) {
        size_t remaining = oldsize - asize;
        if (remaining >= MIN_BLOCK_SIZE) {
            size_t prev_alloc = GET_PREV_ALLOC(HDRP(oldptr));
            PUT(HDRP(oldptr), PACK(asize, prev_alloc << 1 | 1));

            void* next_bp = NEXT_BLKP(oldptr);
            PUT(HDRP(next_bp), PACK(remaining, 1 << 1 | 0));
            PUT(FTRP(next_bp), PACK(remaining, 0));

            void* next_next_bp = NEXT_BLKP(next_bp);
            SET_PREV_ALLOC(HDRP(next_next_bp), 0);

            insert_free_block(next_bp);
        }

        return oldptr;
    }

    void* next_bp = NEXT_BLKP(oldptr);
    size_t next_alloc = GET_ALLOC(HDRP(next_bp));
    size_t combined_size = oldsize + GET_SIZE(HDRP(next_bp));

    if (!next_alloc && combined_size >= asize) {
        remove_free_block(next_bp);

        size_t prev_alloc = GET_PREV_ALLOC(HDRP(oldptr));
        size_t remaining = combined_size - asize;

        if (remaining >= MIN_BLOCK_SIZE) {
            PUT(HDRP(oldptr), PACK(asize, prev_alloc << 1 | 1));

            void* new_next_bp = NEXT_BLKP(oldptr);
            PUT(HDRP(new_next_bp), PACK(remaining, 1 << 1 | 0));
            PUT(FTRP(new_next_bp), PACK(remaining, 0));

            void* next_next_bp = NEXT_BLKP(new_next_bp);
            SET_PREV_ALLOC(HDRP(next_next_bp), 0);

            insert_free_block(new_next_bp);
        } else {
            PUT(HDRP(oldptr), PACK(combined_size, prev_alloc << 1 | 1));

            void* next_next_bp = NEXT_BLKP(oldptr);
            SET_PREV_ALLOC(HDRP(next_next_bp), 1);
        }
        return oldptr;
    }

    if (GET_SIZE(HDRP(NEXT_BLKP(oldptr))) == 0) {
        size_t needed_space = asize - oldsize;

        if ((mem_sbrk(needed_space)) == (void*)-1) {

        } else {
            size_t prev_alloc = GET_PREV_ALLOC(HDRP(oldptr));
            PUT(HDRP(oldptr), PACK(asize, prev_alloc << 1 | 1));
            PUT(HDRP(NEXT_BLKP(oldptr)), PACK(0, 1 << 1 | 1));
            return oldptr;
        }
    }

    size_t padding = 80;

    void *newptr = mm_malloc(size + padding);

    if (newptr == NULL)
        return NULL;

    size_t oldPayloadSize = GET_SIZE(HDRP(oldptr)) - WSIZE;
    size_t copySize = oldPayloadSize;
    if (size < copySize)
        copySize = size;

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}