////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "RestActionHandler.h"

#include "Actions/actions.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Rest/HttpRequest.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestActionHandler::RestActionHandler(GeneralRequest* request,
                                     GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response),
      _action(nullptr),
      _dataLock(),
      _data(nullptr) {
  _action = TRI_LookupActionVocBase(request);
}

bool RestActionHandler::isDirect() const { return _action == nullptr; }

RestHandler::status RestActionHandler::execute() {
  TRI_action_result_t result;

  // check the request path
  if (_request->databaseName() == "_system") {
    if (StringUtils::isPrefix(_request->requestPath(), "/_admin/aardvark")) {
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
    rest::RequestType type = _request->requestType();

    // execute one of the HTTP methods
    switch (type) {
      case rest::RequestType::GET:
      case rest::RequestType::POST:
      case rest::RequestType::PUT:
      case rest::RequestType::DELETE_REQ:
      case rest::RequestType::HEAD:
      case rest::RequestType::OPTIONS:
      case rest::RequestType::PATCH: {
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
  return result.isValid ? status::DONE : status::FAILED;
}

bool RestActionHandler::cancel() { return _action->cancel(&_dataLock, &_data); }

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

TRI_action_result_t RestActionHandler::executeAction() {
  TRI_action_result_t result = _action->execute(
      _vocbase, _request.get(), _response.get(), &_dataLock, &_data);

  if (!result.isValid) {
    if (result.canceled) {
      generateCanceled();
    } else {
      generateNotImplemented(_action->_url);
    }
  }

  return result;
}
