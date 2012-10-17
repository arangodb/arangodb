////////////////////////////////////////////////////////////////////////////////
/// @brief hash array implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Martin Schoenert
/// @author Copyright 2006-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <BasicsC/hashes.h>
#include "HashIndex/hasharray.h"
#include "HashIndex/compare.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief size of a cache line, in bytes
/// the memory acquired for the hash table is aligned to a multiple of this
/// value
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HashArray
/// @{
////////////////////////////////////////////////////////////////////////////////

#define CACHE_LINE_SIZE 64

////////////////////////////////////////////////////////////////////////////////
/// @brief initial preallocation size of the hash table when the table is
/// first created
/// setting this to a high value will waste memory but reduce the number of
/// reallocations/repositionings necessary when the table grows
////////////////////////////////////////////////////////////////////////////////

#define INITIAL_SIZE    256

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HashArray
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new element
////////////////////////////////////////////////////////////////////////////////

static void AddNewElement (TRI_hasharray_t* array, void* element) {
  uint64_t hash;
  uint64_t i;

  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = IndexStaticHashElement(array, element);

  // ...........................................................................
  // search the table
  // ...........................................................................
  
  i = hash % array->_nrAlloc;

  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
#ifdef TRI_INTERNAL_STATS    
    array->_nrProbesR++;
#endif
  }

  // ...........................................................................
  // add a new element to the associative array
  // memcpy ok here since are simply moving array items internally
  // ...........................................................................

  memcpy(array->_table + i * array->_elementSize, element, array->_elementSize);
  array->_nrUsed++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate memory for the hash table
////////////////////////////////////////////////////////////////////////////////
  
