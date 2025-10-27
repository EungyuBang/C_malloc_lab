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

// --- 기본 매크로 ---
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define WSIZE 4 // 워드 크기 (32비트 기준)
#define DSIZE 8 // 더블 워드 크기 (헤더+푸터 기본 오버헤드)
#define CHUNKSIZE (1<<12) // 초기 힙 확장 크기
#define MAX(x, y)((x)>(y) ? (x) : (y))

// --- 헤더/푸터 및 비트 조작 매크로 (푸터 최적화 적용) ---
// PACK: 크기 | 이전 블록 할당 비트 (1번) | 현재 블록 할당 비트 (0번)
#define PACK(size, alloc)((size) | (alloc))

#define GET(p)  (*(unsigned int*)(p)) // 주소 p에서 4바이트 읽기
#define PUT(p, val)(*(unsigned int*)(p) = (val)) // 주소 p에 4바이트 쓰기

#define GET_SIZE(p)  (GET(p) & ~0x7) // 헤더/푸터에서 크기 읽기 (하위 3비트 무시)
// GET_ALLOC: 현재 블록 할당 비트(0번) 읽기
#define GET_ALLOC(p) (GET(p) & 0x1)
// GET_PREV_ALLOC: 이전 블록 할당 비트(1번) 읽기
#define GET_PREV_ALLOC(p) ((GET(p) & 0x2) >> 1)

// SET_PREV_ALLOC: 헤더 p의 이전 블록 할당 비트(1번)를 설정 (다른 비트는 유지)
#define SET_PREV_ALLOC(p, prev_alloc) PUT(p, (GET(p) & ~0x2) | ((prev_alloc) << 1))

#define HDRP(bp) ((char*)(bp) - WSIZE) // 블록 포인터(bp)로 헤더 주소 계산
// FTRP: 푸터 주소 계산 (최적화 버전에서는 오직 '가용' 블록에만 유효!)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// NEXT_BLKP: 다음 블록 포인터 계산 (헤더 크기 사용)
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))
// PREV_BLKP: 이전 블록 포인터 계산 (이전 블록 '푸터'의 크기 사용 - 가용일 때만 유효!)
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

// --- 전역 변수 ---
static void* heap_listp; // 힙의 시작점 (프롤로그 다음)
static void* free_listp; // 명시적 가용 리스트의 시작점

// --- 명시적 리스트 포인터 매크로 ---
#define GET_PTR(p)  (*(void **)(p)) // 주소 p에서 포인터(주소 값) 읽기
#define PUT_PTR(p, val) (*(void **)(p) = (val)) // 주소 p에 포인터(주소 값) 쓰기

#define PRED_PTR(bp) ((char *)(bp)) // pred 포인터가 저장된 '주소'
#define SUCC_PTR(bp) ((char *)(bp) + sizeof(void *)) // succ 포인터가 저장된 '주소'
#define GET_PRED(bp) (GET_PTR(PRED_PTR(bp))) // pred 포인터 '값' 읽기
#define GET_SUCC(bp) (GET_PTR(SUCC_PTR(bp))) // succ 포인터 '값' 읽기

#define P_SIZE (sizeof(void *)) // 포인터 크기 (4 or 8)
// MIN_BLOCK_SIZE: 가용 블록은 헤더, 푸터, pred, succ 필요
#define MIN_BLOCK_SIZE (DSIZE + 2 * P_SIZE)

// --- 명시적 가용 리스트 함수 ---
static void insert_free_block(void *bp){ // LIFO 삽입
    void *succ = free_listp;
    if (succ != NULL) {
        PUT_PTR(PRED_PTR(succ), bp); // 기존 첫 블록의 PRED가 bp를 가리킴
    }
    PUT_PTR(SUCC_PTR(bp), succ);     // 새 블록의 SUCC가 기존 첫 블록을 가리킴
    PUT_PTR(PRED_PTR(bp), NULL);     // 새 블록의 PRED는 NULL
    free_listp = bp;                 // 루트 포인터 업데이트
}

static void remove_free_block(void *bp){ // 리스트에서 제거
    void *pred = GET_PRED(bp);
    void *succ = GET_SUCC(bp);
    if (pred)
        PUT_PTR(SUCC_PTR(pred), succ); // 이전 블록의 SUCC가 succ를 가리킴
    else
        free_listp = succ;             // 루트 포인터를 succ로 업데이트
    if (succ)
        PUT_PTR(PRED_PTR(succ), pred); // 다음 블록의 PRED가 pred를 가리킴
}

