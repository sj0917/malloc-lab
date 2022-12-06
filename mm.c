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
#include "mm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4  //워드 크기
#define DSIZE 8  //더블 워드
#define CHUNKSIZE (1 << 12)  //초기 가용 블록과 힙 확장을 위한 기본 크기(4096)

//파이썬의 max 빌트인 함수와 같은 기능
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* pack a size and allocated bit into a word */
/*
크기와 할당 비트를 통합해서 헤더와 푸터에 저장할 수 있는 값을 리턴함
size는 8비트 단위로 채워져 있는 32비트 데이터.(맨 뒤 3자리 0)
alloc은 1비트만 채워져 있는 32비트 데이터.(맨 뒤 1자리 할당여부에 따라 0 또는 1)
따라서 둘을 비트 통합하면 헤더 워드 전체값을 구할 수 있음
*/
#define PACK(size, alloc) ((size) | (alloc))

/* read and write a word at address p */
//포인터 p가 참조하는 워드를 읽어서 리턴
#define GET(p) (*(unsigned int *)(p))
//포인터 p가 가리키는 워드에 val을 저장
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/*
헤더는 사이즈와 할당여부 정보로 구성되어 있음. 할당 여부는 맨 끝 1비트에,
사이즈는 맨 끝 3비트를 제외한 나머지 비트에 저장 (맨 끝 3비트를 제외하는 이유는
1워드가 8비트 단위로 이루어져 있기 때문) GET_SIZE는 뒤 3비트를 제외한 p 속
데이터의 비트를 가져옴으로써 사이즈를 가져오도록 함 GET_ALLOC은 맨 뒤 1비트를
리턴함으로써 할당 여부를 가져오도록 함
*/
// p에 있는 헤더 또는 푸터가 가지고 있는 블록의 사이즈를 리턴
#define GET_SIZE(p) (GET(p) & ~0x7)
// p에 있는 헤더 또는 푸터가 가지고 있는 블록의 할당 여부 비트 리턴
#define GET_ALLOC(p) (GET(p) & 0x1)

// bp가 속한 블록의 헤더를 가리키는 포인터 리턴
// bp : 블록의 payload 시작점을 가리키는 포인터
#define HDRP(bp) ((char *)(bp)-WSIZE)
//블록 포인터 bp의 블록 푸터를 가리키는 포인터 리턴
// bp의 헤더 블록을 통해 블록 사이즈를 구한 뒤, payload의 시작점 + 블록 사이즈 -
// 더블워드 사이즈(헤더 푸터) => 푸터 시작점
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// bp 다음 블록의 블록 포인터 리턴
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
// bp 이전 블록의 블록 포인터 리턴
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

void *heap_listp;

/*************
 * ************
 * 함수
 * ***************
 * *****************
 * **************/

// bp에 인접한 free 블록들을 연결하는 함수
static void *coalesce(void *bp) {
  // bp 이전 블록의 할당여부
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  // bp 다음 블록의 할당여부
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  // bp가 속한 블록의 사이즈
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc) {
    // 이전 블록, 이후 블록이 모두 할당 상태이면 연결할 필요 없음
    return bp;
  } else if (prev_alloc && !next_alloc) {
    //이전 블록이 할당 상태, 이후 블록이 free 상태이면
    //현재 사이즈에 다음 블록 사이즈 추가
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    //헤더, 푸터에 변경사항 저장
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  } else if (!prev_alloc && next_alloc) {
    // 이전 블록이 free 상태, 이후 블록이 할당 상태이면
    //현재 사이즈에 이전 블록 사이즈 추가
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    //헤더, 푸터에 변경사항 저장
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  } else {
    //이전, 이후 블록 모두 free 상태이면
    //이전 블록 사이즈와 이후 블록 사이즈 더해서 이전 헤더, 이후 푸터에 변경사항
    //저장
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }

  return bp;
}

//힙을 연장하는 함수
static void *extend_heap(size_t words) {
  char *bp;
  size_t size;

  // 정렬 유지를 위해 짝수 넘버를 할당(더블워드 유지를 위해서)
  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  // bp는 힙을 연장한 뒤, 새 영역의 시작 주소
  if ((long)(bp = mem_sbrk(size)) == -1) return NULL;

  // free 블록 헤더, 푸터와 에필로그 헤더 초기화하기
  PUT(HDRP(bp), PACK(size, 0));          //가용 블록 헤더
  PUT(FTRP(bp), PACK(size, 0));          //가용 블록 푸터
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  //새로운 에필로그 헤더

  // 이전 블록 가용 상태였으면 연결하기
  return coalesce(bp);
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
  // 빈 힙 생성하기
  if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
    return -1;
  }
  PUT(heap_listp, 0);
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
  // heap_listp는 프롤로그 푸터의 시작점을 가리키게 됨
  heap_listp += (2 * WSIZE);

  /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;
  return 0;
}

// first_fit 방식으로 구현한 find_fit
char *first_fit(size_t asize) {
  void *mem_heap_lo_result = mem_heap_lo();
  void *mem_heap_hi_result = mem_heap_hi();
  if (mem_heap_lo_result == NULL || mem_heap_hi_result == NULL) {
    // Handle the case where mem_heap_lo() returned NULL
    mem_init();
    return NULL;
  }
  char *now = (char *)mem_heap_lo_result;
  char *mem_brk = (char *)mem_heap_hi_result;
  // mem_start_brk에서 블록들을 이동해 가면서 free한 size를 찾는다.
  while (1) {
    if (!now) {
      return NULL;
    }
    size_t isAlloc = GET_ALLOC(now);
    size_t blockSize = GET_SIZE(now);

    if (!isAlloc && blockSize >= asize) {
      // 삽입 가능한 free_block
      return now;
    } else {
      if (now == mem_brk) {
        return NULL;
      } else {
        now = NEXT_BLKP(now);
      }
    }
  }
}

// bp에 asize를 할당하기
void place(char *bp, size_t asize) {
  PUT(HDRP(bp), PACK(asize, 1));
  PUT(FTRP(bp), PACK(asize, 1));
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 *
 * size 바이트만큼의 payload를 가진 메모리 블록을 요청하는 함수
 */
void *mm_malloc(size_t size) {
  size_t asize;
  size_t extendsize;
  char *bp;

  //만약 사이즈가 0이면 그냥 리턴
  if (size == 0) return NULL;

  if (size <= DSIZE) {
    //만약 사이즈가 더블워드 크기보다 작으면
    // 블록의 크기는 최소 16바이트 : 8바이트는 정렬 만족을 위해, 다른
    // 8바이트는 헤더와 푸터 오버헤드를 위해서임
    asize = 2 * DSIZE;
  } else {
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }

  // find_fit으로 최적 공간을 찾으면, 그 공간에 할당
  if ((bp = first_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }

  // find_fit으로 최적 공간을 찾지 못하면(자리가 없으면), 힙을 연장해서 그
  // 자리에 할당
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL) return NULL;
  place(bp, asize);
  return bp;

  // int newsize = ALIGN(size + SIZE_T_SIZE);
  // void *p = mem_sbrk(newsize);
  // if (p == (void *)-1)
  //   return NULL;
  // else {
  //   *(size_t *)p = size;
  //   return (void *)((char *)p + SIZE_T_SIZE);
  // }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
  size_t size = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
  void *oldptr = ptr;
  void *newptr;
  size_t copySize;

  newptr = mm_malloc(size);
  if (newptr == NULL) return NULL;
  copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
  if (size < copySize) copySize = size;
  memcpy(newptr, oldptr, copySize);
  mm_free(oldptr);
  return newptr;
}
