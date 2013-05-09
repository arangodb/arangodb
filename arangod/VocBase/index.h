////////////////////////////////////////////////////////////////////////////////
/// @brief index
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_VOC_BASE_INDEX_H
#define TRIAGENS_VOC_BASE_INDEX_H 1

#include "VocBase/vocbase.h"

#include "BasicsC/associative-multi.h"
#include "BasicsC/json.h"
#include "BasicsC/linked-list.h"
#include "BitIndexes/bitarrayIndex.h"
#include "FulltextIndex/fulltext-index.h"
#include "GeoIndex/GeoIndex.h"
#include "IndexIterators/index-iterator.h"
#include "IndexOperators/index-operator.h"
#include "PriorityQueue/pqueueindex.h"
#include "ShapedJson/shaped-json.h"
#include "SkipLists/skiplistIndex.h"
#include "SkipListsEx/skiplistExIndex.h"
#include "VocBase/voc-types.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_collection_s;
struct TRI_doc_mptr_s;
struct TRI_shaped_json_s;
struct TRI_document_collection_s;
struct TRI_transaction_collection_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief index identifier
////////////////////////////////////////////////////////////////////////////////

typedef TRI_voc_tick_t TRI_idx_iid_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief index type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_IDX_TYPE_PRIMARY_INDEX,
  TRI_IDX_TYPE_GEO1_INDEX,
  TRI_IDX_TYPE_GEO2_INDEX,
  TRI_IDX_TYPE_HASH_INDEX,
  TRI_IDX_TYPE_EDGE_INDEX,
  TRI_IDX_TYPE_FULLTEXT_INDEX,
  TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX,
  TRI_IDX_TYPE_SKIPLIST_INDEX,
  TRI_IDX_TYPE_SKIPLIST_EX_INDEX,
  TRI_IDX_TYPE_BITARRAY_INDEX,
  TRI_IDX_TYPE_CAP_CONSTRAINT
}
TRI_idx_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index variants
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  INDEX_GEO_NONE = 0,
  INDEX_GEO_INDIVIDUAL_LAT_LON,
  INDEX_GEO_COMBINED_LAT_LON,
  INDEX_GEO_COMBINED_LON_LAT
}
TRI_index_geo_variant_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief index base class
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_index_s {
  TRI_idx_iid_t   _iid;
  TRI_idx_type_e  _type;
  struct TRI_primary_collection_s* _collection;

  TRI_vector_string_t _fields;
  bool _unique;
  bool _ignoreNull;
  bool _needsFullCoverage;

  const char* (*typeName) (struct TRI_index_s const*);
  TRI_json_t* (*json) (struct TRI_index_s*, struct TRI_primary_collection_s const*);
  void (*removeIndex) (struct TRI_index_s*, struct TRI_primary_collection_s*);
  
  // .........................................................................................
  // the following functions are called for transaction management
  // .........................................................................................

  int (*beginTransaction) (struct TRI_index_s*, struct TRI_primary_collection_s*);
  int (*abortTransaction) (struct TRI_index_s*, struct TRI_primary_collection_s*);
  int (*commitTransaction) (struct TRI_index_s*, struct TRI_primary_collection_s*);

  // .........................................................................................
  // the following functions are called for document/collection administration
  // .........................................................................................

  int (*insert) (struct TRI_index_s*, struct TRI_doc_mptr_s const*, const bool);
  int (*remove) (struct TRI_index_s*, struct TRI_doc_mptr_s const*, const bool);

  // NULL by default. will only be called if non-NULL
  int (*postInsert) (struct TRI_transaction_collection_s*, struct TRI_index_s*, struct TRI_doc_mptr_s const*);

  // a garbage collection function for the index
  int (*cleanup) (struct TRI_index_s*);

  // .........................................................................................
  // the following functions are called by the query machinery which attempting to determine an
  // appropriate index and when using the index to obtain a result set.
  // .........................................................................................

  // stores the usefulness of this index for the indicated query in the struct TRI_index_challenge_s
  // returns integer which maps to set of errors.
  // the actual type is:
  // int (*indexQuery) (void*, struct TRI_index_operator_s*, struct TRI_index_challenge_s*, void*);
  // first parameter is the specific index structure, e.g. HashIndex, SkiplistIndex etc
  // fourth parameter is any internal storage which is/will be allocated as a consequence of this call
  TRI_index_query_method_call_t indexQuery;

  // returns the result set in an iterator
  // the actual type is:
  // TRI_index_iterator_t* (*indexQueryResult) (void*, struct TRI_index_operator_s*, void*, bool (*filter) (TRI_index_iterator_t*) );
  // first parameter is the specific index structure, e.g. HashIndex, SkiplistIndex etc
  // third parameter is any internal storage might have been allocated as a consequence of this or the indexQuery call above
  // fourth parameter a filter which the index iterator should apply
  TRI_index_query_result_method_call_t indexQueryResult;

  // during the query or result function call, the index may have created and used
  // additional storage, this method attempts to free this if required.
  // the actual type is:
  // void (*indexQueryFree) (struct TRI_index_s*, void*);
  // second parameter is any internal storage might have been allocated as a consequence of the indexQuery
  // or indexQueryResult calls above
  TRI_index_query_free_method_call_t indexQueryFree;
}
TRI_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_geo_index_s {
  TRI_index_t base;
  TRI_index_geo_variant_e _variant;

  GeoIndex* _geoIndex;

  TRI_shape_pid_t _location;
  TRI_shape_pid_t _latitude;
  TRI_shape_pid_t _longitude;

  bool _geoJson;
  bool _constraint;
}
TRI_geo_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_edge_index_s {
  TRI_index_t base;

  TRI_multi_pointer_t _edges;
}
TRI_edge_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief skiplist index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_priorityqueue_index_s {
  TRI_index_t base;
  PQIndex* _pqIndex;
  TRI_vector_t _paths; // a list of shape pid which identifies the fields of the index
}
TRI_priorityqueue_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief skiplist index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplist_index_s {
  TRI_index_t base;

  SkiplistIndex* _skiplistIndex;  // effectively the skiplist
  TRI_vector_t _paths;            // a list of shape pid which identifies the fields of the index
}
TRI_skiplist_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief skiplistEx index supports transactions
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplistEx_index_s {
  TRI_index_t base;

  SkiplistExIndex* _skiplistExIndex;  // effectively the skiplist
  TRI_vector_t _paths;                // a list of shape pid which identifies the fields of the index
}
TRI_skiplistEx_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_fulltext_index_s {
  TRI_index_t base;

  TRI_fts_index_t* _fulltextIndex;
  TRI_shape_pid_t _attribute;
  int _minWordLength;

  bool _indexSubstrings;
}
TRI_fulltext_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief cap constraint
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_cap_constraint_s {
  TRI_index_t base;

  TRI_linked_array_t _array;
  size_t _size;
}
TRI_cap_constraint_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief bitarray index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_bitarray_index_s {
  TRI_index_t base;

  BitarrayIndex* _bitarrayIndex;
  TRI_vector_t _paths;            // a list of shape pid which identifies the fields of the index
  TRI_vector_t _values;           // a list of json objects which match the list of attributes used by the index
  bool _supportUndef;             // allows documents which do not match the attribute list to be indexed
}
TRI_bitarray_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief index query result
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_index_result_s {
  size_t _length;
  struct TRI_doc_mptr_s** _documents;     // simple list of elements
}
TRI_index_result_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief index query parameter
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_index_search_value_s {
  size_t _length;
  TRI_shaped_json_t* _values;
}
TRI_index_search_value_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                             INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise basic index properties
////////////////////////////////////////////////////////////////////////////////

