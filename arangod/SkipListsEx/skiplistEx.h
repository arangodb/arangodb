////////////////////////////////////////////////////////////////////////////////
/// @brief skip list which suports transactions implementation
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


#ifndef TRIAGENS_BASICS_C_SKIPLIST_EX_H
#define TRIAGENS_BASICS_C_SKIPLIST_EX_H 1

#include "BasicsC/common.h"
#include "BasicsC/locks.h"
#include "BasicsC/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                           skiplistEx public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SkiplistEx
/// @{
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// @brief types which enumerate the probability used to determine the height of node
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_SKIPLIST_EX_PROB_HALF,
  TRI_SKIPLIST_EX_PROB_THIRD,
  TRI_SKIPLIST_EX_PROB_QUARTER
} 
TRI_skiplistEx_prob_e;  


typedef enum {
  TRI_SKIPLIST_EX_COMPARE_STRICTLY_LESS = -1,
  TRI_SKIPLIST_EX_COMPARE_STRICTLY_GREATER = 1,
  TRI_SKIPLIST_EX_COMPARE_STRICTLY_EQUAL = 0,
  TRI_SKIPLIST_EX_COMPARE_SLIGHTLY_LESS = -2,
  TRI_SKIPLIST_EX_COMPARE_SLIGHTLY_GREATER = 2
} 
TRI_skiplistEx_compare_e;  

////////////////////////////////////////////////////////////////////////////////
/// @brief storage structure for a node's nearest neighbours
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// The nearest neighbour node needs to be modified for handling transactions.
// We introduce a structure for the forward and back links.
// To implement 'lock free', we require an atomic Compare & Save (C&S) function
// to use this correctly on Mac/Windows we need to align pointer/integers 
// on 32 or 64 bit boundaries.
// .............................................................................

typedef struct TRI_skiplistEx_nb_s {
  void* _prev;     // points to the previous nearest neighbour of this node (the left node)
  void* _next;     // points to the successor of this node (right node)
} TRI_skiplistEx_nb_t; // nearest neighbour;



////////////////////////////////////////////////////////////////////////////////
/// @brief structure of a skip list node (unique and non-unique)
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// Alignment required on 32/64 bit boundaries
// Use volatile to stop compiler doing fancy optimisations
// .............................................................................

typedef struct TRI_skiplistEx_node_s {
  uint64_t _flag;  // the _flag field operates as follows:
                   // if (_flag & 1) == 1, then the Tower Node (the node which uses this structure) is a Glass Tower Node.
                   // if (_flag & 2) == 2, then busy extending Tower Node
                   // if (_flag & 4) == 4, then busy joing nearest neighbours in this Tower Node
  volatile TRI_skiplistEx_nb_t* _column; // these represent the levels and the links within these, an array of these
  void* _extraData;
  void* _element;  
  uint64_t _delTransID;         // the transaction id which removed (deleted) this node
  uint64_t _insTransID;         // the transaction id which inserted this node
  uint32_t _colLength;          // the height of the column  
} 
TRI_skiplistEx_node_t;

  
////////////////////////////////////////////////////////////////////////////////
/// @brief The base structure of a skiplist (unique and non-unique)
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplistEx_base_s {
  // ...........................................................................
  // 64 bit integer for CAS flags
  // ...........................................................................
  uint64_t _flags;
  
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
  TRI_skiplistEx_prob_e _prob;
  int32_t               _numRandom;
  uint32_t*             _random;
  
  
  TRI_skiplistEx_node_t _startNode;
  TRI_skiplistEx_node_t _endNode;
  
  TRI_mutex_t _startEndNodeExclusiveLock; // Exclusive lock in prepartion for simultaneous inserts which
                                          // affect the -\infty and \infty nodes 
}
TRI_skiplistEx_base_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                    unique skiplistEx public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SkiplistEx_unique
/// @{
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// @brief structure used for a skip list which only accepts unique entries
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplistEx_s {
  TRI_skiplistEx_base_t _base;
  // ...........................................................................
  // callback compare function
  // < 0: implies left < right
  // == 0: implies left == right
  // > 0: implies left > right
  // ...........................................................................
  int (*compareElementElement) (struct TRI_skiplistEx_s*, void*, void*, int);
  int (*compareKeyElement) (struct TRI_skiplistEx_s*, void*, void*, int);    
}
TRI_skiplistEx_t;


////////////////////////////////////////////////////////////////////////////////
/// @brief structure used for a skip list which only accepts unique entries and is thread safe
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// structure for a skiplist which allows unique entries -- with locking
// available for its nearest neighbours.
// TODO: implement locking for nearest neighbours rather than for all of index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplistEx_synced_s {
  TRI_skiplistEx_t _base;
  TRI_read_write_lock_t _lock;  
} TRI_skiplistEx_synced_t;


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
/// @brief initialises a skip list
////////////////////////////////////////////////////////////////////////////////

