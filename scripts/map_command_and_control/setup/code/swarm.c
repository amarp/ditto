
#include "swarm.h"

//TODO dummy function replace with real thing in OLSR

int DEBUG        = 0;
int GRAPH_DEBUG  = 0;
int CONFIG_DEBUG = 0;
int TREE_DEBUG   = 0;
int TABLE_DEBUG  = 1;

int ONLY_MODEL=0;

#define DIJKSTRA 0x1
#define ITERATIVE 0x2

int TREE_TYPE = DIJKSTRA;
//int TREE_TYPE = ITERATIVE;

int **configs;
#define oops(s) { perror((s)); exit(EXIT_FAILURE); }
#define MALLOC(s,t) if(((s) = malloc(t)) == NULL) { oops("error: malloc() "); }
int *s1;
int *s2;
int *de;
double *t;
Graph *metric_graph;
Graph *thro_graph;
Graph *tree_graph;
Graph *subgraph;


#define ANY_DEST        0xffffffff
#define INT_SIZE 30000
int NUM_NODES;
int NUM_GATEWAYS;
int NUM_CONFIGS;
#define FALSE 0
#define TRUE 1

#define RATEKBPS 512

double get_time()
{
  return 10;
}

double
get_cur_time()
{
  struct timeval tv;

  gettimeofday(&tv, 0);
  return (1.0*(tv.tv_sec + tv.tv_usec/1000000.0));
}


//------------------------------LIST CODE USED FOR MANIPULATING LINK CACHE------------------

/*
 * List definitions.
 */
#define LIST_HEAD(name, type)						\
struct name {								\
	type *lh_first;	/* first element */			\
}

#define LIST_ENTRY(type)						\
struct {								\
	type *le_next;	/* next element */			\
	type **le_prev;	/* address of previous next element */	\
}

/*
 * List functions.
 */
#define	LIST_INIT(head) {						\
	(head)->lh_first = NULL;					\
}

#define LIST_INSERT_AFTER(listelm, elm, field) {			\
	if (((elm)->field.le_next = (listelm)->field.le_next) != NULL)	\
		(listelm)->field.le_next->field.le_prev =		\
		    &(elm)->field.le_next;				\
	(listelm)->field.le_next = (elm);				\
	(elm)->field.le_prev = &(listelm)->field.le_next;		\
}

#define LIST_INSERT_BEFORE(listelm, elm, field) {			\
	(elm)->field.le_prev = (listelm)->field.le_prev;		\
	(elm)->field.le_next = (listelm);				\
	*(listelm)->field.le_prev = (elm);				\
	(listelm)->field.le_prev = &(elm)->field.le_next;		\
}

#define LIST_INSERT_HEAD(head, elm, field) {				\
	if (((elm)->field.le_next = (head)->lh_first) != NULL)		\
		(head)->lh_first->field.le_prev = &(elm)->field.le_next;\
	(head)->lh_first = (elm);					\
	(elm)->field.le_prev = &(head)->lh_first;			\
}

#define LIST_REMOVE(elm, field) {					\
	if ((elm)->field.le_next != NULL)				\
		(elm)->field.le_next->field.le_prev = 			\
		    (elm)->field.le_prev;				\
	*(elm)->field.le_prev = (elm)->field.le_next;			\
}

//------------END OF LIST CODE-------------------------------------------------


//-------------------------------------------------------------------------
// FUNCTION: InitGraph
// PURPOSE: Initializes the network graph passed to it
// ARGUMENTS: Pointer to graph
// RETURN: None
//-------------------------------------------------------------------------

void init_graph(Graph *graph)
{
  int i=0;

  graph->lh = (LinkHead *) malloc((sizeof(LinkHead))*(MAX_NODES));
  for(i = 0; i < MAX_NODES; i++) {		
    LIST_INIT(&(graph->lh[i]));
    graph->lh[i].gateway = FALSE;
    graph->lh[i].valid = FALSE;
  }
  graph->d      = (double *) malloc((sizeof(double))*(MAX_NODES));
  graph->tree  = (unsigned int *) malloc((sizeof(unsigned int))*(MAX_NODES));
  graph->pi     = (unsigned int *) malloc((sizeof(unsigned int))*(MAX_NODES));
  graph->ch     = (int *) malloc((sizeof(unsigned int))*(MAX_NODES));
  graph->L     = (int *) malloc((sizeof(unsigned int))*(MAX_NODES));
  graph->T     = (double *) malloc((sizeof(unsigned int))*(MAX_NODES));
  graph->pi_outif     = (unsigned int *) malloc((sizeof(unsigned int))*(MAX_NODES));
  graph->pi_inif     = (unsigned int *) malloc((sizeof(unsigned int))*(MAX_NODES));
  graph->S      = (BOOL *) malloc((sizeof(BOOL))*(MAX_NODES));
  graph->dirty = -1;
}



//-------------------------------------------------------------------------
// FUNCTION: init_link
// PURPOSE: Initialize the link passed and updates the destination address
// ARGUMENTS: Destination Address, Link object
// RETURN: None
//-------------------------------------------------------------------------

void init_link(NodeAddress dest, Link *link)
{
	link->ln_dst  = dest;
	link->ln_time = get_time();
	link->flow    = 0;
}

//-------------------------------------------------------------------------
// FUNCTION: find_link
// PURPOSE: Finds a link in the graph
// ARGUMENTS: link from node, to node, from interface, to interface and graph pointer
// RETURN: Pointer to the link
//-------------------------------------------------------------------------

Link* find_link(NodeAddress from, 
	       NodeAddress tom, 
	       int fromif, 
	       int toif, 
	       Graph *graph)
{
	Link *l;

	//printf("finding link from %d to %d\n",from,tom);
	
	if(from > MAX_NODES || from < 0) return 0;

  	for(l = graph->lh[from].lh_first; l; l = l->ln_link.le_next) {
	  if(l->ln_dst == tom && l->outif == fromif && l->inif == toif) {
	    return l;
	  }
  	}
  	return 0;
}

//-------------------------------------------------------------------------
// FUNCTION: AddUniLink
// PURPOSE: Adds a link to the graph specified
// ARGUMENTS: link from node, to node, cost, routecache pointer, from interface, to interface, channel, node ptr
// RETURN: Whether a new link was inserted or old one updated
//-------------------------------------------------------------------------

int add_link(NodeAddress from, 
	     NodeAddress to,
	     double cost,
	     Graph *graph, 
	     int fromif, 
	     int toif, 
	     int channel)
{	
	Link *l;
	int rc = 0;

	//if(GRAPH_DEBUG) printf("adding link from %d(%d)->%d(%d) of weight %f\n",from,fromif,to,toif,cost);

	graph->lh[from].valid = TRUE;
	graph->lh[to].valid = TRUE;

	if((l = find_link(from,to,fromif,toif,graph)) == 0) {
		l = (Link *)malloc(sizeof(Link));
		assert(l);
   		init_link(to,l);
    		LIST_INSERT_HEAD(&(graph->lh[from]), l, ln_link);
		graph->dirty = CURRENT_TIME;
    		rc = 1;
		l->outif 		= fromif;
		l->inif   		= toif;
		l->channel 	= channel;
  	} else {   
   	 // we want to set flags
		graph->dirty = CURRENT_TIME;
	}

	assert(l->outif == fromif);
	assert(l->inif   == toif);
  
	l->ln_cost = cost;
	l->channel = channel;
	l->ln_time = get_time();
  	return rc;
} 

//-------------------------------------------------------------------------
// FUNCTION: del_link
// PURPOSE: Deletes a link in the graph
// ARGUMENTS: link from node, to node, from interface, to interface, graph pointer
// RETURN: whether operation was successful or not
//-------------------------------------------------------------------------

int del_link(NodeAddress from, 
	     NodeAddress tom, 
	     int fromif, 
	     int toif, 
	     Graph *graph)
{
  Link *l;
  int rc = 0;
  
  if((l = find_link(from, tom, fromif, toif, graph))) {
    LIST_REMOVE(l, ln_link);
    free(l);
    graph->dirty = CURRENT_TIME;
    rc = 1;
  }
  
  return rc;
}

