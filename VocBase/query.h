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

#ifndef TRIAGENS_DURHAM_VOC_BASE_QUERY_H
#define TRIAGENS_DURHAM_VOC_BASE_QUERY_H 1

#include "VocBase/vocbase.h"

#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"
#include "VocBase/index.h"
#include "VocBase/query-join.h"
#include "VocBase/query-where.h"
#include "VocBase/query-order.h"
#include "VocBase/query-cursor.h"
#include "VocBase/query-base.h"
#include "SkipLists/sl-operator.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @page BuildExecuteQuery Building and Executing a Query
///
/// @section LifeCycleQuery Life-Cycle of a Query
///
/// The query life-cycle starts with a string describing the query in the
/// Avocado query language. For example:
///
/// @verbinclude query1
/// 
/// In order to get the result the following steps are taken.
///
/// - the query is parsed
/// - an @CODE{TRI_query_t} representation is constructed
///
/// Later the query is executed:
///
/// - an @CODE{TRI_rc_context_t} is created
/// - the query is executed in this context
/// - a @CODE{TRI_rc_cursor_t} is returned
///
/// Later the result are accessed:
///
/// - the @CODE{next} method returns a @CODE{TRI_rc_result_t}
/// - which can be convert into a JSON or JavaScript object using
///   the @CODE{TRI_qry_select_t}
///
/// @section QueryRepresentation Query Representation
///
/// @copydetails TRI_query_t
///
/// @subsection QueryRepresentationSelect Query Select Representation
///
/// @copydetails TRI_qry_select_t
///
/// @section QueryExecution Query Execution
///
/// @subsection QueryContext Query Execution Context
///
/// @copydetails TRI_rc_context_t
///
/// @subsection ResultCursor Result Cursor
///
/// @copydetails TRI_rc_cursor_t
///
/// @subsection QueryResult Building the Query Result
///
/// @copydetails TRI_rc_result_t
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief select clause type - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_QRY_SELECT_DOCUMENT,
  TRI_QRY_SELECT_GENERAL
}
TRI_qry_select_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract select clause - DEPRECATED
///
/// A description of the @CODE{select} statement of the Avocado query language.
/// This class is respsonible for taking an element of type
/// @CODE{TRI_rc_result_t} as returned by the cursor and transforming it into
/// either a shaped json object of type @CODE{TRI_shaped_json_t} or a JavaScript
/// object of type @CODE{v8::Value}. The returned objects are either freed when
/// the @CODE{TRI_rc_result_t} is deleted or are garbage collected in case of
/// JavaScript.
///
/// The following implementations exist.
///
/// @copydetails TRI_qry_select_direct_t
///
/// @copydetails TRI_qry_select_general_t
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_select_s {
  TRI_qry_select_type_e _type;

  struct TRI_qry_select_s* (*clone) (struct TRI_qry_select_s const*);
  void (*free) (struct TRI_qry_select_s*);

  TRI_shaped_json_t* (*shapedJson) (struct TRI_qry_select_s*, TRI_rc_result_t*);
  bool (*toJavaScript) (struct TRI_qry_select_s*, TRI_rc_result_t*, void*);
}
TRI_qry_select_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief built-in select clause for a single, unaltered document - DEPRECATED
///
/// If a query returns a document unaltered, then @CODE{TRI_qry_select_direct_t}
/// can be used. It extracts a given document from the result set and returns
/// this document without any modification. The member @CODE{_variable} contains
/// the variable identifier of the result document of the next result to use.
///
/// For example
///
/// @verbinclude query2
///
/// will generate a query whose select part consists of an instance of
/// @CODE{TRI_qry_select_direct_t}. 
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_select_direct_s {
  TRI_qry_select_t base;
}
TRI_qry_select_direct_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript select clause - DEPRECATED
///
/// If a query returns a newly created document, then
/// @CODE{TRI_qry_select_general_t} can be used. It uses the V8 engine to
/// compute the document.  The member @CODE{_code} contains the JavaScript code
/// used to generate the document.  When this code is executed, then the result
/// documents of the next result are passed into the JavaScript code bound to
/// variables.
///
/// For example
///
/// @verbinclude query3
///
/// will generate new documents containing only @LIT{name} and @LIT{email}.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_select_general_s {
  TRI_qry_select_t base;

  char* _code;
}
TRI_qry_select_general_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief general query - DEPRECATED
///
/// A query is built using the fluent-interface or the Avocado Query
/// Language. It is of type @CODE{TRI_query_t}. After the query is built, it can
/// be executed. An execution returns a result cursor of type
/// @CODE{TRI_rc_cursor_t}. An execution requires a context of type
/// @CODE{TRI_rc_context_t}. All result are only vaild for the life-time of this
/// context.
///
/// It is possible to execute the same query multiple times - even in
/// parallel. However, each execution run requires it's own context.
///
/// A query contains the following parts:
///
/// - a description of the result in @CODE{_select}
/// - the primary collection for the query in @CODE{_primary}
/// - a list of joins in @CODE{_joins}
/// - a description of the where clause in @CODE{_where}
/// - an optional order by clause in @CODE{_order}
/// - a number limiting the number of returned documents in @CODE{_limit}
/// - a number of documents to skip in @CODE{_skip}
///
/// The @CODE{_select} is of type @CODE{TRI_qry_select_t}. This is in general a
/// representation of the part following the @CODE{select} statement.  There are
/// at least two implementions. The first one uses a built-in evaluator to
/// generate the result document from the next set of selected documents. The
/// second one uses the V8 engine to generate the result.
///
/// The @CODE{_primary} is of type @CODE{TRI_doc_collection_t}. This is the
/// collection following the @CODE{from} statement. The Avocado query language
/// only supports one collection after the @CODE{from}, this is the primary
/// collection of the query. All other collections must be joined with this
/// primary collection.
///
/// The @CODE{_joins} attributes contains a vector of joined collections
/// together with the join condition. The vector elements are of type
/// @CODE{TRI_qry_join_t}.
///
/// The @CODE{_where} attributes contains a description of the where
/// clause. There are at least two implementations. The first one uses a
/// built-in evaluator to evaluate the predicate. The second one uses the V8
/// engine.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_s {
  TRI_vocbase_t* _vocbase;
  TRI_qry_select_t* _select;
  TRI_doc_collection_t* _primary;
  TRI_qry_where_t* _where;
  TRI_select_join_t* _joins;

  TRI_voc_size_t _skip;
  TRI_voc_ssize_t _limit;
}
TRI_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief the result cursor - DEPRECATED
///
/// The result of a query execution. The metod @FN{next} returns the next
/// result documents. There is one document for the primary collection and a
/// list of documents for a list join. The next result documents are delivered
/// as instance of @CODE{TRI_rc_result_t}. You must call the @FN{free} method
/// for these instances.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_rc_cursor_s {
    TRI_rc_context_t* _context;
    TRI_qry_select_t* _select;
    TRI_select_result_t* _selectResult;
    TRI_js_exec_context_t* _selectContext;

    TRI_vector_pointer_t _containers;

    TRI_voc_size_t _scannedIndexEntries;
    TRI_voc_size_t _scannedDocuments;
    TRI_voc_size_t _matchedDocuments;

    void (*free) (struct TRI_rc_cursor_s*);
    TRI_rc_result_t* (*next)(struct TRI_rc_cursor_s*);
    bool (*hasNext)(struct TRI_rc_cursor_s*);
}
TRI_rc_cursor_t;

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
/// @brief creates a query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateHashQuery (const TRI_qry_where_t*, TRI_doc_collection_t*);

