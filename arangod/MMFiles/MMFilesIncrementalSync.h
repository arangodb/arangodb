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
/// @author Simon Graetzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_INCREMENTAL_SYNC_H
#define ARANGOD_MMFILES_INCREMENTAL_SYNC_H 1

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "MMFiles/MMFilesDitch.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesPrimaryIndex.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "Replication/utilities.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
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

////////////////////////////////////////////////////////////////////////////////
/// @brief incrementally fetch data from a collection
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
////////////////////////////////////////////////////////////////////////////////
/// @brief performs a binary search for the given key in the markers vector
////////////////////////////////////////////////////////////////////////////////

static bool BinarySearch(std::vector<uint8_t const*> const& markers,
                         std::string const& key, size_t& position) {
  TRI_ASSERT(!markers.empty());

  size_t l = 0;
  size_t r = markers.size() - 1;

  while (true) {
    // determine midpoint
    position = l + ((r - l) / 2);

    TRI_ASSERT(position < markers.size());
    VPackSlice const otherSlice(markers.at(position));
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

static bool FindRange(std::vector<uint8_t const*> const& markers,
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

Result handleSyncKeysMMFiles(arangodb::DatabaseInitialSyncer& syncer,
                             arangodb::LogicalCollection* coll,
                             std::string const& keysId) {
  double const startTime = TRI_microtime();

  syncer.setProgress(std::string("collecting local keys for collection '") + coll->name() + "'");
  
  if (syncer.isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }
  
  InitialSyncerIncrementalSyncStats stats;

  // fetch all local keys from primary index
  std::vector<uint8_t const*> markers;

  MMFilesDocumentDitch* ditch = nullptr;

  // acquire a replication ditch so no datafiles are thrown away from now on
  // note: the ditch also protects against unloading the collection
  {
    SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(syncer.vocbase()),
      *coll,
      AccessMode::Type::READ
    );
    Result res = trx.begin();

    if (!res.ok()) {
      return Result(res.errorNumber(),
                    std::string("unable to start transaction (") +
                        std::string(__FILE__) + std::string(":") +
                        std::to_string(__LINE__) + std::string("): ") +
                        res.errorMessage());
    }

    ditch = arangodb::MMFilesCollection::toMMFilesCollection(coll)
                ->ditches()
                ->createMMFilesDocumentDitch(false, __FILE__, __LINE__);

    if (ditch == nullptr) {
      return Result(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  TRI_ASSERT(ditch != nullptr);

  TRI_DEFER(arangodb::MMFilesCollection::toMMFilesCollection(coll)
                ->ditches()
                ->freeDitch(ditch));

  {
    SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(syncer.vocbase()),
      *coll,
      AccessMode::Type::READ
    );
    Result res = trx.begin();

    if (!res.ok()) {
      return Result(res.errorNumber(),
                    std::string("unable to start transaction (") +
                        std::string(__FILE__) + std::string(":") +
                        std::to_string(__LINE__) + std::string("): ") +
                        res.errorMessage());
    }

    // We do not take responsibility for the index.
    // The LogicalCollection is protected by trx.
    // Neither it nor it's indexes can be invalidated

    markers.reserve(trx.documentCollection()->numberDocuments(&trx, transaction::CountType::Normal));

    uint64_t iterations = 0;
    ManagedDocumentResult mdr;
    trx.invokeOnAllElements(
        trx.name(), [&syncer, &trx, &mdr, &markers,
                     &iterations](LocalDocumentId const& token) {
          if (trx.documentCollection()->readDocument(&trx, token, mdr)) {
            markers.emplace_back(mdr.vpack());

            if (++iterations % 10000 == 0) {
              if (syncer.isAborted()) {
                return false;
              }
            }
          }
          return true;
        });

    if (syncer.isAborted()) {
      return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }

    if (!syncer._state.isChildSyncer) {
      syncer._batch.extend(syncer._state.connection, syncer._progress);
      syncer._state.barrier.extend(syncer._state.connection);
    }

    syncer.setProgress(std::string("sorting ") + std::to_string(markers.size()) +
                           " local key(s) for collection '" + coll->name() + "'");

    // sort all our local keys
    std::sort(
        markers.begin(), markers.end(),
        [](uint8_t const* lhs, uint8_t const* rhs) -> bool {
          VPackSlice const l(lhs);
          VPackSlice const r(rhs);

          VPackValueLength lLength, rLength;
          char const* lKey = l.get(StaticStrings::KeyString).getString(lLength);
          char const* rKey = r.get(StaticStrings::KeyString).getString(rLength);

          size_t const length =
              static_cast<size_t>(lLength < rLength ? lLength : rLength);
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

  if (syncer.isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  if (!syncer._state.isChildSyncer) {
    syncer._batch.extend(syncer._state.connection, syncer._progress);
    syncer._state.barrier.extend(syncer._state.connection);
  }

  std::vector<size_t> toFetch;

  TRI_voc_tick_t const chunkSize = 5000;
  std::string const baseUrl = replutils::ReplicationUrl + "/keys";

  std::string url =
      baseUrl + "/" + keysId + "?chunkSize=" + std::to_string(chunkSize);
  syncer.setProgress(std::string("fetching remote keys chunks for collection '") + coll->name() + "' from " + url);
    
  double t = TRI_microtime();

  std::unique_ptr<httpclient::SimpleHttpResult> response;
  syncer._state.connection.lease([&](httpclient::SimpleHttpClient* client) {
    response.reset(client->retryRequest(rest::RequestType::GET, url, nullptr, 0));
  });
    
  stats.waitedForInitial += TRI_microtime() - t;

  if (replutils::hasFailed(response.get())) {
    return buildHttpError(response.get(), url, syncer._state.connection);
  }

  TRI_ASSERT(response != nullptr);
  
  VPackBuilder builder;
  Result r = replutils::parseResponse(builder, response.get());

  if (r.fail()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      syncer._state.master.endpoint +
                      ": invalid response is no array");
  }

  VPackSlice const slice = builder.slice();

  if (!slice.isArray()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      syncer._state.master.endpoint + ": response is no array");
  }

  OperationOptions options;
  options.silent = true;
  options.ignoreRevs = true;
  options.isRestore = true;
  options.indexOperationMode = Index::OperationMode::internal;
  if (!syncer._state.leaderId.empty()) {
    options.isSynchronousReplicationFrom = syncer._state.leaderId;
  }

  VPackBuilder keyBuilder;
  size_t const n = static_cast<size_t>(slice.length());

  // remove all keys that are below first remote key or beyond last remote key
  if (n > 0) {
    // first chunk
    SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(syncer.vocbase()),
      *coll,
      AccessMode::Type::WRITE
    );

    trx.addHint(
        transaction::Hints::Hint::RECOVERY);  // to turn off waitForSync!

    Result res = trx.begin();

    if (!res.ok()) {
      return Result(
          res.errorNumber(),
          std::string("unable to start transaction: ") + res.errorMessage());
    }

    VPackSlice chunk = slice.at(0);

    TRI_ASSERT(chunk.isObject());
    auto lowSlice = chunk.get("low");
    TRI_ASSERT(lowSlice.isString());

    std::string const lowKey(lowSlice.copyString());

    for (size_t i = 0; i < markers.size(); ++i) {
      VPackSlice const k(markers[i]);

      std::string const key(k.get(StaticStrings::KeyString).copyString());

      if (key.compare(lowKey) >= 0) {
        break;
      }

      keyBuilder.clear();
      keyBuilder.openObject();
      keyBuilder.add(StaticStrings::KeyString, VPackValue(key));
      keyBuilder.close();

      trx.remove(coll->name(), keyBuilder.slice(), options);
      ++stats.numDocsRemoved;
    }

    // last high
    chunk = slice.at(n - 1);
    TRI_ASSERT(chunk.isObject());

    auto highSlice = chunk.get("high");
    TRI_ASSERT(highSlice.isString());

    std::string const highKey(highSlice.copyString());

    for (size_t i = markers.size(); i >= 1; --i) {
      VPackSlice const k(markers[i - 1]);

      std::string const key(k.get(StaticStrings::KeyString).copyString());

      if (key.compare(highKey) <= 0) {
        break;
      }

      keyBuilder.clear();
      keyBuilder.openObject();
      keyBuilder.add(StaticStrings::KeyString, VPackValue(key));
      keyBuilder.close();

      trx.remove(coll->name(), keyBuilder.slice(), options);
      ++stats.numDocsRemoved;
    }

    trx.commit();
  }

  size_t nextStart = 0;

  // now process each chunk
  for (size_t i = 0; i < n; ++i) {
    if (syncer.isAborted()) {
      return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }

    SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(syncer.vocbase()),
      *coll,
      AccessMode::Type::WRITE
    );

    trx.addHint(
        transaction::Hints::Hint::RECOVERY);  // to turn off waitForSync!

    Result res = trx.begin();

    if (!res.ok()) {
      return Result(
          res.errorNumber(),
          std::string("unable to start transaction: ") + res.errorMessage());
    }

    trx.pinData(coll->id());  // will throw when it fails

    // We do not take responsibility for the index.
    // The LogicalCollection is protected by trx.
    // Neither it nor it's indexes can be invalidated

    ManagedDocumentResult mdr;

    auto physical = static_cast<MMFilesCollection*>(
        trx.documentCollection()->getPhysical());
    auto idx = physical->primaryIndex();

    size_t const currentChunkId = i;
    syncer.setProgress(std::string("processing keys chunk ") + std::to_string(currentChunkId) +
                       " for collection '" + coll->name() + "'");

    if (!syncer._state.isChildSyncer) {
      syncer._batch.extend(syncer._state.connection, syncer._progress);
      syncer._state.barrier.extend(syncer._state.connection);
    }

    // read remote chunk
    VPackSlice chunk = slice.at(i);

    if (!chunk.isObject()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    std::string("got invalid response from master at ") +
                        syncer._state.master.endpoint + ": chunk is no object");
    }

    VPackSlice const lowSlice = chunk.get("low");
    VPackSlice const highSlice = chunk.get("high");
    VPackSlice const hashSlice = chunk.get("hash");

    if (!lowSlice.isString() || !highSlice.isString() ||
        !hashSlice.isString()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    std::string("got invalid response from master at ") +
                        syncer._state.master.endpoint +
                        ": chunks in response have an invalid format");
    }

    std::string const lowString = lowSlice.copyString();
    std::string const highString = highSlice.copyString();

    size_t localFrom;
    size_t localTo;
    bool match = FindRange(markers, lowString, highString, localFrom, localTo);

    if (match) {
      // now must hash the range
      uint64_t hash = 0x012345678;

      for (size_t i = localFrom; i <= localTo; ++i) {
        TRI_ASSERT(i < markers.size());
        VPackSlice const current(markers.at(i));
        hash ^= current.get(StaticStrings::KeyString).hashString();
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
      std::string url = baseUrl + "/" + keysId +
                        "?type=keys&chunk=" + std::to_string(i) +
                        "&chunkSize=" + std::to_string(chunkSize);
      syncer.setProgress(std::string("fetching keys chunk ") + std::to_string(currentChunkId) +
                         " for collection '" + coll->name() + "' from " + url);
    
      double t = TRI_microtime();
      
      std::unique_ptr<httpclient::SimpleHttpResult> response;
      syncer._state.connection.lease([&](httpclient::SimpleHttpClient* client) {
        response.reset(client->retryRequest(rest::RequestType::PUT, url, nullptr, 0));
      });
    
      stats.waitedForKeys += TRI_microtime() - t;
      ++stats.numKeysRequests;
      
      if (replutils::hasFailed(response.get())) {
        return buildHttpError(response.get(), url, syncer._state.connection);
      }

      TRI_ASSERT(response != nullptr);
      
      VPackBuilder builder;
      Result r = replutils::parseResponse(builder, response.get());

      if (r.fail()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          syncer._state.master.endpoint +
                          ": response is no array");
      }

      VPackSlice const slice = builder.slice();
      if (!slice.isArray()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          syncer._state.master.endpoint +
                          ": response is no array");
      }

      // delete all keys at start of the range
      while (nextStart < markers.size()) {
        VPackSlice const keySlice(markers[nextStart]);
        std::string const localKey(
            keySlice.get(StaticStrings::KeyString).copyString());

        if (localKey.compare(lowString) < 0) {
          // we have a local key that is not present remotely
          keyBuilder.clear();
          keyBuilder.openObject();
          keyBuilder.add(StaticStrings::KeyString, VPackValue(localKey));
          keyBuilder.close();

          trx.remove(coll->name(), keyBuilder.slice(), options);
          ++stats.numDocsRemoved;
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
          continue;
        }

        std::string const keyString = keySlice.copyString();

        bool mustRefetch = false;

        while (nextStart < markers.size()) {
          VPackSlice const localKeySlice(markers[nextStart]);
          std::string const localKey(
              localKeySlice.get(StaticStrings::KeyString).copyString());

          // compare local with remote key
          int res = localKey.compare(keyString);

          if (res < 0) {
            // we have a local key that is not present remotely
            keyBuilder.clear();
            keyBuilder.openObject();
            keyBuilder.add(StaticStrings::KeyString, VPackValue(localKey));
            keyBuilder.close();

            trx.remove(coll->name(), keyBuilder.slice(), options);
            ++stats.numDocsRemoved;
            ++nextStart;
          } else if (res == 0) {
            // key match
            break;
          } else {
            // a remotely present key that is not present locally
            TRI_ASSERT(res > 0);
            mustRefetch = true;
            break;
          }
        }

        if (mustRefetch) {
          toFetch.emplace_back(i);
        } else {
          MMFilesSimpleIndexElement element = idx->lookupKey(&trx, keySlice);

          if (!element) {
            // key not found locally
            toFetch.emplace_back(i);
          } else {
            TRI_voc_rid_t currentRevisionId = 0;
            if (physical->readDocument(&trx, element.localDocumentId(), mdr)) {
              currentRevisionId = transaction::helpers::extractRevFromDocument(
                  VPackSlice(mdr.vpack()));
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
      }

      // calculate next starting point
      if (!markers.empty()) {
        BinarySearch(markers, highString, nextStart);

        while (nextStart < markers.size()) {
          VPackSlice const localKeySlice(markers[nextStart]);
          std::string const localKey(
              localKeySlice.get(StaticStrings::KeyString).copyString());

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
        for (auto const& it : toFetch) {
          keysBuilder.add(VPackValue(it));
        }
        keysBuilder.close();

        std::string const keyJsonString(keysBuilder.slice().toJson());

        size_t offsetInChunk = 0;

        while (true) {
          std::string url = baseUrl + "/" + keysId + "?type=docs&chunk=" +
                            std::to_string(currentChunkId) +
                            "&chunkSize=" + std::to_string(chunkSize) +
                            "&offset=" + std::to_string(offsetInChunk);
          syncer.setProgress(std::string("fetching documents chunk ") +
                             std::to_string(currentChunkId) + " for collection '" +
                             coll->name() + "' from " + url);
      
          double t = TRI_microtime();

          std::unique_ptr<httpclient::SimpleHttpResult> response;
          syncer._state.connection.lease([&](httpclient::SimpleHttpClient* client) {
            response.reset(client->retryRequest(rest::RequestType::PUT, url, keyJsonString.c_str(),
                                                keyJsonString.size()));
          });
      
          stats.waitedForDocs += TRI_microtime() - t;
          stats.numDocsRequested += toFetch.size();
          ++stats.numDocsRequests;
          
          if (replutils::hasFailed(response.get())) {
            return buildHttpError(response.get(), url, syncer._state.connection);
          }
          
          TRI_ASSERT(response != nullptr);

          VPackBuilder builder;
          Result r = replutils::parseResponse(builder, response.get());

          if (r.fail()) {
            return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                          std::string("got invalid response from master at ") +
                              syncer._state.master.endpoint +
                              ": response is no array");
          }

          VPackSlice const slice = builder.slice();
          if (!slice.isArray()) {
            return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                          std::string("got invalid response from master at ") +
                              syncer._state.master.endpoint +
                              ": response is no array");
          }

          size_t foundLength = slice.length();

          for (auto const& it : VPackArrayIterator(slice)) {
            if (it.isNull()) {
              continue;
            }

            if (!it.isObject()) {
              return Result(
                  TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      syncer._state.master.endpoint +
                      ": document is no object");
            }

            VPackSlice const keySlice = it.get(StaticStrings::KeyString);

            if (!keySlice.isString()) {
              return Result(
                  TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      syncer._state.master.endpoint +
                      ": document key is invalid");
            }

            VPackSlice const revSlice = it.get(StaticStrings::RevString);

            if (!revSlice.isString()) {
              return Result(
                  TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      syncer._state.master.endpoint +
                      ": document revision is invalid");
            }

            MMFilesSimpleIndexElement element = idx->lookupKey(&trx, keySlice);

            auto removeConflict =
                [&](std::string const& conflictingKey) -> OperationResult {
              VPackBuilder conflict;
              conflict.add(VPackValue(conflictingKey));
              LocalDocumentId conflictId =
                  physical->lookupKey(&trx, conflict.slice());
              if (conflictId.isSet()) {
                ManagedDocumentResult mdr;
                bool success = physical->readDocument(&trx, conflictId, mdr);
                if (success) {
                  VPackSlice conflictingKey(mdr.vpack());
                  return trx.remove(coll->name(), conflictingKey, options);
                }
              }
              return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
            };

            if (!element) {
              // INSERT
              OperationResult opRes = trx.insert(coll->name(), it, options);
              if (opRes.fail()) {
                if (opRes.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) &&
                    opRes.errorMessage() > keySlice.copyString()) {
                  // remove conflict and retry
                  auto inner = removeConflict(opRes.errorMessage());
                  if (inner.fail()) {
                    return opRes.result;
                  }
                  opRes = trx.insert(coll->name(), it, options);
                  if (opRes.fail()) {
                    return opRes.result;
                  }
                } else {
                  return opRes.result;
                }
              }
            } else {
              // REPLACE
              OperationResult opRes = trx.replace(coll->name(), it, options);
              if (opRes.fail()) {
                if (opRes.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) &&
                    opRes.errorMessage() > keySlice.copyString()) {
                  // remove conflict and retry
                  auto inner = removeConflict(opRes.errorMessage());
                  if (inner.fail()) {
                    return opRes.result;
                  }
                  opRes = trx.replace(coll->name(), it, options);
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
      }
    }

    res = trx.commit();

    if (!res.ok()) {
      return res;
    }
  }
  
  syncer.setProgress(
      std::string("incremental sync statistics for collection '") + coll->name() + "': " + 
      "keys requests: " + std::to_string(stats.numKeysRequests) + ", " +
      "docs requests: " + std::to_string(stats.numDocsRequests) + ", " +
      "number of documents requested: " + std::to_string(stats.numDocsRequested) + ", " +
      "number of documents inserted: " + std::to_string(stats.numDocsInserted) + ", " +
      "number of documents removed: " + std::to_string(stats.numDocsRemoved) + ", " +
      "waited for initial: " + std::to_string(stats.waitedForInitial) + " s, " +
      "waited for keys: " + std::to_string(stats.waitedForKeys) + " s, " +
      "waited for docs: " + std::to_string(stats.waitedForDocs) + " s, " + 
      "total time: " + std::to_string(TRI_microtime() - startTime) + " s");

  return Result();
}
}  // namespace arangodb

#endif