//-------------------------------------------------------------------------
// FUNCTION: del_vertex
// PURPOSE: Deletes a vertex in the graph
// ARGUMENTS: link from node, to node, from interface, to interface, graph pointer
// RETURN: whether operation was successful or not
//-------------------------------------------------------------------------

void del_vertex(NodeAddress vertex, Graph *graph)
{
  Link *l, *temp;
  int removeall=FALSE;
  int v;

  if(DEBUG) {printf("removing vertrex %d\n",vertex);fflush(stdout);}
  

  //delete all links addressed to this node
  for(v = 1; v <= NUM_NODES; v++) { 
    if(v == vertex){
      removeall = TRUE;
      graph->lh[v].valid = FALSE;
    }
    else
      removeall = FALSE;
    
    l = graph->lh[v].lh_first; 
    while(l) {
      if(l->ln_dst == vertex || removeall == TRUE) {
	temp = l->ln_link.le_next;
	LIST_REMOVE(l, ln_link);
	free(l);
	l = temp;
      }
      else
	l = l->ln_link.le_next;
    }
  }
  
}


//-------------------------------------------------------------------------
// FUNCTION: reset_graph
// PURPOSE: resets the graph to pristine form
// ARGUMENTS: pointer to graph
// RETURN: 
//-------------------------------------------------------------------------

void reset_graph(Graph *graph)
{  
  Link *l;  	
  Link *temp;  	
  int v,w; 

  for(v = 1; v <= NUM_NODES; v++) {    	
    l = graph->lh[v].lh_first; 
    while(l) {
      temp = l->ln_link.le_next;
      LIST_REMOVE(l, ln_link);
      free(l);
      l = temp;
    }
  }
    
  for(v = 1; v < NUM_NODES; v++) {
      graph->d[v] = INFLEN;
      graph->pi[v] = NIL; // invalid node ID
      graph->ch[v] = 0;
      graph->L[v]=0;
      graph->T[v]=0;
      graph->pi_outif[v] = NIL; // invalid node ID
      graph->pi_inif[v] = NIL; // invalid node ID
      graph->S[v] = FALSE;
      graph->lh[v].gateway = FALSE;
      graph->lh[v].valid = FALSE;
  }  
}

//-------------------------------------------------------------------------
// FUNCTION: reset_graph
// PURPOSE: resets the graph to pristine form
// ARGUMENTS: pointer to graph
// RETURN: 
//-------------------------------------------------------------------------

void copy_graph(Graph *src_graph, Graph *dst_graph)
{  
  Link *l;  	
  int v; 

  for(v = 1; v <= NUM_NODES; v++) {
    for(l = src_graph->lh[v].lh_first; l; l = l->ln_link.le_next) {     
      add_link(v,l->ln_dst,l->ln_cost,dst_graph,0,0,0);
    }
    
    dst_graph->d[v] = src_graph->d[v];
    dst_graph->pi[v] = src_graph->pi[v]; // invalid node ID
    dst_graph->ch[v] = src_graph->ch[v];
    dst_graph->L[v]= src_graph->L[v];
    dst_graph->T[v]= src_graph->T[v];
    dst_graph->pi_outif[v] = src_graph->pi_outif[v]; // invalid node ID
    dst_graph->pi_inif[v] = src_graph->pi_inif[v]; // invalid node ID
    dst_graph->S[v] = src_graph->S[v];
    dst_graph->lh[v].gateway = src_graph->lh[v].gateway;
    dst_graph->lh[v].valid = src_graph->lh[v].valid;
  }  
}


//-------------------------------------------------------------------------
// FUNCTION: dump_graph
// PURPOSE: Prints out the graph 
// ARGUMENTS: pointer to graph
// RETURN: 
//-------------------------------------------------------------------------

void dump_graph(Graph *graph){  
  Link *l;  	
  int i;   
  for(i = 1; i <= NUM_NODES; i++) {    	
    printf("%d(%d) |",i,graph->lh[i].valid);    	
    for(l = graph->lh[i].lh_first; l; l = l->ln_link.le_next) {     
      printf("->%d",l->ln_dst);    	
    }//for    	
    printf("\n");  	
  }//for
}

//-------------------------------------------------------------------------
// FUNCTION: bidirectionalize_graph
// PURPOSE: adds reverse links to links in the graph
// ARGUMENTS: pointer to graph
// RETURN: 
//-------------------------------------------------------------------------

void bidirectionalize_graph(Graph *graph){  
  Link *l;  	
  int i;   
  for(i = 1; i <= NUM_NODES; i++) {    	
    for(l = graph->lh[i].lh_first; l; l = l->ln_link.le_next) {     
      add_link(l->ln_dst, i, l->ln_cost, graph, 0, 0, 0);
    }//for    	
  }//for
}

//-------------------------------------------------------------------------
// FUNCTION: dump_graph_with_metric
// PURPOSE: Prints out the graph with all information
// ARGUMENTS: pointer to graph
// RETURN: 
//-------------------------------------------------------------------------

void dump_graph_with_metric(Graph *graph)
{  
  Link *l;  	
  int i;   
  for(i = 1; i < MAX_NODES; i++) {    	
    printf("%d |",i);    	
    for(l = graph->lh[i].lh_first; l; l = l->ln_link.le_next) {     
      printf("->%d (Out %d:In %d) C%d (%f) F%d",l->ln_dst,l->outif, l->inif, l->channel, l->ln_cost,l->flow);    	
    }//for    	
    printf("\n");  	
  }//for
}

//-------------------------------------------------------------------------
// FUNCTION: dump_graph_buf
// PURPOSE: Print out basic information about the graph passed into a buffer
// ARGUMENTS: pointer to graph 
// RETURN: None
//-------------------------------------------------------------------------

void dump_graph_buf(Graph *graph, 
		    char *tbuf, 
		    int tbuf_size)
{
  Link *l;
  int i;
  unsigned int toff = 0;
  
  memset(&tbuf,0x0,tbuf_size);
  toff = strlen(tbuf);
  for(i = 0; i < MAX_NODES; i++) {
    for(l = graph->lh[i].lh_first; l; l = l->ln_link.le_next) {
      sprintf(tbuf+toff, "%d->%d, ", i, l->ln_dst);
      toff = strlen(tbuf);
      assert(toff < sizeof(tbuf));
    }
  }
}



//-------------------------------------------------------------------------
// FUNCTION: init_single_source
// PURPOSE: Initialize the source address so that Dijkstra can be run from the source address s 
// ARGUMENTS: source address, pointer to routecache, and size of routecache
// RETURN: None
//-------------------------------------------------------------------------

void init_single_source(NodeAddress s, Graph *graph, int size)
{	
  int v;
  Link *l;

  for(v = 0; v < size; v++) {
    graph->d[v] = INFLEN;
    //CHANGE
    graph->pi[v] = NIL; // invalid node ID
    graph->ch[v] = 0; // invalid node I
    graph->L[v] = 0; // invalid node ID
    graph->T[v] = 0; // invalid node ID
    graph->pi_outif[v] = NIL; // invalid node ID
    graph->pi_inif[v] = NIL; // invalid node ID
    graph->S[v] = FALSE;
  }

  //set all the flow values to 0
  for(v = 0; v < size; v++) {
    for(l = graph->lh[v].lh_first; l; l = l->ln_link.le_next) {     
      l->flow = 0;
    }//for    	
  }//for
  
  graph->d[s] = 0;
}

//-------------------------------------------------------------------------
// FUNCTION: relax
// PURPOSE: Performs the relaxation algorithm of Djikstra. Works differently for wcett v/s other metrics ETX/ETT
// ARGUMENTS: permanent node u, new node v, cost from u->v, out interface, in interface, source node, graph pointer, size of graph
// RETURN: None
//-------------------------------------------------------------------------

