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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "RestShutdownHandler.h"

#include "Rest/HttpRequest.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RestShutdownHandler::RestShutdownHandler(arangodb::rest::HttpRequest* request,
                                         void* applicationServer)
    : RestBaseHandler(request),
      _applicationServer(
          static_cast<arangodb::rest::ApplicationServer*>(applicationServer)) {}

bool RestShutdownHandler::isDirect() const { return true; }

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_initiate
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestShutdownHandler::execute() {
  _applicationServer->beginShutdown();

  try {
    VPackBuilder json;
    json.add(VPackValue("OK"));
    VPackSlice slice(json.start());
    generateResult(slice);
  } catch (...) {
    // Ignore the error
  }

  return status_t(HANDLER_DONE);
}
