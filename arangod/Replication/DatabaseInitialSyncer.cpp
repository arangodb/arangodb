////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "DatabaseInitialSyncer.h"
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
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <cstring>
#include <array>
#include <cstring>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

size_t const DatabaseInitialSyncer::MaxChunkSize = 10 * 1024 * 1024;

DatabaseInitialSyncer::DatabaseInitialSyncer(
    TRI_vocbase_t& vocbase,
    ReplicationApplierConfiguration const& configuration
): InitialSyncer(configuration),
   _vocbase(vocbase),
   _hasFlushed(false) {
  _vocbases.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(vocbase.name()),
    std::forward_as_tuple(vocbase)
  );

  if (configuration._database.empty()) {
    _databaseName = vocbase.name();
  }
}

/// @brief run method, performs a full synchronization
Result DatabaseInitialSyncer::runWithInventory(bool incremental,
                                               VPackSlice inventoryColls) {
  if (_client == nullptr || _connection == nullptr || _endpoint == nullptr) {
    return Result(TRI_ERROR_INTERNAL, "invalid endpoint");
  }

  auto res = vocbase().replicationApplier()->preventStart();

  if (res.fail()) {
    return res;
  }

  TRI_DEFER(vocbase().replicationApplier()->allowStart());

  setAborted(false);

  double const startTime = TRI_microtime();

  try {
    setProgress("fetching master state");

    LOG_TOPIC(DEBUG, Logger::REPLICATION)
      << "client: getting master state to dump " << vocbase().name();
    Result r;

    if (!_isChildSyncer) {
      r = getMasterState();

      if (r.fail()) {
        return r;
      }
    }

    TRI_ASSERT(!_masterInfo._endpoint.empty());
    TRI_ASSERT(_masterInfo._serverId != 0);
    TRI_ASSERT(_masterInfo._majorVersion != 0);

    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "client: got master state";
    if (incremental) {
      if (_masterInfo._majorVersion == 1 ||
          (_masterInfo._majorVersion == 2 && _masterInfo._minorVersion <= 6)) {
        LOG_TOPIC(WARN, Logger::REPLICATION) << "incremental replication is "
                                                "not supported with a master < "
                                                "ArangoDB 2.7";
        incremental = false;
      } else {
        r = sendFlush();
        if (r.fail()) {
          return r;
        }
      }
    }

    // create a WAL logfile barrier that prevents WAL logfile collection
    // TODO: why are we ignoring the return value here?
    sendCreateBarrier(_masterInfo._lastLogTick);

    r = sendStartBatch();
    if (r.fail()) {
      return r;
    }
    
    VPackBuilder inventoryResponse;
    if (!inventoryColls.isArray()) {
      // caller did not supply an inventory, we need to fetch it
      Result res = fetchInventory(inventoryResponse);
      if (!res.ok()) {
        return res;
      }
      // we do not really care about the state response
      inventoryColls = inventoryResponse.slice().get("collections");
      if (!inventoryColls.isArray()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      "collections section is missing from response");
      }
    }

    // strip eventual objectIDs and then dump the collections
    auto pair = rocksutils::stripObjectIds(inventoryColls);
    r = handleLeaderCollections(pair.first, incremental);

    // all done here, do not try to finish batch if master is unresponsive
    if (r.isNot(TRI_ERROR_REPLICATION_NO_RESPONSE)) {
      sendFinishBatch();
    }

    if (r.fail()) {
      LOG_TOPIC(ERR, Logger::REPLICATION) << "Error during initial sync: " << r.errorMessage();
    }

    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "initial synchronization with master took: "
      << Logger::FIXED(TRI_microtime() - startTime, 6) << " s. status: " << r.errorMessage();

    return r;
  } catch (arangodb::basics::Exception const& ex) {
    sendFinishBatch();
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    sendFinishBatch();
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    sendFinishBatch();
    return Result(TRI_ERROR_NO_ERROR, "an unknown exception occurred");
  }
}

/// @brief returns the inventory
Result DatabaseInitialSyncer::inventory(VPackBuilder& builder) {
  if (_client == nullptr || _connection == nullptr || _endpoint == nullptr) {
    return Result(TRI_ERROR_INTERNAL, "invalid endpoint");
  }
  
  auto r = sendStartBatch();
  if (r.fail()) {
    return r;
  }
      
  TRI_DEFER(sendFinishBatch());
   
  // caller did not supply an inventory, we need to fetch it
  return fetchInventory(builder);
}

