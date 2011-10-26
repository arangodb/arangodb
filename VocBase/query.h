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
/// @author Copyright 2011-2010, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOCBASE_QUERY_H
#define TRIAGENS_DURHAM_VOCBASE_QUERY_H 1

#include <VocBase/vocbase.h>

#include <VocBase/result-set.h>

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @page DurhamFluentInterface Fluent Interface
///
/// A fluent interface is implemented by using method chaining to relay the
/// instruction context of a subsequent call.
///
/// The Durham Storage Engine provides the following methods:
///
/// - selection by example
/// - field selection aka projection
/// - @ref GeoFI "geo coordinates"
/// - sorting
/// - pagination of the result using @ref LimitFI "limit", @ref SkipFI "skip",
///     and @ref CountFI "count".
///
/// @section FirstStepsFI First Steps
///
/// For instance, in order to select all elements of a collection "examples",
/// one can use
///
/// @verbinclude fluent1
///
/// This will select all documents and prints the first 20 documents. If there
/// are more than 20 documents, then "...more results..." is printed and you
/// can use the variable "it" to access the next 20 document.
///
/// @verbinclude fluent2
///
/// In the above examples "db.examples.select()" defines a query. Printing
/// that query, executes the query and returns a cursor to the result set.
/// The first 20 documents are printed and the query (resp. cursor) is
/// assigned to the variable "it".
///
/// A cursor can also be query using "hasNext()" and "next()".
///
/// @verbinclude fluent3
///
/// @section GeoFI Geo Coordinates
///
/// The Storage Engine allows to selects documents based on geographic
/// coordinates. In order for this to work a geo index must be defined.  This
/// index will a very elaborate index to lookup neighbours that is magnitudes
/// faster than a simple R* index.
///
/// In generale a geo coordinate is a pair of longitude and latitude.  This can
/// either be an list with two elements like "[ -10, +30 ]" or an object like "{
/// lon: -10, lat: +30 }". In order to find all documents within a given radius
/// around a coordinate use the @ref WithinFI "within()" operator. In order to
/// find all documents near a given document use the @ref NearFI "near()"
/// operator.
///
/// @section WithinFI The Within Operator
///
/// @section NearFI The Near Operator
///
/// Assume that the geo coordinate is stored as list of size 2. The first
/// value being the latitude and the second value being the longitude.
///
/// @section LimitFI The Limit Operator
///
/// If, for example, you display the result of a user search, then you are in
/// general not interested in the completed result set, but only the first 10
/// documents. In this case, you can the "limit()" operator. This operators
/// works like LIMIT in MySQL, it specifies a maximal number of documents to
/// return.
///
/// @verbinclude fluent4
///
/// Specifying a limit of "0" returns no documents at all. If you do not need
/// a limit, just do not add the limit operator. If you specifiy a negtive
/// limit of -n, this will return the last n documents instead.
///
/// @verbinclude fluent8
///
/// @section SkipFI The Skip Operator
///
/// "skip" used together with "limit" can be used to implement pagination.
/// The "skip" operator skips over the first n documents. So, in order to
/// create result pages with 10 result documents per page, you can use
/// "skip(n * 10).limit(10)" to access the n.th page.
///
/// @verbinclude fluent5
///
/// @section CountFI The Count Operator
///
/// If you are implementation pagination, then you need the total amount of
/// documents a query returned. "count()" just does this. It returns the
/// total amount of documents regardless of any "limit()" and "skip()"
/// operator used before the "count()". If you really want these to be taken
/// into account, use "count(true)".
///
/// @verbinclude fluent6
///
/// @section ExplainFI The Explain Operator
///
/// In order to optimise queries you need to know how the storage engine
/// executed that type of query. You can use the "explain()" operator to
/// see how a query was executed.
///
/// @verbinclude fluent9
///
/// The "explain()" operator returns an object with the following attributes.
///
/// - cursor: describes how the result set was computed.
/// - scannedIndexEntries: how many index entries were scanned
/// - scannedDocuments: how many documents were scanned
/// - matchedDocuments: the sum of all matched documents in each step
/// - runtime: the runtime in seconds
///
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_rs_container_s;
struct TRI_index_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief query type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_QUE_TYPE_COLLECTION,
  TRI_QUE_TYPE_DOCUMENT,
  TRI_QUE_TYPE_LIMIT,
  TRI_QUE_TYPE_NEAR,
  TRI_QUE_TYPE_SKIP
}
TRI_que_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_s {
  TRI_que_type_e _type;
  struct TRI_doc_collection_s* _collection;
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
/// @brief document query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_document_query_s {
  TRI_query_t base;

  TRI_query_t* _operand;
  TRI_voc_did_t _did;
}
TRI_document_query_t;

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

  struct TRI_index_s* _index;
  double _latitude;
  double _longitude;
  TRI_voc_size_t _count;
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a full scan of a collection
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateCollectionQuery (struct TRI_doc_collection_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateDocumentQuery (TRI_query_t*, TRI_voc_did_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief limits an existing query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateLimitQuery (TRI_query_t*, TRI_voc_ssize_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up documents near a given point
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateNearQuery (TRI_query_t*,
                                  char const* location,
                                  double latitude,
                                  double longitude,
                                  TRI_voc_size_t count,
                                  char const** error);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up documents near a given point
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateNear2Query (TRI_query_t*,
                                   char const* locLatitude,
                                   double latitude,
                                   char const* locLongitude,
                                   double longitude,
                                   TRI_voc_size_t count,
                                   char const** error);

////////////////////////////////////////////////////////////////////////////////
/// @brief skips elements of an existing query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateSkipQuery (TRI_query_t*, TRI_voc_size_t);

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
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
