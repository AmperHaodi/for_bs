#include <stdio.h>  
#include <sys/time.h>
 
#include <libpmemobj.h>
#include <stdlib.h>  
#include <limits.h>  

#include "layout.h" 
#define N 5000
#define PMEMOBJ_POOL (8*1024*1024*100)
#define LAYOUT_NAME "bplus"


// /* 插入 */  
// extern TOID(BPlusNode) Insert(TOID(BPlusNode) T,KeyType Key);  
// /* 删除 */  
// extern TOID(BPlusNode) Remove(TOID(BPlusNode) T,KeyType Key);  
// /* 销毁 */  
// extern TOID(BPlusNode) Destroy(TOID(BPlusNode) T);  
/* 遍历节点 */  
void Travel(TOID(BPlusNode) T);  
static KeyType Unavailable = INT_MIN;  
PMEMobjpool *pop;  

inline static double wtime()
{
    double time[2];
    struct timeval time1;
    gettimeofday(&time1,NULL);

    time[0] = time1.tv_sec;
    time[1] = time1.tv_usec;

    return time[0] + time[1]*1.0e-6;
}


/* 生成节点并初始化 */  
static TOID(BPlusNode) MallocNewNode()
{  
    // printf("MallocNewNode\n");
    TOID(BPlusNode) NewNode;  
    int i;  
    NewNode = TX_ZALLOC(BPlusNode,sizeof(BPlusNode));
    if (TOID_IS_NULL(NewNode))  
        exit(EXIT_FAILURE);  
    i = 0;  
    while (i < M + 1){  
        D_RW(NewNode)->Key[i] = Unavailable;  
        D_RW(NewNode)->Children[i].oid = OID_NULL;  
        i++;  
    }  
    D_RW(NewNode)->Next.oid = OID_NULL;  
    D_RW(NewNode)->KeyNum = 0; 
    return NewNode;  
}  

static TOID(BPlusNode) Tx_MallocNewNode(){    
    int i;  
    TOID(BPlusNode) NewNode;
    TX_BEGIN(pop){
        NewNode = TX_ZALLOC(BPlusNode,sizeof(BPlusNode));
        if (TOID_IS_NULL(NewNode))  
            exit(EXIT_FAILURE);  
        i = 0;  
        while (i < M + 1){  
            D_RW(NewNode)->Key[i] = Unavailable;  
            D_RW(NewNode)->Children[i].oid = OID_NULL;  
            i++;  
        }  
        D_RW(NewNode)->Next.oid = OID_NULL;  
        D_RW(NewNode)->KeyNum = 0; 
    }TX_END
    return NewNode;  
}  
  

  
static TOID(BPlusNode) FindMostLeft(TOID(BPlusNode) P){  
    TOID(BPlusNode) Tmp;  
      
    Tmp = P;  
      
    while (!TOID_IS_NULL(Tmp) && !TOID_IS_NULL(D_RW(Tmp)->Children[0])) {  
        Tmp = D_RW(Tmp)->Children[0];  
    }  
    return Tmp;  
}  
  
static TOID(BPlusNode) FindMostRight(TOID(BPlusNode) P){  
    TOID(BPlusNode) Tmp;  
      
    Tmp = P;  
      
    while (!TOID_IS_NULL(Tmp) && !TOID_IS_NULL(D_RW(Tmp)->Children[D_RW(Tmp)->KeyNum - 1])) {  
        Tmp = D_RW(Tmp)->Children[D_RW(Tmp)->KeyNum - 1];  
    }  
    TX_ADD(Tmp);
    return Tmp;  
}  
  
/* 寻找一个兄弟节点，其存储的关键字未满，否则返回NULL */  
static TOID(BPlusNode) FindSibling(TOID(BPlusNode) Parent,int i){  
    TOID(BPlusNode) Sibling;  
    int Limit;  
      
    Limit = M;  
      
    Sibling.oid = OID_NULL;  
    if (i == 0){  
        if (D_RW(D_RW(Parent)->Children[1])->KeyNum < Limit)  
            Sibling = D_RW(Parent)->Children[1];  
    }  
    else if (D_RW(D_RW(Parent)->Children[i - 1])->KeyNum < Limit)  
        Sibling = D_RW(Parent)->Children[i - 1];  
    else if (i + 1 < D_RW(Parent)->KeyNum && D_RW(D_RW(Parent)->Children[i + 1])->KeyNum < Limit){  
        Sibling = D_RW(Parent)->Children[i + 1];  
    }  
    
    if(!TOID_IS_NULL(Sibling))
        TX_ADD(Sibling);
    return Sibling;  
}  
  
