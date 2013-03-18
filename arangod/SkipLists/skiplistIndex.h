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

#include "IndexIterators/index-iterator.h"
#include "IndexOperators/index-operator.h"
#include "ShapedJson/shaped-json.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_skiplist_s;
struct TRI_skiplist_multi_s;
struct TRI_skiplist_node_s;
struct TRI_doc_mptr_s;
struct TRI_shaped_sub_s;

// -----------------------------------------------------------------------------
// --SECTION--                                        skiplistIndex public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup skiplistIndex
/// @{
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  union {
    struct TRI_skiplist_s* uniqueSkiplist;
    struct TRI_skiplist_multi_s* nonUniqueSkiplist;
  } skiplist;
  bool unique;
} SkiplistIndex;


typedef struct {
  size_t numFields;          // the number of fields
  TRI_shaped_json_t* fields; // list of shaped json objects which the collection should know about
  void* collection;          // pointer to the collection;
}
TRI_skiplist_index_key_t;

typedef struct {
  size_t numFields;                     // the number of fields
  struct TRI_shaped_sub_s* _subObjects; // list of shaped json objects which the collection should know about
  struct TRI_doc_mptr_s* _document;     // master document pointer
  void* collection;                     // pointer to the collection;
}
TRI_skiplist_index_element_t;

typedef struct {
  size_t _numElements;
  TRI_skiplist_index_element_t* _elements; // simple list of elements
} SkiplistIndexElements;


////////////////////////////////////////////////////////////////////////////////
/// @brief Iterator structure for skip list. We require a start and stop node
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplist_iterator_interval_s {
  struct TRI_skiplist_node_s*  _leftEndPoint;
  struct TRI_skiplist_node_s*  _rightEndPoint;
} TRI_skiplist_iterator_interval_t;

typedef struct TRI_skiplist_iterator_s {
  SkiplistIndex* _index;
  TRI_vector_t _intervals;
  size_t _currentInterval; // starts with 0
  void* _cursor; // initially null
  bool  (*_hasNext) (struct TRI_skiplist_iterator_s*);
  void* (*_next)    (struct TRI_skiplist_iterator_s*);
  void* (*_nexts)   (struct TRI_skiplist_iterator_s*, int64_t jumpSize);
  bool  (*_hasPrev) (struct TRI_skiplist_iterator_s*);
  void* (*_prev)    (struct TRI_skiplist_iterator_s*);
  void* (*_prevs)   (struct TRI_skiplist_iterator_s*, int64_t jumpSize);
} TRI_skiplist_iterator_t;


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
// Unique skiplist indexes
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

SkiplistIndex* SkiplistIndex_new (void);

TRI_skiplist_iterator_t* SkiplistIndex_find (SkiplistIndex*, TRI_vector_t*, TRI_index_operator_t*);

int SkiplistIndex_insert (SkiplistIndex*, TRI_skiplist_index_element_t*);

int SkiplistIndex_remove (SkiplistIndex*, TRI_skiplist_index_element_t*);

bool SkiplistIndex_update (SkiplistIndex*, const TRI_skiplist_index_element_t*, const TRI_skiplist_index_element_t*);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Multi-skiplist non-unique skiplist indexes
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


SkiplistIndex* MultiSkiplistIndex_new (void);

TRI_skiplist_iterator_t* MultiSkiplistIndex_find (SkiplistIndex*, TRI_vector_t*, TRI_index_operator_t*);

int MultiSkiplistIndex_insert (SkiplistIndex*, TRI_skiplist_index_element_t*);

int MultiSkiplistIndex_remove (SkiplistIndex*, TRI_skiplist_index_element_t*);

bool MultiSkiplistIndex_update (SkiplistIndex*, TRI_skiplist_index_element_t*, TRI_skiplist_index_element_t*);


#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

