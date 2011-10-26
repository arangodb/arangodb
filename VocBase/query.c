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

#include "query.h"

#include <Basics/logging.h>
#include <Basics/strings.h>
#include <VocBase/simple-collection.h>
#include <VocBase/index.h>

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static TRI_doc_mptr_t const** ExecuteQuery (TRI_query_t* query,
                                            TRI_rs_info_t* info,
                                            TRI_voc_size_t* length,
                                            TRI_voc_size_t* total);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a full scan
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const** ExecuteCollectionQuery (TRI_collection_query_t* query,
                                                      TRI_rs_info_t* info,
                                                      TRI_voc_size_t* length,
                                                      TRI_voc_size_t* total) {
  TRI_doc_mptr_t const** result;
  TRI_doc_mptr_t const** qtr;
  TRI_sim_collection_t* collection;
  void** end;
  void** ptr;

  TRI_AppendString(&info->_cursor, ":collection");

  *length = 0;

  if (query->base._collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return NULL;
  }

  collection = (TRI_sim_collection_t*) query->base._collection;

  *total = *length = collection->_primaryIndex._nrUsed;

  if (*length == 0) {
    return NULL;
  }

  result = qtr = TRI_Allocate(sizeof(TRI_doc_mptr_t*) * (*length));

  ptr = collection->_primaryIndex._table;
  end = collection->_primaryIndex._table + collection->_primaryIndex._nrAlloc;

  for (;  ptr < end;  ++ptr) {
    ++info->_scannedDocuments;

    if (*ptr) {
      ++info->_matchedDocuments;
      *qtr++ = *ptr;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a document lookup as primary index lookup
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const** ExecuteDocumentQueryPrimary (TRI_document_query_t* query,
                                                           TRI_doc_collection_t* collection,
                                                           TRI_rs_info_t* info,
                                                           TRI_voc_size_t* length,
                                                           TRI_voc_size_t* total) {
  TRI_doc_mptr_t const* document;
  TRI_doc_mptr_t const** result;

  document = collection->read(collection, query->_did);

  TRI_AppendString(&info->_cursor, ":primary");

  ++info->_scannedIndexEntries;

  if (document == NULL) {
    *length = 0;
    *total = 0;
    return NULL;
  }
  else {
    *length = 1;
    *total = 1;

    ++info->_matchedDocuments;

    result = TRI_Allocate(sizeof(TRI_doc_mptr_t*));
    *result = document;

    return result;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a document lookup as full scan
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const** ExecuteDocumentQuerySlow (TRI_document_query_t* query,
                                                        TRI_rs_info_t* info,
                                                        TRI_voc_size_t* length,
                                                        TRI_voc_size_t* total) {
  TRI_doc_mptr_t const** result;
  TRI_doc_mptr_t const** ptr;
  TRI_doc_mptr_t const** sub;
  TRI_doc_mptr_t const** end;
  TRI_voc_size_t subtotal;

  sub = ExecuteQuery(query->_operand, info, &subtotal, total);

  TRI_AppendString(&info->_cursor, ":full-scan");

  if (sub == NULL) {
    *length = 0;
    return NULL;
  }

  if (subtotal == 0) {
    TRI_Free(sub);

    *length = 0;
    return NULL;
  }

  end = sub + subtotal;

  for (ptr = sub;  ptr < end;  ++ptr) {
    ++info->_scannedDocuments;

    if ((*ptr)->_did == query->_did) {
      ++info->_matchedDocuments;
      result = TRI_Allocate(sizeof(TRI_doc_mptr_t*));

      *result = *ptr;
      *length = 1;

      TRI_Free(sub);

      return result;
    }
  }

  TRI_Free(sub);

  *length = 0;
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a document lookup
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const** ExecuteDocumentQuery (TRI_document_query_t* query,
                                                    TRI_rs_info_t* info,
                                                    TRI_voc_size_t* length,
                                                    TRI_voc_size_t* total) {
  if (query->_operand != NULL && query->_operand->_type == TRI_QUE_TYPE_COLLECTION) {
    return ExecuteDocumentQueryPrimary(query,
                                       query->_operand->_collection,
                                       info,
                                       length,
                                       total);
  }
  else {
    return ExecuteDocumentQuerySlow(query, info, length, total);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a limit
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const** ExecuteLimitQuery (TRI_limit_query_t* query,
                                                 TRI_rs_info_t* info,
                                                 TRI_voc_size_t* length,
                                                 TRI_voc_size_t* total) {
  TRI_doc_mptr_t const** result;
  TRI_doc_mptr_t const** sub;
  TRI_voc_size_t subtotal;
  TRI_voc_size_t offset;
  TRI_voc_size_t limit;

  sub = ExecuteQuery(query->_operand, info, &subtotal, total);

  TRI_AppendString(&info->_cursor, ":limit");

  if (sub == NULL) {
    *length = 0;
    return NULL;
  }

  // get the last "limit" documents
  if (query->_limit < 0) {
    limit = -query->_limit;
    offset = subtotal + query->_limit;
  }
  else {
    limit = query->_limit;
    offset = 0;
  }

  if (subtotal <= limit) {
    info->_matchedDocuments += subtotal;
    *length = subtotal;
    return sub;
  }

  info->_matchedDocuments += limit;
  result = TRI_Allocate(sizeof(TRI_doc_mptr_t*) * limit);

  memcpy(result, sub + offset, sizeof(TRI_doc_mptr_t*) * limit);
  *length = limit;

  TRI_Free(sub);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a near
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const** ExecuteNearQuery (TRI_near_query_t* query,
                                                TRI_rs_info_t* info,
                                                TRI_voc_size_t* length,
                                                TRI_voc_size_t* total) {
  GeoCoordinate* end;
  GeoCoordinate* ptr;
  GeoCoordinates* cors;
  TRI_doc_mptr_t const** result;
  TRI_doc_mptr_t const** wtr;
  size_t n;

  TRI_AppendString(&info->_cursor, ":near");

  cors = TRI_NearestGeoIndex(query->_index, query->_latitude, query->_longitude, query->_count);

  if (cors == NULL) {
    LOG_WARNING("cannot execute geo-index");
    *length = 0;
    return NULL;
  }

  n = cors->length;

  ptr = cors->coordinates;
  end = cors->coordinates + n;

  result = (TRI_doc_mptr_t const**) (wtr = TRI_Allocate(sizeof(TRI_doc_mptr_t*) * n));

  for (;  ptr < end;  ++ptr, ++wtr) {
    *wtr = ptr->data;
  }

  GeoIndex_CoordinatesFree(cors);

  *length = n;
  *total = n;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a skip
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const** ExecuteSkipQuery (TRI_skip_query_t* query,
                                                TRI_rs_info_t* info,
                                                TRI_voc_size_t* length,
                                                TRI_voc_size_t* total) {
  TRI_doc_mptr_t const** result;
  TRI_doc_mptr_t const** sub;
  TRI_voc_size_t subtotal;
  TRI_voc_size_t newtotal;

  sub = ExecuteQuery(query->_operand, info, &subtotal, total);

  TRI_AppendString(&info->_cursor, ":skip");

  if (sub == NULL) {
    *length = 0;
    return NULL;
  }

  if (subtotal <= query->_skip) {
    TRI_Free(sub);
    *length = 0;
    return NULL;
  }

  if (query->_skip == 0) {
    info->_matchedDocuments += subtotal;
    *length = subtotal;
    return sub;
  }

  newtotal = (subtotal - query->_skip);
  info->_matchedDocuments += newtotal;
  result = TRI_Allocate(sizeof(TRI_doc_mptr_t*) * newtotal);

  memcpy(result, sub + query->_skip, sizeof(TRI_doc_mptr_t*) * newtotal);
  *length = subtotal - query->_skip;

  TRI_Free(sub);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const** ExecuteQuery (TRI_query_t* query,
                                            TRI_rs_info_t* info,
                                            TRI_voc_size_t* length,
                                            TRI_voc_size_t* total) {
  TRI_doc_mptr_t const** result;

  result = NULL;
  *length = 0;

  switch (query->_type) {
    case TRI_QUE_TYPE_COLLECTION:
      result = ExecuteCollectionQuery((TRI_collection_query_t*) query, info, length, total);
      break;

    case TRI_QUE_TYPE_DOCUMENT:
      result = ExecuteDocumentQuery((TRI_document_query_t*) query, info, length, total);
      break;

    case TRI_QUE_TYPE_LIMIT:
      result = ExecuteLimitQuery((TRI_limit_query_t*) query, info, length, total);
      break;

    case TRI_QUE_TYPE_NEAR:
      result = ExecuteNearQuery((TRI_near_query_t*) query, info, length, total);
      break;

    case TRI_QUE_TYPE_SKIP:
      result = ExecuteSkipQuery((TRI_skip_query_t*) query, info, length, total);
      break;
  }

  return result;
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
/// @brief creates a full scan of a collection
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateCollectionQuery (struct TRI_doc_collection_s* collection) {
  TRI_collection_query_t* query;

  query = TRI_Allocate(sizeof(TRI_collection_query_t));

  query->base._type = TRI_QUE_TYPE_COLLECTION;
  query->base._collection = collection;

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateDocumentQuery (TRI_query_t* operand, TRI_voc_did_t did) {
  TRI_document_query_t* query;

  query = TRI_Allocate(sizeof(TRI_document_query_t));

  query->base._type = TRI_QUE_TYPE_DOCUMENT;
  query->base._collection = operand->_collection;

  query->_operand = operand;
  query->_did = did;

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief limits an existing query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateLimitQuery (TRI_query_t* operand, TRI_voc_ssize_t limit) {
  TRI_limit_query_t* query;

  query = TRI_Allocate(sizeof(TRI_limit_query_t));

  query->base._type = TRI_QUE_TYPE_LIMIT;
  query->base._collection = operand->_collection;

  query->_operand = operand;
  query->_limit = limit;

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up documents near a given point
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateNearQuery (TRI_query_t* operand,
                                  char const* location,
                                  double latitude,
                                  double longitude,
                                  TRI_voc_size_t count,
                                  char const** error) {
  TRI_index_t* geo;
  TRI_near_query_t* query;

  if (operand->_type != TRI_QUE_TYPE_COLLECTION) {
    *error = "'near' requires a collection as operand";
    return NULL;
  }

#warning LOOKUP MUST BE DONE WITH AT LEAST A READ TRANSACTION
  if (operand->_collection->base._type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    geo = TRI_LookupGeoIndexSimCollection((TRI_sim_collection_t*) operand->_collection, location);
  }
  else {
    *error = "unknown collection type";
    return NULL;
  }

  if (geo == NULL) {
    *error = "'near' requires a geo-index";
    return NULL;
  }

  query = TRI_Allocate(sizeof(TRI_near_query_t));

  query->base._type = TRI_QUE_TYPE_NEAR;
  query->base._collection = operand->_collection;

  query->_index = geo;
  query->_latitude = latitude;
  query->_longitude = longitude;
  query->_count = count;

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up documents near a given point
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateNear2Query (TRI_query_t* operand,
                                   char const* locLatitude,
                                   double latitude,
                                   char const* locLongitude,
                                   double longitude,
                                   TRI_voc_size_t count,
                                   char const** error) {
  if (operand->_type != TRI_QUE_TYPE_COLLECTION) {
    *error = "near requires a collection as operand";
    return 0;
  }

  *error = "not yet";
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skips elements of an existing query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateSkipQuery (TRI_query_t* operand, TRI_voc_size_t skip) {
  TRI_skip_query_t* query;

  query = TRI_Allocate(sizeof(TRI_skip_query_t));

  query->base._type = TRI_QUE_TYPE_SKIP;
  query->base._collection = operand->_collection;

  query->_operand = operand;
  query->_skip = skip;

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query
////////////////////////////////////////////////////////////////////////////////

TRI_result_set_t* TRI_ExecuteQuery (TRI_query_t* query) {
  TRI_doc_collection_t* collection = query->_collection;
  TRI_doc_mptr_t const** result;
  TRI_voc_size_t length;
  TRI_voc_size_t total;
  TRI_result_set_t* rs;
  TRI_rs_info_t info;

  result = NULL;
  length = 0;

  info._cursor = TRI_DuplicateString("");
  info._scannedIndexEntries = 0;
  info._scannedDocuments = 0;
  info._matchedDocuments = 0;
  info._runtime = -TRI_microtime();

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->beginRead(collection);

  result = ExecuteQuery(query, &info, &length, &total);

  LOG_TRACE("query returned '%lu' documents", (unsigned long) length);

  rs = TRI_CreateRSVector(collection, result, length, total);

  collection->endRead(collection);

  // .............................................................................
  // outside a read transaction
  // .............................................................................

  info._runtime += TRI_microtime();

  rs->_info = info;

  return rs;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