void relax(unsigned int u, 
	   unsigned int v, 
	   double w, 
	   int outif, 
	   int inif, 
	   NodeAddress source, 
	   Graph *graph, 
	   int size)
{
  Link *l;
  assert(graph->d[u] < INFLEN);
  
  if(graph->d[v] > graph->d[u] + w) {
    graph->d[v] = graph->d[u] + w;
    graph->pi[v] = u;
    graph->pi_outif[v] = outif;
    graph->pi_inif[v] = inif;
  }
}

//-------------------------------------------------------------------------
// FUNCTION: extract_min_q
// PURPOSE: Extract the minimum cost node which will now be made permanent
// ARGUMENTS: graph pointer, size of graph
// RETURN: node with lowest cost
//-------------------------------------------------------------------------

int extract_min_q(Graph *graph, 
		  int size)
{
  int u;
  //CHANGE
  int min_u = NIL;
  //CHANGE
  for(u = 0; u < size; u++) {
    if(graph->S[u] == FALSE && graph->d[u] < INFLEN && (min_u == NIL || graph->d[u] < graph->d[min_u])) {
      min_u = u;
    }
  }
  return min_u; // (min_u == 0) ==> no valid link
}



//-------------------------------------------------------------------------
// FUNCTION: Dijkstra
// PURPOSE: Performs Dijkstra's algorithm
// ARGUMENTS: source, graph pointer, destination, size of graph
// RETURN: None
//-------------------------------------------------------------------------

void dijkstra(NodeAddress source, 
	      Graph *graph, 
	      int size)
{
  int u;
  Link *v;
    
  init_single_source(source,graph,size);
  //CHANGE
  while((u = extract_min_q(graph,size)) != NIL) {
    graph->S[u] = TRUE;
    v = graph->lh[u].lh_first;
    for( ; v; v = v->ln_link.le_next) {
      if(v && graph->S[v->ln_dst]!= TRUE) { //if the link exists and has not been explored
	relax(u, v->ln_dst, v->ln_cost,v->outif,v->inif, source, graph, size);
      }
    }//for
  }//while
}

void make_tree_graph(Graph *g, 
		     NodeAddress srcAddr,
		     int size)
{
  NodeAddress i,v;
  Link *l;

  //if(DEBUG) printf("making tree graph for gateway is %d over \n",srcAddr+1);
  //dump_graph(g);

  //for all nodes except the srcNode find path backwards to srcAddr and add forward links to the tree graph  
  for(i=1;i<=NUM_NODES;i++){
    
    if(i == srcAddr+1){
      g->d[i] = 0;     
      continue;
    }

    for(v = i; g->d[v] < INFLEN; v = g->pi[v]) {    	
      //printf("inner loop\n");
      assert(v >= 0 && v <= size);

      if(v == srcAddr+1)	    
	break;	  
    	
      l = find_link(g->pi[v],v,g->pi_outif[v],g->pi_inif[v], metric_graph);
      //if(DEBUG) printf("finding link between %d->%d\n",g->pi[v],v);
      assert(l);
      add_link(g->pi[v], v, l->ln_cost, tree_graph, g->pi_outif[v], g->pi_inif[v], l->channel);      
    }
  }

  for(v=1;v<size;v++){
    tree_graph->pi[v] = g->pi[v];
    tree_graph->d[v] = g->d[v];
    tree_graph->pi_outif[v] = g->pi_outif[v];
    tree_graph->pi_inif[v] = g->pi_inif[v];
  }

}

int disconnected(Graph *g)
{
  int v;

  for(v=1;v<=NUM_NODES;v++){
    if(g->lh[v].valid == TRUE && g->d[v] == INFLEN){
      if(1) printf("graoh is disconnected at %d coz of  vali % or dv %d\n",v,g->lh[v].valid, g->d[v]);
      return TRUE;
    }
  }
  if(DEBUG) printf("****graoh is not disconnected****\n");
  return FALSE;
}



void mark_l_and_t(Graph *graph)
{
  int v;
  Link *l;

  for(v = 1; v <= NUM_NODES; v++) {
    double T=0.0;
    double L=0;
    for(l = graph->lh[v].lh_first; l; l = l->ln_link.le_next) {    
      //printf("came in to count links and flow is %d\n",l->flow);
      L+=l->flow;
      T+=l->flow*l->ln_cost;
    }//for  

    graph->L[v] = L;
    graph->T[v] = T;
    
    // printf("putting vertex %d l[v] as %d\n",v,graph->L[v]);
  }//for
  
}

void dump_l_and_t(Graph *graph)
{
  int v;

  for(v = 0; v < MAX_NODES; v++) {
    if(graph->L[v]>0)
      printf("%d | L=%d | T=%f\n",v,graph->L[v],graph->T[v]);
  }//for
  
}

BOOL check_lint(NodeAddress k, NodeAddress h, NodeAddress s, NodeAddress d)
{
  int v;
  Link *l;
  int interf=0;
  int links=0;

  for(v = 0; v < INT_SIZE; v++) {
    if(s1[v] == k && s2[v] == s && de[v] == h ) { //this is information about what happens when MR k transmits to h and s interferes. Now we check if s affects the links k*
      l=find_link(k,h,0,0,thro_graph);
      if(l) { //a links exists when there is no interference. see what happens to it with "s" transmitting
	double BIR = t[v]/l->ln_cost;
	if(BIR < 1.0) 
	  return TRUE;
	else
	  return FALSE;
      }
      else 
	assert(FALSE);
    }
  }

  return FALSE; //if no int file exists do this
  //assert(FALSE);
}

BOOL check_int(NodeAddress k, NodeAddress s, NodeAddress d)
{
  int v;
  Link *l;
  int interf=0;
  int links=0;

  // return TRUE;

  for(v = 0; v < INT_SIZE; v++) {
    if(s1[v] == k && s2[v] == s) { //this is information about what happens when MR k and s transmit together. Now we check if s affects the links k*
      int td = de[v];
      l=find_link(k,td,0,0,thro_graph);
      if(l) { //a links exists when there is no interference. see what happens to it with "s" transmitting
	double BIR = t[v]/l->ln_cost;
	if(l->ln_cost > 0){
	  if(BIR<0.75)
	    interf++;
	  links++;
	}
      }
    }
  }

  if((double)interf/links > 0.25){
    //if(DEBUG) printf("checking for interference between %d and %d and %d links exist and %d links become bad TRUE\n",k,s,links,interf);
    return TRUE;
  }
  else {
    //if(DEBUG) printf("checking for interference between %d and %d and %d links exist and %d links become bad FALSE\n",k,s,links,interf);
    return FALSE;
  }

}

double get_int(NodeAddress k,
	       Graph *graph)
{
  int v;
  double cost=0.0;
  Link *l;
   
  for(v = 1; v <= NUM_NODES; v++) {
    if(v == k) continue; 
    for(l = graph->lh[v].lh_first; l; l = l->ln_link.le_next) {     
      if(l->ln_dst == k) continue;
      
      //now check if the current link interferes with Node k and if so add it to the cost
      if(check_int(k,v,l->ln_dst))
	cost+=l->flow*pow(l->ln_cost,2);
      
    }
  }
  return cost;
}

double get_lint(NodeAddress k,
	       NodeAddress h,
	       Graph *graph)
{
  int v;
  double cost=0.0;
  Link *l;
   
  for(v = 1; v <= NUM_NODES; v++) {
    if(v == k) continue; 
    for(l = graph->lh[v].lh_first; l; l = l->ln_link.le_next) {     
      //now check if the current link interferes with Node k and if so add it to the cost
      //if(check_lint(k,h,v,l->ln_dst))
	cost+=check_lint(k,h,v,l->ln_dst)*l->flow*pow(l->ln_cost,2);
    }
  }
  return cost;
}

double get_big_term(NodeAddress k, 
		   Graph *graph)
{
  Link *l;
  double termvalue=0;

  //sigma over all h
  for(l = graph->lh[k].lh_first; l; l = l->ln_link.le_next) {     
    termvalue+=l->flow*get_lint(k,l->ln_dst,graph);
  }//for    	
  return termvalue;
}


