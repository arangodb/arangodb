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
#include "Basics/system-functions.h"
#include "Indexes/IndexIterator.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "Replication/utilities.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBIterators.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

// remove all keys that are below first remote key or beyond last remote key
Result removeKeysOutsideRange(VPackSlice chunkSlice, LogicalCollection* coll,
                              OperationOptions& options,
                              InitialSyncerIncrementalSyncStats& stats) {
  size_t const numChunks = chunkSlice.length();

  if (numChunks == 0) {
    // no need to do anything
    return Result();
  }

  SingleCollectionTransaction trx(transaction::StandaloneContext::Create(coll->vocbase()),
                                  *coll, AccessMode::Type::EXCLUSIVE);

  trx.addHint(transaction::Hints::Hint::NO_INDEXING);
  // turn on intermediate commits as the number of keys to delete can be huge
  // here
  trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);

  RocksDBCollection* physical = static_cast<RocksDBCollection*>(coll->getPhysical());

  Result res = trx.begin();

  if (!res.ok()) {
    return Result(res.errorNumber(),
                  std::string("unable to start transaction: ") + res.errorMessage());
  }

  VPackSlice chunk = chunkSlice.at(0);

  TRI_ASSERT(chunk.isObject());
  auto lowSlice = chunk.get("low");
  TRI_ASSERT(lowSlice.isString());
  arangodb::velocypack::StringRef lowRef(lowSlice);

  // last high
  chunk = chunkSlice.at(numChunks - 1);
  TRI_ASSERT(chunk.isObject());

  auto highSlice = chunk.get("high");
  TRI_ASSERT(highSlice.isString());
  arangodb::velocypack::StringRef highRef(highSlice);

  auto iterator = createPrimaryIndexIterator(&trx, coll);

  VPackBuilder builder;
  ManagedDocumentResult mdr;

  // remove everything from the beginning of the key range until the lowest
  // remote key
  iterator.next(
      [&](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
        arangodb::velocypack::StringRef docKey(RocksDBKey::primaryKey(rocksKey));
        if (docKey.compare(lowRef) < 0) {
          builder.clear();
          builder.add(velocypack::ValuePair(docKey.data(), docKey.size(),
                                            velocypack::ValueType::String));
          auto r = physical->remove(trx, builder.slice(), mdr, options);

          if (r.fail() && r.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
            // ignore not found, we remove conflicting docs ahead of time
            THROW_ARANGO_EXCEPTION(r);
          }

          if (r.ok()) {
            ++stats.numDocsRemoved;
          }
          // continue iteration
          return true;
        }

        // stop iteration
        return false;
      },
      std::numeric_limits<std::uint64_t>::max());

  // remove everything from the highest remote key until the end of the key
  // range
  auto index = coll->lookupIndex(IndexId::primary());  // RocksDBCollection->primaryIndex() is private
  TRI_ASSERT(index->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  auto primaryIndex = static_cast<RocksDBPrimaryIndex*>(index.get());

  RocksDBKeyLeaser key(&trx);
  key->constructPrimaryIndexValue(primaryIndex->objectId(), highRef);
  iterator.seek(key->string());

  iterator.next(
      [&](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
        arangodb::velocypack::StringRef docKey(RocksDBKey::primaryKey(rocksKey));
        if (docKey.compare(highRef) > 0) {
          builder.clear();
          builder.add(velocypack::ValuePair(docKey.data(), docKey.size(),
                                            velocypack::ValueType::String));
          auto r = physical->remove(trx, builder.slice(), mdr, options);

          if (r.fail() && r.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
            // ignore not found, we remove conflicting docs ahead of time
            THROW_ARANGO_EXCEPTION(r);
          }

          if (r.ok()) {
            ++stats.numDocsRemoved;
          }
        }

        // continue iteration until end
        return true;
      },
      std::numeric_limits<std::uint64_t>::max());

  return trx.commit();
}

