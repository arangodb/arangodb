////////////////////////////////////////////////////////////////////////////////
/// @brief V8 action functions
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

#ifndef TRIAGENS_V8_V8_ACTIONS_H
#define TRIAGENS_V8_V8_ACTIONS_H 1

#include <Basics/Common.h>

#include <v8.h>

#include <VocBase/vocbase.h>

////////////////////////////////////////////////////////////////////////////////
/// @page Actions First Steps with Actions
///
/// Actions are small JavaScript functions which are used to compute the result
/// of a request. Normally, the function will use the request parameter @a
/// collection to locate a collection, compute the result and return this result
/// as JSON object.
///
/// An action is defined using the function @c defineAction. You need to provide
/// a path, a function, and a description of the parameters.
///
/// @verbinclude action1
///
/// This will define a new action accessible under @c /_action/pagination, with
/// three parameters @a collection, @a blocksize, and @a page. The action
/// function is called with two parameters @a req and @a res. The variable @a
/// req contains the request parameters. The result is stored in the variable @a
/// res.
///
/// The function @c queryResult is predefined. It expects three parameters, the
/// request, the response, and a result set. The function @c queryResult uses
/// the parameters @a blocksize and @a page to paginate the result.
///
/// @verbinclude action2
///
/// The result contains the @a total number of documents, the number of
/// documents returned in @a count, the @a offset, the @a blocksize, the current
/// @a page, and the @a documents.
///
/// There is an alternative function @c queryReferences, which will only return
/// the document references, not the whole document.
///
/// @verbinclude action3
///
/// You can then use the rest interface to extract the documents.
///
/// @verbinclude action4
///
/// Next steps:
///
/// - learn more about @ref DefineAction "Defining an Action"
/// - learn about @ref ActionsQueryBuilding "Query Building Functions"
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page ActionsQueryBuildingTOC
///
/// <ol>
///  <li>all</li>
///  <li>distance</li>
///  <li>document</li>
///  <li>geo</li>
///  <li>limit</li>
///  <li>near</li>
///  <li>select</li>
///  <li>skip</li>
///  <li>within</li>
/// </ol>
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page ActionsQueryBuilding Query Building Functions for Actions
///
/// The following functions can be used to build the result-set.
///
/// <hr>
/// @copydoc ActionsQueryBuildingTOC
/// <hr>
///
/// @copydetails JS_AllQuery
///
/// @copydetails JS_DistanceQuery
///
/// @copydetails JS_DocumentQuery
///
/// @copydetails JS_GeoQuery
///
/// @copydetails JS_LimitQuery
///
/// @copydetails JS_NearQuery
///
/// @copydetails JS_SelectQuery
///
/// @copydetails JS_SkipQuery
///
/// @copydetails JS_WithinQuery
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class HttpResponse;
    class HttpRequest;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                           GENERAL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Actions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new action
////////////////////////////////////////////////////////////////////////////////

void TRI_CreateActionVocBase (std::string const& name,
                              v8::Handle<v8::Function> callback,
                              v8::Handle<v8::Object> options);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

triagens::rest::HttpResponse* TRI_ExecuteActionVocBase (TRI_vocbase_t* vocbase,
                                                        std::string const& name,
                                                        triagens::rest::HttpRequest* request);

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 actions function inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Actions (v8::Handle<v8::Context> context);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
