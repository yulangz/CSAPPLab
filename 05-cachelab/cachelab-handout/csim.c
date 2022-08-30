#include "cachelab.h"
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
int h, v, s, S, E, b, B;
char t[200];
int hits, misses, evictions;
long stempnow = 0;
typedef struct
{
    int valid;
    int tag;
    long LRU_stemp;
} cacheline, *cache_set, **_cache;
_cache cache;
typedef int result_tag;
#define MISS 0
#define HIT 1
#define EVICTION 2
void printUsage();
void init_cache();
void close_cache();
void printverbose(char oper, unsigned int address, int size, char *opresult);
void do_trace();
result_tag access_cache(char oper, unsigned int address, int size);

int main(int argc, char **argv)
{

    int opt;
    while (-1 != (opt = (getopt(argc, argv, "hvs:E:b:t:"))))
    {
        switch (opt)
        {
        case 'h':
            h = 1;
            printUsage();
            break;
        case 'v':
            v = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            strcpy(t, optarg);
            break;
        default:
            printUsage();
            break;
        }
    }
    if (s <= 0 || E <= 0 || b <= 0 || t == NULL) // 如果选项参数不合格就退出
        return -1;
    S = 1 << s;
    B = 1 << b;
    init_cache();
    do_trace();
    close_cache();
    printSummary(hits, misses, evictions);
    return 0;
}

void printUsage()
{
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n"
           "Options:\n"
           "  -h         Print this help message.\n"
           "  -v         Optional verbose flag.\n"
           "  -s <num>   Number of set index bits.\n"
           "  -E <num>   Number of lines per set.\n"
           "  -b <num>   Number of block offset bits.\n"
           "  -t <file>  Trace file.\n\n"
           "Examples:\n"
           "  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n"
           "  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

void init_cache()
{
    cache = (_cache)malloc(sizeof(cache_set) * S);
    int i, j;
    for (i = 0; i < S; i++)
    {
        cache[i] = (cache_set)malloc(sizeof(cacheline) * E);
        for (j = 0; j < E; j++)
        {
            cache[i][j].tag = 0;
            cache[i][j].valid = 0;
            cache[i][j].LRU_stemp = 0;
        }
    }
}

void close_cache()
{
    int i;
    for (i = 0; i < S; i++)
        free(cache[i]);
    free(cache);
}

void printverbose(char oper, unsigned int address, int size, char *opresult)
{
    printf("%c %x,%d %s\n", oper, address, size, opresult);
}

void do_trace()
{
    FILE *fp = fopen(t, "r"); // 读取文件名
    if (fp == NULL)
    {
        printf("open error");
        exit(-1);
    }

    char operation;       // 命令开头的 I L M S
    unsigned int address; // 地址参数
    int size;             // 大小
    while (fscanf(fp, " %c %xu,%d\n", &operation, &address, &size) > 0)
    {
        size = 1;
        result_tag r;
        switch (operation)
        {
        case 'I':
            break;
        case 'L':
            r = access_cache(operation, address, size);
            if (v)
            {
                if (r == HIT)
                    printverbose(operation, address, size, "hit");
                else if (r == MISS)
                    printverbose(operation, address, size, "miss");
                else if (r == EVICTION)
                    printverbose(operation, address, size, "miss eviction");
            }
            break;
        case 'S':
            r = access_cache(operation, address, size);
            if (v)
            {
                if (r == HIT)
                    printverbose(operation, address, size, "hit");
                else if (r == MISS)
                    printverbose(operation, address, size, "miss");
                else if (r == EVICTION)
                    printverbose(operation, address, size, "miss eviction");
            }
            break;
        case 'M':
            r = access_cache(operation, address, size);
            hits++;         //M的第二次一定命中
            if (v)
            {
                if (r == HIT)
                    printverbose(operation, address, size, "hit hit");
                else if (r == MISS)
                    printverbose(operation, address, size, "miss hit");
                else if (r == EVICTION)
                    printverbose(operation, address, size, "miss eviction hit");
            }
            break;
        }
    }
    fclose(fp);
}

result_tag access_cache(char oper, unsigned int address, int size)
{
    int access_tag, access_set;
    int tag_mask, set_mask;
    set_mask = ((1 << (b + s)) - 1) - ((1 << b) - 1);
    access_set = ((set_mask & address) >> b) % S;
    tag_mask = -1 << (b + s);
    access_tag = tag_mask & address;
    int i;
    for (i = 0; i < E; i++)
    {
        if (cache[access_set][i].valid && cache[access_set][i].tag == access_tag) //命中
        {
            hits++;
            cache[access_set][i].LRU_stemp = stempnow++;
            return HIT;
        }
    }

    for (i = 0; i < E; i++)
    {
        if (!cache[access_set][i].valid) //未命中找到空行
        {
            cache[access_set][i].valid = 1;
            cache[access_set][i].tag = access_tag;
            cache[access_set][i].LRU_stemp = stempnow++;
            misses++;
            return MISS;
        }
    }

    //寻找时间戳最小（最少最近使用）的行
    int eviction_line = 0;
    long maxstemp = ~((long)1 << 63);
    for (i = 0; i < E; i++)
    {
        if (cache[access_set][i].LRU_stemp < maxstemp)
        {
            eviction_line = i;
            maxstemp = cache[access_set][i].LRU_stemp;
        }
    }
    //行替换
    cache[access_set][eviction_line].valid = 1;
    cache[access_set][eviction_line].tag = access_tag;
    cache[access_set][eviction_line].LRU_stemp = stempnow++;
    misses++;
    evictions++;
    return EVICTION;
}