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
#include "V8/v8-c-utils.h"
#include "VocBase/document-collection.h"
#include "VocBase/index.h"

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

////////////////////////////////////////////////////////////////////////////////
/// @page AQL Avocado Query Language
/// 
/// A query constists of the following parts:
///
/// - select clause
/// - primary collection
/// - joins
/// - where clause
/// - order by
/// - limit
///
/// @section AqlSelect Select Clause
///
/// To select all documents unlatered from a collection use
///
/// @verbinclude query4
///
/// You can also build a new document from the attributes of the result
///
/// @verbinclude query5
///
/// If the attribute @CODE{name} is unknown, null is used. You can apply
/// functions to build the result.
///
/// @verbinclude query6
///
/// The @FN{nvl} returns the second argument in case that the name is unknown.
/// 
/// @section AqlFrom Primary Collection
///
/// There is one primary collection, which drive the selection process.
///
/// @verbinclude query7
///
/// @section AqlJoin Joins
///
/// It is possible to join other collections.
///
/// @subsection AllListJoin List Joins
///
/// Joins documents from a second collection. All suitable documents are
/// returned as list
///
/// @verbinclude query8
///
/// If an attribute is a object reference, you can compare it the document
/// itself.
///
/// @verbinclude query9
///
/// @section AqlWhere Where Clause
///
/// @subsection AqlPredicate Comparison and Boolean Operators
///
/// - @LIT{==} equality
/// - @LIT{===} strict equality
/// - @LIT{!=} inquality
/// - @LIT{!==} strict inequality
/// - @LIT{<}, @LIT{<=}, @LIT{>=}, @LIT{>} less than, less than or equal,
///   greater than or equal, greater than
/// - @LIT{&&} boolean @LIT{and} operator
/// - @LIT{||} boolean @LIT{or} operator
/// - @LIT{!} boolean @LIT{not} operator
/// - @LIT{in} tests if a key exists
/// 
/// Assume that @LIT{doc} is a document, such that the attribute @LIT{d} is a
/// number, the attribute @LIT{n} is null, and the attribute @LIT{x} does not
/// exists.  Than the following holds:
///
/// <TABLE>
///   <TR><TH></TH><TH>== null</TH><TH>=== null</TH><TH>=== undefined</TH></TR>
///   <TR><TD>doc.d</TD><TD>false</TD><TD>false</TD><TD>false</TD></TR>
///   <TR><TD>doc.n</TD><TD>true</TD><TD>true</TD><TD>false</TD></TR>
///   <TR><TD>doc.x</TD><TD>true</TD><TD>false</TD><TD>true</TD></TR>
/// </TABLE>
/// 
/// @subsection AqlPredicateFunctions Functions
///
/// - @LIT{contains} tests if a value exists
/// - @LIT{distance}, see below
///
/// @subsection AqlParameter Parameter
///
/// - @LIT{\@0} - positional parameter, starting with 0
/// - @LIT{\@name\@} - named parameter
///
/// @subsection AqlGeoQuery Geo Queries
/// 
/// All documents with a given radius:
///
/// @verbinclude query10
///
/// The nearest documents
///
/// @verbinclude query11
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief execution context of a query
///
/// In order to execute a query, you need an execution context. The results are
/// only valid as long as the context exists. After freeing the context you
/// should no longer access the result cursor or the result documents.
///
/// When creating an execution context, parts of clauses might use JavaScript.
/// A JavaScript execution context is created for these clauses.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_rc_context_s {
  char* _primaryName;
  TRI_doc_collection_t* _primary;

  TRI_js_exec_context_t _selectClause;
  TRI_js_exec_context_t _whereClause;
  TRI_js_exec_context_t _orderClause;
}
TRI_rc_context_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief the result documents for the next result
///
/// The result documents of the next result. When a cursor returns the next set
/// of documents, it will return these documents as instance of this class.  The
/// methods @FN{shapedJson} and @FN{toJavaScript} of @CODE{TRI_qry_select_t}
/// must be used to convert these documents into the final json or JavaScript
/// result object. Note that a call to @FN{next} will free the current result.
/// You do not need to free the result returned by @FN{next}, it is destroyed
/// when the cursor itself is destroyed or when @FN{next} is called again.
///
/// The member @CODE{_primary} holds the primary document pointer for the
/// primary collection.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_rc_result_s {
  TRI_rc_context_t* _context;

  TRI_doc_mptr_t* _primary;
  TRI_json_t _augmention;
}
TRI_rc_result_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief select clause type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_QRY_SELECT_DOCUMENT,
  TRI_QRY_SELECT_GENERAL
}
TRI_qry_select_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract select clause
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
/// @brief built-in select clause for a single, unaltered document
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
/// @brief JavaScript select clause
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
/// @brief where clause type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_QRY_WHERE_BOOLEAN,
  TRI_QRY_WHERE_GENERAL,
  TRI_QRY_WHERE_WITHIN_CONSTANT,
  TRI_QRY_WHERE_PRIMARY_CONSTANT
}
TRI_qry_where_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract where clause
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_s {
  TRI_qry_where_type_e _type;

  struct TRI_qry_where_s* (*clone) (struct TRI_qry_where_s const*);
  void (*free) (struct TRI_qry_where_s*);
}
TRI_qry_where_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract where clause for conditions
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_cond_s {
  TRI_qry_where_t base;

  bool (*checkCondition) (struct TRI_qry_where_cond_s*, TRI_rc_context_t*, TRI_doc_mptr_t const*);
}
TRI_qry_where_cond_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief constant where clause
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_boolean_s {
  TRI_qry_where_cond_t base;

  bool _value;
}
TRI_qry_where_boolean_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript where clause
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_general_s {
  TRI_qry_where_cond_t base;

  char* _code;
}
TRI_qry_where_general_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract primary index where clause
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_primary_s {
  TRI_qry_where_t base;

  TRI_voc_did_t (*did) (struct TRI_qry_where_primary_s*, TRI_rc_context_t*);
}
TRI_qry_where_primary_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief primary index where clause with a constant document identifier
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_primary_const_s {
  TRI_qry_where_primary_t base;

  TRI_voc_did_t _did;
}
TRI_qry_where_primary_const_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index where clause
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_within_s {
  TRI_qry_where_t base;

  TRI_idx_iid_t _iid;
  char* _nameDistance;

  double* (*coordinates) (struct TRI_qry_where_within_s*, TRI_rc_context_t*);
  double (*radius) (struct TRI_qry_where_within_s*, TRI_rc_context_t*);
}
TRI_qry_where_within_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index where clause with a constant document identifier
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_where_within_const_s {
  TRI_qry_where_within_t base;

  double _coordinates[2];
  double _radius;
}
TRI_qry_where_within_const_t;


