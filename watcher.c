
#include <string.h>  
#include <stdio.h>  
#include <stdlib.h>  
#include <sys/time.h>
#include <libpmemobj.h>

#define PMEMOBJ_POOL (8*1024*1024*100)
#define LAYOUT_NAME "bfs"

/************************************************************************/    
#define MaxVertexNum 100    
#define QueueSize 30     
typedef enum{FALSE, TRUE}bool;    
typedef int NodeType;
typedef int VertexType;   
typedef struct queue Queue;
typedef struct node   EdgeNode;
typedef struct vnode   VertexNode;   
typedef struct algraph ALGraph;               //对于简单的应用，无须定义此类型，可直接使用AdjList类型 


POBJ_LAYOUT_BEGIN(bfs);
POBJ_LAYOUT_ROOT(bfs,ALGraph);
POBJ_LAYOUT_TOID(bfs,EdgeNode);
POBJ_LAYOUT_TOID(bfs,VertexNode);
POBJ_LAYOUT_END(bfs);   
struct  queue 
{  
    int front;  
    int rear;  
    int size;  
    NodeType data[QueueSize];  
};  
PMEMobjpool *pop;
TOID(ALGraph) a;
/************************************************************************/    
/* 图的邻接表存储结构                                                   */    

     
bool visited[MaxVertexNum];      
struct node     //边表结点    
{    
    NodeType adjvex;         //邻接点域    
    TOID(EdgeNode) next;  //域链    
    //若是要表示边上的权,则应增加一个数据域    
};    
struct vnode    //顶点边结点    
{    
    VertexType vertex;  //顶点域    
    TOID(EdgeNode) firstedge;//边表头指针    
};   
 
struct algraph    
{    
    VertexNode adjlist[MaxVertexNum];    //邻接表    
    int n;              //图中当前顶点数  
    int e;              //图中当前边数    
};               //对于简单的应用，无须定义此类型，可直接使用AdjList类型 



static bool initALGraph();    
static bool BFSTraverseM(TOID(ALGraph) a);  
static bool BFS(TOID(ALGraph) a, int i); 


// inline static double wtime()
// {
//     double time[2];
//     struct timeval time1;
//     gettimeofday(&time1,NULL);

//     time[0] = time1.tv_sec;
//     time[1] = time1.tv_usec;

//     return time[0] + time[1]*1.0e-6;
// }

/************************************************************************/    
/* 队尾入队                                                             */    
/************************************************************************/  
static bool enQueue(Queue* q, NodeType data)  
{  
    if(q == NULL)  
        return FALSE;  
    if(q->size == QueueSize)  
        return FALSE;  
    q->data[q->rear] = data;  
    q->rear = (q->rear+1) % QueueSize;  
    q->size++;  
      
    return TRUE;  
}  

/************************************************************************/    
/* 队首出队                                                             */    
/************************************************************************/  
static NodeType deQueue(Queue* q)  
{  
    NodeType res;  
    if(q == NULL)  
        exit(0);  
    if(q->size == 0)  
        return FALSE;  
    res = q->data[q->front];  
    q->front = (q->front+1) % QueueSize;  
    q->size--;  
      
    return res;  
}  





static bool initALGraph()       
{  
    a.oid = OID_NULL;  
    TOID(EdgeNode) e;
    e.oid = OID_NULL;  
    int i, j, k;  
    int v1, v2;      
    printf("请输入顶点数和边数(输入格式为:顶点数 边数): ");  
    scanf("%d %d", &i, &j);  
    if(i<0 || j<0)  
        return FALSE;   
    a = TX_ZALLOC(ALGraph,sizeof(ALGraph));
    if(TOID_IS_NULL(a))  
        return FALSE;  
    D_RW(a)->n = i;  
    D_RW(a)->e = j;  
           
    for(i=0;  i<D_RW(a)->n; i++)  
    {  
        printf("输入顶点 ");  
  
        scanf("%d",&(D_RW(a)->adjlist[i].vertex)); // 读入顶点信息       
        D_RW(a)->adjlist[i].firstedge.oid = OID_NULL;            // 点的边表头指针设为空    
    }  
    for(k=0; k<D_RW(a)->e; k++)  
    {  
        printf("输入边 ");   
        scanf("%d %d", &v1, &v2);  
        for(i=0; v1 != D_RW(a)->adjlist[i].vertex; i++); //找到顶点对应的存储序号     
        for(j=0; v2 != D_RW(a)->adjlist[j].vertex; j++);//找到顶点对应的存储序号  
          
        e = TX_ZALLOC(EdgeNode,sizeof(EdgeNode));  
        D_RW(e)->adjvex = i;  
        D_RW(e)->next = D_RW(a)->adjlist[j].firstedge;  
        D_RW(a)->adjlist[j].firstedge = e;  
        e = TX_ZALLOC(EdgeNode,sizeof(EdgeNode));  
        D_RW(e)->adjvex = j;  
        D_RW(e)->next = D_RW(a)->adjlist[i].firstedge;  
        D_RW(a)->adjlist[i].firstedge = e;  
    }  
    return TRUE;  
}      

/************************************************************************/    
/* 广度优先遍历(递归实现)                                               */    
/************************************************************************/    
static bool BFS(TOID(ALGraph) a, int i)  
{  
    int j, k;  
    Queue *q = NULL; 
    TOID(EdgeNode) e;
    e.oid = OID_NULL;  
    if(TOID_IS_NULL(a))  
        return FALSE;   
    q = (Queue*)malloc(sizeof(Queue));  
    if(q == NULL)  
        return FALSE;  
    memset(q->data, 0, QueueSize);  
    q->front = q->rear = 0;  
    q->size = 0;  
   
    if(!visited[i])  
    {  
        printf("node %d\t", D_RW(a)->adjlist[i].vertex);  
        visited[i] = TRUE;  
    }  
    j = D_RW(D_RW(a)->adjlist[i].firstedge)->adjvex;  
    e = D_RW(D_RW(a)->adjlist[i].firstedge)->next;  
    if(!visited[j])  
    {  
        enQueue(q, j);  
        while(!TOID_IS_NULL(e))  
        {         
            k = D_RW(e)->adjvex;  
            if(!visited[k])  
            {  
                enQueue(q, D_RW(e)->adjvex);  
                printf("node %d\t", D_RW(a)->adjlist[k].vertex);  
                visited[k] = TRUE;  
            }  
            e = D_RW(e)->next;  
        }  
    }  
    while(q->size != 0)  
    {  
        j = deQueue(q);  
        BFS(a, j);  
    }  
    return TRUE;
}  

bool BFSTraverseM(TOID(ALGraph) a)  
{  
    int i;  
    if(TOID_IS_NULL(a))  
        return FALSE;  
   	printf("BFS:");
    for(i = 0; i < D_RW(a)->n; i++)  
        visited[i] = FALSE;  
    for(i = 0; i < D_RW(a)->n; i++)  
        BFS(a, i);  
    printf("\n");
    return TRUE;  
}  

int main(int argc, const char * argv[]) 
{
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
        // t0 = wtime();
        pmemobj_ck_totol_initial(pop,PMEMOBJ_POOL);
        // t1 = wtime();
        // printf("initime = %lf\n",t1-t0 );
    }
    TX_BEGIN(pop){
        initALGraph();  
    }TX_END

    
    BFSTraverseM(a);  
    return 0;     
}  