double get_newb(NodeAddress k, 
	     Graph *graph)
{
  double b;
  
  b = 0.5*(graph->T[k] + sqrt(pow(graph->T[k],2) +2*get_big_term(k,graph))/*end off sqrt*/  );

  return b;
}

double get_b(NodeAddress k, 
	     Graph *graph)
{
  double b;
  
  b = 0.5*(graph->T[k] + sqrt(pow(graph->T[k],2) +2*graph->L[k]*get_int(k,graph))/* of sqrt*/  );

  return b;
}


int count_children(NodeAddress vertex, 
		   Graph *graph)
{
  Link *v;
  int count=0;

  v = graph->lh[vertex].lh_first;
  for( ; v; v = v->ln_link.le_next) {
    count+=1+count_children(v->ln_dst,graph);
  }
  //if(DEBUG) 
  //printf("children of %d are %d\n",vertex,count);
  graph->ch[vertex] = count;

  //now find the link from the parent of this vertex to this vertex and mark its flow value with Lij value
  //printf("finding link %d->%d\n",graph->pi[vertex],vertex);
  v = find_link(graph->pi[vertex],vertex,0,0,graph);
  if(v) {
    v->flow = count+1;
    //printf("adding flow %d to %-.%d\n",v->flow, graph->pi[vertex],vertex);
  }

  return count;
}

double model_tree(Graph *g, int gwid, int num_nodes_in_config)
{
  int i;
  double Tcycle = 0.0;
  double frac;
 
  //printf("modeling tree for gwid %d\n",gwid);
  //dump_graph(g);

  count_children(gwid+1,g);
  mark_l_and_t(g);
  if(DEBUG) dump_l_and_t(g);

  //now find max over all k Bk
  for(i=1;i<=NUM_NODES;i++) {
    double cost = get_b(i,g);
    //   printf("Tcycle cost if %f and 1/cost if %f\n",cost,1/cost);
    if(cost > Tcycle)
      Tcycle = cost;
    //Tcycle += cost;
  }
  
  //printf("Tcycle is %f and throughput is %f\n",Tcycle,NUM_NODES/Tcycle);
  //return Tcycle;
  frac = (double)((double)num_nodes_in_config/(NUM_NODES-NUM_GATEWAYS));
  //printf("frac is %f \n",frac);
  //fflush(stdout);
  return frac*Tcycle;
}


//uses the tree graph for temporary purposes
double model_performance(Graph *G, int new_node, int attach_point, int num_nodes_in_config, int gwid)
{
  Link *l = find_link(attach_point, new_node, 0, 0, metric_graph);
  double rank;

  if(l) { //this node can actually be attached in this manner
    add_link(attach_point, new_node,l->ln_cost, tree_graph, 0, 0, 0);
    tree_graph->pi[new_node]=attach_point;
    //printf("going to rank this tree gwid is %d-->\n",gwid);
    //dump_graph(tree_graph);
    rank = model_tree(tree_graph, gwid,  num_nodes_in_config);
    //printf("RANKKKK***is %f\n",rank);
    del_link(attach_point, new_node, 0, 0, tree_graph);
    tree_graph->pi[new_node]=NIL;
    return rank;
  }
  else
    return -1;
}

Graph *construct_tree(Graph *G, int gateway_id)
{
  
  if(TREE_TYPE == DIJKSTRA) {
    reset_graph(tree_graph);
    //printf("before dijk cost of %d is %f and 12 is %d\n",gateway_id+1,G->d[gateway_id+1], G->d[12]);
    dijkstra(gateway_id+1,G,NUM_NODES+1);
    //printf("after dijk cost of %d is %f and 12 is %d\n",gateway_id+1,G->d[gateway_id+1], G->d[12]);
    if(disconnected(G))
      return NULL;    
    make_tree_graph(G,gateway_id, NUM_NODES+1);
    
    if(TREE_DEBUG){
      printf("dijk tree made is -->\n");
      dump_graph(tree_graph);
    }
    
    return tree_graph;
  }
  else if (TREE_TYPE == ITERATIVE){     
    int Q[NUM_NODES+1];
    int i;       
    int num_nodes = 0;        
    int nodes_in_tree = 0;
    Link *l;
    
    //printf("constructing iterative tree on gateway %d\n",gateway_id+1);
    //printf("current network graph is -->\n");
    //dump_graph(metric_graph);

    reset_graph(tree_graph);

    for(i = 0; i <= NUM_NODES; i++) 
      Q[i] = 0;
    
    for(i = 1; i < MAX_NODES; i++) {    	
      if (G->lh[i].valid) {
	Q[i] = 1;
	num_nodes++;
      }
    }//for
    
    
    //add gateway first
    Q[gateway_id+1] = 0;
    num_nodes--;
    nodes_in_tree++;
    tree_graph->lh[gateway_id+1].valid = 1;
    
    while (num_nodes > 0) {
      
      //extract best node from all remaining nodes
      int attach_point = -1;
      double best_xput = -1;
      int best_node = -1;
      int disc = TRUE;

      for(i = 1; i <= NUM_NODES; i++) {
	int j;
	
	if (Q[i] == 0) continue;
	
	//remaining nodes
	//printf("trying to attach nodes %d\n",i);
	
	for(j = 1; j <= NUM_NODES; j++) {    	
	  if (tree_graph->lh[j].valid && i != j) {
	    double xput = model_performance(tree_graph, i, j, nodes_in_tree, gateway_id); //CHANGE
	    tree_graph->lh[i].valid = FALSE;
	    //printf("adding %d to %d perf is %f\n",i,j,xput);
	    if (xput > -1) 
	      disc = FALSE;
	    
	    if (xput > best_xput) {
	      best_node = i;
	      attach_point = j;
	      best_xput = xput;
	    }
	  }
	}//for j			
      }//for i
      
      if (disc) {
	  printf("Disconnected configuration\n");
	  return(NULL);
      }

      assert(best_node >= 0);
      num_nodes--;
      nodes_in_tree++;
      Q[best_node] = 0;
      tree_graph->lh[best_node].valid = 1;
      tree_graph->pi[best_node] = attach_point;
      //printf("attaching %d to %d--------------\n",best_node,attach_point);
      l = find_link(attach_point,best_node,0,0, metric_graph);
      assert(l);
      add_link(attach_point, best_node, l->ln_cost, tree_graph, 0,0,0);
    } //while

    if(TREE_DEBUG) {
      printf("iterative tree made is -->\n");
      dump_graph(tree_graph);
    }
    //exit(1);
    return tree_graph;
  }
  else
    {
      oops("TREE TYPE unknown\n");
    }
}





//-------------------------------------------------------------------------
// FUNCTION: get_route
// PURPOSE: Actually gets a route based on the ETT,ETX metric
// ARGUMENTS: src and destination for which route is needed, pointer to graph, pointer to node, pointer to route location to fill up and size of graph, flag specifies if we want route with the source in it or not
// RETURN: pointer to chosen route
//-------------------------------------------------------------------------

