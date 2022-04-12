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
void *mem_sbrk(int incr);
team_t team = {
    /* Team name */
    "ylteam",
    /* First member's full name */
    "yulan",
    /* First member's email address */
    "yulan@qq.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define HD_SIZE 4
#define FT_SIZE 4

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define ALLOC_SELF 1
#define ALLOC_PRE 2

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX(x, y) ((x) > (y)) ? (x) : (y)

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & ALLOC_SELF)
#define GET_PRE_ALLOC(p) (GET(p) & ALLOC_PRE)
#define GET_NEXT_ALLOC(p) (GET_ALLOC(NEXT_HD(p)))

#define BP_2_HD(bp) ((char *)(bp)-HD_SIZE)                               /*块指针转头指针*/
#define BP_2_FT(bp) ((char *)(bp) + GET_SIZE(BP_2_HD(bp)) - (ALIGNMENT)) /*块指针转尾指针*/

#define HD_2_BP(p) ((char *)(p) + HD_SIZE)                 /*头指针转块指针*/
#define FT_2_BP(p) ((char *)(p)-GET_SIZE(p) + (ALIGNMENT)) /*尾指针转块指针*/

#define HD_2_FT(p) ((char *)(p) + GET_SIZE(p) - FT_SIZE) /*头指针转尾指针*/
#define FT_2_HD(p) ((char *)(p)-GET_SIZE(p) + HD_SIZE)   /*尾转头*/

#define NEXT_HD(p) ((char *)(p) + GET_SIZE(p)) /*下一块的头指针*/
#define PRE_FT(p) ((char *)(p)-FT_SIZE)        /*上一块的尾指针*/

static size_t chunksize;  /*每次拓展向内核申请的大小，init时定义为page大小*/
static char *heap_header; /*指向堆头部的指针(不是序言块)*/
//static unsigned long int chunknum = 0; /*总共从内核申请的块数*/
/* 
 * mm_init - initialize the malloc package.
 */

static void *coalesce_block(char *p)
{
    int pre_alloc = GET_PRE_ALLOC(p);
    int next_alloc = GET_NEXT_ALLOC(p);

    if (pre_alloc && next_alloc)
    {
        return p;
    }
    else if (!pre_alloc && next_alloc)
    {
        char *pre = FT_2_HD(PRE_FT(p));
        size_t size = GET_SIZE(p) + GET_SIZE(pre);
        PUT(HD_2_FT(p), PACK(size, 0));
        PUT(pre, PACK(size, ALLOC_PRE)); /*一个空闲块的前一个块一定已分配*/
        //PUT(pre,PACK(size,GET_PRE_ALLOC(pre)));
        return pre;
    }
    else if (pre_alloc && !next_alloc)
    {
        char *next_ft = HD_2_FT(NEXT_HD(p));
        size_t size = GET_SIZE(p) + GET_SIZE(next_ft);
        PUT(p, PACK(size, ALLOC_PRE)); /*前一个块已分配*/
        PUT(next_ft, PACK(size, 0));
        return p;
    }
    else
    {
        char *pre = FT_2_HD(PRE_FT(p));
        char *next_ft = HD_2_FT(NEXT_HD(p));
        size_t size = GET_SIZE(p) + GET_SIZE(pre) + GET_SIZE(next_ft);
        PUT(next_ft, PACK(size, 0));
        PUT(pre, PACK(size, ALLOC_PRE)); /*一个空闲块的前一个块一定已分配*/
        //PUT(pre,PACK(size,GET_PRE_ALLOC(pre)));
        return pre;
    }
}

static void *extern_heap(size_t size)
{
    char *p;
    if (((long)(p = mem_sbrk(size)) == -1))
        return NULL;
    p = BP_2_HD(p);
    PUT(p, PACK(size, GET_PRE_ALLOC(p)));
    PUT(HD_2_FT(p), PACK(size, 0));
    PUT(HD_2_FT(p) + FT_SIZE, PACK(0, ALLOC_SELF));

    return coalesce_block(p);
}

/*
int mm_init(void)
{
    chunksize = mem_pagesize();
    char *temp = mem_sbrk(2 * (ALIGNMENT));
    if (temp == (void *)-1)
        return -1;
    PUT(temp, PACK(ALIGNMENT, ALLOC_SELF));
    PUT(temp + FT_SIZE, PACK(ALIGNMENT, ALLOC_SELF));
    PUT(temp + (2 * HD_SIZE), PACK(ALIGNMENT, ALLOC_SELF | ALLOC_PRE));
    PUT(temp + (3 * HD_SIZE), PACK(ALIGNMENT, ALLOC_SELF));
    heap_header = temp + ALIGNMENT;
    if (extern_heap(chunksize) == NULL)
        return -1;
    return 0;
}
*/