/* 查找兄弟节点，其关键字数大于M/2 ;没有返回NULL*/  
static TOID(BPlusNode) FindSiblingKeyNum_M_2(TOID(BPlusNode) Parent,int i,int *j){  
    int Limit;  
    TOID(BPlusNode) Sibling;  
    Sibling.oid = OID_NULL;  
      
    Limit = LIMIT_M_2;  
      
    if (i == 0){  
        if (D_RW(D_RW(Parent)->Children[1])->KeyNum > Limit){  
            Sibling = D_RW(Parent)->Children[1];  
            *j = 1;  
        }  
    }  
    else{  
        if (D_RW(D_RW(Parent)->Children[i - 1])->KeyNum > Limit){  
            Sibling = D_RW(Parent)->Children[i - 1];  
            *j = i - 1;  
        }  
        else if (i + 1 < D_RW(Parent)->KeyNum && D_RW(D_RW(Parent)->Children[i + 1])->KeyNum > Limit){  
            Sibling = D_RW(Parent)->Children[i + 1];  
            *j = i + 1;  
        }  
          
    }  
    
    if(!TOID_IS_NULL(Sibling))
        TX_ADD(Sibling);
    return Sibling;  
}  
  
/* 当要对X插入Key的时候，i是X在Parent的位置，j是Key要插入的位置 
   当要对Parent插入X节点的时候，i是要插入的位置，Key和j的值没有用 
 */  
//need tx
static TOID(BPlusNode) InsertElement(int isKey, TOID(BPlusNode) Parent,TOID(BPlusNode) X,KeyType Key,int i,int j)
{    
    // printf("InsertElement\n");
    int k;  
    if (isKey){  
        /* 插入key */  
        
        k = D_RW(X)->KeyNum - 1;  
        while (k >= j){  
            D_RW(X)->Key[k + 1] = D_RW(X)->Key[k];k--;  
        }  
          
        D_RW(X)->Key[j] = Key;  
        D_RW(X)->KeyNum++;  

        if (!TOID_IS_NULL(Parent))  
        {
            TX_ADD(Parent);
            D_RW(Parent)->Key[i] = D_RW(X)->Key[0];
        }       
    }
    else{  
        /* 插入节点 */  
        TX_ADD(Parent);  
        /* 对树叶节点进行连接 */  
        if (TOID_IS_NULL(D_RW(X)->Children[0])){  
            if (i > 0)  
            {
                TX_ADD(D_RW(Parent)->Children[i - 1]);
                D_RW(D_RW(Parent)->Children[i - 1])->Next = X; 
            } 
            D_RW(X)->Next = D_RW(Parent)->Children[i];  
        }  
        k = D_RW(Parent)->KeyNum - 1;  
        while (k >= i){  
            D_RW(Parent)->Children[k + 1] = D_RW(Parent)->Children[k];  
            D_RW(Parent)->Key[k + 1] = D_RW(Parent)->Key[k];  
            k--;  
        }  
        D_RW(Parent)->Key[i] = D_RW(X)->Key[0];  
        D_RW(Parent)->Children[i] = X;  
          
        D_RW(Parent)->KeyNum++;  
          
    }  
    
    return X;  
}  
  
  
static TOID(BPlusNode) RemoveElement(int isKey, TOID(BPlusNode) Parent,TOID(BPlusNode) X,int i,int j)
{        
    int k,Limit;  
    // printf("RemoveElement\n");
    if (isKey){  
        Limit = D_RW(X)->KeyNum;  
        /* 删除key */  
        k = j + 1;  
        while (k < Limit){  
            D_RW(X)->Key[k - 1] = D_RW(X)->Key[k];k++;  
        }  
          
        D_RW(X)->Key[D_RW(X)->KeyNum - 1] = Unavailable;  
          
        D_RW(Parent)->Key[i] = D_RW(X)->Key[0];  
          
        D_RW(X)->KeyNum--;  
    }else{  
        /* 删除节点 */  

        /* 修改树叶节点的链接 */  
        if (TOID_IS_NULL(D_RW(X)->Children[0]) && i > 0)
        {  
            TX_ADD(D_RW(Parent)->Children[i - 1]);
            D_RW(D_RW(Parent)->Children[i - 1])->Next = D_RW(Parent)->Children[i + 1];  
        }  
        Limit = D_RW(Parent)->KeyNum;  
        k = i + 1;  
        while (k < Limit)
        {  
            D_RW(Parent)->Children[k - 1] = D_RW(Parent)->Children[k];  
            D_RW(Parent)->Key[k - 1] = D_RW(Parent)->Key[k];  
            k++;  
        }  
          
        D_RW(Parent)->Children[D_RW(Parent)->KeyNum - 1].oid = OID_NULL;  
        D_RW(Parent)->Key[D_RW(Parent)->KeyNum - 1] = Unavailable;  
          
        D_RW(Parent)->KeyNum--;  
          
    }  

    return X;  
}  
  
