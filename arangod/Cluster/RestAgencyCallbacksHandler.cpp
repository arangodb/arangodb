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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "RestAgencyCallbacksHandler.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Cluster/AgencyCallbackRegistry.h"

using namespace arangodb;
using namespace arangodb::rest;

RestAgencyCallbacksHandler::RestAgencyCallbacksHandler(arangodb::HttpRequest* request,
    arangodb::AgencyCallbackRegistry* agencyCallbackRegistry)
    : RestVocbaseBaseHandler(request),
    _agencyCallbackRegistry(agencyCallbackRegistry) {
}

bool RestAgencyCallbacksHandler::isDirect() const { return false; }

arangodb::rest::HttpHandler::status_t RestAgencyCallbacksHandler::execute() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid callback");
    return status_t(HANDLER_DONE);
  }

  // extract the sub-request type
  auto const type = _request->requestType();
  if (type != GeneralRequest::RequestType::POST) {
    generateError(GeneralResponse::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return status_t(HANDLER_DONE);
  }
  
  bool parseSuccess = true;
  VPackOptions options = VPackOptions::Defaults;
  options.checkAttributeUniqueness = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&options, parseSuccess);
  if (!parseSuccess) {
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid JSON");
    return status_t(HANDLER_DONE);
  }

  try {
    std::stringstream ss(suffix.at(0));
    uint32_t index;
    ss >> index;

    auto callback = _agencyCallbackRegistry->getCallback(index);
    LOG(DEBUG) << "Agency callback has been triggered. refetching!";
    callback->refetchAndUpdate();
    createResponse(arangodb::GeneralResponse::ResponseCode::ACCEPTED);
  } catch (arangodb::basics::Exception const&) {
    // mop: not found...expected
    createResponse(arangodb::GeneralResponse::ResponseCode::NOT_FOUND);
  }
  return status_t(HANDLER_DONE);
}