/// @brief check whether the initial synchronization should be aborted
bool DatabaseInitialSyncer::isAborted() const {
  if (application_features::ApplicationServer::isStopping() ||
      (vocbase().replicationApplier() != nullptr &&
       vocbase().replicationApplier()->stopInitialSynchronization())) {
    return true;
  }

  return Syncer::isAborted();
}

void DatabaseInitialSyncer::setProgress(std::string const& msg) {
  _progress = msg;

  if (_configuration._verbose) {
    LOG_TOPIC(INFO, Logger::REPLICATION) << msg;
  } else {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << msg;
  }

  auto* applier = vocbase().replicationApplier();

  if (applier != nullptr) {
    applier->setProgress(msg);
  }
}

/// @brief send a WAL flush command
Result DatabaseInitialSyncer::sendFlush() {
  std::string const url = "/_admin/wal/flush";
  std::string const body =
      "{\"waitForSync\":true,\"waitForCollector\":true,"
      "\"waitForCollectorQueue\":true}";

  // send request
  std::string const progress = "sending WAL flush command to url " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
      rest::RequestType::PUT, url, body.c_str(), body.size()));

  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url);
  }

  _hasFlushed = true;
  return Result();
}

namespace {
  std::string const kTypeString = "type";
  std::string const kDataString = "data";
}