NodeAddress *
get_route(NodeAddress srcAddr, NodeAddress destAddr, Graph *graph, NodeAddress *route, int size){

  unsigned int v;  	
  NodeAddress rpath[MAX_SR_LEN];	
  NodeAddress outif[MAX_SR_LEN];	
  NodeAddress inif[MAX_SR_LEN];  	
  NodeAddress temproute[MAX_SR_LEN];  	
  unsigned int roff = 0;  	
  int z;  	
  Link *l;  	
  int for_me =1;  	
  NodeAddress last;	

  
  //Compute all of the shortest paths...	
  if(graph->dirty <= CURRENT_TIME) {    	
    dijkstra(srcAddr,graph,size);  	
  }
  
  //Trace backwards to figure out the path to DEST	
  for(v = destAddr; graph->d[v] < INFLEN; v = graph->pi[v]) {    	
    //CHANGE   		
    assert(v >= 0 && v <= size);    	
    if (roff >= MAX_SR_LEN) {     
      // path between us and dest is too long to be useful     
      break;    	
    }    
    
    rpath[roff] = v;		
    outif[roff]  = graph->pi_outif[v];		
    inif[roff]  = graph->pi_inif[v];	    
    roff++;	    
    
    if(v == srcAddr)	    
      break;	  
  }
  
  
  if(roff < 2 || graph->d[v] == INFLEN || roff >= MAX_SR_LEN) {   		
    return NULL; // no path  	
  }
  
  
  //Build the path that we need...
  //resetting the route  	
  for(z=0; z < MAX_SR_LEN;z++)    	
    {     
      temproute[z] = ANY_DEST;    	
    }
  
  //add yourself to the path    	
  temproute[0]= srcAddr;   
  for(v = 1; v < roff ; v++) {    	
    assert((int) (roff - v - 1) >= 0 && (roff - v - 1) < MAX_SR_LEN);    	
    l = find_link(temproute[v-1] & 0xff, rpath[roff - v - 1],outif[roff - v - 1],inif[roff - v - 1],graph); 	    	
    assert(l);    	
    //temproute[v] = rpath[roff-v-1];    	
    temproute[v] = l->ln_dst;  	
  }
  
  for(z=0; z < MAX_SR_LEN;z++)    	
    route[z] = temproute[z];    	

  return route;
  
}


//-------------------------------------------------------------------------
// FUNCTION: PrintRoute
// PURPOSE: Prints out a route
// ARGUMENTS: pointer to route
// RETURN: 
//-------------------------------------------------------------------------

void 
print_route(NodeAddress *route)
{	
	int i;  	
	printf("<");  	
	for (i =0; i< MAX_SR_LEN; i++)    	
	{     
 		if(route[i]!=ANY_DEST)			
			printf("%d-",route[i]);     
 		else	{
	  		printf(">\n");	  		
			break;		
		}	
    	}
}

//-------------------------------------------------------------------------
// FUNCTION: Length
// PURPOSE: return length of a route
// ARGUMENTS: pointer to route
// RETURN: length
//-------------------------------------------------------------------------

int length(NodeAddress *route){	
	int i;  	
	if  (route==NULL)    	
	return -1;  	
	for (i =0; i< MAX_SR_LEN; i++)    	
	{     
	 		if(route[i]==ANY_DEST)			
				return(i-1);    	
		}	
		return -1;

	}

int length_of_array(NodeAddress *route){  
	int i;  	
	if (route==NULL)    	
	return -1;  	
	for (i =0; i< MAX_SR_LEN; i++)    	
	{     
 		if(route[i]==ANY_DEST)			
			return(i);    	
	}	
	return -1;
}

void reset_route(NodeAddress *route){  
	int i;  	
	for (i =0; i< MAX_SR_LEN; i++)    	
	{     
 		route[i]=ANY_DEST;    	
	}
}

void copy_route(NodeAddress *src, NodeAddress *dest){  
	int i;  	
	for (i =0; i< MAX_SR_LEN; i++)    	
	{     
 		dest[i] = src[i];    	
	}
}

void read_conflict_file(char *filename)
{
  char buf[1024];
  char tmp[1024];
  struct token_struct{
    char buffer[100];
  };
  struct token_struct token[100];
  char *temp;
  int count = 0;
  FILE *fp;
  int z;
  int i;
  i = 0;

  fp = fopen(filename, "r");

  while (fgets(buf, 1024, fp) != NULL) 
    {
      for(z=0; z<100;z++)
	bzero(&token[z].buffer,sizeof(token[z].buffer));
	  
      strcpy(tmp,buf);
      count =0;
      temp = strtok(buf,"\t ");
	  
      while(temp != NULL)
	{
	  bzero(&token[count].buffer,sizeof(token[count].buffer));
	  strcpy(token[count].buffer, temp);
	  //printf("%d:%s ",count,token[count].buffer);
	  count++;
	  temp = strtok(NULL,"\t ");
	}
      //printf("\n");

      if(atoi(token[0].buffer) == -1 || atoi(token[0].buffer) > NUM_NODES || atoi(token[2].buffer) > NUM_NODES ||  atoi(token[0].buffer) ==  atoi(token[2].buffer) )
	continue; //this is a comment line

      if(atoi(token[1].buffer) == -1){
	if(DEBUG) printf("Throughput of %d->%d = %.2fKbps\n",atoi(token[0].buffer),atoi(token[2].buffer),atof(token[3].buffer)/1000);
	if(DEBUG) fflush(stdout);
	add_link(atoi(token[0].buffer),atoi(token[2].buffer),atof(token[3].buffer),thro_graph,0,0,0);
	
      }
      else{
	s1[i]=atoi(token[0].buffer);
	s2[i]=atoi(token[1].buffer);
	de[i]=atoi(token[2].buffer);
	t[i]=atof(token[3].buffer);
	i++;
	if(i>INT_SIZE){
	  printf("size of interference array is too small\n");
	  exit(1);
	}
      }

    }
}

void read_metric_file(char *filename)
{
  char buf[1024];
  char tmp[1024];
  struct token_struct{
    char buffer[100];
  };
  struct token_struct token[100];
  char *temp;
  int count = 0;
  FILE *fp;
  int z;

  fp = fopen(filename, "r");

  while (fgets(buf, 1024, fp) != NULL) 
    {
      for(z=0; z<100;z++)
	bzero(&token[z].buffer,sizeof(token[z].buffer));
	  
      strcpy(tmp,buf);
      count =0;
      temp = strtok(buf,"\t ");
	  
      while(temp != NULL)
	{
	  bzero(&token[count].buffer,sizeof(token[count].buffer));
	  strcpy(token[count].buffer, temp);
	  //printf("%d:%s ",count,token[count].buffer);
	  count++;
	  temp = strtok(NULL,"\t ");
	}
      //printf("\n");

      if(strcmp(token[0].buffer,"GRAPHENTRY") == 0){
	//printf("Metric of %d->%d = %f\n",atoi(token[1].buffer),atoi(token[2].buffer),atof(token[3].buffer));
	//if(atof(token[3].buffer) < 0.0025)
	  add_link(atoi(token[1].buffer),atoi(token[2].buffer),atof(token[3].buffer),metric_graph,0,0,0);
      }
    }

  printf("read metric file...network graph is -->\n");
  dump_graph(metric_graph);

}

void read_config_file(char *filename)
{
  char buf[1024];
  char tmp[1024];
  struct token_struct{
    char buffer[100];
  };
  struct token_struct token[100];
  char *temp;
  int count = 0;
  FILE *fp;
  int z,i;

  fp = fopen(filename, "r");

  while (fgets(buf, 1024, fp) != NULL) 
    {
      int config_num;
      
      for(z=0; z<100;z++)
	bzero(&token[z].buffer,sizeof(token[z].buffer));
	  
      strcpy(tmp,buf);
      count =0;
      temp = strtok(buf,"\t ");
	  
      while(temp != NULL)
	{
	  bzero(&token[count].buffer,sizeof(token[count].buffer));
	  strcpy(token[count].buffer, temp);
	  //printf("%d:%s ",count,token[count].buffer);
	  count++;
	  temp = strtok(NULL,"\t ");
	}
      //printf("\n");

      config_num = atoi(token[0].buffer);
      for(z=1; z<=(NUM_NODES-NUM_GATEWAYS);z++)
	{
	  int curr_value = atoi(token[z].buffer);
	  configs[config_num][z-1] = curr_value;
	}
    }


  if(CONFIG_DEBUG) 
    {
      for(z=0;z<NUM_CONFIGS;z++)
	{
	  printf("Config %d [",z);
	  for(i=0;i<(NUM_NODES-NUM_GATEWAYS);i++)
	    {
	      printf("%d",configs[z][i]);
	    }
	  printf("]\n");
	}
    }  
}



