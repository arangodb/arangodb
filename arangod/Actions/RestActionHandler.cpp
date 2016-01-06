////////////////////////////////////////////////////////////////////////////////
/// @brief action request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
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


////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestActionHandler::RestActionHandler(HttpRequest* request,
                                     action_options_t* data)
    : RestVocbaseBaseHandler(request),
      _action(nullptr),
      _dataLock(),
      _data(nullptr) {
  _action = TRI_LookupActionVocBase(request);
}


////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestActionHandler::isDirect() const { return _action == nullptr; }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestActionHandler::execute() {
  TRI_action_result_t result;

  // check the request path
  if (_request->databaseName() == "_system") {
    if (TRI_IsPrefixString(_request->requestPath(), "/_admin/aardvark")) {
      requestStatisticsAgentSetIgnore();
    }
  }

  // need an action
  if (_action == nullptr) {
    generateNotImplemented(_request->requestPath());
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
  return status_t(result.isValid ? HANDLER_DONE : HANDLER_FAILED);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestActionHandler::cancel() { return _action->cancel(&_dataLock, &_data); }


////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

TRI_action_result_t RestActionHandler::executeAction() {
  TRI_action_result_t result =
      _action->execute(_vocbase, _request, &_dataLock, &_data);

  if (result.isValid) {
    _response = result.response;
    result.response = nullptr;
  } else if (result.canceled) {
    result.isValid = true;
    generateCanceled();
  } else {
    result.isValid = true;
    generateNotImplemented(_action->_url);
  }

  if (result.response != nullptr) {
    delete result.response;
  }

  return result;
}


