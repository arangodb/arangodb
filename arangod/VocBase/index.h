////////////////////////////////////////////////////////////////////////////////
/// @brief index
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_INDEX_H
#define ARANGODB_VOC_BASE_INDEX_H 1

#include "Basics/Common.h"

#include "VocBase/vocbase.h"

#include "Basics/associative-multi.h"
#include "Basics/json.h"
#include "FulltextIndex/fulltext-index.h"
#include "GeoIndex/GeoIndex.h"
#include "IndexOperators/index-operator.h"
#include "ShapedJson/shaped-json.h"
#include "SkipLists/skiplistIndex.h"
#include "VocBase/voc-types.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_collection_t;
struct TRI_doc_mptr_t;
struct TRI_shaped_json_s;
struct TRI_document_collection_t;
struct TRI_transaction_collection_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief index type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_IDX_TYPE_UNKNOWN = 0,
  TRI_IDX_TYPE_PRIMARY_INDEX,
  TRI_IDX_TYPE_GEO1_INDEX,
  TRI_IDX_TYPE_GEO2_INDEX,
  TRI_IDX_TYPE_HASH_INDEX,
  TRI_IDX_TYPE_EDGE_INDEX,
  TRI_IDX_TYPE_FULLTEXT_INDEX,
  TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX, // DEPRECATED and not functional anymore
  TRI_IDX_TYPE_SKIPLIST_INDEX,
  TRI_IDX_TYPE_BITARRAY_INDEX,       // DEPRECATED and not functional anymore
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
  struct TRI_document_collection_t* _collection;

  TRI_vector_string_t _fields;
  bool _unique;
  bool _ignoreNull;
  bool _sparse;

  size_t (*memory) (struct TRI_index_s const*);
  TRI_json_t* (*json) (struct TRI_index_s const*);
  void (*removeIndex) (struct TRI_index_s*, struct TRI_document_collection_t*);

  // .........................................................................................
  // the following functions are called for document/collection administration
  // .........................................................................................

  int (*insert) (struct TRI_index_s*, struct TRI_doc_mptr_t const*, bool);
  int (*remove) (struct TRI_index_s*, struct TRI_doc_mptr_t const*, bool);

  // NULL by default. will only be called if non-NULL
  int (*postInsert) (struct TRI_transaction_collection_s*, struct TRI_index_s*, struct TRI_doc_mptr_t const*);

  // a garbage collection function for the index
  int (*cleanup) (struct TRI_index_s*);

  // give index a hint about the expected size
  int (*sizeHint) (struct TRI_index_s*, size_t);

  // .........................................................................................
  // the following functions are called by the query machinery which attempting to determine an
  // appropriate index and when using the index to obtain a result set.
  // .........................................................................................
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

  TRI_multi_pointer_t _edges_from;
  TRI_multi_pointer_t _edges_to;
}
TRI_edge_index_t;

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

  size_t _count;
  int64_t _size;
}
TRI_cap_constraint_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief index query result
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_index_result_s {
  size_t _length;
  struct TRI_doc_mptr_t** _documents;     // simple list of elements
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

// -----------------------------------------------------------------------------
// --SECTION--                                                             INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise basic index properties
////////////////////////////////////////////////////////////////////////////////

void TRI_InitIndex (TRI_index_t*,
                    TRI_idx_iid_t,
                    TRI_idx_type_e,
                    struct TRI_document_collection_t*,
                    bool,   // unique
                    bool);  // sparse

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not an index needs full coverage
////////////////////////////////////////////////////////////////////////////////

bool TRI_NeedsFullCoverageIndex (TRI_idx_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name of an index type
////////////////////////////////////////////////////////////////////////////////

TRI_idx_type_e TRI_TypeIndex (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name of an index type
////////////////////////////////////////////////////////////////////////////////

char const* TRI_TypeNameIndex (TRI_idx_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief validate index id string
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValidateIdIndex (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief validate an index id (collection name + / + index id)
////////////////////////////////////////////////////////////////////////////////

bool TRI_ValidateIndexIdIndex (char const*,
                               size_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free an index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an index file
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveIndexFile (struct TRI_document_collection_t*,
                          TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an index
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveIndex (struct TRI_document_collection_t*,
                   TRI_index_t*,
                   bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index identifier
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupIndex (struct TRI_document_collection_t*,
                              TRI_idx_iid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a basic index description as JSON
/// this only contains the common index fields and needs to be extended by the
/// specialised index
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonIndex (TRI_memory_zone_t*,
                           TRI_index_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a result set returned by a hash index query
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyIndexResult (TRI_index_result_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a path vector
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyPathVector (TRI_vector_t*,
                         TRI_vector_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief copies all pointers from a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyFieldsVector (TRI_vector_string_t*,
                           TRI_vector_pointer_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a path vector into a field list
///
/// Note that you must free the field list itself, but not the fields. The
/// belong to the shaper.
////////////////////////////////////////////////////////////////////////////////

char const** TRI_FieldListByPathList (TRI_shaper_t const*,
                                      TRI_vector_t const*);

// -----------------------------------------------------------------------------
// --SECTION--                                                     PRIMARY INDEX
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the primary index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreatePrimaryIndex (struct TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a primary index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePrimaryIndex (TRI_index_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                        EDGE INDEX
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the edge index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateEdgeIndex (struct TRI_document_collection_t*,
                                  TRI_idx_iid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an edge index, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyEdgeIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free an edge index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeEdgeIndex (TRI_index_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                    SKIPLIST INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

TRI_skiplist_iterator_t* TRI_LookupSkiplistIndex (TRI_index_t*,
                                                  TRI_index_operator_t*,
                                                  bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a skiplist index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateSkiplistIndex (struct TRI_document_collection_t*,
                                      TRI_idx_iid_t,
                                      TRI_vector_pointer_t*,
                                      TRI_vector_t*,
                                      bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkiplistIndex (TRI_index_t* idx);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkiplistIndex (TRI_index_t* idx);

// -----------------------------------------------------------------------------
// --SECTION--                                                    FULLTEXT INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

struct TRI_doc_mptr_t** TRI_LookupFulltextIndex (TRI_index_t*,
                                                 const char*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a fulltext index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateFulltextIndex (struct TRI_document_collection_t*,
                                      TRI_idx_iid_t,
                                      const char*,
                                      const bool,
                                      int);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyFulltextIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeFulltextIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief index comparator, used by the coordinator to detect if two index
/// contents are the same
////////////////////////////////////////////////////////////////////////////////

bool IndexComparator (TRI_json_t const* lhs, TRI_json_t const* rhs);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