Result syncChunkRocksDB(DatabaseInitialSyncer& syncer, SingleCollectionTransaction* trx,
                        InitialSyncerIncrementalSyncStats& stats,
                        std::string const& keysId, uint64_t chunkId,
                        std::string const& lowString, std::string const& highString,
                        std::vector<std::string> const& markers) {
  // first thing we do is extend the barrier's lifetime
  if (!syncer._state.isChildSyncer) {
    syncer._state.barrier.extend(syncer._state.connection);
  }

  std::string const baseUrl = replutils::ReplicationUrl + "/keys";
  TRI_voc_tick_t const chunkSize = 5000;
  LogicalCollection* coll = trx->documentCollection();
  std::string const& collectionName = coll->name();
  RocksDBCollection* physical = static_cast<RocksDBCollection*>(coll->getPhysical());
  OperationOptions options;
  options.silent = true;
  options.ignoreRevs = true;
  options.isRestore = true;
  options.indexOperationMode = Index::OperationMode::internal;
  options.waitForSync = false;
  options.validate = false;

  if (!syncer._state.leaderId.empty()) {
    options.isSynchronousReplicationFrom = syncer._state.leaderId;
  }

  LOG_TOPIC("295ed", TRACE, Logger::REPLICATION) << "syncing chunk. low: '" << lowString
                                        << "', high: '" << highString << "'";

  // no match
  // must transfer keys for non-matching range
  std::unique_ptr<httpclient::SimpleHttpResult> response;

  {
    std::string const url =
        baseUrl + "/" + keysId + "?type=keys&chunk=" + std::to_string(chunkId) +
        "&chunkSize=" + std::to_string(chunkSize) + "&low=" + lowString;

    syncer.setProgress(std::string("fetching keys chunk ") +
                       std::to_string(chunkId) + " from " + url);

    // time how long the request takes
    double t = TRI_microtime();

    syncer._state.connection.lease([&](httpclient::SimpleHttpClient* client) {
      response.reset(client->retryRequest(rest::RequestType::PUT, url, nullptr,
                                          0, replutils::createHeaders()));
    });

    stats.waitedForKeys += TRI_microtime() - t;
    ++stats.numKeysRequests;

    if (replutils::hasFailed(response.get())) {
      return replutils::buildHttpError(response.get(), url, syncer._state.connection);
    }
  }

  TRI_ASSERT(response != nullptr);

  VPackBuilder builder;
  Result r = replutils::parseResponse(builder, response.get());
  response.reset();  // not needed anymore

  if (r.fail()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      syncer._state.master.endpoint + ": " + r.errorMessage());
  }

  VPackSlice const responseBody = builder.slice();
  if (!responseBody.isArray()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      syncer._state.master.endpoint + ": response is no array");
  }

  size_t const numKeys = static_cast<size_t>(responseBody.length());
  if (numKeys == 0) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      syncer._state.master.endpoint +
                      ": response contains an empty chunk. Collection: " + collectionName +
                      " Chunk: " + std::to_string(chunkId));
  }
  TRI_ASSERT(numKeys > 0);

  // state for RocksDBCollection insert/replace/remove
  ManagedDocumentResult mdr, previous;

  transaction::BuilderLeaser keyBuilder(trx);
  std::vector<size_t> toFetch;
  size_t i = 0;
  size_t nextStart = 0;

  for (VPackSlice pair : VPackArrayIterator(responseBody)) {
    if (!pair.isArray() || pair.length() != 2) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    std::string("got invalid response from master at ") +
                        syncer._state.master.endpoint +
                        ": response key pair is no valid array");
    }

    // key
    VPackSlice const keySlice = pair.at(0);
    if (!keySlice.isString()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    std::string("got invalid response from master at ") +
                        syncer._state.master.endpoint +
                        ": response key is no string");
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
      std::string const& localKey = markers[nextStart];

      int res = keySlice.compareString(localKey);
      if (res > 0) {
        // we have a local key that is not present remotely
        keyBuilder->clear();
        keyBuilder->add(VPackValue(localKey));

        auto r = physical->remove(*trx, keyBuilder->slice(), mdr, options);

        if (r.fail() && r.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
          // ignore not found, we remove conflicting docs ahead of time
          return r;
        }

        if (r.ok()) {
          ++stats.numDocsRemoved;
        }

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
      TRI_voc_rid_t currentRevisionId = 0;
      if (!physical->lookupRevision(trx, keySlice, currentRevisionId)) {
        // key not found locally
        toFetch.emplace_back(i);
      } else {
        // key found locally. now compare revisions
        if (!pair.at(1).isEqualString(TRI_RidToString(currentRevisionId))) {
          // key found, but revision id differs
          toFetch.emplace_back(i);
        }
        ++nextStart;
      }
    }

    i++;
  }

  // delete all keys at end of the range
  while (nextStart < markers.size()) {
    std::string const& localKey = markers[nextStart];

    if (localKey.compare(highString) > 0) {
      // we have a local key that is not present remotely
      keyBuilder->clear();
      keyBuilder->add(VPackValue(localKey));

      auto r = physical->remove(*trx, keyBuilder->slice(), mdr, options);

      if (r.fail() && r.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
        // ignore not found, we remove conflicting docs ahead of time
        return r;
      }

      if (r.ok()) {
        ++stats.numDocsRemoved;
      }
    }
    ++nextStart;
  }

  if (toFetch.empty()) {
    // nothing to do
    return Result();
  }

  if (!syncer._state.isChildSyncer) {
    syncer._state.barrier.extend(syncer._state.connection);
  }

  LOG_TOPIC("48f94", TRACE, Logger::REPLICATION)
      << "will refetch " << toFetch.size() << " documents for this chunk";

  keyBuilder->clear();
  keyBuilder->openArray(false);
  for (auto const& it : toFetch) {
    keyBuilder->add(VPackValue(it));
  }
  keyBuilder->close();

  std::string const keyJsonString(keyBuilder->slice().toJson());

  size_t offsetInChunk = 0;

  while (true) {
    std::unique_ptr<httpclient::SimpleHttpResult> response;

    {
      std::string const url =
          baseUrl + "/" + keysId + "?type=docs&chunk=" + std::to_string(chunkId) +
          "&chunkSize=" + std::to_string(chunkSize) + "&low=" + lowString +
          "&offset=" + std::to_string(offsetInChunk);

      syncer.setProgress(std::string("fetching documents chunk ") +
                         std::to_string(chunkId) + " (" + std::to_string(toFetch.size()) +
                         " keys) for collection '" + collectionName + "' from " + url);

      double t = TRI_microtime();

      syncer._state.connection.lease([&](httpclient::SimpleHttpClient* client) {
        response.reset(client->retryRequest(rest::RequestType::PUT, url,
                                            keyJsonString.data(), keyJsonString.size(),
                                            replutils::createHeaders()));
      });

      stats.waitedForDocs += TRI_microtime() - t;
      stats.numDocsRequested += toFetch.size();
      ++stats.numDocsRequests;

      if (replutils::hasFailed(response.get())) {
        return replutils::buildHttpError(response.get(), url, syncer._state.connection);
      }
    }

    TRI_ASSERT(response != nullptr);

    transaction::BuilderLeaser docsBuilder(trx);
    docsBuilder->clear();
    Result r = replutils::parseResponse(*docsBuilder.get(), response.get());

    if (r.fail()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    std::string("got invalid response from master at ") +
                        syncer._state.master.endpoint +
                        ": " + r.errorMessage());
    }

    VPackSlice const slice = docsBuilder->slice();
    if (!slice.isArray()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    std::string("got invalid response from master at ") +
                        syncer._state.master.endpoint +
                        ": response is no array");
    }

    size_t foundLength = slice.length();

    double t = TRI_microtime();
    for (VPackSlice it : VPackArrayIterator(slice)) {
      if (it.isNull()) {
        continue;
      }

      if (!it.isObject()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          syncer._state.master.endpoint +
                          ": document is no object");
      }

      VPackSlice const keySlice = it.get(StaticStrings::KeyString);

      if (!keySlice.isString()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          syncer._state.master.endpoint +
                          ": document key is invalid");
      }

      VPackSlice const revSlice = it.get(StaticStrings::RevString);

      if (!revSlice.isString()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          syncer._state.master.endpoint +
                          ": document revision is invalid");
      }

      auto removeConflict = [&](std::string const& conflictingKey) -> Result {
        keyBuilder->clear();
        keyBuilder->add(VPackValue(conflictingKey));

        auto res = physical->remove(*trx, keyBuilder->slice(), mdr, options);

        if (res.ok()) {
          ++stats.numDocsRemoved;
        }

        return res;
      };

      std::pair<LocalDocumentId, TRI_voc_rid_t> lookupResult;
      if (physical->lookupKey(trx, keySlice.stringRef(), lookupResult).is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
        // key does not yet exist
        // INSERT
        TRI_ASSERT(options.indexOperationMode == Index::OperationMode::internal);

        Result res = physical->insert(trx, it, mdr, options);
        
        if (res.fail()) {
          if (res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) &&
              res.errorMessage() > keySlice.copyString()) {
            // remove conflict and retry
            // errorMessage() is this case contains the conflicting key
            auto inner = removeConflict(res.errorMessage());
            if (inner.fail()) {
              return res;
            }

            options.indexOperationMode = Index::OperationMode::normal;
            res = physical->insert(trx, it, mdr, options);
            
            options.indexOperationMode = Index::OperationMode::internal;
            if (res.fail()) {
              return res;
            }
            // fall-through
          } else {
            int errorNumber = res.errorNumber();
            res.reset(errorNumber, std::string(TRI_errno_string(errorNumber)) + ": " + res.errorMessage());
            return res;
          }
        }

        ++stats.numDocsInserted;
      } else {
        // REPLACE
        TRI_ASSERT(options.indexOperationMode == Index::OperationMode::internal);

        Result res = physical->replace(trx, it, mdr, options, previous);
        
        if (res.fail()) {
          if (res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) &&
              res.errorMessage() > keySlice.copyString()) {
            // remove conflict and retry
            // errorMessage() is this case contains the conflicting key
            auto inner = removeConflict(res.errorMessage());
            if (inner.fail()) {
              return res;
            }
            options.indexOperationMode = Index::OperationMode::normal;
            res = physical->replace(trx, it, mdr, options, previous);
            
            options.indexOperationMode = Index::OperationMode::internal;
            if (res.fail()) {
              return res;
            }
            // fall-through
          } else {
            int errorNumber = res.errorNumber();
            res.reset(errorNumber, std::string(TRI_errno_string(errorNumber)) + ": " + res.errorMessage());
            return res;
          }
        }
      }
    }
    stats.waitedForInsertions += TRI_microtime() - t;

    if (foundLength >= toFetch.size()) {
      break;
    }

    // try again in next round
    offsetInChunk = foundLength;
  }

  return Result();
}