void write_qualnet_configuration(char* prefix)
{
  FILE *fp;
  int i;
  char filename[512];

  memset(&filename,0x0,sizeof(filename));
  strcpy(filename,prefix);
  strcat(filename,".config");

  fp = fopen(filename,"w");

  fprintf(fp,"VERSION 3.9.5\n");
  fprintf(fp,"EXPERIMENT-NAME %s\n",prefix);
  fprintf(fp,"SIMULATION-TIME   100S\n");
  fprintf(fp,"SEED   1\n");
  fprintf(fp,"COORDINATE-SYSTEM    CARTESIAN\n");
  fprintf(fp,"TERRAIN-DIMENSIONS   (800, 800)\n");
  fprintf(fp,"SUBNET N8-0.0.0.0 {1 thru 12}\n");
  fprintf(fp,"NODE-PLACEMENT        UNIFORM\n");
  fprintf(fp,"APP-CONFIG-FILE       %s.app\n",prefix);
  fprintf(fp,"IP-QUEUE-SCHEDULER   STRICT-PRIORITY\n");
  fprintf(fp,"MOBILITY   NONE\n");
  fprintf(fp,"MOBILITY-POSITION-GRANULARITY   1.0\n");            
  fprintf(fp,"PROPAGATION-CHANNEL-FREQUENCY 5.18e9\n");
  fprintf(fp,"PROPAGATION-LIMIT   -111.0\n");
  fprintf(fp,"PROPAGATION-PATHLOSS-MODEL  TWO-RAY\n");
  fprintf(fp,"PROPAGATION-SHADOWING-MODEL CONSTANT\n");
  fprintf(fp,"PROPAGATION-SHADOWING-MEAN 4.0\n");
  //fprintf(fp,"PROPAGATION-FADING-MODEL NONE\n");
  fprintf(fp,"PROPAGATION-FADING-MODEL RAYLEIGH\n");
  fprintf(fp,"PROPAGATION-FADING-MAX-VELOCITY 2\n");
  fprintf(fp,"PROPAGATION-FADING-GAUSSIAN-COMPONENTS-FILE ./default.fading\n");
  fprintf(fp,"PHY-MODEL                   PHY802.11a\n");
  fprintf(fp,"PHY-LISTENABLE-CHANNEL-MASK 1\n");
  fprintf(fp,"PHY-LISTENING-CHANNEL-MASK  1\n");
  fprintf(fp,"PHY-TEMPERATURE             290\n");
  fprintf(fp,"PHY-NOISE-FACTOR            7.0\n");
  fprintf(fp,"PHY-RX-MODEL                PHY802.11a\n");
  fprintf(fp,"PHY802.11-AUTO-RATE-FALLBACK YES\n");
  fprintf(fp,"PHY802.11-DATA-RATE                6000000\n");
  fprintf(fp,"PHY802.11-DATA-RATE-FOR-BROADCAST  6000000\n");
  fprintf(fp,"PHY802.11a-TX-POWER--6MBPS  20.0\n");
  fprintf(fp,"PHY802.11a-TX-POWER--9MBPS  20.0\n");
  fprintf(fp,"PHY802.11a-TX-POWER-12MBPS  19.0\n");
  fprintf(fp,"PHY802.11a-TX-POWER-18MBPS  19.0\n");
  fprintf(fp,"PHY802.11a-TX-POWER-24MBPS  18.0\n");
  fprintf(fp,"PHY802.11a-TX-POWER-36MBPS  18.0\n");
  fprintf(fp,"PHY802.11a-TX-POWER-48MBPS  16.0\n");
  fprintf(fp,"PHY802.11a-TX-POWER-54MBPS  16.0\n");
  fprintf(fp,"PHY802.11b-TX-POWER--1MBPS  15.0\n");
  fprintf(fp,"PHY802.11b-TX-POWER--2MBPS  15.0\n");
  fprintf(fp,"PHY802.11b-TX-POWER--6MBPS  15.0\n");
  fprintf(fp,"PHY802.11b-TX-POWER-11MBPS  15.0\n");
  fprintf(fp,"PHY802.11a-RX-SENSITIVITY--6MBPS  -85.0\n");
  fprintf(fp,"PHY802.11a-RX-SENSITIVITY--9MBPS  -85.0\n");
  fprintf(fp,"PHY802.11a-RX-SENSITIVITY-12MBPS  -83.0\n");
  fprintf(fp,"PHY802.11a-RX-SENSITIVITY-18MBPS  -83.0\n");
  fprintf(fp,"PHY802.11a-RX-SENSITIVITY-24MBPS  -78.0\n");
  fprintf(fp,"PHY802.11a-RX-SENSITIVITY-32MBPS  -78.0\n");
  fprintf(fp,"PHY802.11a-RX-SENSITIVITY-48MBPS  -69.0\n");
  fprintf(fp,"PHY802.11a-RX-SENSITIVITY-54MBPS  -69.0\n");            
  fprintf(fp,"ANTENNA-GAIN             0.0\n");
  fprintf(fp,"ANTENNA-EFFICIENCY       0.8\n");
  fprintf(fp,"ANTENNA-MISMATCH-LOSS    0.3\n");
  fprintf(fp,"ANTENNA-CABLE-LOSS       0.0\n");
  fprintf(fp,"ANTENNA-CONNECTION-LOSS  0.2\n");
  fprintf(fp,"ANTENNA-HEIGHT  1.5\n");
  fprintf(fp,"ANTENNA-MODEL   OMNIDIRECTIONAL\n");
  fprintf(fp,"MAC-PROTOCOL   MAC802.11\n");
  fprintf(fp,"PROMISCUOUS-MODE   YES\n");
  fprintf(fp,"NETWORK-PROTOCOL   IP\n");
  fprintf(fp,"IP-QUEUE-NUM-PRIORITIES   3\n");
  fprintf(fp,"IP-QUEUE-PRIORITY-QUEUE-SIZE   50000000\n");
  fprintf(fp,"IP-QUEUE-TYPE   FIFO\n");
  fprintf(fp,"IP-FORWARDING YES\n");
  fprintf(fp,"ROUTING-PROTOCOL   LQSR\n");
  fprintf(fp,"LQSR-METRIC   STATIC\n");
  fprintf(fp,"STATIC-TREE-FILE %s.routes\n",prefix);
  fprintf(fp,"SCHEDULER-QUEUE-TYPE            SPLAYTREE\n");
  fprintf(fp,"APPLICATION-STATISTICS                  YES\n");
  fprintf(fp,"TCP-STATISTICS                          YES\n");
  fprintf(fp,"UDP-STATISTICS                          YES\n");
  fprintf(fp,"RSVP-STATISTICS                         NO\n");
  fprintf(fp,"ROUTING-STATISTICS                      YES\n");
  fprintf(fp,"ACCESS-LIST-STATISTICS                  NO\n");
  fprintf(fp,"ROUTE-REDISTRIBUTION-STATISTICS         NO\n");
  fprintf(fp,"IGMP-STATISTICS                         NO\n");
  fprintf(fp,"EXTERIOR-GATEWAY-PROTOCOL-STATISTICS    YES\n");
  fprintf(fp,"NETWORK-LAYER-STATISTICS                YES\n");
  fprintf(fp,"DIFFSERV-EDGE-ROUTER-STATISTICS         NO\n");
  fprintf(fp,"QUEUE-STATISTICS                        YES\n");
  fprintf(fp,"MAC-LAYER-STATISTICS                    YES\n");
  fprintf(fp,"PHY-LAYER-STATISTICS                    YES\n");
  fprintf(fp,"MOBILITY-STATISTICS                     NO\n");
  fclose(fp);
}

void write_traffic_file(char* prefix, int gwid)
{
  double rate = (RATEKBPS*1000)/(8*1460);
  double INTERVAL = 1 / rate;
  int i;
  FILE *fp;
  char filename[512];
  
  memset(&filename,0x0,sizeof(filename));
  strcpy(filename,prefix);
  strcat(filename,".app");
  
  fp = fopen(filename,"w");

  for(i=1;i<=NUM_NODES;i++) 
    {
     
      if(i==gwid) continue;
 
      //this node belongs to the current gateway so write out a traffic flow from the gateway
      //CBR <src> <dest> <items_to_send> <item_size> <interval> <start time> <end time>
      fprintf(fp,"CBR %d %d 0 1460 %fS 0S 90S\n",gwid,i,INTERVAL);
      // FTP/GENERIC <src> <dest> <items to send> <item size> <start time>  <end time>
      //fprintf(fp,"FTP/GENERIC %d %d 0 1460 0S 90S\n",gwid,i);
    }

  fclose(fp);
}



