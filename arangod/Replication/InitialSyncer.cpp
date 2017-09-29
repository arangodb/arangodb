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

#include "InitialSyncer.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Logger/Logger.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "RestServer/DatabaseFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <cstring>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

InitialSyncer::InitialSyncer(
    ReplicationApplierConfiguration const* configuration,
    std::unordered_map<std::string, bool> const& restrictCollections,
    Syncer::RestrictType restrictType, bool verbose, bool skipCreateDrop)
    : Syncer(configuration),
      _progress("not started"),
      _restrictCollections(restrictCollections),
      _processedCollections(),
      _batchId(0),
      _batchUpdateTime(0),
      _batchTtl(180),
      _skipCreateDrop(skipCreateDrop) {
  if (_chunkSize == 0) {
    _chunkSize = (uint64_t)2 * 1024 * 1024;  // 2 mb
  } else if (_chunkSize < 128 * 1024) {
    _chunkSize = 128 * 1024;
  }

  _restrictType = restrictType;
  _verbose = verbose;
}

InitialSyncer::~InitialSyncer() {
  try {
    sendFinishBatch();
  } catch (...) {}
}

/// @brief send a "start batch" command
int InitialSyncer::sendStartBatch(std::string& errorMsg) {
  if (_isChildSyncer) {
    return TRI_ERROR_NO_ERROR;
  }
  
  _batchId = 0;
  std::string const url =
      BaseUrl + "/batch" + "?serverId=" + _localServerIdString;
  std::string const body = "{\"ttl\":" + StringUtils::itoa(_batchTtl) + "}";

  // send request
  std::string const progress = "send batch start command to url " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
      rest::RequestType::POST, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "could not connect to master at " + _masterInfo._endpoint +
               ": " + _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    return TRI_ERROR_REPLICATION_MASTER_ERROR;
  }

  VPackBuilder builder;
  int res = parseResponse(builder, response.get());

  if (res != TRI_ERROR_NO_ERROR) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  VPackSlice const slice = builder.slice();
  if (!slice.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  std::string const id = VelocyPackHelper::getStringValue(slice, "id", "");
  if (id.empty()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  _batchId = StringUtils::uint64(id);
  _batchUpdateTime = TRI_microtime();

  return TRI_ERROR_NO_ERROR;
}

/// @brief send an "extend batch" command
int InitialSyncer::sendExtendBatch() {
  if (_isChildSyncer || _batchId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  double now = TRI_microtime();

  if (now <= _batchUpdateTime + _batchTtl - 60.0) {
    // no need to extend the batch yet
    return TRI_ERROR_NO_ERROR;
  }

  std::string const url = BaseUrl + "/batch/" + StringUtils::itoa(_batchId) +
                          "?serverId=" + _localServerIdString;
  std::string const body = "{\"ttl\":" + StringUtils::itoa(_batchTtl) + "}";

  // send request
  std::string const progress = "send batch extend command to url " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(
      _client->request(rest::RequestType::PUT, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  int res = TRI_ERROR_NO_ERROR;

  if (response->wasHttpError()) {
    res = TRI_ERROR_REPLICATION_MASTER_ERROR;
  } else {
    _batchUpdateTime = TRI_microtime();
  }

  return res;
}

/// @brief send a "finish batch" command
int InitialSyncer::sendFinishBatch() {
  if (_isChildSyncer || _batchId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  try {
    std::string const url = BaseUrl + "/batch/" + StringUtils::itoa(_batchId) +
                            "?serverId=" + _localServerIdString;

    // send request
    std::string const progress = "send batch finish command to url " + url;
    setProgress(progress);

    std::unique_ptr<SimpleHttpResult> response(
        _client->retryRequest(rest::RequestType::DELETE_REQ, url, nullptr, 0));

    if (response == nullptr || !response->isComplete()) {
      return TRI_ERROR_REPLICATION_NO_RESPONSE;
    }

    TRI_ASSERT(response != nullptr);

    int res = TRI_ERROR_NO_ERROR;

    if (response->wasHttpError()) {
      res = TRI_ERROR_REPLICATION_MASTER_ERROR;
    } else {
      _batchId = 0;
      _batchUpdateTime = 0;
    }
    return res;
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

