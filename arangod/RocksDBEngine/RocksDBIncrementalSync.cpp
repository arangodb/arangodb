////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBIncrementalSync.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Indexes/IndexIterator.h"
#include "Replication/InitialSyncer.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
int syncChunkRocksDB(
    InitialSyncer& syncer, SingleCollectionTransaction* trx,
    std::string const& keysId, uint64_t chunkId, std::string const& lowString,
    std::string const& highString,
    std::vector<std::pair<std::string, uint64_t>> const& markers,
    std::string& errorMsg) {
  std::string const baseUrl = syncer.BaseUrl + "/keys";
  TRI_voc_tick_t const chunkSize = 5000;
  std::string const& collectionName = trx->documentCollection()->name();
  PhysicalCollection* physical = trx->documentCollection()->getPhysical();
  OperationOptions options;
  options.silent = true;
  options.ignoreRevs = true;
  options.isRestore = true;
  if (!syncer._leaderId.empty()) {
    options.isSynchronousReplicationFrom = syncer._leaderId;
  }

  LOG_TOPIC(TRACE, Logger::REPLICATION) << "synching chunk. low: '" << lowString
                                        << "', high: '" << highString << "'";

  // no match
  // must transfer keys for non-matching range
  std::string url = baseUrl + "/" + keysId + "?type=keys&chunk=" +
                    std::to_string(chunkId) + "&chunkSize=" +
                    std::to_string(chunkSize) + "&low=" + lowString;

  std::string progress =
      "fetching keys chunk '" + std::to_string(chunkId) + "' from " + url;
  syncer.setProgress(progress);

  std::unique_ptr<httpclient::SimpleHttpResult> response(
      syncer._client->retryRequest(rest::RequestType::PUT, url, nullptr, 0, syncer.createHeaders()));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "could not connect to master at " +
               syncer._masterInfo._endpoint + ": " +
               syncer._client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from master at " +
               syncer._masterInfo._endpoint + ": HTTP " +
               basics::StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();

    return TRI_ERROR_REPLICATION_MASTER_ERROR;
  }

  auto builder = std::make_shared<VPackBuilder>();
  int res = syncer.parseResponse(builder, response.get());

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg = "got invalid response from master at " +
               syncer._masterInfo._endpoint + ": response is no array";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  VPackSlice const responseBody = builder->slice();
  if (!responseBody.isArray()) {
    errorMsg = "got invalid response from master at " +
               syncer._masterInfo._endpoint + ": response is no array";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  transaction::BuilderLeaser keyBuilder(trx);

  std::vector<size_t> toFetch;

  size_t const numKeys = static_cast<size_t>(responseBody.length());
  if (numKeys == 0) {
    errorMsg = "got invalid response from master at " +
               syncer._masterInfo._endpoint +
               ": response contains an empty chunk. Collection: " +
               collectionName + " Chunk: " + std::to_string(chunkId);
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  TRI_ASSERT(numKeys > 0);

  size_t i = 0;
  size_t nextStart = 0;

  for (VPackSlice const& pair : VPackArrayIterator(responseBody)) {
    if (!pair.isArray() || pair.length() != 2) {
      errorMsg = "got invalid response from master at " +
                 syncer._masterInfo._endpoint +
                 ": response key pair is no valid array";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    // key
    VPackSlice const keySlice = pair.at(0);
    if (!keySlice.isString()) {
      errorMsg = "got invalid response from master at " +
                 syncer._masterInfo._endpoint + ": response key is no string";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    // rid
    if (markers.empty()) {
      // no local markers
      toFetch.emplace_back(i);
      i++;
      continue;
    }

    bool mustRefetch = false;

    // remove keys not present anymore
    while (nextStart < markers.size()) {
      std::string const& localKey = markers[nextStart].first;

      int res = keySlice.compareString(localKey);
      if (res > 0) {
        // we have a local key that is not present remotely
        keyBuilder->clear();
        keyBuilder->openObject();
        keyBuilder->add(StaticStrings::KeyString, VPackValue(localKey));
        keyBuilder->close();

        trx->remove(collectionName, keyBuilder->slice(), options);

        ++nextStart;
      } else if (res == 0) {
        // key match
        break;
      } else {
        // we have a remote key that is not present locally
        TRI_ASSERT(res < 0);
        mustRefetch = true;
        break;
      }
    }

    if (mustRefetch) {
      toFetch.emplace_back(i);
    } else {
      // see if key exists
      DocumentIdentifierToken token = physical->lookupKey(trx, keySlice);
      if (token._data == 0) {
        // key not found locally
        toFetch.emplace_back(i);
      } else if (token._data !=
                 basics::StringUtils::uint64(pair.at(1).copyString())) {
        // key found, but revision id differs
        toFetch.emplace_back(i);
        ++nextStart;
      } else {
        // a match - nothing to do!
        ++nextStart;
      }
    }

    i++;
  }

  // delete all keys at end of the range
  while (nextStart < markers.size()) {
    std::string const& localKey = markers[nextStart].first;

    if (localKey.compare(highString) > 0) {
      // we have a local key that is not present remotely
      keyBuilder->clear();
      keyBuilder->openObject();
      keyBuilder->add(StaticStrings::KeyString, VPackValue(localKey));
      keyBuilder->close();

      trx->remove(collectionName, keyBuilder->slice(), options);
    }
    ++nextStart;
  }

  LOG_TOPIC(TRACE, Logger::REPLICATION) << "will refetch " << toFetch.size()
                                        << " documents for this chunk";

  if (!toFetch.empty()) {
    VPackBuilder keysBuilder;
    keysBuilder.openArray();
    for (auto& it : toFetch) {
      keysBuilder.add(VPackValue(it));
    }
    keysBuilder.close();

    std::string url = baseUrl + "/" + keysId + "?type=docs&chunk=" +
                      std::to_string(chunkId) + "&chunkSize=" +
                      std::to_string(chunkSize) + "&low=" + lowString;

    progress = "fetching documents chunk " + std::to_string(chunkId) +
               " for collection '" + collectionName + "' from " + url;
    syncer.setProgress(progress);

    std::string const keyJsonString(keysBuilder.slice().toJson());

    
    std::unique_ptr<httpclient::SimpleHttpResult> response(
        syncer._client->retryRequest(rest::RequestType::PUT, url,
                                     keyJsonString.c_str(),
                                     keyJsonString.size(), syncer.createHeaders()));

    if (response == nullptr || !response->isComplete()) {
      errorMsg = "could not connect to master at " +
                 syncer._masterInfo._endpoint + ": " +
                 syncer._client->getErrorMessage();

      return TRI_ERROR_REPLICATION_NO_RESPONSE;
    }

    TRI_ASSERT(response != nullptr);

    if (response->wasHttpError()) {
      errorMsg = "got invalid response from master at " +
                 syncer._masterInfo._endpoint + ": HTTP " +
                 basics::StringUtils::itoa(response->getHttpReturnCode()) +
                 ": " + response->getHttpReturnMessage();

      return TRI_ERROR_REPLICATION_MASTER_ERROR;
    }

    auto builder = std::make_shared<VPackBuilder>();
    int res = syncer.parseResponse(builder, response.get());

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "got invalid response from master at " +
                 std::string(syncer._masterInfo._endpoint) +
                 ": response is no array";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    VPackSlice const slice = builder->slice();
    if (!slice.isArray()) {
      errorMsg = "got invalid response from master at " +
                 syncer._masterInfo._endpoint + ": response is no array";

      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }

    for (auto const& it : VPackArrayIterator(slice)) {
      if (!it.isObject()) {
        errorMsg = "got invalid response from master at " +
                   syncer._masterInfo._endpoint + ": document is no object";

        return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }

      VPackSlice const keySlice = it.get(StaticStrings::KeyString);

      if (!keySlice.isString()) {
        errorMsg = "got invalid response from master at " +
                   syncer._masterInfo._endpoint + ": document key is invalid";

        return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }

      VPackSlice const revSlice = it.get(StaticStrings::RevString);

      if (!revSlice.isString()) {
        errorMsg = "got invalid response from master at " +
                   syncer._masterInfo._endpoint +
                   ": document revision is invalid";

        return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
      }

      DocumentIdentifierToken token = physical->lookupKey(trx, keySlice);

      if (!token._data) {
        // INSERT
        OperationResult opRes = trx->insert(collectionName, it, options);
        res = opRes.code;
      } else {
        // UPDATE
        OperationResult opRes = trx->update(collectionName, it, options);
        res = opRes.code;
      }

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }
  }
  return TRI_ERROR_NO_ERROR;
}

int handleSyncKeysRocksDB(InitialSyncer& syncer,
                          arangodb::LogicalCollection* col,
                          std::string const& keysId, std::string const& cid,
                          std::string const& collectionName,
                          TRI_voc_tick_t maxTick, std::string& errorMsg) {
  std::string progress =
      "collecting local keys for collection '" + collectionName + "'";
  syncer.setProgress(progress);

  if (syncer.checkAborted()) {
    return TRI_ERROR_REPLICATION_APPLIER_STOPPED;
  }

  syncer.sendExtendBatch();
  syncer.sendExtendBarrier();

  TRI_voc_tick_t const chunkSize = 5000;
  std::string const baseUrl = syncer.BaseUrl + "/keys";

  std::string url =
      baseUrl + "/" + keysId + "?chunkSize=" + std::to_string(chunkSize);
  progress = "fetching remote keys chunks for collection '" + collectionName +
             "' from " + url;
  syncer.setProgress(progress);
  auto const headers = syncer.createHeaders();
  std::unique_ptr<httpclient::SimpleHttpResult> response(
      syncer._client->retryRequest(rest::RequestType::GET, url, nullptr, 0, headers));

  if (response == nullptr || !response->isComplete()) {
    errorMsg = "could not connect to master at " +
               syncer._masterInfo._endpoint + ": " +
               syncer._client->getErrorMessage();

    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    errorMsg = "got invalid response from master at " +
               syncer._masterInfo._endpoint + ": HTTP " +
               basics::StringUtils::itoa(response->getHttpReturnCode()) + ": " +
               response->getHttpReturnMessage();

    return TRI_ERROR_REPLICATION_MASTER_ERROR;
  }

  auto builder = std::make_shared<VPackBuilder>();
  int res = syncer.parseResponse(builder, response.get());

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg = "got invalid response from master at " +
               std::string(syncer._masterInfo._endpoint) +
               ": invalid response is no array";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  VPackSlice const chunkSlice = builder->slice();

  if (!chunkSlice.isArray()) {
    errorMsg = "got invalid response from master at " +
               syncer._masterInfo._endpoint + ": response is no array";

    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  ManagedDocumentResult mmdr;
  OperationOptions options;
  options.silent = true;
  options.ignoreRevs = true;
  options.isRestore = true;
  if (!syncer._leaderId.empty()) {
    options.isSynchronousReplicationFrom = syncer._leaderId;
  }

  VPackBuilder keyBuilder;
  size_t const numChunks = static_cast<size_t>(chunkSlice.length());

  // remove all keys that are below first remote key or beyond last remote key
  if (numChunks > 0) {
    // first chunk
    SingleCollectionTransaction trx(
        transaction::StandaloneContext::Create(syncer._vocbase), col->cid(),
        AccessMode::Type::EXCLUSIVE);

    Result res = trx.begin();

    if (!res.ok()) {
      errorMsg =
          std::string("unable to start transaction: ") + res.errorMessage();
      res.reset(res.errorNumber(), errorMsg);
      return res.errorNumber();
    }

    VPackSlice chunk = chunkSlice.at(0);

    TRI_ASSERT(chunk.isObject());
    auto lowSlice = chunk.get("low");
    TRI_ASSERT(lowSlice.isString());

    // last high
    chunk = chunkSlice.at(numChunks - 1);
    TRI_ASSERT(chunk.isObject());

    auto highSlice = chunk.get("high");
    TRI_ASSERT(highSlice.isString());

    std::string const lowKey(lowSlice.copyString());
    std::string const highKey(highSlice.copyString());

    LogicalCollection* coll = trx.documentCollection();
    auto ph = static_cast<RocksDBCollection*>(coll->getPhysical());
    std::unique_ptr<IndexIterator> iterator =
        ph->getSortedAllIterator(&trx, &mmdr);
    iterator->next(
        [&](DocumentIdentifierToken const& token) {
          if (coll->readDocument(&trx, token, mmdr) == false) {
            return;
          }
          VPackSlice doc(mmdr.vpack());
          VPackSlice key = doc.get(StaticStrings::KeyString);
          if (key.compareString(lowKey.data(), lowKey.length()) < 0) {
            trx.remove(collectionName, doc, options);
          } else if (key.compareString(highKey.data(), highKey.length()) > 0) {
            trx.remove(collectionName, doc, options);
          }
        },
        UINT64_MAX);

    res = trx.commit();
  }

  {
    if (syncer.checkAborted()) {
      return TRI_ERROR_REPLICATION_APPLIER_STOPPED;
    }

    SingleCollectionTransaction trx(
        transaction::StandaloneContext::Create(syncer._vocbase), col->cid(),
        AccessMode::Type::EXCLUSIVE);

    Result res = trx.begin();

    if (!res.ok()) {
      errorMsg =
          std::string("unable to start transaction: ") + res.errorMessage();
      res.reset(res.errorNumber(), res.errorMessage());
      return res.errorNumber();
    }

    // We do not take responsibility for the index.
    // The LogicalCollection is protected by trx.
    // Neither it nor it's indexes can be invalidated

    size_t currentChunkId = 0;

    std::string lowKey;
    std::string highKey;
    std::string hashString;
    uint64_t localHash = 0x012345678;
    // chunk keys + revisionId
    std::vector<std::pair<std::string, uint64_t>> markers;
    bool foundLowKey = false;

    auto resetChunk = [&]() -> void {
      syncer.sendExtendBatch();
      syncer.sendExtendBarrier();

      progress = "processing keys chunk " + std::to_string(currentChunkId) +
                 " for collection '" + collectionName + "'";
      syncer.setProgress(progress);

      // read remote chunk
      VPackSlice chunk = chunkSlice.at(currentChunkId);
      if (!chunk.isObject()) {
        errorMsg = "got invalid response from master at " +
                   syncer._masterInfo._endpoint + ": chunk is no object";
        THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_INVALID_RESPONSE);
      }

      VPackSlice const lowSlice = chunk.get("low");
      VPackSlice const highSlice = chunk.get("high");
      VPackSlice const hashSlice = chunk.get("hash");
      if (!lowSlice.isString() || !highSlice.isString() ||
          !hashSlice.isString()) {
        errorMsg = "got invalid response from master at " +
                   syncer._masterInfo._endpoint +
                   ": chunks in response have an invalid format";
        THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_INVALID_RESPONSE);
      }

      // now reset chunk information
      markers.clear();
      lowKey = lowSlice.copyString();
      highKey = highSlice.copyString();
      hashString = hashSlice.copyString();
      localHash = 0x012345678;
      foundLowKey = false;
    };
    // set to first chunk
    resetChunk();

    std::function<void(VPackSlice, VPackSlice)> parseDoc = [&](VPackSlice doc,
                                                               VPackSlice key) {

      bool rangeUnequal = false;
      bool nextChunk = false;

      int cmp1 = key.compareString(lowKey.data(), lowKey.length());
      int cmp2 = key.compareString(highKey.data(), highKey.length());

      if (cmp1 < 0) {
        // smaller values than lowKey mean they don't exist remotely
        trx.remove(collectionName, key, options);
        return;
      } else if (cmp1 >= 0 && cmp2 <= 0) {
        // we only need to hash we are in the range
        if (cmp1 == 0) {
          foundLowKey = true;
        }

        markers.emplace_back(key.copyString(), TRI_ExtractRevisionId(doc));
        // don't bother hashing if we have't found lower key
        if (foundLowKey) {
          VPackSlice revision = doc.get(StaticStrings::RevString);
          localHash ^= key.hashString();
          localHash ^= revision.hash();

          if (cmp2 == 0) {  // found highKey
            rangeUnequal = std::to_string(localHash) != hashString;
            nextChunk = true;
          }
        } else if (cmp2 == 0) {  // found high key, but not low key
          rangeUnequal = true;
          nextChunk = true;
        }
      } else if (cmp2 > 0) {  // higher than highKey
        // current range was unequal and we did not find the
        // high key. Load range and skip to next
        rangeUnequal = true;
        nextChunk = true;
      }

      TRI_ASSERT(!rangeUnequal || nextChunk);  // A => B
      if (nextChunk) {  // we are out of range, see next chunk
        if (rangeUnequal && currentChunkId < numChunks) {
          int res = syncChunkRocksDB(syncer, &trx, keysId, currentChunkId,
                                     lowKey, highKey, markers, errorMsg);
          if (res != TRI_ERROR_NO_ERROR) {
            THROW_ARANGO_EXCEPTION(res);
          }
        }
        currentChunkId++;
        if (currentChunkId < numChunks) {
          resetChunk();
          // key is higher than upper bound, recheck the current document
          if (cmp2 > 0) {
            parseDoc(doc, key);
          }
        }
      }
    };

    auto ph = static_cast<RocksDBCollection*>(col->getPhysical());
    std::unique_ptr<IndexIterator> iterator =
        ph->getSortedAllIterator(&trx, &mmdr);
    iterator->next(
        [&](DocumentIdentifierToken const& token) {
          if (col->readDocument(&trx, token, mmdr) == false) {
            return;
          }
          VPackSlice doc(mmdr.vpack());
          VPackSlice key = doc.get(StaticStrings::KeyString);
          parseDoc(doc, key);
        },
        UINT64_MAX);

    // we might have missed chunks, if the keys don't exist at all locally
    while (currentChunkId < numChunks) {
      int res = syncChunkRocksDB(syncer, &trx, keysId, currentChunkId, lowKey,
                                 highKey, markers, errorMsg);
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
      currentChunkId++;
      if (currentChunkId < numChunks) {
        resetChunk();
      }
    }

    res = trx.commit();
    if (!res.ok()) {
      return res.errorNumber();
    }
  }

  return res;
}
}