void TRI_InitIndex (TRI_index_t*, 
                    const TRI_idx_type_e, 
                    struct TRI_primary_collection_s*,
                    bool,
                    bool);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief free an index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeIndex (TRI_index_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an index file
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveIndexFile (struct TRI_primary_collection_s* collection, TRI_index_t* idx);

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an index
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveIndex (struct TRI_primary_collection_s*, TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index identifier
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupIndex (struct TRI_primary_collection_s*, TRI_idx_iid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a basic index description as JSON
/// this only contains the common index fields and needs to be extended by the
/// specialised index 
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonIndex (TRI_memory_zone_t*, TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a result set returned by a hash index query
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyIndexResult (TRI_index_result_t* result);

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a path vector
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyPathVector (TRI_vector_t* dst, TRI_vector_t* src);

////////////////////////////////////////////////////////////////////////////////
/// @brief copies all pointers from a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyFieldsVector (TRI_vector_string_t* dst, TRI_vector_pointer_t* src);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a path vector into a field list
///
/// Note that you must free the field list itself, but not the fields. The
/// belong to the shaper.
////////////////////////////////////////////////////////////////////////////////

char const** TRI_FieldListByPathList (TRI_shaper_t* shaper, 
                                      TRI_vector_t* paths);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     PRIMARY INDEX
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create the primary index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreatePrimaryIndex (struct TRI_primary_collection_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a primary index, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPrimaryIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a primary index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePrimaryIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        EDGE INDEX
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create the edge index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateEdgeIndex (struct TRI_primary_collection_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an edge index, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyEdgeIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free an edge index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeEdgeIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              PRIORITY QUEUE INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

PQIndexElements* TRI_LookupPriorityQueueIndex (TRI_index_t*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a priority queue index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreatePriorityQueueIndex (struct TRI_primary_collection_s*,
                                           TRI_vector_pointer_t*,
                                           TRI_vector_t*,
                                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPriorityQueueIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePriorityQueueIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    SKIPLIST INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_iterator_t* TRI_LookupSkiplistIndex (TRI_index_t*, TRI_index_operator_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a skiplist index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateSkiplistIndex (struct TRI_primary_collection_s*,
                                      TRI_vector_pointer_t* fields,
                                      TRI_vector_t* paths,
                                      bool unique);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkiplistIndex (TRI_index_t* idx);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkiplistIndex (TRI_index_t* idx);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    FULLTEXT INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

struct TRI_doc_mptr_s** TRI_LookupFulltextIndex (TRI_index_t*, const char* query);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a fulltext index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateFulltextIndex (struct TRI_primary_collection_s*,
                                      const char*,
                                      const bool,
                                      int);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyFulltextIndex (TRI_index_t* idx);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeFulltextIndex (TRI_index_t* idx);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    BITARRAY INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an iterator for a lookup query
////////////////////////////////////////////////////////////////////////////////

TRI_index_iterator_t* TRI_LookupBitarrayIndex (TRI_index_t*, TRI_index_operator_t*, bool (*filter) (TRI_index_iterator_t*) );


////////////////////////////////////////////////////////////////////////////////
/// @brief creates a bitarray index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateBitarrayIndex (struct TRI_primary_collection_s*,
                                      TRI_vector_pointer_t*,
                                      TRI_vector_t*,
                                      TRI_vector_pointer_t*,
                                      bool,
                                      int*,
                                      char**);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyBitarrayIndex (TRI_index_t* idx);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBitarrayIndex (TRI_index_t* idx);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
