#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

// #include "mm.h"
// #include "memlib.h"

// team_t team = {
//     "6조",
//     "BANG",
//     "BANG",
//     "",
//     ""};

#define ALIGNMENT 8

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) // 4kb 4096byte

#define MAX(x, y)((x)>(y) ? (x) : (y))

// 블록의 크기와(size), 할당여부(alloc 0 or 1) 비트를 비트 or 연산을 통해서 합쳐서 4바이트 int 값으로 만든다 헤더에 들어갈 실제 int 값
#define PACK(size, alloc)((size) | (alloc))

// 포인터 p 를 unsigned int 형 포인터로 형변환 한 뒤, 주소의 값을 역참조 하여 4바이트 값 읽어온다
#define GET(p)  (*(unsigned int*)(p))
// GET의 반대, p의 주소에 4바이트 val 값을 씀
#define PUT(p, val)(*(unsigned int*)(p) = (val))

// 헤더에 저장된 값의 끝이 0 이든 1 이든 0으로 초기화돼서 원래 블록의 사이즈 알 수 있게된다!  
#define GET_SIZE(p)  (GET(p) & ~0x7) 
// 헤더/푸터 값에서 블록 크기 정보는 다 버리고, 오직 할당 상태 비트(1 또는 0)만 깔끔하게 뽑아내준다
#define GET_ALLOC(p) (GET(p) & 0x1) 

#define HDRP(bp) ((char*)(bp) - WSIZE) // 해당 블록의 헤더 주소 계산
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 해당 블록의 footer 주소 계산

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE))) // 내 다음 블록의 페이로드 주소
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) // 내 이전 블록의 페이로드 주소
// 초기 힙 구성시 포인터
static void* heap_listp;

// free list의 첫 가용 블록 포인터
static void* free_listp;

// 포인터 읽기/쓰기 (블록 페이로드에 pred/succ 포인터를 저장하기 위함)
#define GET_PTR(p)  (*(void **)(p))
#define PUT_PTR(p, val) (*(void **)(p) = (val))

// PRED, SUCC 의 위치 
#define PRED_PTR(bp) ((char *)(bp))
#define SUCC_PTR(bp) ((char *)(bp) + sizeof(void *))

// free 블록 최소 크기 -> header + footer + pred + succ
#define P_SIZE (sizeof(void *))
#define MIN_BLOCK_SIZE (DSIZE + 2 * P_SIZE)

static void insert_free_block(void *bp) {
    // LIFO : 맨 앞에 삽입
    if(free_listp != NULL) {
        PUT_PTR(PRED_PTR(free_listp), bp);
    }
    PUT_PTR(SUCC_PTR(bp), free_listp); // 제일 앞에 들어가니까 다음은 free_listp
    PUT_PTR(PRED_PTR(bp), NULL); // 제일 앞에 들어가니까 앞은 NULL
    free_listp = bp; // free list 제일 앞 가리키는 포인터 새로 들어간 블록의 payload로 업데이트
} 

static void delete_free_block(void *bp) {
    void *pred = GET_PTR(PRED_PTR(bp));
    void *succ = GET_PTR(SUCC_PTR(bp));

    if(pred) {
        PUT_PTR(SUCC_PTR(pred), succ);
    } else {
        free_listp = succ;
    }
    if(succ) {
        PUT_PTR(PRED_PTR(succ), pred);
    }
}

static void* coalesce(void* bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case 1
    if (prev_alloc && next_alloc) {}
    // case 2
    else if (prev_alloc && !next_alloc){
        delete_free_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // case 3
    else if (!prev_alloc && next_alloc){
        delete_free_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // case 4
    else{
        delete_free_block(NEXT_BLKP(bp));
        delete_free_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert_free_block(bp);
    return bp;
}

static void* extend_heap(size_t words){
    char* bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1) return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void* find_fit(size_t asize){
    // explicit free list 안에서 first fit
    void* bp;

    for(bp = free_listp; bp != NULL; bp = GET_PTR(SUCC_PTR(bp))) {
        size_t size = GET_SIZE(HDRP(bp));
        if(size >= asize) return bp;
    }

    return NULL; // 힙 전체를 다 뒤져도 못 찾음
}

static void place (void* bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    delete_free_block(bp);
    if((csize - asize) >= (MIN_BLOCK_SIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        void *next_bp = NEXT_BLKP(bp);
        PUT(HDRP(next_bp), PACK(csize - asize, 0));
        PUT(FTRP(next_bp), PACK(csize - asize, 0));
        insert_free_block(next_bp);
    }else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

int mm_init(void){
    /* Create the initial empty head */
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1) return -1;
    
    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* Epliogue header */
    heap_listp += (2*WSIZE);
    free_listp = NULL;
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    return 0;
}

void *mm_malloc(size_t size){
    size_t asize; 
    size_t extendsize;
    char* bp; 

    if(size == 0) return NULL;
    // 요구 크기(asize) 계산: 헤더/푸터 오버헤드 포함하고 8바이트 정렬
    asize = ALIGN(size + DSIZE);
    if (asize < MIN_BLOCK_SIZE) asize = MIN_BLOCK_SIZE;

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

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr)) - DSIZE; // 올바른 코드
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}