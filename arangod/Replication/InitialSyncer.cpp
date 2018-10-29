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

/*static*/ constexpr double InitialSyncer::defaultBatchTimeout;

InitialSyncer::InitialSyncer(
    ReplicationApplierConfiguration const& configuration)
    : Syncer(configuration),
      _progress("not started"),
      _processedCollections(),
      _batchId(0),
      _batchUpdateTime(0),
      _batchTtl(defaultBatchTimeout) {}

InitialSyncer::~InitialSyncer() {
  try {
    sendFinishBatch();
  } catch (...) {}
}

/// @brief send a "start batch" command
/// @param patchCount try to patch count of this collection
///        only effective with the incremental sync
Result InitialSyncer::sendStartBatch(std::string const& patchCount) {
  if (_isChildSyncer) {
    return Result();
  }
  
  double const now = TRI_microtime();
  _batchId = 0;
  // SimpleHttpClient automatically adds database prefix
  std::string const url = ReplicationUrl + "/batch" + "?serverId=" + _localServerIdString;
  VPackBuilder b;
  {
    VPackObjectBuilder guard(&b, true);
    guard->add("ttl", VPackValue(_batchTtl));
    if (!patchCount.empty()) {
      guard->add("patchCount", VPackValue(patchCount));
    }
  }
  std::string const body = b.toJson();

  // send request
  std::string const progress = "sending batch start command to url " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
      rest::RequestType::POST, url, body.c_str(), body.size()));

  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url);
  }

  VPackBuilder builder;
  Result r = parseResponse(builder, response.get());

  if (r.fail()) {
    return r;
  }

  VPackSlice const slice = builder.slice();
  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "start batch response is not an object");
  }

  std::string const id = VelocyPackHelper::getStringValue(slice, "id", "");
  if (id.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "start batch id is missing in response");
  }

  _batchId = StringUtils::uint64(id);
  _batchUpdateTime = now;

  return Result();
}

/// @brief send an "extend batch" command
Result InitialSyncer::sendExtendBatch() {
  if (_isChildSyncer || _batchId == 0) {
    return Result();
  }

  double const now = TRI_microtime();

  if (now <= _batchUpdateTime + _batchTtl * 0.25) {
    // no need to extend the batch yet - only extend it if a quarter of its ttl is already over
    return Result();
  }

  std::string const url = ReplicationUrl + "/batch/" + StringUtils::itoa(_batchId) +
                          "?serverId=" + _localServerIdString;
  std::string const body = "{\"ttl\":" + StringUtils::itoa(_batchTtl) + "}";

  // send request
  std::string const progress = "sending batch extend command to url " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(
      _client->request(rest::RequestType::PUT, url, body.c_str(), body.size()));
  
  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url);
  }
  
  _batchUpdateTime = now;

  return Result();
}

/// @brief send a "finish batch" command
Result InitialSyncer::sendFinishBatch() {
  if (_isChildSyncer || _batchId == 0) {
    return Result();
  }

  try {
    std::string const url = ReplicationUrl + "/batch/" + StringUtils::itoa(_batchId) +
                            "?serverId=" + _localServerIdString;

    // send request
    std::string const progress = "sending batch finish command to url " + url;
    setProgress(progress);

    std::unique_ptr<SimpleHttpResult> response(
        _client->retryRequest(rest::RequestType::DELETE_REQ, url, nullptr, 0));

    if (hasFailed(response.get())) {
      return buildHttpError(response.get(), url);
    }
    
    _batchId = 0;
    _batchUpdateTime = 0;
    return Result();
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }
}
