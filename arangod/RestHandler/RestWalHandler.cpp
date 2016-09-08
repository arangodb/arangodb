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

#include "RestWalHandler.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Wal/LogfileManager.h"

using namespace arangodb;
using namespace arangodb::rest;

RestWalHandler::RestWalHandler(
    GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestHandler::status RestWalHandler::execute() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting /_admin/wal/<operation>");
    return status::DONE;
  }
  
  std::string const& operation = suffix[0];

  // extract the sub-request type
  auto const type = _request->requestType();

  if (operation == "transactions") {
    if (type == rest::RequestType::GET) {
      transactions();
      return status::DONE;
    }
  } else if (operation == "flush") {
    if (type == rest::RequestType::PUT) {
      flush();
      return status::DONE;
    }
  } else if (operation == "properties") {
    if (type == rest::RequestType::GET || type == rest::RequestType::PUT) {
      properties();
      return status::DONE;
    }
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting /_admin/wal/<operation>");
    return status::DONE;
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return status::DONE;
}

void RestWalHandler::properties() {
  auto l = arangodb::wal::LogfileManager::instance();

  if (_request->requestType() == rest::RequestType::PUT) {
    std::shared_ptr<VPackBuilder> parsedRequest;
    VPackSlice slice;
    try {
      slice = _request->payload();
    } catch (...) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid body value. expecting object");
      return;
    }
    if (!slice.isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid body value. expecting object");
    }

    if (slice.hasKey("allowOversizeEntries")) {
      bool value = slice.get("allowOversizeEntries").getBoolean();
      l->allowOversizeEntries(value);
    }
    
    if (slice.hasKey("logfileSize")) {
      uint32_t value = slice.get("logfileSize").getNumericValue<uint32_t>();
      l->filesize(value);
    }
    
    if (slice.hasKey("historicLogfiles")) {
      uint32_t value = slice.get("historicLogfiles").getNumericValue<uint32_t>();
      l->historicLogfiles(value);
    }
    
    if (slice.hasKey("reserveLogfiles")) {
      uint32_t value = slice.get("reserveLogfiles").getNumericValue<uint32_t>();
      l->reserveLogfiles(value);
    }
    
    if (slice.hasKey("throttleWait")) {
      uint64_t value = slice.get("throttleWait").getNumericValue<uint64_t>();
      l->maxThrottleWait(value);
    }
    
    if (slice.hasKey("throttleWhenPending")) {
      uint64_t value = slice.get("throttleWhenPending").getNumericValue<uint64_t>();
      l->throttleWhenPending(value);
    }
  }

  VPackBuilder builder;
  builder.openObject();
  builder.add("allowOversizeEntries", VPackValue(l->allowOversizeEntries()));
  builder.add("logfileSize", VPackValue(l->filesize()));
  builder.add("historicLogfiles", VPackValue(l->historicLogfiles()));
  builder.add("reserveLogfiles", VPackValue(l->reserveLogfiles()));
  builder.add("syncInterval", VPackValue(l->syncInterval()));
  builder.add("throttleWait", VPackValue(l->maxThrottleWait()));
  builder.add("throttleWhenPending", VPackValue(l->throttleWhenPending()));

  builder.close();
  generateResult(rest::ResponseCode::OK, builder.slice());
}

void RestWalHandler::flush() {
  std::shared_ptr<VPackBuilder> parsedRequest;
  VPackSlice slice;
  try {
    slice = _request->payload();
  } catch (...) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid body value. expecting object");
    return;
  }
  if (!slice.isObject() && !slice.isNone()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid body value. expecting object");
  }
  
  bool waitForSync = false;
  bool waitForCollector = false;

  if (slice.isObject()) {
    // got a request body
    VPackSlice value = slice.get("waitForSync");
    if (value.isString()) {
      waitForSync = (value.copyString() == "true");
    } else if (value.isBoolean()) {
      waitForSync = value.getBoolean();
    }
    
    value = slice.get("waitForCollector");
    if (value.isString()) {
      waitForCollector = (value.copyString() == "true");
    } else if (value.isBoolean()) {
      waitForCollector = value.getBoolean();
    }
  } else {
    // no request body
    bool found;
    {
      std::string const& v = _request->value("waitForSync", found);
      if (found) {
        waitForSync = (v == "1" || v == "true");
      }
    }
    {
      std::string const& v = _request->value("waitForCollector", found);
      if (found) {
        waitForCollector = (v == "1" || v == "true");
      }
    }
  }
  
  int res;
  if (ServerState::instance()->isCoordinator()) {
    res = flushWalOnAllDBServers(waitForSync, waitForCollector);
  } else {
    res = arangodb::wal::LogfileManager::instance()->flush(
        waitForSync, waitForCollector, false);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  generateResult(rest::ResponseCode::OK, basics::VelocyPackHelper::EmptyObjectValue());
}

void RestWalHandler::transactions() {
  auto const& info =
      arangodb::wal::LogfileManager::instance()->runningTransactions();
 
  VPackBuilder builder;
  builder.openObject();
  builder.add("runningTransactions", VPackValue(static_cast<double>(std::get<0>(info))));

  // lastCollectedId
  {
    auto value = std::get<1>(info);
    if (value == UINT64_MAX) {
      builder.add("minLastCollected", VPackValue(VPackValueType::Null));
    } else {
      builder.add("minLastCollected", VPackValue(value));
    }
  }

  // lastSealedId
  {
    auto value = std::get<2>(info);
    if (value == UINT64_MAX) {
      builder.add("minLastSealed", VPackValue(VPackValueType::Null));
    } else {
      builder.add("minLastSealed", VPackValue(value));
    }
  }

  builder.close();

  generateResult(rest::ResponseCode::OK, builder.slice());
}
