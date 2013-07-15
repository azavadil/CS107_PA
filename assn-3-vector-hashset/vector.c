#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static const int defaultAllocation = 4; 

static void ShuffleForward(vector *v, int position); 
static void ShuffleBack(vector *v, int position);
static void VectorGrow(vector *v);

void VectorNew(vector *v, int elemSize, VectorFreeFunction freeFn, int initialAllocation)
{
  // don't allow initial allocation of 0
  if(initialAllocation == 0) initialAllocation = defaultAllocation; 
  
  assert(elemSize > 0 ); 
  v->elems = malloc(elemSize * initialAllocation); 
  assert(v->elems != NULL) 
  v->elemSize = elemSize; 
  v->logicalLen = 0; 
  v->allocLen = initialAllocation; 
  v->freeFn = freeFn; 
}

void VectorDispose(vector *v)
{
  if (v->freeFn != NULL) { 
    for (int i = 0; i < v->logicalLen; i++) { 
      v->freeFn((char*)v->elems + i*v->elemSize); 
    }
  }
  free(v->elems);
}

int VectorLength(const vector *v)
{ return v->logicalLen; }

void *VectorNth(const vector *v, int position)
{ 
  assert(position >= 0 && position < v->logicalLen); 
  void *returnPtr = (char*)v->elems + position*v->elemSize; 
  return returnPtr; 
}

/** Implementation note: VectorReplace
 * -----------------------------------
 * We're note passing back a pointer to the existing element to the 
 * client so our vector needs to take care of freeing the memory
 */ 

void VectorReplace(vector *v, const void *elemAddr, int position)
{
  assert(position >= 0 && position < v->logicalLen); 
  void *target = (char*)v->elems + position*v->elemSize; 
  
  // free old elem
  if (v->freefn != NULL) {
    v->freefn(target); 
  }
  
  memcpy(target, elemAddr, v->elemSize); 
}

void VectorInsert(vector *v, const void *elemAddr, int position)
{
  assert(position >= 0 && position <= v->logicalLen);
  if(v->logicalLen == v->allocLen){ 
    VectorGrow(v); 
  }

  ShuffleBack(v, position); 
  
  void *target = (char*)v->elems + position*v->elemSize; 
  memcpy(target, elemAddr, v->elemSize); 
  v->logicalLen++; 
}

void VectorAppend(vector *v, const void *elemAddr)
{
  if(v->logicalLen == v->allocLen){ 
    VectorGrow(v); 
  }

  void *target = (char*)v->elems + v->logicalLen * v->elemSize; 
  memcpy(target, elemAddr, v->elemSize); 
  v->logicalLen++;  
}

void VectorDelete(vector *v, int position)
{
  assert(position >= 0 && position < v->logicalLen); 
  
  if (v->freeFn != NULL) { 
    void *target = (char*)v->elems + position*v->elemSize; 
    v->freeFn(target); 
  }
  ShuffleForward(v, position); 
  v->logicalLen--; 
}


void VectorSort(vector *v, VectorCompareFunction compare)
{
  assert( compare != NULL); 
  qsort(v->elems, v->logicalLen, v->elemSize, compare); 

}

void VectorMap(vector *v, VectorMapFunction mapFn, void *auxData)
{
  for (int i = 0; i < v->logicalLen; i++){ 
    void *currVal = VectorNth(v, i); 
    mapFn(currVal, auxData); 
  }
}

/** Private member function: VectorGrow
 * ------------------------------------
 * Vector grow doubles the current capacity of the vector's internal 
 * storage copying all the existing values
 */ 

static void VectorGrow(vector *v)
{ 
  v->allocLen *= 2; 
  v->elems = realloc(v->elems, v->allocLen * v->elemSize); 
  assert(v->elems != NULL); 
}

/** Private member function: ShuffleBack
 * -------------------------------------
 * ShuffleBack shifts each element back one step in memory. 
 * ShuffleBack is used in concert with VectorInsert.  
 */ 

static void ShuffleBack(vector *v, int position)
{
  for (int i = v->logicalLen - 1; i >= position; i--) {
    void *fromLoc = (char*)v->elems + i*v->elemSize; 
    void *destination = (char*)v->elems + (i+1)*v->elemSize; 
    memcpy(destination, fromLoc, v->elemSize); 
  } 
}

/** Private member function: ShuffleForward
 * -------------------------------------
 * ShuffleForward shifts each element forward one step in memory. 
 * ShuffleForward is used in concert with VectorDelete. 
 */ 


static void ShuffleForward(vector *v, int position)
{
  for(int i = position; i < v->logicalLen; i++){ 
    void *fromLoc = (char*)v->elems + (i+1)*v->elemSize; 
    void *destination = (char*)v->elems + (i)*v->elemSize; 
    memcpy(destination, fromLoc, v->elemSize); 
  }
}



static const int kNotFound = -1;

int VectorSearch(const vector *v, const void *key, VectorCompareFunction searchFn, int startIndex, bool isSorted)
{ 
  if (isSorted) { 
    const void *startSearch = (char*)v->elems + startIndex*v->elemSize;
    int nMembers = v->logicalLen - startIndex; 

    void *found = bsearch(key, startSearch, nMembers, v->elemSize, searchFn); 
    
    if (found == NULL) return -1; 
    
    int idx = ((char*)found - (char*)v->elems)/v->elemSize; 
    return idx;
  }

  else  { 
    void *startSearch = (char*)v->elems + startIndex*v->elemSize; 
    int nMembers = v->logicalLen - startIndex;
    void *found = lfind(key, startSearch, &nMembers, v->elemSize, searchFn); 
    
    if (found == NULL) return -1; 
    
    int idx = ((char*)found - (char*)v->elems)/v->elemSize; 
    return idx; 
  }
} 