// --- 핵심 로직 함수 ---
static void* coalesce(void* bp){
    // 이전 블록 상태는 '현재' 헤더에서 읽음 (푸터 최적화)
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    void* next_bp = NEXT_BLKP(bp); // 다음 블록 포인터 미리 저장
    void* prev_bp = NULL;          // 이전 블록 포인터 (Case 3, 4에서 사용)
    if (!prev_alloc) {
        prev_bp = PREV_BLKP(bp); // 이전 블록이 가용일 때만 계산
    }


    // Case 1: 양쪽 모두 할당됨 - 병합 없음
    if (prev_alloc && next_alloc) {
        insert_free_block(bp); // 현재 블록만 리스트에 삽입
        SET_PREV_ALLOC(HDRP(next_bp), 0); // 다음 블록 헤더 업데이트: 이전(bp)이 free(0) 상태
        return bp;
    }
    // Case 2: 다음 블록만 가용
    else if (prev_alloc && !next_alloc){
        remove_free_block(next_bp);       // 다음 블록 리스트에서 제거
        size += GET_SIZE(HDRP(next_bp));  // 크기 합산
        // 현재 헤더 업데이트 (이전 할당 상태 유지, 현재=가용)
        PUT(HDRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp)) << 1 | 0));
        PUT(FTRP(bp), PACK(size, 0));     // 푸터 업데이트 (가용 블록이므로 푸터 필요)
        void* next_next_bp = NEXT_BLKP(bp); // 합쳐진 블록의 '다음' 블록
        SET_PREV_ALLOC(HDRP(next_next_bp), 0); // 그 다음 블록 헤더 업데이트: 이전(bp)이 free(0)
    }
    // Case 3: 이전 블록만 가용
    else if (!prev_alloc && next_alloc){
        remove_free_block(prev_bp);       // 이전 블록 리스트에서 제거
        size += GET_SIZE(HDRP(prev_bp));  // 크기 합산
        // 이전 블록 헤더 업데이트 (이전 블록의 이전 할당 상태 유지, 현재=가용)
        PUT(HDRP(prev_bp), PACK(size, GET_PREV_ALLOC(HDRP(prev_bp)) << 1 | 0));
        PUT(FTRP(bp), PACK(size, 0));     // 현재 위치(합쳐진 블록 끝)에 푸터 업데이트
        bp = prev_bp;                     // bp를 이전 블록 위치로 업데이트
        SET_PREV_ALLOC(HDRP(next_bp), 0); // 다음 블록(원래 next_bp) 헤더 업데이트: 이전(bp)이 free(0)
    }
    // Case 4: 양쪽 모두 가용
    else {
        remove_free_block(prev_bp);       // 이전 블록 제거
        remove_free_block(next_bp);       // 다음 블록 제거
        size += GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp)); // 크기 합산 (헤더 사용 일관성)
        // 이전 블록 헤더 업데이트 (이전 블록의 이전 할당 상태 유지, 현재=가용)
        PUT(HDRP(prev_bp), PACK(size, GET_PREV_ALLOC(HDRP(prev_bp)) << 1 | 0));
        PUT(FTRP(next_bp), PACK(size, 0)); // 다음 블록 위치(합쳐진 블록 끝)에 푸터 업데이트
        bp = prev_bp;                     // bp를 이전 블록 위치로 업데이트
        void* next_next_bp = NEXT_BLKP(bp); // 합쳐진 블록의 '다음' 블록
        SET_PREV_ALLOC(HDRP(next_next_bp), 0); // 그 다음 블록 헤더 업데이트: 이전(bp)이 free(0)
    }

    // 최종 병합된 블록(bp)을 가용 리스트에 삽입 (Case 2, 3, 4 공통)
    insert_free_block(bp);
    return bp;
}


static void* extend_heap(size_t words){
    char* bp;
    size_t size;
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; // 크기 정렬
    if((long)(bp = mem_sbrk(size)) == -1) return NULL; // 힙 확장

    // 새 블록 헤더 쓰기 전에, 이전 에필로그 헤더에서 '이전 블록 상태' 읽기
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));

    // 새 가용 블록 헤더 설정 (현재=가용(0), 이전=읽어온 상태)
    PUT(HDRP(bp), PACK(size, prev_alloc << 1 | 0));
    // 새 가용 블록 푸터 설정
    PUT(FTRP(bp), PACK(size, 0));
    // 새 에필로그 헤더 설정 (현재=할당(1), 이전=가용(0))
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0 << 1 | 1));

    // 이전 블록이 가용이었을 수 있으므로 병합 시도
    return coalesce(bp);
}

// find_fit: 변경 없음 (가용 리스트만 순회)
static void* find_fit(size_t asize){
    void* bp;
    for (bp = free_listp; bp != NULL; bp = GET_SUCC(bp)) {
        size_t size = GET_SIZE(HDRP(bp));
        if (size >= asize) return bp;
    }
    return NULL;
}