/// @brief handle a single dump marker
Result DatabaseInitialSyncer::parseCollectionDumpMarker(transaction::Methods& trx,
                                                        LogicalCollection* coll,
                                                        VPackSlice const& marker) {
  
  if (!marker.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  TRI_replication_operation_e type = REPLICATION_INVALID;
  VPackSlice doc;
  
  for (auto const& it : VPackObjectIterator(marker, true)) {
    if (it.key.isEqualString(kTypeString)) {
      if (it.value.isNumber()) {
        type = static_cast<TRI_replication_operation_e>(it.value.getNumber<int>());
      }
    } else if (it.key.isEqualString(kDataString)) {
      if (it.value.isObject()) {
        doc = it.value;
      }
    }
    if (type != REPLICATION_INVALID && doc.isObject()) {
      break;
    }
  }
  
  if (!doc.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  // key must not be empty, but doc can otherwise be empty
  VPackSlice key = doc.get(StaticStrings::KeyString);
  if (!key.isString() || key.getStringLength() == 0) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  return applyCollectionDumpMarker(trx, coll, type, doc);
}

/// @brief apply the data from a collection dump
Result DatabaseInitialSyncer::parseCollectionDump(transaction::Methods& trx,
                                                  LogicalCollection* coll,
                                                  SimpleHttpResult* response,
                                                  uint64_t& markersProcessed) {
  std::string const invalidMsg =
  "received invalid dump data for collection '" + coll->name() + "'";
  
  StringBuffer const& data = response->getBody();
  char const* p = data.begin();
  char const* end = p + data.length();
  
  bool found = false;
  std::string cType = response->getHeaderField(StaticStrings::ContentTypeHeader, found);
  if (found && (cType == StaticStrings::MimeTypeVPack)) {
    
    VPackOptions options;
    options.unsupportedTypeBehavior = VPackOptions::FailOnUnsupportedType;
    VPackValidator validator(&options);
    
    try {
      while (p < end) {
        ptrdiff_t remaining = end - p;
        // throws if the data is invalid
        validator.validate(p, remaining, /*isSubPart*/true);
        
        VPackSlice marker(p);
        Result r = parseCollectionDumpMarker(trx, coll, marker);
        if (r.fail()) {
          r.reset(r.errorNumber(), invalidMsg);
          return r;
        }
        ++markersProcessed;
        p += marker.byteSize();
      }
    } catch(velocypack::Exception const& e) {
      LOG_TOPIC(ERR, Logger::REPLICATION) << "Error parsing VPack response: " << e.what();
      return Result(e.errorCode(), e.what());
    }
    
  } else {
    // buffer must end with a NUL byte
    TRI_ASSERT(*end == '\0');
    
    VPackBuilder builder;
    while (p < end) {
      char const* q = strchr(p, '\n');
      if (q == nullptr) {
        q = end;
      }
      
      if (q - p < 2) {
        // we are done
        return Result();
      }
      
      TRI_ASSERT(q <= end);
      try {
        builder.clear();
        VPackParser parser(builder);
        parser.parse(p, static_cast<size_t>(q - p));
      } catch (velocypack::Exception const& e) {
        LOG_TOPIC(ERR, Logger::REPLICATION) << "while parsing collection dump: " << e.what();
        return Result(e.errorCode(), e.what());
      }
      
      p = q + 1;
      
      Result r = parseCollectionDumpMarker(trx, coll, builder.slice());
      if (r.fail()) {
        return r;
      }
      ++markersProcessed;
    }
  }
  
  // reached the end
  return Result();
}

// lets keep this experimental until we make it faster
#define VPACK_DUMP 0

/// @brief incrementally fetch data from a collection
Result DatabaseInitialSyncer::fetchCollectionDump(arangodb::LogicalCollection* coll,
                                                  std::string const& leaderColl,
                                                  TRI_voc_tick_t maxTick) {
  std::string appendix;
  
  if (_hasFlushed) {
    appendix = "&flush=false";
  } else {
    // only flush WAL once
    appendix = "&flush=true&flushWait=15";
    _hasFlushed = true;
  }
  
  uint64_t chunkSize = _configuration._chunkSize;
  
  TRI_ASSERT(_batchId);  // should not be equal to 0
  std::string const baseUrl = ReplicationUrl + "/dump?collection=" + StringUtils::urlEncode(leaderColl) +
  "&batchId=" + std::to_string(_batchId) + appendix;
  
  TRI_voc_tick_t fromTick = 0;
  int batch = 1;
  uint64_t bytesReceived = 0;
  uint64_t markersProcessed = 0;
  
#if VPACK_DUMP
  double const nowTotal = TRI_microtime();
  double sumFetchTime = 0;
  double sumApplyTime = 0;
#endif
  
  while (true) {
    if (isAborted()) {
      return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }
    
    sendExtendBatch();
    sendExtendBarrier();
    
    std::string url = baseUrl + "&from=" + StringUtils::itoa(fromTick);
    
    if (maxTick > 0) {
      url += "&to=" + StringUtils::itoa(maxTick + 1);
    }
    
    url += "&serverId=" + _localServerIdString;
    url += "&chunkSize=" + StringUtils::itoa(chunkSize);
    url += "&includeSystem=" + std::string(_configuration._includeSystem ? "true" : "false");
    
    std::string const typeString =
        (coll->type() == TRI_COL_TYPE_EDGE ? "edge" : "document");
    
    // send request
    std::string const progress =
        "fetching master collection dump for collection '" + coll->name() +
        "', type: " + typeString + ", id " + leaderColl + ", batch " +
        StringUtils::itoa(batch) +
        ", markers processed: " + StringUtils::itoa(markersProcessed) +
        ", bytes received: " + StringUtils::itoa(bytesReceived);
    
    setProgress(progress);
    
    // use async mode for first batch
    auto headers = createHeaders();
    if (batch == 1) {
      headers[StaticStrings::Async] = "store";
    }
#if VPACK_DUMP
    int vv = _masterInfo._majorVersion * 1000000 + _masterInfo._minorVersion * 1000;
    if (vv >= 3003009) {
      headers[StaticStrings::Accept] = StaticStrings::MimeTypeVPack;
    }
    double fetchTimeNow = TRI_microtime();
#endif
    
    std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
        rest::RequestType::GET, url, nullptr, 0, headers));
    
    if (hasFailed(response.get())) {
      return buildHttpError(response.get(), url);
    }
    
    // use async mode for first batch
    if (batch == 1) {
      bool found = false;
      std::string jobId =
          response->getHeaderField(StaticStrings::AsyncId, found);
      
      if (!found) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                      _masterInfo._endpoint + url + ": could not find 'X-Arango-Async' header");
      }
      
      double const startTime = TRI_microtime();
      
      // wait until we get a responsable response
      while (true) {
        sendExtendBatch();
        sendExtendBarrier();
        
        std::string const jobUrl = "/_api/job/" + jobId;
        response.reset(
                       _client->request(rest::RequestType::PUT, jobUrl, nullptr, 0));
        
        if (response != nullptr && response->isComplete()) {
          if (response->hasHeaderField("x-arango-async-id")) {
            // got the actual response
            break;
          }
          if (response->getHttpReturnCode() == 404) {
            // unknown job, we can abort
            return Result(TRI_ERROR_REPLICATION_NO_RESPONSE, std::string("job not found on master at ") + _masterInfo._endpoint);
          }
        }
        
        double waitTime = TRI_microtime() - startTime;
        
        if (static_cast<uint64_t>(waitTime * 1000.0 * 1000.0) >=
            _configuration._initialSyncMaxWaitTime) {
          return Result(TRI_ERROR_REPLICATION_NO_RESPONSE, std::string("timed out waiting for response from master at ") + _masterInfo._endpoint);
        }
        
        std::chrono::milliseconds sleepTime;
        if (waitTime < 5.0) {
          sleepTime = std::chrono::milliseconds(250);
        } else if (waitTime < 20.0) {
          sleepTime = std::chrono::milliseconds(500);
        } else if (waitTime < 60.0) {
          sleepTime = std::chrono::seconds(1);
        } else {
          sleepTime = std::chrono::seconds(2);
        }
        
        if (isAborted()) {
          return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
        }
        std::this_thread::sleep_for(sleepTime);
      }
      // fallthrough here in case everything went well
    }
    
