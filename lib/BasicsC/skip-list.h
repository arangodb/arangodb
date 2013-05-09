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
// --SECTION--                                             skiplist public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Skiplist
/// @{
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// @brief types which enumerate the probability used to determine the height of node
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_SKIP_LIST_CREATE_ELEMENT,
  TRI_SKIP_LIST_DESTROY_ELEMENT,
  TRI_SKIP_LIST_REPLACE_ELEMENT
} 
TRI_skip_list_action_e;  

typedef enum {
  TRI_SKIP_LIST_PROB_HALF,
  TRI_SKIP_LIST_PROB_THIRD,
  TRI_SKIP_LIST_PROB_QUARTER
} 
TRI_skip_list_prob_e;  


typedef enum {
  TRI_SKIP_LIST_COMPARE_STRICTLY_LESS = -1,
  TRI_SKIP_LIST_COMPARE_STRICTLY_GREATER = 1,
  TRI_SKIP_LIST_COMPARE_STRICTLY_EQUAL = 0,
  TRI_SKIP_LIST_COMPARE_SLIGHTLY_LESS = -2,
  TRI_SKIP_LIST_COMPARE_SLIGHTLY_GREATER = 2
} 
TRI_skip_list_compare_e;  

////////////////////////////////////////////////////////////////////////////////
/// @brief storage structure for a node's nearest neighbours
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skip_list_nb_s {
  void* _prev;
  void* _next;
}
TRI_skip_list_nb_t; // nearest neighbour;



////////////////////////////////////////////////////////////////////////////////
/// @brief structure of a skip list node (unique and non-unique)
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skip_list_node_s {
  TRI_skip_list_nb_t* _column; // these represent the levels
  uint32_t _colLength; 
  void* _extraData;
  void* _element;  
} 
TRI_skip_list_node_t;

  
////////////////////////////////////////////////////////////////////////////////
/// @brief The base structure of a skiplist (unique and non-unique)
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skip_list_base_s {
  // ...........................................................................
  // The maximum height of this skip list. Thus 2^(_maxHeight) elements can be
  // stored in the skip list. 
  // ...........................................................................
  uint32_t _maxHeight;

  // ...........................................................................
  // The size of each element which is to be stored. This allows us to store
  // memory assigned to the element within the node directly, saving one level
  // of indirection when we access the data within the element.
  // ...........................................................................
  uint32_t _elementSize;
  
  
  // ...........................................................................
  // The probability which is used to determine the level for insertions
  // into the list. Note the following
  // ...........................................................................
  TRI_skip_list_prob_e _prob;
  int32_t             _numRandom;
  uint32_t*           _random; // storage for random numbers so we do not need
                               // to constantly allocate and de-allocate memory
  bool                _unique; // true if skiplist supports only unique keys
  
  // ...........................................................................
  // The main two nodes which are always required in any skiplist.
  // ...........................................................................
  TRI_skip_list_node_t _startNode;
  TRI_skip_list_node_t _endNode;
}
TRI_skip_list_base_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                      unique skiplist public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Skiplist_unique
/// @{
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// @brief structure used for a skip list which only accepts unique entries
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skip_list_s {
  TRI_skip_list_base_t _base;
  // ...........................................................................
  // callback compare function
  // < 0: implies left < right
  // == 0: implies left == right
  // > 0: implies left > right
  // ...........................................................................
  int (*_compareElementElement) (struct TRI_skip_list_s*, void*, void*, int);
  int (*_compareKeyElement) (struct TRI_skip_list_s*, void*, void*, int);    
  int (*_actionElement) (struct TRI_skip_list_s*, TRI_skip_list_action_e, void*, const void*, void*);    
}
TRI_skip_list_t;


////////////////////////////////////////////////////////////////////////////////
/// @brief structure used for a skip list which only accepts unique entries and is thread safe
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// structure for a skiplist which allows unique entries -- with locking
// available for its nearest neighbours.
// TODO: implement locking for nearest neighbours rather than for all of index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skip_list_synced_s {
  TRI_skip_list_t _base;
  TRI_read_write_lock_t _lock;  
} TRI_skip_list_synced_t;


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                 unique skiplist      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Skiplist_unique
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates and initialises a skip list
////////////////////////////////////////////////////////////////////////////////

int TRI_InitSkipList (TRI_skip_list_t*,
                       uint32_t elementSize,
                       int (*compareElementElement) (TRI_skip_list_t*, void*, void*, int),
                       int (*compareKeyElement) (TRI_skip_list_t*, void*, void*, int),
                       int (*actionElement) (TRI_skip_list_t*, TRI_skip_list_action_e, void*, const void*, void*),
                       TRI_skip_list_prob_e, uint32_t);


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipList (TRI_skip_list_t*);


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipList (TRI_skip_list_t*);




