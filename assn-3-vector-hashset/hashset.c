#include "hashset.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/** Implementation note: hashset
 * -----------------------------
 * hashset is a separate chaining implementation of a hash set. It can easily be converted to 
 * a hashmap by storing key/value pairs where the compare routines know how to access the 
 * the key portion of the pair
 */ 

void HashSetNew(hashset *h, int elemSize, int numBuckets,
		HashSetHashFunction hashfn, HashSetCompareFunction comparefn, HashSetFreeFunction freefn)
{
  h->chains = malloc(numBuckets * sizeof(vector)); 
  h->nChains = numBuckets; 
  h->chainSize = sizeof(vector); 
  h->hashFn = hashfn; 
  h->cmpFn = comparefn; 
  h->freeFn = freefn;
  h->logicalLen = 0; 
  
  for (int i = 0; i < numBuckets; i++) { 
    vector *target = (vector *)((char*)h->chains + i * sizeof(vector)); 
    VectorNew(target, elemSize, freefn, 10); 
  }
}

void HashSetDispose(hashset *h)
{
  if(h->freeFn != NULL) { 
    for (int i = 0; i < h->nChains; i++) { 
      vector *v = (vector*)((char*)h->chains + i * h->chainSize); 
      VectorDispose(v); 
    }
  }
  free(h->chains); 
}

int HashSetCount(const hashset *h)
{ 
  return h->logicalLen; 
}

void HashSetMap(hashset *h, HashSetMapFunction mapfn, void *auxData)
{
  for (int i = 0; i < h->nChains; i++) {
    vector *v = (vector*)((char*)h->chains + i * h->chainSize); 
    VectorMap(v, mapfn, auxData); 
  }
}

void HashSetEnter(hashset *h, const void *elemAddr)
{
  assert(elemAddr != NULL); 
  int bucketIndex = h->hashFn(elemAddr, h->nChains); 
  assert(bucketIndex >= 0 && bucketIndex < h->nChains); 
  
  vector *v = (vector*)((char*)h->chains + bucketIndex * h->chainSize);
  
  int vectorIndex = VectorSearch(v, elemAddr, h->cmpFn, 0, false); 
  
  if (vectorIndex == -1) {
    VectorAppend(v, elemAddr); 
    h->logicalLen++; 
  } else { 
    VectorReplace(v, elemAddr, vectorIndex); 
  }
}

void *HashSetLookup(const hashset *h, const void *elemAddr)
{ 
  assert(elemAddr != NULL);
  int bucketIndex = h->hashFn(elemAddr, h->nChains); 
  assert(bucketIndex >= 0 && bucketIndex < h->nChains); 
  
  vector *v = (vector*)((char*)h->chains + bucketIndex*h->chainSize); 
  
  int elemIndex = VectorSearch(v, elemAddr, h->cmpFn, 0, false); 

  if (elemIndex == -1) return NULL; 
  
  void *vp = VectorNth(v, elemIndex); 
  return vp;
}
