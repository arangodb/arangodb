////////////////////////////////////////////////////////////////////////////////
/// @brief query
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_VOC_BASE_QUERY_H
#define TRIAGENS_VOC_BASE_QUERY_H 1

#include <VocBase/vocbase.h>

#include <Basics/json.h>
#include <Basics/string-buffer.h>
#include <VocBase/result-set.h>
#include <VocBase/simple-collection.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_rs_container_s;
struct TRI_index_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief query type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_QUE_TYPE_COLLECTION,
  TRI_QUE_TYPE_DISTANCE,
  TRI_QUE_TYPE_DOCUMENT,
  TRI_QUE_TYPE_EDGES,
  TRI_QUE_TYPE_GEO_INDEX,
  TRI_QUE_TYPE_LIMIT,
  TRI_QUE_TYPE_NEAR,
  TRI_QUE_TYPE_SKIP,
  TRI_QUE_TYPE_WITHIN
}
TRI_que_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief query result
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_result_s {
  char* _cursor;
  char* _error;

  TRI_voc_size_t _length;
  TRI_voc_size_t _total;

  TRI_voc_size_t _scannedIndexEntries;
  TRI_voc_size_t _scannedDocuments;
  TRI_voc_size_t _matchedDocuments;

  struct TRI_doc_mptr_s const** _documents;
  struct TRI_json_s* _augmented;
}
TRI_query_result_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief query
///
/// A query contains the following methods.
///
/// <b><tt>char* optimise (TRI_query_t**)</tt></b>
///
/// Checks and optimises the query. This might change the pointer. If a new
/// query is create, the old query is freed. In case of an error, an error
/// message is returned.
///
/// <b><tt>void execute (TRI_query_t*, TRI_query_result_t*)</tt></b>
///
/// Executes the query and stores the result set in a @c TRI_query_result_t.
/// You must call @c optimise before calling @c execute.  In case of an error
/// during the executing, the error message is stored in the field @c
/// _error. The query result must be clear using the free method of the @c
/// TRI_query_result_t.
///
/// <b><tt>TRI_json_t* json (TRI_query_t*)</tt></b>
///
/// Returns a json description.
///
/// <b><tt>void stringify (TRI_query_t*, TRI_string_buffer_t*)</tt></b>
///
/// Appends a string representation to the buffer.
///
/// <b><tt>TRI_query_t* clone (TRI_query_t*)</tt></b>
///
/// Clones an existing query.
///
/// <b><tt>void free (TRI_query_t*, bool)</tt></b>
///
/// Frees all resources associated with the query. If the second parameter is
/// true, also free all operands. This method will not clear any resources
/// allocated during an @c execute associated with the @c TRI_query_result_t.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_s {
  TRI_que_type_e _type;
  TRI_vocbase_col_t const* _collection;
  bool _isOptimised;

  char* (*optimise) (struct TRI_query_s**);
  void (*execute) (struct TRI_query_s*, TRI_query_result_t*);
  TRI_json_t* (*json) (struct TRI_query_s*);
  void (*stringify) (struct TRI_query_s*, TRI_string_buffer_t*);
  struct TRI_query_s* (*clone) (struct TRI_query_s*);
  void (*free) (struct TRI_query_s*, bool);
}
TRI_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief full collection query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_collection_query_s {
  TRI_query_t base;
}
TRI_collection_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief distance query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_distance_query_s {
  TRI_query_t base;

  TRI_query_t* _operand;
  char* _distance;
}
TRI_distance_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_document_query_s {
  TRI_query_t base;

  TRI_query_t* _operand;
  TRI_voc_did_t _did;
}
TRI_document_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief edges query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_edges_query_s {
  TRI_query_t base;

  TRI_query_t* _operand;

  TRI_edge_direction_e _direction;
}
TRI_edges_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_geo_index_query_s {
  TRI_query_t base;

  TRI_query_t* _operand;

  char* _location;
  char* _latitude;
  char * _longitude;

  bool _geoJson;
}
TRI_geo_index_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief limit query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_limit_query_s {
  TRI_query_t base;

  TRI_query_t* _operand;
  TRI_voc_ssize_t _limit;
}
TRI_limit_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief near query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_near_query_s {
  TRI_query_t base;

  TRI_query_t* _operand;

  char* _distance;

  double _latitude;
  double _longitude;

  TRI_voc_ssize_t _limit;
}
TRI_near_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief skip query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skip_query_s {
  TRI_query_t base;

  TRI_query_t* _operand;
  TRI_voc_size_t _skip;
}
TRI_skip_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief within query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_within_query_s {
  TRI_query_t base;

  TRI_query_t* _operand;

  char* _distance;

  double _latitude;
  double _longitude;
  double _radius;
}
TRI_within_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a full scan of a collection
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateCollectionQuery (TRI_vocbase_col_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the distance to a query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateDistanceQuery (TRI_query_t*, char const* name);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateDocumentQuery (TRI_query_t*, TRI_voc_did_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateEdgesQuery (TRI_vocbase_col_t const* edges,
                                   TRI_query_t*,
                                   TRI_edge_direction_e direction);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an geo-spatial index
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateGeoIndexQuery (TRI_query_t*,
                                      char const* location,
                                      char const* latitude,
                                      char const* longitude,
                                      bool geoJson);

////////////////////////////////////////////////////////////////////////////////
/// @brief limits an existing query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateLimitQuery (TRI_query_t*, TRI_voc_ssize_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up documents near a given point
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateNearQuery (TRI_query_t*,
                                  double latitude,
                                  double longitude,
                                  char const* distance);

////////////////////////////////////////////////////////////////////////////////
/// @brief skips elements of an existing query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateSkipQuery (TRI_query_t*, TRI_voc_size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up documents within a given radius
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateWithinQuery (TRI_query_t*,
                                    double latitude,
                                    double longitude,
                                    double radius,
                                    char const* distance);

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
/// @brief executes a query
////////////////////////////////////////////////////////////////////////////////

TRI_result_set_t* TRI_ExecuteQuery (TRI_query_t*);

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
