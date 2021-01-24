////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "DatabaseTailingSyncer.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/system-functions.h"
#include "Logger/Logger.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/ReplicationMetricsFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Hints.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

namespace {
arangodb::velocypack::StringRef const cuidRef("cuid");
}

DatabaseTailingSyncer::DatabaseTailingSyncer(TRI_vocbase_t& vocbase,
                                             ReplicationApplierConfiguration const& configuration,
                                             TRI_voc_tick_t initialTick,
                                             bool useTick)
    : TailingSyncer(vocbase.replicationApplier(), configuration, initialTick, useTick),
      _vocbase(&vocbase),
      _queriedTranslations(false) {
  _state.vocbases.try_emplace(vocbase.name(), vocbase);

  if (configuration._database.empty()) {
    _state.databaseName = vocbase.name();
  }
}

std::shared_ptr<DatabaseTailingSyncer> DatabaseTailingSyncer::create(TRI_vocbase_t& vocbase,
                                                                     ReplicationApplierConfiguration const& configuration,
                                                                     TRI_voc_tick_t initialTick,
                                                                     bool useTick) {
  // enable make_shared on a class with a private constructor
  struct Enabler final : public DatabaseTailingSyncer {
    Enabler(TRI_vocbase_t& vocbase, ReplicationApplierConfiguration const& configuration, TRI_voc_tick_t initialTick, bool useTick)
      : DatabaseTailingSyncer(vocbase, configuration, initialTick, useTick) {}
  };

  return std::make_shared<Enabler>(vocbase, configuration, initialTick, useTick);
}

/// @brief save the current applier state
Result DatabaseTailingSyncer::saveApplierState() {
  auto rv = _applier->persistStateResult(false);
  if (rv.fail()) {
    THROW_ARANGO_EXCEPTION(rv);
  }
  return rv;
}

