////////////////////////////////////////////////////////////////////////////////
/// @brief unique hash index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_SKIP_LISTS_SKIPLIST_INDEX_H
#define TRIAGENS_SKIP_LISTS_SKIPLIST_INDEX_H 1

#include "BasicsC/common.h"

#include "BasicsC/skip-list.h"

#include "IndexIterators/index-iterator.h"
#include "IndexOperators/index-operator.h"
#include "ShapedJson/shaped-json.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_doc_mptr_s;
struct TRI_primary_collection_s;

// -----------------------------------------------------------------------------
// --SECTION--                                        skiplistIndex public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup skiplistIndex
/// @{
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  TRI_skiplist_t* skiplist;
  bool unique;
  bool sparse;
  struct TRI_primary_collection_s* _collection;
  size_t _numFields;
} 
SkiplistIndex;

typedef struct {
  TRI_shaped_json_t* _fields;   // list of shaped json objects which the 
                                // collection should know about
} 
TRI_skiplist_index_key_t;

typedef struct {
  TRI_shaped_sub_t* _subObjects;    // list of shaped json objects which the 
                                    // collection should know about
  struct TRI_doc_mptr_s* _document; // master document pointer
} 
TRI_skiplist_index_element_t;

  
////////////////////////////////////////////////////////////////////////////////
/// @brief Iterator structure for skip list. We require a start and stop node
///
/// Intervals are open in the sense that both end points are not members 
/// of the interval. This means that one has to use TRI_SkipListNextNode
/// on the start node to get the first element and that the stop node
/// can be NULL. Note that it is ensured that all intervals in an iterator
/// are non-empty.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplist_iterator_interval_s {
  TRI_skiplist_node_t* _leftEndPoint;
  TRI_skiplist_node_t* _rightEndPoint;
} 
TRI_skiplist_iterator_interval_t;

typedef struct TRI_skiplist_iterator_s {
  SkiplistIndex* _index;
  TRI_vector_t _intervals;
  size_t _currentInterval; // starts with 0, current interval used
  TRI_skiplist_node_t* _cursor; 
                 // always holds the last node returned, initially equal to 
                 // the _leftEndPoint of the first interval, can be NULL
                 // if there are no intervals (yet), in any case, if
                 // _cursor is NULL, then there are (currently) no more
                 // documents in the iterator.
  bool  (*_hasNext) (struct TRI_skiplist_iterator_s*);
  void* (*_next)    (struct TRI_skiplist_iterator_s*);
  void* (*_nexts)   (struct TRI_skiplist_iterator_s*, int64_t jumpSize);
} 
TRI_skiplist_iterator_t;


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                     skiplistIndex  public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup skiplistIndex
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a skiplist iterator
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkiplistIterator (TRI_skiplist_iterator_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list index , but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void SkiplistIndex_destroy (SkiplistIndex*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a skip list index and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void SkiplistIndex_free (SkiplistIndex*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

int SkiplistIndex_assignMethod (void*, TRI_index_method_assignment_type_e);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Skiplist indices, both unique and non-unique
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

SkiplistIndex* SkiplistIndex_new (struct TRI_primary_collection_s*,
                                  size_t, bool, bool);

TRI_skiplist_iterator_t* SkiplistIndex_find (SkiplistIndex*, TRI_vector_t*, 
                                             TRI_index_operator_t*); 

int SkiplistIndex_insert (SkiplistIndex*, TRI_skiplist_index_element_t*);

int SkiplistIndex_remove (SkiplistIndex*, TRI_skiplist_index_element_t*); 

bool SkiplistIndex_update (SkiplistIndex*, const TRI_skiplist_index_element_t*,
                           const TRI_skiplist_index_element_t*);

uint64_t SkiplistIndex_getNrUsed(SkiplistIndex*);

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