#if VPACK_DUMP
    sumFetchTime += (TRI_microtime() - fetchTimeNow);
#endif
    
    if (hasFailed(response.get())) {
      return buildHttpError(response.get(), url);
    }
    
    if (response->hasContentLength()) {
      bytesReceived += response->getContentLength();
    }
    
    bool found;
    std::string header = response->getHeaderField(StaticStrings::ReplicationHeaderCheckMore, found);
    if (!found) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                    _masterInfo._endpoint + url + ": required header " + StaticStrings::ReplicationHeaderCheckMore +
                    " is missing in dump response");
    }
    
    TRI_voc_tick_t tick;
    bool checkMore = StringUtils::boolean(header);
    
    if (checkMore) {
      header = response->getHeaderField(StaticStrings::ReplicationHeaderLastIncluded,
                                        found);
      if (!found) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                      _masterInfo._endpoint + url + ": required header " + StaticStrings::ReplicationHeaderLastIncluded
                      + " is missing in dump response");
      }

      tick = StringUtils::uint64(header);

      if (tick > fromTick) {
        fromTick = tick;
      } else {
        // we got the same tick again, this indicates we're at the end
        checkMore = false;
      }
    }

#if VPACK_DUMP
    double applyTimeNow = TRI_microtime();
#endif

    SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(vocbase()),
      coll,
      AccessMode::Type::EXCLUSIVE
    );

    // to turn off waitForSync!
    trx.addHint(transaction::Hints::Hint::RECOVERY);
    // smaller batch sizes should work better here
#if VPACK_DUMP
    trx.state()->options().intermediateCommitCount = 128;
    trx.state()->options().intermediateCommitSize = 16 * 1024 * 1024;
#endif

    Result res = trx.begin();

    if (!res.ok()) {
      return Result(res.errorNumber(), std::string("unable to start transaction: ") + res.errorMessage());
    }

    trx.pinData(coll->id());  // will throw when it fails
    res = parseCollectionDump(trx, coll, response.get(), markersProcessed);

    if (res.fail()) {
      return res;
    }

    res = trx.commit();

#if VPACK_DUMP
    sumApplyTime += (TRI_microtime() - applyTimeNow);
#endif

    std::string const progress2 =
        "fetched master collection dump for collection '" + coll->name() +
        "', type: " + typeString + ", id " + leaderColl + ", batch " +
        StringUtils::itoa(batch) +
        ", markers processed: " + StringUtils::itoa(markersProcessed) +
        ", bytes received: " + StringUtils::itoa(bytesReceived);

    setProgress(progress2);

    if (!res.ok()) {
      return res;
    }

    if (!checkMore || fromTick == 0) {
#if VPACK_DUMP
      LOG_TOPIC(ERR, Logger::FIXME) << "Collection " << leaderColl << ", local: " << coll->name();
      LOG_TOPIC(ERR, Logger::FIXME) << "Fetching dump: " << sumFetchTime;
      LOG_TOPIC(ERR, Logger::FIXME) << "Applying dump: " << sumApplyTime;
      LOG_TOPIC(ERR, Logger::FIXME) << "Total: " << (TRI_microtime() - nowTotal);
      LOG_TOPIC(ERR, Logger::FIXME) << "Markers processed: " << markersProcessed;
#endif
      // done
      return Result();
    }

    // increase chunk size for next fetch
    if (chunkSize < MaxChunkSize) {
      chunkSize = static_cast<uint64_t>(chunkSize * 1.25);

      if (chunkSize > MaxChunkSize) {
        chunkSize = MaxChunkSize;
      }
    }

    batch++;
  }

  TRI_ASSERT(false);
  return Result(TRI_ERROR_INTERNAL);
}

