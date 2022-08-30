/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";

void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M == 32)
    {
        //cache 大小为 s=5,E=1,b=5,即32组*1行*32字节，共256个int，块大小定为8*8,对于32*32矩阵通过
        int i, j, block_i;
        int t0, t1, t2, t3, t4, t5, t6, t7;
        for (i = 0; i < N; i += 8)
        {
            for (j = 0; j < M; j += 8)
            {
                for (block_i = 0; block_i < 8; block_i++)
                {
                    t0 = A[i + block_i][j + 0];
                    t1 = A[i + block_i][j + 1];
                    t2 = A[i + block_i][j + 2];
                    t3 = A[i + block_i][j + 3];
                    t4 = A[i + block_i][j + 4];
                    t5 = A[i + block_i][j + 5];
                    t6 = A[i + block_i][j + 6];
                    t7 = A[i + block_i][j + 7];
                    B[j + 0][i + block_i] = t0;
                    B[j + 1][i + block_i] = t1;
                    B[j + 2][i + block_i] = t2;
                    B[j + 3][i + block_i] = t3;
                    B[j + 4][i + block_i] = t4;
                    B[j + 5][i + block_i] = t5;
                    B[j + 6][i + block_i] = t6;
                    B[j + 7][i + block_i] = t7;
                }
            }
        }
    }
    else if (N == 64)
    {
        for (int i = 0; i < N; i += 8)
        {
            for (int j = 0; j < M; j += 8)
            {
                for (int k = i; k < i + 4; ++k)
                {
                    /* 读取1 2，暂时放在左下角1 2 */
                    int temp_value0 = A[k][j];
                    int temp_value1 = A[k][j + 1];
                    int temp_value2 = A[k][j + 2];
                    int temp_value3 = A[k][j + 3];
                    int temp_value4 = A[k][j + 4];
                    int temp_value5 = A[k][j + 5];
                    int temp_value6 = A[k][j + 6];
                    int temp_value7 = A[k][j + 7];

                    B[j][k] = temp_value0;
                    B[j + 1][k] = temp_value1;
                    B[j + 2][k] = temp_value2;
                    B[j + 3][k] = temp_value3;
                    /* 逆序放置 */
                    B[j][k + 4] = temp_value7;
                    B[j + 1][k + 4] = temp_value6;
                    B[j + 2][k + 4] = temp_value5;
                    B[j + 3][k + 4] = temp_value4;
                }
                for (int l = 0; l < 4; ++l)
                {
                    /* 按列读取 */
                    int temp_value0 = A[i + 4][j + 3 - l];
                    int temp_value1 = A[i + 5][j + 3 - l];
                    int temp_value2 = A[i + 6][j + 3 - l];
                    int temp_value3 = A[i + 7][j + 3 - l];
                    int temp_value4 = A[i + 4][j + 4 + l];
                    int temp_value5 = A[i + 5][j + 4 + l];
                    int temp_value6 = A[i + 6][j + 4 + l];
                    int temp_value7 = A[i + 7][j + 4 + l];

                    /* 从下向上按行转换2到3 */
                    B[j + 4 + l][i] = B[j + 3 - l][i + 4];
                    B[j + 4 + l][i + 1] = B[j + 3 - l][i + 5];
                    B[j + 4 + l][i + 2] = B[j + 3 - l][i + 6];
                    B[j + 4 + l][i + 3] = B[j + 3 - l][i + 7];
                    /* 将3 4放到正确的位置 */
                    B[j + 3 - l][i + 4] = temp_value0;
                    B[j + 3 - l][i + 5] = temp_value1;
                    B[j + 3 - l][i + 6] = temp_value2;
                    B[j + 3 - l][i + 7] = temp_value3;
                    B[j + 4 + l][i + 4] = temp_value4;
                    B[j + 4 + l][i + 5] = temp_value5;
                    B[j + 4 + l][i + 6] = temp_value6;
                    B[j + 4 + l][i + 7] = temp_value7;
                }
            }
        }
    }
    else
    { //没有什么特征，只能试块大小，在14的时候misses为1996，通过了
        int block_size = 14;
        int i, j, bl_i, bl_j;
        for (i = 0; i < N; i += block_size)
            for (j = 0; j < M; j += block_size)
                for (bl_i = 0; bl_i < block_size && bl_i + i < N; bl_i++)
                    for (bl_j = 0; bl_j < block_size && bl_j + j < M; bl_j++)
                        B[j + bl_j][i + bl_i] = A[i + bl_i][j + bl_j];
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; j++)
        {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);
}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; ++j)
        {
            if (A[i][j] != B[j][i])
            {
                return 0;
            }
        }
    }
    return 1;
}
