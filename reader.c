/*
 * Copyright 2015-2017, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * reader.c -- example from introduction part 2
 */

#include <sys/time.h>
#include <libpmemobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 #include <time.h> 
PMEMobjpool *pop;
#define TESTNUM 150
#define GROUP_FOR_TEST 5
#define BUCKETCOUNT 160
#define KeySize 20
#define ErrorValue -1
#define PMEMOBJ_POOL (8*1024*1024*100)
#define LAYOUT_NAME "hash"

typedef struct hashEntry entry;

typedef struct hashTable table;
POBJ_LAYOUT_BEGIN(hash);
POBJ_LAYOUT_ROOT(hash,table);
POBJ_LAYOUT_TOID(hash,entry);
POBJ_LAYOUT_TOID(hash,char);
POBJ_LAYOUT_END(hash);


struct hashEntry
{
    TOID(char) key;
    char value;
    TOID(entry) next;
};

struct hashTable
{
    TOID(entry) bucket[BUCKETCOUNT];
};

TOID(table) t;


inline static double wtime()
{
    double time[2];
    struct timeval time1;
    gettimeofday(&time1,NULL);

    time[0] = time1.tv_sec;
    time[1] = time1.tv_usec;

    return time[0] + time[1]*1.0e-6;
}

static int random_num(int limit)
{ 
    
    return rand()%limit+1;
}

static void IntialHash()
{
    int i;
    if (TOID_IS_NULL(t))return;
    for (i = 0; i<BUCKETCOUNT; ++i) {
        D_RW(t)->bucket[i].oid = OID_NULL;
    }
}


//释放哈希表
static void freeHashTable()
{
    int i;
    TOID(entry) e,ep;
    if (TOID_IS_NULL(t))return;
    for (i = 0; i<BUCKETCOUNT; ++i) {
        if(TOID_IS_NULL(D_RW(t)->bucket[i]))
            continue;
        e = D_RW(t)->bucket[i];
        while (!TOID_IS_NULL(e)) {
            ep = D_RW(e)->next;
            TX_FREE(D_RW(e)->key);
            TX_FREE(e);
            e = ep;
        }
        D_RW(t)->bucket[i].oid = OID_NULL;
    }
}
//哈希散列方法函数
static int keyToIndex(const char* key)
{

    int index , len , i;
    if (key == NULL)return -1;

    len = strlen(key);
    index = (int)key[0];
    for (i = 1; i<len; ++i) {
        index *= 1103515245 + (int)key[i];
    }
    index >>= 27;
    index &= (BUCKETCOUNT - 1);

    return index ;
}


//向哈希表中插入数据
static int insertEntry(char * key , const char value)
{
    // printf("insertEntry %s %d\n",key,value );
    int index ;
    index = keyToIndex(key);
    TOID(entry) e , ep;
    if (TOID_IS_NULL(t) || key == NULL) {
        return -1;
    }
    if (TOID_IS_NULL(D_RW(t)->bucket[index])) 
    {
        D_RW(t)->bucket[index] = TX_ZALLOC(entry,sizeof(entry));
        D_RW(D_RW(t)->bucket[index])->key.oid = OID_NULL;
        D_RW(D_RW(t)->bucket[index])->value = 0;
        D_RW(D_RW(t)->bucket[index])->next.oid = OID_NULL;
    }
    if (TOID_IS_NULL(D_RW(D_RW(t)->bucket[index])->key)) {
        D_RW(D_RW(t)->bucket[index])->key = TX_ZALLOC(char,sizeof(char)*KeySize);
        memcpy(pmemobj_direct(D_RW(D_RW(t)->bucket[index])->key.oid),key,KeySize);
        D_RW(D_RW(t)->bucket[index])->value = value;
        D_RW(D_RW(t)->bucket[index])->next.oid = OID_NULL;
    }
    else {
        e = ep = D_RW(t)->bucket[index];
        while (!TOID_IS_NULL(e)) { //先从已有的找
            if (strcmp((char*)pmemobj_direct(D_RW(e)->key.oid) , key) == 0) {
                //找到key所在，替换值
                TX_ADD(e);
                D_RW(e)->value = value;
                return index;   //插入完成了
            }
            ep = e;
            e = D_RW(e)->next;
        } // end while(e...

        //没有在当前桶中找到
        //创建条目加入
        e = TX_ZALLOC(entry,sizeof(entry));
        D_RW(e)->key = TX_ZALLOC(char,sizeof(char)*KeySize);
        memcpy(pmemobj_direct(D_RW(e)->key.oid),key,KeySize);
        D_RW(e)->value = value;
        D_RW(e)->next.oid = OID_NULL;
        D_RW(ep)->next = e;
    }
    return index;
}

//在哈希表中查找key对应的value
//找到了返回value的地址，没找到返回NULL
static char findValueByKey( const char* key)
{
    int index;
    TOID(entry) e;
    if (TOID_IS_NULL(t) || key == NULL) {
        return ErrorValue;
    }
    index = keyToIndex(key);
    e = D_RW(t)->bucket[index];
    if (TOID_IS_NULL(e)) return ErrorValue;//这个桶还没有元素
    while (!TOID_IS_NULL(e)) {
        if (strcmp(key , pmemobj_direct(D_RW(e)->key.oid)) == 0) {
            return D_RW(e)->value;    //找到了，返回值
        }
        e = D_RW(e)->next;
    }
    return ErrorValue;
}