////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                 unique skiplist  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Skiplist_unique
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the end node which belongs to a skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_EndNodeSkipList (TRI_skip_list_t*);



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list using element for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementSkipList (TRI_skip_list_t*, void*, bool);



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list using key for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeySkipList (TRI_skip_list_t*, void*, void*, bool);



////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key, returns greatest left element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LeftLookupByKeySkipList (TRI_skip_list_t*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key, returns null if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeySkipList (TRI_skip_list_t*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node in the skip list, if the end is reached returns the end node
////////////////////////////////////////////////////////////////////////////////

void* TRI_NextNodeSkipList (TRI_skip_list_t*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the prev node in the skip list, if the beginning is reached returns the start node
////////////////////////////////////////////////////////////////////////////////

void* TRI_PrevNodeSkipList(TRI_skip_list_t*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list using element for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementSkipList (TRI_skip_list_t*, void*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list using key for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeySkipList (TRI_skip_list_t*, void*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key, returns least right element
////////////////////////////////////////////////////////////////////////////////

void* TRI_RightLookupByKeySkipList (TRI_skip_list_t*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief returns the start node  which belongs to a skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_StartNodeSkipList (TRI_skip_list_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                  non-unique skiplist public types
// -----------------------------------------------------------------------------



////////////////////////////////////////////////////////////////////////////////
/// @brief structure used for a multi skiplist
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Skiplist_non_unique
/// @{
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skip_list_multi_s {
  TRI_skip_list_base_t _base;
  // ...........................................................................
  // callback compare function
  // < 0: implies left < right
  // == 0: implies left == right
  // > 0: implies left > right
  // ...........................................................................
  int (*_compareElementElement) (struct TRI_skip_list_multi_s*, void*, void*, int);
  int (*_compareKeyElement) (struct TRI_skip_list_multi_s*, void*, void*, int);    

  // ...........................................................................
  // Returns true if the element is an exact copy, or if the data which the
  // element points to is an exact copy
  // ...........................................................................
  bool (*_equalElementElement) (struct TRI_skip_list_multi_s*, void*, void*);

  int (*_actionElement) (struct TRI_skip_list_multi_s*, TRI_skip_list_action_e, void*, const void*, void*);      
  
} TRI_skip_list_multi_t;



////////////////////////////////////////////////////////////////////////////////
/// @brief structure used for a multi skip list and is thread safe
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skip_list_synced_multi_s {
  TRI_skip_list_t _base;
  TRI_read_write_lock_t _lock;  
} TRI_skip_list_synced_multi_t;



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////



// -----------------------------------------------------------------------------
// --SECTION--                 non-unique skiplist  constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Skiplist_non_unique
/// @{
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a skip list
////////////////////////////////////////////////////////////////////////////////


int TRI_InitSkipListMulti (TRI_skip_list_multi_t*,
                            uint32_t elementSize,
                            int (*compareElementElement) (TRI_skip_list_multi_t*, void*, void*, int),
                            int (*compareKeyElement) (TRI_skip_list_multi_t*, void*, void*, int),
                            bool (*equalElementElement) (TRI_skip_list_multi_t*, void*, void*),
                            int (*actionElement) (TRI_skip_list_multi_t*, TRI_skip_list_action_e, void*, const void*, void*),
                            TRI_skip_list_prob_e, uint32_t);


                                              
////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a multi skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipListMulti (TRI_skip_list_multi_t*);



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipListMulti (TRI_skip_list_multi_t*);




////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////




// -----------------------------------------------------------------------------
// --SECTION--                                 unique skiplist  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Skiplist_non_unique
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the end node which belongs to a skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_EndNodeSkipListMulti (TRI_skip_list_multi_t*);



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list using element for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementSkipListMulti (TRI_skip_list_multi_t*, void*, bool);



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list using key for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeySkipListMulti (TRI_skip_list_multi_t*, void*, void*, bool);



////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key, returns greatest left element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LeftLookupByKeySkipListMulti (TRI_skip_list_multi_t*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key, returns null if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeySkipListMulti (TRI_skip_list_multi_t*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node in the skip list, if the end is reached returns the end node
////////////////////////////////////////////////////////////////////////////////

void* TRI_NextNodeSkipListMulti (TRI_skip_list_multi_t*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the prev node in the skip list, if the beginning is reached returns the start node
////////////////////////////////////////////////////////////////////////////////

void* TRI_PrevNodeSkipListMulti (TRI_skip_list_multi_t*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list using element for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementSkipListMulti (TRI_skip_list_multi_t*, void*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list using key for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeySkipListMulti (TRI_skip_list_multi_t*, void*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key, returns least right element
////////////////////////////////////////////////////////////////////////////////

void* TRI_RightLookupByKeySkipListMulti (TRI_skip_list_multi_t*, void*);



////////////////////////////////////////////////////////////////////////////////
/// @brief returns the start node  which belongs to a skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_StartNodeSkipListMulti (TRI_skip_list_multi_t*);



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
