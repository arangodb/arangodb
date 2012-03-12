////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_V8_V8_VOCBASE_H
#define TRIAGENS_V8_V8_VOCBASE_H 1

#include "V8/v8-globals.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     documentation
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @page AvocadoScript Avocado Script
///
/// All the actions and transactions are programmed in JavaScript. The AvocadoDB
/// provides a fluent interface in JavaScript to access the database directly
/// together with a set of support function.  A fluent interface is implemented
/// by using method chaining to relay the instruction context of a subsequent
/// call.  The AvocadoDB defines the following methods:
///
/// - selection by example
/// - field selection (aka projection)
/// - sorting
/// - pagination of the result-set
///
/// Advanced topics are
///
/// - geo coordinates
///
/// A complete list of the available JavaScript functions can be found @ref
/// JavaScriptFunc "here".
///
/// @section FirstStepsFI First Steps
///
/// All documents are stored in collections. All collections are stored in a
/// database.
///
/// @verbinclude fluent41
///
/// Printing the @VAR{db} variable will show you the location, where the
/// datafiles of the collections are stored by default.
///
/// Creating a collection is simple.  It will automatically be created
/// when accessing the members of the @VAR{db}.
///
/// @verbinclude fluent42
///
/// If the collections does not exists, it is called a new-born. No file has
/// been created so far. If you access the collection, then the directory and
/// corresponding files will be created.
///
/// @verbinclude fluent43
///
/// If you restart the server and access the collection again, it will now show
/// as "unloaded".
///
/// @verbinclude fluent44
///
/// In order to create new documents in a collection, use the @FN{save}
/// operator.
///
/// @verbinclude fluent45
///
/// In order to select all elements of a collection, one can use the @FN{all}
/// operator.
///
/// @verbinclude fluent46
///
/// This will select all documents and prints the first 20 documents. If there
/// are more than 20 documents, then @CODE{...more results...} is printed and
/// you can use the variable @VAR{it} to access the next 20 document.
///
/// @verbinclude fluent2
///
/// In the above examples @CODE{db.examples.all()} defines a query. Printing
/// that query, executes the query and returns a cursor to the result set.  The
/// first 20 documents are printed and the query (resp. cursor) is assigned to
/// the variable @VAR{it}.
///
/// A query can also be executed using @FN{hasNext} and @FN{next}.
///
/// @verbinclude fluent3
///
/// Next steps:
///
/// - learn about @ref SelectionByExample
/// - learn about @ref FieldSelection
/// - learn about @ref Sorting
/// - learn about @ref Pagination
/// - learn about @ref GeoCoordinates
/// - look at all the @ref JavaScriptFunc
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page JavaScriptFuncIndex JavaScript Function Index
///
/// @section JSFDatabaseSelection Database Selection
///
/// - @ref MapGetVocBase "db".@FA{database}
///
/// - @ref MapGetVocBase "edges".@FA{database}
///
/// @section JSFDatabases Database Functions
///
/// - @ref JS_ParameterVocbaseCol "parameter"
///
/// @subsection JSFDocument Database Document Functions
///
/// - @ref JS_DeleteVocbaseCol "delete"
/// - @ref JS_ReplaceVocbaseCol "replace"
/// - @ref JS_SaveVocbaseCol "save"
/// - @ref JS_SaveEdgesCol "save" for edges
///
/// @subsection JSFIndex Database Index Functions
///
/// - @ref JS_DropIndexVocbaseCol "dropIndex"
/// - @ref JS_EnsureGeoIndexVocbaseCol "ensureGeoIndex"
/// - @ref JS_EnsureHashIndexVocbaseCol "ensureHashIndex"
/// - @ref JS_EnsureMultiHashIndexVocbaseCol "ensureMultiHashIndex"
/// - @ref JS_GetIndexesVocbaseCol "getIndexes"
///
/// @section JSFQueries Query Functions
///
/// @subsection JSFQueryBuilding Query Building Functions
///
/// - @ref JS_AllQuery "all"
/// - @ref JS_DocumentQuery "document"
/// - @ref JS_SelectQuery "select"
///
/// @section JSFGlobal Global Functions
///
/// - @ref JS_Execute "execute"
/// - @ref JS_Load "load"
/// - @ref JS_LogLevel "logLevel"
/// - @ref JS_Output "output"
/// - @ref JS_print "print"
/// - @ref JS_ProcessCsvFile "processCsvFile"
/// - @ref JS_ProcessJsonFile "processJsonFile"
/// - @ref JS_Read "read"
/// - @ref JS_Time "time"
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page JavaScriptFunc JavaScript Functions
///
/// @section JSFDatabaseSelection Database Selection
///
/// @ref MapGetVocBase "db".@FA{database}
///
/// @section JSFDatabases Database Functions
///
/// @copydetails JS_ParameterVocbaseCol
///
/// @subsection JSFDocument Database Document Functions
///
/// @copydetails JS_DeleteVocbaseCol
///
/// @copydetails JS_ReplaceVocbaseCol
///
/// @copydetails JS_SaveVocbaseCol
///
/// @copydetails JS_SaveEdgesCol
///
/// @subsection JSFIndex Database Index Functions
///
/// @copydetails JS_DropIndexVocbaseCol
///
/// @copydetails JS_EnsureGeoIndexVocbaseCol
///
/// @copydetails JS_EnsureHashIndexVocbaseCol
///
/// @copydetails JS_EnsureMultiHashIndexVocbaseCol
///
/// @copydetails JS_GetIndexesVocbaseCol
///
/// @section JSFQueries Query Functions
///
/// @subsection JSFQueryBuilding Query Building Functions
///
/// A complete list of the available query building functions can be found @ref
/// SimpleQueries "here".
///
/// @copydetails JS_DocumentQuery
///
/// @subsection JSFQueryExecuting Query Execution Functions
///
/// @section JSFGlobal Global Functions
///
/// @copydetails JS_Execute
///
/// @copydetails JS_Load
///
/// @copydetails JS_LogLevel
///
/// @copydetails JS_Output
///
/// @copydetails JSF_print
///
/// @copydetails JS_ProcessCsvFile
///
/// @copydetails JS_ProcessJsonFile
///
/// @copydetails JS_Read
///
/// @copydetails JS_Time
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapVocBase (TRI_vocbase_t const* database);

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_t for edges
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapEdges (TRI_vocbase_t const* database);

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapCollection (TRI_vocbase_col_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_col_t for edges
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapEdgesCollection (TRI_vocbase_col_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_WrapShapedJson (TRI_vocbase_col_t const* collection,
                                          TRI_doc_mptr_t const* document,
                                          TRI_barrier_t* barrier);
        
////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8VocBridge (v8::Handle<v8::Context> context, TRI_vocbase_t* vocbase);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
