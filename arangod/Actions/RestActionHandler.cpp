////////////////////////////////////////////////////////////////////////////////
/// @brief action request handler
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
/// @author Dr. Frank Celler
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestActionHandler.h"
#include "Actions/actions.h"
#include "Basics/StringUtils.h"
#include "Rest/HttpRequest.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestActionHandler::RestActionHandler (HttpRequest* request, 
                                      action_options_t* data)
  : RestVocbaseBaseHandler(request),
    _action(0),
    _queue(),
    _allowed(false) {
  
  _action = TRI_LookupActionVocBase(request);

  // check if the action is allowed
  if (_action != 0) {
    for (set<string>::const_iterator i = data->_contexts.begin();  i != data->_contexts.end();  ++i) {
      if (_action->_contexts.find(*i) != _action->_contexts.end()) {
        _allowed = true;
        break;
      }
    }

    if (! _allowed) {
      _action = 0;
    }
  }

  // use the queue from options if an action is known
  if (_action != 0) {
    _queue = data->_queue;
  }

  if (_queue.empty()) {
    // must have a queue
    _queue = "STANDARD";
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestActionHandler::isDirect () {
  return _action == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

std::string const& RestActionHandler::queue () const {
  return _queue;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestActionHandler::execute () {
  TRI_action_result_t result;

  // need an action
  if (_action == 0) {
    generateNotImplemented(_request->requestPath());
  }

  // need permission
  else if (! _allowed) {
    generateForbidden();
  }

  // execute
  else {

    // extract the sub-request type
    HttpRequest::HttpRequestType type = _request->requestType();

    // execute one of the HTTP methods
    switch (type) {
      case HttpRequest::HTTP_REQUEST_GET:
      case HttpRequest::HTTP_REQUEST_POST:
      case HttpRequest::HTTP_REQUEST_PUT:
      case HttpRequest::HTTP_REQUEST_DELETE:
      case HttpRequest::HTTP_REQUEST_HEAD:
      case HttpRequest::HTTP_REQUEST_OPTIONS:
      case HttpRequest::HTTP_REQUEST_PATCH: {
        result = executeAction();
        break;
      }

      default:
        result.isValid = true;
        generateNotImplemented("METHOD");
        break;
    }
  }

  // handler has finished, generate result
  if (result.isValid) {
    if (result.requeue) {
      status_t status(HANDLER_REQUEUE);
      status.sleep = result.sleep;

      return status;
    }
    else {
      return status_t(HANDLER_DONE);
    }
  }
  else {
    return status_t(HANDLER_FAILED);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

TRI_action_result_t RestActionHandler::executeAction () {
  TRI_action_result_t result = _action->execute(_vocbase, _request);

  if (result.isValid) {
    _response = result.response;
  }
  else {
    result.isValid = true;
    generateNotImplemented(_action->_url);
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
