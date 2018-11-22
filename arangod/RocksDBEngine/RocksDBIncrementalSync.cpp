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
#include "Replication/DatabaseInitialSyncer.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBIterators.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

Result syncChunkRocksDB(DatabaseInitialSyncer& syncer,
    SingleCollectionTransaction* trx,
    IncrementalSyncStats& stats,
    std::string const& keysId, uint64_t chunkId, std::string const& lowString,
    std::string const& highString,
    std::vector<std::pair<std::string, uint64_t>> const& markers) {
  // first thing we do is extend the batch lifetime
  syncer.sendExtendBatch();
  syncer.sendExtendBarrier();

  std::string const baseUrl = syncer.ReplicationUrl + "/keys";
  TRI_voc_tick_t const chunkSize = 5000;
  std::string const& collectionName = trx->documentCollection()->name();
  PhysicalCollection* physical = trx->documentCollection()->getPhysical();
  OperationOptions options;
  options.silent = true;
  options.ignoreRevs = true;
  options.isRestore = true;
  options.indexOperationMode = Index::OperationMode::internal;
  if (!syncer._leaderId.empty()) {
    options.isSynchronousReplicationFrom = syncer._leaderId;
  }

  LOG_TOPIC(TRACE, Logger::REPLICATION) << "syncing chunk. low: '" << lowString
                                        << "', high: '" << highString << "'";

  // no match
  // must transfer keys for non-matching range
  std::string url = baseUrl + "/" + keysId + "?type=keys&chunk=" +
                    std::to_string(chunkId) + "&chunkSize=" +
                    std::to_string(chunkSize) + "&low=" + lowString;

  std::string progress =
      "fetching keys chunk " + std::to_string(chunkId) + " from " + url;
  syncer.setProgress(progress);

  double t = TRI_microtime();
  ++stats.numKeysRequests;
  std::unique_ptr<httpclient::SimpleHttpResult> response(
      syncer._client->retryRequest(rest::RequestType::PUT, url, nullptr, 0, syncer.createHeaders()));

  stats.waitedForKeys += TRI_microtime() - t;

  if (response == nullptr || !response->isComplete()) {
    return Result(TRI_ERROR_REPLICATION_NO_RESPONSE, std::string("could not connect to master at ") + syncer._masterInfo._endpoint + url + ": " + syncer._client->getErrorMessage());
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    return Result(TRI_ERROR_REPLICATION_MASTER_ERROR, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": HTTP " + basics::StringUtils::itoa(response->getHttpReturnCode()) + ": " + response->getHttpReturnMessage());
  }

  VPackBuilder builder;
  Result r = syncer.parseResponse(builder, response.get());

  if (r.fail()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": response is no array");
  }

  VPackSlice const responseBody = builder.slice();
  if (!responseBody.isArray()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": response is no array");
  }

  transaction::BuilderLeaser keyBuilder(trx);

  std::vector<size_t> toFetch;

  size_t const numKeys = static_cast<size_t>(responseBody.length());
  if (numKeys == 0) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": response contains an empty chunk. Collection: " + collectionName + " Chunk: " + std::to_string(chunkId));
  }
  TRI_ASSERT(numKeys > 0);

  ManagedDocumentResult mmdr;
  size_t i = 0;
  size_t nextStart = 0;

  for (VPackSlice const& pair : VPackArrayIterator(responseBody)) {
    if (!pair.isArray() || pair.length() != 2) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": response key pair is no valid array");
    }

    // key
    VPackSlice const keySlice = pair.at(0);
    if (!keySlice.isString()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": response key is no string");
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
        ++stats.numDocsRemoved;

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
      LocalDocumentId const documentId = physical->lookupKey(trx, keySlice);
      if (!documentId.isSet()) {
        // key not found locally
        toFetch.emplace_back(i);
      } else {
        TRI_voc_rid_t currentRevisionId = 0;
        if (physical->readDocument(trx, documentId, mmdr)) {
          currentRevisionId = transaction::helpers::extractRevFromDocument(VPackSlice(mmdr.vpack()));
        }
        if (TRI_RidToString(currentRevisionId) != pair.at(1).copyString()) {
          // key found, but revision id differs
          toFetch.emplace_back(i);
          ++nextStart;
        } else {
          // a match - nothing to do!
          ++nextStart;
        }
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
      ++stats.numDocsRemoved;
    }
    ++nextStart;
  }

  if (toFetch.empty()) {
    // nothing to do
    return Result();
  }

  syncer.sendExtendBatch();
  syncer.sendExtendBarrier();

  ++stats.numDocsRequests;
  stats.numDocsRequested += toFetch.size();

  LOG_TOPIC(TRACE, Logger::REPLICATION) << "will refetch " << toFetch.size()
                                        << " documents for this chunk";

  VPackBuilder keysBuilder;
  keysBuilder.openArray();
  for (auto const& it : toFetch) {
    keysBuilder.add(VPackValue(it));
  }
  keysBuilder.close();

  std::string const keyJsonString(keysBuilder.slice().toJson());

  size_t offsetInChunk = 0;

  while (true) {
    std::string url = baseUrl + "/" + keysId + "?type=docs&chunk=" +
                      std::to_string(chunkId) + "&chunkSize=" +
                      std::to_string(chunkSize) + "&low=" + lowString +
                      "&offset=" + std::to_string(offsetInChunk);

    progress = "fetching documents chunk " + std::to_string(chunkId) +
               " (" + std::to_string(toFetch.size()) + " keys) for collection '" + collectionName + "' from " + url;
    syncer.setProgress(progress);

    double t = TRI_microtime();

    std::unique_ptr<httpclient::SimpleHttpResult> response(
        syncer._client->retryRequest(rest::RequestType::PUT, url,
                                     keyJsonString.c_str(),
                                     keyJsonString.size(), syncer.createHeaders()));

    stats.waitedForDocs += TRI_microtime() - t;

    if (response == nullptr || !response->isComplete()) {
      return Result(TRI_ERROR_REPLICATION_NO_RESPONSE, std::string("could not connect to master at ") + syncer._masterInfo._endpoint + url + ": " + syncer._client->getErrorMessage());
    }

    TRI_ASSERT(response != nullptr);

    if (response->wasHttpError()) {
      return Result(TRI_ERROR_REPLICATION_MASTER_ERROR, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": HTTP " + basics::StringUtils::itoa(response->getHttpReturnCode()) + ": " + response->getHttpReturnMessage());
    }

    VPackBuilder builder;
    Result r = syncer.parseResponse(builder, response.get());

    if (r.fail()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": response is no array");
    }

    VPackSlice const slice = builder.slice();
    if (!slice.isArray()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": response is no array");
    }

    size_t foundLength = slice.length();

    for (auto const& it : VPackArrayIterator(slice)) {
      if (it.isNull()) {
        continue;
      }

      if (!it.isObject()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": document is no object");
      }

      VPackSlice const keySlice = it.get(StaticStrings::KeyString);

      if (!keySlice.isString()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": document key is invalid");
      }

      VPackSlice const revSlice = it.get(StaticStrings::RevString);

      if (!revSlice.isString()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": document revision is invalid");
      }

      LocalDocumentId const documentId = physical->lookupKey(trx, keySlice);

      auto removeConflict = [&](std::string const& conflictingKey) -> OperationResult {
        VPackBuilder conflict;
        conflict.add(VPackValue(conflictingKey));
        LocalDocumentId conflictId = physical->lookupKey(trx, conflict.slice());
        if (conflictId.isSet()) {
          ManagedDocumentResult mmdr;
          bool success = physical->readDocument(trx, conflictId, mmdr);
          if (success) {
            VPackSlice conflictingKey(mmdr.vpack());
            return trx->remove(collectionName, conflictingKey, options);
          }
        }
        return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
      };

      if (!documentId.isSet()) {
        // INSERT
        OperationResult opRes = trx->insert(collectionName, it, options);
        if (opRes.fail()) {
          if (opRes.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) && opRes.errorMessage() > keySlice.copyString()) {
            // remove conflict and retry
            auto inner = removeConflict(opRes.errorMessage());
            if (inner.fail()) {
              return opRes.result;
            }
            opRes = trx->insert(collectionName, it, options);
            if (opRes.fail()) {
              return opRes.result;
            }
          } else {
            return opRes.result;
          }
        }
      } else {
        // REPLACE
        OperationResult opRes = trx->replace(collectionName, it, options);
        if (opRes.fail()) {
          if (opRes.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) && opRes.errorMessage() > keySlice.copyString()) {
            // remove conflict and retry
            auto inner = removeConflict(opRes.errorMessage());
            if (inner.fail()) {
              return opRes.result;
            }
            opRes = trx->replace(collectionName, it, options);
            if (opRes.fail()) {
              return opRes.result;
            }
          } else {
            return opRes.result;
          }
        }
      }
      ++stats.numDocsInserted;
    }

    if (foundLength >= toFetch.size()) {
      break;
    }

    // try again in next round
    offsetInChunk = foundLength;
  }

  return Result();
}

