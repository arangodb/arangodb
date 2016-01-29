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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestQueryCacheHandler.h"
#include "Aql/QueryCache.h"
#include "Rest/HttpRequest.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestQueryCacheHandler::RestQueryCacheHandler(HttpRequest* request)
    : RestVocbaseBaseHandler(request) {}

bool RestQueryCacheHandler::isDirect() const { return false; }

HttpHandler::status_t RestQueryCacheHandler::execute() {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  switch (type) {
    case HttpRequest::HTTP_REQUEST_DELETE:
      clearCache();
      break;
    case HttpRequest::HTTP_REQUEST_GET:
      readProperties();
      break;
    case HttpRequest::HTTP_REQUEST_PUT:
      replaceProperties();
      break;

    case HttpRequest::HTTP_REQUEST_POST:
    case HttpRequest::HTTP_REQUEST_HEAD:
    case HttpRequest::HTTP_REQUEST_PATCH:
    case HttpRequest::HTTP_REQUEST_ILLEGAL:
    default: {
      generateNotImplemented("ILLEGAL " + DOCUMENT_PATH);
      break;
    }
  }

  // this handler is done
  return status_t(HANDLER_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock DeleteApiQueryCache
////////////////////////////////////////////////////////////////////////////////

bool RestQueryCacheHandler::clearCache() {
  auto queryCache = arangodb::aql::QueryCache::instance();
  queryCache->invalidate();
  try {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("error", VPackValue(false));
    result.add("code", VPackValue(HttpResponse::OK));
    result.close();
    VPackSlice slice = result.slice();
    generateResult(slice);
  } catch (...) {
    // Ignore the error
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock GetApiQueryCacheProperties
////////////////////////////////////////////////////////////////////////////////

bool RestQueryCacheHandler::readProperties() {
  try {
    auto queryCache = arangodb::aql::QueryCache::instance();

    VPackBuilder result = queryCache->properties();
    VPackSlice slice = result.slice();
    generateResult(slice);
  } catch (Exception const& err) {
    handleError(err);
  } catch (std::exception const& ex) {
    arangodb::basics::Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__,
                                    __LINE__);
    handleError(err);
  } catch (...) {
    arangodb::basics::Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
    handleError(err);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock PutApiQueryCacheProperties
////////////////////////////////////////////////////////////////////////////////

bool RestQueryCacheHandler::replaceProperties() {
  auto const& suffix = _request->suffix();

  if (suffix.size() != 1 || suffix[0] != "properties") {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/query-cache/properties");
    return true;
  }
  bool validBody = true;
  VPackOptions options;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&options, validBody);

  if (!validBody) {
    // error message generated in parseJsonBody
    return true;
  }
  VPackSlice body = parsedBody.get()->slice();

  if (!body.isObject()) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting a JSON-Object body");
    return true;
  }

  auto queryCache = arangodb::aql::QueryCache::instance();

  try {
    std::pair<std::string, size_t> cacheProperties;
    queryCache->properties(cacheProperties);

    VPackSlice attribute = body.get("mode");
    if (attribute.isString()) {
      cacheProperties.first = attribute.copyString();
    }

    attribute = body.get("maxResults");

    if (attribute.isNumber()) {
      cacheProperties.second = static_cast<size_t>(attribute.getUInt());
    }

    queryCache->setProperties(cacheProperties);

    return readProperties();
  } catch (Exception const& err) {
    handleError(err);
  } catch (std::exception const& ex) {
    arangodb::basics::Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__,
                                    __LINE__);
    handleError(err);
  } catch (...) {
    arangodb::basics::Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
    handleError(err);
  }

  return true;
}