Result handleSyncKeysRocksDB(DatabaseInitialSyncer& syncer,
                             arangodb::LogicalCollection* col, std::string const& keysId) {
  double const startTime = TRI_microtime();

  syncer.setProgress(std::string("collecting local keys for collection '") +
                     col->name() + "'");

  if (syncer.isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  if (!syncer._state.isChildSyncer) {
    syncer._batch.extend(syncer._state.connection, syncer._progress, syncer._state.syncerId);
    syncer._state.barrier.extend(syncer._state.connection);
  }

  TRI_voc_tick_t const chunkSize = 5000;
  std::string const baseUrl = replutils::ReplicationUrl + "/keys";

  InitialSyncerIncrementalSyncStats stats;

  std::unique_ptr<httpclient::SimpleHttpResult> response;

  {
    std::string const url =
        baseUrl + "/" + keysId + "?chunkSize=" + std::to_string(chunkSize);

    syncer.setProgress(
        std::string("fetching remote keys chunks for collection '") +
        col->name() + "' from " + url);

    auto const headers = replutils::createHeaders();

    double t = TRI_microtime();

    syncer._state.connection.lease([&](httpclient::SimpleHttpClient* client) {
      response.reset(client->retryRequest(rest::RequestType::GET, url, nullptr, 0, headers));
    });

    stats.waitedForInitial += TRI_microtime() - t;

    if (replutils::hasFailed(response.get())) {
      return replutils::buildHttpError(response.get(), url, syncer._state.connection);
    }
  }

  TRI_ASSERT(response != nullptr);

  VPackBuilder builder;
  Result r = replutils::parseResponse(builder, response.get());

  if (r.fail()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      syncer._state.master.endpoint + ": " + r.errorMessage());
  }

  VPackSlice const chunkSlice = builder.slice();

  if (!chunkSlice.isArray()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      syncer._state.master.endpoint + ": response is no array");
  }

  OperationOptions options;
  options.silent = true;
  options.ignoreRevs = true;
  options.isRestore = true;
  options.waitForSync = false;
  options.validate = false;

  if (!syncer._state.leaderId.empty()) {
    options.isSynchronousReplicationFrom = syncer._state.leaderId;
  }

  {
    // remove all keys that are below first remote key or beyond last remote key
    Result res = removeKeysOutsideRange(chunkSlice, col, options, stats);

    if (res.fail()) {
      return res;
    }
  }

  size_t const numChunks = static_cast<size_t>(chunkSlice.length());
  uint64_t const numberDocumentsRemovedBeforeStart = stats.numDocsRemoved;

  {
    if (syncer.isAborted()) {
      return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }

    SingleCollectionTransaction trx(transaction::StandaloneContext::Create(
                                        syncer.vocbase()),
                                    *col, AccessMode::Type::EXCLUSIVE);

    trx.addHint(transaction::Hints::Hint::NO_INDEXING);
    // turn on intermediate commits as the number of operations can be huge here
    trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);

    Result res = trx.begin();

    if (!res.ok()) {
      return Result(res.errorNumber(),
                    std::string("unable to start transaction: ") + res.errorMessage());
    }

    // We do not take responsibility for the index.
    // The LogicalCollection is protected by trx.
    // Neither it nor its indexes can be invalidated

    RocksDBCollection* physical = static_cast<RocksDBCollection*>(col->getPhysical());
    size_t currentChunkId = 0;

    std::string lowKey;
    std::string highKey;
    std::string hashString;
    uint64_t localHash = 0x012345678;
    // chunk keys
    std::vector<std::string> markers;
    bool foundLowKey = false;

    auto resetChunk = [&]() -> void {
      if (!syncer._state.isChildSyncer) {
        syncer._batch.extend(syncer._state.connection, syncer._progress, syncer._state.syncerId);
        syncer._state.barrier.extend(syncer._state.connection);
      }

      syncer.setProgress(std::string("processing keys chunk ") + std::to_string(currentChunkId) +
          " of " + std::to_string(numChunks) + " for collection '" + col->name() + "'");

      // read remote chunk
      TRI_ASSERT(chunkSlice.isArray());
      TRI_ASSERT(chunkSlice.length() > 0);  // chunkSlice.at will throw otherwise
      VPackSlice chunk = chunkSlice.at(currentChunkId);
      if (!chunk.isObject()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_REPLICATION_INVALID_RESPONSE,
            std::string("got invalid response from master at ") +
                syncer._state.master.endpoint + ": chunk is no object");
      }

      VPackSlice const lowSlice = chunk.get("low");
      VPackSlice const highSlice = chunk.get("high");
      VPackSlice const hashSlice = chunk.get("hash");
      if (!lowSlice.isString() || !highSlice.isString() || !hashSlice.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_REPLICATION_INVALID_RESPONSE,
            std::string("got invalid response from master at ") +
                syncer._state.master.endpoint +
                ": chunks in response have an invalid format");
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

    // tempBuilder is reused inside compareChunk
    VPackBuilder tempBuilder;

    std::function<void(std::string, std::uint64_t)> compareChunk =
        [&trx, &physical, &options, &foundLowKey, &markers, &localHash,
         &hashString, &syncer, &currentChunkId, &numChunks, &keysId,
         &resetChunk, &compareChunk, &lowKey, &highKey, &tempBuilder,
         &stats](std::string const& docKey, std::uint64_t docRev) {
          int cmp1 = docKey.compare(lowKey);

          if (cmp1 < 0) {
            // smaller values than lowKey mean they don't exist remotely
            tempBuilder.clear();
            tempBuilder.add(VPackValue(docKey));

            ManagedDocumentResult previous;
            auto r = physical->remove(trx, tempBuilder.slice(), previous, options);

            if (r.fail() && r.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
              // ignore not found, we remove conflicting docs ahead of time
              THROW_ARANGO_EXCEPTION(r);
            }

            if (r.ok()) {
              ++stats.numDocsRemoved;
            }

            return;
          }

          bool rangeUnequal = false;
          bool nextChunk = false;
          int cmp2 = docKey.compare(highKey);

          if (cmp1 >= 0 && cmp2 <= 0) {
            // we only need to hash if we are in the range
            if (cmp1 == 0) {
              foundLowKey = true;
            }

            markers.emplace_back(docKey);
            // don't bother hashing if we have't found lower key
            if (foundLowKey) {
              tempBuilder.clear();
              tempBuilder.add(VPackValue(docKey));
              localHash ^= tempBuilder.slice().hashString();

              tempBuilder.clear();
              // use a temporary char buffer for building to rid string
              char ridBuffer[21];
              tempBuilder.add(TRI_RidToValuePair(docRev, &ridBuffer[0]));
              localHash ^= tempBuilder.slice().hashString();  // revision as string

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
              Result res = syncChunkRocksDB(syncer, &trx, stats, keysId,
                                            currentChunkId, lowKey, highKey, markers);
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
        };  // compare chunk - end

    uint64_t documentsFound = 0;
    auto iterator = createPrimaryIndexIterator(&trx, col);
    iterator.next(
        [&](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
          ++documentsFound;
          std::string docKey = RocksDBKey::primaryKey(rocksKey).toString();
          TRI_voc_rid_t docRev;
          if (!RocksDBValue::revisionId(rocksValue, docRev)) {
            // for collections that do not have the revisionId in the value
            auto documentId = RocksDBValue::documentId(rocksValue);  // we want probably to do this instead
            col->readDocumentWithCallback(&trx, documentId,
                                          [&docRev](LocalDocumentId const&, VPackSlice doc) {
                                            docRev = TRI_ExtractRevisionId(doc);
                                            return true;
                                          });
          }
          compareChunk(docKey, docRev);
          return true;
        },
        std::numeric_limits<std::uint64_t>::max());  // no limit on documents

    // we might have missed chunks, if the keys don't exist at all locally
    while (currentChunkId < numChunks) {
      Result res = syncChunkRocksDB(syncer, &trx, stats, keysId, currentChunkId,
                                    lowKey, highKey, markers);
      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }
      currentChunkId++;
      if (currentChunkId < numChunks) {
        resetChunk();
      }
    }

    {
      uint64_t numberDocumentsAfterSync =
          documentsFound + stats.numDocsInserted -
          (stats.numDocsRemoved - numberDocumentsRemovedBeforeStart);
      uint64_t numberDocumentsDueToCounter =
          col->numberDocuments(&trx, transaction::CountType::Normal);
      syncer.setProgress(
          std::string("number of remaining documents in collection '") +
          col->name() + "': " + std::to_string(numberDocumentsAfterSync) +
          ", number of documents due to collection count: " +
          std::to_string(numberDocumentsDueToCounter));

      if (numberDocumentsAfterSync != numberDocumentsDueToCounter) {
        LOG_TOPIC("118bd", WARN, Logger::REPLICATION)
            << "number of remaining documents in collection '" + col->name() +
                   "' is " + std::to_string(numberDocumentsAfterSync) +
                   " and differs from number of documents returned by "
                   "collection count " +
                   std::to_string(numberDocumentsDueToCounter);

        // patch the document counter of the collection and the transaction
        int64_t diff = static_cast<int64_t>(numberDocumentsAfterSync) -
                       static_cast<int64_t>(numberDocumentsDueToCounter);
        auto seq = rocksutils::latestSequenceNumber();
        static_cast<RocksDBCollection*>(trx.documentCollection()->getPhysical())
            ->meta().adjustNumberDocuments(seq, /*revId*/0, diff);
      }
    }

    res = trx.commit();

    if (res.fail()) {
      return res;
    }
  }

  syncer.setProgress(
      std::string("incremental sync statistics for collection '") + col->name() +
      "': " + "keys requests: " + std::to_string(stats.numKeysRequests) + ", " +
      "docs requests: " + std::to_string(stats.numDocsRequests) + ", " +
      "number of documents requested: " + std::to_string(stats.numDocsRequested) +
      ", " + "number of documents inserted: " + std::to_string(stats.numDocsInserted) +
      ", " + "number of documents removed: " + std::to_string(stats.numDocsRemoved) +
      ", " + "waited for initial: " + std::to_string(stats.waitedForInitial) +
      " s, " + "waited for keys: " + std::to_string(stats.waitedForKeys) +
      " s, " + "waited for docs: " + std::to_string(stats.waitedForDocs) +
      " s, " + "waited for insertions: " + std::to_string(stats.waitedForInsertions) +
      " s, " + "total time: " + std::to_string(TRI_microtime() - startTime) +
      " s");

  return Result();
}
}  // namespace arangodb