static bool AllocateTable (TRI_hasharray_t* array, size_t numElements) {
  char* data;
  char* table;
  size_t offset;

  data = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, CACHE_LINE_SIZE + (array->_elementSize * numElements), true);
  if (data == NULL) {
    return false;
  }

  // position array directly on a cache line boundary
  offset = ((uint64_t) data) % CACHE_LINE_SIZE;

  if (offset == 0) {
    // we're already on a cache line boundary
    table = data;
  }
  else {
    // move to start of a cache line
    table = data + (CACHE_LINE_SIZE - offset);
  }
  assert(((uint64_t) table) % CACHE_LINE_SIZE == 0);

  array->_data = data;
  array->_table = table;
  array->_nrAlloc = numElements;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static bool ResizeHashArray (TRI_hasharray_t* array) {
  char* oldData;
  char* oldTable;
  uint64_t oldAlloc;
  uint64_t j;

  oldData = array->_data;
  oldTable = array->_table;
  oldAlloc = array->_nrAlloc;
 
  if (! AllocateTable(array, 2 * array->_nrAlloc + 1)) {
    return false;
  }

  array->_nrUsed = 0;
#ifdef TRI_INTERNAL_STATS
  array->_nrResizes++;
#endif

  for (j = 0; j < oldAlloc; j++) {
    if (! IndexStaticIsEmptyElement(array, oldTable + j * array->_elementSize)) {
      AddNewElement(array, oldTable + j * array->_elementSize);
    }
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, oldData);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static bool ResizeHashArrayMulti (TRI_hasharray_t* array) {
  return ResizeHashArray(array);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HashArray
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InitHashArray (TRI_hasharray_t* array,
                        size_t initialDocumentCount,
                        size_t numFields,
                        size_t elementSize,
                        uint64_t (*hashKey) (TRI_hasharray_t*, void*),
                        uint64_t (*hashElement) (TRI_hasharray_t*, void*),
                        void (*clearElement) (TRI_hasharray_t*, void*),
                        bool (*isEmptyElement) (TRI_hasharray_t*, void*),
                        bool (*isEqualKeyElement) (TRI_hasharray_t*, void*, void*),
                        bool (*isEqualElementElement) (TRI_hasharray_t*, void*, void*)) {

  size_t initialSize;

  // ...........................................................................
  // Assign the callback functions
  // ...........................................................................

  assert(numFields > 0);
  
  array->clearElement          = clearElement;
  array->isEmptyElement        = isEmptyElement;
  array->isEqualKeyElement     = isEqualKeyElement;
  array->isEqualElementElement = isEqualElementElement;

  array->_numFields = numFields;
  array->_elementSize = elementSize;
  array->_table = NULL;

  if (initialDocumentCount > 0) {
    // use initial document count provided as initial size
    initialSize = (size_t) (2.5 * initialDocumentCount);
  }
  else {
    initialSize = INITIAL_SIZE;
  }

 
  if (! AllocateTable(array, initialSize)) {
    return false;
  }
   
  // ...........................................................................
  // allocate storage for the hash array
  // ...........................................................................
  
  array->_nrUsed = 0;

#ifdef TRI_INTERNAL_STATS    
  array->_nrFinds = 0;
  array->_nrAdds = 0;
  array->_nrRems = 0;
  array->_nrResizes = 0;
  array->_nrProbesF = 0;
  array->_nrProbesA = 0;
  array->_nrProbesD = 0;
  array->_nrProbesR = 0;
#endif
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyHashArray (TRI_hasharray_t* array) {
  if (array == NULL) {
    return;
  }  
  
  // ...........................................................................
  // Go through each item in the array and remove any internal allocated memory
  // ...........................................................................
  
  // array->_table might be NULL if array initialisation fails
  if (array->_table != NULL) {
    char* p;
    char* e;

    p = array->_table;
    e = p + array->_elementSize * array->_nrAlloc;
    for (;  p < e;  p += array->_elementSize) {
      IndexStaticDestroyElement(array, p);
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, array->_data);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHashArray (TRI_hasharray_t* array) {
  if (array != NULL) {
    TRI_DestroyHashArray(array);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, array);
  }    
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeyHashArray (TRI_hasharray_t* array, void* key) {
  uint64_t hash;
  uint64_t i;

  // ...........................................................................
  // compute the hash
  // ...........................................................................
  
  hash = IndexStaticHashKey(array, key);
  i = hash % array->_nrAlloc;

  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS    
  array->_nrFinds++;
#endif
  
  // ...........................................................................
  // search the table
  // ...........................................................................
  
  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize) &&
         ! IndexStaticIsEqualKeyElement(array, key, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
#ifdef TRI_INTERNAL_STATS    
    array->_nrProbesF++;
#endif
  }


  // ...........................................................................
  // return whatever we found
  // ...........................................................................
  
  return array->_table + (i * array->_elementSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, return NULL if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_FindByKeyHashArray (TRI_hasharray_t* array, void* key) {
  void* element;

  element = TRI_LookupByKeyHashArray(array, key);

  if (! IndexStaticIsEmptyElement(array, element) && IndexStaticIsEqualKeyElement(array, key, element)) {
    return element;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementHashArray (TRI_hasharray_t* array, void* element) {
  uint64_t hash;
  uint64_t i;

  // ...........................................................................
  // compute the hash
  // ...........................................................................
  
  hash = IndexStaticHashElement(array, element);
  i = hash % array->_nrAlloc;

  
  // ...........................................................................
  // update statistics
  // ...........................................................................
  
#ifdef TRI_INTERNAL_STATS    
  array->_nrFinds++;
#endif

  
  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize) &&
         ! IndexStaticIsEqualElementElement(array, element, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
#ifdef TRI_INTERNAL_STATS    
    array->_nrProbesF++;
#endif
  }
  
  // ...........................................................................
  // return whatever we found
  // ...........................................................................

  return (array->_table) + (i * array->_elementSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given an element, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_FindByElementHashArray (TRI_hasharray_t* array, void* element) {
  void* element2;

  element2 = TRI_LookupByElementHashArray(array, element);

  if (! IndexStaticIsEmptyElement(array, element2) && IndexStaticIsEqualElementElement(array, element2, element)) {
    return element2;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertElementHashArray (TRI_hasharray_t* array, void* element, bool overwrite) {
  uint64_t hash;
  uint64_t i;
  bool found;
  void* arrayElement;
  
  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = IndexStaticHashElement(array, element);
  i = hash % array->_nrAlloc;

  
  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS    
  array->_nrAdds++;
#endif

  
  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize) &&
         ! IndexStaticIsEqualElementElement(array, element, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
#ifdef TRI_INTERNAL_STATS    
    array->_nrProbesA++;
#endif
  }
  
  arrayElement = array->_table + (i * array->_elementSize);
  
  
  // ...........................................................................
  // if we found an element, return
  // ...........................................................................

  found = !IndexStaticIsEmptyElement(array, arrayElement); 
  if (found) {
    if (overwrite) {
      // destroy the underlying element since we are going to stomp on top if it
      IndexStaticDestroyElement(array, arrayElement);
      IndexStaticCopyElementElement(array, arrayElement, element);
    }
    return false;
  }
  
  // ...........................................................................
  // add a new element to the hash array (existing item is empty so no need to
  // destroy it)
  // ...........................................................................

  if (! IndexStaticCopyElementElement(array, arrayElement, element)) {
    return false;
  }
  
  array->_nrUsed++;

  
  // ...........................................................................
  // we are adding and the table is more than half full, extend it
  // ...........................................................................
  
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    if (! ResizeHashArray(array) ) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertKeyHashArray (TRI_hasharray_t* array, void* key, void* element, bool overwrite) {
  uint64_t hash;
  uint64_t i;
  bool found;
  void* arrayElement;
  
  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = IndexStaticHashKey(array, key);
  i = hash % array->_nrAlloc;

  
  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS    
  array->_nrAdds++;
#endif

    
  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize) &&
         ! IndexStaticIsEqualKeyElement(array, key, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
#ifdef TRI_INTERNAL_STATS    
    array->_nrProbesA++;
#endif
  }

  arrayElement = array->_table + (i * array->_elementSize);
  
  
  // ...........................................................................
  // if we found an element, return
  // ...........................................................................

  
  found = ! IndexStaticIsEmptyElement(array, arrayElement);
  if (found) {
    if (overwrite) {
      // destroy the underlying element since we are going to stomp on top if it
      IndexStaticDestroyElement(array, arrayElement);
      IndexStaticCopyElementElement(array, arrayElement,  element);
    }
    return false;
  }


  // ...........................................................................
  // add a new element to the associative array
  // ...........................................................................

  if (! IndexStaticCopyElementElement(array, arrayElement,  element)) {
    return false;
  }
  
  array->_nrUsed++;

  
  // ...........................................................................
  // we are adding and the table is more than half full, extend it
  // ...........................................................................
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeHashArray(array);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveElementHashArray (TRI_hasharray_t* array, void* element) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  bool found;
  void* arrayElement;  
  
  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = IndexStaticHashElement(array, element);
  i = hash % array->_nrAlloc;

  
  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS    
  array->_nrRems++;
#endif


  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize) &&
         ! IndexStaticIsEqualElementElement(array, element, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
#ifdef TRI_INTERNAL_STATS    
    array->_nrProbesD++;
#endif
  }

  arrayElement = array->_table + (i * array->_elementSize);

  
  // ...........................................................................
  // if we did not find such an item return false
  // ...........................................................................
  
  found = ! IndexStaticIsEmptyElement(array, arrayElement);
  
  if (!found) {
    return false;
  }

  
  // ...........................................................................
  // remove item - destroy any internal memory associated with the element structure
  // ...........................................................................
  
  IndexStaticDestroyElement(array, arrayElement);
  array->_nrUsed--;


  // ...........................................................................
  // and now check the following places for items to move closer together 
  // so that there are no gaps in the array
  // ...........................................................................
  
  k = (i + 1) % array->_nrAlloc;

  while (! IndexStaticIsEmptyElement(array, array->_table + k * array->_elementSize)) {
    uint64_t j = IndexStaticHashElement(array, array->_table + k * array->_elementSize) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      // memcpy ok here since we are moving the items of the array around internally
      memcpy(array->_table + i * array->_elementSize, array->_table + k * array->_elementSize, array->_elementSize);
      IndexStaticClearElement(array, array->_table + k * array->_elementSize);
      i = k;
    }
    k = (k + 1) % array->_nrAlloc;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveKeyHashArray (TRI_hasharray_t* array, void* key) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  bool found;
  void* arrayElement;  

  // ...........................................................................
  // compute the hash
  // ...........................................................................
  
  hash = IndexStaticHashKey(array, key);
  i = hash % array->_nrAlloc;


  // ...........................................................................
  // update statistics
  // ...........................................................................
  
#ifdef TRI_INTERNAL_STATS    
  array->_nrRems++;
#endif


  // ...........................................................................
  // search the table
  // ...........................................................................
  
  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize) &&
         ! IndexStaticIsEqualKeyElement(array, key, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
#ifdef TRI_INTERNAL_STATS    
    array->_nrProbesD++;
#endif
  }

  arrayElement = array->_table + (i * array->_elementSize);

  
  // ...........................................................................
  // if we did not find such an item return false
  // ...........................................................................

  found = !IndexStaticIsEmptyElement(array, arrayElement);
  
  if (!found) {
    return false;
  }


  // ...........................................................................
  // remove item
  // ...........................................................................

  IndexStaticDestroyElement(array, arrayElement);
  array->_nrUsed--;



  // ...........................................................................
  // and now check the following places for items to move here
  // ...........................................................................

  k = (i + 1) % array->_nrAlloc;

  while (! IndexStaticIsEmptyElement(array, array->_table + k * array->_elementSize)) {
    uint64_t j = IndexStaticHashElement(array, array->_table + k * array->_elementSize) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      // memcpy ok here since only moving items along the array internally
      memcpy(array->_table + i * array->_elementSize, array->_table + k * array->_elementSize, array->_elementSize);
      IndexStaticClearElement(array, array->_table + k * array->_elementSize);
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }


  // ...........................................................................
  // return success
  // ...........................................................................
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// --SECTION--                                                 HASH ARRAY MULTI
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HashArray
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByKeyHashArrayMulti (TRI_hasharray_t* array, void* key) {
  TRI_vector_pointer_t result;
  uint64_t hash;
  uint64_t i;

  
  // ...........................................................................
  // initialise the vector which will hold the result if any
  // ...........................................................................
 
  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);
   
   
  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = IndexStaticHashKey(array, key);
  i = hash % array->_nrAlloc;

  
  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS    
  array->_nrFinds++;
#endif

  
  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize)) {
  
    if (IndexStaticIsEqualKeyElementMulti(array, key, array->_table + i * array->_elementSize)) {
      TRI_PushBackVectorPointer(&result, array->_table + i * array->_elementSize);             
    }
#ifdef TRI_INTERNAL_STATS    
    else {
      array->_nrProbesF++;
    }      
#endif

    i = (i + 1) % array->_nrAlloc;
  }

  
  // ...........................................................................
  // return whatever we found -- which could be an empty vector list if nothing
  // matches.
  // ...........................................................................
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupByElementHashArrayMulti (TRI_hasharray_t* array, void* element) {
  TRI_vector_pointer_t result;
  uint64_t hash;
  uint64_t i;

  
  // ...........................................................................
  // initialise the vector which will hold the result if any
  // ...........................................................................
 
  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);
   
   
  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = IndexStaticHashElement(array, element);
  i = hash % array->_nrAlloc;

  
  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS    
  array->_nrFinds++;
#endif

  
  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize)) {
  
    if (IndexStaticIsEqualElementElementMulti(array, element, array->_table + i * array->_elementSize)) {
      TRI_PushBackVectorPointer(&result, array->_table + i * array->_elementSize);             
    }
#ifdef TRI_INTERNAL_STATS    
    else {
      array->_nrProbesF++;
    }      
#endif

    i = (i + 1) % array->_nrAlloc;
  }

  // ...........................................................................
  // return whatever we found -- which could be an empty vector list if nothing
  // matches. Note that we allow multiple elements (compare with pointer impl).
  // ...........................................................................
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertElementHashArrayMulti (TRI_hasharray_t* array, void* element, bool overwrite) {

  uint64_t hash;
  uint64_t i;
  bool found;
  void* arrayElement;
  
  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = IndexStaticHashElement(array, element);
  i = hash % array->_nrAlloc;

  
  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS    
  array->_nrAdds++;
#endif

  
  
  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize) &&
         ! IndexStaticIsEqualElementElementMulti(array, element, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
#ifdef TRI_INTERNAL_STATS    
    array->_nrProbesA++;
#endif
  }

  arrayElement = array->_table + (i * array->_elementSize);
  
  
  // ...........................................................................
  // If we found an element, return. While we allow duplicate entries in the hash
  // table, we do not allow duplicate elements. Elements would refer to the 
  // (for example) an actual row in memory. This is different from the 
  // TRI_InsertElementMultiArray function below where we only have keys to
  // differentiate between elements.
  // ...........................................................................
  
  found = ! IndexStaticIsEmptyElement(array, arrayElement);

  if (found) {
    if (overwrite) {
      // destroy the underlying element since we are going to stomp on top if it
      IndexStaticDestroyElement(array, arrayElement);
      IndexStaticCopyElementElement(array, arrayElement,  element);
    }

    return false;
  }

  
  // ...........................................................................
  // add a new element to the associative array
  // ...........................................................................

  IndexStaticCopyElementElement(array, arrayElement,  element);
  array->_nrUsed++;


  // ...........................................................................
  // if we were adding and the table is more than half full, extend it
  // ...........................................................................

  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeHashArrayMulti(array);
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertKeyHashArrayMulti (TRI_hasharray_t* array, void* key, void* element, bool overwrite) {
  uint64_t hash;
  uint64_t i;
  void* arrayElement;
  
  
  // ...........................................................................
  // compute the hash
  // ...........................................................................

  hash = IndexStaticHashKey(array, key);
  i = hash % array->_nrAlloc;

  
  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS    
  array->_nrAdds++;
#endif

  
  // ...........................................................................
  // search the table
  // ...........................................................................

  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
#ifdef TRI_INTERNAL_STATS    
    array->_nrProbesA++;
#endif
  }

  arrayElement = array->_table + (i * array->_elementSize);
  
  
  // ...........................................................................
  // We do not look for an element (as opposed to the function above). So whether
  // or not there exists a duplicate we do not care.
  // ...........................................................................


  // ...........................................................................
  // add a new element to the associative array
  // ...........................................................................

  IndexStaticCopyElementElement(array, arrayElement,  element);
  array->_nrUsed++;


  // ...........................................................................
  // if we were adding and the table is more than half full, extend it
  // ...........................................................................

  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeHashArrayMulti(array);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveElementHashArrayMulti (TRI_hasharray_t* array, void* element) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  bool found;
  void* arrayElement;
  
  // ...........................................................................
  // Obtain the hash
  // ...........................................................................
  
  hash = IndexStaticHashElement(array, element);
  i = hash % array->_nrAlloc;


  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS    
  array->_nrRems++;
#endif

  
  // ...........................................................................
  // search the table
  // ...........................................................................
  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize) &&
         ! IndexStaticIsEqualElementElementMulti(array, element, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
#ifdef TRI_INTERNAL_STATS    
    array->_nrProbesD++;
#endif
  }

  arrayElement = array->_table + (i * array->_elementSize);

  
  // ...........................................................................
  // if we did not find such an item return false
  // ...........................................................................
  
  found = ! IndexStaticIsEmptyElement(array, arrayElement);

  if (! found) {
    return false;
  }


  // ...........................................................................
  // remove item
  // ...........................................................................

  IndexStaticDestroyElement(array, arrayElement);
  array->_nrUsed--;


  // ...........................................................................
  // and now check the following places for items to move here
  // ...........................................................................
  
  k = (i + 1) % array->_nrAlloc;

  while (! IndexStaticIsEmptyElement(array, array->_table + k * array->_elementSize)) {
    uint64_t j = IndexStaticHashElement(array, array->_table + k * array->_elementSize) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      memcpy(array->_table + i * array->_elementSize, array->_table + k * array->_elementSize, array->_elementSize);
      IndexStaticClearElement(array, array->_table + k * array->_elementSize);
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveKeyHashArrayMulti (TRI_hasharray_t* array, void* key) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  bool found;
  void* arrayElement;
  
  // ...........................................................................
  // generate hash using key
  // ...........................................................................
  
  hash = IndexStaticHashKey(array, key);
  i = hash % array->_nrAlloc;


  // ...........................................................................
  // update statistics
  // ...........................................................................

#ifdef TRI_INTERNAL_STATS    
  array->_nrRems++;
#endif


  // ...........................................................................
  // search the table -- we can only match keys
  // ...........................................................................
  
  while (! IndexStaticIsEmptyElement(array, array->_table + i * array->_elementSize) &&
         ! IndexStaticIsEqualKeyElementMulti(array, key, array->_table + i * array->_elementSize)) {
    i = (i + 1) % array->_nrAlloc;
#ifdef TRI_INTERNAL_STATS    
    array->_nrProbesD++;
#endif
  }

  arrayElement = array->_table + (i * array->_elementSize);

  
  // ...........................................................................
  // if we did not find such an item return false
  // ...........................................................................
  
  found = ! IndexStaticIsEmptyElement(array, arrayElement);
  
  if (! found) {
    return false;
  }


  // ...........................................................................
  // remove item
  // ...........................................................................
  
  IndexStaticDestroyElement(array, arrayElement);
  array->_nrUsed--;


  // ...........................................................................
  // and now check the following places for items to move here
  // ...........................................................................
  k = (i + 1) % array->_nrAlloc;

  while (! IndexStaticIsEmptyElement(array, array->_table + k * array->_elementSize)) {
    uint64_t j = IndexStaticHashElement(array, array->_table + k * array->_elementSize) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      memcpy(array->_table + i * array->_elementSize, array->_table + k * array->_elementSize, array->_elementSize);
      IndexStaticClearElement(array, array->_table + k * array->_elementSize);
      i = k;
    }

    k = (k + 1) % array->_nrAlloc;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
