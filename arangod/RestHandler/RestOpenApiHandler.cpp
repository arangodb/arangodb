////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "RestOpenApiHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Rest/CommonDefines.h"

using namespace arangodb;
using namespace arangodb::rest;

namespace {
// Embedded OpenAPI specs as compile-time byte arrays
constexpr char kOpenApiV0[] = {
#include "openapi-v0.csx"
};

constexpr char kOpenApiV1[] = {
#include "openapi-v1.csx"
};
}  // namespace

RestOpenApiHandler::RestOpenApiHandler(
    application_features::ApplicationServer& server, GeneralRequest* request,
    GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

std::string_view RestOpenApiHandler::getOpenApiSpec(uint32_t apiVersion) const {
  switch (apiVersion) {
    case 0:
      return std::string_view(kOpenApiV0, sizeof(kOpenApiV0));
    case 1:
      return std::string_view(kOpenApiV1, sizeof(kOpenApiV1));
    default:
      return std::string_view();
  }
}

RestStatus RestOpenApiHandler::execute() {
  // Get requested API version (already parsed by GeneralRequest)
  uint32_t apiVersion = _request->requestedApiVersion();

  // Get the OpenAPI spec for this version
  std::string_view spec = getOpenApiSpec(apiVersion);

  if (spec.empty()) {
    // No spec available for this version
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "OpenAPI specification not available for API version " +
                      std::to_string(apiVersion));
    return RestStatus::DONE;
  }

  // Set response headers
  _response->setResponseCode(rest::ResponseCode::OK);
  _response->setContentType(rest::ContentType::JSON);
  _response->setAllowCompression(
      rest::ResponseCompressionType::kAllowCompression);

  // Add the raw JSON payload
  _response->addRawPayload(spec);

  return RestStatus::DONE;
}