////////////////////////////////////////////////////////////////////////////////
/// @brief order by type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_QRY_ORDER_GENERAL
}
TRI_qry_order_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract order by clause
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_order_s {
  TRI_qry_order_type_e _type;

  struct TRI_qry_order_s* (*clone) (struct TRI_qry_order_s const*);
  void (*free) (struct TRI_qry_order_s*);
}
TRI_qry_order_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract order by clause for conditions
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_order_cond_s {
  TRI_qry_order_t base;

  bool (*checkCondition) (struct TRI_qry_order_cond_s*, TRI_rc_context_t*, TRI_doc_mptr_t const*);
}
TRI_qry_order_cond_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript order by clause
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_order_general_s {
  TRI_qry_order_cond_t base;

  char* _code;
}
TRI_qry_order_general_t;


////////////////////////////////////////////////////////////////////////////////
/// @brief general query
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
  TRI_qry_select_t* _select;
  char* _primaryName;
  TRI_doc_collection_t* _primary;
  TRI_vector_pointer_t _joins;
  TRI_qry_where_t* _where;
  TRI_qry_order_t* _order;

  TRI_voc_size_t _skip;
  TRI_voc_ssize_t _limit;
}
TRI_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief the result cursor
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
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief no limit
////////////////////////////////////////////////////////////////////////////////

#define TRI_QRY_NO_LIMIT ((TRI_voc_ssize_t) INT32_MAX)

////////////////////////////////////////////////////////////////////////////////
/// @brief no skip
////////////////////////////////////////////////////////////////////////////////

#define TRI_QRY_NO_SKIP ((TRI_voc_size_t) 0)

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
/// @brief creates a query
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateQuery (TRI_qry_select_t const* selectStmt,
                              TRI_qry_where_t  const* whereStmt,
                              TRI_qry_order_t  const* orderStmt,
                              char const* name,
                              TRI_doc_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a query
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQuery (TRI_query_t* query);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a context
////////////////////////////////////////////////////////////////////////////////

TRI_rc_context_t* TRI_CreateContextQuery (TRI_query_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a context
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeContextQuery (TRI_rc_context_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query selection for unaltered documents
////////////////////////////////////////////////////////////////////////////////

TRI_qry_select_t* TRI_CreateQuerySelectDocument (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query selection for general, generated documents
////////////////////////////////////////////////////////////////////////////////

TRI_qry_select_t* TRI_CreateQuerySelectGeneral (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition for constant conditions
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereBoolean (bool where);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition for general, JavaScript conditions
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereGeneral (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition using the primary index and a constant
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWherePrimaryConstant (TRI_voc_did_t did);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition using an geo index and a constants
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereWithinConstant (TRI_idx_iid_t iid,
                                                     char const* nameDistance,
                                                     double latitiude,
                                                     double longitude,
                                                     double radius);


////////////////////////////////////////////////////////////////////////////////
/// @brief creates an order by clause for general JavaScript code
////////////////////////////////////////////////////////////////////////////////

TRI_qry_order_t* TRI_CreateQueryOrderGeneral (char const*);


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

void TRI_ReadLockCollectionsQuery (TRI_query_t* query);

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks all collections of a query
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
/// @brief executes a query
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
