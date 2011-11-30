////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
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

#ifndef TRIAGENS_DURHAM_V8_V8_VOCBASE_H
#define TRIAGENS_DURHAM_V8_V8_VOCBASE_H 1

#include <Basics/Common.h>

#include <VocBase/vocbase.h>

#include <v8.h>

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
/// - cursors
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
/// - learn about @ref GeoCoordinates "geo coordinates"
/// - learn about @ref Pagination "pagination"
/// - look at all the @ref JavaScriptFunc "functions"
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page GeoCoordinates Geo Coordinates
///
/// The AvocadoDB allows to selects documents based on geographic
/// coordinates. In order for this to work, a geo-spatial index must be defined.
/// This index will use a very elaborate algorithm to lookup neighbours that is
/// a magnitude faster than a simple R* index.
///
/// In general a geo coordinate is a pair of latitude and longitude.  This can
/// either be an list with two elements like @CODE{[ -10\, +30 ]} (latitude
/// first, followed by longitude) or an object like @CODE{{ lon: -10\, lat: +30
/// }}. In order to find all documents within a given radius around a
/// coordinate use the @FN{within} operator. In order to find all
/// documents near a given document use the @FN{near} operator.
///
/// It is possible to define more than one geo-spatial index per collection.  In
/// this case you must give a hint which of indexes should be used in a query.
///
/// @section EnsureGeoIndex Create a Geo-Spatial Index
///
/// First create an index.
///
/// @copydetails JS_EnsureGeoIndexVocbaseCol
///
/// @section NearOperator The Near Operator
///
/// @copydetails JS_NearQuery
///
/// @section WithinOperator The Within Operator
///
/// @copydetails JS_WithinQuery
///
/// @section GeoOperator The Geo Operator
///
/// @copydetails JS_GeoQuery
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page Pagination Pagination
///
/// @section LimitOperator The Limit Operator
///
/// If, for example, you display the result of a user search, then you are in
/// general not interested in the completed result set, but only the first 10
/// documents. In this case, you can the @FN{limit} operator. This operators
/// works like LIMIT in MySQL, it specifies a maximal number of documents to
/// return.
///
/// @verbinclude fluent4
///
/// @copydetails JS_LimitQuery
///
/// @section SkipOperator The Skip Operator
///
/// @FN{skip} used together with @FN{limit} can be used to implement
/// pagination.  The @FN{skip} operator skips over the first n documents. So, in
/// order to create result pages with 10 result documents per page, you can use
/// @CODE{skip(n * 10).limit(10)} to access the n.th page.
///
/// @verbinclude fluent5
///
/// @copydetails JS_SkipQuery
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
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8VocBridge (v8::Handle<v8::Context> context, TRI_vocbase_t* vocbase);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