void rank_configuration(int gwid, FILE *rankfp)
{
  int i;
  double Tcost=0.0;
  double Tcycle = 0.0;
  double rank=0.0;
  Graph *tree;
  //find Tcost of the tree using the metric_graph  
  //dijkstra(gwid,metric_graph,NUM_NODES+1);
  //reset_graph(tree_graph);
  //make_tree_graph(metric_graph,gwid-1, NUM_NODES+1);

  construct_tree(metric_graph,gwid-1);
  rank = model_tree(tree_graph,gwid-1,12);

  //count_children(gwid,tree_graph);
  //  mark_l_and_t(tree_graph);
  //  if(DEBUG) dump_l_and_t(tree_graph);
  //  if(DEBUG) dump_graph(tree_graph);

  //now find max over all k Bk
  //  for(i=1;i<=NUM_NODES;i++) {
  //    double cost = get_b(i,tree_graph);
  //    //   printf("Tcycle cost if %f and 1/cost if %f\n",cost,1/cost);
  //    if(cost > Tcycle)
  //      Tcycle = cost;
  //  }

  printf("Config %d is %f\n",gwid,rank);
}


//return positive number of nodes in configuration or return 0
int construct_graph_for_a_subconfiguration(Graph *g, int config_num, int gwid)
{
  int gwindex,i;
  int num_nodes_in_config=0;

  if (DEBUG) printf("Config %d Making tree for gw %d which is index %d\n",config_num,gwid+1,(gwid-(NUM_NODES-NUM_GATEWAYS)));

  reset_graph(subgraph); //make the subgraph clean
  copy_graph(g,subgraph); //replicate the original network graph

  gwindex = gwid - (NUM_NODES - NUM_GATEWAYS);

  for (i = 0; i < (NUM_NODES - NUM_GATEWAYS); i++)
    {
      if (configs[config_num][i] != gwindex)
	del_vertex(i+1,subgraph); //convert index in config array to actual nodeid
      else
	num_nodes_in_config++;
    }

  if(DEBUG) printf("came here after del vertex\n");

  //now remove the other gateways from the graph as well
  for (i = NUM_NODES - NUM_GATEWAYS; i < NUM_NODES; i++)
    {
      if (i != gwid)
	{
	  del_vertex(i+1,subgraph);
	}
    }

  if(DEBUG) printf("After removal graph is\n");
  if(DEBUG) dump_graph(subgraph);
	    
  return num_nodes_in_config;
}

void print_graph_to_file(Graph *tree, FILE *fp)
{
  Link *l;  	
  int i;   
  for(i = 1; i <= NUM_NODES; i++) {    	
    fprintf(fp,"%d |",i);    	
    for(l = tree->lh[i].lh_first; l; l = l->ln_link.le_next) {     
      fprintf(fp,"->%d",l->ln_dst);    	
    }//for    	
    fprintf(fp,"\n");  	
  }//for
}

void print_membership(Graph *tree, FILE *fp, int channel, int gw)
{
  Link *l;  	
  int i;   
  for(i = 1; i <= NUM_NODES; i++) {    	
    if(tree->lh[i].valid){

      if(i > (NUM_NODES-NUM_GATEWAYS) && i!=gw) continue;

      fprintf(fp,"%d %d %d\n",i,gw,channel);
    }//if    	
  }//for
}

void print_routing_tables(Graph *tree, FILE *fp, int gw)
{
  int i,j;
  NodeAddress temproute[MAX_SR_LEN];
  NodeAddress *route;
  
  if(TABLE_DEBUG) printf("came to routing tables printing\n");
  if(TABLE_DEBUG) { printf("before bidir graph is\n"); dump_graph(tree);}
  bidirectionalize_graph(tree);
  if(TABLE_DEBUG) { printf("after bidir graph is\n"); dump_graph(tree);}


  for(i=1;i<=NUM_NODES;i++){
    for(j=1;j<=NUM_NODES;j++)
      {
	
	reset_route(temproute);
	
	
	if(i == j) continue;
	if(tree->lh[i].valid == FALSE || tree->lh[j].valid == FALSE) continue;
	if(i > (NUM_NODES-NUM_GATEWAYS) && i!= gw) continue;
	if(j > (NUM_NODES-NUM_GATEWAYS) && j!= gw) continue;
	
	printf("finding route from %d to %d\n",i,j);
	route = get_route(i, j, tree, temproute, NUM_NODES+1);
	
	//write this out as a configuration with tree, app, file etc
	printf("Route from %d->%d is: ",i,j);
	print_route(route);
	fprintf(fp,"%d %d %d\n",i,j,route[1]);
      }
  }
  
}

//either returns less than 0 is pruned or returns the rank
void print_configuration(Graph *g, int config_num)
{
  int i,j;
  int ret_val;
  double rank=0.0;
  Graph *tree;
  FILE *treeout = fopen("TREE-OUTPUT","w");
  FILE *tableout = fopen("TABLE-OUTPUT","w");
  FILE *memberout = fopen("MEMBER-OUTPUT","w");
  int channel=1;

  printf("Printing configuration --> %d\n",config_num);
  
   //construct tree for each gateway
  for (i = NUM_NODES-NUM_GATEWAYS; i < NUM_NODES; i++)
    {
      printf("Constructing subgraph for gateway %d\n",i);    
      ret_val = construct_graph_for_a_subconfiguration(g, config_num, i);
      printf("Created configuration with %d nodes\n",ret_val);      
      tree = construct_tree(subgraph,i);
      if(tree == NULL){
	printf("Cannot print configuration %d...something is wrong\n");
      	return;
      }
      print_graph_to_file(tree,treeout);
      print_routing_tables(tree,tableout,i+1);
      print_membership(tree,memberout,channel,i+1);
      channel+=5;
    }
  fclose(memberout);
  fclose(treeout);
  fclose(tableout);
  return;
}


//either returns less than 0 is pruned or returns the rank
double execute_configuration(Graph *g, int config_num)
{
  int i,j;
  int ret_val;
  double rank=0.0;
  Graph *tree;
  
  if (1)
    {
      printf("Executing configuration %d --> ",config_num);
      for (j = 0; j < (NUM_NODES-NUM_GATEWAYS); j++)
	{
	  printf("%d",configs[config_num][j]);
	}
      printf("\n");
    }

   //construct tree for each gateway
  for (i = NUM_NODES-NUM_GATEWAYS; i < NUM_NODES; i++)
    {
      //if(DEBUG) printf("Constructing tree for gateway %d\n",i);                               
      ret_val = construct_graph_for_a_subconfiguration(g, config_num, i);
      //if(DEBUG) printf("Created configuration with %d nodes\n",ret_val);
      
      tree = construct_tree(subgraph,i);
      if(tree == NULL){
	
      	return FALSE;
      }
      //rank += (double)(ret_val/NUM_NODES)*model_tree(tree,i,ret_val);
      rank += model_tree(tree,i,ret_val);
    }
  
  printf("config %d rank %f\n",config_num,rank);
  //if we reached here. It means that all the subconfigurations for each gateway were valid and we need to model this configuration    
  return rank;
}

//returns the index of the best configuration
int perform_exhaustive_search(Graph *g)
{
  int i;
  int num_real_configs = 0;
  double ret_val;
  int num_valid=0;
  int num_disc=0;
  double best_rank = 1000000;
  int best_config = -1;



  //iterate through all possible configurations
  for (i = 0; i < NUM_CONFIGS; i++)
    {
      if(DEBUG)printf("-----------------executing config %d---------------\n",i);
      ret_val = execute_configuration(g, i);
      //printf("config %d return %f\n",i,ret_val);
      if(ret_val > 0.0) {
	if(ret_val < best_rank){ //lower the better?
	  best_config = i;
	  best_rank = ret_val;
	}
	num_valid++;
      }
      else
	num_disc++;
      
    }
  printf("num valid %d num disc %d\n",num_valid, num_disc);
  return best_config;
}