/* Src和Dst是两个相邻的节点，i是Src在Parent中的位置； 
 将Src的元素移动到Dst中 ,n是移动元素的个数*/  
static TOID(BPlusNode) MoveElement(TOID(BPlusNode) Src,TOID(BPlusNode) Dst,TOID(BPlusNode) Parent,int i,int n){  
    KeyType TmpKey;  
    TOID(BPlusNode) Child;  
    int j,SrcInFront;  
     // printf("MoveElement\n"); 
    SrcInFront = 0;  
      
    if (D_RW(Src)->Key[0] < D_RW(Dst)->Key[0])  
        SrcInFront = 1;  
      
    j = 0;  
    /* 节点Src在Dst前面 */  
    if (SrcInFront){  
        if (!TOID_IS_NULL(D_RW(Src)->Children[0])){  
            while (j < n) {  
                Child = D_RW(Src)->Children[D_RW(Src)->KeyNum - 1];  
                TX_ADD(Child);
                RemoveElement(0, Src, Child, D_RW(Src)->KeyNum - 1, Unavailable);  
                InsertElement(0, Dst, Child, Unavailable, 0, Unavailable);  
                j++;  
            }  
        }else{  
            while (j < n) {  
                TmpKey = D_RW(Src)->Key[D_RW(Src)->KeyNum -1];  
                RemoveElement(1, Parent, Src, i, D_RW(Src)->KeyNum - 1);  
                InsertElement(1, Parent, Dst, TmpKey, i + 1, 0);  
                j++;  
            }  
              
        }  
        
        D_RW(Parent)->Key[i + 1] = D_RW(Dst)->Key[0];  
        /* 将树叶节点重新连接 */  
        if (D_RW(Src)->KeyNum > 0)  
        {
            D_RW(FindMostRight(Src))->Next = FindMostLeft(Dst);  
        }
          
    }else{  
        if (!TOID_IS_NULL(D_RW(Src)->Children[0]) ){  
            while (j < n) {  
                Child = D_RW(Src)->Children[0];
                TX_ADD(Child); 
                RemoveElement(0, Src, Child, 0, Unavailable);  
                InsertElement(0, Dst, Child, Unavailable, D_RW(Dst)->KeyNum, Unavailable);  
                j++;  
            }  
              
        }else{  
            while (j < n) {  
                TmpKey = D_RW(Src)->Key[0];  
                RemoveElement(1, Parent, Src, i, 0);  
                InsertElement(1, Parent, Dst, TmpKey, i - 1, D_RW(Dst)->KeyNum);  
                j++;  
            }  
              
        }  
        D_RW(Parent)->Key[i] = D_RW(Src)->Key[0];  
        if (D_RW(Src)->KeyNum > 0)  
        {
            D_RW(FindMostRight(Dst))->Next = FindMostLeft(Src);  
        }
    }  
      
    return Parent;  
}  
  
