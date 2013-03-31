////////////////////////////////////////////////////////////////////////////////
/// @brief version request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestVersionHandler.h"

#include "BasicsC/json.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/conversions.h"
#include "Rest/HttpRequest.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestVersionHandler::RestVersionHandler (HttpRequest* request, version_options_t const* data)
  : RestBaseHandler(request),
    _name(data->_name),
    _version(data->_version),
    _isDirect(data->_isDirect),
    _queue(data->_queue) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup RestServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestVersionHandler::isDirect () {
  return _isDirect;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestVersionHandler::queue () {
  return _queue;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_e RestVersionHandler::execute () {
  TRI_json_t result;
  TRI_json_t server;
  TRI_json_t version;

  TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);

  TRI_InitStringJson(TRI_CORE_MEM_ZONE, &server, TRI_DuplicateString(_name.c_str()));
  TRI_Insert2ArrayJson(TRI_CORE_MEM_ZONE, &result, "server", &server);

  TRI_InitStringJson(TRI_CORE_MEM_ZONE, &version, TRI_DuplicateString(_version.c_str()));
  TRI_Insert2ArrayJson(TRI_CORE_MEM_ZONE, &result, "version", &version);

  generateResult(&result);
  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);

  return HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