void generate_configurations()
{
  int i,j;
  int gateway;
  Link *l;
  FILE *scriptfp;
  FILE *rankfp; 
  FILE *modelfp;

  if(!ONLY_MODEL) {
    scriptfp = fopen("crun.sh","w");
    rankfp = fopen("rank.txt","w");
    modelfp = fopen("model.txt","w");
  }

  for(i=1;i<=NUM_NODES;i++)
    {
      FILE *fp;
      char filename[512];
      char prefix[512];

      if(1) {
      
      if(!ONLY_MODEL) {
	memset(&filename,0x0,sizeof(filename));
	memset(&prefix,0x0,sizeof(prefix));
	sprintf(prefix,"c%d",i);
	strcpy(filename,prefix);
	strcat(filename,".routes");

	fp=fopen(filename,"w");
      }

      gateway=i;
      

      rank_configuration(gateway,rankfp);
      
      if(!ONLY_MODEL) {
	for(j = 1; j < MAX_NODES; j++) {    		
	  for(l = tree_graph->lh[j].lh_first; l; l = l->ln_link.le_next) {     
	    fprintf(fp,"%d %d 0 0 %f\n",j,l->ln_dst, l->ln_cost);    	
	  }//for    	
	}//for
	fclose(fp);

	write_traffic_file(prefix,gateway);
	write_qualnet_configuration(prefix);
	fprintf(scriptfp,"./qualnet.exe c%d.config\n",i);
      }

      }
    }
  
  if(!ONLY_MODEL) {
    fclose(scriptfp);
    fclose(rankfp);
    fclose(modelfp);
  }
}



/*
  for(j=1;j<=NUM_NODES;j++)
  {
  NodeAddress temproute[MAX_SR_LEN];
  reset_route(temproute);
  NodeAddress *route;
  
  if(gateway == j) continue;
  
  route = get_route(gateway, j, metric_graph, temproute, NUM_NODES+1);
  
  //write this out as a configuration with tree, app, file etc
  printf("Route from %d->%d is: ",gateway,j);
  print_route(route);
  //source destination 0 0 metric
  }
*/
    


void usage()
{
  printf("./swarm <conflict information file> <metric information file> <configs> <NUM NODES> <NUM_GATEWAYS> <NUM_CONFIGS>\n");
  exit(1);
}

int main(int argc,char *argv[])
{
  double before;
  double after;
  int i;
  int best_config;
  s1 = (int *)malloc(sizeof(int)*INT_SIZE); // 1st source
  s2 = (int *)malloc(sizeof(int)*INT_SIZE); // 2nd source (interferer)
  de = (int *)malloc(sizeof(int)*INT_SIZE);  // destination
  t = (double *)malloc(sizeof(double)*INT_SIZE);  // throughput under this scenario

  //  printf("Hello world\n");

  if(argc != 7 )
    {
      usage();
    }

  NUM_NODES = atoi(argv[4]);
  NUM_GATEWAYS = atoi(argv[5]);
  NUM_CONFIGS = atoi(argv[6]);

  MALLOC(configs, sizeof(int *) * NUM_CONFIGS);
  for (i = 0; i < NUM_CONFIGS; i++) {
    MALLOC(configs[i], sizeof(int) * (NUM_NODES - NUM_GATEWAYS));
  }
  metric_graph = (Graph *)malloc(sizeof(Graph));
  thro_graph = (Graph *)malloc(sizeof(Graph));
  tree_graph = (Graph *)malloc(sizeof(Graph));
  subgraph = (Graph *)malloc(sizeof(Graph));

  init_graph(subgraph);
  init_graph(metric_graph);
  init_graph(thro_graph);
  init_graph(tree_graph);
  read_conflict_file(argv[1]);
  read_metric_file(argv[2]);
  read_config_file(argv[3]);
  
  before = get_cur_time();
  best_config = perform_exhaustive_search(metric_graph);
  //generate_configurations();
  after=get_cur_time();
  printf("modeling took %f seconds\n",after-before);
  printf("Best configuration found is %d\n",best_config);
  print_configuration(metric_graph,best_config);
}



void convert(int from, int to, char *s)
{
  int length = strlen(s);
  int i;
  int il = length;
  int fs[il];
  int k = 0;
  int ol = il * (from / to + 1);
  int ts[ol+10]; //assign accumulation array
  int cums[ol+10]; //assign the result array
  int j;
  char sout[128]; //initialise output string
  int index;

  //Return error if input is empty
  if (s == NULL)
    {
      printf("Error: Nothing in Input String");
      exit(1);
    }

  printf("length of input string is %d\n",length);

  //only do base 2 to base 36 (digit represented by charecaters 0-Z)"
  if (from < 2 || from > 36 || to < 2 || to > 36) 
    { 
      printf("Base requested outside range"); 
      exit(1);
    }
  
  //convert string to an array of integer digits representing number in base:from
  for (i = length - 1; i >= 0; i--)
    {
      if (s[i] >= '0' && s[i] <= '9') { fs[k++] = (int)(s[i] - '0'); }
      else
	{
	  if (s[i] >= 'A' && s[i] <= 'Z') { fs[k++] = 10 + (int)(s[i] - 'A'); }
	  else
	    { 
	      printf("Error: Input string must only contain any of 0-9 or A-Z"); 
	      exit(1);
	    } //only allow 0-9 A-Z characters
	}
    }

  //check the input for digits that exceed the allowable for base:from
  for(i=0; i < il; i++)
    {
      if (fs[i] >= from) 
	{ 
	  printf("Error: Not a valid number for this input base"); 
	  exit(1);
	}
      else
	printf("%d\n",fs[i]);
    }
  //find how many digits the output needs
  
  ts[0] = 1; //initialise array with number 1 

  //evaluate the output
  for (i = 0; i < il; i++) //for each input digit
    {
      for (j = 0; j < ol; j++) //add the input digit times (base:to from^i) to the output cumulator
	{
	  int temp = cums[j];
	  int rem = 0;
	  int ip = j;

	  cums[j] += ts[j] * fs[i];
	  temp = cums[j];
	  rem = 0;
	  ip = j;
	  do // fix up any remainders in base:to
	    {
	      rem = temp / to;
	      cums[ip] = temp - rem * to; ip++;
	      cums[ip] += rem;
	      temp = cums[ip];
	    }
	  while (temp >= to);
	}
      //calculate the next power from^i) in base:to format
      for (j = 0; j < ol; j++)
	{
	  ts[j] = ts[j] * from;
	}
      for (j = 0; j < ol; j++) //check for any remainders
	{
	  int temp = ts[j];
	  int rem = 0;
	  int ip = j;
	  do  //fix up any remainders
	    {
	      rem = temp / to;
	      ts[ip] = temp - rem * to; ip++;
	      ts[ip] += rem;
	      temp = ts[ip];
	    }
	  while (temp >= to);
	}
    }
  //convert the output to string format (digits 0,to-1 converted to 0-Z characters) 
  
  BOOL first = 0; //leading zero flag
  index=0;
  for (i = ol; i >= 0; i--)
    {
      if (cums[i] != 0) { first = 1; }
      if (!first) { continue; }
      if (cums[i] < 10)
	{
	  sout[index] = (char)(cums[i] + '0');
	  printf("%d\n",sout[index]);
	  index++;
	}
      else
	{
	  sout[index]= (char)(cums[i] + 'A' - 10);
	  printf("%d\n",sout[index]);
	  index++;
	}

    }
  //if (String.IsNullOrEmpty(sout)) { return "0"; } //input was zero, return 0
  //return the converted string
  printf("The final output is %s\n",sout);
}