static TOID(BPlusNode) SplitNode(TOID(BPlusNode) Parent,TOID(BPlusNode) X,int i){  
    int j,k,Limit;  
    TOID(BPlusNode) NewNode;  
    // printf("SplitNode\n");
    NewNode = MallocNewNode();  
    k = 0;  
    j = D_RW(X)->KeyNum / 2;  
    Limit = D_RW(X)->KeyNum; 
    while (j < Limit){  
        if (!TOID_IS_NULL(D_RW(X)->Children[0])){  
            D_RW(NewNode)->Children[k] = D_RW(X)->Children[j];  
            D_RW(X)->Children[j].oid = OID_NULL;  
        }  
        D_RW(NewNode)->Key[k] = D_RW(X)->Key[j];  
        D_RW(X)->Key[j] = Unavailable;  
        D_RW(NewNode)->KeyNum++;D_RW(X)->KeyNum--;  
        j++;k++;  
    }  
      
    if (!TOID_IS_NULL(Parent))  
    {
        TX_ADD(Parent);
        InsertElement(0, Parent, NewNode, Unavailable, i + 1, Unavailable);  
    }
    else{  
        /* 如果是X是根，那么创建新的根并返回 */  
        Parent = MallocNewNode();  
        InsertElement(0, Parent, X, Unavailable, 0, Unavailable);  
        InsertElement(0, Parent, NewNode, Unavailable, 1, Unavailable);  
          
        return Parent;  
    }
    return X;  
}   
  
/* 合并节点,X少于M/2关键字，S有大于或等于M/2个关键字*/  
static TOID(BPlusNode) MergeNode(TOID(BPlusNode) Parent, TOID(BPlusNode) X,TOID(BPlusNode) S,int i){  
    int Limit;  
    // printf("MergeNode\n");
    /* S的关键字数目大于M/2 */  
    if (D_RW(S)->KeyNum > LIMIT_M_2){  
        /* 从S中移动一个元素到X中 */  
        MoveElement(S, X, Parent, i,1);  
    }else{  
        /* 将X全部元素移动到S中，并把X删除 */  
        Limit = D_RW(X)->KeyNum;  
        MoveElement(X,S, Parent, i,Limit);  
        RemoveElement(0, Parent, X, i, Unavailable);  
        TX_FREE(X);  
        X.oid = OID_NULL;  
    }  
      
    return Parent;  
}  
  
static TOID(BPlusNode) RecursiveInsert(TOID(BPlusNode) T,KeyType Key,int i,TOID(BPlusNode) Parent){  
    int j,Limit;  
    TOID(BPlusNode) Sibling;  
    // printf("RecursiveInsert\n");      
    /* 查找分支 */  
    j = 0;  
    while (j < D_RW(T)->KeyNum && Key >= D_RW(T)->Key[j]){  
        /* 重复值不插入 */  
        if (Key == D_RW(T)->Key[j])  
            return T;  
        j++;  
    }  
    if (j != 0 && !TOID_IS_NULL(D_RW(T)->Children[0])) j--;  
      
    /* 树叶 */  
    TX_ADD(T);
    if (TOID_IS_NULL(D_RW(T)->Children[0]))  
    {
        T = InsertElement(1, Parent, T, Key, i, j);        
    }
    /* 内部节点 */  
    else  
    { // Recursive
        D_RW(T)->Children[j] = RecursiveInsert(D_RW(T)->Children[j], Key, j, T);  
    }  

  
    /* 调整节点 */  
      
    Limit = M;  
    
    if (D_RW(T)->KeyNum > Limit){  
        /* 根 */  
        TX_ADD(T);
        if (TOID_IS_NULL(Parent)){  
            /* 分裂节点 */  
            T = SplitNode(Parent, T, i);  
        }  
        else{  
            Sibling = FindSibling(Parent, i);  
            if (TOID_IS_NULL(Sibling)){  
                /* 将T的一个元素（Key或者Child）移动的Sibing中 */  
                TX_ADD(Parent);
                MoveElement(T, Sibling, Parent, i, 1);  
            }else{  
                /* 分裂节点 */  
                T = SplitNode(Parent, T, i);  
            }  
        }        
    }  
      
    if (!TOID_IS_NULL(Parent))  
    {
        TX_ADD(Parent);
        D_RW(Parent)->Key[i] = D_RW(T)->Key[0];  
    } 
    TX_ADD(T);
    return T;  
}  
  
