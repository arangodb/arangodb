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

#ifndef TRIAGENS_DURHAM_V8_V8_ACTIONS_H
#define TRIAGENS_DURHAM_V8_V8_ACTIONS_H 1

#include <BasicsC/Common.h>

#include <v8.h>

#include <VocBase/vocbase.h>

////////////////////////////////////////////////////////////////////////////////
/// @page Actions First Steps with Actions
///
/// Actions are small JavaScript functions which are used to compute the result
/// of a request. Normally, the function will use the request parameter
/// @FA{collection} to locate a collection, compute the result and return this
/// result as JSON object.
///
/// Actions are defined in JaveScript files living in a directory called
/// _ACTIONS under the database directory. The files must exists when the
/// server is started. You can use the @CO{action.directory} to use a different
/// directory during startup.
///
/// Inside these files an action is defined using the function
/// @FN{defineAction}. You need to provide a path, a function, and a description
/// of the parameters.
///
/// @verbinclude action1
///
/// This will define a new action accessible under @LIT{/_action/pagination}, with
/// three parameters @FA{collection}, @FA{blocksize}, and @FA{page}. The action
/// function is called with two parameters @LIT{req} and @LIT{res}. The variable
/// @LIT{req} contains the request parameters. The result is stored in the
/// variable @FA{res}.
///
/// The function @FN{queryResult} is predefined. It expects three parameters,
/// the request, the response, and a result set. The function @FN{queryResult}
/// uses the parameters @FA{blocksize} and @FA{page} to paginate the result.
///
/// @verbinclude action2
///
/// The result contains the @LIT{total} number of documents, the number of
/// documents is returned in @LIT{count}, it also contains the @FA{offset}, the
/// @FA{blocksize}, the current @FA{page}, and the @FA{documents}.
///
/// There is an alternative function @FN{queryReferences}, which will only
/// return the document references, not the whole document.
///
/// @verbinclude action3
///
/// You can then use the rest interface to extract the documents.
///
/// @verbinclude action4
///
/// Next steps: learn more about
///
/// - @ref DefineAction
/// - @ref ActionsQueryBuilding
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page ActionsQueryBuildingTOC
///
/// <ol>
///  <li>@ref ActionsQueryBuildingAll "all"</li>
///  <li>@ref ActionsQueryBuildingDistance "distance"</li>
///  <li>@ref ActionsQueryBuildingDocument "document"</li>
///  <li>@ref ActionsQueryBuildingGeo "geo"</li>
///  <li>@ref ActionsQueryBuildingLimit "limit"</li>
///  <li>@ref ActionsQueryBuildingNear "near"</li>
///  <li>@ref ActionsQueryBuildingSelect "select"</li>
///  <li>@ref ActionsQueryBuildingSkip "skip"</li>
///  <li>@ref ActionsQueryBuildingWithin "within"</li>
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
/// @anchor ActionsQueryBuildingAll
/// @copydetails JS_AllQuery
///
/// @anchor ActionsQueryBuildingDistance
/// @copydetails JS_DistanceQuery
///
/// @anchor ActionsQueryBuildingDocument
/// @copydetails JS_DocumentQuery
///
/// @anchor ActionsQueryBuildingGeo
/// @copydetails JS_GeoQuery
///
/// @anchor ActionsQueryBuildingLimit
/// @copydetails JS_LimitQuery
///
/// @anchor ActionsQueryBuildingNear
/// @copydetails JS_NearQuery
///
/// @anchor ActionsQueryBuildingSelect
/// @copydetails JS_SelectQuery
///
/// @anchor ActionsQueryBuildingSkip
/// @copydetails JS_SkipQuery
///
/// @anchor ActionsQueryBuildingWithin
/// @copydetails JS_WithinQuery
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page DefineActionTOC
///
/// <ol>
///  <li>@ref DefineActionDefineAction "defineAction"</li>
///  <li>@ref DefineActionDefineSystemAction "defineSystemAction"</li>
/// </ol>
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page DefineAction Defining an Action
///
/// Actions are user defined functions and are stored in JavaScript files in a
/// directory called _ACTIONS under the database directory. System action are
/// predefined actions used by the server. They are stored in system wide
/// directory.
///
/// <hr>
/// @copydoc DefineActionTOC
/// <hr>
///
/// @anchor DefineActionDefineAction
/// @copydetails JS_DefineAction
///
/// @anchor DefineActionDefineSystemAction
/// @copydetails JS_DefineSystemAction
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
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Actions
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new action
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_ACT_CATEGORY_USER,
  TRI_ACT_CATEGORY_SYSTEM
}
TRI_action_category_e;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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

void TRI_CreateActionVocBase (TRI_action_category_e,
                              std::string const& name,
                              v8::Handle<v8::Function> callback,
                              v8::Handle<v8::Object> options);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

triagens::rest::HttpResponse* TRI_ExecuteActionVocBase (TRI_vocbase_t* vocbase,
                                                        TRI_action_category_e category,
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