/// @brief finalize the synchronization of a collection by tailing the WAL
/// and filtering on the collection name until no more data is available
Result DatabaseTailingSyncer::syncCollectionCatchupInternal(arangodb::replutils::LeaderInfo const& leaderInfo,
                                                            std::string const& collectionName,
                                                            double timeout, bool hard,
                                                            TRI_voc_tick_t& until,
                                                            bool& didTimeout, char const* context) {
  didTimeout = false;

  setAborted(false);

  // fetch leader state just once
  TRI_ASSERT(!_state.isChildSyncer);
  TRI_ASSERT(!_state.leader.endpoint.empty());
  TRI_ASSERT(!_state.leader.serverId.isSet());
  TRI_ASSERT(_state.leader.engine.empty());
  TRI_ASSERT(_state.leader.version() == 0);
  
  TRI_ASSERT(!leaderInfo.endpoint.empty());
  TRI_ASSERT(leaderInfo.endpoint == _state.leader.endpoint);
  TRI_ASSERT(leaderInfo.serverId.isSet());
  TRI_ASSERT(!leaderInfo.engine.empty());
  TRI_ASSERT(leaderInfo.version() > 0);

  _state.leader.serverId = leaderInfo.serverId;;
  _state.leader.engine = leaderInfo.engine;
  _state.leader.majorVersion = leaderInfo.majorVersion;
  _state.leader.minorVersion = leaderInfo.minorVersion;
  // note: neither LeaderInfo::lastLogTick is not used during tailing syncing

  Result r;

  if (_state.leader.engine.empty()) {
    // fetch leader state only if we need to. this should not be needed, normally
    TRI_ASSERT(false);

    r = _state.leader.getState(_state.connection, /*_state.isChildSyncer*/ false, context);
    if (r.fail()) {
      return r;
    }
  } else {
    LOG_TOPIC("6c922", DEBUG, arangodb::Logger::REPLICATION)
      << "connected to leader at " << _state.leader.endpoint 
      << ", version " << _state.leader.majorVersion << "." << _state.leader.minorVersion 
      << ", context: " << context;
  }
  
  TRI_ASSERT(_state.leader.serverId.isSet());
  TRI_ASSERT(!_state.leader.engine.empty());
  TRI_ASSERT(_state.leader.version() > 0);

  // print extra info for debugging
  _state.applier._verbose = true;
  // we do not want to apply rename, create and drop collection operations
  _ignoreRenameCreateDrop = true;

  TRI_voc_tick_t fromTick = _initialTick;
  TRI_voc_tick_t lastScannedTick = fromTick;

  if (hard) {
    LOG_TOPIC("0e15c", DEBUG, Logger::REPLICATION) << "starting syncCollectionFinalize: " << collectionName
                                          << ", fromTick " << fromTick;
  } else {
    LOG_TOPIC("70711", DEBUG, Logger::REPLICATION) << "starting syncCollectionCatchup: " << collectionName
                                          << ", fromTick " << fromTick;
  }

  VPackBuilder builder; // will be recycled for every batch

  auto clock = std::chrono::steady_clock();
  auto startTime = clock.now();
    
  auto headers = replutils::createHeaders();

  while (true) {
    if (vocbase()->server().isStopping()) {
      return Result(TRI_ERROR_SHUTTING_DOWN);
    }

    std::string const url = tailingBaseUrl("tail") +
                            "chunkSize=" + StringUtils::itoa(_state.applier._chunkSize) +
                            "&from=" + StringUtils::itoa(fromTick) +
                            "&lastScanned=" + StringUtils::itoa(lastScannedTick) +
                            "&serverId=" + _state.localServerIdString +
                            "&collection=" + StringUtils::urlEncode(collectionName);

    // send request
    double start = TRI_microtime();
    std::unique_ptr<httpclient::SimpleHttpResult> response;
    _state.connection.lease([&](httpclient::SimpleHttpClient* client) {
      ++_stats.numTailingRequests;
      response.reset(client->request(rest::RequestType::GET, url, nullptr, 0, headers));
    });

    _stats.waitedForTailing += TRI_microtime() - start;

    if (replutils::hasFailed(response.get())) {
      until = fromTick;
      return replutils::buildHttpError(response.get(), url, _state.connection);
    }

    if (response->getHttpReturnCode() == 204) {
      // HTTP 204 No content: this means we are done
      TRI_ASSERT(r.ok());
      if (hard) {
        // now do a final sync-to-disk call. note that this can fail
        auto& engine = vocbase()->server().getFeature<EngineSelectorFeature>().engine();
        r = engine.flushWal(/*waitForSync*/ true, /*waitForCollector*/ false);
      }
      until = fromTick;
      return r;
    }
  
    if (response->hasContentLength()) {
      _stats.numTailingBytesReceived += response->getContentLength();
    }

    bool found;
    std::string header =
        response->getHeaderField(StaticStrings::ReplicationHeaderCheckMore, found);
    bool checkMore = false;
    if (found) {
      checkMore = StringUtils::boolean(header);
    }

    header = response->getHeaderField(StaticStrings::ReplicationHeaderLastScanned, found);
    if (found) {
      lastScannedTick = StringUtils::uint64(header);
    }

    header = response->getHeaderField(StaticStrings::ReplicationHeaderLastIncluded, found);
    if (!found) {
      until = fromTick;
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    std::string("got invalid response from leader at ") +
                        _state.leader.endpoint + ": required header " +
                        StaticStrings::ReplicationHeaderLastIncluded +
                        " is missing");
    }
    TRI_voc_tick_t lastIncludedTick = StringUtils::uint64(header);

    // was the specified from value included the result?
    bool fromIncluded = false;
    header = response->getHeaderField(StaticStrings::ReplicationHeaderFromPresent, found);
    if (found) {
      fromIncluded = StringUtils::boolean(header);
    }
    if (!fromIncluded && fromTick > 0) {
      until = fromTick;
      abortOngoingTransactions();
      ++_stats.numFollowTickNotPresent;
      return Result(
          TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT,
          std::string("required follow tick value '") + StringUtils::itoa(lastIncludedTick) +
              "' is not present (anymore?) on leader at " + _state.leader.endpoint +
              ". Last tick available on leader is '" + StringUtils::itoa(lastIncludedTick) +
              "'. It may be required to do a full resync and increase the "
              "number of historic logfiles on the leader.");
    }

    builder.clear();

    ApplyStats applyStats;
    uint64_t ignoreCount = 0;
    r = applyLog(response.get(), fromTick, applyStats, builder, ignoreCount);
    if (r.fail()) {
      until = fromTick;
      return r;
    }

    // update the tick from which we will fetch in the next round
    if (lastIncludedTick > fromTick) {
      fromTick = lastIncludedTick;
    } else if (lastIncludedTick == 0 && lastScannedTick > 0 && lastScannedTick > fromTick) {
      fromTick = lastScannedTick - 1;
    } else if (checkMore) {
      // we got the same tick again, this indicates we're at the end
      checkMore = false;
      LOG_TOPIC("098be", WARN, Logger::REPLICATION) << "we got the same tick again, "
                                           << "this indicates we're at the end";
    }

    // If this is non-hard, we employ some heuristics to stop early:
    if (!hard) {
      if (clock.now() - startTime > std::chrono::duration<double>(timeout) &&
          _ongoingTransactions.empty()) {
        checkMore = false;
        didTimeout = true;
      } else {
        TRI_voc_tick_t lastTick = 0;
        header = response->getHeaderField(StaticStrings::ReplicationHeaderLastTick, found);
        if (found) {
          lastTick = StringUtils::uint64(header);
          if (_ongoingTransactions.empty() && lastTick > lastIncludedTick &&  // just to make sure!
              lastTick - lastIncludedTick < 1000) {
            checkMore = false;
          }
        }
      }
    }

    if (!checkMore) {
      // done!
      TRI_ASSERT(r.ok());
      if (hard) {
        // now do a final sync-to-disk call. note that this can fail
        auto& engine = vocbase()->server().getFeature<EngineSelectorFeature>().engine();
        r = engine.flushWal(/*waitForSync*/ true, /*waitForCollector*/ false);
      }
      until = fromTick;
      return r;
    }

    LOG_TOPIC("2598f", DEBUG, Logger::REPLICATION) << "Fetching more data, fromTick: " << fromTick
                                          << ", lastScannedTick: " << lastScannedTick;

    _stats.publish();
  }
}