Result handleSyncKeysRocksDB(DatabaseInitialSyncer& syncer,
                             arangodb::LogicalCollection* col,
                             std::string const& keysId) {
                             
  IncrementalSyncStats stats;
  
  std::string progress =
      "collecting local keys for collection '" + col->name() + "'";
  syncer.setProgress(progress);

  if (syncer.isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  syncer.sendExtendBatch();
  syncer.sendExtendBarrier();

  TRI_voc_tick_t const chunkSize = 5000;
  std::string const baseUrl = syncer.ReplicationUrl + "/keys";

  std::string url =
      baseUrl + "/" + keysId + "?chunkSize=" + std::to_string(chunkSize);
  progress = "fetching remote keys chunks for collection '" + col->name() +
             "' from " + url;
  syncer.setProgress(progress);
  auto const headers = syncer.createHeaders();

  double t = TRI_microtime();

  std::unique_ptr<httpclient::SimpleHttpResult> response(
      syncer._client->retryRequest(rest::RequestType::GET, url, nullptr, 0, headers));
  
  stats.waitedForInitial += TRI_microtime() - t;

  if (response == nullptr || !response->isComplete()) {
    return Result(TRI_ERROR_REPLICATION_NO_RESPONSE, std::string("could not connect to master at ") + syncer._masterInfo._endpoint + url + ": " + syncer._client->getErrorMessage());
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    return Result(TRI_ERROR_REPLICATION_MASTER_ERROR, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": HTTP " + basics::StringUtils::itoa(response->getHttpReturnCode()) + ": " + response->getHttpReturnMessage());
  }

  VPackBuilder builder;
  Result r  = syncer.parseResponse(builder, response.get());

  if (r.fail()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": response is no array");
  }

  VPackSlice const chunkSlice = builder.slice();

  if (!chunkSlice.isArray()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": response is no array");
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
        transaction::StandaloneContext::Create(syncer.vocbase()), col->cid(),
        AccessMode::Type::EXCLUSIVE);

    trx.addHint(transaction::Hints::Hint::RECOVERY);  // turn off waitForSync!
    trx.addHint(transaction::Hints::Hint::NO_INDEXING);
    // turn on intermediate commits as the number of keys to delete can be huge here
    trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);

    Result res = trx.begin();

    if (!res.ok()) {
      return Result(res.errorNumber(), std::string("unable to start transaction: ") + res.errorMessage());
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

    StringRef lowRef(lowSlice);
    StringRef highRef(highSlice);

    LogicalCollection* coll = trx.documentCollection();
    auto iterator = createPrimaryIndexIterator(&trx, coll);
    VPackBuilder builder;
    iterator.next(
        [&](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
          StringRef docKey(RocksDBKey::primaryKey(rocksKey));
          if (docKey.compare(lowRef) < 0 || docKey.compare(highRef) > 0) {
            builder.clear();
            builder.add(velocypack::ValuePair(docKey.data(),docKey.size(), velocypack::ValueType::String));
            trx.remove(col->name(), builder.slice(), options);
          }
        },
        std::numeric_limits<std::uint64_t>::max());

    res = trx.commit();

    if (!res.ok()) {
      return res;
    }
  }

  {
    if (syncer.isAborted()) {
      return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }

    SingleCollectionTransaction trx(
        transaction::StandaloneContext::Create(syncer.vocbase()), col->cid(),
        AccessMode::Type::EXCLUSIVE);

    trx.addHint(transaction::Hints::Hint::RECOVERY);  // turn off waitForSync!
    trx.addHint(transaction::Hints::Hint::NO_INDEXING);
    // turn on intermediate commits as the number of operations can be huge here
    trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);

    Result res = trx.begin();
    
    if (!res.ok()) {
      return Result(res.errorNumber(), std::string("unable to start transaction: ") + res.errorMessage());
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
                 " for collection '" + col->name() + "'";
      syncer.setProgress(progress);

      // read remote chunk
      TRI_ASSERT(chunkSlice.isArray());
      TRI_ASSERT(chunkSlice.length() > 0); // chunkSlice.at will throw otherwise
      VPackSlice chunk = chunkSlice.at(currentChunkId);
      if (!chunk.isObject()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": chunk is no object");
      }

      VPackSlice const lowSlice = chunk.get("low");
      VPackSlice const highSlice = chunk.get("high");
      VPackSlice const hashSlice = chunk.get("hash");
      if (!lowSlice.isString() || !highSlice.isString() ||
          !hashSlice.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + syncer._masterInfo._endpoint + url + ": chunks in response have an invalid format");
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

    std::function<void(std::string, std::uint64_t)> compareChunk =
      [&trx,&col,&options,&foundLowKey
      ,&markers,&localHash,&hashString
      ,&syncer, &currentChunkId
      ,&numChunks, &keysId, &resetChunk, &compareChunk
      ,&lowKey,&highKey, &stats]
      (std::string const& docKey, std::uint64_t docRev) {
      
      bool rangeUnequal = false;
      bool nextChunk = false;

      VPackBuilder docKeyBuilder;
      docKeyBuilder.add(VPackValue(docKey));
      VPackSlice docKeySlice(docKeyBuilder.slice());
      int cmp1 = docKey.compare(lowKey);
      int cmp2 = docKey.compare(highKey);

      if (cmp1 < 0) {
        // smaller values than lowKey mean they don't exist remotely
        trx.remove(col->name(), docKeySlice, options);
        return;
      } else if (cmp1 >= 0 && cmp2 <= 0) {
        // we only need to hash we are in the range
        if (cmp1 == 0) {
          foundLowKey = true;
        }

        markers.emplace_back(docKey, docRev); //revision as unit64
        // don't bother hashing if we have't found lower key
        if (foundLowKey) {
          VPackBuilder revBuilder;
          revBuilder.add(VPackValue(TRI_RidToString(docRev)));
          VPackSlice revision = revBuilder.slice(); //revision as string
          localHash ^= docKeySlice.hashString();
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
          Result res = syncChunkRocksDB(syncer, &trx, stats, keysId, currentChunkId,
                                        lowKey, highKey, markers);
          if (!res.ok()) {
            THROW_ARANGO_EXCEPTION(res);
          }
        }
        currentChunkId++;
        if (currentChunkId < numChunks) {
          resetChunk();
          // key is higher than upper bound, recheck the current document
          if (cmp2 > 0) {
            compareChunk(docKey, docRev);
          }
        }
      }
    }; //compare chunk - end

    LogicalCollection* coll = trx.documentCollection();
    auto iterator = createPrimaryIndexIterator(&trx, coll);
    iterator.next(
        [&](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
          std::string docKey = RocksDBKey::primaryKey(rocksKey).toString();
          TRI_voc_rid_t docRev;
          if(!RocksDBValue::revisionId(rocksValue, docRev)){
            // for collections that do not have the revisionId in the value
            auto documentId = RocksDBValue::documentId(rocksValue); // we want probably to do this instead
            if(col->readDocument(&trx, documentId, mmdr) == false) {
              TRI_ASSERT(false);
              return;
            }
            VPackSlice doc(mmdr.vpack());
            docRev = TRI_ExtractRevisionId(doc);
          }
          compareChunk(docKey, docRev);
        },
        std::numeric_limits<std::uint64_t>::max()); // no limit on documents

    // we might have missed chunks, if the keys don't exist at all locally
    while (currentChunkId < numChunks) {
      Result res = syncChunkRocksDB(syncer, &trx, stats, keysId, currentChunkId, lowKey,
                                    highKey, markers);
      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }
      currentChunkId++;
      if (currentChunkId < numChunks) {
        resetChunk();
      }
    }

    res = trx.commit();
    if (!res.ok()) {
      return res;
    }
  }

  {
    std::string progress =
        std::string("incremental sync statistics: ") + 
        "keys requests: " + std::to_string(stats.numKeysRequests) + ", " +
        "docs requests: " + std::to_string(stats.numDocsRequests) + ", " +
        "number of documents requested: " + std::to_string(stats.numDocsRequested) + ", " +
        "number of documents inserted: " + std::to_string(stats.numDocsInserted) + ", " +
        "number of documents removed: " + std::to_string(stats.numDocsRemoved) + ", " +
        "waited for initial: " + std::to_string(stats.waitedForInitial) + " s, " +
        "waited for keys: " + std::to_string(stats.waitedForKeys) + " s, " +
        "waited for docs: " + std::to_string(stats.waitedForDocs) + " s";
    syncer.setProgress(progress);
  }

  return Result();
}
}