static void place (void* bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    // 헤더에서 '이전 블록 상태' 읽어두기 (리스트 제거 전에)
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    remove_free_block(bp); // 가용 리스트에서 제거

    if((csize - asize) >= MIN_BLOCK_SIZE){ // 분할 가능?
        // 앞부분 할당
        // 헤더 설정 (현재=할당(1), 이전=읽어온 상태)
        PUT(HDRP(bp), PACK(asize, prev_alloc << 1 | 1));
        // 할당된 블록은 푸터 없음!

        // 뒷부분 (새 가용 블록)
        void *next_bp = NEXT_BLKP(bp);
        // 새 가용 블록 헤더 (현재=가용(0), 이전=할당(1))
        PUT(HDRP(next_bp), PACK(csize - asize, 1 << 1 | 0));
        // 새 가용 블록 푸터
        PUT(FTRP(next_bp), PACK(csize - asize, 0));
        insert_free_block(next_bp); // 가용 리스트에 삽입

        // 그 다음 블록 헤더 업데이트: 이전(next_bp)이 free(0)
        void* next_next_bp = NEXT_BLKP(next_bp);
        SET_PREV_ALLOC(HDRP(next_next_bp), 0);

    }else{ // 통째로 할당
        // 헤더 설정 (현재=할당(1), 이전=읽어온 상태)
        PUT(HDRP(bp), PACK(csize, prev_alloc << 1 | 1));
        // 할당된 블록은 푸터 없음!

        // 다음 블록 헤더 업데이트: 이전(bp)이 allocated(1)
        void* next_bp = NEXT_BLKP(bp);
        SET_PREV_ALLOC(HDRP(next_bp), 1);
    }
}


int mm_init(void){
    // 초기 힙: 패딩, 프롤로그 H/F, 에필로그 H
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1) return -1;
    PUT(heap_listp, 0);                                    // Alignment padding
    // Prologue Header: 크기 DSIZE, 현재=할당(1), 이전=할당(1)
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1 << 1 | 1));
    // Prologue Footer: 크기 DSIZE, 할당(1) (푸터엔 prev_alloc 비트 불필요)
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    // Epilogue Header: 크기 0, 현재=할당(1), 이전=할당(1) (프롤로그)
    PUT(heap_listp + (3*WSIZE), PACK(0, 1 << 1 | 1));
    heap_listp += (2*WSIZE); // heap_listp는 프롤로그 푸터 다음을 가리킴
    free_listp = NULL;       // 가용 리스트 초기화

    // 초기 힙 확장
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    return 0;
}


void *mm_malloc(size_t size){
    size_t asize;
    size_t extendsize;
    char* bp;

    if(size == 0) return NULL;

    // 할당 크기 계산: 오버헤드(헤더만 WSIZE) + 정렬
    // 단, 나중에 free될 때 pred/succ를 넣어야 하므로 MIN_BLOCK_SIZE는 보장
    if (size <= (MIN_BLOCK_SIZE - DSIZE)) // 헤더/푸터 제외한 공간보다 작거나 같으면
        asize = MIN_BLOCK_SIZE;
    else
        asize = ALIGN(size + WSIZE); // 헤더(WSIZE)만 더하고 정렬

    asize = MAX(asize, MIN_BLOCK_SIZE); // 최종적으로 최소 크기 보장

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
    // 헤더 읽기 전에 이전 상태 저장
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(ptr));

    // 헤더 설정 (현재=가용(0), 이전=저장된 상태 유지)
    PUT(HDRP(ptr), PACK(size, prev_alloc << 1 | 0));
    // 푸터 설정 (가용 블록이므로 푸터 필요)
    PUT(FTRP(ptr), PACK(size, 0));

    // 다음 블록 헤더 업데이트: 이전(ptr)이 free(0)
    void* next_bp = NEXT_BLKP(ptr);
    SET_PREV_ALLOC(HDRP(next_bp), 0);

    // 병합 시도 (coalesce가 리스트 삽입까지 처리)
    coalesce(ptr);
}


// mm_realloc: copySize 계산 시 할당 오버헤드를 WSIZE(헤더)만 고려
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    size_t oldPayloadSize;

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    // 기존 블록의 실제 페이로드 크기 계산
    oldPayloadSize = GET_SIZE(HDRP(oldptr)) - WSIZE; // 헤더만 제외

    // 새 블록 할당
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;

    // 복사할 크기 결정
    copySize = oldPayloadSize;
    if (size < copySize)
        copySize = size;

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}