/// @brief incrementally fetch data from a collection
Result DatabaseInitialSyncer::fetchCollectionSync(arangodb::LogicalCollection* coll,
                                                  std::string const& leaderColl,
                                                  TRI_voc_tick_t maxTick) {
  sendExtendBatch();
  sendExtendBarrier();

  std::string const baseUrl = ReplicationUrl + "/keys";
  std::string url = baseUrl + "/keys" +
                    "?collection=" + StringUtils::urlEncode(leaderColl) +
                    "&to=" + std::to_string(maxTick) +
                    "&serverId=" + _localServerIdString +
                    "&batchId=" + std::to_string(_batchId);

  std::string progress = "fetching collection keys for collection '" +
                         coll->name() + "' from " + url;
  setProgress(progress);

  // send an initial async request to collect the collection keys on the other
  // side
  // sending this request in a blocking fashion may require very long to
  // complete,
  // so we're sending the x-arango-async header here
  auto headers = createHeaders();
  headers[StaticStrings::Async] = "store";
  std::unique_ptr<SimpleHttpResult> response(
      _client->retryRequest(rest::RequestType::POST, url, nullptr, 0, headers));

  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url);
  }

  bool found = false;
  std::string jobId = response->getHeaderField(StaticStrings::AsyncId, found);

  if (!found) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ")
                  + _masterInfo._endpoint + url + ": could not find 'X-Arango-Async' header");
  }

  double const startTime = TRI_microtime();

  while (true) {
    sendExtendBatch();
    sendExtendBarrier();

    std::string const jobUrl = "/_api/job/" + jobId;
    response.reset(
        _client->request(rest::RequestType::PUT, jobUrl, nullptr, 0));

    if (response != nullptr && response->isComplete()) {
      if (response->hasHeaderField("x-arango-async-id")) {
        // job is done, got the actual response
        break;
      }
      if (response->getHttpReturnCode() == 404) {
        // unknown job, we can abort
        return Result(TRI_ERROR_REPLICATION_NO_RESPONSE, std::string("job not found on master at ") + _masterInfo._endpoint);
      }
    }

    double waitTime = TRI_microtime() - startTime;

    if (static_cast<uint64_t>(waitTime * 1000.0 * 1000.0) >=
        _configuration._initialSyncMaxWaitTime) {
      return Result(TRI_ERROR_REPLICATION_NO_RESPONSE, std::string("timed out waiting for response from master at ") + _masterInfo._endpoint);
    }

    std::chrono::milliseconds sleepTime;
    if (waitTime < 5.0) {
      sleepTime = std::chrono::milliseconds(250);
    } else if (waitTime < 20.0) {
      sleepTime = std::chrono::milliseconds(500);
    } else if (waitTime < 60.0) {
      sleepTime = std::chrono::seconds(1);
    } else {
      sleepTime = std::chrono::seconds(2);
    }

    if (isAborted()) {
      return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }
    std::this_thread::sleep_for(sleepTime);
  }
  
  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url);
  }

  VPackBuilder builder;
  Result r = parseResponse(builder, response.get());

  if (r.fail()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                  _masterInfo._endpoint + url + ": " + r.errorMessage());
  }

  VPackSlice const slice = builder.slice();
  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                  _masterInfo._endpoint + url + ": response is no object");
  }

  VPackSlice const keysId = slice.get("id");

  if (!keysId.isString()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                  _masterInfo._endpoint + url + ": response does not contain valid 'id' attribute");
  }

  auto shutdown = [&]() -> void {
    url = baseUrl + "/" + keysId.copyString();
    std::string progress =
        "deleting remote collection keys object for collection '" +
        coll->name() + "' from " + url;
    setProgress(progress);

    // now delete the keys we ordered
    std::unique_ptr<SimpleHttpResult> response(
        _client->retryRequest(rest::RequestType::DELETE_REQ, url, nullptr, 0));
  };

  TRI_DEFER(shutdown());

  VPackSlice const count = slice.get("count");

  if (!count.isNumber()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                  _masterInfo._endpoint + url + ": response does not contain valid 'count' attribute");
  }

  if (count.getNumber<size_t>() <= 0) {
    // remote collection has no documents. now truncate our local collection
    SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(vocbase()),
      coll,
      AccessMode::Type::EXCLUSIVE
    );
    Result res = trx.begin();

    if (!res.ok()) {
      return Result(res.errorNumber(), std::string("unable to start transaction (") + std::string(__FILE__) + std::string(":") + std::to_string(__LINE__) + std::string("): ") + res.errorMessage());
    }

    OperationOptions options;

    if (!_leaderId.empty()) {
      options.isSynchronousReplicationFrom = _leaderId;
    }

    OperationResult opRes = trx.truncate(coll->name(), options);

    if (opRes.fail()) {
      return Result(opRes.errorNumber(), std::string("unable to truncate collection '") + coll->name()
                    + "': " + TRI_errno_string(opRes.errorNumber()));
    }

    return trx.finish(opRes.result);
  }

  // now we can fetch the complete chunk information from the master
  try {
    return EngineSelectorFeature::ENGINE->handleSyncKeys(
      *this, *coll, keysId.copyString()
    );
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }
}

