////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBRestWalHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Utils/ExecContext.h"

#include <rocksdb/utilities/transaction_db.h>

using namespace arangodb;
using namespace arangodb::rest;

RocksDBRestWalHandler::RocksDBRestWalHandler(ArangodServer& server,
                                             GeneralRequest* request,
                                             GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RocksDBRestWalHandler::execute() {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting /_admin/wal/<operation>");
    return RestStatus::DONE;
  }

  // extract the sub-request type
  auto const type = _request->requestType();
  std::string const& operation = suffixes[0];
  if (operation == "transactions") {
    if (type == rest::RequestType::GET) {
      transactions();
      return RestStatus::DONE;
    }
  } else if (operation == "flush") {
    if (type == rest::RequestType::PUT) {
      flush();
      return RestStatus::DONE;
    }
  } else if (operation == "properties") {
    if (type == rest::RequestType::GET || type == rest::RequestType::PUT) {
      properties();
      return RestStatus::DONE;
    }
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting /_admin/wal/<operation>");
    return RestStatus::DONE;
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}

void RocksDBRestWalHandler::properties() {
  // not supported on rocksdb
  generateResult(rest::ResponseCode::NOT_IMPLEMENTED,
                 arangodb::velocypack::Slice::emptyObjectSlice());
}

void RocksDBRestWalHandler::flush() {
  bool parseSuccess = true;
  VPackSlice slice = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error already created
    return;
  }
  if (!slice.isObject() && !slice.isNone()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid body value. expecting object");
    return;
  }

  bool waitForSync =
      _request->parsedValue(StaticStrings::WaitForSyncString, false);
  bool flushColumnFamilies = _request->parsedValue("waitForCollector", false);

  if (slice.isObject()) {
    // got a request body
    VPackSlice value = slice.get(StaticStrings::WaitForSyncString);
    if (value.isString()) {
      waitForSync = basics::StringUtils::boolean(value.stringView());
    } else if (value.isBoolean()) {
      waitForSync = value.getBoolean();
    }

    value = slice.get("waitForCollector");
    if (value.isString()) {
      flushColumnFamilies = basics::StringUtils::boolean(value.stringView());
    } else if (value.isBoolean()) {
      flushColumnFamilies = value.getBoolean();
    }
  }

  Result res;
  if (ServerState::instance()->isCoordinator()) {
    auto& feature = server().getFeature<ClusterFeature>();
    res = flushWalOnAllDBServers(feature, waitForSync, flushColumnFamilies);
  } else {
    server().getFeature<EngineSelectorFeature>().engine().flushWal(
        waitForSync, flushColumnFamilies);
  }

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  generateResult(rest::ResponseCode::OK,
                 arangodb::velocypack::Slice::emptyObjectSlice());
}

void RocksDBRestWalHandler::transactions() {
  transaction::Manager* mngr = transaction::ManagerFeature::manager();
  VPackBuilder builder;
  builder.openObject();
  builder.add("runningTransactions",
              VPackValue(mngr->getActiveTransactionCount()));
  builder.close();
  generateResult(rest::ResponseCode::NOT_IMPLEMENTED, builder.slice());
}
