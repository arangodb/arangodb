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

#include <BasicsC/logging.h>
#include "BasicsC/string-buffer.h"
#include <BasicsC/strings.h>
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
/// @brief converts result to JavaScript
////////////////////////////////////////////////////////////////////////////////

static bool ToJavaScriptSelectDocument (TRI_qry_select_t* s,
                                        TRI_rc_result_t* result,
                                        void* storage) {

  TRI_doc_mptr_t masterPointer;
  TRI_doc_mptr_t* document;
  TRI_select_size_t* numPtr;
  TRI_select_size_t num;
  TRI_select_result_t* selectResult = result->_selectResult;
  TRI_select_datapart_t* part = (TRI_select_datapart_t*) selectResult->_dataParts->_buffer[0];

  numPtr = (TRI_select_size_t*) result->_dataPtr;
  num = *numPtr++;
  assert(num == 1);
  document = *((TRI_sr_documents_t*) numPtr);
  TRI_MarkerMasterPointer(document, &masterPointer);

  return TRI_ObjectDocumentPointer(part->_collection, &masterPointer, storage);
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
/// @brief converts result to JavaScript
////////////////////////////////////////////////////////////////////////////////

static bool ToJavaScriptSelectGeneral (TRI_qry_select_t* s,
                                       TRI_rc_result_t* result,
                                       void* storage) {
  TRI_rc_context_t* context;
  TRI_js_exec_context_t* execContext;

  context = result->_context;
  execContext = context->_selectClause;

  if (execContext == NULL) {
    LOG_ERROR("no JavaScript context for select is known");
    return false;
  }

  // first define the augumentation, they might get overwritten
  if (result->_augmention._type == TRI_JSON_ARRAY) {
    TRI_DefineJsonArrayExecutionContext(execContext, &result->_augmention);
  }

  // define the documents
  TRI_DefineSelectExecutionContext(execContext, result);

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

  TRI_select_size_t _length;
  TRI_select_size_t _currentRow;

  TRI_doc_mptr_t* _documents;
  TRI_doc_mptr_t* _current;
  TRI_doc_mptr_t* _end;

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


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next element
////////////////////////////////////////////////////////////////////////////////

static TRI_rc_result_t* NextCollectionCursor (TRI_rc_cursor_t* c) {
  collection_cursor_t* cursor;

  cursor = (collection_cursor_t*) c;
  if (cursor->_currentRow < cursor->_length) {
    cursor->_result._dataPtr = 
      cursor->_result._selectResult->getAt(cursor->_result._selectResult, cursor->_currentRow++);

    if (cursor->_augmentions != NULL) {
      cursor->_result._augmention = *cursor->_currentAugmention++;
    }

    return &cursor->_result;
  }

  return NULL;
}


static bool ToJavaScriptHashDocument (TRI_qry_select_t* s,
                                      TRI_rc_result_t* result,
                                      void* storage) {

  TRI_doc_mptr_t* document;
  TRI_doc_collection_t* collection;  

  collection = result->_context->_primary;  

  /* start oreste  
  TRI_doc_mptr_t* doc = (TRI_doc_mptr_t*) result->_dataPtr;
  TRI_shaped_json_t const* shaped;
  TRI_shaper_t* shaper;
  TRI_string_buffer_t buffer;
  TRI_InitStringBuffer(&buffer);
  simCollection = (TRI_sim_collection_t*)(collection);
  shaper = simCollection->base._shaper;
  shaped = &doc->_document;
  TRI_StringifyShapedJson (shaper, &buffer, shaped);
  printf("%s:%u:#######:%s\n",__FILE__, __LINE__,buffer._buffer);
  printf("%s:%u:%lu:%lu:%lu\n",__FILE__,__LINE__,
         (uint64_t)(doc),
         (uint64_t)(shaped),
         (uint64_t)(shaper));
         
  TRI_FreeStringBuffer(&buffer);
  end oreste */
  
  document = (TRI_doc_mptr_t*) result->_dataPtr;
  return TRI_ObjectDocumentPointer(collection, document, storage);
}

static TRI_rc_result_t* NextHashCollectionCursor (TRI_rc_cursor_t* c) {
  collection_cursor_t* cursor;
  TRI_doc_collection_t* collection;

  cursor = (collection_cursor_t*) c;
  collection = cursor->base._context->_primary;
 
  if (cursor->_currentRow == cursor->_length) {
    return NULL;
  }  

  cursor->_current = &(cursor->_documents[cursor->_currentRow]);
  cursor->_result._dataPtr = (TRI_sr_documents_t*) (cursor->_current);
  ++(cursor->_currentRow);
 
  return &cursor->_result;
  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
////////////////////////////////////////////////////////////////////////////////

static bool HasNextCollectionCursor (TRI_rc_cursor_t* c) {
  collection_cursor_t* cursor;

  cursor = (collection_cursor_t*) c;

  return cursor->_currentRow < cursor->_length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a cursor
////////////////////////////////////////////////////////////////////////////////

static void FreeCollectionCursor (TRI_rc_cursor_t* c) {
  collection_cursor_t* cursor;
  TRI_qry_select_t* slct;
  size_t i;

  cursor = (collection_cursor_t*) c;

  // free result set
  if (cursor->_result._selectResult != NULL) {
    cursor->_result._selectResult->free(cursor->_result._selectResult);
  }
  
  // free select
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
/// @brief applys a skip and limit on the result set
////////////////////////////////////////////////////////////////////////////////

static bool TransformDataSkipLimit (TRI_rc_result_t* result,
                                    TRI_voc_size_t skip,
                                    TRI_voc_ssize_t limit) {

  TRI_select_result_t* _result = result->_selectResult;
   
  // return nothing
  if (limit == 0 || _result->_numRows <= skip) {
    return TRI_SliceSelectResult(_result, 0, 0);
  }

  // positive limit, skip and slice from front
  if (limit > 0) {
    if (_result->_numRows - skip < limit) {
      return TRI_SliceSelectResult(_result, skip, _result->_numRows - skip);
    }

    return TRI_SliceSelectResult(_result, skip, limit);
  }

  // negative limit, slice from back
  if (_result->_numRows - skip < -limit) {
    return TRI_SliceSelectResult(_result, 0, _result->_numRows - skip);
  }

  return TRI_SliceSelectResult(_result, _result->_numRows - skip + limit, -limit);
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

  //result->base.checkCondition = NULL;

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

  //result->base.checkCondition = CheckConditionWhereGeneral;

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

static TRI_qry_where_t* CloneQueryWhereHashConstant (const TRI_qry_where_t* w) {
  TRI_qry_where_hash_const_t* whereClause;

  whereClause = (TRI_qry_where_hash_const_t*) w;

  return TRI_CreateQueryWhereHashConstant(whereClause->_iid, whereClause->_parameters);
}

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

static void FreeQueryWhereHashConstant (TRI_qry_where_t* w) {
  TRI_qry_where_hash_const_t* whereClause;
  whereClause = (TRI_qry_where_hash_const_t*) w;
  TRI_FreeJson(whereClause->_parameters);
  TRI_Free(whereClause);
}

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
/// @brief creates a query condition for hash index
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereHashConstant (TRI_idx_iid_t iid, TRI_json_t* parameters) {
  TRI_qry_where_hash_const_t* result;
  result = TRI_Allocate(sizeof(TRI_qry_where_hash_const_t));
  result->base._type  = TRI_QRY_WHERE_HASH_CONSTANT;
  result->base.clone  = CloneQueryWhereHashConstant;
  result->base.free   =  FreeQueryWhereHashConstant;
  result->_iid        = iid;
  result->_parameters = parameters;
  return &result->base;
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

static bool InitCollectionsQuery (TRI_query_t* query) {
  TRI_join_part_t* part;
  TRI_vocbase_col_t const* collection;
  size_t i;

  assert(query->_vocbase);

  for (i = 0 ; i < query->_joins->_parts._length; i++) {
    part = (TRI_join_part_t*) query->_joins->_parts._buffer[i];
    assert(part->_collection == NULL);
    assert(part->_alias);
    collection = TRI_LoadCollectionVocBase(query->_vocbase, part->_collectionName);
    if (!collection) {
      return false;
    }
    part->_collection = (TRI_doc_collection_t*) collection->_collection;
    if (part->_type == JOIN_TYPE_PRIMARY) {
      query->_primary = part->_collection;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a hash query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateHashQuery(const TRI_qry_where_t* whereStmt,
                                 TRI_doc_collection_t* collection) {
  TRI_query_t* query;

  query = TRI_Allocate(sizeof(TRI_query_t));
  if (!query) {
    return NULL;
  }
  
  query->_where       = ( whereStmt == NULL ? NULL : whereStmt->clone(whereStmt));
  query->_skip        = TRI_QRY_NO_SKIP;
  query->_limit       = TRI_QRY_NO_LIMIT;
  query->_primary     = collection;
  query->_joins       = NULL;
  query->_select      = TRI_CreateQuerySelectDocument();  
  return query;                               
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query
////////////////////////////////////////////////////////////////////////////////
                                 
TRI_query_t* TRI_CreateQuery (TRI_vocbase_t* vocbase,
                              TRI_qry_select_t* selectStmt,
                              TRI_qry_where_t* whereStmt,
                              TRI_qry_order_t* orderStmt,
                              TRI_select_join_t* joins) {
  TRI_query_t* query;

  query = TRI_Allocate(sizeof(TRI_query_t));
  if (!query) {
    return NULL;
  }

  query->_vocbase = vocbase;
  query->_select = selectStmt;
  query->_where = whereStmt;
  query->_order = orderStmt;
  query->_joins = joins;
  query->_skip = TRI_QRY_NO_SKIP;
  query->_limit = TRI_QRY_NO_LIMIT;

  if (!InitCollectionsQuery(query)) {
    TRI_FreeQuery(query);
    return NULL;
  }

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
  if (query->_joins != NULL) {
    query->_joins->free(query->_joins);
  }

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
  TRI_join_part_t* part;
  size_t i;

  if (query->_joins == NULL) {
    query->_primary->beginRead(query->_primary);
    return;
  }
  
  
  // note: the same collection might be read-locked multiple times here
  for (i = 0; i < query->_joins->_parts._length; i++) {
    part = (TRI_join_part_t*) query->_joins->_parts._buffer[i];
    assert(part->_collection);
    part->_collection->beginRead(part->_collection);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks all collections of a query
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockCollectionsQuery (TRI_query_t* query) {
  TRI_join_part_t* part;
  size_t i;

  if (query->_joins == NULL) {
    query->_primary->endRead(query->_primary);
    return;
  }
  
  // note: the same collection might be read-unlocked multiple times here
  i = query->_joins->_parts._length;
  while (i > 0) {
    i--;
    part = (TRI_join_part_t*) query->_joins->_parts._buffer[i];
    assert(part->_collection);
    part->_collection->endRead(part->_collection);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a gc marker for all collections
////////////////////////////////////////////////////////////////////////////////

void TRI_AddCollectionsCursor (TRI_rc_cursor_t* cursor, TRI_query_t* query) {
  TRI_join_part_t* part;
  TRI_rs_container_element_t* ce;
  size_t i;

  if (query->_joins == NULL) {
    ce = TRI_AddResultSetRSContainer(&query->_primary->_resultSets);
    TRI_PushBackVectorPointer(&cursor->_containers, ce);
    return;
  }
  
    
  // note: the same collection might be added multiple times here
  for (i = 0; i < query->_joins->_parts._length; i++) {
    part = (TRI_join_part_t*) query->_joins->_parts._buffer[i];
    assert(part->_collection);
    ce = TRI_AddResultSetRSContainer(&part->_collection->_resultSets);
    TRI_PushBackVectorPointer(&cursor->_containers, ce);
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief removes the gc markers for all collections of a query
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

static void FilterDataHashQuery(collection_cursor_t* cursor,TRI_query_t* query, 
                                TRI_rc_context_t* context) {
  
  bool ok;  
  TRI_index_t* idx;
  TRI_qry_where_hash_const_t* where;
  TRI_sim_collection_t* collection; 
  HashIndexElements* hashElements;
  TRI_doc_mptr_t* wtr; 
  TRI_doc_mptr_t* doc;
  
  cursor->base._context = context;
  cursor->base._select  = query->_select->clone(query->_select);
   
  cursor->base.next     = NextHashCollectionCursor;
  cursor->base.hasNext  = HasNextCollectionCursor;
  cursor->base.free     = FreeCollectionCursor;
  cursor->_result._selectResult = NULL;
  
  cursor->_result._context = context;
  cursor->_result._dataPtr = NULL;
   
  cursor->base._select->toJavaScript = ToJavaScriptHashDocument; 

  TRI_InitVectorPointer(&cursor->base._containers);
  
  TRI_ReadLockCollectionsQuery(query);

  TRI_AddCollectionsCursor(&cursor->base, query);

  ok = (query->_primary->base._type == TRI_COL_TYPE_SIMPLE_DOCUMENT);
  
  if (ok) {
    where = (TRI_qry_where_hash_const_t*)(query->_where);
    ok = (where->base._type == TRI_QRY_WHERE_HASH_CONSTANT);
  }
  
  if (ok) {
    collection = (TRI_sim_collection_t*) query->_primary;
    idx = TRI_IndexSimCollection(collection, where->_iid);
    ok = (idx != NULL);
  }  

  if (ok) {
    hashElements = TRI_LookupHashIndex(idx,where->_parameters);
    ok = (hashElements != NULL);
  }
  
  
  if (ok) {
    cursor->_documents = (TRI_doc_mptr_t*) (TRI_Allocate(sizeof(TRI_doc_mptr_t) * hashElements->_numElements));

    wtr                            = cursor->_documents;
    cursor->_length                = 0;
    cursor->base._matchedDocuments = 0;
    cursor->_current               = 0;
    cursor->_currentRow            = 0;
    
    for (size_t j = 0; j < hashElements->_numElements; ++j) {
      // should not be necessary to check that documents have not been deleted    
      doc = (TRI_doc_mptr_t*)((hashElements->_elements[j]).data);
      if (doc->_deletion) {
        continue;
      }
      if (cursor->_current == 0) {
        cursor->_current = wtr;
      }
      ++cursor->base._matchedDocuments;
      ++cursor->_length;
      *wtr = *doc;
      ++wtr;
    }
    
    
    if (hashElements->_elements != NULL) {
      TRI_Free(hashElements->_elements);
      TRI_Free(hashElements);
    }
  }
  
  TRI_ReadUnlockCollectionsQuery(query);
  
  if (!ok) {
    cursor->_length = 0; 
    cursor->_currentRow = 0;
  }
    
}



TRI_rc_cursor_t* TRI_ExecuteQueryAql (TRI_query_t* query, TRI_rc_context_t* context) {
  TRI_qry_where_t* where;
  TRI_voc_size_t skip;
  TRI_voc_ssize_t limit;
  TRI_select_result_t* selectResult;
  collection_cursor_t* cursor;
  TRI_order_fptr_t order;
  bool applyPostSkipLimit;
  bool emptyQuery;

  skip = query->_skip;
  limit = query->_limit;
  applyPostSkipLimit = false;

  if (limit < 0) {
    limit = TRI_QRY_NO_LIMIT;
    skip = 0;
    applyPostSkipLimit = true;
  }

  if (query->_order && (query->_skip > 0 || query->_limit != TRI_QRY_NO_LIMIT)) {
    // query has an order by. we must postpone limit to after sorting
    limit = TRI_QRY_NO_LIMIT;
    skip = 0;
    applyPostSkipLimit = true;
  }
 
  if (query->_order == NULL) {
    order = NULL;
  } 
  else {
    order = TRI_OrderDataGeneralQuery;
  }

  // set up where condition
  emptyQuery = false;
  where = 0;

  if (query->_where != NULL) {
    where = query->_where;
    if (where->_type == TRI_QRY_WHERE_BOOLEAN) {
      TRI_qry_where_boolean_t* b;
      b = (TRI_qry_where_boolean_t*) where;
      if (!b->_value) {
        emptyQuery = true;
      }
      where = NULL;
    }
    else if (where->_type == TRI_QRY_WHERE_PRIMARY_CONSTANT) {
      assert(false);
    }
    else if (where->_type == TRI_QRY_WHERE_WITHIN_CONSTANT) {
      assert(false);
    }
    else if (where->_type == TRI_QRY_WHERE_HASH_CONSTANT) {
      cursor = TRI_Allocate(sizeof(collection_cursor_t));
      if (cursor == NULL) {
        return NULL;
      }
      FilterDataHashQuery(cursor,query,context);
      return &cursor->base;
    }
  }

  // create a select result container for the joins
  selectResult = TRI_JoinSelectResult(query->_vocbase, query->_joins);
  if (!selectResult) {
    return NULL;
  }

  // .............................................................................
  // construct a collection subset
  // .............................................................................
  
  cursor = TRI_Allocate(sizeof(collection_cursor_t));
  if (!cursor) {
    selectResult->free(selectResult);
    return NULL;
  }
  cursor->base._context = context;
  cursor->base._select = query->_select->clone(query->_select);
  cursor->base.next = NextCollectionCursor;
  cursor->base.hasNext = HasNextCollectionCursor;
  cursor->base.free = FreeCollectionCursor;
  cursor->_result._selectResult = selectResult;
  cursor->_result._dataPtr = NULL;
  cursor->_result._context = context;
  cursor->_result._augmention._type = TRI_JSON_UNUSED;

  TRI_InitVectorPointer(&cursor->base._containers);

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  TRI_ReadLockCollectionsQuery(query);
  TRI_AddCollectionsCursor(&cursor->base, query);
 
  // Execute joins
  if (!emptyQuery) {
    TRI_ExecuteJoins(selectResult, query->_joins, where, context, skip, limit);
  }

  TRI_ReadUnlockCollectionsQuery(query);

  // .............................................................................
  // outside a read transaction
  // .............................................................................

  // order by
  if (order) {
    order(&cursor->_result, query->_order);
  }

  // apply a negative limit or a limit after ordering
  if (applyPostSkipLimit) {
    TransformDataSkipLimit(&cursor->_result, query->_skip, query->_limit);
  }
  
  // adjust cursor length
  cursor->_length = selectResult->_numRows; 
  cursor->_currentRow = 0;

  return &cursor->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

