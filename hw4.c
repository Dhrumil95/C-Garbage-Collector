#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct memory_region{
  size_t * start;
  size_t * end;
};

struct memory_region global_mem;
struct memory_region heap_mem;
struct memory_region stack_mem;

void walk_region_and_mark(void* start, void* end);

//how many ptrs into the heap we have
#define INDEX_SIZE 1000
void* heapindex[INDEX_SIZE];


//grabbing the address and size of the global memory region from proc 
void init_global_range(){
  char file[100];
  char * line=NULL;
  size_t n=0;
  size_t read_bytes=0;
  size_t start, end;

  sprintf(file, "/proc/%d/maps", getpid());
  FILE * mapfile  = fopen(file, "r");
  if (mapfile==NULL){
    perror("opening maps file failed\n");
    exit(-1);
  }

  int counter=0;
  while ((read_bytes = getline(&line, &n, mapfile)) != -1) {
    if (strstr(line, "hw4")!=NULL){
      ++counter;
      if (counter==3){
        sscanf(line, "%lx-%lx", &start, &end);
        global_mem.start=(size_t*)start;
        // with a regular address space, our globals spill over into the heap
        global_mem.end=malloc(256);
        free(global_mem.end);
      }
    }
    else if (read_bytes > 0 && counter==3) {
      if(strstr(line,"heap")==NULL) {
        // with a randomized address space, our globals spill over into an unnamed segment directly following the globals
        sscanf(line, "%lx-%lx", &start, &end);
        printf("found an extra segment, ending at %zx\n",end);						
        global_mem.end=(size_t*)end;
      }
      break;
    }
  }
  fclose(mapfile);
}


//marking related operations

int is_marked(size_t* chunk) {
  return ((*chunk) & 0x2) > 0;
}

void mark(size_t* chunk) {
  (*chunk)|=0x2;
}

void clear_mark(size_t* chunk) {
  (*chunk)&=(~0x2);
}

// chunk related operations

#define chunk_size(c)  ((*((size_t*)c))& ~(size_t)3 ) 
void* next_chunk(void* c) { 
  if(chunk_size(c) == 0) {
    printf("Panic, chunk is of zero size.\n");
  }
  if((c+chunk_size(c)) < sbrk(0))
    return ((void*)c+chunk_size(c));
  else 
    return 0;
}
int in_use(void *c) { 
  return (next_chunk(c) && ((*(size_t*)next_chunk(c)) & 1));
}


// index related operations

#define IND_INTERVAL ((sbrk(0) - (void*)(heap_mem.start - 1)) / INDEX_SIZE)
void build_heap_index() {
  // TODO
}

// the actual collection code
void sweep() {
 
 for ( size_t* currChunk = heap_mem.start - 1; currChunk && currChunk < (size_t*)sbrk(0); ) {
   if( !(is_marked(currChunk)) && in_use(currChunk) ) {	
       size_t *nxtChunk = next_chunk(currChunk);
       free(currChunk + 1);
       currChunk = nxtChunk;
    } else {	
       clear_mark(((size_t *)currChunk));           
       currChunk = next_chunk(currChunk);
    }
  }
} 


//determine if what "looks" like a pointer actually points to a block in the heap
size_t * is_pointer(size_t * ptr) {
  //printf("is_pointer : %p \n", ptr);
  //printf( "heap start: %p heap end: %p \n ", heap_mem.start, heap_mem.end);
 
  if ( ptr == NULL ||  ptr < heap_mem.start || ptr >= heap_mem.end ) return NULL;

  size_t* currChunk = heap_mem.start;
  size_t* lstChunk = heap_mem.end;
  while (currChunk < lstChunk) {
   //printf("\n Looping: %p \n",currChunk);
   size_t* currHead = currChunk - 1;
   size_t* nxtChunk = (size_t*)next_chunk(currHead);
   //printf("currHead:  %p \n",currHead);
   //printf("nxtChunk:  %p \n",nxtChunk);
   if ( in_use(currHead) && currChunk <= ptr && ptr < (nxtChunk + 1) ) return currHead;
    currChunk = nxtChunk + 1;
  }

 return NULL;

} 


void markChunk (size_t* currChunk ) {
  
  if (currChunk == NULL) return;
  if (is_marked(currChunk)) return;

  mark(currChunk);
  
  size_t* currVal = currChunk + 1;
  //printf( " length: %lu \n",chunk_size(currChunk));
      for (int i = 0; i < chunk_size(currChunk); i++) {
        size_t* nxtChunk = is_pointer((size_t*)currVal[i]);
        //printf(" next_chunk: %p  \n",next_chunk);
        if (nxtChunk && !is_marked(nxtChunk)) //printf(" nxt_chunk: %p \n",next_chunk);
           markChunk(nxtChunk);
        else break;
    }
}


void walk_region_and_mark(void* start, void* end) {
 //printf( "  start: %p end: %p \n ",start, end);
 size_t* currMem = start;
 size_t* endMem = end;
 while ( currMem < endMem ) {
   //printf("  loop: %p \n",currMem);
   size_t* currChunk = is_pointer((size_t*)*currMem);
   //printf(" currChunk: %p \n ", currChunk); 
   if(currChunk && (currMem < heap_mem.start || currMem > heap_mem.end)) 
           markChunk(currChunk);
  
   currMem++;
 }

}

// standard initialization 
void init_gc() {
  size_t stack_var;
  init_global_range();
  heap_mem.start=malloc(512);
  //since the heap grows down, the end is found first
  stack_mem.end=(size_t *)&stack_var;
}

void gc() {
  size_t stack_var;
  heap_mem.end=sbrk(0);
  //grows down, so start is a lower address
  stack_mem.start=(size_t *)&stack_var;

  // build the index that makes determining valid ptrs easier
  // implementing this smart function for collecting garbage can get bonus;
  // if you can't figure it out, just comment out this function.
  // walk_region_and_mark and sweep are enough for this project.
  //build_heap_index();

  //walk memory regions
  walk_region_and_mark(global_mem.start,global_mem.end);
  walk_region_and_mark(stack_mem.start,stack_mem.end);

  sweep();
}
