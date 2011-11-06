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

#define TRI_WITHIN_C
#include <Basics/Common.h>
#include <VocBase/vocbase.h>
#undef TRI_WITHIN_C

#include <v8.h>

////////////////////////////////////////////////////////////////////////////////
/// @page FluentInterface Fluent Interface
///
/// A fluent interface is implemented by using method chaining to relay the
/// instruction context of a subsequent call.  The AvocadoDB provides the
/// following methods:
///
/// - selection by example
/// - @ref GeoFI "geo coordinates"
/// - field selection (aka projection)
/// - sorting
/// - cursors
/// - pagination of the result-set
///
////////////////////////////////////////////////////////////////////////////////
/// @section FirstStepsFI First Steps
////////////////////////////////////////////////////////////////////////////////
///
/// For instance, in order to select all elements of a collection "examples",
/// one can use the "all()" operator.
///
/// @verbinclude fluent1
///
/// This will select all documents and prints the first 20 documents. If there
/// are more than 20 documents, then "...more results..." is printed and you
/// can use the variable "it" to access the next 20 document.
///
/// @verbinclude fluent2
///
/// In the above examples "db.examples.all()" defines a query. Printing
/// that query, executes the query and returns a cursor to the result set.
/// The first 20 documents are printed and the query (resp. cursor) is
/// assigned to the variable "it".
///
/// A cursor can also be queried using "hasNext()" and "next()". Calling
/// either of these functions also executes the query, turning it into
/// a cursor.
///
/// @verbinclude fluent3
///
////////////////////////////////////////////////////////////////////////////////
/// @section GeoFI Geo Coordinates
////////////////////////////////////////////////////////////////////////////////
///
/// The AvocadoDB allows to selects documents based on geographic
/// coordinates. In order for this to work a geo-spatial index must be defined.
/// This index will use a very elaborate algorithm to lookup neighbours that is
/// a magnitude faster than a simple R* index.
///
/// In general a geo coordinate is a pair of latitude and longitude.  This can
/// either be an list with two elements like "[ -10, +30 ]" (latitude first,
/// followed by longitude) or an object like "{ lon: -10, lat: +30 }". In order
/// to find all documents within a given radius around a coordinate use the @ref
/// WithinFI "within()" operator. In order to find all documents near a given
/// document use the @ref NearFI "near()" operator.
///
/// It is possible to define more than one geo-spatial index per collection.  In
/// this case you must give a hint which of indexes should be used in a query.
///
////////////////////////////////////////////////////////////////////////////////
/// @subsection EnsureGeoIndexFI Create a Geo-Spatial Index
////////////////////////////////////////////////////////////////////////////////
///
/// @copydetails JS_EnsureGeoIndexVocbaseCol
///
////////////////////////////////////////////////////////////////////////////////
/// @subsection NearFI The Near Operator
////////////////////////////////////////////////////////////////////////////////
///
/// @copydetails JS_NearQuery
///
////////////////////////////////////////////////////////////////////////////////
/// @subsection WithinFI The Within Operator
////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
/// @subsection GeoOperatorFI The Geo Operator
////////////////////////////////////////////////////////////////////////////////
///
/// @copydetails JS_GeoQuery
///
////////////////////////////////////////////////////////////////////////////////
/// @section LimitFI The Limit Operator
////////////////////////////////////////////////////////////////////////////////
///
/// If, for example, you display the result of a user search, then you are in
/// general not interested in the completed result set, but only the first 10
/// documents. In this case, you can the "limit()" operator. This operators
/// works like LIMIT in MySQL, it specifies a maximal number of documents to
/// return.
///
/// @verbinclude fluent4
///
/// Specifying a limit of "0" returns no documents at all. If you do not need
/// a limit, just do not add the limit operator. If you specifiy a negtive
/// limit of -n, this will return the last n documents instead.
///
/// @verbinclude fluent8
///
////////////////////////////////////////////////////////////////////////////////
/// @section SkipFI The Skip Operator
////////////////////////////////////////////////////////////////////////////////
///
/// "skip" used together with "limit" can be used to implement pagination.
/// The "skip" operator skips over the first n documents. So, in order to
/// create result pages with 10 result documents per page, you can use
/// "skip(n * 10).limit(10)" to access the n.th page.
///
/// @verbinclude fluent5
///
////////////////////////////////////////////////////////////////////////////////
/// @section CountFI The Count Operator
////////////////////////////////////////////////////////////////////////////////
///
/// If you are implementation pagination, then you need the total amount of
/// documents a query returned. "count()" just does this. It returns the
/// total amount of documents regardless of any "limit()" and "skip()"
/// operator used before the "count()". If you really want these to be taken
/// into account, use "count(true)".
///
/// @verbinclude fluent6
///
////////////////////////////////////////////////////////////////////////////////
/// @section ExplainFI The Explain Operator
////////////////////////////////////////////////////////////////////////////////
///
/// @copydetails JS_ExplainQuery
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8VocBase V8-VocBase Bridge
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

v8::Persistent<v8::Context> InitV8VocBridge (TRI_vocbase_t* vocbase);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