bool DatabaseTailingSyncer::skipMarker(VPackSlice const& slice) {
  // we do not have a "cname" attribute in the marker...
  // now check for a globally unique id attribute ("cuid")
  // if its present, then we will use our local cuid -> collection name
  // translation table
  VPackSlice const name = slice.get(::cuidRef);
  if (!name.isString()) {
    return false;
  }

  if (_state.leader.version() < 30300) {
    // globallyUniqueId only exists in 3.3 and higher
    return false;
  }

  if (!_queriedTranslations) {
    // no translations yet... query leader inventory to find names of all
    // collections
    try {
      VPackBuilder inventoryResponse;

      auto syncer = DatabaseInitialSyncer::create(*_vocbase, _state.applier);
      Result res = syncer->getInventory(inventoryResponse);
      _queriedTranslations = true;
      if (res.fail()) {
        LOG_TOPIC("89080", ERR, Logger::REPLICATION)
            << "got error while fetching leader inventory for collection name "
               "translations: "
            << res.errorMessage();
        return false;
      }
      VPackSlice invSlice = inventoryResponse.slice();
      if (!invSlice.isObject()) {
        return false;
      }
      invSlice = invSlice.get("collections");
      if (!invSlice.isArray()) {
        return false;
      }

      for (VPackSlice it : VPackArrayIterator(invSlice)) {
        if (!it.isObject()) {
          continue;
        }
        VPackSlice c = it.get("parameters");
        if (c.hasKey("name") && c.hasKey("globallyUniqueId")) {
          _translations[c.get("globallyUniqueId").copyString()] =
              c.get("name").copyString();
        }
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("cfaf3", ERR, Logger::REPLICATION)
          << "got error while fetching inventory: " << ex.what();
      return false;
    }
  }

  // look up cuid in translations map
  auto it = _translations.find(name.copyString());

  if (it != _translations.end()) {
    return isExcludedCollection((*it).second);
  }

  return false;
}
