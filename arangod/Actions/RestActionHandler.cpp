////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Statistics/RequestStatistics.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestActionHandler::RestActionHandler(application_features::ApplicationServer& server,
                                     GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _action(TRI_LookupActionVocBase(request)),
      _data(nullptr) {}

RestStatus RestActionHandler::execute() {
  // need an action
  if (_action == nullptr) {
    generateNotImplemented(_request->fullUrl());
    return RestStatus::DONE;
  }

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
      executeAction();
      break;
    }

    default:
      generateNotImplemented("METHOD");
      break;
  }

  return RestStatus::DONE;
}

void RestActionHandler::cancel() {
  RestVocbaseBaseHandler::cancel();
  _action->cancel(&_dataLock, &_data);
}

/// @brief executes an action
void RestActionHandler::executeAction() {
  // handle redirections for web interface here
  rest::RequestType type = _request->requestType();
  if (type == rest::RequestType::GET) {
    std::vector<std::string> const& suffixes = _request->decodedSuffixes();
    if (suffixes.empty() ||
        (suffixes.size() == 2 && suffixes[0] == "_admin" && suffixes[1] == "html")) {
      // request to just /
      _response->setResponseCode(rest::ResponseCode::MOVED_PERMANENTLY);
      _response->setHeaderNC(StaticStrings::Location,
                             "/_db/" + StringUtils::encodeURIComponent(_vocbase.name()) +
                                 "/_admin/aardvark/index.html");
      return;
    }
  }

  TRI_action_result_t result =
      _action->execute(&_vocbase, _request.get(), _response.get(), &_dataLock, &_data);

  if (!result.isValid) {
    if (result.canceled) {
      generateCanceled();
    } else {
      generateNotImplemented(_action->_url);
    }
  }
}