/// @brief incrementally fetch data from a collection
/// @brief changes the properties of a collection, based on the VelocyPack
/// provided
Result DatabaseInitialSyncer::changeCollection(arangodb::LogicalCollection* col,
                                               VPackSlice const& slice) {
  arangodb::CollectionGuard guard(&vocbase(), col->id());
  bool doSync =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database")
          ->forceSyncProperties();

  return guard.collection()->updateProperties(slice, doSync);
}

/// @brief determine the number of documents in a collection
int64_t DatabaseInitialSyncer::getSize(arangodb::LogicalCollection* col) {
  SingleCollectionTransaction trx(
    transaction::StandaloneContext::Create(vocbase()),
    col,
    AccessMode::Type::READ
  );
  Result res = trx.begin();

  if (res.fail()) {
    return -1;
  }

  OperationResult result = trx.count(col->name(), true);

  if (result.result.fail()) {
    return -1;
  }

  VPackSlice s = result.slice();

  if (!s.isNumber()) {
    return -1;
  }

  return s.getNumber<int64_t>();
}

/// @brief handle the information about a collection
Result DatabaseInitialSyncer::handleCollection(VPackSlice const& parameters,
                                               VPackSlice const& indexes, bool incremental,
                                               sync_phase_e phase) {
  if (isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  if (!parameters.isObject() || !indexes.isArray()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE);
  }

  sendExtendBatch();
  sendExtendBarrier();

  std::string const masterName =
      VelocyPackHelper::getStringValue(parameters, "name", "");

  TRI_voc_cid_t const masterCid = VelocyPackHelper::extractIdValue(parameters);

  if (masterCid == 0) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "collection id is missing in response");
  }
  
  std::string const masterUuid =
      VelocyPackHelper::getStringValue(parameters, "globallyUniqueId", "");

  VPackSlice const type = parameters.get("type");

  if (!type.isNumber()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, "collection type is missing in response");
  }

  std::string const typeString =
      (type.getNumber<int>() == 3 ? "edge" : "document");

  std::string const collectionMsg = "collection '" + masterName + "', type " +
                                    typeString + ", id " +
                                    StringUtils::itoa(masterCid);

  // phase handling
  if (phase == PHASE_VALIDATE) {
    // validation phase just returns ok if we got here (aborts above if data is
    // invalid)
    _processedCollections.emplace(masterCid, masterName);

    return Result();
  }

  // drop and re-create collections locally
  // -------------------------------------------------------------------------------------

  if (phase == PHASE_DROP_CREATE) {
    auto* col = resolveCollection(vocbase(), parameters).get();

    if (col == nullptr) {
      // not found...
      col = vocbase().lookupCollection(masterName).get();

      if (col != nullptr
          && (col->name() != masterName
              || (!masterUuid.empty() && col->guid() != masterUuid))) {
        // found another collection with the same name locally.
        // in this case we must drop it because we will run into duplicate
        // name conflicts otherwise
        try {
          auto res =
            vocbase().dropCollection(col->id(), true, -1.0).errorNumber();

          if (res == TRI_ERROR_NO_ERROR) {
            col = nullptr;
          }
        } catch (...) {
        }
      }
    }

    if (col != nullptr) {
      if (!incremental) {
        // first look up the collection
        if (col != nullptr) {
          bool truncate = false;

          if (col->name() == TRI_COL_NAME_USERS) {
            // better not throw away the _users collection. otherwise it is gone
            // and this may be a problem if the
            // server crashes in-between.
            truncate = true;
          }

          if (truncate) {
            // system collection
            setProgress("truncating " + collectionMsg);

            SingleCollectionTransaction trx(
              transaction::StandaloneContext::Create(vocbase()),
              col,
              AccessMode::Type::EXCLUSIVE
            );
            Result res = trx.begin();

            if (!res.ok()) {
              return Result(res.errorNumber(), std::string("unable to truncate ") + collectionMsg + ": " + res.errorMessage());
            }

            OperationOptions options;

            if (!_leaderId.empty()) {
              options.isSynchronousReplicationFrom = _leaderId;
            }

            OperationResult opRes = trx.truncate(col->name(), options);

            if (opRes.fail()) {
              return Result(opRes.errorNumber(), std::string("unable to truncate ") + collectionMsg + ": " + TRI_errno_string(opRes.errorNumber()));
            }

            res = trx.finish(opRes.result);

            if (!res.ok()) {
              return Result(res.errorNumber(), std::string("unable to truncate ") + collectionMsg + ": " + res.errorMessage());
            }
          } else {
            // drop a regular collection
            if (_configuration._skipCreateDrop) {
              setProgress("dropping " + collectionMsg + " skipped because of configuration");
              return Result();
            }
            setProgress("dropping " + collectionMsg);

            auto res =
              vocbase().dropCollection(col->id(), true, -1.0).errorNumber();

            if (res != TRI_ERROR_NO_ERROR) {
              return Result(res, std::string("unable to drop ") + collectionMsg + ": " + TRI_errno_string(res));
            }
          }
        }
      } else {
        // incremental case
        TRI_ASSERT(incremental);

        // collection is already present
        std::string const progress =
            "checking/changing parameters of " + collectionMsg;
        setProgress(progress);
        return changeCollection(col, parameters);
      }
    }

    std::string progress = "creating " + collectionMsg;

    if (_configuration._skipCreateDrop) {
      progress += " skipped because of configuration";
      setProgress(progress);
      return Result();
    }

    setProgress(progress);

    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Dump is creating collection "
      << parameters.toJson();

    auto r = createCollection(vocbase(), parameters, &col);

    if (r.fail()) {
      return Result(r.errorNumber(), std::string("unable to create ") +
                    collectionMsg + ": " + TRI_errno_string(r.errorNumber()) +
                    ". Collection info " + parameters.toJson());
    }

    return r;
  }

  // sync collection data
  // -------------------------------------------------------------------------------------

  else if (phase == PHASE_DUMP) {
    std::string const progress = "dumping data for " + collectionMsg;
    setProgress(progress);

    auto* col = resolveCollection(vocbase(), parameters).get();

    if (col == nullptr) {
      return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, std::string("cannot dump: ") +
                    collectionMsg + " not found on slave. Collection info " + parameters.toJson());
    }

    Result res;
    std::string const& masterColl = !masterUuid.empty() ? masterUuid : StringUtils::itoa(masterCid);

    if (incremental && getSize(col) > 0) {
      res = fetchCollectionSync(col, masterColl, _masterInfo._lastLogTick);
    } else {
      res = fetchCollectionDump(col, masterColl, _masterInfo._lastLogTick);
    }

    if (!res.ok()) {
      return res;
    }

    if (masterName == TRI_COL_NAME_USERS) {
      reloadUsers();
    }

    // now create indexes
    TRI_ASSERT(indexes.isArray());
    VPackValueLength const n = indexes.length();

    if (n > 0) {
      sendExtendBatch();
      sendExtendBarrier();

      std::string const progress = "creating " + std::to_string(n) +
                                   " index(es) for " + collectionMsg;
      setProgress(progress);

      try {
        SingleCollectionTransaction trx(
          transaction::StandaloneContext::Create(vocbase()),
          col,
          AccessMode::Type::EXCLUSIVE
        );

        res = trx.begin();

        if (!res.ok()) {
          return Result(res.errorNumber(), std::string("unable to start transaction: ") + res.errorMessage());
        }

        trx.pinData(col->id()); // will throw when it fails

        LogicalCollection* document = trx.documentCollection();
        TRI_ASSERT(document != nullptr);
        auto physical = document->getPhysical();
        TRI_ASSERT(physical != nullptr);

        for (auto const& idxDef : VPackArrayIterator(indexes)) {
          std::shared_ptr<arangodb::Index> idx;

          if (idxDef.isObject()) {
            VPackSlice const type = idxDef.get("type");
            if (type.isString()) {
              std::string const progress = "creating index of type " +
                                            type.copyString() + " for " +
                                            collectionMsg;
              setProgress(progress);
            }
          }

          res = physical->restoreIndex(&trx, idxDef, idx);

          if (!res.ok()) {
            res.reset(res.errorNumber(), std::string("could not create index: ") + res.errorMessage());
            break;
          }
        }

        if (res.ok()) {
          res = trx.commit();
        }
      } catch (arangodb::basics::Exception const& ex) {
        return Result(ex.code(), ex.what());
      } catch (std::exception const& ex) {
        return Result(TRI_ERROR_INTERNAL, ex.what());
      } catch (...) {
        return Result(TRI_ERROR_INTERNAL);
      }
    }

    return res;
  }

  // we won't get here
  TRI_ASSERT(false);
  return Result(TRI_ERROR_INTERNAL);
}

