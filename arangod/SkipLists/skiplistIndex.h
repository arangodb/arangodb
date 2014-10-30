////////////////////////////////////////////////////////////////////////////////
/// @brief unique hash index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SKIP_LISTS_SKIPLIST_INDEX_H
#define ARANGODB_SKIP_LISTS_SKIPLIST_INDEX_H 1

#include "Basics/Common.h"

#include "Basics/skip-list.h"

#include "IndexOperators/index-operator.h"
#include "ShapedJson/shaped-json.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_doc_mptr_t;
struct TRI_document_collection_t;

// -----------------------------------------------------------------------------
// --SECTION--                                        skiplistIndex public types
// -----------------------------------------------------------------------------

typedef struct {
  triagens::basics::SkipList* skiplist;
  bool unique;
  struct TRI_document_collection_t* _collection;
  size_t _numFields;
}
SkiplistIndex;

typedef struct {
  TRI_shaped_json_t* _fields;   // list of shaped json objects which the
                                // collection should know about
  size_t _numFields;   // Note that the number of fields coming from
                       // a query can be smaller than the number of
                       // fields indexed
}
TRI_skiplist_index_key_t;

typedef struct {
  TRI_shaped_sub_t* _subObjects;    // list of shaped json objects which the
                                    // collection should know about
  struct TRI_doc_mptr_t* _document; // master document pointer
}
TRI_skiplist_index_element_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Iterator structure for skip list. We require a start and stop node
///
/// Intervals are open in the sense that both end points are not members
/// of the interval. This means that one has to use SkipList::nextNode
/// on the start node to get the first element and that the stop node
/// can be NULL. Note that it is ensured that all intervals in an iterator
/// are non-empty.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplist_iterator_interval_s {
  triagens::basics::SkipListNode* _leftEndPoint;
  triagens::basics::SkipListNode* _rightEndPoint;
}
TRI_skiplist_iterator_interval_t;

typedef struct TRI_skiplist_iterator_s {
  SkiplistIndex* _index;
  TRI_vector_t _intervals;
  size_t _currentInterval; // starts with 0, current interval used
  triagens::basics::SkipListNode* _cursor;
                 // always holds the last node returned, initially equal to
                 // the _leftEndPoint of the first interval (or the 
                 // _rightEndPoint of the last interval in the reverse
                 // case), can be nullptr if there are no intervals
                 // (yet), or, in the reverse case, if the cursor is
                 // at the end of the last interval. Additionally
                 // in the non-reverse case _cursor is set to nullptr
                 // if the cursor is exhausted.
                 // See SkiplistNextIterationCallback and
                 // SkiplistPrevIterationCallback for the exact
                 // condition for the iterator to be exhausted.
  bool  (*hasNext) (struct TRI_skiplist_iterator_s const*);
  TRI_skiplist_index_element_t* (*next)(struct TRI_skiplist_iterator_s*);
}
TRI_skiplist_iterator_t;

// -----------------------------------------------------------------------------
// --SECTION--                                     skiplistIndex  public methods
// -----------------------------------------------------------------------------

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

int SkiplistIndex_assignMethod (void*, TRI_index_method_assignment_type_e);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Skiplist indices, both unique and non-unique
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

SkiplistIndex* SkiplistIndex_new (struct TRI_document_collection_t*,
                                  size_t, bool);

TRI_skiplist_iterator_t* SkiplistIndex_find (SkiplistIndex*, 
                                             TRI_vector_t const*,
                                             TRI_index_operator_t const*,
                                             bool);

int SkiplistIndex_insert (SkiplistIndex*, TRI_skiplist_index_element_t*);

int SkiplistIndex_remove (SkiplistIndex*, TRI_skiplist_index_element_t*);

bool SkiplistIndex_update (SkiplistIndex*, const TRI_skiplist_index_element_t*,
                           const TRI_skiplist_index_element_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of elements in the index
////////////////////////////////////////////////////////////////////////////////

uint64_t SkiplistIndex_getNrUsed (SkiplistIndex*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the memory used by the index
////////////////////////////////////////////////////////////////////////////////

size_t SkiplistIndex_memoryUsage (SkiplistIndex const*);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
