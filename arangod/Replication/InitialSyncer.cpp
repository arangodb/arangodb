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
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"
#include "Indexes/PrimaryIndex.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/CollectionGuard.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/Ditch.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief performs a binary search for the given key in the markers vector
////////////////////////////////////////////////////////////////////////////////

static bool BinarySearch(std::vector<void const*> const& markers,
                         std::string const& key, size_t& position) {
  TRI_ASSERT(!markers.empty());

  size_t l = 0;
  size_t r = markers.size() - 1;

  while (true) {
    // determine midpoint
    position = l + ((r - l) / 2);

    TRI_ASSERT(position < markers.size());
    VPackSlice const otherSlice(reinterpret_cast<char const*>(markers.at(position)));
    VPackSlice const otherKey = otherSlice.get(StaticStrings::KeyString);

    int res = key.compare(otherKey.copyString());

    if (res == 0) {
      return true;
    }

    if (res < 0) {
      if (position == 0) {
        return false;
      }
      r = position - 1;
    } else {
      l = position + 1;
    }

    if (r < l) {
      return false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a key range in the markers vector
////////////////////////////////////////////////////////////////////////////////

static bool FindRange(std::vector<void const*> const& markers,
                      std::string const& lower, std::string const& upper,
                      size_t& lowerPos, size_t& upperPos) {
  bool found = false;

  if (!markers.empty()) {
    found = BinarySearch(markers, lower, lowerPos);

    if (found) {
      found = BinarySearch(markers, upper, upperPos);
    }
  }

  return found;
}

size_t const InitialSyncer::MaxChunkSize = 10 * 1024 * 1024;

InitialSyncer::InitialSyncer(
    TRI_vocbase_t* vocbase,
    TRI_replication_applier_configuration_t const* configuration,
    std::unordered_map<std::string, bool> const& restrictCollections,
    std::string const& restrictType, bool verbose)
    : Syncer(vocbase, configuration),
      _progress("not started"),
      _restrictCollections(restrictCollections),
      _restrictType(restrictType),
      _processedCollections(),
      _batchId(0),
      _batchUpdateTime(0),
      _batchTtl(180),
      _includeSystem(false),
      _chunkSize(configuration->_chunkSize),
      _verbose(verbose),
      _hasFlushed(false) {
  if (_chunkSize == 0) {
    _chunkSize = (uint64_t)2 * 1024 * 1024;  // 2 mb
  } else if (_chunkSize < 128 * 1024) {
    _chunkSize = 128 * 1024;
  }

  _includeSystem = configuration->_includeSystem;
}

InitialSyncer::~InitialSyncer() {
  try {
    sendFinishBatch();
  }
  catch (...) {
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run method, performs a full synchronization
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::run(std::string& errorMsg, bool incremental) {
  if (_client == nullptr || _connection == nullptr || _endpoint == nullptr) {
    errorMsg = "invalid endpoint";

    return TRI_ERROR_INTERNAL;
  }

  int res = _vocbase->replicationApplier()->preventStart();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_DEFER(_vocbase->replicationApplier()->allowStart());

  try {
    setProgress("fetching master state");

    res = getMasterState(errorMsg);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (_masterInfo._majorVersion == 1 ||
        (_masterInfo._majorVersion == 2 && _masterInfo._minorVersion <= 6)) {
      LOG_TOPIC(WARN, Logger::REPLICATION) << "incremental replication is not supported with a master < ArangoDB 2.7";
      incremental = false;
    }
      
    if (incremental) {
      res = sendFlush(errorMsg);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }

    // create a WAL logfile barrier that prevents WAL logfile collection
    sendCreateBarrier(errorMsg, _masterInfo._lastLogTick);

    res = sendStartBatch(errorMsg);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    std::string url = BaseUrl + "/inventory?serverId=" + _localServerIdString;
    if (_includeSystem) {
      url += "&includeSystem=true";
    }

    // send request
    std::string const progress = "fetching master inventory from " + url;
    setProgress(progress);

    std::unique_ptr<SimpleHttpResult> response(
        _client->retryRequest(rest::RequestType::GET, url, nullptr, 0));

    if (response == nullptr || !response->isComplete()) {
      errorMsg = "could not connect to master at " +
                 _masterInfo._endpoint + ": " +
                 _client->getErrorMessage();

      sendFinishBatch();

      return TRI_ERROR_REPLICATION_NO_RESPONSE;
    }

    TRI_ASSERT(response != nullptr);

    if (response->wasHttpError()) {
      res = TRI_ERROR_REPLICATION_MASTER_ERROR;

      errorMsg = "got invalid response from master at " +
                 _masterInfo._endpoint + ": HTTP " +
                 StringUtils::itoa(response->getHttpReturnCode()) + ": " +
                 response->getHttpReturnMessage();
    } else {
      auto builder = std::make_shared<VPackBuilder>();
      res = parseResponse(builder, response.get());

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "got invalid response from master at " +
                   std::string(_masterInfo._endpoint) +
                   ": invalid response type for initial data. expecting array";

        return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }

      VPackSlice const slice = builder->slice();
      if (!slice.isObject()) {
        res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;

        errorMsg = "got invalid response from master at " +
                   _masterInfo._endpoint + ": invalid JSON";
      }
      else {
        res = handleInventoryResponse(slice, incremental, errorMsg);
      }
    }

    sendFinishBatch();

    if (res != TRI_ERROR_NO_ERROR && errorMsg.empty()) {
      errorMsg = TRI_errno_string(res);
    }

    return res;
  } catch (arangodb::basics::Exception const& ex) {
    sendFinishBatch();
    errorMsg = ex.what();
    return ex.code();
  } catch (std::exception const& ex) {
    sendFinishBatch();
    errorMsg = ex.what();
    return TRI_ERROR_INTERNAL;
  } catch (...) {
    sendFinishBatch();
    errorMsg = "an unknown exception occurred";
    return TRI_ERROR_NO_ERROR;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send a WAL flush command
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::sendFlush(std::string& errorMsg) {
  std::string const url = "/_admin/wal/flush";
  std::string const body =
      "{\"waitForSync\":true,\"waitForCollector\":true,"
      "\"waitForCollectorQueue\":true}";

  // send request
  std::string const progress = "send WAL flush command to url " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
      rest::RequestType::PUT, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "could not connect to master at " +
               _masterInfo._endpoint + ": " +
               _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    int res = TRI_ERROR_REPLICATION_MASTER_ERROR;

    errorMsg = "got invalid response from master at " +
               _masterInfo._endpoint + ": HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();

    return res;
  }

  _hasFlushed = true;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send a "start batch" command
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::sendStartBatch(std::string& errorMsg) {
  _batchId = 0;

  std::string const url = BaseUrl + "/batch";
  std::string const body = "{\"ttl\":" + StringUtils::itoa(_batchTtl) + "}";

  // send request
  std::string const progress = "send batch start command to url " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
      rest::RequestType::POST, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "could not connect to master at " +
               _masterInfo._endpoint + ": " +
               _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    return TRI_ERROR_REPLICATION_MASTER_ERROR;
  } 
    
  auto builder = std::make_shared<VPackBuilder>();
  int res = parseResponse(builder, response.get());

  if (res != TRI_ERROR_NO_ERROR) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  } 
      
  VPackSlice const slice = builder->slice();
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

////////////////////////////////////////////////////////////////////////////////
/// @brief send an "extend batch" command
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::sendExtendBatch() {
  if (_batchId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  double now = TRI_microtime();

  if (now <= _batchUpdateTime + _batchTtl - 60.0) {
    // no need to extend the batch yet
    return TRI_ERROR_NO_ERROR;
  }

  std::string const url = BaseUrl + "/batch/" + StringUtils::itoa(_batchId);
  std::string const body = "{\"ttl\":" + StringUtils::itoa(_batchTtl) + "}";

  // send request
  std::string const progress = "send batch extend command to url " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(_client->request(
      rest::RequestType::PUT, url, body.c_str(), body.size()));

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

////////////////////////////////////////////////////////////////////////////////
/// @brief send a "finish batch" command
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::sendFinishBatch() {
  if (_batchId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  try {
    std::string const url = BaseUrl + "/batch/" + StringUtils::itoa(_batchId);

    // send request
    std::string const progress = "send batch finish command to url " + url;
    setProgress(progress);

    std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
        rest::RequestType::DELETE_REQ, url, nullptr, 0));

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

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the initial synchronization should be aborted
////////////////////////////////////////////////////////////////////////////////

bool InitialSyncer::checkAborted() {
  if (_vocbase->replicationApplier() != nullptr &&
      _vocbase->replicationApplier()->stopInitialSynchronization()) {
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from a collection dump
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::applyCollectionDump(
    arangodb::Transaction& trx, std::string const& collectionName,
    SimpleHttpResult* response, uint64_t& markersProcessed,
    std::string& errorMsg) {
  std::string const invalidMsg = "received invalid JSON data for collection " +
                                 collectionName;

  StringBuffer& data = response->getBody();
  char const* p = data.begin();
  char const* end = p + data.length();

  // buffer must end with a NUL byte
  TRI_ASSERT(*end == '\0');
    
  auto builder = std::make_shared<VPackBuilder>();

  while (p < end) {
    char const* q = strchr(p, '\n');

    if (q == nullptr) {
      q = end;
    }

    if (q - p < 2) {
      // we are done
      return TRI_ERROR_NO_ERROR;
    }

    TRI_ASSERT(q <= end);

    try {
      builder->clear();
      VPackParser parser(builder);
      parser.parse(p, static_cast<size_t>(q - p));
    }
    catch (...) {
      // TODO: improve error reporting
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    p = q + 1;

    VPackSlice const slice = builder->slice();

    if (!slice.isObject()) {
      errorMsg = invalidMsg;

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    TRI_replication_operation_e type = REPLICATION_INVALID;
    std::string key;
    std::string rev;
    VPackSlice doc;

    for (auto const& it : VPackObjectIterator(slice)) {
      std::string const attributeName(it.key.copyString());

      if (attributeName == "type") {
        if (it.value.isNumber()) {
          type = static_cast<TRI_replication_operation_e>(it.value.getNumber<int>());
        }
      }

      else if (attributeName == "data") {
        if (it.value.isObject()) {
          doc = it.value;
        }
      }
    }
      
    if (!doc.isNone()) {
      VPackSlice value = doc.get(StaticStrings::KeyString);
      if (value.isString()) {
        key = value.copyString();
      }
      
      value = doc.get(StaticStrings::RevString);
      
      if (value.isString()) {
        rev = value.copyString();
      }
    } 

    // key must not be empty, but doc can be empty
    if (key.empty()) {
      errorMsg = invalidMsg;

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    ++markersProcessed;

    VPackBuilder oldBuilder;
    oldBuilder.openObject();
    oldBuilder.add(StaticStrings::KeyString, VPackValue(key));
    if (!rev.empty()) {
      oldBuilder.add(StaticStrings::RevString, VPackValue(rev));
    }
    oldBuilder.close();

    VPackSlice const old = oldBuilder.slice();

    int res = applyCollectionDumpMarker(trx, collectionName, type, old, doc, errorMsg);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  // reached the end
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief incrementally fetch data from a collection
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleCollectionDump(
    arangodb::LogicalCollection* col, std::string const& cid,
    std::string const& collectionName, TRI_voc_tick_t maxTick,
    std::string& errorMsg) {
  std::string appendix;

  if (_hasFlushed) {
    appendix = "&flush=false";
  } else {
    // only flush WAL once
    appendix = "&flush=true&flushWait=5";
    _hasFlushed = true;
  }

  uint64_t chunkSize = _chunkSize;

  std::string const baseUrl = BaseUrl + "/dump?collection=" + cid + appendix;

  TRI_voc_tick_t fromTick = 0;
  int batch = 1;
  uint64_t bytesReceived = 0;
  uint64_t markersProcessed = 0;

  while (true) {
    if (checkAborted()) {
      return TRI_ERROR_REPLICATION_APPLIER_STOPPED;
    }

    sendExtendBatch();
    sendExtendBarrier();

    std::string url = baseUrl + "&from=" + StringUtils::itoa(fromTick);

    if (maxTick > 0) {
      url += "&to=" + StringUtils::itoa(maxTick + 1);
    }

    url += "&serverId=" + _localServerIdString;
    url += "&chunkSize=" + StringUtils::itoa(chunkSize);
    url += "&includeSystem=" + std::string(_includeSystem ? "true" : "false");

    std::string const typeString =
        (col->type() == TRI_COL_TYPE_EDGE
             ? "edge"
             : "document");

    // send request
    std::string const progress =
        "fetching master collection dump for collection '" + collectionName +
        "', type: " + typeString + ", id " + cid + ", batch " +
        StringUtils::itoa(batch) + ", markers processed: " +
        StringUtils::itoa(markersProcessed) + ", bytes received: " +
        StringUtils::itoa(bytesReceived);

    setProgress(progress);

    // use async mode for first batch
    std::unordered_map<std::string, std::string> headers;
    if (batch == 1) {
      headers["X-Arango-Async"] = "store";
    }
    std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
        rest::RequestType::GET, url, nullptr, 0, headers));

    if (response == nullptr || !response->isComplete()) {
      errorMsg = "could not connect to master at " +
                 _masterInfo._endpoint + ": " +
                 _client->getErrorMessage();

      return TRI_ERROR_REPLICATION_NO_RESPONSE;
    }

    TRI_ASSERT(response != nullptr);

    if (response->wasHttpError()) {
      errorMsg = "got invalid response from master at " +
                 _masterInfo._endpoint + ": HTTP " +
                 StringUtils::itoa(response->getHttpReturnCode()) + ": " +
                 response->getHttpReturnMessage();

      return TRI_ERROR_REPLICATION_MASTER_ERROR;
    }

    // use async mode for first batch
    if (batch == 1) {
      bool found = false;
      std::string jobId = response->getHeaderField(StaticStrings::AsyncId, found);

      if (!found) {
        errorMsg = "got invalid response from master at " +
                   _masterInfo._endpoint +
                   ": could not find 'X-Arango-Async' header";
        return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }

      double const startTime = TRI_microtime();

      // wait until we get a responsable response
      while (true) {
        sendExtendBatch();
        sendExtendBarrier();

        std::string const jobUrl = "/_api/job/" + jobId;
        response.reset(_client->request(rest::RequestType::PUT, jobUrl,
                                        nullptr, 0));

        if (response != nullptr && response->isComplete()) {
          if (response->hasHeaderField("x-arango-async-id")) {
            // got the actual response
            break;
          }
          if (response->getHttpReturnCode() == 404) {
            // unknown job, we can abort
            errorMsg = "job not found on master at " + _masterInfo._endpoint;
            return TRI_ERROR_REPLICATION_NO_RESPONSE;
          }
        }

        double waitTime = TRI_microtime() - startTime;

        if (static_cast<uint64_t>(waitTime * 1000.0 * 1000.0) >=
            _configuration._initialSyncMaxWaitTime) {
          errorMsg = "timed out waiting for response from master at " +
                     _masterInfo._endpoint;
          return TRI_ERROR_REPLICATION_NO_RESPONSE;
        }

        double sleepTime;
        if (waitTime < 5.0) {
          sleepTime = 0.25;
        } else if (waitTime < 20.0) {
          sleepTime = 0.5;
        } else if (waitTime < 60.0) {
          sleepTime = 1.0;
        } else {
          sleepTime = 2.0;
        }

        if (checkAborted()) {
          return TRI_ERROR_REPLICATION_APPLIER_STOPPED;
        }
        this->sleep(static_cast<uint64_t>(sleepTime * 1000.0 * 1000.0));
      }
      // fallthrough here in case everything went well
    }

    if (response->hasContentLength()) {
      bytesReceived += response->getContentLength();
    }

    int res = TRI_ERROR_NO_ERROR;  // Just to please the compiler
    bool checkMore = false;
    bool found;
    TRI_voc_tick_t tick;

    std::string header =
        response->getHeaderField(TRI_REPLICATION_HEADER_CHECKMORE, found);
    if (found) {
      checkMore = StringUtils::boolean(header);
      res = TRI_ERROR_NO_ERROR;

      if (checkMore) {
        header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTINCLUDED,
                                          found);
        if (found) {
          tick = StringUtils::uint64(header);

          if (tick > fromTick) {
            fromTick = tick;
          } else {
            // we got the same tick again, this indicates we're at the end
            checkMore = false;
          }
        }
      }
    }

    if (!found) {
      errorMsg = "got invalid response from master at " +
                 _masterInfo._endpoint +
                 ": required header is missing";
      res = TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    if (res == TRI_ERROR_NO_ERROR) {
      SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), col->cid(), TRI_TRANSACTION_WRITE);
      
      res = trx.begin();

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "unable to start transaction: " + std::string(TRI_errno_string(res));

        return res;
      }
     
      trx.orderDitch(col->cid()); // will throw when it fails

      if (res == TRI_ERROR_NO_ERROR) {
        res = applyCollectionDump(trx, collectionName, response.get(), markersProcessed, errorMsg);
        res = trx.finish(res);
      }
    }

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (!checkMore || fromTick == 0) {
      // done
      return res;
    }

    // increase chunk size for next fetch
    if (chunkSize < MaxChunkSize) {
      chunkSize = static_cast<uint64_t>(chunkSize * 1.5);
      if (chunkSize > MaxChunkSize) {
        chunkSize = MaxChunkSize;
      }
    }

    batch++;
  }

  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief incrementally fetch data from a collection
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleCollectionSync(
    arangodb::LogicalCollection* col, std::string const& cid, 
    std::string const& collectionName, TRI_voc_tick_t maxTick,
    std::string& errorMsg) {
  sendExtendBatch();
  sendExtendBarrier();

  std::string const baseUrl = BaseUrl + "/keys";
  std::string url =
      baseUrl + "?collection=" + cid + "&to=" + std::to_string(maxTick);

  std::string progress = "fetching collection keys for collection '" +
                         collectionName + "' from " + url;
  setProgress(progress);

  // send an initial async request to collect the collection keys on the other
  // side
  // sending this request in a blocking fashion may require very long to
  // complete,
  // so we're sending the x-arango-async header here
  std::unordered_map<std::string, std::string> headers;
  headers["X-Arango-Async"] = "store";
  std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
      rest::RequestType::POST, url, nullptr, 0, headers));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "could not connect to master at " +
               _masterInfo._endpoint + ": " +
               _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from master at " +
               _masterInfo._endpoint + ": HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();

    return TRI_ERROR_REPLICATION_MASTER_ERROR;
  }

  bool found = false;
  std::string jobId = response->getHeaderField(StaticStrings::AsyncId, found);

  if (!found) {
    errorMsg = "got invalid response from master at " +
               _masterInfo._endpoint +
               ": could not find 'X-Arango-Async' header";
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
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
        errorMsg = "job not found on master at " + _masterInfo._endpoint;
        return TRI_ERROR_REPLICATION_NO_RESPONSE;
      }
    }

    double waitTime = TRI_microtime() - startTime;

    if (static_cast<uint64_t>(waitTime * 1000.0 * 1000.0) >=
        _configuration._initialSyncMaxWaitTime) {
      errorMsg = "timed out waiting for response from master at " +
                 _masterInfo._endpoint;
      return TRI_ERROR_REPLICATION_NO_RESPONSE;
    }

    double sleepTime;
    if (waitTime < 5.0) {
      sleepTime = 0.25;
    } else if (waitTime < 20.0) {
      sleepTime = 0.5;
    } else if (waitTime < 60.0) {
      sleepTime = 1.0;
    } else {
      sleepTime = 2.0;
    }

    if (checkAborted()) {
      return TRI_ERROR_REPLICATION_APPLIER_STOPPED;
    }
    this->sleep(static_cast<uint64_t>(sleepTime * 1000.0 * 1000.0));
  }

  auto builder = std::make_shared<VPackBuilder>();
  int res = parseResponse(builder, response.get());

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg = "got invalid response from master at " +
               std::string(_masterInfo._endpoint) +
               ": response is no object";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  VPackSlice const slice = builder->slice();
  if (!slice.isObject()) {
    errorMsg = "got invalid response from master at " +
               _masterInfo._endpoint + ": response is no object";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  VPackSlice const id = slice.get("id");

  if (!id.isString()) {
    errorMsg = "got invalid response from master at " +
               _masterInfo._endpoint +
               ": response does not contain valid 'id' attribute";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  auto shutdown = [&]() -> void {
    url = baseUrl + "/" + id.copyString();
    std::string progress =
        "deleting remote collection keys object for collection '" +
        collectionName + "' from " + url;
    setProgress(progress);

    // now delete the keys we ordered
    std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
        rest::RequestType::DELETE_REQ, url, nullptr, 0));
  };

  TRI_DEFER(shutdown());

  VPackSlice const count = slice.get("count");

  if (!count.isNumber()) {
    errorMsg = "got invalid response from master at " +
               _masterInfo._endpoint +
               ": response does not contain valid 'count' attribute";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  if (count.getNumber<size_t>() <= 0) {
    // remote collection has no documents. now truncate our local collection
    SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), col->cid(), TRI_TRANSACTION_WRITE);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = std::string("unable to start transaction: ") + TRI_errno_string(res);

      return res;
    }

    OperationOptions options;      
    OperationResult opRes = trx.truncate(collectionName, options);

    if (!opRes.successful()) {
      errorMsg = "unable to truncate collection '" + collectionName + "': " +
                 TRI_errno_string(opRes.code);
      return opRes.code;
    }

    res = trx.finish(opRes.code);
    
    return res;
  }

  // now we can fetch the complete chunk information from the master
  try {
    res = handleSyncKeys(col, id.copyString(), cid, collectionName, maxTick, errorMsg);
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (std::exception const& ex) {
    errorMsg = ex.what();
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief incrementally fetch data from a collection
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleSyncKeys(arangodb::LogicalCollection* col,
    std::string const& keysId, std::string const& cid,
    std::string const& collectionName, TRI_voc_tick_t maxTick,
    std::string& errorMsg) {

  std::string progress =
      "collecting local keys for collection '" + collectionName + "'";
  setProgress(progress);

  // fetch all local keys from primary index
  std::vector<void const*> markers;
    
  DocumentDitch* ditch = nullptr;

  // acquire a replication ditch so no datafiles are thrown away from now on
  // note: the ditch also protects against unloading the collection
  {    
    SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), col->cid(), TRI_TRANSACTION_READ);
  
    int res = trx.begin();
  
    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = std::string("unable to start transaction: ") + TRI_errno_string(res);
      return res;
    }
    
    ditch = col->ditches()->createDocumentDitch(false, __FILE__, __LINE__);

    if (ditch == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  TRI_ASSERT(ditch != nullptr);

  TRI_DEFER(col->ditches()->freeDitch(ditch));

  {
    SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), col->cid(), TRI_TRANSACTION_READ);
  
    int res = trx.begin();
  
    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = std::string("unable to start transaction: ") + TRI_errno_string(res);
      return res;
    }
    
    // We do not take responsibility for the index.
    // The LogicalCollection is protected by trx.
    // Neither it nor it's indexes can be invalidated
    auto idx = trx.documentCollection()->primaryIndex();
    markers.reserve(idx->size());

    uint64_t iterations = 0;
    trx.invokeOnAllElements(trx.name(), [this, &markers, &iterations](TRI_doc_mptr_t const* mptr) {
      markers.emplace_back(mptr->vpack());
      
      if (++iterations % 10000 == 0) {
        if (checkAborted()) {
          return false;
        }
      }
      return true;
    });

    if (checkAborted()) {
      return TRI_ERROR_REPLICATION_APPLIER_STOPPED;
    }
  
    sendExtendBatch();
    sendExtendBarrier();

    std::string progress = "sorting " + std::to_string(markers.size()) +
                           " local key(s) for collection '" + collectionName +
                           "'";
    setProgress(progress);

    // sort all our local keys
    std::sort(
        markers.begin(), markers.end(),
        [](void const* lhs, void const* rhs) -> bool {
          VPackSlice const l(reinterpret_cast<char const*>(lhs));
          VPackSlice const r(reinterpret_cast<char const*>(rhs));

          VPackValueLength lLength, rLength;
          char const* lKey = l.get(StaticStrings::KeyString).getString(lLength);
          char const* rKey = r.get(StaticStrings::KeyString).getString(rLength);

          size_t const length = static_cast<size_t>(lLength < rLength ? lLength : rLength);
          int res = memcmp(lKey, rKey, length);

          if (res < 0) {
            // left is smaller than right
            return true;
          }
          if (res == 0 && lLength < rLength) {
            // left is equal to right, but of shorter length
            return true;
          }

          return false;
        });
  }

  if (checkAborted()) {
    return TRI_ERROR_REPLICATION_APPLIER_STOPPED;
  }

  sendExtendBatch();
  sendExtendBarrier();

  std::vector<size_t> toFetch;

  TRI_voc_tick_t const chunkSize = 5000;
  std::string const baseUrl = BaseUrl + "/keys";

  std::string url =
      baseUrl + "/" + keysId + "?chunkSize=" + std::to_string(chunkSize);
  progress = "fetching remote keys chunks for collection '" + collectionName +
             "' from " + url;
  setProgress(progress);

  std::unique_ptr<SimpleHttpResult> response(
      _client->retryRequest(rest::RequestType::GET, url, nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "could not connect to master at " +
               _masterInfo._endpoint + ": " +
               _client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from master at " +
               _masterInfo._endpoint + ": HTTP " +
               StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();

    return TRI_ERROR_REPLICATION_MASTER_ERROR;
  }

  auto builder = std::make_shared<VPackBuilder>();
  int res = parseResponse(builder, response.get());

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg = "got invalid response from master at " +
               std::string(_masterInfo._endpoint) +
               ": invalid response is no array";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  VPackSlice const slice = builder->slice();

  if (!slice.isArray()) {
    errorMsg = "got invalid response from master at " +
               _masterInfo._endpoint + ": response is no array";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  
  OperationOptions options;
  options.silent = true;
  options.ignoreRevs = true;
  options.isRestore = true;

  VPackBuilder keyBuilder;
  size_t const n = static_cast<size_t>(slice.length());

  // remove all keys that are below first remote key or beyond last remote key
  if (n > 0) {
    // first chunk
    SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), col->cid(), TRI_TRANSACTION_WRITE);
  
    int res = trx.begin();
  
    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = std::string("unable to start transaction: ") + TRI_errno_string(res);
      return res;
    }
    
    VPackSlice chunk = slice.at(0);

    TRI_ASSERT(chunk.isObject());
    auto lowSlice = chunk.get("low");
    TRI_ASSERT(lowSlice.isString());

    std::string const lowKey(lowSlice.copyString());
      
    for (size_t i = 0; i < markers.size(); ++i) {
      VPackSlice const k(reinterpret_cast<char const*>(markers[i]));
     
      std::string const key(k.get(StaticStrings::KeyString).copyString());

      if (key.compare(lowKey) >= 0) { 
        break;
      }

      keyBuilder.clear();
      keyBuilder.openObject();
      keyBuilder.add(StaticStrings::KeyString, VPackValue(key));
      keyBuilder.close();

      trx.remove(collectionName, keyBuilder.slice(), options);
    }

    // last high
    chunk = slice.at(n - 1);
    TRI_ASSERT(chunk.isObject());

    auto highSlice = chunk.get("high");
    TRI_ASSERT(highSlice.isString());

    std::string const highKey(highSlice.copyString());

    for (size_t i = markers.size(); i >= 1; --i) {
      VPackSlice const k(reinterpret_cast<char const*>(markers[i - 1]));

      std::string const key(k.get(StaticStrings::KeyString).copyString());

      if (key.compare(highKey) <= 0) { 
        break;
      }
      
      keyBuilder.clear();
      keyBuilder.openObject();
      keyBuilder.add(StaticStrings::KeyString, VPackValue(key));
      keyBuilder.close();

      trx.remove(collectionName, keyBuilder.slice(), options);
    }

    trx.commit();
  }
    
  size_t nextStart = 0;

  // now process each chunk
  for (size_t i = 0; i < n; ++i) {
    if (checkAborted()) {
      return TRI_ERROR_REPLICATION_APPLIER_STOPPED;
    }
    
    SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), col->cid(), TRI_TRANSACTION_WRITE);
  
    int res = trx.begin();
  
    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = std::string("unable to start transaction: ") + TRI_errno_string(res);
      return res;
    }
    
    trx.orderDitch(col->cid()); // will throw when it fails
    
    // We do not take responsibility for the index.
    // The LogicalCollection is protected by trx.
    // Neither it nor it's indexes can be invalidated
    auto idx = trx.documentCollection()->primaryIndex();

    size_t const currentChunkId = i;
    progress = "processing keys chunk " + std::to_string(currentChunkId) +
               " for collection '" + collectionName + "'";
    setProgress(progress);

    sendExtendBatch();
    sendExtendBarrier();

    // read remote chunk
    VPackSlice chunk = slice.at(i);

    if (!chunk.isObject()) {
      errorMsg = "got invalid response from master at " +
                 _masterInfo._endpoint + ": chunk is no object";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    VPackSlice const lowSlice = chunk.get("low");
    VPackSlice const highSlice = chunk.get("high");
    VPackSlice const hashSlice = chunk.get("hash");

    if (!lowSlice.isString() || !highSlice.isString() ||
        !hashSlice.isString()) {
      errorMsg = "got invalid response from master at " +
                 _masterInfo._endpoint +
                 ": chunks in response have an invalid format";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    std::string const lowString = lowSlice.copyString();
    std::string const highString = highSlice.copyString();

    size_t localFrom;
    size_t localTo;
    bool match =
        FindRange(markers, lowString, highString, localFrom, localTo);

    if (match) {
      // now must hash the range
      uint64_t hash = 0x012345678;

      for (size_t i = localFrom; i <= localTo; ++i) {
        TRI_ASSERT(i < markers.size());
        VPackSlice const current(reinterpret_cast<char const*>(markers.at(i)));
        hash ^= current.get(StaticStrings::KeyString).hash();
        hash ^= current.get(StaticStrings::RevString).hash();
      }

      if (std::to_string(hash) != hashSlice.copyString()) {
        match = false;
      }
    }
  
    if (match) {
      // match
      nextStart = localTo + 1;
    } else {
      // no match
      // must transfer keys for non-matching range
      std::string url = baseUrl + "/" + keysId + "?type=keys&chunk=" +
                        std::to_string(i) + "&chunkSize=" +
                        std::to_string(chunkSize);
      progress = "fetching keys chunk " + std::to_string(currentChunkId) +
                 " for collection '" + collectionName + "' from " + url;
      setProgress(progress);

      std::unique_ptr<SimpleHttpResult> response(_client->retryRequest(
          rest::RequestType::PUT, url, nullptr, 0));

      if (response == nullptr || !response->isComplete()) {
        errorMsg = "could not connect to master at " +
                   _masterInfo._endpoint + ": " +
                   _client->getErrorMessage();

        return TRI_ERROR_REPLICATION_NO_RESPONSE;
      }

      TRI_ASSERT(response != nullptr);

      if (response->wasHttpError()) {
        errorMsg = "got invalid response from master at " +
                   _masterInfo._endpoint + ": HTTP " +
                   StringUtils::itoa(response->getHttpReturnCode()) + ": " +
                   response->getHttpReturnMessage();

        return TRI_ERROR_REPLICATION_MASTER_ERROR;
      }

      auto builder = std::make_shared<VPackBuilder>();
      int res = parseResponse(builder, response.get());

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "got invalid response from master at " +
                   _masterInfo._endpoint +
                   ": response is no array";

        return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }

      VPackSlice const slice = builder->slice();
      if (!slice.isArray()) {
        errorMsg = "got invalid response from master at " +
                   _masterInfo._endpoint +
                   ": response is no array";

        return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }


      // delete all keys at start of the range
      while (nextStart < markers.size()) {
        VPackSlice const keySlice(reinterpret_cast<char const*>(markers[nextStart]));
        std::string const localKey(keySlice.get(StaticStrings::KeyString).copyString());

        if (localKey.compare(lowString) < 0) {
          // we have a local key that is not present remotely
          keyBuilder.clear();
          keyBuilder.openObject();
          keyBuilder.add(StaticStrings::KeyString, VPackValue(localKey));
          keyBuilder.close();

          trx.remove(collectionName, keyBuilder.slice(), options);
          ++nextStart;
        } else {
          break;
        }
      }

      toFetch.clear();

      size_t const n = static_cast<size_t>(slice.length());
      TRI_ASSERT(n > 0);
  
      for (size_t i = 0; i < n; ++i) {
        VPackSlice const pair = slice.at(i);

        if (!pair.isArray() || pair.length() != 2) {
          errorMsg = "got invalid response from master at " +
                     _masterInfo._endpoint +
                     ": response key pair is no valid array";

          return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
        }

        // key
        VPackSlice const keySlice = pair.at(0);
        
        if (!keySlice.isString()) {
          errorMsg = "got invalid response from master at " +
                     _masterInfo._endpoint +
                     ": response key is no string";

          return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
        }

        // rid
        if (markers.empty()) {
          // no local markers
          toFetch.emplace_back(i);
          continue;
        }
          
        std::string const keyString = keySlice.copyString();

        while (nextStart < markers.size()) {
          VPackSlice const localKeySlice(reinterpret_cast<char const*>(markers[nextStart]));
          std::string const localKey(localKeySlice.get(StaticStrings::KeyString).copyString());

          int res = localKey.compare(keyString);

          if (res != 0) {
            // we have a local key that is not present remotely
            keyBuilder.clear();
            keyBuilder.openObject();
            keyBuilder.add(StaticStrings::KeyString, VPackValue(localKey));
            keyBuilder.close();

            trx.remove(collectionName, keyBuilder.slice(), options);
            ++nextStart;
          } else {
            // key match
            break;
          }
        }

        auto mptr = idx->lookupKey(&trx, keySlice);

        if (mptr == nullptr) {
          // key not found locally
          toFetch.emplace_back(i);
        } else if (std::to_string(mptr->revisionId()) != pair.at(1).copyString()) {
          // key found, but revision id differs
          toFetch.emplace_back(i);
          ++nextStart;
        } else {
          // a match - nothing to do!
          ++nextStart;
        }
      }

      // calculate next starting point
      if (!markers.empty()) {
        BinarySearch(markers, highString, nextStart);

        while (nextStart < markers.size()) {
          VPackSlice const localKeySlice(reinterpret_cast<char const*>(markers[nextStart]));
          std::string const localKey(localKeySlice.get(StaticStrings::KeyString).copyString());
          
          int res = localKey.compare(highString);

          if (res <= 0) {
            ++nextStart;
          } else {
            break;
          }
        }
      }

      if (!toFetch.empty()) {
        VPackBuilder keysBuilder;
        keysBuilder.openArray();
        for (auto& it : toFetch) {
          keysBuilder.add(VPackValue(it));
        }
        keysBuilder.close();
  
        std::string url = baseUrl + "/" + keysId + "?type=docs&chunk=" +
                          std::to_string(currentChunkId) + "&chunkSize=" +
                          std::to_string(chunkSize);
        progress = "fetching documents chunk " +
                   std::to_string(currentChunkId) + " for collection '" +
                   collectionName + "' from " + url;
        setProgress(progress);

        std::string const keyJsonString(keysBuilder.slice().toJson()); 

        std::unique_ptr<SimpleHttpResult> response(
            _client->retryRequest(rest::RequestType::PUT, url,
                                  keyJsonString.c_str(), keyJsonString.size()));

        if (response == nullptr || !response->isComplete()) {
          errorMsg = "could not connect to master at " +
                     _masterInfo._endpoint + ": " +
                     _client->getErrorMessage();

          return TRI_ERROR_REPLICATION_NO_RESPONSE;
        }

        TRI_ASSERT(response != nullptr);

        if (response->wasHttpError()) {
          errorMsg = "got invalid response from master at " +
                     _masterInfo._endpoint + ": HTTP " +
                     StringUtils::itoa(response->getHttpReturnCode()) + ": " +
                     response->getHttpReturnMessage();

          return TRI_ERROR_REPLICATION_MASTER_ERROR;
        }

        auto builder = std::make_shared<VPackBuilder>();
        int res = parseResponse(builder, response.get());

        if (res != TRI_ERROR_NO_ERROR) {
          errorMsg = "got invalid response from master at " +
                     std::string(_masterInfo._endpoint) +
                     ": response is no array";

          return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
        }
 
        VPackSlice const slice = builder->slice();
        if (!slice.isArray()) {
          errorMsg = "got invalid response from master at " +
                     _masterInfo._endpoint +
                     ": response is no array";

          return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
        }

        for (auto const& it : VPackArrayIterator(slice)) {
          if (!it.isObject()) {
            errorMsg = "got invalid response from master at " +
                       _masterInfo._endpoint +
                       ": document is no object";

            return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
          }

          VPackSlice const keySlice = it.get(StaticStrings::KeyString);

          if (!keySlice.isString()) {
            errorMsg = "got invalid response from master at " +
                       _masterInfo._endpoint +
                       ": document key is invalid";

            return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
          }

          VPackSlice const revSlice = it.get(StaticStrings::RevString);

          if (!revSlice.isString()) {
            errorMsg = "got invalid response from master at " +
                       _masterInfo._endpoint +
                       ": document revision is invalid";

            return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
          }

          auto mptr = idx->lookupKey(&trx, keySlice);

          if (mptr == nullptr) {
            // INSERT
            OperationResult opRes = trx.insert(collectionName, it, options);
            res = opRes.code;
          } else {
            // UPDATE
            OperationResult opRes = trx.update(collectionName, it, options);
            res = opRes.code;
          }

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
        }
      }
    }

    res = trx.commit();
    
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the properties of a collection, based on the VelocyPack
/// provided
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::changeCollection(arangodb::LogicalCollection* col,
                                    VPackSlice const& slice) {
  arangodb::CollectionGuard guard(_vocbase, col->cid());
  bool doSync =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database")
          ->forceSyncProperties();

  return guard.collection()->update(slice, doSync);
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief determine the number of documents in a collection
////////////////////////////////////////////////////////////////////////////////

int64_t InitialSyncer::getSize(arangodb::LogicalCollection* col) {
  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), col->cid(), TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    return -1;
  }
  
  auto document = trx.documentCollection();
  return static_cast<int64_t>(document->numberDocuments());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the information about a collection
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleCollection(VPackSlice const& parameters,
                                    VPackSlice const& indexes,
                                    bool incremental, std::string& errorMsg,
                                    sync_phase_e phase) {
  if (checkAborted()) {
    return TRI_ERROR_REPLICATION_APPLIER_STOPPED;
  }
    
  if (!parameters.isObject() || !indexes.isArray()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  sendExtendBatch();
  sendExtendBarrier();

  std::string const masterName =
      VelocyPackHelper::getStringValue(parameters, "name", "");

  TRI_voc_cid_t const cid = VelocyPackHelper::extractIdValue(parameters);

  if (cid == 0) {
    errorMsg = "collection id is missing in response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  VPackSlice const type = parameters.get("type");

  if (!type.isNumber()) {
    errorMsg = "collection type is missing in response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  std::string const typeString =
      (type.getNumber<int>() == 3 ? "edge" : "document");

  std::string const collectionMsg = "collection '" + masterName + "', type " +
                                    typeString + ", id " +
                                    StringUtils::itoa(cid);

  // phase handling
  if (phase == PHASE_VALIDATE) {
    // validation phase just returns ok if we got here (aborts above if data is
    // invalid)
    _processedCollections.emplace(cid, masterName);

    return TRI_ERROR_NO_ERROR;
  }

  // drop and re-create collections locally
  // -------------------------------------------------------------------------------------

  if (phase == PHASE_DROP_CREATE) {
    if (!incremental) {
      // first look up the collection by the cid
      arangodb::LogicalCollection* col = getCollectionByIdOrName(cid, masterName);

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

          SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), col->cid(), TRI_TRANSACTION_WRITE);

          int res = trx.begin();

          if (res != TRI_ERROR_NO_ERROR) {
            errorMsg = "unable to truncate " + collectionMsg + ": " +
                       TRI_errno_string(res);

            return res;
          }
    
          OperationOptions options;
          OperationResult opRes = trx.truncate(col->name(), options);

          if (!opRes.successful()) {
            errorMsg = "unable to truncate " + collectionMsg + ": " +
                       TRI_errno_string(opRes.code);

            return opRes.code;
          }

          res = trx.finish(opRes.code);

          if (res != TRI_ERROR_NO_ERROR) {
            errorMsg = "unable to truncate " + collectionMsg + ": " +
                       TRI_errno_string(res);

            return res;
          }
        } else {
          // regular collection
          setProgress("dropping " + collectionMsg);

          int res = _vocbase->dropCollection(col, true, true);

          if (res != TRI_ERROR_NO_ERROR) {
            errorMsg = "unable to drop " + collectionMsg + ": " +
                       TRI_errno_string(res);

            return res;
          }
        }
      }
    }

    arangodb::LogicalCollection* col = nullptr;

    if (incremental) {
      col = getCollectionByIdOrName(cid, masterName);

      if (col != nullptr) {
        // collection is already present
        std::string const progress =
            "checking/changing parameters of " + collectionMsg;
        setProgress(progress.c_str());
        return changeCollection(col, parameters);
      }
    }

    std::string const progress = "creating " + collectionMsg;
    setProgress(progress.c_str());

    int res = createCollection(parameters, &col);

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg =
          "unable to create " + collectionMsg + ": " + TRI_errno_string(res);

      return res;
    }

    return TRI_ERROR_NO_ERROR;
  }

  // sync collection data
  // -------------------------------------------------------------------------------------

  else if (phase == PHASE_DUMP) {
    std::string const progress = "dumping data for " + collectionMsg;
    setProgress(progress.c_str());

    arangodb::LogicalCollection* col = getCollectionByIdOrName(cid, masterName);

    if (col == nullptr) {
      errorMsg = "cannot dump: " + collectionMsg + " not found";

      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    int res = TRI_ERROR_INTERNAL;

    {
      READ_LOCKER(readLocker, _vocbase->_inventoryLock);

      if (incremental && getSize(col) > 0) {
        res = handleCollectionSync(col, StringUtils::itoa(cid), masterName, _masterInfo._lastLogTick, errorMsg);
      }
      else {
        res = handleCollectionDump(col, StringUtils::itoa(cid), masterName, _masterInfo._lastLogTick, errorMsg);
      }

      if (res == TRI_ERROR_NO_ERROR) {
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
            SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase), col->cid(), TRI_TRANSACTION_WRITE);
      
            res = trx.begin();

            if (res != TRI_ERROR_NO_ERROR) {
              errorMsg = "unable to start transaction: " + std::string(TRI_errno_string(res));
              return res;
            }

            trx.orderDitch(col->cid()); // will throw when it fails
      
            LogicalCollection* document = trx.documentCollection();
            TRI_ASSERT(document != nullptr);

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

              res = document->restoreIndex(&trx, idxDef, idx);

              if (res != TRI_ERROR_NO_ERROR) {
                errorMsg = "could not create index: " +
                            std::string(TRI_errno_string(res));
                break;
              } else {
                TRI_ASSERT(idx != nullptr);

                res = document->saveIndex(idx.get(), true);

                if (res != TRI_ERROR_NO_ERROR) {
                  errorMsg = "could not save index: " +
                              std::string(TRI_errno_string(res));
                  break;
                }
              }
            }
      
            res = trx.finish(res);
          } catch (arangodb::basics::Exception const& ex) {
            res = ex.code();
          } catch (...) {
            res = TRI_ERROR_INTERNAL;
          }
        }
      }
    }

    return res;
  }

  // we won't get here
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle the inventory response of the master
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::handleInventoryResponse(VPackSlice const& slice,
                                           bool incremental,
                                           std::string& errorMsg) {
  VPackSlice const data = slice.get("collections");

  if (!data.isArray()) {
    errorMsg = "collections section is missing from response";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  std::vector<std::pair<VPackSlice, VPackSlice>> collections;

  for (auto const& it : VPackArrayIterator(data)) {
    if (!it.isObject()) {
      errorMsg = "collection declaration is invalid in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    VPackSlice const parameters = it.get("parameters");

    if (!parameters.isObject()) {
      errorMsg = "collection parameters declaration is invalid in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    VPackSlice const indexes = it.get("indexes");

    if (!indexes.isArray()) {
      errorMsg = "collection indexes declaration is invalid in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    std::string const masterName =
        VelocyPackHelper::getStringValue(parameters, "name", "");

    if (masterName.empty()) {
      errorMsg = "collection name is missing in response";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    if (TRI_ExcludeCollectionReplication(masterName.c_str(), _includeSystem)) {
      continue;
    }

    if (VelocyPackHelper::getBooleanValue(parameters, "deleted", false)) {
      // we don't care about deleted collections
      continue;
    }

    if (!_restrictType.empty()) {
      auto const it = _restrictCollections.find(masterName);

      bool found = (it != _restrictCollections.end());

      if (_restrictType == "include" && !found) {
        // collection should not be included
        continue;
      } else if (_restrictType == "exclude" && found) {
        // collection should be excluded
        continue;
      }
    }

    collections.emplace_back(parameters, indexes);
  }

  int res;

  // STEP 1: validate collection declarations from master
  // ----------------------------------------------------------------------------------

  // iterate over all collections from the master...
  res = iterateCollections(collections, incremental, errorMsg, PHASE_VALIDATE);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // STEP 2: drop and re-create collections locally if they are also present on
  // the master
  //  ------------------------------------------------------------------------------------

  res =
      iterateCollections(collections, incremental, errorMsg, PHASE_DROP_CREATE);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // STEP 3: sync collection data from master and create initial indexes
  // ----------------------------------------------------------------------------------

  return iterateCollections(collections, incremental, errorMsg, PHASE_DUMP);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all collections from an array and apply an action
////////////////////////////////////////////////////////////////////////////////

int InitialSyncer::iterateCollections(
    std::vector<std::pair<VPackSlice, VPackSlice>> const&
        collections,
    bool incremental, std::string& errorMsg, sync_phase_e phase) {
  std::string phaseMsg("starting phase " + translatePhase(phase) + " with " +
                       std::to_string(collections.size()) + " collections");
  setProgress(phaseMsg);

  for (auto const& collection : collections) {
    VPackSlice const parameters = collection.first;
    VPackSlice const indexes = collection.second;

    errorMsg = "";
    int res =
        handleCollection(parameters, indexes, incremental, errorMsg, phase);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  // all ok
  return TRI_ERROR_NO_ERROR;
}
