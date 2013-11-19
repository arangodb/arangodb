////////////////////////////////////////////////////////////////////////////////
/// @brief generic skip list implementation
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
/// @author Max Neunhoeffer
/// @author Copyright 2013-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_SKIP_LIST_H
#define TRIAGENS_BASICS_C_SKIP_LIST_H 1

#include "BasicsC/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// We will probably never see more than 2^48 documents in a skip list
#define TRI_SKIPLIST_MAX_HEIGHT 48

// -----------------------------------------------------------------------------
// --SECTION--                                                         SKIP LIST
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a skiplist node
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplist_node_s {
    struct TRI_skiplist_node_s** next;
    int height;
    void *doc;
} TRI_skiplist_node_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief two possibilities for comparison, see below
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_CMP_PREORDER,
  TRI_CMP_TOTORDER
}
TRI_cmp_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a function pointer to a comparison function for a skiplist.
///
/// The last argument is called preorder. If true then the comparison
/// function must implement a preorder (reflexive and transitive).
/// If preorder is false, then a proper total order must be
/// implemented by the comparison function. The proper total order
/// must refine the preorder in the sense that a < b in the proper order
/// implies a <= b in the preorder.
/// The first argument is a data pointer which may contain arbitrary
/// infrastructure needed for the comparison. See cmpdata field in the
/// skiplist type.
////////////////////////////////////////////////////////////////////////////////

typedef int (*TRI_skiplist_compare_func_t)(void*, void*, void*, TRI_cmp_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief Type of a pointer to a function that is called whenever a
/// document is removed from a skiplist.
////////////////////////////////////////////////////////////////////////////////

typedef void (*TRI_skiplist_free_func_t)(void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a skiplist
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplist_s {
    TRI_skiplist_node_t* start;
    TRI_skiplist_compare_func_t compare;
    void* cmpdata;   // will be the first arguemnt
    TRI_skiplist_free_func_t free;
    bool unique;     // indicates whether multiple entries that
                     // are equal in the preorder are allowed in
    uint64_t nrUsed;
} TRI_skiplist_t;


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new skiplist
///
/// Returns NULL if allocation fails and a pointer to the skiplist
/// otherwise.
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_t* TRI_InitSkipList (TRI_skiplist_compare_func_t cmpfunc,
                                  void *cmpdata,
                                  TRI_skiplist_free_func_t freefunc,
                                  bool unique);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a skiplist and all its documents
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkipList (TRI_skiplist_t* sl);


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Collections
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return the start node
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_node_t* TRI_SkipListStartNode (TRI_skiplist_t* sl);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the successor node or NULL if last node
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_node_t* TRI_SkipListNextNode (TRI_skiplist_node_t* node);

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a new document into a skiplist
///
/// Comparison is done using proper order comparison. If the skiplist
/// is unique then no two documents that compare equal in the preorder
/// can be inserted. Returns 0 if all is well, -1 if allocation failed
/// and -2 if the unique constraint would have been violated by the
/// insert. In the latter two cases nothing is inserted.
////////////////////////////////////////////////////////////////////////////////

int TRI_SkipListInsert (TRI_skiplist_t *sl, void *doc);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a skiplist
///
/// Comparison is done using proper order comparison. Returns 0 if all
/// is well and -1 if the document was not found. In the latter two
/// cases nothing is inserted.
////////////////////////////////////////////////////////////////////////////////

int TRI_SkipListRemove (TRI_skiplist_t *sl, void *doc);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document that is less to doc in the preorder
/// comparison or the start node if none is.
///
/// Only comparisons using the preorder are done.
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_node_t* TRI_SkipListLeftLookup (TRI_skiplist_t *sl, void *doc);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the last document that is less or equal to doc in
/// the preorder comparison or the start node if none is.
///
/// Only comparisons using the preorder are done.
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_node_t* TRI_SkipListRightLookup (TRI_skiplist_t *sl, void *doc);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up doc in the skiplist using the proper order
/// comparison. 
///
/// Only comparisons using the proper order are done. Returns NULL
/// if doc is not in the skiplist.
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_node_t* TRI_SkipListLookup (TRI_skiplist_t *sl, void *doc);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