/* 插入 */  
static TOID(BPlusNode) Insert(TOID(BPlusNode) T,KeyType Key){  
    TOID(BPlusNode) Tmp,temp;
    temp.oid = OID_NULL;
    Tmp = RecursiveInsert(T, Key, 0, temp);  
    return Tmp;
}  
static TOID(BPlusNode) RecursiveRemove(TOID(BPlusNode) T,KeyType Key,int i,TOID(BPlusNode) Parent){  
      
    int j,NeedAdjust;  
    TOID(BPlusNode) Sibling,Tmp;  
      
    Sibling.oid = OID_NULL;  
     TX_ADD(T);   
    /* 查找分支 */  
    j = 0;  
    while (j < D_RW(T)->KeyNum && Key >= D_RW(T)->Key[j]){  
        if (Key == D_RW(T)->Key[j])  
            break;  
        j++;  
    }  
      
    if (TOID_IS_NULL(D_RW(T)->Children[0])){  
        /* 没找到 */  
        if (Key != D_RW(T)->Key[j] || j == D_RW(T)->KeyNum)  
            return T;  
    }else  
        if (j == D_RW(T)->KeyNum || Key < D_RW(T)->Key[j]) j--;  
      
    /* 树叶 */  
    
    if (TOID_IS_NULL(D_RW(T)->Children[0])){  
        TX_ADD(Parent);
        T = RemoveElement(1, Parent, T, i, j);  
    }else{  
        D_RW(T)->Children[j] = RecursiveRemove(D_RW(T)->Children[j], Key, j, T);  
    }  
    TX_ADD(T);  
    NeedAdjust = 0;  
    /* 树的根或者是一片树叶，或者其儿子数在2到M之间 */  
    if (TOID_IS_NULL(Parent) && !TOID_IS_NULL(D_RW(T)->Children[0]) && D_RW(T)->KeyNum < 2)  
        NeedAdjust = 1;  
    /* 除根外，所有非树叶节点的儿子数在[M/2]到M之间。(符号[]表示向上取整) */  
    else if (!TOID_IS_NULL(Parent) && !TOID_IS_NULL(D_RW(T)->Children[0]) && D_RW(T)->KeyNum < LIMIT_M_2)  
        NeedAdjust = 1;  
    /* （非根）树叶中关键字的个数也在[M/2]和M之间 */  
    else if (!TOID_IS_NULL(Parent) && TOID_IS_NULL(D_RW(T)->Children[0]) && D_RW(T)->KeyNum < LIMIT_M_2)  
        NeedAdjust = 1;  
        /* 调整节点 */  
    if (NeedAdjust){  
        /* 根 */  
        if (TOID_IS_NULL(Parent)){  
            if(!TOID_IS_NULL(D_RW(T)->Children[0]) && D_RW(T)->KeyNum < 2){  
                Tmp = T;  
                T = D_RW(T)->Children[0];  
                TX_FREE(Tmp);
                return T;  
            }  
              
        }else{  
            /* 查找兄弟节点，其关键字数目大于M/2 */  
            Sibling = FindSiblingKeyNum_M_2(Parent, i,&j);  
            TX_ADD(Parent);
            if (!TOID_IS_NULL(Sibling)){  
                MoveElement(Sibling, T, Parent, j, 1);  
            }else{  
                if (i == 0)  
                    Sibling = D_RW(Parent)->Children[1];  
                else  
                    Sibling = D_RW(Parent)->Children[i - 1];  
                TX_ADD(Sibling);
                
                Parent = MergeNode(Parent, T, Sibling, i);  
                T = D_RW(Parent)->Children[i];  
            }  
        }  
          
    }  
    
    return T;  
}  
  
/* 删除 */  
static TOID(BPlusNode) Remove(TOID(BPlusNode) T,KeyType Key){  
    TOID(BPlusNode) Tmp,temp;
    temp.oid = OID_NULL;
    Tmp = RecursiveRemove(T, Key, 0, temp);  
    return Tmp;
}  
  
  
/* 销毁 */  
// static TOID(BPlusNode) Destroy(TOID(BPlusNode) T){
//     TX_BEGIN(pop){  
//         int i,j;  
//         if (!TOID_IS_NULL(T)){  
//             i = 0;  
//             while (i < D_RW(T)->KeyNum + 1){  
//                 Destroy(D_RW(T)->Children[i]);i++;  
//             }  
              