TRI_query_t* TRI_CreatePriorityQueueQuery (const TRI_qry_where_t*, TRI_doc_collection_t*);

TRI_query_t* TRI_CreateSkiplistQuery (const TRI_qry_where_t*, TRI_doc_collection_t*);

TRI_query_t* TRI_CreateQuery (TRI_vocbase_t*,
                              TRI_qry_select_t*,
                              TRI_qry_where_t*,
                              TRI_select_join_t*,
                              TRI_voc_size_t,
                              TRI_voc_ssize_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQuery (TRI_query_t* query);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a context - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_rc_context_t* TRI_CreateContextQuery (TRI_query_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a context - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeContextQuery (TRI_rc_context_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query selection for unaltered documents - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_select_t* TRI_CreateQuerySelectDocument (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query selection for general, generated documents - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_select_t* TRI_CreateQuerySelectGeneral (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition for constant conditions - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereBoolean (bool where);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition for general, JavaScript conditions - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereGeneral (char const*);


////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition for a hash with constant parameters - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereHashConstant (TRI_idx_iid_t, TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition for a priority queue with constant parameters - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWherePQConstant (TRI_idx_iid_t, TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition for a hash with constant parameters - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereSkiplistConstant (TRI_idx_iid_t, TRI_sl_operator_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition using the primary index and a constant - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWherePrimaryConstant (TRI_voc_did_t did);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition using an geo index and a constants - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereWithinConstant (TRI_idx_iid_t iid,
                                                     char const* nameDistance,
                                                     double latitiude,
                                                     double longitude,
                                                     double radius);

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
/// @brief read locks all collections of a query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadLockCollectionsQuery (TRI_query_t* query);

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks all collections of a query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockCollectionsQuery (TRI_query_t* query);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a gc marker for all collections
////////////////////////////////////////////////////////////////////////////////

void TRI_AddCollectionsCursor (TRI_rc_cursor_t* cursor, TRI_query_t* query);

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks all collections of a query
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveCollectionsCursor (TRI_rc_cursor_t* cursor);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_rc_cursor_t* TRI_ExecuteQueryAql (TRI_query_t*, TRI_rc_context_t*);

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
