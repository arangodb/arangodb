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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "query.h"

#include <Basics/logging.h>
#include <Basics/strings.h>
#include <Basics/string-buffer.h>
#include <Basics/conversions.h>
#include <VocBase/simple-collection.h>
#include <VocBase/index.h>

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the old storage of a TRI_query_result_t
////////////////////////////////////////////////////////////////////////////////

static void FreeDocuments (TRI_query_result_t* result) {
  if (result->_documents != NULL) {
    TRI_Free(result->_documents);
    result->_documents = NULL;
  }

  if (result->_augmented != NULL) {
    TRI_Free(result->_augmented);
    result->_augmented = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  COLLECTION QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a full scan
////////////////////////////////////////////////////////////////////////////////

static void ExecuteCollectionQuery (TRI_query_t* qry, TRI_query_result_t* result) {
  TRI_document_query_t* query;
  TRI_doc_mptr_t const** qtr;
  TRI_sim_collection_t* collection;
  void** end;
  void** ptr;

  // extract query
  query = (TRI_document_query_t*) qry;

  // sanity check
  if (! query->base._collection->_loaded) {
    return;
  }

  if (query->base._collection->_collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return;
  }

  collection = (TRI_sim_collection_t*) query->base._collection->_collection;

  // append information about the execution plan
  TRI_AppendString(&result->_cursor, ">collection");

  // free any old storage
  FreeDocuments(result);

  // add a new document list and copy all documents
  result->_total = result->_length = collection->_primaryIndex._nrUsed;

  if (result->_length == 0) {
    return;
  }

  result->_documents = (qtr = TRI_Allocate(sizeof(TRI_doc_mptr_t*) * (result->_length)));

  ptr = collection->_primaryIndex._table;
  end = collection->_primaryIndex._table + collection->_primaryIndex._nrAlloc;

  for (;  ptr < end;  ++ptr) {
    ++result->_scannedDocuments;

    if (*ptr) {
      ++result->_matchedDocuments;
      *qtr++ = *ptr;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a full scan
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneCollectionQuery (TRI_query_t* query) {
  return TRI_CreateCollectionQuery(query->_collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a full scan
////////////////////////////////////////////////////////////////////////////////

static void FreeCollectionQuery (TRI_query_t* query) {
  TRI_Free(query->_string);
  TRI_Free(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    DOCUMENT QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a document lookup as primary index lookup
////////////////////////////////////////////////////////////////////////////////

static void ExecuteDocumentQueryPrimary (TRI_document_query_t* query,
                                         TRI_vocbase_col_t const* collection,
                                         TRI_query_result_t* result) {
  TRI_doc_mptr_t const* document;

  // sanity check
  if (! query->base._collection->_loaded) {
    return;
  }

  // look up the document
  document = collection->_collection->read(collection->_collection, query->_did);

  // append information about the execution plan
  TRI_AppendString(&result->_cursor, ">primary");

  ++result->_scannedIndexEntries;

  // free any old storage
  FreeDocuments(result);

  // the document is the result
  if (document == NULL) {
    result->_total = result->_length = 0;
  }
  else {
    result->_total = result->_length = 1;

    ++result->_matchedDocuments;

    result->_documents = TRI_Allocate(sizeof(TRI_doc_mptr_t*));
    *result->_documents = document;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a document lookup as full scan
////////////////////////////////////////////////////////////////////////////////

static void ExecuteDocumentQueryFullScan (TRI_document_query_t* query,
                                          TRI_query_result_t* result) {
  TRI_doc_mptr_t const** ptr;
  TRI_doc_mptr_t const** end;
  TRI_json_t aug;
  size_t pos;

  // execute the sub-query
  query->_operand->execute(query->_operand, result);

  if (result->_error) {
    return;
  }

  // append information about the execution plan
  TRI_AppendString(&result->_cursor, ">full-scan[document]");

  // without any document there will be no match
  if (result->_length == 0) {
    return;
  }

  // do a full scan
  pos = 0;
  ptr = result->_documents;
  end = result->_documents + result->_length;

  for (;  ptr < end;  ++ptr, ++pos) {
    ++result->_scannedDocuments;

    // found a match, create new result
    if ((*ptr)->_did == query->_did) {
      ++result->_matchedDocuments;

      if (result->_length == 1) {
        return;
      }

      TRI_Free(result->_documents);
      result->_documents = TRI_Allocate(sizeof(TRI_doc_mptr_t*));

      result->_total = result->_length = 1;
      *result->_documents = *ptr;

      if (result->_augmented != NULL) {
        aug = result->_augmented[pos];

        TRI_Free(result->_augmented);
        result->_augmented = TRI_Allocate(sizeof(TRI_json_t));

        result->_augmented[0] = aug;
      }

      return;
    }
  }

  // nothing found
  result->_total = result->_length = 0;

  FreeDocuments(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a document lookup
////////////////////////////////////////////////////////////////////////////////

static void ExecuteDocumentQuery (TRI_query_t* qry,
                                  TRI_query_result_t* result) {
  TRI_document_query_t* query;

  query = (TRI_document_query_t*) qry;

  if (query->_operand != NULL && query->_operand->_type == TRI_QUE_TYPE_COLLECTION) {
    ExecuteDocumentQueryPrimary(query, query->_operand->_collection, result);
  }
  else {
    ExecuteDocumentQueryFullScan(query, result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a document lookup
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneDocumentQuery (TRI_query_t* qry) {
  TRI_document_query_t* query;

  query = (TRI_document_query_t*) qry;

  return TRI_CreateDocumentQuery(query->_operand->clone(query->_operand), query->_did);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a document lookup
////////////////////////////////////////////////////////////////////////////////

static void FreeDocumentQuery (TRI_query_t* qry) {
  TRI_document_query_t* query;

  query = (TRI_document_query_t*) qry;

  query->_operand->free(query->_operand);

  TRI_Free(query->base._string);
  TRI_Free(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   GEO INDEX QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a geo-spatial index
////////////////////////////////////////////////////////////////////////////////

static void ExecuteGeoIndexQuery (TRI_query_t* qry,
                                  TRI_query_result_t* result) {
  TRI_geo_index_query_t* query;

  query = (TRI_geo_index_query_t*) qry;

  query->_operand->execute(query->_operand, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a geo-spatial index
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneGeoIndexQuery (TRI_query_t* qry) {
  TRI_geo_index_query_t* query;

  query = (TRI_geo_index_query_t*) qry;

  return TRI_CreateGeoIndexQuery(query->_operand->clone(query->_operand),
                                 query->_location,
                                 query->_latitude,
                                 query->_longitude);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a geo-spatial index
////////////////////////////////////////////////////////////////////////////////

static void FreeGeoIndeyQuery (TRI_query_t* qry) {
  TRI_geo_index_query_t* query;

  query = (TRI_geo_index_query_t*) qry;

  query->_operand->free(query->_operand);

  if (query->_location != NULL) { TRI_Free(query->_location); }
  if (query->_latitude != NULL) { TRI_Free(query->_latitude); }
  if (query->_longitude != NULL) { TRI_Free(query->_longitude); }

  TRI_Free(query->base._string);
  TRI_Free(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       LIMIT QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a limit query
////////////////////////////////////////////////////////////////////////////////

static void ExecuteLimitQuery (TRI_query_t* qry,
                               TRI_query_result_t* result) {
  TRI_doc_mptr_t const** copy;
  TRI_json_t* aug = NULL;
  TRI_limit_query_t* query;
  TRI_voc_size_t limit;
  TRI_voc_size_t offset;

  query = (TRI_limit_query_t*) qry;

  // execute the sub-query
  query->_operand->execute(query->_operand, result);

  if (result->_error) {
    return;
  }

  // append information about the execution plan
  TRI_AppendString(&result->_cursor, ">limit");

  // without any documents there will be no limit
  if (result->_length == 0) {
    return;
  }

  // get the last "limit" documents
  if (query->_limit < 0) {
    limit = -query->_limit;

    if (result->_length < limit) {
      offset = 0;
    }
    else {
      offset = result->_length - limit;
    }
  }

  // get the first "limit" documents
  else {
    limit = query->_limit;
    offset = 0;
  }

  // not enough documents to limit query
  if (result->_length <= limit) {
    result->_matchedDocuments += result->_length;
    return;
  }

  // shorten result list
  result->_matchedDocuments += limit;

  copy = TRI_Allocate(sizeof(TRI_doc_mptr_t*) * limit);
  memcpy(copy, result->_documents + offset, sizeof(TRI_doc_mptr_t*) * limit);

  if (result->_augmented != NULL) {
    aug = TRI_Allocate(sizeof(TRI_json_t) * limit);
    memcpy(aug, result->_augmented + offset, sizeof(TRI_json_t) * limit);
  }

  result->_length = limit;

  TRI_Free(result->_documents);
  result->_documents = copy;

  if (result->_augmented != NULL) {
    TRI_Free(result->_augmented);
    result->_augmented = aug;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a limit query
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneLimitQuery (TRI_query_t* qry) {
  TRI_limit_query_t* query;

  query = (TRI_limit_query_t*) qry;

  return TRI_CreateLimitQuery(query->_operand->clone(query->_operand), query->_limit);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a limit query
////////////////////////////////////////////////////////////////////////////////

static void FreeLimitQuery (TRI_query_t* qry) {
  TRI_limit_query_t* query;

  query = (TRI_limit_query_t*) qry;

  query->_operand->free(query->_operand);

  TRI_Free(query->base._string);
  TRI_Free(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        NEAR QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  double _distance;
  void const* _data;
}
geo_coordinate_distance_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief locates a suitable geo index
////////////////////////////////////////////////////////////////////////////////

static TRI_geo_index_t* LocateGeoIndex (TRI_query_t* query) {
  TRI_sim_collection_t* collection;

  // sanity check
  if (! query->_collection->_loaded) {
    return NULL;
  }

  if (query->_collection->_collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return NULL;
  }

  collection = (TRI_sim_collection_t*) query->_collection->_collection;

  // return the geo index
  return collection->_geoIndex;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sorts geo coordinates
////////////////////////////////////////////////////////////////////////////////

static int CompareGeoCoordinateDistance (geo_coordinate_distance_t* left, geo_coordinate_distance_t* right) {
  if (left->_distance < right->_distance) {
    return -1;
  }
  else if (left->_distance > right->_distance) {
    return 1;
  }
  else {
    return 0;
  }
}

#define FSRT_NAME SortGeoCoordinates
#define FSRT_TYPE geo_coordinate_distance_t

#define FSRT_COMP(l,r,s) CompareGeoCoordinateDistance(l,r)

uint32_t FSRT_Rand = 0;

static uint32_t RandomGeoCoordinateDistance (void) {
  return (FSRT_Rand = FSRT_Rand * 31415 + 27818);
}

#define FSRT__RAND \
  ((fs_b) + FSRT__UNIT * (RandomGeoCoordinateDistance() % FSRT__DIST(fs_e,fs_b,FSRT__SIZE)))

#include <Basics/fsrt.inc>

////////////////////////////////////////////////////////////////////////////////
/// @brief uses a specific geo-spatial index
////////////////////////////////////////////////////////////////////////////////

static TRI_geo_index_t* LookupGeoIndex (TRI_geo_index_query_t* query) {
  TRI_doc_collection_t* collection;
  TRI_index_t* idx;
  TRI_shape_pid_t lat = 0;
  TRI_shape_pid_t loc = 0;
  TRI_shape_pid_t lon = 0;
  TRI_shaper_t* shaper;
  TRI_sim_collection_t* sim;

  // lookup an geo index
  collection = query->base._collection->_collection;

  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    LOG_ERROR("cannot handle collection type %ld", collection->base._type);
    return NULL;
  }

  sim = (TRI_sim_collection_t*) collection;

  shaper = query->base._collection->_collection->_shaper;

  if (query->_location != NULL) {
    loc = shaper->findAttributePathByName(shaper, query->_location);
  }

  if (query->_latitude != NULL) {
    lat = shaper->findAttributePathByName(shaper, query->_latitude);
  }

  if (query->_longitude != NULL) {
    lon = shaper->findAttributePathByName(shaper, query->_longitude);
  }

  if (query->_location != NULL) {
    idx = TRI_LookupGeoIndexSimCollection(sim, loc);
  }
  else if (query->_longitude != NULL && query->_latitude != NULL) {
    idx = TRI_LookupGeoIndex2SimCollection(sim, lat, lon);
  }
  else {
    idx = NULL;
  }

  return (TRI_geo_index_t*) idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create result
////////////////////////////////////////////////////////////////////////////////

static void GeoResult (GeoCoordinates* cors,
                       char const* distance,
                       TRI_query_result_t* result) {
  GeoCoordinate* end;
  GeoCoordinate* ptr;
  TRI_doc_mptr_t const** wtr;
  double* dtr;
  geo_coordinate_distance_t* gnd;
  geo_coordinate_distance_t* gtr;
  geo_coordinate_distance_t* tmp;
  size_t n;

  if (cors == NULL) {
    result->_error = TRI_DuplicateString("cannot execute geo index search");
    return;
  }

  // sort the result
  n = cors->length;

  gtr = (tmp = TRI_Allocate(sizeof(geo_coordinate_distance_t) * n));
  gnd = tmp + n;

  ptr = cors->coordinates;
  end = cors->coordinates + n;

  dtr = cors->distances;

  for (;  ptr < end;  ++ptr, ++dtr, ++gtr) {
    gtr->_distance = *dtr;
    gtr->_data = ptr->data;
  }

  GeoIndex_CoordinatesFree(cors);

  SortGeoCoordinates(tmp, gnd);

  // copy the documents
  result->_documents = (TRI_doc_mptr_t const**) (wtr = TRI_Allocate(sizeof(TRI_doc_mptr_t*) * n));

  for (gtr = tmp;  gtr < gnd;  ++gtr, ++wtr) {
    *wtr = gtr->_data;
  }

  // copy the distance
  if (distance != NULL) {
    TRI_json_t* atr;

    result->_augmented = (atr = TRI_Allocate(sizeof(TRI_json_t) * n));

    for (gtr = tmp;  gtr < gnd;  ++gtr, ++atr) {
      atr->_type = TRI_JSON_ARRAY;
      TRI_InitVector(&atr->_value._objects, sizeof(TRI_json_t));

      TRI_InsertArrayJson(atr, distance, TRI_CreateNumberJson(gtr->_distance));
    }
  }

  TRI_Free(tmp);

  result->_total = result->_length = n;
  result->_scannedIndexEntries += n;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a near query
////////////////////////////////////////////////////////////////////////////////

static void ExecuteNearQuery (TRI_query_t* qry,
                              TRI_query_result_t* result) {
  GeoCoordinates* cors;
  TRI_geo_index_t* idx;
  TRI_near_query_t* query;
  TRI_query_t* operand;

  query = (TRI_near_query_t*) qry;
  operand = query->_operand;

  // free any old results
  FreeDocuments(result);

  // we need a suitable geo index
  if (operand->_type == TRI_QUE_TYPE_GEO_INDEX) {
    TRI_geo_index_query_t* geo;

    geo = (TRI_geo_index_query_t*) operand;

    idx = LookupGeoIndex(geo);
    operand = geo->_operand;
  }
  else {
    idx = LocateGeoIndex(&query->base);
  }

  if (idx == NULL) {
    result->_error = TRI_DuplicateString("cannot locate suitable geo index");
    return;
  }

  // sub-query must be the collection
  if (operand->_type != TRI_QUE_TYPE_COLLECTION) {
    result->_error = TRI_DuplicateString("near query can only be executed for the complete collection");
    return;
  }

  // append information about the execution plan
  TRI_AppendString(&result->_cursor, ">near");

  // execute geo search
  cors = TRI_NearestGeoIndex(&idx->base, query->_latitude, query->_longitude, query->_limit);

  // and append result
  GeoResult(cors, query->_distance, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a near query
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneNearQuery (TRI_query_t* qry) {
  TRI_near_query_t* query;

  query = (TRI_near_query_t*) qry;

  return TRI_CreateNearQuery(query->_operand->clone(query->_operand),
                             query->_latitude,
                             query->_longitude,
                             query->_limit,
                             query->_distance);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a near query
////////////////////////////////////////////////////////////////////////////////

static void FreeNearQuery (TRI_query_t* qry) {
  TRI_near_query_t* query;

  query = (TRI_near_query_t*) qry;

  TRI_Free(query->base._string);
  TRI_Free(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        SKIP QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a skip query
////////////////////////////////////////////////////////////////////////////////

static void ExecuteSkipQuery (TRI_query_t* qry,
                              TRI_query_result_t* result) {
  TRI_doc_mptr_t const** copy;
  TRI_json_t* aug = NULL;
  TRI_skip_query_t* query;
  TRI_voc_size_t newtotal;

  query = (TRI_skip_query_t*) qry;

  // execute the sub-query
  query->_operand->execute(query->_operand, result);

  if (result->_error) {
    return;
  }

  TRI_AppendString(&result->_cursor, ">skip");

  // no skip at all
  if (query->_skip == 0) {
    result->_matchedDocuments += result->_length;
    return;
  }

  // without any documents there will be no skipping
  if (result->_length == 0) {
    return;
  }

  // not enough documents
  if (result->_length <= query->_skip) {
    result->_length = 0;

    FreeDocuments(result);

    return;
  }

  // shorten result list
  newtotal = (result->_length - query->_skip);

  result->_matchedDocuments += newtotal;

  copy = TRI_Allocate(sizeof(TRI_doc_mptr_t*) * newtotal);
  memcpy(copy, result->_documents + query->_skip, sizeof(TRI_doc_mptr_t*) * newtotal);

  if (result->_augmented != NULL) {
    aug = TRI_Allocate(sizeof(TRI_json_t) * newtotal);
    memcpy(aug, result->_augmented + query->_skip, sizeof(TRI_json_t) * newtotal);
  }

  result->_length = newtotal;

  TRI_Free(result->_documents);
  result->_documents = copy;

  if (result->_augmented != NULL) {
    TRI_Free(result->_augmented);
    result->_augmented = aug;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a skip query
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneSkipQuery (TRI_query_t* qry) {
  TRI_skip_query_t* query;

  query = (TRI_skip_query_t*) qry;

  return TRI_CreateSkipQuery(query->_operand->clone(query->_operand), query->_skip);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a skip query
////////////////////////////////////////////////////////////////////////////////

static void FreeSkipQuery (TRI_query_t* qry) {
  TRI_skip_query_t* query;

  query = (TRI_skip_query_t*) qry;

  query->_operand->free(query->_operand);

  TRI_Free(query->base._string);
  TRI_Free(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      WITHIN QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a within query
////////////////////////////////////////////////////////////////////////////////

static void ExecuteWithinQuery (TRI_query_t* qry,
                                TRI_query_result_t* result) {
  GeoCoordinates* cors;
  TRI_geo_index_t* idx;
  TRI_within_query_t* query;
  TRI_query_t* operand;

  query = (TRI_within_query_t*) qry;
  operand = query->_operand;

  // free any old results
  FreeDocuments(result);

  // we need a suitable geo index
  if (operand->_type == TRI_QUE_TYPE_GEO_INDEX) {
    TRI_geo_index_query_t* geo;

    geo = (TRI_geo_index_query_t*) operand;

    idx = LookupGeoIndex(geo);
    operand = geo->_operand;
  }
  else {
    idx = LocateGeoIndex(&query->base);
  }

  if (idx == NULL) {
    result->_error = TRI_DuplicateString("cannot locate suitable geo index");
    return;
  }

  // sub-query must be the collection
  if (operand->_type != TRI_QUE_TYPE_COLLECTION) {
    result->_error = TRI_DuplicateString("within query can only be executed for the complete collection");
    return;
  }

  // append information about the execution plan
  TRI_AppendString(&result->_cursor, ">within");

  // execute geo search
  cors = TRI_WithinGeoIndex(&idx->base, query->_latitude, query->_longitude, query->_radius);

  // and create result
  GeoResult(cors, query->_distance, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a within query
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneWithinQuery (TRI_query_t* qry) {
  TRI_within_query_t* query;

  query = (TRI_within_query_t*) qry;

  return TRI_CreateWithinQuery(query->_operand->clone(query->_operand),
                               query->_latitude,
                               query->_longitude,
                               query->_radius,
                               query->_distance);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a within query
////////////////////////////////////////////////////////////////////////////////

static void FreeWithinQuery (TRI_query_t* qry) {
  TRI_within_query_t* query;

  query = (TRI_within_query_t*) qry;

  TRI_Free(query->base._string);
  TRI_Free(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a full scan of a collection
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateCollectionQuery (TRI_vocbase_col_t const* collection) {
  TRI_collection_query_t* query;

  query = TRI_Allocate(sizeof(TRI_collection_query_t));

  query->base._type = TRI_QUE_TYPE_COLLECTION;
  query->base._collection = collection;

  query->base._string = TRI_Concatenate2String("db.", collection->_name);

  query->base.execute = ExecuteCollectionQuery;
  query->base.clone = CloneCollectionQuery;
  query->base.free = FreeCollectionQuery;

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the distance to a query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateDistanceQuery (TRI_query_t* operand, char const* name) {

  // .............................................................................
  // operand is a NEAR
  // .............................................................................

  if (operand->_type == TRI_QUE_TYPE_NEAR) {
    TRI_near_query_t* near;

    near = (TRI_near_query_t*) operand;

    return TRI_CreateNearQuery(near->_operand->clone(near->_operand),
                               near->_latitude,
                               near->_longitude,
                               near->_limit,
                               name);
  }

  // .............................................................................
  // operand is a WITHIN
  // .............................................................................

  else if (operand->_type == TRI_QUE_TYPE_WITHIN) {
    TRI_within_query_t* within;

    within = (TRI_within_query_t*) operand;

    return TRI_CreateWithinQuery(within->_operand->clone(within->_operand),
                                 within->_latitude,
                                 within->_longitude,
                                 within->_radius,
                                 name);
  }

  // .............................................................................
  // general case
  // .............................................................................

  return operand->clone(operand);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateDocumentQuery (TRI_query_t* operand, TRI_voc_did_t did) {
  TRI_document_query_t* query;
  char* number;

  query = TRI_Allocate(sizeof(TRI_document_query_t));

  query->base._type = TRI_QUE_TYPE_DOCUMENT;
  query->base._collection = operand->_collection;

  number = TRI_StringUInt64(did);
  query->base._string = TRI_Concatenate4String(operand->_string, ".document(", number, ")");
  TRI_FreeString(number);

  query->base.execute = ExecuteDocumentQuery;
  query->base.clone = CloneDocumentQuery;
  query->base.free = FreeDocumentQuery;

  query->_operand = operand;
  query->_did = did;

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an geo-spatial index
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateGeoIndexQuery (TRI_query_t* operand,
                                      char const* location,
                                      char const* latitude,
                                      char const* longitude) {
  TRI_geo_index_query_t* query;

  query = TRI_Allocate(sizeof(TRI_geo_index_query_t));

  query->base._type = TRI_QUE_TYPE_GEO_INDEX;
  query->base._collection = operand->_collection;

  if (location != NULL) {
    query->base._string = TRI_Concatenate4String(operand->_string, ".geo(", location, ")");
  }
  else if (latitude != NULL && longitude != NULL) {
    query->base._string = TRI_Concatenate6String(operand->_string, ".geo(", latitude, ",", longitude, ")");
  }
  else {
    query->base._string = TRI_DuplicateString("unknown");
  }

  query->base.execute = ExecuteGeoIndexQuery;
  query->base.clone = CloneGeoIndexQuery;
  query->base.free = FreeGeoIndeyQuery;

  query->_operand = operand;
  query->_location = (location == NULL ? NULL : TRI_DuplicateString(location));
  query->_latitude = (latitude == NULL ? NULL : TRI_DuplicateString(latitude));
  query->_longitude = (longitude == NULL ? NULL : TRI_DuplicateString(longitude));

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief limits an existing query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateLimitQuery (TRI_query_t* operand, TRI_voc_ssize_t limit) {

  // .............................................................................
  // operand is a NEAR
  // .............................................................................

  if (0 <= limit && operand->_type == TRI_QUE_TYPE_NEAR) {
    TRI_near_query_t* near;

    near = (TRI_near_query_t*) operand;

    if (near->_limit < limit) {
      limit = near->_limit;
    }

    return TRI_CreateNearQuery(near->_operand->clone(near->_operand),
                               near->_latitude,
                               near->_longitude,
                               limit,
                               near->_distance);
  }

  // .............................................................................
  // operand is a LIMIT
  // .............................................................................

  if (operand->_type == TRI_QUE_TYPE_LIMIT) {
    TRI_limit_query_t* lquery;
    bool combine;

    lquery = (TRI_limit_query_t*) operand;
    combine = true;

    if (lquery->_limit < 0 && limit <= 0) {
      if (limit < lquery->_limit) {
        limit = lquery->_limit;
      }
    }
    else if (lquery->_limit > 0 && limit >= 0) {
      if (limit > lquery->_limit) {
        limit = lquery->_limit;
      }
    }
    else if (abs(lquery->_limit) <= abs(limit)) {
      return lquery->base.clone(&lquery->base);
    }
    else {
      combine = false;
    }

    if (combine) {
      return TRI_CreateLimitQuery(lquery->_operand->clone(lquery->_operand),
                                  limit);
    }
  }

  // .............................................................................
  // general case
  // .............................................................................

  TRI_limit_query_t* query;
  char* number;

  query = TRI_Allocate(sizeof(TRI_limit_query_t));

  query->base._type = TRI_QUE_TYPE_LIMIT;
  query->base._collection = operand->_collection;

  number = TRI_StringInt64(limit);
  query->base._string = TRI_Concatenate4String(operand->_string, ".limit(", number, ")");
  TRI_FreeString(number);

  query->base.execute = ExecuteLimitQuery;
  query->base.clone = CloneLimitQuery;
  query->base.free = FreeLimitQuery;

  query->_operand = operand;
  query->_limit = limit;

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up documents near a given point
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateNearQuery (TRI_query_t* operand,
                                  double latitude,
                                  double longitude,
                                  TRI_voc_size_t count,
                                  char const* distance) {
  TRI_string_buffer_t sb;
  TRI_near_query_t* query;

  query = TRI_Allocate(sizeof(TRI_near_query_t));

  query->base._type = TRI_QUE_TYPE_NEAR;
  query->base._collection = operand->_collection;


  TRI_InitStringBuffer(&sb);

  TRI_AppendStringStringBuffer(&sb, ".near(");
  TRI_AppendDoubleStringBuffer(&sb, latitude);
  TRI_AppendStringStringBuffer(&sb, ",");
  TRI_AppendDoubleStringBuffer(&sb, longitude);
  TRI_AppendStringStringBuffer(&sb, ")");

  TRI_AppendStringStringBuffer(&sb, ".limit(");
  TRI_AppendUInt64StringBuffer(&sb, count);
  TRI_AppendStringStringBuffer(&sb, ")");

  if (distance != NULL) {
    TRI_AppendStringStringBuffer(&sb, ".distance(\"");
    TRI_AppendStringStringBuffer(&sb, distance);
    TRI_AppendStringStringBuffer(&sb, "\")");
  }

  query->base._string = sb._buffer;

  query->base.execute = ExecuteNearQuery;
  query->base.clone = CloneNearQuery;
  query->base.free = FreeNearQuery;

  query->_operand = operand;
  query->_location = NULL;
  query->_latitude = latitude;
  query->_longitude = longitude;
  query->_limit = count;
  query->_distance = (distance == NULL ? NULL : TRI_DuplicateString(distance));

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skips elements of an existing query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateSkipQuery (TRI_query_t* operand, TRI_voc_size_t skip) {
  TRI_skip_query_t* query;
  char* number;

  // .............................................................................
  // operand is a SKIP
  // .............................................................................

  if (0 <= skip && operand->_type == TRI_QUE_TYPE_SKIP) {
    TRI_skip_query_t* squery;

    squery = (TRI_skip_query_t*) operand;

    return TRI_CreateSkipQuery(squery->_operand->clone(squery->_operand),
                               squery->_skip + skip);
  }

  // .............................................................................
  // general case
  // .............................................................................

  query = TRI_Allocate(sizeof(TRI_skip_query_t));

  query->base._type = TRI_QUE_TYPE_SKIP;
  query->base._collection = operand->_collection;

  number = TRI_StringInt64(skip);
  query->base._string = TRI_Concatenate4String(operand->_string, ".skip(", number, ")");
  TRI_FreeString(number);

  query->base.execute = ExecuteSkipQuery;
  query->base.clone = CloneSkipQuery;
  query->base.free = FreeSkipQuery;

  query->_operand = operand;
  query->_skip = skip;

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up documents within a given radius
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateWithinQuery (TRI_query_t* operand,
                                    double latitude,
                                    double longitude,
                                    double radius,
                                    char const* distance) {
  TRI_string_buffer_t sb;
  TRI_within_query_t* query;

  query = TRI_Allocate(sizeof(TRI_within_query_t));

  query->base._type = TRI_QUE_TYPE_WITHIN;
  query->base._collection = operand->_collection;


  TRI_InitStringBuffer(&sb);

  TRI_AppendStringStringBuffer(&sb, ".within(");
  TRI_AppendDoubleStringBuffer(&sb, latitude);
  TRI_AppendStringStringBuffer(&sb, ",");
  TRI_AppendDoubleStringBuffer(&sb, longitude);
  TRI_AppendStringStringBuffer(&sb, ",");
  TRI_AppendDoubleStringBuffer(&sb, radius);
  TRI_AppendStringStringBuffer(&sb, ")");

  if (distance != NULL) {
    TRI_AppendStringStringBuffer(&sb, ".distance(\"");
    TRI_AppendStringStringBuffer(&sb, distance);
    TRI_AppendStringStringBuffer(&sb, "\")");
  }

  query->base._string = sb._buffer;

  query->base.execute = ExecuteWithinQuery;
  query->base.clone = CloneWithinQuery;
  query->base.free = FreeWithinQuery;

  query->_operand = operand;
  query->_location = NULL;
  query->_latitude = latitude;
  query->_longitude = longitude;
  query->_radius = radius;
  query->_distance = (distance == NULL ? NULL : TRI_DuplicateString(distance));

  return &query->base;
}

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
/// @brief executes a query
////////////////////////////////////////////////////////////////////////////////

TRI_result_set_t* TRI_ExecuteQuery (TRI_query_t* query) {
  TRI_doc_collection_t* collection = query->_collection->_collection;
  TRI_query_result_t result;
  TRI_result_set_t* rs;
  TRI_rs_info_t info;

  result._cursor = TRI_DuplicateString("query");
  result._error = NULL;

  result._length = 0;
  result._total = 0;

  result._documents = NULL;
  result._augmented = NULL;

  result._scannedIndexEntries = 0;
  result._scannedDocuments = 0;
  result._matchedDocuments = 0;

  info._runtime = -TRI_microtime();

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->beginRead(collection);

  query->execute(query, &result);

  if (result._error) {
    LOG_TRACE("query returned error: %s", result._error);
    rs = TRI_CreateRSSingle(collection, NULL, 0);
    rs->_error = result._error;
  }
  else {
    LOG_TRACE("query returned '%lu' documents", (unsigned long) result._length);
    rs = TRI_CreateRSVector(collection,
                            result._documents,
                            result._augmented,
                            result._length,
                            result._total);
  }

  collection->endRead(collection);

  // .............................................................................
  // outside a read transaction
  // .............................................................................

  info._cursor = result._cursor;

  info._runtime += TRI_microtime();

  info._scannedIndexEntries = result._scannedIndexEntries;
  info._scannedDocuments = result._scannedDocuments;
  info._matchedDocuments = result._matchedDocuments;

  rs->_info = info;

  FreeDocuments(&result);

  return rs;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
