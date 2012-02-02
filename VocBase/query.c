////////////////////////////////////////////////////////////////////////////////
/// @brief query
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "query.h"

#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "VocBase/simple-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                   SELECT DOCUMENT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query selection for unaltered documents
////////////////////////////////////////////////////////////////////////////////

static TRI_qry_select_t* CloneQuerySelectDocument (TRI_qry_select_t const* s) {
  return TRI_CreateQuerySelectDocument();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a query selection for unaltered documents
////////////////////////////////////////////////////////////////////////////////

static void FreeQuerySelectDocument (TRI_qry_select_t* s) {
  TRI_qry_select_direct_t* selectClause;

  selectClause = (TRI_qry_select_direct_t*) s;

  TRI_Free(selectClause);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts result to shaped json
////////////////////////////////////////////////////////////////////////////////

static TRI_shaped_json_t* ShapedJsonSelectDocument (TRI_qry_select_t* s,
                                                    TRI_rc_result_t* document) {
  return &document->_primary->_document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts result to JavaScript
////////////////////////////////////////////////////////////////////////////////

static bool ToJavaScriptSelectDocument (TRI_qry_select_t* s,
                                        TRI_rc_result_t* document,
                                        void* storage) {
  return TRI_ObjectDocumentPointer(document->_context->_primary, document->_primary, storage);
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
/// @brief creates a query selection for unaltered documents
////////////////////////////////////////////////////////////////////////////////

TRI_qry_select_t* TRI_CreateQuerySelectDocument () {
  TRI_qry_select_direct_t* result;

  result = TRI_Allocate(sizeof(TRI_qry_select_direct_t));

  result->base._type = TRI_QRY_SELECT_DOCUMENT;

  result->base.clone = CloneQuerySelectDocument;
  result->base.free = FreeQuerySelectDocument;
  result->base.shapedJson = ShapedJsonSelectDocument;
  result->base.toJavaScript = ToJavaScriptSelectDocument;

  return &result->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    SELECT GENERAL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query selection for general documents
////////////////////////////////////////////////////////////////////////////////

static TRI_qry_select_t* CloneQuerySelectGeneral (TRI_qry_select_t const* s) {
  TRI_qry_select_general_t* selectClause;

  selectClause = (TRI_qry_select_general_t*) s;

  return TRI_CreateQuerySelectGeneral(selectClause->_code);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a query selection for general documents
////////////////////////////////////////////////////////////////////////////////

static void FreeQuerySelectGeneral (TRI_qry_select_t* s) {
  TRI_qry_select_general_t* selectClause;

  selectClause = (TRI_qry_select_general_t*) s;

  TRI_FreeString(selectClause->_code);
  TRI_Free(selectClause);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts result to shaped json
////////////////////////////////////////////////////////////////////////////////

static TRI_shaped_json_t* ShapedJsonSelectGeneral (TRI_qry_select_t* s,
                                                   TRI_rc_result_t* document) {
  return &document->_primary->_document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts result to JavaScript
////////////////////////////////////////////////////////////////////////////////

static bool ToJavaScriptSelectGeneral (TRI_qry_select_t* s,
                                       TRI_rc_result_t* document,
                                       void* storage) {
  TRI_rc_context_t* context;
  TRI_js_exec_context_t* execContext;

  context = document->_context;
  execContext = context->_selectClause;

  if (execContext == NULL) {
    LOG_ERROR("no JavaScript context for select is known");
    return false;
  }

  // first define the augumentation, they might get overwritten
  if (document->_augmention._type == TRI_JSON_ARRAY) {
    TRI_DefineJsonArrayExecutionContext(execContext, &document->_augmention);
  }

  // define the primary collection
  TRI_DefineDocumentExecutionContext(execContext, context->_primaryName, context->_primary, document->_primary);

  // execute everything
  return TRI_ExecuteExecutionContext(execContext, storage);
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
/// @brief creates a query selection for general, generated documents
////////////////////////////////////////////////////////////////////////////////

TRI_qry_select_t* TRI_CreateQuerySelectGeneral (char const* clause) {
  TRI_qry_select_general_t* result;

  result = TRI_Allocate(sizeof(TRI_qry_select_general_t));

  result->base._type = TRI_QRY_SELECT_GENERAL;

  result->base.clone = CloneQuerySelectGeneral;
  result->base.free = FreeQuerySelectGeneral;
  result->base.shapedJson = ShapedJsonSelectGeneral;
  result->base.toJavaScript = ToJavaScriptSelectGeneral;

  result->_code = TRI_DuplicateString(clause);

  return &result->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    DOCUMENTS LIST
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief list join
////////////////////////////////////////////////////////////////////////////////

typedef struct doc_mptr_list_s {
  uint32_t _length;
  TRI_doc_mptr_t* _documents;
}
doc_mptr_list_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of all documents
////////////////////////////////////////////////////////////////////////////////

typedef struct collection_cursor_s {
  TRI_rc_cursor_t base;

  TRI_voc_size_t _length;

  size_t _nListJoins;

  TRI_doc_mptr_t* _documents;
  TRI_doc_mptr_t* _current;
  TRI_doc_mptr_t* _end;

  doc_mptr_list_t* _listJoins;
  doc_mptr_list_t* _listJoinsCurrent;

  TRI_json_t* _augmentions;
  TRI_json_t* _currentAugmention;

  TRI_rc_result_t _result;
}
collection_cursor_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief fill function
////////////////////////////////////////////////////////////////////////////////

typedef void (*cond_fptr) (collection_cursor_t*,
                           TRI_sim_collection_t*,
                           TRI_rc_context_t*,
                           TRI_qry_where_t*,
                           TRI_voc_size_t,
                           TRI_voc_ssize_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief order by function
////////////////////////////////////////////////////////////////////////////////

typedef void (*order_fptr) (collection_cursor_t*,
                            TRI_sim_collection_t*,
                            TRI_rc_context_t*,
                            TRI_qry_order_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next element
////////////////////////////////////////////////////////////////////////////////

static TRI_rc_result_t* NextCollectionCursor (TRI_rc_cursor_t* c) {
  collection_cursor_t* cursor;

  cursor = (collection_cursor_t*) c;

  if (cursor->_current < cursor->_end) {
    cursor->_result._primary = cursor->_current++;

    if (cursor->_augmentions != NULL) {
      cursor->_result._augmention = *cursor->_currentAugmention++;
    }

    return &cursor->_result;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
////////////////////////////////////////////////////////////////////////////////

static bool HasNextCollectionCursor (TRI_rc_cursor_t* c) {
  collection_cursor_t* cursor;

  cursor = (collection_cursor_t*) c;

  return cursor->_current < cursor->_end;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a cursor
////////////////////////////////////////////////////////////////////////////////

static void FreeCollectionCursor (TRI_rc_cursor_t* c) {
  collection_cursor_t* cursor;
  TRI_qry_select_t* slct;
  size_t i;

  cursor = (collection_cursor_t*) c;
  slct = cursor->base._select;

  slct->free(slct);

  TRI_RemoveCollectionsCursor(&cursor->base);

  TRI_DestroyVectorPointer(&cursor->base._containers);

  TRI_FreeContextQuery(cursor->base._context);

  TRI_Free(cursor->_documents);

  if (cursor->_augmentions != NULL) {
    for (i = 0;  i < cursor->_length;  ++i) {
      TRI_DestroyJson(&cursor->_augmentions[i]);
    }

    TRI_Free(cursor->_augmentions);
  }

  TRI_Free(cursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief applys a skip and limit
////////////////////////////////////////////////////////////////////////////////

static void TransformDataSkipLimit (collection_cursor_t* result,
                                    TRI_voc_size_t skip,
                                    TRI_voc_ssize_t limit) {
  TRI_voc_size_t length;
  TRI_doc_mptr_t const* src;

  // return nothing
  if (limit == 0) {
    result->_length = 0;
    result->_end = result->_documents + result->_length;
    return;
  }

  if (result->_length <= skip) {
    result->_length = 0;
    result->_end = result->_documents + result->_length;
    return;
  }

  // positive limit, skip and slice from front
  if (0 < limit) {
    src = result->_documents + skip;

    if (result->_length - skip < limit) {
      length = result->_length - skip;
    }
    else {
      length = limit;
    }
  }

  // negative limit, slice from back
  else {

    if (result->_length - skip < -limit) {
      src = result->_documents;
      length = result->_length - skip;
    }
    else {
      src = result->_documents + (result->_length - skip + limit);
      length = -limit;
    }
  }

  // and move
  if (result->_documents != src) {
    memmove(result->_documents, src, length * sizeof(TRI_doc_mptr_t));
  }

  result->_length = length;
  result->_end = result->_documents + result->_length;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     WHERE BOOLEAN
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query condition for constant conditions
////////////////////////////////////////////////////////////////////////////////

static TRI_qry_where_t* CloneQueryWhereBoolean (TRI_qry_where_t const* w) {
  TRI_qry_where_boolean_t* whereClause;

  whereClause = (TRI_qry_where_boolean_t*) w;

  return TRI_CreateQueryWhereBoolean(whereClause->_value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a query condition for constant conditions
////////////////////////////////////////////////////////////////////////////////

static void FreeQueryWhereBoolean (TRI_qry_where_t* w) {
  TRI_qry_where_boolean_t* whereClause;

  whereClause = (TRI_qry_where_boolean_t*) w;

  TRI_Free(whereClause);
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
/// @brief creates a query condition for constant conditions
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereBoolean (bool where) {
  TRI_qry_where_boolean_t* result;

  result = TRI_Allocate(sizeof(TRI_qry_where_boolean_t));

  result->base.base._type = TRI_QRY_WHERE_BOOLEAN;

  result->base.base.clone = CloneQueryWhereBoolean;
  result->base.base.free = FreeQueryWhereBoolean;

  result->base.checkCondition = NULL;

  result->_value = where;

  return &result->base.base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                 WHERE PRIMARY INDEX WITH CONSTANT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query condition using the primary index and a constant
////////////////////////////////////////////////////////////////////////////////

static TRI_qry_where_t* CloneQueryWherePrimaryConstant (TRI_qry_where_t const* w) {
  TRI_qry_where_primary_const_t* whereClause;

  whereClause = (TRI_qry_where_primary_const_t*) w;

  return TRI_CreateQueryWherePrimaryConstant(whereClause->_did);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a query condition using the primary index and a constant
////////////////////////////////////////////////////////////////////////////////

static void FreeQueryWherePrimaryConstant (TRI_qry_where_t* w) {
  TRI_qry_where_primary_const_t* whereClause;

  whereClause = (TRI_qry_where_primary_const_t*) w;

  TRI_Free(whereClause);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns document identifier for a constant
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_did_t DidQueryWherePrimaryConstant (TRI_qry_where_primary_t* w,
                                                   TRI_rc_context_t* context) {
  TRI_qry_where_primary_const_t* whereClause;

  whereClause = (TRI_qry_where_primary_const_t*) w;

  return whereClause->_did;
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
/// @brief creates a query condition using the primary index and a constant
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWherePrimaryConstant (TRI_voc_did_t did) {
  TRI_qry_where_primary_const_t* result;

  result = TRI_Allocate(sizeof(TRI_qry_where_primary_const_t));

  result->base.base._type = TRI_QRY_WHERE_PRIMARY_CONSTANT;

  result->base.base.clone = CloneQueryWherePrimaryConstant;
  result->base.base.free = FreeQueryWherePrimaryConstant;

  result->base.did = DidQueryWherePrimaryConstant;

  result->_did = did;

  return &result->base.base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     WHERE GENERAL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query condition for general, JavaScript conditions
////////////////////////////////////////////////////////////////////////////////

static TRI_qry_where_t* CloneQueryWhereGeneral (TRI_qry_where_t const* w) {
  TRI_qry_where_general_t* whereClause;

  whereClause = (TRI_qry_where_general_t*) w;

  return TRI_CreateQueryWhereGeneral(whereClause->_code);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a query condition for general, JavaScript conditions
////////////////////////////////////////////////////////////////////////////////

static void FreeQueryWhereGeneral (TRI_qry_where_t* w) {
  TRI_qry_where_general_t* whereClause;

  whereClause = (TRI_qry_where_general_t*) w;

  TRI_FreeString(whereClause->_code);
  TRI_Free(whereClause);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the where condition
////////////////////////////////////////////////////////////////////////////////

static bool CheckConditionWhereGeneral (TRI_qry_where_cond_t* w,
                                        TRI_rc_context_t* context,
                                        TRI_doc_mptr_t const* document) {
  TRI_js_exec_context_t* execContext;
  bool result;
  bool ok;

  execContext = context->_whereClause;

  if (execContext == NULL) {
    LOG_ERROR("no JavaScript context for where is known");
    return false;
  }

  TRI_DefineDocumentExecutionContext(execContext, context->_primaryName, context->_primary, document);

  ok = TRI_ExecuteConditionExecutionContext(execContext, &result);

  if (! ok) {
    LOG_ERROR("cannot execute JavaScript where clause");
    return false;
  }

  return result;
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
/// @brief creates a query condition for general, JavaScript conditions
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereGeneral (char const* clause) {
  TRI_qry_where_general_t* result;

  result = TRI_Allocate(sizeof(TRI_qry_where_general_t));

  result->base.base._type = TRI_QRY_WHERE_GENERAL;

  result->base.base.clone = CloneQueryWhereGeneral;
  result->base.base.free = FreeQueryWhereGeneral;

  result->base.checkCondition = CheckConditionWhereGeneral;

  result->_code = TRI_DuplicateString(clause);

  return &result->base.base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            WHERE WITHIN GEO INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query condition using the geo index and constants
////////////////////////////////////////////////////////////////////////////////

static TRI_qry_where_t* CloneQueryWhereWithinConstant (TRI_qry_where_t const* w) {
  TRI_qry_where_within_const_t* whereClause;

  whereClause = (TRI_qry_where_within_const_t*) w;

  return TRI_CreateQueryWhereWithinConstant(whereClause->base._iid,
                                            whereClause->base._nameDistance,
                                            whereClause->_coordinates[0],
                                            whereClause->_coordinates[1],
                                            whereClause->_radius);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a query condition using the geo index and constants
////////////////////////////////////////////////////////////////////////////////

static void FreeQueryWhereWithinConstant (TRI_qry_where_t* w) {
  TRI_qry_where_within_const_t* whereClause;

  whereClause = (TRI_qry_where_within_const_t*) w;

  if (whereClause->base._nameDistance != NULL) {
    TRI_Free(whereClause->base._nameDistance);
  }

  TRI_Free(whereClause);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the lattitude and longitude for constants
////////////////////////////////////////////////////////////////////////////////

static double* CoordinatesQueryWhereWithinConstant (TRI_qry_where_within_t* w,
                                                    TRI_rc_context_t* context) {
  TRI_qry_where_within_const_t* whereClause;

  whereClause = (TRI_qry_where_within_const_t*) w;

  return whereClause->_coordinates;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the radius for constants
////////////////////////////////////////////////////////////////////////////////

static double RadiusQueryWhereWithinConstant (TRI_qry_where_within_t* w,
                                              TRI_rc_context_t* context) {
  TRI_qry_where_within_const_t* whereClause;
  
  whereClause = (TRI_qry_where_within_const_t*) w;

  return whereClause->_radius;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create result from geo result
////////////////////////////////////////////////////////////////////////////////

static void StoreGeoResult (collection_cursor_t* result,
                            GeoCoordinates* cors,
                            char const* attribute) {
  GeoCoordinate* end;
  GeoCoordinate* ptr;
  TRI_doc_mptr_t* wtr;
  TRI_json_t* atr;
  TRI_json_t num;
  double* dtr;
  size_t n;

  if (cors == NULL) {
    return;
  }

  n = cors->length;

  // copy the documents
  result->_documents = (TRI_doc_mptr_t*) (wtr = TRI_Allocate(sizeof(TRI_doc_mptr_t) * n));

  if (attribute != NULL) {
    result->_augmentions = (TRI_json_t*) (atr = TRI_Allocate(sizeof(TRI_json_t) * n));
  }

  ptr = cors->coordinates;
  end = cors->coordinates + n;

  if (attribute == NULL) {
    for (;  ptr < end;  ++ptr, ++wtr) {
      *wtr = * (TRI_doc_mptr_t*) ptr->data;
    }
  }
  else {
    dtr = cors->distances;

    for (;  ptr < end;  ++ptr, ++wtr, ++atr, ++dtr) {
      *wtr = * (TRI_doc_mptr_t*) ptr->data;

      TRI_InitArrayJson(atr);
      TRI_InitNumberJson(&num, *dtr);
      TRI_Insert2ArrayJson(atr, attribute, &num);
    }
  }

  GeoIndex_CoordinatesFree(cors);

  result->_length = n;
  result->base._scannedIndexEntries += n;

  result->_current = result->_documents;
  result->_end = result->_documents + result->_length;

  if (attribute != NULL) {
    result->_currentAugmention = result->_augmentions;
  }
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
/// @brief creates a query condition using the geo index and a constant
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereWithinConstant (TRI_idx_iid_t iid,
                                                     char const* nameDistance,
                                                     double latitiude,
                                                     double longitude,
                                                     double radius) {
  TRI_qry_where_within_const_t* result;

  result = TRI_Allocate(sizeof(TRI_qry_where_within_const_t));

  result->base.base._type = TRI_QRY_WHERE_WITHIN_CONSTANT;

  result->base.base.clone = CloneQueryWhereWithinConstant;
  result->base.base.free = FreeQueryWhereWithinConstant;

  result->base.coordinates = CoordinatesQueryWhereWithinConstant;
  result->base.radius = RadiusQueryWhereWithinConstant;

  result->base._iid = iid;
  result->base._nameDistance = nameDistance == NULL ? NULL : TRI_DuplicateString(nameDistance);

  result->_coordinates[0] = latitiude;
  result->_coordinates[1] = longitude;
  result->_radius = radius;

  return &result->base.base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  FILTER FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lists all documents
////////////////////////////////////////////////////////////////////////////////

static void FilterDataCollectionQuery (collection_cursor_t* result,
                                       TRI_sim_collection_t* collection,
                                       TRI_rc_context_t* context,
                                       TRI_qry_where_t* where,
                                       TRI_voc_size_t skip,
                                       TRI_voc_ssize_t limit) {
  TRI_doc_mptr_t* qtr;
  size_t total;
  void** end;
  void** ptr;

  assert(where == NULL);
  assert(result->_documents == NULL);
  assert(0 <= limit);

  result->_documents = (qtr = TRI_Allocate(sizeof(TRI_doc_mptr_t) * (collection->_primaryIndex._nrUsed)));

  ptr = collection->_primaryIndex._table;
  end = collection->_primaryIndex._table + collection->_primaryIndex._nrAlloc;

  for (total = 0;  ptr < end && total < limit;  ++ptr) {
    ++result->base._scannedDocuments;

    if (*ptr) {
      TRI_doc_mptr_t* mptr = *ptr;

      if (mptr->_deletion == 0) {
        ++result->base._matchedDocuments;

        if (0 < skip) {
          --skip;
          continue;
        }

        *qtr++ = *mptr;
        total++;
      }
    }
  }

  result->_length = total;
  result->_current = result->_documents;
  result->_end = result->_documents + result->_length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lists no documents
////////////////////////////////////////////////////////////////////////////////

static void FilterDataNoneQuery (collection_cursor_t* result,
                                 TRI_sim_collection_t* collection,
                                 TRI_rc_context_t* context,
                                 TRI_qry_where_t* where,
                                 TRI_voc_size_t skip,
                                 TRI_voc_ssize_t limit) {
  assert(where == NULL);
  assert(result->_documents == NULL);

  result->_documents = TRI_Allocate(sizeof(TRI_doc_mptr_t));

  result->_length = 0;
  result->_current = result->_documents;
  result->_end = result->_documents + result->_length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lists document from primary index
////////////////////////////////////////////////////////////////////////////////

static void FilterDataPrimaryQuery (collection_cursor_t* result,
                                    TRI_sim_collection_t* collection,
                                    TRI_rc_context_t* context,
                                    TRI_qry_where_t* w,
                                    TRI_voc_size_t skip,
                                    TRI_voc_ssize_t limit) {
  TRI_doc_mptr_t const* document;
  TRI_qry_where_primary_const_t* where;
  TRI_voc_did_t did;

  assert(w != NULL);
  assert(w->_type == TRI_QRY_WHERE_PRIMARY_CONSTANT);
  assert(result->_documents == NULL);
  assert(0 <= limit);

  where = (TRI_qry_where_primary_const_t*) w;

  result->_documents = TRI_Allocate(sizeof(TRI_doc_mptr_t));

  // strange cases
  if (limit == 0 || 0 < skip) {
    result->_length = 0;
  }

  // look up the document
  else {
    did = where->base.did(&where->base, context);
    document = collection->base.read(&collection->base, did);

    // either no or one document
    if (document == NULL) {
      result->_length = 0;
    }
    else {
      ++result->base._matchedDocuments;
      
      result->_documents[0] = *document;
      result->_length = 1;
    }
  }

  result->_current = result->_documents;
  result->_end = result->_documents + result->_length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief list of filtered documents
////////////////////////////////////////////////////////////////////////////////

static void FilterDataGeneralQuery (collection_cursor_t* result,
                                    TRI_sim_collection_t* collection,
                                    TRI_rc_context_t* context,
                                    TRI_qry_where_t* w,
                                    TRI_voc_size_t skip,
                                    TRI_voc_ssize_t limit) {
  TRI_qry_where_cond_t* where;
  TRI_doc_mptr_t* qtr;
  size_t total;
  void** end;
  void** ptr;

  assert(w != NULL);
  assert(result->_documents == NULL);
  assert(0 <= limit);

  where = (TRI_qry_where_cond_t*) w;

  assert(where->checkCondition != NULL);

  result->_documents = (qtr = TRI_Allocate(sizeof(TRI_doc_mptr_t) * (collection->_primaryIndex._nrUsed)));

  ptr = collection->_primaryIndex._table;
  end = collection->_primaryIndex._table + collection->_primaryIndex._nrAlloc;

  for (total = 0;  ptr < end && total < limit;  ++ptr) {
    ++result->base._scannedDocuments;

    if (*ptr) {
      TRI_doc_mptr_t* mptr = *ptr;

      if (mptr->_deletion == 0) {
        if (where->checkCondition(where, context, mptr)) {
          ++result->base._matchedDocuments;

          if (0 < skip) {
            --skip;
            continue;
          }

          *qtr++ = *mptr;
          total++;
        }
      }
    }
  }

  result->_length = total;
  result->_current = result->_documents;
  result->_end = result->_documents + total;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lists document from within geo index
////////////////////////////////////////////////////////////////////////////////

static void FilterDataWithinQuery (collection_cursor_t* result,
                                   TRI_sim_collection_t* collection,
                                   TRI_rc_context_t* context,
                                   TRI_qry_where_t* w,
                                   TRI_voc_size_t skip,
                                   TRI_voc_ssize_t limit) {
  GeoCoordinates* cors;
  TRI_index_t* idx;
  TRI_qry_where_within_t* where;
  double* ll;
  double radius;

  assert(w != NULL);
  assert(w->_type == TRI_QRY_WHERE_WITHIN_CONSTANT);
  assert(result->_documents == NULL);
  assert(0 <= limit);

  where = (TRI_qry_where_within_t*) w;

  // locate the index
  idx = TRI_IndexSimCollection(collection, where->_iid);

  if (idx == NULL) {
    return;
  }

  // locate all documents within a given radius
  ll = where->coordinates(where, context);
  radius = where->radius(where, context);
  cors = TRI_WithinGeoIndex(idx, ll[0], ll[1], radius);

  // and create result
  StoreGeoResult(result, cors, where->_nameDistance);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                          ORDER BY
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function used for order by comparisons (called by fruitsort)
////////////////////////////////////////////////////////////////////////////////

static int OrderDataCompareFunc(const TRI_doc_mptr_t *left, const TRI_doc_mptr_t *right, size_t size, void *data) {
  TRI_rc_context_t *context = (TRI_rc_context_t *) data;
  bool ok;
  int result;

  TRI_DefineCompareExecutionContext(context->_orderClause, context->_primary, left, right);
  ok = TRI_ExecuteOrderExecutionContext(context->_orderClause, &result);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fruitsort initialization parameters
///
/// Included fsrt.inc with these parameters will create a function OrderData()
/// that is used to do the sorting. OrderData() will call OrderDataCompareFunc()
/// to do the actual element comparisons
////////////////////////////////////////////////////////////////////////////////

#define FSRT_INSTANCE Query
#define FSRT_NAME OrderData
#define FSRT_TYPE TRI_doc_mptr_t
#define FSRT_EXTRA_ARGS context
#define FSRT_EXTRA_DECL TRI_rc_context_t *context
#define FSRT_COMP(l,r,s) OrderDataCompareFunc(l,r,s)

uint32_t QueryFSRT_Rand = 0;
static uint32_t QuerySortRandomGenerator (void) {
  return (QueryFSRT_Rand = QueryFSRT_Rand * 31415 + 27818);
}
#define FSRT__RAND \
  ((fs_b) + FSRT__UNIT * (QuerySortRandomGenerator() % FSRT__DIST(fs_e,fs_b,FSRT__SIZE)))

#include <BasicsC/fsrt.inc>


////////////////////////////////////////////////////////////////////////////////
/// @brief executes sorting
////////////////////////////////////////////////////////////////////////////////

static void OrderDataGeneralQuery (collection_cursor_t *result,
                                   TRI_sim_collection_t* collection,
                                   TRI_rc_context_t* context,
                                   TRI_qry_order_t* o) {
  TRI_js_exec_context_t* execContext;

  assert(result != NULL);
  assert(o != NULL);
  assert(result->_documents != NULL);

  if (result->_length == 0) {
    // result set is empty, no need to sort it
    return;
  }

  execContext = context->_orderClause;

  if (execContext == NULL) {
    LOG_ERROR("no JavaScript context for order is known");
    return;
  }
  
  OrderData(result->_current, result->_end, context);
}


// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clones an order by clause for general JavaScript conditions
////////////////////////////////////////////////////////////////////////////////

static TRI_qry_order_t* CloneQueryOrderGeneral (TRI_qry_order_t const* o) {
  TRI_qry_order_general_t* orderClause;

  orderClause = (TRI_qry_order_general_t*) o;

  return TRI_CreateQueryOrderGeneral(orderClause->_code);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees an order by clause for general JavaScript conditions
////////////////////////////////////////////////////////////////////////////////

static void FreeQueryOrderGeneral (TRI_qry_order_t* o) {
  TRI_qry_order_general_t* orderClause;

  orderClause = (TRI_qry_order_general_t*) o;

  TRI_FreeString(orderClause->_code);
  TRI_Free(orderClause);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an order by clause for general JavaScript code
////////////////////////////////////////////////////////////////////////////////

TRI_qry_order_t* TRI_CreateQueryOrderGeneral (char const* clause) {
  TRI_qry_order_general_t* result;

  result = TRI_Allocate(sizeof(TRI_qry_order_general_t));

  result->base.base._type = TRI_QRY_ORDER_GENERAL;

  result->base.base.clone = CloneQueryOrderGeneral;
  result->base.base.free = FreeQueryOrderGeneral;

  result->_code = TRI_DuplicateString(clause);

  return &result->base.base;
}


// -----------------------------------------------------------------------------
// --SECTION--                                                             QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateQuery (TRI_qry_select_t const* selectStmt,
                              TRI_qry_where_t const* whereStmt,
                              TRI_qry_order_t const* orderStmt,
                              char const* name,
                              TRI_doc_collection_t* collection) {
  TRI_query_t* query;

  query = TRI_Allocate(sizeof(TRI_query_t));

  query->_select = selectStmt->clone(selectStmt);
  query->_primaryName = TRI_DuplicateString(name);
  query->_primary = collection;
  query->_where = whereStmt == NULL ? NULL : whereStmt->clone(whereStmt);
  query->_order = orderStmt == NULL ? NULL : orderStmt->clone(orderStmt);
  query->_skip = TRI_QRY_NO_SKIP;
  query->_limit = TRI_QRY_NO_LIMIT;

  TRI_InitVectorPointer(&query->_joins);

  return query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a query
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQuery (TRI_query_t* query) {
  query->_select->free(query->_select);

  if (query->_where != NULL) {
    query->_where->free(query->_where);
  }
  if (query->_order != NULL) {
    query->_order->free(query->_order);
  }

  TRI_FreeString(query->_primaryName);
  TRI_Free(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a context
////////////////////////////////////////////////////////////////////////////////

TRI_rc_context_t* TRI_CreateContextQuery (TRI_query_t* query) {
  TRI_rc_context_t* context;
  TRI_qry_select_general_t* selectGeneral;
  TRI_qry_where_general_t* whereGeneral;
  TRI_qry_order_general_t* orderGeneral;

  context = TRI_Allocate(sizeof(TRI_rc_context_t));

  context->_primary = query->_primary;
  context->_primaryName = TRI_DuplicateString(query->_primaryName);

  // check if we need a JavaScript execution context
  if (query->_select->_type == TRI_QRY_SELECT_GENERAL) {
    selectGeneral = (TRI_qry_select_general_t*) query->_select;

    context->_selectClause = TRI_CreateExecutionContext(selectGeneral->_code);

    if (context->_selectClause == NULL) {
      TRI_FreeContextQuery(context);
      return NULL;
    }
  }

  if (query->_where != NULL && query->_where->_type == TRI_QRY_WHERE_GENERAL) {
    whereGeneral = (TRI_qry_where_general_t*) query->_where;

    context->_whereClause = TRI_CreateExecutionContext(whereGeneral->_code);

    if (context->_whereClause == NULL) {
      TRI_FreeContextQuery(context);
      return NULL;
    }
  }
    
  if (query->_order != NULL && query->_order->_type == TRI_QRY_ORDER_GENERAL) {
    orderGeneral = (TRI_qry_order_general_t*) query->_order;

    context->_orderClause = TRI_CreateExecutionContext(orderGeneral->_code);

    if (context->_orderClause == NULL) {
      TRI_FreeContextQuery(context);
      return NULL;
    }
  }

  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a context
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeContextQuery (TRI_rc_context_t* context) {
  if (context->_selectClause != NULL) {
    TRI_FreeExecutionContext(context->_selectClause);
  }

  if (context->_whereClause != NULL) {
    TRI_FreeExecutionContext(context->_whereClause);
  }
  
  if (context->_orderClause != NULL) {
    TRI_FreeExecutionContext(context->_orderClause);
  }
  
  TRI_FreeString(context->_primaryName);
  TRI_Free(context);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks all collections of a query
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadLockCollectionsQuery (TRI_query_t* query) {
  query->_primary->beginRead(query->_primary);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks all collections of a query
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockCollectionsQuery (TRI_query_t* query) {
  query->_primary->endRead(query->_primary);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a gc marker for all collections
////////////////////////////////////////////////////////////////////////////////

void TRI_AddCollectionsCursor (TRI_rc_cursor_t* cursor, TRI_query_t* query) {
  TRI_rs_container_element_t* ce;

  ce = TRI_AddResultSetRSContainer(&query->_primary->_resultSets);
  TRI_PushBackVectorPointer(&cursor->_containers, ce);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks all collections of a query
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveCollectionsCursor (TRI_rc_cursor_t* cursor) {
  TRI_rs_container_element_t* ce;
  size_t i;

  for (i = 0;  i < cursor->_containers._length;  ++i) {
    ce = cursor->_containers._buffer[i];

    TRI_RemoveRSContainer(ce);
  }

  cursor->_containers._length = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query
////////////////////////////////////////////////////////////////////////////////

TRI_rc_cursor_t* TRI_ExecuteQueryAql (TRI_query_t* query, TRI_rc_context_t* context) {
  TRI_qry_where_t* where;
  TRI_voc_size_t skip;
  TRI_voc_ssize_t limit;
  collection_cursor_t* cursor;
  cond_fptr condition;
  order_fptr order;
  bool applyPostSkipLimit;

  skip = query->_skip;
  limit = query->_limit;
  applyPostSkipLimit = false;

  if (limit < 0) {
    limit = TRI_QRY_NO_LIMIT;
    skip = 0;
    applyPostSkipLimit = true;
  }

  if (query->_order) {
    // query has an order by. we must postpone limit to after sorting
    limit = TRI_QRY_NO_LIMIT;
    skip = 0;
    applyPostSkipLimit = true;
  }

  // .............................................................................
  // case: no WHERE clause, no JOIN
  // .............................................................................

  if (query->_where == NULL) {
    condition = FilterDataCollectionQuery;
    where = NULL;
  }

  // .............................................................................
  // case: WHERE clause, no JOIN
  // .............................................................................

  else {
    where = query->_where;

    if (where->_type == TRI_QRY_WHERE_GENERAL) {
      condition = FilterDataGeneralQuery;
    }
    else if (where->_type == TRI_QRY_WHERE_BOOLEAN) {
      TRI_qry_where_boolean_t* b;

      b = (TRI_qry_where_boolean_t*) where;

      if (b->_value) {
        condition = FilterDataCollectionQuery;
      }
      else {
        condition = FilterDataNoneQuery;

        limit = 0;
        skip = 0;
        applyPostSkipLimit = false;
      }

      where = NULL;
    }
    else if (where->_type == TRI_QRY_WHERE_PRIMARY_CONSTANT) {
      condition = FilterDataPrimaryQuery;
    }
    else if (where->_type == TRI_QRY_WHERE_WITHIN_CONSTANT) {
      condition = FilterDataWithinQuery;

      limit = TRI_QRY_NO_LIMIT;
      skip = 0;
      applyPostSkipLimit = true;
    }
    else {
      where = NULL;
      condition = FilterDataNoneQuery;
      assert(false);
    }
  }

  if (query->_order == NULL) {
    order = NULL;
  } 
  else {
    order = OrderDataGeneralQuery;
  }

  // .............................................................................
  // construct a collection subset
  // .............................................................................

  cursor = TRI_Allocate(sizeof(collection_cursor_t));
  cursor->base._context = context;
  cursor->base._select = query->_select->clone(query->_select);

  cursor->base.next = NextCollectionCursor;
  cursor->base.hasNext = HasNextCollectionCursor;
  cursor->base.free = FreeCollectionCursor;

  cursor->_result._context = context;
  cursor->_result._augmention._type = TRI_JSON_UNUSED;

  TRI_InitVectorPointer(&cursor->base._containers);

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  TRI_ReadLockCollectionsQuery(query);

  TRI_AddCollectionsCursor(&cursor->base, query);


  if (query->_primary->base._type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    condition(cursor, (TRI_sim_collection_t*) query->_primary, context, where, skip, limit);
  }
  else {
    cursor->base.free(&cursor->base);

    LOG_ERROR("unknown collection type '%lu'", (unsigned long) query->_primary->base._type);
    return 0;
  }

  // order by
  if (query->_order != NULL) {
    order(cursor, (TRI_sim_collection_t*) query->_primary, context, query->_order);
  }


  TRI_ReadUnlockCollectionsQuery(query);

  // .............................................................................
  // outside a read transaction
  // .............................................................................

  // apply a negative limit

  if (applyPostSkipLimit) {
    TransformDataSkipLimit(cursor, query->_skip, query->_limit);
  }

  return &cursor->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
