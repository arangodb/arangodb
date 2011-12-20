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

#include <BasicsC/logging.h>
#include <BasicsC/strings.h>
#include <BasicsC/string-buffer.h>
#include <BasicsC/conversions.h>
#include <VocBase/simple-collection.h>
#include <VocBase/index.h>

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static void ExecuteDocumentQueryPrimary (TRI_query_t* qry,
                                         TRI_query_result_t* result);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the old storage of a TRI_query_result_t
///
/// Note the augument json objects are not freed.
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
/// @brief frees the augument json objects
////////////////////////////////////////////////////////////////////////////////

static void FreeAugmented (TRI_query_result_t* result) {
  TRI_json_t* ptr;
  TRI_json_t* end;

  if (result->_augmented != NULL) {
    ptr = result->_augmented;
    end = result->_augmented + result->_length;

    for (;  ptr < end;  ++ptr) {
      TRI_DestroyJson(ptr);
    }

    result->_augmented = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                             QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a TRI_query_t
////////////////////////////////////////////////////////////////////////////////

static void CloneQuery (TRI_query_t* dst, TRI_query_t* src) {
  dst->_type = src->_type;
  dst->_collection = src->_collection;
  dst->_isOptimised = src->_isOptimised;

  dst->optimise = src->optimise;
  dst->execute = src->execute;
  dst->json = src->json;
  dst->stringify = src->stringify;
  dst->clone = src->clone;
  dst->free = src->free;
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
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief optimises a full scan
////////////////////////////////////////////////////////////////////////////////

static char* OptimiseCollectionQuery (TRI_query_t** qry) {
  TRI_document_query_t* query;
  TRI_col_type_e type;

  // extract query
  query = (TRI_document_query_t*) *qry;

  // sanity check
  if (! query->base._collection->_loaded) {
    return TRI_Concatenate3String("collection \"", query->base._collection->_name, "\" not loaded");
  }

  // check if query is already optimised
  if (query->base._isOptimised) {
    return NULL;
  }

  type = query->base._collection->_collection->base._type;

  if (type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return TRI_DuplicateString("cannot handle collection type");
  }

  // nothing to optimise
  query->base._isOptimised = true;
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a full scan
////////////////////////////////////////////////////////////////////////////////

static void ExecuteCollectionQuery (TRI_query_t* qry, TRI_query_result_t* result) {
  TRI_doc_mptr_t const** qtr;
  TRI_document_query_t* query;
  TRI_sim_collection_t* collection;
  size_t total;
  void** end;
  void** ptr;

  // extract query and document
  query = (TRI_document_query_t*) qry;
  collection = (TRI_sim_collection_t*) query->base._collection->_collection;

  // append information about the execution plan
  TRI_AppendString(&result->_cursor, ">collection");

  // free any old storage
  FreeAugmented(result);
  FreeDocuments(result);

  // add a new document list and copy all documents
  result->_total = result->_length = 0;

  if (collection->_primaryIndex._nrUsed == 0) {
    return;
  }

  result->_documents = (qtr = TRI_Allocate(sizeof(TRI_doc_mptr_t*) * (collection->_primaryIndex._nrUsed)));

  ptr = collection->_primaryIndex._table;
  end = collection->_primaryIndex._table + collection->_primaryIndex._nrAlloc;

  for (total = 0;  ptr < end;  ++ptr) {
    ++result->_scannedDocuments;

    if (*ptr) {
      TRI_doc_mptr_t* d = *ptr;

      if (d->_deletion == 0) {
        ++result->_matchedDocuments;
        *qtr++ = *ptr;
        total++;
      }
    }
  }

  result->_total = result->_length = total;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief json representation of a full scan
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonCollectionQuery (TRI_query_t* qry) {
  TRI_document_query_t* query;
  TRI_json_t* json;

  query = (TRI_document_query_t*) qry;
  json = TRI_CreateArrayJson();

  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("collection"));
  TRI_Insert2ArrayJson(json, "collection", TRI_CreateStringCopyJson(query->base._collection->_name));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string representation of a full scan
////////////////////////////////////////////////////////////////////////////////

static void StringifyCollectionQuery (TRI_query_t* qry, TRI_string_buffer_t* buffer) {
  TRI_document_query_t* query;

  query = (TRI_document_query_t*) qry;

  TRI_AppendStringStringBuffer(buffer, "collection(\"");
  TRI_AppendStringStringBuffer(buffer, query->base._collection->_name);
  TRI_AppendStringStringBuffer(buffer, "\")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a full scan
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneCollectionQuery (TRI_query_t* qry) {
  TRI_collection_query_t* clone;

  clone = TRI_Allocate(sizeof(TRI_collection_query_t));

  CloneQuery(&clone->base, qry);

  return &clone->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a full scan
////////////////////////////////////////////////////////////////////////////////

static void FreeCollectionQuery (TRI_query_t* query, bool descend) {
  TRI_Free(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    DISTANCE QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief optimises a distance query
////////////////////////////////////////////////////////////////////////////////

static char* OptimiseDistanceQuery (TRI_query_t** qry) {
  TRI_distance_query_t* query;
  TRI_query_t* operand;
  char* e;

  query = (TRI_distance_query_t*) *qry;

  // sanity check
  if (! query->base._collection->_loaded) {
    return TRI_Concatenate3String("collection \"", query->base._collection->_name, "\" not loaded");
  }

  // check if query is already optimised
  if (query->base._isOptimised) {
    return NULL;
  }

  // optimise operand
  e = query->_operand->optimise(&query->_operand);

  if (e != NULL) {
    return e;
  }

  operand = query->_operand;

  // .............................................................................
  // operand is a NEAR
  // .............................................................................

  if (operand->_type == TRI_QUE_TYPE_NEAR) {
    TRI_near_query_t* near;

    near = (TRI_near_query_t*) operand;

    near->_distance = TRI_DuplicateString(query->_distance);
    near->base._isOptimised = false;

    // free the query, but not the operands of the query
    query->base.free(&query->base, false);

    // change the query to its operand
    *qry = &near->base;

    // recure
    return (*qry)->optimise(qry);
  }

  // .............................................................................
  // operand is a WITHIN
  // .............................................................................

  else if (operand->_type == TRI_QUE_TYPE_WITHIN) {
    TRI_within_query_t* within;

    within = (TRI_within_query_t*) operand;

    within->_distance = TRI_DuplicateString(query->_distance);
    within->base._isOptimised = false;

    // free the query, but not the operands of the query
    query->base.free(&query->base, false);

    // change the query to its operand
    *qry = &within->base;

    // recure
    return (*qry)->optimise(qry);
  }

  query->base._isOptimised = true;
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a distance query
////////////////////////////////////////////////////////////////////////////////

static void ExecuteDistanceQuery (TRI_query_t* qry,
                              TRI_query_result_t* result) {
  TRI_distance_query_t* query;

  query = (TRI_distance_query_t*) qry;

  query->_operand->execute(query->_operand, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief json representation of a distance query
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonDistanceQuery (TRI_query_t* qry) {
  TRI_json_t* json;
  TRI_distance_query_t* query;

  query = (TRI_distance_query_t*) qry;
  json = TRI_CreateArrayJson();

  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("distance"));
  TRI_Insert2ArrayJson(json, "distance", TRI_CreateStringCopyJson(query->_distance));
  TRI_Insert2ArrayJson(json, "operand", query->_operand->json(query->_operand));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string representation of a distance query
////////////////////////////////////////////////////////////////////////////////

static void StringifyDistanceQuery (TRI_query_t* qry, TRI_string_buffer_t* buffer) {
  TRI_distance_query_t* query;

  query = (TRI_distance_query_t*) qry;

  query->_operand->stringify(query->_operand, buffer);

  TRI_AppendStringStringBuffer(buffer, ".distance(");
  TRI_AppendStringStringBuffer(buffer, query->_distance);
  TRI_AppendStringStringBuffer(buffer, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a distance query
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneDistanceQuery (TRI_query_t* qry) {
  TRI_distance_query_t* clone;
  TRI_distance_query_t* query;

  clone = TRI_Allocate(sizeof(TRI_distance_query_t));
  query = (TRI_distance_query_t*) qry;

  CloneQuery(&clone->base, &query->base);

  clone->_operand = query->_operand->clone(query->_operand);
  clone->_distance = TRI_DuplicateString(query->_distance);

  return &clone->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a distance query
////////////////////////////////////////////////////////////////////////////////

static void FreeDistanceQuery (TRI_query_t* qry, bool descend) {
  TRI_distance_query_t* query;

  query = (TRI_distance_query_t*) qry;

  if (descend) {
    query->_operand->free(query->_operand, descend);
  }

  TRI_FreeString(query->_distance);
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
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief optimises a document lookup
////////////////////////////////////////////////////////////////////////////////

static char* OptimiseDocumentQuery (TRI_query_t** qry) {
  TRI_document_query_t* query;
  TRI_col_type_e type;
  char* e;

  query = (TRI_document_query_t*) *qry;

  // sanity check
  if (! query->base._collection->_loaded) {
    return TRI_Concatenate3String("collection \"", query->base._collection->_name, "\" not loaded");
  }

  // check if query is already optimised
  if (query->base._isOptimised) {
    return NULL;
  }

  // underlying query is a collection, use the primary index
  if (query->_operand != NULL && query->_operand->_type == TRI_QUE_TYPE_COLLECTION) {
    type = query->base._collection->_collection->base._type;

    if (type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
      return TRI_DuplicateString("cannot handle collection type");
    }

    query->_operand->free(query->_operand, true);
    query->_operand = NULL;

    query->base.execute = ExecuteDocumentQueryPrimary;
  }

  // optimise operand
  else {
    e = query->_operand->optimise(&query->_operand);

    if (e != NULL) {
      return e;
    }
  }

  query->base._isOptimised = true;
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a document lookup as primary index lookup
////////////////////////////////////////////////////////////////////////////////

static void ExecuteDocumentQueryPrimary (TRI_query_t* qry,
                                         TRI_query_result_t* result) {
  TRI_doc_mptr_t const* document;
  TRI_document_query_t* query;
  TRI_vocbase_col_t const* collection;

  query = (TRI_document_query_t*) qry;

  // look up the document
  collection = query->base._collection;
  document = collection->_collection->read(collection->_collection, query->_did);

  // append information about the execution plan
  TRI_AppendString(&result->_cursor, ">primary");

  ++result->_scannedIndexEntries;

  // free any old storage
  FreeAugmented(result);
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

static void ExecuteDocumentQuery (TRI_query_t* qry,
                                  TRI_query_result_t* result) {
  TRI_doc_mptr_t const** end;
  TRI_doc_mptr_t const** ptr;
  TRI_doc_mptr_t const* found;
  TRI_document_query_t* query;
  TRI_json_t aug;
  size_t pos;

  // get the result set of the underlying query
  query = (TRI_document_query_t*) qry;

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

      found = *ptr;
      TRI_Free(result->_documents);
      result->_documents = TRI_Allocate(sizeof(TRI_doc_mptr_t*));

      result->_total = result->_length = 1;
      *result->_documents = found;

      if (result->_augmented != NULL) {
        TRI_CopyToJson(&aug, &result->_augmented[pos]);

        FreeAugmented(result);
        result->_augmented = TRI_Allocate(sizeof(TRI_json_t));

        result->_augmented[0] = aug;
      }

      return;
    }
  }

  // nothing found
  result->_total = result->_length = 0;

  FreeAugmented(result);
  FreeDocuments(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief json representation of a document lookup
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonDocumentQuery (TRI_query_t* qry) {
  TRI_json_t* json;
  TRI_document_query_t* query;

  query = (TRI_document_query_t*) qry;
  json = TRI_CreateArrayJson();

  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("document"));
  TRI_Insert2ArrayJson(json, "identfier", TRI_CreateNumberJson(query->_did));

  if (query->_operand == NULL) {
    TRI_Insert2ArrayJson(json, "collection", TRI_CreateStringCopyJson(query->base._collection->_name));
  }
  else {
    TRI_Insert2ArrayJson(json, "operand", query->_operand->json(query->_operand));
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string representation of a document lookup
////////////////////////////////////////////////////////////////////////////////

static void StringifyDocumentQuery (TRI_query_t* qry, TRI_string_buffer_t* buffer) {
  TRI_document_query_t* query;

  query = (TRI_document_query_t*) qry;

  if (query->_operand == NULL) {
    TRI_AppendStringStringBuffer(buffer, "collection(\"");
    TRI_AppendStringStringBuffer(buffer, query->base._collection->_name);
    TRI_AppendStringStringBuffer(buffer, "\")");
  }
  else {
    query->_operand->stringify(query->_operand, buffer);
  }

  TRI_AppendStringStringBuffer(buffer, ".document(");
  TRI_AppendUInt64StringBuffer(buffer, query->_did);
  TRI_AppendStringStringBuffer(buffer, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a document lookup
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneDocumentQuery (TRI_query_t* qry) {
  TRI_document_query_t* clone;
  TRI_document_query_t* query;

  clone = TRI_Allocate(sizeof(TRI_document_query_t));
  query = (TRI_document_query_t*) qry;

  CloneQuery(&clone->base, &query->base);

  clone->_operand = query->_operand == NULL ? NULL : query->_operand->clone(query->_operand);
  clone->_did = query->_did;

  return &clone->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a document lookup
////////////////////////////////////////////////////////////////////////////////

static void FreeDocumentQuery (TRI_query_t* qry, bool descend) {
  TRI_document_query_t* query;

  query = (TRI_document_query_t*) qry;

  if (descend) {
    if (query->_operand != NULL) {
      query->_operand->free(query->_operand, descend);
    }
  }

  TRI_Free(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       EDGES QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief optimises an edges lookup
////////////////////////////////////////////////////////////////////////////////

static char* OptimiseEdgesQuery (TRI_query_t** qry) {
  TRI_edges_query_t* query;
  char* e;

  query = (TRI_edges_query_t*) *qry;

  // sanity check
  if (! query->base._collection->_loaded) {
    return TRI_Concatenate3String("collection \"", query->base._collection->_name, "\" not loaded");
  }

  if (query->base._collection->_type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return TRI_DuplicateString("cannot handle collection type");
  }

  // check if query is already optimised
  if (query->base._isOptimised) {
    return NULL;
  }

  // optimise operand
  e = query->_operand->optimise(&query->_operand);

  if (e != NULL) {
    return e;
  }

  query->base._isOptimised = true;
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an edges lookup
////////////////////////////////////////////////////////////////////////////////

static void ExecuteEdgesQuery (TRI_query_t* qry,
                               TRI_query_result_t* result) {
  TRI_edges_query_t* query;
  TRI_sim_collection_t* edgesCollection;
  TRI_vector_pointer_t v;
  size_t i;
  size_t j;

  query = (TRI_edges_query_t*) qry;

  // execute the sub-query
  query->_operand->execute(query->_operand, result);

  if (result->_error) {
    return;
  }

  TRI_AppendString(&result->_cursor, ">edges");

  // without any documents there will be no edges
  if (result->_length == 0) {
    return;
  }

  // create a vector for the result
  TRI_InitVectorPointer(&v);

  edgesCollection = (TRI_sim_collection_t*) query->base._collection->_collection;

  // for each document get the edges
  for (i = 0;  i < result->_length;  ++i) {
    TRI_doc_mptr_t const* mptr;
    TRI_vector_pointer_t edges;

    mptr = result->_documents[i];
    edges = TRI_LookupEdgesSimCollection(edgesCollection,
                                         query->_direction,
                                         query->_operand->_collection->_cid,
                                         mptr->_did);

    for (j = 0;  j < edges._length;  ++j) {
      TRI_PushBackVectorPointer(&v, edges._buffer[j]);
    }

    TRI_DestroyVectorPointer(&edges);
  }

  // free any old results
  FreeAugmented(result);
  FreeDocuments(result);

  result->_documents = (TRI_doc_mptr_t const**) v._buffer;
  result->_total = result->_length = v._length;
  result->_scannedDocuments += v._length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief json representation of an edges lookup
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonEdgesQuery (TRI_query_t* qry) {
  TRI_json_t* json;
  TRI_edges_query_t* query;

  query = (TRI_edges_query_t*) qry;
  json = TRI_CreateArrayJson();

  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("edges"));
  TRI_Insert2ArrayJson(json, "operand", query->_operand->json(query->_operand));
  TRI_Insert2ArrayJson(json, "direction", TRI_CreateNumberJson(query->_direction));
  TRI_Insert2ArrayJson(json, "edge-collection", TRI_CreateNumberJson(query->base._collection->_cid));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string representation of an edges lookup
////////////////////////////////////////////////////////////////////////////////

static void StringifyEdgesQuery (TRI_query_t* qry, TRI_string_buffer_t* buffer) {
  TRI_edges_query_t* query;

  query = (TRI_edges_query_t*) qry;

  switch (query->_direction) {
    case TRI_EDGE_IN:
      TRI_AppendStringStringBuffer(buffer, "inEdges(\"");
      break;

    case TRI_EDGE_OUT:
      TRI_AppendStringStringBuffer(buffer, "outEdges(\"");
      break;

    case TRI_EDGE_ANY:
      TRI_AppendStringStringBuffer(buffer, "edges(\"");
      break;

    case TRI_EDGE_UNUSED:
      TRI_AppendStringStringBuffer(buffer, "nonEdges(\"");
      break;
  }

  TRI_AppendUInt64StringBuffer(buffer, query->base._collection->_cid);

  TRI_AppendStringStringBuffer(buffer, "\")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones an edges lookup
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneEdgesQuery (TRI_query_t* qry) {
  TRI_edges_query_t* clone;
  TRI_edges_query_t* query;

  clone = TRI_Allocate(sizeof(TRI_edges_query_t));
  query = (TRI_edges_query_t*) qry;

  CloneQuery(&clone->base, &query->base);

  clone->_operand = query->_operand == NULL ? NULL : query->_operand->clone(query->_operand);
  clone->_direction = query->_direction;

  return &clone->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a edges lookup
////////////////////////////////////////////////////////////////////////////////

static void FreeEdgesQuery (TRI_query_t* qry, bool descend) {
  TRI_edges_query_t* query;

  query = (TRI_edges_query_t*) qry;

  if (descend) {
    if (query->_operand != NULL) {
      query->_operand->free(query->_operand, descend);
    }
  }

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
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief optimises a geo-spatial hint
////////////////////////////////////////////////////////////////////////////////

static char* OptimiseGeoIndexQuery (TRI_query_t** qry) {
  TRI_geo_index_query_t* query;
  char* e;

  query = (TRI_geo_index_query_t*) *qry;

  // sanity check
  if (! query->base._collection->_loaded) {
    return TRI_Concatenate3String("collection \"", query->base._collection->_name, "\" not loaded");
  }

  // check if query is already optimised
  if (query->base._isOptimised) {
    return NULL;
  }

  // optimise operand
  e = query->_operand->optimise(&query->_operand);

  if (e != NULL) {
    return e;
  }

  query->base._isOptimised = true;
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a geo-spatial hint
////////////////////////////////////////////////////////////////////////////////

static void ExecuteGeoIndexQuery (TRI_query_t* qry,
                                  TRI_query_result_t* result) {
  TRI_geo_index_query_t* query;

  query = (TRI_geo_index_query_t*) qry;

  query->_operand->execute(query->_operand, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief json representation of a geo-spatial hint
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeoIndexQuery (TRI_query_t* qry) {
  TRI_json_t* json;
  TRI_geo_index_query_t* query;

  query = (TRI_geo_index_query_t*) qry;
  json = TRI_CreateArrayJson();

  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("geo"));
  TRI_Insert2ArrayJson(json, "operand", query->_operand->json(query->_operand));

  if (query->_location != NULL) {
    TRI_Insert2ArrayJson(json, "location", TRI_CreateStringCopyJson(query->_location));
    TRI_Insert2ArrayJson(json, "geoJson", TRI_CreateBooleanJson(query->_geoJson));
  }
  else if (query->_latitude != NULL && query->_longitude != NULL) {
    TRI_Insert2ArrayJson(json, "latitude", TRI_CreateStringCopyJson(query->_latitude));
    TRI_Insert2ArrayJson(json, "longitude", TRI_CreateStringCopyJson(query->_longitude));
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string representation of a geo-spatial hint
////////////////////////////////////////////////////////////////////////////////

static void StringifyGeoIndexQuery (TRI_query_t* qry, TRI_string_buffer_t* buffer) {
  TRI_geo_index_query_t* query;

  query = (TRI_geo_index_query_t*) qry;

  query->_operand->stringify(query->_operand, buffer);

  TRI_AppendStringStringBuffer(buffer, ".geo(");

  if (query->_location != NULL) {
    TRI_AppendStringStringBuffer(buffer, query->_location);
    TRI_AppendStringStringBuffer(buffer, ",");
    TRI_AppendStringStringBuffer(buffer, query->_geoJson ? "true" : "false");
  }
  else if (query->_latitude != NULL && query->_longitude != NULL) {
    TRI_AppendStringStringBuffer(buffer, query->_latitude);
    TRI_AppendStringStringBuffer(buffer, ",");
    TRI_AppendStringStringBuffer(buffer, query->_longitude);
  }

  TRI_AppendStringStringBuffer(buffer, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a geo-spatial hint
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneGeoIndexQuery (TRI_query_t* qry) {
  TRI_geo_index_query_t* clone;
  TRI_geo_index_query_t* query;

  clone = TRI_Allocate(sizeof(TRI_geo_index_query_t));
  query = (TRI_geo_index_query_t*) qry;

  CloneQuery(&clone->base, &query->base);

  clone->_operand = query->_operand->clone(query->_operand);

  clone->_location = query->_location == NULL ? NULL : TRI_DuplicateString(query->_location);
  clone->_latitude = query->_latitude == NULL ? NULL : TRI_DuplicateString(query->_latitude);
  clone->_longitude = query->_longitude == NULL ? NULL : TRI_DuplicateString(query->_longitude);

  clone->_geoJson = query->_geoJson;

  return &clone->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a geo-spatial hint
////////////////////////////////////////////////////////////////////////////////

static void FreeGeoIndeyQuery (TRI_query_t* qry, bool descend) {
  TRI_geo_index_query_t* query;

  query = (TRI_geo_index_query_t*) qry;

  if (descend) {
    query->_operand->free(query->_operand, descend);
  }

  if (query->_location != NULL) { TRI_Free(query->_location); }
  if (query->_latitude != NULL) { TRI_Free(query->_latitude); }
  if (query->_longitude != NULL) { TRI_Free(query->_longitude); }

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
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief optimises a limit query
////////////////////////////////////////////////////////////////////////////////

static char* OptimiseLimitQuery (TRI_query_t** qry) {
  TRI_limit_query_t* query;
  TRI_query_t* operand;
  char* e;

  query = (TRI_limit_query_t*) *qry;

  // sanity check
  if (! query->base._collection->_loaded) {
    return TRI_Concatenate3String("collection \"", query->base._collection->_name, "\" not loaded");
  }

  // check if query is already optimised
  if (query->base._isOptimised) {
    return NULL;
  }

  // optimise operand
  e = query->_operand->optimise(&query->_operand);

  if (e != NULL) {
    return e;
  }

  operand = query->_operand;

  // .............................................................................
  // operand is a NEAR
  // .............................................................................

  if (0 <= query->_limit && operand->_type == TRI_QUE_TYPE_NEAR) {
    TRI_near_query_t* near;

    near = (TRI_near_query_t*) operand;

    // no limit specified, that the limit
    if (near->_limit < 0) {
      near->_limit = query->_limit;
      near->base._isOptimised = false;
    }

    // modify near query
    else if (query->_limit < near->_limit) {
      near->_limit = query->_limit;
      near->base._isOptimised = false;
    }

    // free the query, but not the operands of the query
    query->base.free(&query->base, false);

    // change the query to its operand
    *qry = &near->base;

    // recure
    return (*qry)->optimise(qry);
  }

  // .............................................................................
  // operand is a LIMIT
  // .............................................................................

  else if (operand->_type == TRI_QUE_TYPE_LIMIT) {
    TRI_limit_query_t* op;

    op = (TRI_limit_query_t*) operand;

    // both limits are positive, use minimum
    if (query->_limit >= 0 && op->_limit >= 0) {
      if (query->_limit < op->_limit) {
        op->_limit = query->_limit;
        op->base._isOptimised = false;
      }

      // free the query, but not the operands of the query
      query->base.free(&query->base, false);

      // change the query to its operand
      *qry = &op->base;

      // recure
      return (*qry)->optimise(qry);
    }

    // both limits are negative, use maximum
    else if (query->_limit <= 0 && op->_limit <= 0) {
      if (query->_limit > op->_limit) {
        op->_limit = query->_limit;
        op->base._isOptimised = false;
      }

      // free the query, but not the operands of the query
      query->base.free(&query->base, false);

      // change the query to its operand
      *qry = &op->base;

      // recure
      return (*qry)->optimise(qry);
    }

    // operand has a smaller limit, forget query
    else if (labs(op->_limit) <= labs(query->_limit)) {

      // free the query, but not the operands of the query
      query->base.free(&query->base, false);

      // change the query to its operand
      *qry = &op->base;

      return (*qry)->optimise(qry);
    }
  }

  query->base._isOptimised = true;
  return NULL;
}

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
/// @brief json representation of a limit query
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonLimitQuery (TRI_query_t* qry) {
  TRI_json_t* json;
  TRI_limit_query_t* query;

  query = (TRI_limit_query_t*) qry;
  json = TRI_CreateArrayJson();

  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("limit"));
  TRI_Insert2ArrayJson(json, "limit", TRI_CreateNumberJson(query->_limit));
  TRI_Insert2ArrayJson(json, "operand", query->_operand->json(query->_operand));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string representation of a limit query
////////////////////////////////////////////////////////////////////////////////

static void StringifyLimitQuery (TRI_query_t* qry, TRI_string_buffer_t* buffer) {
  TRI_limit_query_t* query;

  query = (TRI_limit_query_t*) qry;

  query->_operand->stringify(query->_operand, buffer);

  TRI_AppendStringStringBuffer(buffer, ".limit(");
  TRI_AppendInt64StringBuffer(buffer, query->_limit);
  TRI_AppendStringStringBuffer(buffer, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a limit query
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneLimitQuery (TRI_query_t* qry) {
  TRI_limit_query_t* clone;
  TRI_limit_query_t* query;

  clone = TRI_Allocate(sizeof(TRI_limit_query_t));
  query = (TRI_limit_query_t*) qry;

  CloneQuery(&clone->base, &query->base);

  clone->_operand = query->_operand->clone(query->_operand);
  clone->_limit = query->_limit;

  return &clone->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a limit query
////////////////////////////////////////////////////////////////////////////////

static void FreeLimitQuery (TRI_query_t* qry, bool descend) {
  TRI_limit_query_t* query;

  query = (TRI_limit_query_t*) qry;

  if (descend) {
    query->_operand->free(query->_operand, descend);
  }

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
/// @addtogroup VocBase
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
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief locates a suitable geo index
////////////////////////////////////////////////////////////////////////////////

static TRI_geo_index_t* LocateGeoIndex (TRI_query_t* query) {
  TRI_col_type_e type;
  TRI_index_t* idx2;
  TRI_index_t* idx;
  TRI_sim_collection_t* collection;
  size_t i;

  // sanity check
  if (! query->_collection->_loaded) {
    return NULL;
  }

  type = query->_collection->_collection->base._type;

  if (type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return NULL;
  }

  collection = (TRI_sim_collection_t*) query->_collection->_collection;

  // return the geo index
  idx = NULL;

  for (i = 0;  i < collection->_indexes._length;  ++i) {
    idx2 = collection->_indexes._buffer[i];

    if (idx2->_type == TRI_IDX_TYPE_GEO_INDEX) {
      if (idx == NULL) {
        idx = idx2;
      }
      else if (idx2->_iid < idx->_iid) {
        idx = idx2;
      }
    }
  }

  return (TRI_geo_index_t*) idx;
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

#include <BasicsC/fsrt.inc>

////////////////////////////////////////////////////////////////////////////////
/// @brief uses a specific geo-spatial index
////////////////////////////////////////////////////////////////////////////////

static TRI_geo_index_t* LookupGeoIndex (TRI_geo_index_query_t* query) {
  TRI_col_type_e type;
  TRI_doc_collection_t* collection;
  TRI_index_t* idx;
  TRI_shape_pid_t lat = 0;
  TRI_shape_pid_t loc = 0;
  TRI_shape_pid_t lon = 0;
  TRI_shaper_t* shaper;
  TRI_sim_collection_t* sim;

  // lookup an geo index
  collection = query->base._collection->_collection;

  type = collection->base._type;

  if (type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
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
    idx = TRI_LookupGeoIndexSimCollection(sim, loc, query->_geoJson);
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
/// @brief optimises a near query
////////////////////////////////////////////////////////////////////////////////

static char* OptimiseNearQuery (TRI_query_t** qry) {
  TRI_near_query_t* query;
  char* e;

  query = (TRI_near_query_t*) *qry;

  // sanity check
  if (! query->base._collection->_loaded) {
    return TRI_Concatenate3String("collection \"", query->base._collection->_name, "\" not loaded");
  }

  // check if query is already optimised
  if (query->base._isOptimised) {
    return NULL;
  }

  // optimise operand
  e = query->_operand->optimise(&query->_operand);

  if (e != NULL) {
    return e;
  }

  query->base._isOptimised = true;
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a near query
////////////////////////////////////////////////////////////////////////////////

static void ExecuteNearQuery (TRI_query_t* qry,
                              TRI_query_result_t* result) {
  static size_t const DEFAULT_LIMIT = 100;

  GeoCoordinates* cors;
  TRI_geo_index_t* idx;
  TRI_near_query_t* query;
  TRI_query_t* operand;

  query = (TRI_near_query_t*) qry;
  operand = query->_operand;

  // free any old results
  FreeAugmented(result);
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
  if (query->_limit < 0) {
    cors = TRI_NearestGeoIndex(&idx->base, query->_latitude, query->_longitude, DEFAULT_LIMIT);
  }
  else {
    cors = TRI_NearestGeoIndex(&idx->base, query->_latitude, query->_longitude, query->_limit);
  }

  // and append result
  GeoResult(cors, query->_distance, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief json representation of a near lookup
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonNearQuery (TRI_query_t* qry) {
  TRI_json_t* json;
  TRI_near_query_t* query;

  query = (TRI_near_query_t*) qry;
  json = TRI_CreateArrayJson();

  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("near"));
  TRI_Insert2ArrayJson(json, "operand", query->_operand->json(query->_operand));
  TRI_Insert2ArrayJson(json, "latitude", TRI_CreateNumberJson(query->_latitude));
  TRI_Insert2ArrayJson(json, "longitude", TRI_CreateNumberJson(query->_longitude));
  TRI_Insert2ArrayJson(json, "limit", TRI_CreateNumberJson(query->_limit));

  if (query->_distance != NULL) {
    TRI_Insert2ArrayJson(json, "distance", TRI_CreateStringCopyJson(query->_distance));
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string representation of a near query
////////////////////////////////////////////////////////////////////////////////

static void StringifyNearQuery (TRI_query_t* qry, TRI_string_buffer_t* buffer) {
  TRI_near_query_t* query;

  query = (TRI_near_query_t*) qry;

  query->_operand->stringify(query->_operand, buffer);

  TRI_AppendStringStringBuffer(buffer, ".near(");
  TRI_AppendDoubleStringBuffer(buffer, query->_latitude);
  TRI_AppendStringStringBuffer(buffer, ",");
  TRI_AppendDoubleStringBuffer(buffer, query->_longitude);
  TRI_AppendStringStringBuffer(buffer, ")");

  if (query->_distance != NULL) {
    TRI_AppendStringStringBuffer(buffer, ".distance(");
    TRI_AppendStringStringBuffer(buffer, query->_distance);
    TRI_AppendStringStringBuffer(buffer, ")");
  }

  if (0 <= query->_limit) {
    TRI_AppendStringStringBuffer(buffer, ".limit(");
    TRI_AppendUInt64StringBuffer(buffer, query->_limit);
    TRI_AppendStringStringBuffer(buffer, ")");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a near query
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneNearQuery (TRI_query_t* qry) {
  TRI_near_query_t* clone;
  TRI_near_query_t* query;

  clone = TRI_Allocate(sizeof(TRI_near_query_t));
  query = (TRI_near_query_t*) qry;

  CloneQuery(&clone->base, &query->base);

  clone->_operand = query->_operand->clone(query->_operand);
  clone->_latitude = query->_latitude;
  clone->_longitude = query->_longitude;
  clone->_limit = query->_limit;
  clone->_distance = query->_distance == NULL ? NULL : TRI_DuplicateString(query->_distance);

  return &clone->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a near query
////////////////////////////////////////////////////////////////////////////////

static void FreeNearQuery (TRI_query_t* qry, bool descend) {
  TRI_near_query_t* query;

  query = (TRI_near_query_t*) qry;

  if (descend) {
    query->_operand->free(query->_operand, descend);
  }

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
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief optimises a skip query
////////////////////////////////////////////////////////////////////////////////

static char* OptimiseSkipQuery (TRI_query_t** qry) {
  TRI_skip_query_t* query;
  TRI_query_t* operand;
  char* e;

  query = (TRI_skip_query_t*) *qry;

  // sanity check
  if (! query->base._collection->_loaded) {
    return TRI_Concatenate3String("collection \"", query->base._collection->_name, "\" not loaded");
  }

  // check if query is already optimised
  if (query->base._isOptimised) {
    return NULL;
  }

  // optimise operand
  e = query->_operand->optimise(&query->_operand);

  if (e != NULL) {
    return e;
  }

  operand = query->_operand;

  // .............................................................................
  // no skip at all
  // .............................................................................

  if (query->_skip == 0) {
    TRI_skip_query_t* op;

    op = (TRI_skip_query_t*) operand;

    // free the query, but not the operands of the query
    query->base.free(&query->base, false);

    // change the query to its operand
    *qry = &op->base;

    // done
    return NULL;
  }

  // .............................................................................
  // operand is a SKIP
  // .............................................................................

  if (operand->_type == TRI_QUE_TYPE_SKIP) {
    TRI_skip_query_t* op;

    op = (TRI_skip_query_t*) operand;

    op->_skip = op->_skip + query->_skip;
    op->base._isOptimised = false;

    // free the query, but not the operands of the query
    query->base.free(&query->base, false);

    // change the query to its operand
    *qry = &op->base;

    // recure
    return (*qry)->optimise(qry);
  }

  query->base._isOptimised = true;
  return NULL;
}

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

    FreeAugmented(result);
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
/// @brief json representation of a skip query
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonSkipQuery (TRI_query_t* qry) {
  TRI_json_t* json;
  TRI_skip_query_t* query;

  query = (TRI_skip_query_t*) qry;
  json = TRI_CreateArrayJson();

  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("skip"));
  TRI_Insert2ArrayJson(json, "skip", TRI_CreateNumberJson(query->_skip));
  TRI_Insert2ArrayJson(json, "operand", query->_operand->json(query->_operand));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string representation of a skip query
////////////////////////////////////////////////////////////////////////////////

static void StringifySkipQuery (TRI_query_t* qry, TRI_string_buffer_t* buffer) {
  TRI_skip_query_t* query;

  query = (TRI_skip_query_t*) qry;

  query->_operand->stringify(query->_operand, buffer);

  TRI_AppendStringStringBuffer(buffer, ".skip(");
  TRI_AppendUInt64StringBuffer(buffer, query->_skip);
  TRI_AppendStringStringBuffer(buffer, ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a skip query
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneSkipQuery (TRI_query_t* qry) {
  TRI_skip_query_t* clone;
  TRI_skip_query_t* query;

  clone = TRI_Allocate(sizeof(TRI_skip_query_t));
  query = (TRI_skip_query_t*) qry;

  CloneQuery(&clone->base, &query->base);

  clone->_operand = query->_operand->clone(query->_operand);
  clone->_skip = query->_skip;

  return &clone->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a skip query
////////////////////////////////////////////////////////////////////////////////

static void FreeSkipQuery (TRI_query_t* qry, bool descend) {
  TRI_skip_query_t* query;

  query = (TRI_skip_query_t*) qry;

  if (descend) {
    query->_operand->free(query->_operand, descend);
  }

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
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief optimises a within query
////////////////////////////////////////////////////////////////////////////////

static char* OptimiseWithinQuery (TRI_query_t** qry) {
  TRI_within_query_t* query;
  char* e;

  query = (TRI_within_query_t*) *qry;

  // sanity check
  if (! query->base._collection->_loaded) {
    return TRI_Concatenate3String("collection \"", query->base._collection->_name, "\" not loaded");
  }

  // check if query is already optimised
  if (query->base._isOptimised) {
    return NULL;
  }

  // optimise operand
  e = query->_operand->optimise(&query->_operand);

  if (e != NULL) {
    return e;
  }

  query->base._isOptimised = true;
  return NULL;
}

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
  FreeAugmented(result);
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
/// @brief json representation of a within lookup
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonWithinQuery (TRI_query_t* qry) {
  TRI_json_t* json;
  TRI_within_query_t* query;

  query = (TRI_within_query_t*) qry;
  json = TRI_CreateArrayJson();

  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("within"));
  TRI_Insert2ArrayJson(json, "operand", query->_operand->json(query->_operand));
  TRI_Insert2ArrayJson(json, "latitude", TRI_CreateNumberJson(query->_latitude));
  TRI_Insert2ArrayJson(json, "longitude", TRI_CreateNumberJson(query->_longitude));
  TRI_Insert2ArrayJson(json, "radius", TRI_CreateNumberJson(query->_radius));

  if (query->_distance != NULL) {
    TRI_Insert2ArrayJson(json, "distance", TRI_CreateStringCopyJson(query->_distance));
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string representation of a within query
////////////////////////////////////////////////////////////////////////////////

static void StringifyWithinQuery (TRI_query_t* qry, TRI_string_buffer_t* buffer) {
  TRI_within_query_t* query;

  query = (TRI_within_query_t*) qry;

  query->_operand->stringify(query->_operand, buffer);

  TRI_AppendStringStringBuffer(buffer, ".within(");
  TRI_AppendDoubleStringBuffer(buffer, query->_latitude);
  TRI_AppendStringStringBuffer(buffer, ",");
  TRI_AppendDoubleStringBuffer(buffer, query->_longitude);
  TRI_AppendStringStringBuffer(buffer, ",");
  TRI_AppendDoubleStringBuffer(buffer, query->_radius);
  TRI_AppendStringStringBuffer(buffer, ")");

  if (query->_distance != NULL) {
    TRI_AppendStringStringBuffer(buffer, ".distance(");
    TRI_AppendStringStringBuffer(buffer, query->_distance);
    TRI_AppendStringStringBuffer(buffer, ")");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a within query
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* CloneWithinQuery (TRI_query_t* qry) {
  TRI_within_query_t* clone;
  TRI_within_query_t* query;

  clone = TRI_Allocate(sizeof(TRI_within_query_t));
  query = (TRI_within_query_t*) qry;

  CloneQuery(&clone->base, &query->base);

  clone->_operand = query->_operand->clone(query->_operand);
  clone->_latitude = query->_latitude;
  clone->_longitude = query->_longitude;
  clone->_radius = query->_radius;
  clone->_distance = query->_distance == NULL ? NULL : TRI_DuplicateString(query->_distance);

  return &clone->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a within query
////////////////////////////////////////////////////////////////////////////////

static void FreeWithinQuery (TRI_query_t* qry, bool descend) {
  TRI_within_query_t* query;

  query = (TRI_within_query_t*) qry;

  if (descend) {
    query->_operand->free(query->_operand, descend);
  }

  TRI_Free(query);
}

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

TRI_query_t* TRI_CreateCollectionQuery (TRI_vocbase_col_t const* collection) {
  TRI_collection_query_t* query;

  query = TRI_Allocate(sizeof(TRI_collection_query_t));

  query->base._type = TRI_QUE_TYPE_COLLECTION;
  query->base._collection = collection;
  query->base._isOptimised = false;

  query->base.optimise = OptimiseCollectionQuery;
  query->base.execute = ExecuteCollectionQuery;
  query->base.json = JsonCollectionQuery;
  query->base.stringify = StringifyCollectionQuery;
  query->base.clone = CloneCollectionQuery;
  query->base.free = FreeCollectionQuery;

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the distance to a query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateDistanceQuery (TRI_query_t* operand, char const* distance) {
  TRI_distance_query_t* query;

  query = TRI_Allocate(sizeof(TRI_distance_query_t));

  query->base._type = TRI_QUE_TYPE_DISTANCE;
  query->base._collection = operand->_collection;
  query->base._isOptimised = false;

  query->base.optimise = OptimiseDistanceQuery;
  query->base.execute = ExecuteDistanceQuery;
  query->base.json = JsonDistanceQuery;
  query->base.stringify = StringifyDistanceQuery;
  query->base.clone = CloneDistanceQuery;
  query->base.free = FreeDistanceQuery;

  query->_operand = operand;
  query->_distance = TRI_DuplicateString(distance);

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
  query->base._isOptimised = false;

  query->base.optimise = OptimiseDocumentQuery;
  query->base.execute = ExecuteDocumentQuery;
  query->base.json = JsonDocumentQuery;
  query->base.stringify = StringifyDocumentQuery;
  query->base.clone = CloneDocumentQuery;
  query->base.free = FreeDocumentQuery;

  query->_operand = operand;
  query->_did = did;

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateEdgesQuery (TRI_vocbase_col_t const* edges,
                                   TRI_query_t* operand,
                                   TRI_edge_direction_e direction) {
  TRI_edges_query_t* query;

  query = TRI_Allocate(sizeof(TRI_edges_query_t));

  query->base._type = TRI_QUE_TYPE_EDGES;
  query->base._collection = edges;
  query->base._isOptimised = false;

  query->base.optimise = OptimiseEdgesQuery;
  query->base.execute = ExecuteEdgesQuery;
  query->base.json = JsonEdgesQuery;
  query->base.stringify = StringifyEdgesQuery;
  query->base.clone = CloneEdgesQuery;
  query->base.free = FreeEdgesQuery;

  query->_operand = operand;
  query->_direction = direction;

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an geo-spatial index as hint
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateGeoIndexQuery (TRI_query_t* operand,
                                      char const* location,
                                      char const* latitude,
                                      char const* longitude,
                                      bool geoJson) {
  TRI_geo_index_query_t* query;

  query = TRI_Allocate(sizeof(TRI_geo_index_query_t));

  query->base._type = TRI_QUE_TYPE_GEO_INDEX;
  query->base._collection = operand->_collection;
  query->base._isOptimised = false;

  query->base.optimise = OptimiseGeoIndexQuery;
  query->base.execute = ExecuteGeoIndexQuery;
  query->base.json = JsonGeoIndexQuery;
  query->base.stringify = StringifyGeoIndexQuery;
  query->base.clone = CloneGeoIndexQuery;
  query->base.free = FreeGeoIndeyQuery;

  query->_operand = operand;

  query->_location = (location == NULL ? NULL : TRI_DuplicateString(location));
  query->_geoJson = geoJson;

  query->_latitude = (latitude == NULL ? NULL : TRI_DuplicateString(latitude));
  query->_longitude = (longitude == NULL ? NULL : TRI_DuplicateString(longitude));

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
  query->base._isOptimised = false;

  query->base.optimise = OptimiseLimitQuery;
  query->base.execute = ExecuteLimitQuery;
  query->base.json = JsonLimitQuery;
  query->base.stringify = StringifyLimitQuery;
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
                                  char const* distance) {
  TRI_near_query_t* query;

  query = TRI_Allocate(sizeof(TRI_near_query_t));

  query->base._type = TRI_QUE_TYPE_NEAR;
  query->base._collection = operand->_collection;
  query->base._isOptimised = false;

  query->base.optimise = OptimiseNearQuery;
  query->base.execute = ExecuteNearQuery;
  query->base.json = JsonNearQuery;
  query->base.stringify = StringifyNearQuery;
  query->base.clone = CloneNearQuery;
  query->base.free = FreeNearQuery;

  query->_operand = operand;
  query->_latitude = latitude;
  query->_longitude = longitude;
  query->_limit = -1;
  query->_distance = (distance == NULL ? NULL : TRI_DuplicateString(distance));

  return &query->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skips elements of an existing query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateSkipQuery (TRI_query_t* operand, TRI_voc_size_t skip) {
  TRI_skip_query_t* query;

  query = TRI_Allocate(sizeof(TRI_skip_query_t));

  query->base._type = TRI_QUE_TYPE_SKIP;
  query->base._collection = operand->_collection;
  query->base._isOptimised = false;

  query->base.optimise = OptimiseSkipQuery;
  query->base.execute = ExecuteSkipQuery;
  query->base.json = JsonSkipQuery;
  query->base.stringify = StringifySkipQuery;
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
  TRI_within_query_t* query;

  query = TRI_Allocate(sizeof(TRI_within_query_t));

  query->base._type = TRI_QUE_TYPE_WITHIN;
  query->base._collection = operand->_collection;
  query->base._isOptimised = false;

  query->base.optimise = OptimiseWithinQuery;
  query->base.execute = ExecuteWithinQuery;
  query->base.json = JsonWithinQuery;
  query->base.stringify = StringifyWithinQuery;
  query->base.clone = CloneWithinQuery;
  query->base.free = FreeWithinQuery;

  query->_operand = operand;
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
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query
////////////////////////////////////////////////////////////////////////////////

TRI_result_set_t* TRI_ExecuteQuery (TRI_query_t* query) {
  TRI_doc_collection_t* collection = query->_collection->_collection;
  TRI_rs_container_element_t* ce;
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

  ce = TRI_AddResultSetRSContainer(&collection->_resultSets);

  query->execute(query, &result);

  if (result._error) {
    LOG_TRACE("query returned error: %s", result._error);
    rs = TRI_CreateRSSingle(collection, ce, NULL, 0);
    rs->_error = result._error;

    FreeAugmented(&result);
  }
  else {
    LOG_TRACE("query returned '%lu' documents", (unsigned long) result._length);
    rs = TRI_CreateRSVector(collection,
                            ce,
                            result._documents,
                            result._augmented,
                            result._length,
                            result._total);
  }

  ce->_resultSet = rs;

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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