int TRI_InitSkipListEx (TRI_skiplistEx_t*,
                         size_t elementSize,
                         int (*compareElementElement) (TRI_skiplistEx_t*, void*, void*, int),
                         int (*compareKeyElement) (TRI_skiplistEx_t*, void*, void*, int),
                         TRI_skiplistEx_prob_e, 
                         uint32_t, 
                         uint64_t lastKnownTransID);


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipListEx (TRI_skiplistEx_t*);


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipListEx (TRI_skiplistEx_t*);




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

void* TRI_EndNodeSkipListEx (TRI_skiplistEx_t*);



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list using element for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementSkipListEx (TRI_skiplistEx_t*, void*, bool, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list using key for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeySkipListEx (TRI_skiplistEx_t*, void*, void*, bool, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key, returns greatest left element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LeftLookupByKeySkipListEx (TRI_skiplistEx_t*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key, returns null if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeySkipListEx (TRI_skiplistEx_t*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node in the skip list, if the end is reached returns the end node
////////////////////////////////////////////////////////////////////////////////

void* TRI_NextNodeSkipListEx (TRI_skiplistEx_t*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the prev node in the skip list, if the beginning is reached returns the start node
////////////////////////////////////////////////////////////////////////////////

void* TRI_PrevNodeSkipListEx (TRI_skiplistEx_t*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list using element for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementSkipListEx (TRI_skiplistEx_t*, void*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list using key for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeySkipListEx (TRI_skiplistEx_t*, void*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key, returns least right element
////////////////////////////////////////////////////////////////////////////////

void* TRI_RightLookupByKeySkipListEx (TRI_skiplistEx_t*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief returns the start node  which belongs to a skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_StartNodeSkipListEx (TRI_skiplistEx_t*);

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

typedef struct TRI_skiplistEx_multi_s {
  TRI_skiplistEx_base_t _base;
  // ...........................................................................
  // callback compare function
  // < 0: implies left < right
  // == 0: implies left == right
  // > 0: implies left > right
  // ...........................................................................
  int (*compareElementElement) (struct TRI_skiplistEx_multi_s*, void*, void*, int);
  int (*compareKeyElement) (struct TRI_skiplistEx_multi_s*, void*, void*, int);
  
  // ...........................................................................
  // Returns true if the element is an exact copy, or if the data which the
  // element points to is an exact copy
  // ...........................................................................
  bool (*equalElementElement) (struct TRI_skiplistEx_multi_s*, void*, void*);
} TRI_skiplistEx_multi_t;



////////////////////////////////////////////////////////////////////////////////
/// @brief structure used for a multi skip list and is thread safe
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplistEx_synced_multi_s {
  TRI_skiplistEx_t _base;
  TRI_read_write_lock_t _lock;  
} TRI_skiplistEx_synced_multi_t;



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


int TRI_InitSkipListExMulti (TRI_skiplistEx_multi_t*,
                             size_t elementSize,
                             int (*compareElementElement) (TRI_skiplistEx_multi_t*, void*, void*, int),
                             int (*compareKeyElement) (TRI_skiplistEx_multi_t*, void*, void*, int),
                             bool (*equalElementElement) (TRI_skiplistEx_multi_t*, void*, void*),
                             TRI_skiplistEx_prob_e, 
                             uint32_t,
                             uint64_t lastKnownTransID);


                                              
////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a multi skip list, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkipListExMulti (TRI_skiplistEx_multi_t*);



////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipListExMulti (TRI_skiplistEx_multi_t*);




////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////




// -----------------------------------------------------------------------------
// --SECTION--                            non-unique skiplistEx public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SkiplistEx_non_unique
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the end node which belongs to a skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_EndNodeSkipListExMulti (TRI_skiplistEx_multi_t*);



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list using element for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertElementSkipListExMulti (TRI_skiplistEx_multi_t*, void*, bool, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the skip list using key for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeySkipListExMulti (TRI_skiplistEx_multi_t*, void*, void*, bool, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key, returns greatest left element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LeftLookupByKeySkipListExMulti (TRI_skiplistEx_multi_t*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key, returns null if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeySkipListExMulti (TRI_skiplistEx_multi_t*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the next node in the skip list, if the end is reached returns the end node
////////////////////////////////////////////////////////////////////////////////

void* TRI_NextNodeSkipListExMulti (TRI_skiplistEx_multi_t*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief given a node returns the prev node in the skip list, if the beginning is reached returns the start node
////////////////////////////////////////////////////////////////////////////////

void* TRI_PrevNodeSkipListExMulti (TRI_skiplistEx_multi_t*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list using element for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementSkipListExMulti (TRI_skiplistEx_multi_t*, void*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the skip list using key for comparison
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveKeySkipListExMulti (TRI_skiplistEx_multi_t*, void*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief lookups an element given a key, returns least right element
////////////////////////////////////////////////////////////////////////////////

void* TRI_RightLookupByKeySkipListExMulti (TRI_skiplistEx_multi_t*, void*, uint64_t thisTransID);



////////////////////////////////////////////////////////////////////////////////
/// @brief returns the start node  which belongs to a skiplist
////////////////////////////////////////////////////////////////////////////////

void* TRI_StartNodeSkipListExMulti (TRI_skiplistEx_multi_t*);



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
