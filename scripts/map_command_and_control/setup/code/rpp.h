#include <sys/time.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <string.h>

//----------------------SYSTEM CONFIGURATION PARAMETERS---------------

#define MAX_NODES 100 //maximum number of nodes in the mesh network..need to have this dynamic later on

typedef int NodeAddress;
typedef unsigned int clocktype;
typedef unsigned int BOOL;

typedef struct  link_struct{  
  double      	ln_cost;             // cost of using the link  
  NodeAddress   ln_dst;    
  clocktype 	ln_time;	     //time entry was created
  int	        outif;               // Outgoing interface on source node.
  int 		inif;                // Incoming interface on target node.
  int 		channel;             //channel on which this link is operating
  int           flow;                // amount of flow on this link (after dijkstra is done)
  //Pathak change
  int tcost; //traffic cost through the link
  //end

  struct {								    
    struct  link_struct *le_next;	// next element 	
    struct  link_struct **le_prev;	// address of previous next element  
  } ln_link;
}Link;

typedef struct{							       
  Link *lh_first;	// first element 		
  int gateway;
  int valid;
}LinkHead;

typedef struct
{
  LinkHead *lh;
  double dirty; 
  unsigned int *tree;
  double *d;
  int *ch;                  //stores number of children of this node
  int *L;
  double *T;
  unsigned int *pi;
  unsigned int *pi_outif;
  unsigned int *pi_inif;
  BOOL *S;
}Graph;

//contains information pertaining to this node
typedef struct{
  NodeAddress node_id;
}Node;

#define CURRENT_TIME 1
#define NIL 10000
#define INFLEN 0x7fffffff
#define POSINF 999999

#define MAX_SR_LEN 25  //size of a maximum source route


void print_route(NodeAddress *route);
void reset_graph(Graph *graph);

/*
void LqsrFurtherInit(Node *node);
void LqsrInitRouteCache(LqsrRouteCache *routeCache, Node *node);
void LqsrInitLink(NodeAddress dest, LQSR_Link *link);
int LqsrAddLink(NodeAddress from, NodeAddress tom,double cost, LqsrRouteCache *routeCache);
int LqsrAddUniLink(NodeAddress from, NodeAddress tom,double cost, LqsrRouteCache *routeCache, int fromif, int toif, int channel, Node *node);
LQSR_Link* LqsrFindLink(NodeAddress from, NodeAddress tom, int fromif, int toif, LqsrRouteCache *routeCache);
int LqsrDelLink(NodeAddress from, NodeAddress tom, int fromif, int toif, LqsrRouteCache *routeCache);
void LqsrPurgeLink(LqsrRouteCache *routeCache,  Node *node);
void LqsrPurgeGraph(LqsrRouteCache *routeCache, Node *node);
void LqsrDumpLink(LqsrRouteCache *routeCache, Node *node);
void LqsrInit_Single_Source(NodeAddress s, LqsrRouteCache *routeCache, Node *node,int size);
void LqsrRelax(unsigned int u, unsigned int v, double w, int outif, int inif, NodeAddress source,  LqsrRouteCache *routeCache,Node *node,int size);
int LqsrExtract_Min_Q(LqsrRouteCache *routeCache, Node *node,int size);
void LqsrDijkstra(NodeAddress source, LqsrRouteCache *routeCache, NodeAddress destAddr, Node *node,int size);
void LqsrDump_Djikstra(NodeAddress dst, LqsrRouteCache *routeCache, Node *node);
NodeAddress *LqsrGetRoute(NodeAddress srcAddr, NodeAddress destAddr, LqsrRouteCache *routeCache, Node *node, NodeAddress *route, int size, int flag);
void LqsrPrintRoute(NodeAddress *route);
int LqsrLength(NodeAddress *route);
int LqsrLengthOfArray(NodeAddress *route);
void LqsrResetRoute(NodeAddress *route);
void LqsrCopyRoute(NodeAddress *src, NodeAddress *dest);
void LqsrDumpCache(LqsrRouteCache *routeCache, Node *node, int size);
void LqsrDumpCacheWithMetric(LqsrRouteCache *routeCache, Node *node, int size);
*/

void convert(int from, int to, char *s);