Result DatabaseInitialSyncer::fetchInventory(VPackBuilder& builder) {
  std::string url = ReplicationUrl + "/inventory?serverId=" + _localServerIdString +
  "&batchId=" + std::to_string(_batchId);
  if (_configuration._includeSystem) {
    url += "&includeSystem=true";
  }
  
  // send request
  std::string const progress = "fetching master inventory from " + url;
  setProgress(progress);
  
  std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(rest::RequestType::GET, url, nullptr, 0));
  
  if (hasFailed(response.get())) {
    sendFinishBatch();
    return buildHttpError(response.get(), url);
  }
  
  Result r = parseResponse(builder, response.get());
  
  if (r.fail()) {
    return Result(r.errorNumber(), std::string("got invalid response from master at ") +
                  _masterInfo._endpoint + url + ": invalid response type for initial data. expecting array");
  }
  
  VPackSlice const slice = builder.slice();
  if (!slice.isObject()) {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "client: DatabaseInitialSyncer::run - inventoryResponse is not an object";
   
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") +
                  _masterInfo._endpoint + url + ": invalid JSON");
  }

  return Result();
}

/// @brief handle the inventory response of the master
Result DatabaseInitialSyncer::handleLeaderCollections(VPackSlice const& collSlice,
                                                      bool incremental) {
  TRI_ASSERT(collSlice.isArray());

  std::vector<std::pair<VPackSlice, VPackSlice>> collections;
  for (VPackSlice it : VPackArrayIterator(collSlice)) {
    if (!it.isObject()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "collection declaration is invalid in response");
    }

    VPackSlice const parameters = it.get("parameters");

    if (!parameters.isObject()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "collection parameters declaration is invalid in response");
    }

    VPackSlice const indexes = it.get("indexes");

    if (!indexes.isArray()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "collection indexes declaration is invalid in response");
    }

    std::string const masterName =
        VelocyPackHelper::getStringValue(parameters, "name", "");

    if (masterName.empty()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "collection name is missing in response");
    }

    if (TRI_ExcludeCollectionReplication(masterName, _configuration._includeSystem)) {
      continue;
    }

    if (VelocyPackHelper::getBooleanValue(parameters, "deleted", false)) {
      // we don't care about deleted collections
      continue;
    }

    if (!_configuration._restrictType.empty()) {
      auto const it = _configuration._restrictCollections.find(masterName);
      bool found = (it != _configuration._restrictCollections.end());

      if (_configuration._restrictType == "include" && !found) {
        // collection should not be included
        continue;
      } else if (_configuration._restrictType == "exclude" && found) {
        // collection should be excluded
        continue;
      }
    }

    collections.emplace_back(parameters, indexes);
  }

  // STEP 1: validate collection declarations from master
  // ----------------------------------------------------------------------------------
  
  // STEP 2: drop and re-create collections locally if they are also present on
  // the master
  //  ------------------------------------------------------------------------------------
  
  // STEP 3: sync collection data from master and create initial indexes
  // ----------------------------------------------------------------------------------

  // iterate over all collections from the master...
  std::array<sync_phase_e, 3> phases{{ PHASE_VALIDATE, PHASE_DROP_CREATE, PHASE_DUMP }};
  for (auto const& phase : phases) {
    Result r = iterateCollections(collections, incremental, phase);

    if (r.fail()) {
      return r;
    }
  }

  return Result();
}

/// @brief iterate over all collections from an array and apply an action
Result DatabaseInitialSyncer::iterateCollections(
    std::vector<std::pair<VPackSlice, VPackSlice>> const& collections,
    bool incremental, sync_phase_e phase) {
  std::string phaseMsg("starting phase " + translatePhase(phase) + " with " +
                       std::to_string(collections.size()) + " collections");
  setProgress(phaseMsg);

  for (auto const& collection : collections) {
    VPackSlice const parameters = collection.first;
    VPackSlice const indexes = collection.second;

    Result res = handleCollection(parameters, indexes, incremental, phase);

    if (res.fail()) {
      return res;
    }
  }

  // all ok
  return Result();
}

std::unordered_map<std::string, std::string> DatabaseInitialSyncer::createHeaders() const {
  return { {StaticStrings::ClusterCommSource, ServerState::instance()->getId()} };
}
