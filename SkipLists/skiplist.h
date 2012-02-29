////////////////////////////////////////////////////////////////////////////////
/// @brief skip list implementation
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
/// @author Dr. O
/// @author Copyright 2006-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


#ifndef TRIAGENS_BASICS_C_SKIPLIST_H
#define TRIAGENS_BASICS_C_SKIPLIST_H 1

#include "BasicsC/common.h"
#include "BasicsC/locks.h"
#include "BasicsC/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief skip list
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_SKIPLIST_PROB_HALF,
  TRI_SKIPLIST_PROB_THIRD,
  TRI_SKIPLIST_PROB_QUARTER
} 
TRI_skiplist_prob_e;  

typedef struct TRI_skiplist_nb_s {
  void* _prev;
  void* _next;
}
TRI_skiplist_nb_t; // nearest neighbour;

typedef struct TRI_skiplist_node_s {
  TRI_skiplist_nb_t* _column; // these represent the levels
  uint32_t _colLength; 
  void* _extraData;
  void* _element;  
} 
TRI_skiplist_node_t;

  
////////////////////////////////////////////////////////////////////////////////
// The base structure of a skiplist
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplist_base_s {
  // ...........................................................................
  // The maximum height of this skip list. Thus 2^(_maxHeight) elements can be
  // stored in the skip list. 
  // ...........................................................................
  uint32_t _maxHeight;

  // ...........................................................................
  // The size of each element which is to be stored.
  // ...........................................................................
  uint32_t _elementSize;
  
  // ...........................................................................
  // The actual list itself
  // ...........................................................................
  char* _skiplist; 
  
  // ...........................................................................
  // The probability which is used to determine the level for insertions
  // into the list. Note the following
  // ...........................................................................
  TRI_skiplist_prob_e _prob;
  int32_t             _numRandom;
  uint32_t*           _random;
  
  
  TRI_skiplist_node_t _startNode;
  TRI_skiplist_node_t _endNode;
  
}
TRI_skiplist_base_t;


////////////////////////////////////////////////////////////////////////////////
// The base structure of a skiplist
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplist_s {
  TRI_skiplist_base_t _base;
  // ...........................................................................
  // callback compare function
  // < 0: implies left < right
  // == 0: implies left == right
  // > 0: implies left > right
  // ...........................................................................
  int (*compareElementElement) (struct TRI_skiplist_s*, void*, void*);
  int (*compareKeyElement) (struct TRI_skiplist_s*, void*, void*);    
}
TRI_skiplist_t;

////////////////////////////////////////////////////////////////////////////////
// structure for a skiplist which allows duplicate entries
// the find returns a begin and end iterator
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplist_multi_resut_s {
  TRI_skiplist_node_t* _startIterator;
  TRI_skiplist_node_t* _endIterator;  
  // this structure is not thread safe, one or both of the iterators may have
  // disappeared.
} TRI_skiplist_multi_resut_t;

typedef struct TRI_skiplist_multi_s {
  TRI_skiplist_base_t _base;
  // ...........................................................................
  // callback compare function
  // < 0: implies left < right
  // == 0: implies left == right
  // > 0: implies left > right
  // ...........................................................................
  int (*compareElementElement) (struct TRI_skiplist_multi_s*, void*, void*);
  int (*compareKeyElement) (struct TRI_skiplist_multi_s*, void*, void*);
} TRI_skiplist_multi_t;


////////////////////////////////////////////////////////////////////////////////
// structure for a skiplist which allows unique entries -- with locking
// available for its nearest neighbours.
// TODO: implement locking for nearest neighbours rather than for all of index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplist_synced_s {
  TRI_skiplist_t _base;
  TRI_read_write_lock_t _lock;  
} TRI_skiplist_synced_t;

typedef struct TRI_skiplist_synced_multi_s {
  TRI_skiplist_t _base;
  TRI_read_write_lock_t _lock;  
} TRI_skiplist_synced_multi_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                         SKIP LIST
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a skip list
////////////////////////////////////////////////////////////////////////////////

void TRI_InitSkipList (TRI_skiplist_t*,
                       size_t elementSize,
                       int (*compareElementElement) (TRI_skiplist_t*, void*, void*),
                       int (*compareKeyElement) (TRI_skiplist_t*, void*, void*),
                       TRI_skiplist_prob_e, uint32_t);

void TRI_InitSkipListMulti (TRI_skiplist_multi_t*,
                       size_t elementSize,
                       int (*compareElementElement) (TRI_skiplist_multi_t*, void*, void*),
                       int (*compareKeyElement) (TRI_skiplist_multi_t*, void*, void*),
                       TRI_skiplist_prob_e, uint32_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipList (TRI_skiplist_t*);

void TRI_DestroySkipListMulti (TRI_skiplist_multi_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipList (TRI_skiplist_t*);

void TRI_FreeSkipListMulti (TRI_skiplist_multi_t*);

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

void* TRI_LookupByKeySkipList (TRI_skiplist_t*, void* key);

TRI_vector_pointer_t TRI_LookupByKeySkipListMulti (TRI_skiplist_multi_t*, void* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_FindByKeySkipList (TRI_skiplist_t*, void* key);

TRI_vector_pointer_t TRI_FindByKeySkipListMulti (TRI_skiplist_multi_t*, void* key);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementSkipList (TRI_skiplist_t*, void* element);

TRI_vector_pointer_t TRI_LookupByElementSkipListMulti (TRI_skiplist_multi_t*, void* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given an element, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_FindByElementSkipList (TRI_skiplist_t*, void* element);

TRI_vector_pointer_t TRI_FindByElementSkipListMulti (TRI_skiplist_multi_t*, void* element);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertElementSkipList (TRI_skiplist_t*, void* element, bool overwrite);

bool TRI_InsertElementSkipListMulti (TRI_skiplist_multi_t*, void* element, bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the skip list 
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertKeySkipList (TRI_skiplist_t*, void* key, void* element, bool overwrite);

bool TRI_InsertKeySkipListMulti (TRI_skiplist_multi_t*, void* key, void* element, bool overwrite);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveElementSkipList (TRI_skiplist_t*, void* element, void* old);

bool TRI_RemoveElementSkipListMulti (TRI_skiplist_multi_t*, void* element, void* old);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the skip list
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveKeySkipList (TRI_skiplist_t*, void* key, void* old);

bool TRI_RemoveKeySkipListMulti (TRI_skiplist_multi_t*, void* key, void* old);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
