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

static TRI_qry_select_t* CloneQuerySelectDocument (TRI_qry_select_t* s) {
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

static TRI_qry_select_t* CloneQuerySelectGeneral (TRI_qry_select_t* s) {
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

  TRI_DefineDocumentExecutionContext(execContext, context->_primaryName, context->_primary, document->_primary);

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
/// @brief list of all documents
////////////////////////////////////////////////////////////////////////////////

typedef struct collection_cursor_s {
  TRI_rc_cursor_t base;

  TRI_doc_mptr_t* _documents;
  TRI_doc_mptr_t* _current;
  TRI_doc_mptr_t* _end;

  TRI_voc_size_t _length;

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

static TRI_rc_result_t* NextCollectionQuery (TRI_rc_cursor_t* c) {
  collection_cursor_t* cursor;

  cursor = (collection_cursor_t*) c;

  if (cursor->_current < cursor->_end) {
    cursor->_result._primary = cursor->_current;

    ++cursor->_current;

    return &cursor->_result;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
////////////////////////////////////////////////////////////////////////////////

static bool HasNextCollectionQuery (TRI_rc_cursor_t* c) {
  collection_cursor_t* cursor;

  cursor = (collection_cursor_t*) c;

  return cursor->_current < cursor->_end;
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

static TRI_qry_where_t* CloneQueryWhereBoolean (TRI_qry_where_t* w) {
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

  result->base._type = TRI_QRY_WHERE_BOOLEAN;

  result->base.clone = CloneQueryWhereBoolean;
  result->base.free = FreeQueryWhereBoolean;
  result->base.checkCondition = NULL;

  result->_value = where;

  return &result->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               WHERE PRIMARY INDEX
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

static TRI_qry_where_t* CloneQueryWherePrimaryConstant (TRI_qry_where_t* w) {
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
  result->base.base.checkCondition = NULL;

  result->base.did = DidQueryWherePrimaryConstant;

  result->_did = did;

  return &result->base.base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   WHERE GEO INDEX
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

static TRI_qry_where_t* CloneQueryWhereGeoConstant (TRI_qry_where_t* w) {
  TRI_qry_where_geo_const_t* whereClause;

  whereClause = (TRI_qry_where_geo_const_t*) w;

  return TRI_CreateQueryWhereGeoConstant(whereClause->_iid,
                                         whereClause->_coordinates[0],
                                         whereClause->_coordinates[1]);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a query condition using the geo index and constants
////////////////////////////////////////////////////////////////////////////////

static void FreeQueryWhereGeoConstant (TRI_qry_where_t* w) {
  TRI_qry_where_geo_const_t* whereClause;

  whereClause = (TRI_qry_where_geo_const_t*) w;

  TRI_Free(whereClause);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the lattitude and longitude for constants
////////////////////////////////////////////////////////////////////////////////

static double* CoordinatesQueryWhereGeoConstant (TRI_qry_where_geo_t* w,
                                                 TRI_rc_context_t* context) {
  TRI_qry_where_geo_const_t* whereClause;

  whereClause = (TRI_qry_where_geo_const_t*) w;

  return whereClause->_coordinates;
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

TRI_qry_where_t* TRI_CreateQueryWhereGeoConstant (TRI_idx_iid_t iid,
                                                  double latitiude,
                                                  double longitude) {
  TRI_qry_where_geo_const_t* result;

  result = TRI_Allocate(sizeof(TRI_qry_where_geo_const_t));

  result->base.base._type = TRI_QRY_WHERE_GEO_CONSTANT;

  result->base.base.clone = CloneQueryWhereGeoConstant;
  result->base.base.free = FreeQueryWhereGeoConstant;
  result->base.base.checkCondition = NULL;

  result->base.coordinates = CoordinatesQueryWhereGeoConstant;

  result->_coordinates[0] = latitiude;
  result->_coordinates[1] = longitude;

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

static TRI_qry_where_t* CloneQueryWhereGeneral (TRI_qry_where_t* w) {
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

static bool CheckConditionWhereGeneral (TRI_qry_where_t* w,
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

  result->base._type = TRI_QRY_WHERE_GENERAL;

  result->base.clone = CloneQueryWhereGeneral;
  result->base.free = FreeQueryWhereGeneral;
  result->base.checkCondition = CheckConditionWhereGeneral;

  result->_code = TRI_DuplicateString(clause);

  return &result->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      WHERE FILTER
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

  result->_documents = (qtr = TRI_Allocate(sizeof(TRI_doc_mptr_t) * (collection->_primaryIndex._nrUsed)));
  result->_current = result->_documents;

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

  result->_documents = TRI_Allocate(sizeof(TRI_doc_mptr_t));
  result->_current = result->_documents;

  result->_length = 0;
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

  where = (TRI_qry_where_primary_const_t*) w;

  result->_documents = TRI_Allocate(sizeof(TRI_doc_mptr_t));
  result->_current = result->_documents;

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

  result->_end = result->_documents + result->_length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief list of filtered documents
////////////////////////////////////////////////////////////////////////////////

static void FilterDataGeneralQuery (collection_cursor_t* result,
                                    TRI_sim_collection_t* collection,
                                    TRI_rc_context_t* context,
                                    TRI_qry_where_t* where,
                                    TRI_voc_size_t skip,
                                    TRI_voc_ssize_t limit) {
  TRI_doc_mptr_t* qtr;
  size_t total;
  void** end;
  void** ptr;

  assert(where != NULL);
  assert(where->checkCondition != NULL);

  result->_documents = (qtr = TRI_Allocate(sizeof(TRI_doc_mptr_t) * (collection->_primaryIndex._nrUsed)));
  result->_current = result->_documents;

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
  result->_end = result->_documents + total;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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

TRI_query_t* TRI_CreateQuery (TRI_qry_select_t* selectStmt,
                              char const* name,
                              TRI_doc_collection_t* collection) {
  TRI_query_t* query;

  query = TRI_Allocate(sizeof(TRI_query_t));

  query->_select = selectStmt;
  query->_primaryName = TRI_DuplicateString(name);
  query->_primary = collection;
  query->_skip = TRI_QRY_NO_SKIP;
  query->_limit = TRI_QRY_NO_LIMIT;

  TRI_InitVectorPointer(&query->_joins);

  return query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a context
////////////////////////////////////////////////////////////////////////////////

TRI_rc_context_t* TRI_CreateContextQuery (TRI_query_t* query) {
  TRI_rc_context_t* context;
  TRI_qry_select_general_t* selectGeneral;
  TRI_qry_where_general_t* whereGeneral;

  context = TRI_Allocate(sizeof(TRI_rc_context_t));

  context->_primary = query->_primary;
  context->_primaryName = TRI_DuplicateString(query->_primaryName);

  // check if we need a JavaScript execution context
  if (query->_select->_type == TRI_QRY_SELECT_GENERAL) {
    selectGeneral = (TRI_qry_select_general_t*) query->_select;

    context->_selectClause = TRI_CreateExecutionContext(selectGeneral->_code);

    if (context->_selectClause == NULL) {
      // TODO free
      return NULL;
    }
  }

  if (query->_where != NULL && query->_where->_type == TRI_QRY_WHERE_GENERAL) {
    whereGeneral = (TRI_qry_where_general_t*) query->_where;

    context->_whereClause = TRI_CreateExecutionContext(whereGeneral->_code);

    if (context->_whereClause == NULL) {
      // TODO free
      return NULL;
    }
  }

  return context;
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
/// @brief executes a query
////////////////////////////////////////////////////////////////////////////////

TRI_rc_cursor_t* TRI_ExecuteQueryAql (TRI_query_t* query, TRI_rc_context_t* context) {
  collection_cursor_t* cursor;
  cond_fptr condition;
  TRI_qry_where_t* where;
  TRI_voc_size_t skip;
  TRI_voc_ssize_t limit;

  skip = query->_skip;
  limit = query->_limit;

  if (limit < 0) {
    limit = TRI_QRY_NO_LIMIT;
    skip = 0;
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
      }

      where = NULL;
    }
    else if (where->_type == TRI_QRY_WHERE_PRIMARY_CONSTANT) {
      condition = FilterDataPrimaryQuery;
    }
    else {
      where = NULL;
      condition = FilterDataNoneQuery;
      assert(false);
    }
  }

  // .............................................................................
  // construct a collection subset
  // .............................................................................

  cursor = TRI_Allocate(sizeof(collection_cursor_t));
  cursor->base._context = context;
  cursor->base._select = query->_select->clone(query->_select);

  cursor->base.next = NextCollectionQuery;
  cursor->base.hasNext = HasNextCollectionQuery;

  cursor->_result._context = context;

  if (query->_primary->base._type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    condition(cursor, (TRI_sim_collection_t*) query->_primary, context, where, skip, limit);
  }
  else {
    cursor->base._select->free(cursor->base._select);
    TRI_Free(cursor);

    LOG_ERROR("unknown collection type '%lu'", (unsigned long) query->_primary->base._type);
    return 0;
  }

  // .............................................................................
  // apply a negative limit
  // .............................................................................

  if (query->_limit < 0) {
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