//             printf("Destroy:(");  
//             j = 0;  
//             while (j < D_RW(T)->KeyNum)/*  T->Key[i] != Unavailable*/  
//                 printf("%d:",D_RW(T)->Key[j++]);  
//             printf(") ");  
    
//             POBJ_FREE(&T);  
//             T.oid = OID_NULL;  
//         }  
//     }TX_END
//     return T;  
// }  
  
static void RecursiveTravel(TOID(BPlusNode) T,int Level){  
    int i;  
    if (!TOID_IS_NULL(T)){  
        // printf("  ");  
        // printf("[Level:%d]-->",Level);  
        // printf("(");  
        // i = 0;  
        // while (i < D_RW(T)->KeyNum)/*  T->Key[i] != Unavailable*/  
        //     printf("%d:",D_RW(T)->Key[i++]);  
        // printf(")");  
          
        Level++;  
          
        i = 0;  
        while (i <= D_RW(T)->KeyNum) {  
            RecursiveTravel(D_RW(T)->Children[i], Level);  
            i++;  
        }  
          
          
    }  
}  
  
/* 遍历 */  
void Travel(TOID(BPlusNode) T){  
    RecursiveTravel(T, 0);  
    printf("\n");  
}  


  
/* 遍历树叶节点的数据 */   
// void TravelData(TOID(BPlusNode) T){  
//     TOID(BPlusNode) Tmp;  
//     int i;  
//     if (!TOID_IS_NULL(T))  
//         return ;  
//     printf("All Data:");  
//     Tmp = T;  
//     while (!TOID_IS_NULL(D_RW(Tmp)->Children[0]) )  
//         Tmp = D_RW(Tmp)->Children[0];  
//     /* 第一片树叶 */  
//     while (!TOID_IS_NULL(Tmp)){  
//         i = 0;  
//         while (i < D_RW(Tmp)->KeyNum)  
//             printf(" %d",D_RW(Tmp)->Key[i++]);  
//         Tmp = D_RW(Tmp)->Next;  
//     }  
// }  

 
int main(int argc, const char * argv[]) {  
    int i; 
    int pid = -1;
    double t1,t0; 
    if (argc != 2) {
        printf("usage: %s file-name\n", argv[0]);
        return 1;
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
        t0 = wtime();
        pmemobj_ck_totol_initial(pop,PMEMOBJ_POOL);
        t1 = wtime();
        printf("initime = %lf\n",t1-t0 );
    }
    
    if (M < (3)){  
        printf("M最小等于3！");  
        exit(EXIT_FAILURE);  
    }  
    printf("**********************************************\n");
    printf("*************Really begin       **************\n");
    printf("**********************************************\n");
    /* 根结点 */
    t0 = wtime();
    TOID(BPlusNode) Head = Tx_MallocNewNode();
    TX_BEGIN(pop)
    {
        TX_ADD(Head);
        D_RW(Head)->Children[0] = MallocNewNode();
    }TX_END
        // TOID(BPlusNode) T = POBJ_ROOT(pop, BPlusNode);
    double tt1,tt2,tt3;
    double texe=0,tend=0,totol_texe=0,totol_tend=0;
    int j;
    i = 10*N; 
    for(j = 9;j>=0;j--)
    {
        // if(j == 5) DO_checkpoint(pop,&pid);
        // if(j == 7) DO_checkpoint(pop,&pid);
        // if(j == 6) DO_checkpoint(pop,&pid);
        // if(j == 8) DO_checkpoint(pop,&pid);
        // if(j == 3) DO_checkpoint(pop,&pid);
        TX_BEGIN(pop)
        {
            tt1 = wtime();
            while (i >j*N)
            {              
                TX_ADD(Head);
                D_RW(Head)->Children[0] = Insert(D_RW(Head)->Children[0], i--);
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
    
    i = 3*N; 
    for(j = 2;j>=0;j--)
    {
        TX_BEGIN(pop)
        {
            tt1 = wtime();
            while (i >j*N)
            { 
                TX_ADD(Head);
                D_RW(Head)->Children[0] = Remove(D_RW(Head)->Children[0], i--);
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
    
    t1 =wtime();
    printf("time = %lf %lf %lf\n",t1-t0 ,totol_texe,totol_tend);
    Travel(D_RW(Head)->Children[0]); 
    ck_for_security(pid);
    pmemobj_close(pop);  
}  