int mm_init(void)
{
    chunksize = mem_pagesize();
    chunksize = ALIGN(chunksize);
    //printf("\nchunk size : %d\n",chunksize);
    char *temp = mem_sbrk(4 * (HD_SIZE));
    if (temp == (void *)-1)
        return -1;
    PUT(temp, 0);
    PUT(temp + HD_SIZE, PACK(ALIGNMENT, ALLOC_SELF));
    PUT(temp + (2 * HD_SIZE), PACK(ALIGNMENT, ALLOC_SELF));
    PUT(temp + (3 * HD_SIZE), PACK(0, ALLOC_SELF | ALLOC_PRE));
    heap_header = temp + ALIGNMENT + HD_SIZE;
    if (extern_heap(chunksize) == NULL)
        return -1;
    return 0;
}

/*
int mm_init(void)
{
    chunksize = mem_pagesize();
    char *temp = mem_sbrk((ALIGNMENT));
    if (temp == (void *)-1)
        return -1;
    PUT(temp, ALLOC_SELF);
    PUT(temp + FT_SIZE, PACK(0, ALLOC_SELF | ALLOC_PRE));
    heap_header = temp + HD_SIZE;
    if (extern_heap(chunksize) == NULL)
        return -1;
    return 0;
}*/
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (size == 0)
        return NULL;
    char *p;
    size_t asize = ALIGN(size + HD_SIZE);
    for (p = heap_header; GET_SIZE(p) > 0; p = NEXT_HD(p))
    {
        if (GET_SIZE(p) >= asize && !GET_ALLOC(p))
        {
            break;
        }
    }
    if (GET_SIZE(p) == 0)
    {
        if ((p = extern_heap(MAX(asize, chunksize))) == NULL)
            return NULL;
    }
    size_t old_size = GET_SIZE(p);

    if (asize == old_size)
    {
        PUT(p, PACK(asize, ALLOC_SELF | GET_PRE_ALLOC(p)));
        PUT(HD_2_FT(p), PACK(asize, ALLOC_SELF));
        PUT(HD_2_FT(p) + HD_SIZE, PACK(GET(HD_2_FT(p) + HD_SIZE), ALLOC_PRE));
        return (void *)HD_2_BP(p);
    }
    char *ft = HD_2_FT(p);
    PUT(p, PACK(asize, ALLOC_SELF | GET_PRE_ALLOC(p)));
    PUT(HD_2_FT(p), PACK(asize, ALLOC_SELF));

    PUT(ft, PACK(old_size - asize, 0));
    PUT(FT_2_HD(ft), PACK(old_size - asize, ALLOC_PRE));

    PUT(ft + HD_SIZE, PACK(GET(ft + HD_SIZE), ALLOC_PRE));
    char *bp = HD_2_BP(p);
    return (void *)bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    char *p = BP_2_HD(bp);
    if (!GET_ALLOC(p))
        return;
    size_t size = GET_SIZE(p);
    //int t = PACK(size,GET_PRE_ALLOC(p));
    PUT(p, PACK(size, GET_PRE_ALLOC(p)));
    PUT(HD_2_FT(p), PACK(size, 0));
    PUT(NEXT_HD(p), GET(NEXT_HD(p)) & (~ALLOC_PRE));
    coalesce_block(p);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{

    if (size == 0)
    {
        mm_free(bp);
        return NULL;
    }
    else
    {
        char *oldbp = (char *)bp;
        char *newbp;
        char *current_p = BP_2_HD(bp);
        size_t asize = ALIGN(size + HD_SIZE);
        size_t block_size = GET_SIZE(current_p);
        if (asize == block_size)
        {
            return bp;
        }
        else if (asize < block_size){
            char * ft = HD_2_FT(current_p);
            PUT(current_p,PACK(asize,GET_PRE_ALLOC(current_p) | ALLOC_SELF));
            PUT(HD_2_FT(current_p),PACK(asize,ALLOC_SELF));

            PUT(ft,PACK(block_size-asize,0));
            PUT(FT_2_HD(ft),PACK(block_size-asize,ALLOC_PRE));

            PUT(ft+FT_SIZE,GET(ft+FT_SIZE) & (~ALLOC_PRE) );
            return bp;
        }
        else if (!GET_ALLOC(NEXT_HD(current_p)) && GET_SIZE(NEXT_HD(current_p)) + block_size> asize){
            size_t total_size = GET_SIZE(NEXT_HD(current_p)) + block_size;
            char * ft = HD_2_FT(NEXT_HD(current_p));
            PUT(current_p,PACK(asize,GET_PRE_ALLOC(current_p) | ALLOC_SELF));
            PUT(HD_2_FT(current_p),PACK(asize,ALLOC_SELF));

            PUT(ft,PACK(total_size-asize,0));
            PUT(FT_2_HD(ft),PACK(total_size-asize,ALLOC_PRE));

            PUT(ft+FT_SIZE,GET(ft+FT_SIZE) & (~ALLOC_PRE) );
            return bp;
        }
        else
        {
            newbp = mm_malloc(size);
            if (newbp == NULL)
                return NULL;

            memcpy(newbp, oldbp, size);
            mm_free(oldbp);
            return (void *)newbp;
        }
    }
}