//在哈希表中查找key对应的entry
//找到了返回entry，并将其从哈希表中移除
//没找到返回NULL
static void removeEntry( char* key)
{
    TOID(entry) e,ep;   //查找的时候，把ep作为返回值
    if (TOID_IS_NULL(t) || key == NULL) {
        return ;
    }
    int index;
    index = keyToIndex(key);
    e = D_RW(t)->bucket[index];
    while (!TOID_IS_NULL(e)) {
        if (strcmp(key , pmemobj_direct(D_RW(e)->key.oid)) == 0) {
            if (TOID_EQUALS(e,D_RW(t)->bucket[index])) {
                //如果是桶的第一个
                //如果这个桶有两个或以上元素
                //交换第一个和第二个，然后移除第二个
                ep = D_RW(e)->next;
                if (!TOID_IS_NULL(ep)) {
                    D_RW(t)->bucket[index] = ep;
                }
                else {//这个桶只有第一个元素
                    D_RW(t)->bucket[index].oid = OID_NULL;
                }
            }
            else {
                //如果不是桶的第一个元素
                //找到它的前一个(这是前面设计不佳导致的多余操作)
                ep = D_RW(t)->bucket[index];
                while (!TOID_EQUALS(D_RW(ep)->next,e))ep = D_RW(ep)->next;
                //将e从中拿出来
                D_RW(ep)->next = D_RW(e)->next;
            }
            D_RW(e)->next.oid = OID_NULL;
            TX_FREE(D_RW(e)->key);
            TX_FREE(e);
            return ;
        }// end if(strcmp...
        e = D_RW(e)->next;
    }
    return ;
}

static void printTable()
{
    int num = 0;
    printf("printTable\n");
    int i;
    TOID(entry) e;
    if (TOID_IS_NULL(t))return;
    for (i = 0; i<BUCKETCOUNT; ++i) {
        // printf("\nbucket[%d]:\n" , i);
        e = D_RW(t)->bucket[i];
        if(TOID_IS_NULL(e))
            continue;
        while (!TOID_IS_NULL(D_RW(e)->key) ) {
            // printf("\t%s\t=\t%d\n" , (char*)pmemobj_direct(D_RW(e)->key.oid) , (int)D_RW(e)->value);
            num++;
            if (TOID_IS_NULL(D_RW(e)->next))break;
            e = D_RW(e)->next;
        }
    }
    printf("num = %d\n",num );
}



int main(int argc, const char * argv[]) 
{   
    double t0,t1;
    srand(time(NULL));
    char Data[TESTNUM][KeySize];
    if (argc != 2) {
        printf("usage: %s file-name\n", argv[0]);
        return 1;
    } 

    for(int i = 0;i < TESTNUM/GROUP_FOR_TEST;i++)
    {
        int tkeysize = random_num(KeySize-2);
        for(int j = 0;j< GROUP_FOR_TEST ;j++)
        {
            int z = i*GROUP_FOR_TEST+j;
            int k;
            for(k = 0;k < tkeysize;k++)
            {
                Data[z][k] = random_num(24)+'a'-1; 
            }
            for(;k < KeySize;k++)
                Data[z][k] = (char)0;
            // printf("init Data[z] =%s\n",Data[z]);
        }
    }
    




    pop = pmemobj_open(argv[1], LAYOUT_NAME);
    if (pop == NULL) {
        printf("NOTE:No such file!Create & Open a new one!\n");
        pop = pmemobj_create(argv[1], LAYOUT_NAME,
                PMEMOBJ_POOL, 0666);
        if (pop == NULL) {
            perror("pmemobj_create"); 
            return 1;
        }

pmemobj_ck_totol_initial(pop,PMEMOBJ_POOL);
    }

    double texe=0,tend=0,totol_texe=0,totol_tend=0,tt1,tt2,tt3;
    t0 = wtime();
    TX_BEGIN(pop)
    {
        t = TX_ZALLOC(table,sizeof(table));
        IntialHash();
    }TX_END
    tt1 = wtime();
    printf("initime=%lf\n",tt1-t0 );

    for(int j = 0;j<10;j++)
    {
        TX_BEGIN(pop)
        {
            tt1 = wtime();
            TX_ADD(t);
            for(int i =(TESTNUM/10)*j;i<(TESTNUM/10)*(j+1);i++)
            {
                // printf("insert Data[z]=%s\n",Data[i]);
                insertEntry( Data[i] , i%127);
            }
            tt2 = wtime();
        }TX_END
        tt3 = wtime();
        tend = tt3 -tt2;
        texe = tt2-tt1; 
        totol_texe += texe;
        totol_tend += tend;
        printf(" texe = %lf tend = %lf\n",texe,tend);
    }
    
    printTable();

    for(int j = 0;j<3;j++)
    {
        TX_BEGIN(pop)
        {
            tt1 = wtime();
            TX_ADD(t);
            for(int i =TESTNUM*j;i<TESTNUM*(j+1);i++)
            {
                removeEntry( Data[i]);
            }
            tt2 = wtime();
        }TX_END
        tt3 = wtime();
        tend = tt3 -tt2;
        texe = tt2-tt1; 
        totol_texe += texe;
        totol_tend += tend;
        printf(" texe = %lf tend = %lf\n",texe,tend);
    }
    
    // const char* keys[] = { "632da" , "13dsa","11" , "sad8" };
    // for (int i = 0; i < 4; ++i) {
         findValueByKey("sad8");
    //     if (value != ErrorValue) {
    //         printf("find %s\t=\t%d\n" ,keys[i], value);
    //     }
    //     else {
    //         printf("not found %s\n",keys[i]);
    //     }
    // }
    TX_BEGIN(pop)
    {
        freeHashTable();
    }TX_END
    t1 = wtime();
   printf("time = %lf %lf %lf\n",t1-t0 ,totol_texe,totol_tend);
    getchar();
    return 0;
}