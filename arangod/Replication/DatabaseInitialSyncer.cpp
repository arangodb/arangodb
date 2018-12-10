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
#include "Replication/utilities.h"
#include "RestServer/DatabaseFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Validator.h>
#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>
#include <array>
#include <cstring>

// lets keep this experimental until we make it faster
#define VPACK_DUMP 0

namespace {

/// @brief maximum internal value for chunkSize
size_t const maxChunkSize = 10 * 1024 * 1024;

std::chrono::milliseconds sleepTimeFromWaitTime(double waitTime) {
  if (waitTime < 1.0) {
    return std::chrono::milliseconds(100);
  }
  if (waitTime < 5.0) {
    return std::chrono::milliseconds(200);
  }
  if (waitTime < 20.0) {
    return std::chrono::milliseconds(500);
  }
  if (waitTime < 60.0) {
    return std::chrono::seconds(1);
  }

  return std::chrono::seconds(2);
}

std::string const kTypeString = "type";
std::string const kDataString = "data";

}  // namespace

namespace arangodb {

DatabaseInitialSyncer::Configuration::Configuration(
    ReplicationApplierConfiguration const& a, replutils::BarrierInfo& bar,
    replutils::BatchInfo& bat, replutils::Connection& c, bool f,
    replutils::MasterInfo& m, replutils::ProgressInfo& p, SyncerState& s,
    TRI_vocbase_t& v)
    : applier{a},
      barrier{bar},
      batch{bat},
      connection{c},
      flushed{f},
      master{m},
      progress{p},
      state{s},
      vocbase{v} {}

bool DatabaseInitialSyncer::Configuration::isChild() const {
  return state.isChildSyncer;
}

DatabaseInitialSyncer::DatabaseInitialSyncer(
    TRI_vocbase_t& vocbase,
    ReplicationApplierConfiguration const& configuration)
    : InitialSyncer(
          configuration,
          [this](std::string const& msg) -> void { setProgress(msg); }),
      _config{_state.applier,    _state.barrier, _batch,
              _state.connection, false,          _state.master,
              _progress,         _state,         vocbase} {
  _state.vocbases.emplace(std::piecewise_construct,
                          std::forward_as_tuple(vocbase.name()),
                          std::forward_as_tuple(vocbase));

  if (configuration._database.empty()) {
    _state.databaseName = vocbase.name();
  }
}

/// @brief run method, performs a full synchronization
Result DatabaseInitialSyncer::runWithInventory(bool incremental,
                                               VPackSlice dbInventory) {
  if (!_config.connection.valid()) {
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
    _config.progress.set("fetching master state");

    LOG_TOPIC(DEBUG, Logger::REPLICATION)
        << "client: getting master state to dump " << vocbase().name();
    Result r;

    if (!_config.isChild()) {
      r = _config.master.getState(_config.connection, _config.isChild());

      if (r.fail()) {
        return r;
      }
    }

    TRI_ASSERT(!_config.master.endpoint.empty());
    TRI_ASSERT(_config.master.serverId != 0);
    TRI_ASSERT(_config.master.majorVersion != 0);

    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "client: got master state";
    if (incremental) {
      if (_config.master.majorVersion == 1 ||
          (_config.master.majorVersion == 2 &&
           _config.master.minorVersion <= 6)) {
        LOG_TOPIC(WARN, Logger::REPLICATION) << "incremental replication is "
                                                "not supported with a master < "
                                                "ArangoDB 2.7";
        incremental = false;
      }
    }

    r = sendFlush();
    if (r.fail()) {
      return r;
    }

    if (!_config.isChild()) {
      // create a WAL logfile barrier that prevents WAL logfile collection
      r = _config.barrier.create(_config.connection,
                                 _config.master.lastLogTick);
      if (r.fail()) {
        return r;
      }
      
      // enable patching of collection count for ShardSynchronization Job
      std::string patchCount = StaticStrings::Empty;
      std::string const& engineName = EngineSelectorFeature::ENGINE->typeName();
      if (incremental && engineName == "rocksdb" && _config.applier._skipCreateDrop &&
          _config.applier._restrictType == ReplicationApplierConfiguration::RestrictType::Include &&
          _config.applier._restrictCollections.size() == 1) {
        patchCount = *_config.applier._restrictCollections.begin();
      }

      r = _config.batch.start(_config.connection, _config.progress, patchCount);
      if (r.fail()) {
        return r;
      }
      
      startRecurringBatchExtension();
    }

    VPackSlice collections, views;
    if (dbInventory.isObject()) {
      collections = dbInventory.get("collections"); // required
      views = dbInventory.get("views"); // optional
    }
    VPackBuilder inventoryResponse; // hold response data
    if (!collections.isArray()) {
      // caller did not supply an inventory, we need to fetch it
      Result res = fetchInventory(inventoryResponse);
      if (!res.ok()) {
        return res;
      }
      // we do not really care about the state response
      collections = inventoryResponse.slice().get("collections");
      if (!collections.isArray()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      "collections section is missing from response");
      }
      views = inventoryResponse.slice().get("views");
    }

    // strip eventual objectIDs and then dump the collections
    auto pair = rocksutils::stripObjectIds(collections);
    r = handleCollectionsAndViews(pair.first, views, incremental);
    
    // all done here, do not try to finish batch if master is unresponsive
    if (r.isNot(TRI_ERROR_REPLICATION_NO_RESPONSE) && !_config.isChild()) {
      _config.batch.finish(_config.connection, _config.progress);
    }

    if (r.fail()) {
      LOG_TOPIC(ERR, Logger::REPLICATION)
          << "Error during initial sync: " << r.errorMessage();
    }

    LOG_TOPIC(DEBUG, Logger::REPLICATION)
        << "initial synchronization with master took: "
        << Logger::FIXED(TRI_microtime() - startTime, 6)
        << " s. status: " << (r.errorMessage().empty() ? "all good" : r.errorMessage());

    return r;
  } catch (arangodb::basics::Exception const& ex) {
    if (!_config.isChild()) {
      _config.batch.finish(_config.connection, _config.progress);
    }
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    if (!_config.isChild()) {
      _config.batch.finish(_config.connection, _config.progress);
    }
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    if (!_config.isChild()) {
      _config.batch.finish(_config.connection, _config.progress);
    }
    return Result(TRI_ERROR_NO_ERROR, "an unknown exception occurred");
  }
}

/// @brief fetch the server's inventory, public method for TailingSyncer
Result DatabaseInitialSyncer::getInventory(VPackBuilder& builder) {
  if (!_state.connection.valid()) {
    return Result(TRI_ERROR_INTERNAL, "invalid endpoint");
  }

  auto r = _config.batch.start(_config.connection, _config.progress);
  if (r.fail()) {
    return r;
  }

  TRI_DEFER(_config.batch.finish(_config.connection, _config.progress));

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
  _config.progress.message = msg;

  if (_config.applier._verbose) {
    LOG_TOPIC(INFO, Logger::REPLICATION) << msg;
  } else {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << msg;
  }

  auto* applier = _config.vocbase.replicationApplier();

  if (applier != nullptr) {
    applier->setProgress(msg);
  }
}

/// @brief send a WAL flush command
Result DatabaseInitialSyncer::sendFlush() {
  if (isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  std::string const& engineName = EngineSelectorFeature::ENGINE->typeName();
  if (engineName == "rocksdb" && _state.master.engine == engineName) {
    // no WAL flush required for RocksDB. this is only relevant for MMFiles
    return Result();
  }

  std::string const url = "/_admin/wal/flush";

  VPackBuilder builder;
  builder.openObject();
  builder.add("waitForSync", VPackValue(true));
  builder.add("waitForCollector", VPackValue(true));
  builder.add("waitForCollectorQueue", VPackValue(true));
  builder.add("maxWaitTime", VPackValue(300.0));
  builder.close();

  VPackSlice bodySlice = builder.slice();
  std::string const body = bodySlice.toJson();

  // send request
  _config.progress.set("sending WAL flush command to url " + url);

  std::unique_ptr<httpclient::SimpleHttpResult> response;
  _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
    response.reset(client->retryRequest(rest::RequestType::PUT, url,
                                        body.c_str(), body.size()));
  });

  if (replutils::hasFailed(response.get())) {
    return replutils::buildHttpError(response.get(), url, _config.connection);
  }

  _config.flushed = true;
  return Result();
}

/// @brief handle a single dump marker
Result DatabaseInitialSyncer::parseCollectionDumpMarker(
    transaction::Methods& trx, LogicalCollection* coll,
    VPackSlice const& marker) {
  if (!marker.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_replication_operation_e type = REPLICATION_INVALID;
  VPackSlice doc;

  for (auto const& it : VPackObjectIterator(marker, true)) {
    if (it.key.isEqualString(kTypeString)) {
      if (it.value.isNumber()) {
        type =
            static_cast<TRI_replication_operation_e>(it.value.getNumber<int>());
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
Result DatabaseInitialSyncer::parseCollectionDump(
    transaction::Methods& trx, LogicalCollection* coll,
    httpclient::SimpleHttpResult* response, uint64_t& markersProcessed) {

  basics::StringBuffer const& data = response->getBody();
  char const* p = data.begin();
  char const* end = p + data.length();

  bool found = false;
  std::string const& cType =
      response->getHeaderField(StaticStrings::ContentTypeHeader, found);
  if (found && (cType == StaticStrings::MimeTypeVPack)) {
    VPackOptions options;
    options.unsupportedTypeBehavior = VPackOptions::FailOnUnsupportedType;
    VPackValidator validator(&options);

    try {
      while (p < end) {
        ptrdiff_t remaining = end - p;
        // throws if the data is invalid
        validator.validate(p, remaining, /*isSubPart*/ true);

        VPackSlice marker(p);
        Result r = parseCollectionDumpMarker(trx, coll, marker);
        if (r.fail()) {
          r.reset(r.errorNumber(), std::string("received invalid dump data for collection '") + coll->name() + "'");
          return r;
        }
        ++markersProcessed;
        p += marker.byteSize();
      }
    } catch (velocypack::Exception const& e) {
      LOG_TOPIC(ERR, Logger::REPLICATION)
          << "Error parsing VPack response: " << e.what();
      return Result(e.errorCode(), e.what());
    }

  } else {
    // buffer must end with a NUL byte
    TRI_ASSERT(*end == '\0');

    VPackBuilder builder;
    VPackParser parser(builder);

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
        parser.parse(p, static_cast<size_t>(q - p));
      } catch (velocypack::Exception const& e) {
        LOG_TOPIC(ERR, Logger::REPLICATION)
            << "while parsing collection dump: " << e.what();
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

/// @brief order a new chunk from the /dump API
void DatabaseInitialSyncer::fetchDumpChunk(std::shared_ptr<Syncer::JobSynchronizer> sharedStatus,
                                           std::string const& baseUrl,
                                           arangodb::LogicalCollection* coll,
                                           std::string const& leaderColl,
                                           InitialSyncerDumpStats& stats,
                                           int batch,
                                           TRI_voc_tick_t fromTick,
                                           uint64_t chunkSize) {

  using ::arangodb::basics::StringUtils::itoa;

  if (isAborted()) {
    sharedStatus->gotResponse(Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED));
    return;
  }

  // check if master & slave use the same storage engine
  // if both use RocksDB, there is no need to use an async request for the
  // initial batch. this is because with RocksDB there is no initial load
  // time for collections as there may be with MMFiles if the collection is
  // not yet in memory
  std::string const& engineName = EngineSelectorFeature::ENGINE->typeName();
  bool const useAsync = (batch == 1 &&
                         (engineName != "rocksdb" || _state.master.engine != engineName));

  try {
    std::string const typeString = (coll->type() == TRI_COL_TYPE_EDGE ? "edge" : "document");

    if (!_config.isChild()) {
      _config.batch.extend(_config.connection, _config.progress);
      _config.barrier.extend(_config.connection);
    }

    // assemble URL to call
    std::string url = baseUrl + "&from=" + itoa(fromTick) + "&chunkSize=" + itoa(chunkSize);

    if (_config.flushed) {
      url += "&flush=false";
    } else {
      // only flush WAL once
      url += "&flush=true&flushWait=180";
      _config.flushed = true;
    }

    auto headers = replutils::createHeaders();
    if (useAsync) {
      // use async mode for first batch
      headers[StaticStrings::Async] = "store";
    }

#if VPACK_DUMP
    int vv = _config.master.majorVersion * 1000000 +
            _config.master.minorVersion * 1000;
    if (vv >= 3003009) {
      headers[StaticStrings::Accept] = StaticStrings::MimeTypeVPack;
    }
#endif

    _config.progress.set(std::string("fetching master collection dump for collection '") +
                         coll->name() + "', type: " + typeString + ", id: " +
                         leaderColl + ", batch " + itoa(batch) + ", url: " + url);

    ++stats.numDumpRequests;
    double t = TRI_microtime();

    // send request
    std::unique_ptr<httpclient::SimpleHttpResult> response;
    _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
      response.reset(client->retryRequest(rest::RequestType::GET, url,
                                          nullptr, 0, headers));
    });
    

    if (replutils::hasFailed(response.get())) {
      stats.waitedForDump += TRI_microtime() - t;
      sharedStatus->gotResponse(replutils::buildHttpError(response.get(), url, _config.connection));
      return;
    }

    // use async mode for first batch
    if (useAsync) {
      bool found = false;
      std::string jobId =
          response->getHeaderField(StaticStrings::AsyncId, found);

      if (!found) {
        sharedStatus->gotResponse(Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                                  std::string("got invalid response from master at ") +
                                  _config.master.endpoint + url +
                                  ": could not find 'X-Arango-Async' header"));
        return;
      }

      double const startTime = TRI_microtime();

      // wait until we get a reasonable response
      while (true) {
        if (!_config.isChild()) {
          _config.batch.extend(_config.connection, _config.progress);
          _config.barrier.extend(_config.connection);
        }

        std::string const jobUrl = "/_api/job/" + jobId;
        _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
          response.reset(client->request(rest::RequestType::PUT, jobUrl, nullptr, 0));
        });

        if (response != nullptr && response->isComplete()) {
          if (response->hasHeaderField("x-arango-async-id")) {
            // got the actual response
            break;
          }

          if (response->getHttpReturnCode() == 404) {
            // unknown job, we can abort
            sharedStatus->gotResponse(Result(TRI_ERROR_REPLICATION_NO_RESPONSE,
                                      std::string("job not found on master at ") +
                                      _config.master.endpoint));
            return;
          }
        }

        double waitTime = TRI_microtime() - startTime;

        if (static_cast<uint64_t>(waitTime * 1000.0 * 1000.0) >=
            _config.applier._initialSyncMaxWaitTime) {
          sharedStatus->gotResponse(Result(TRI_ERROR_REPLICATION_NO_RESPONSE,
                                    std::string("timed out waiting for response from master at ") +
                                    _config.master.endpoint));
          return;
        }

        if (isAborted()) {
          sharedStatus->gotResponse(Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED));
          return;
        }

        std::chrono::milliseconds sleepTime = ::sleepTimeFromWaitTime(waitTime);
        std::this_thread::sleep_for(sleepTime);
      }
      // fallthrough here in case everything went well
    }

    stats.waitedForDump += TRI_microtime() - t;

    if (replutils::hasFailed(response.get())) {
      // failure
      sharedStatus->gotResponse(replutils::buildHttpError(response.get(), url, _config.connection));
    } else {
      // success!
      sharedStatus->gotResponse(std::move(response));
    }
  } catch (basics::Exception const& ex) {
    sharedStatus->gotResponse(Result(ex.code(), ex.what()));
  } catch (std::exception const& ex) {
    sharedStatus->gotResponse(Result(TRI_ERROR_INTERNAL, ex.what()));
  }
}

/// @brief incrementally fetch data from a collection
Result DatabaseInitialSyncer::fetchCollectionDump(
    arangodb::LogicalCollection* coll, std::string const& leaderColl,
    TRI_voc_tick_t maxTick) {
  using ::arangodb::basics::StringUtils::boolean;
  using ::arangodb::basics::StringUtils::itoa;
  using ::arangodb::basics::StringUtils::uint64;
  using ::arangodb::basics::StringUtils::urlEncode;

  if (isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  std::string const typeString = (coll->type() == TRI_COL_TYPE_EDGE ? "edge" : "document");

  InitialSyncerDumpStats stats;

  TRI_ASSERT(_config.batch.id);  // should not be equal to 0

  // assemble base URL
  std::string baseUrl =
      replutils::ReplicationUrl + "/dump?collection=" + urlEncode(leaderColl) +
      "&batchId=" + std::to_string(_config.batch.id) +
      "&includeSystem=" + std::string(_config.applier._includeSystem ? "true" : "false") +
      "&serverId=" + _state.localServerIdString;

  if (maxTick > 0) {
    baseUrl += "&to=" + itoa(maxTick + 1);
  }

  // state variables for the dump
  TRI_voc_tick_t fromTick = 0;
  int batch = 1;
  uint64_t chunkSize = _config.applier._chunkSize;
  uint64_t bytesReceived = 0;
  uint64_t markersProcessed = 0;

  double const startTime = TRI_microtime();

  // the shared status will wait in its destructor until all posted
  // requests have been completed/canceled!
  auto self = shared_from_this();
  auto sharedStatus = std::make_shared<Syncer::JobSynchronizer>(self);

  // order initial chunk. this will block until the initial response
  // has arrived
  fetchDumpChunk(sharedStatus, baseUrl, coll, leaderColl, stats, batch, fromTick, chunkSize);

  while (true) {
    std::unique_ptr<httpclient::SimpleHttpResult> dumpResponse;

    // block until we either got a response or were shut down
    Result res = sharedStatus->waitForResponse(dumpResponse);

    if (res.fail()) {
      // no response or error or shutdown
      return res;
    }

    // now we have got a response!
    TRI_ASSERT(dumpResponse != nullptr);

    if (dumpResponse->hasContentLength()) {
      bytesReceived += dumpResponse->getContentLength();
    }

    bool found;
    std::string header = dumpResponse->getHeaderField(
        StaticStrings::ReplicationHeaderCheckMore, found);
    if (!found) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    std::string("got invalid response from master at ") +
                        _config.master.endpoint + ": required header " +
                        StaticStrings::ReplicationHeaderCheckMore +
                        " is missing in dump response");
    }

    TRI_voc_tick_t tick;
    bool checkMore = boolean(header);

    if (checkMore) {
      header = dumpResponse->getHeaderField(
          StaticStrings::ReplicationHeaderLastIncluded, found);
      if (!found) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          _config.master.endpoint + ": required header " +
                          StaticStrings::ReplicationHeaderLastIncluded +
                          " is missing in dump response");
      }

      tick = uint64(header);

      if (tick > fromTick) {
        fromTick = tick;
      } else {
        // we got the same tick again, this indicates we're at the end
        checkMore = false;
      }
    }

    // increase chunk size for next fetch
    if (chunkSize < ::maxChunkSize) {
      chunkSize = static_cast<uint64_t>(chunkSize * 1.25);

      if (chunkSize > ::maxChunkSize) {
        chunkSize = ::maxChunkSize;
      }
    }

    if (checkMore && !isAborted()) {
      // already fetch next batch in the background, by posting the
      // request to the scheduler, which can run it asynchronously
      sharedStatus->request(
        [this, self, &stats, &baseUrl, sharedStatus, coll, leaderColl, batch, fromTick, chunkSize]() {
          fetchDumpChunk(sharedStatus, baseUrl, coll, leaderColl, stats, batch + 1, fromTick, chunkSize);
        }
      );
    }

    SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(vocbase()),
      *coll,
      AccessMode::Type::EXCLUSIVE
    );

    // to turn off waitForSync!
    trx.addHint(transaction::Hints::Hint::RECOVERY);
    // do not index the operations in our own transaction
    trx.addHint(transaction::Hints::Hint::NO_INDEXING);

    // smaller batch sizes should work better here
#if VPACK_DUMP
//    trx.state()->options().intermediateCommitCount = 128;
//    trx.state()->options().intermediateCommitSize = 16 * 1024 * 1024;
#endif

    res = trx.begin();

    if (!res.ok()) {
      return Result(
          res.errorNumber(),
          std::string("unable to start transaction: ") + res.errorMessage());
    }

    trx.pinData(coll->id());  // will throw when it fails

    double t = TRI_microtime();
    res = parseCollectionDump(trx, coll, dumpResponse.get(), markersProcessed);

    if (res.fail()) {
      return res;
    }

    res = trx.commit();

    double applyTime = TRI_microtime() - t;
    stats.waitedForApply += applyTime;

    _config.progress.set(std::string("fetched master collection dump for collection '") +
                         coll->name() + "', type: " + typeString + ", id: " +
                         leaderColl + ", batch " + itoa(batch) +
                         ", markers processed: " + itoa(markersProcessed) +
                         ", bytes received: " + itoa(bytesReceived) +
                         ", apply time: " + std::to_string(applyTime) + " s");

    if (!res.ok()) {
      return res;
    }

    if (!checkMore || fromTick == 0) {
      // done
      _config.progress.set(std::string("finished initial dump for collection '") + coll->name() +
        "', type: " + typeString + ", id: " + leaderColl +
        ", markers processed: " + itoa(markersProcessed) +
        ", bytes received: " + itoa(bytesReceived) +
        ", dump requests: " + std::to_string(stats.numDumpRequests) +
        ", waited for dump: " + std::to_string(stats.waitedForDump) + " s" +
        ", apply time: " + std::to_string(stats.waitedForApply) + " s" +
        ", total time: " + std::to_string(TRI_microtime() - startTime) + " s");
      return Result();
    }

    batch++;

    if (isAborted()) {
      return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }
  }

  TRI_ASSERT(false);
  return Result(TRI_ERROR_INTERNAL);
}

/// @brief incrementally fetch data from a collection
Result DatabaseInitialSyncer::fetchCollectionSync(
    arangodb::LogicalCollection* coll, std::string const& leaderColl,
    TRI_voc_tick_t maxTick) {
  using ::arangodb::basics::StringUtils::urlEncode;

  if (!_config.isChild()) {
    _config.batch.extend(_config.connection, _config.progress);
    _config.barrier.extend(_config.connection);
  }

  std::string const baseUrl = replutils::ReplicationUrl + "/keys";
  std::string url = baseUrl + "/keys" + "?collection=" + urlEncode(leaderColl) +
                    "&to=" + std::to_string(maxTick) +
                    "&serverId=" + _state.localServerIdString +
                    "&batchId=" + std::to_string(_config.batch.id);

  std::string msg = "fetching collection keys for collection '" + coll->name() +
                    "' from " + url;
  _config.progress.set(msg);

  // send an initial async request to collect the collection keys on the other
  // side
  // sending this request in a blocking fashion may require very long to
  // complete,
  // so we're sending the x-arango-async header here
  auto headers = replutils::createHeaders();
  headers[StaticStrings::Async] = "store";
  
  std::unique_ptr<httpclient::SimpleHttpResult> response;
  _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
    response.reset(client->retryRequest(rest::RequestType::POST, url,
                                        nullptr, 0, headers));
  });

  if (replutils::hasFailed(response.get())) {
    return replutils::buildHttpError(response.get(), url, _config.connection);
  }

  bool found = false;
  std::string jobId = response->getHeaderField(StaticStrings::AsyncId, found);

  if (!found) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      _config.master.endpoint + url +
                      ": could not find 'X-Arango-Async' header");
  }

  double const startTime = TRI_microtime();

  while (true) {
    if (!_config.isChild()) {
      _config.batch.extend(_config.connection, _config.progress);
      _config.barrier.extend(_config.connection);
    }

    std::string const jobUrl = "/_api/job/" + jobId;
    _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
      response.reset(client->request(rest::RequestType::PUT, jobUrl, nullptr, 0));
    });

    if (response != nullptr && response->isComplete()) {
      if (response->hasHeaderField("x-arango-async-id")) {
        // job is done, got the actual response
        break;
      }
      if (response->getHttpReturnCode() == 404) {
        // unknown job, we can abort
        return Result(TRI_ERROR_REPLICATION_NO_RESPONSE,
                      std::string("job not found on master at ") +
                          _config.master.endpoint);
      }
    }

    double waitTime = TRI_microtime() - startTime;

    if (static_cast<uint64_t>(waitTime * 1000.0 * 1000.0) >=
        _config.applier._initialSyncMaxWaitTime) {
      return Result(
          TRI_ERROR_REPLICATION_NO_RESPONSE,
          std::string("timed out waiting for response from master at ") +
              _config.master.endpoint);
    }

    if (isAborted()) {
      return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }

    std::chrono::milliseconds sleepTime = ::sleepTimeFromWaitTime(waitTime);
    std::this_thread::sleep_for(sleepTime);
  }

  if (replutils::hasFailed(response.get())) {
    return replutils::buildHttpError(response.get(), url, _config.connection);
  }

  VPackBuilder builder;
  Result r = replutils::parseResponse(builder, response.get());

  if (r.fail()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      _config.master.endpoint + url + ": " + r.errorMessage());
  }

  VPackSlice const slice = builder.slice();
  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      _config.master.endpoint + url +
                      ": response is no object");
  }

  VPackSlice const keysId = slice.get("id");

  if (!keysId.isString()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      _config.master.endpoint + url +
                      ": response does not contain valid 'id' attribute");
  }

  auto shutdown = [&]() -> void {
    url = baseUrl + "/" + keysId.copyString();
    std::string msg =
        "deleting remote collection keys object for collection '" +
        coll->name() + "' from " + url;
    _config.progress.set(msg);

    // now delete the keys we ordered
    std::unique_ptr<httpclient::SimpleHttpResult> response;
    _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
      response.reset(client->retryRequest(rest::RequestType::DELETE_REQ,
                                          url, nullptr, 0));
    });
  };

  TRI_DEFER(shutdown());

  VPackSlice const count = slice.get("count");

  if (!count.isNumber()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      _config.master.endpoint + url +
                      ": response does not contain valid 'count' attribute");
  }

  if (count.getNumber<size_t>() <= 0) {
    // remote collection has no documents. now truncate our local collection
    SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(vocbase()),
      *coll,
      AccessMode::Type::EXCLUSIVE
    );
    trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
    trx.addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
    Result res = trx.begin();

    if (!res.ok()) {
      return Result(res.errorNumber(),
                    std::string("unable to start transaction (") +
                        std::string(__FILE__) + std::string(":") +
                        std::to_string(__LINE__) + std::string("): ") +
                        res.errorMessage());
    }

    OperationOptions options;

    if (!_state.leaderId.empty()) {
      options.isSynchronousReplicationFrom = _state.leaderId;
    }

    OperationResult opRes = trx.truncate(coll->name(), options);

    if (opRes.fail()) {
      return Result(opRes.errorNumber(),
                    std::string("unable to truncate collection '") +
                        coll->name() +
                        "': " + TRI_errno_string(opRes.errorNumber()));
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

  return guard.collection()->properties(slice, false); // always a full-update
}

/// @brief determine the number of documents in a collection
int64_t DatabaseInitialSyncer::getSize(arangodb::LogicalCollection const& col) {
  SingleCollectionTransaction trx(
    transaction::StandaloneContext::Create(vocbase()),
    col,
    AccessMode::Type::READ
  );
  Result res = trx.begin();

  if (res.fail()) {
    return -1;
  }

  auto result = trx.count(col.name(), transaction::CountType::Normal);

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
                                               VPackSlice const& indexes,
                                               bool incremental,
                                               SyncPhase phase) {
  using ::arangodb::basics::StringUtils::itoa;

  if (isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  if (!parameters.isObject() || !indexes.isArray()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE);
  }

  if (!_config.isChild()) {
    _config.batch.extend(_config.connection, _config.progress);
    _config.barrier.extend(_config.connection);
  }

  std::string const masterName =
      basics::VelocyPackHelper::getStringValue(parameters, "name", "");

  TRI_voc_cid_t const masterCid =
      basics::VelocyPackHelper::extractIdValue(parameters);

  if (masterCid == 0) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "collection id is missing in response");
  }

  std::string const masterUuid = basics::VelocyPackHelper::getStringValue(
      parameters, "globallyUniqueId", "");

  VPackSlice const type = parameters.get("type");

  if (!type.isNumber()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "collection type is missing in response");
  }

  std::string const typeString =
      (type.getNumber<int>() == 3 ? "edge" : "document");

  std::string const collectionMsg = "collection '" + masterName + "', type " +
                                    typeString + ", id " + itoa(masterCid);

  // phase handling
  if (phase == PHASE_VALIDATE) {
    // validation phase just returns ok if we got here (aborts above if data is
    // invalid)
    _config.progress.processedCollections.emplace(masterCid, masterName);

    return Result();
  }

  // drop and re-create collections locally
  // -------------------------------------------------------------------------------------

  if (phase == PHASE_DROP_CREATE) {
    auto* col = resolveCollection(vocbase(), parameters).get();

    if (col == nullptr) {
      // not found...
      col = vocbase().lookupCollection(masterName).get();

      if (col != nullptr &&
          (col->name() != masterName ||
           (!masterUuid.empty() && col->guid() != masterUuid))) {
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
            _config.progress.set("truncating " + collectionMsg);

            SingleCollectionTransaction trx(
              transaction::StandaloneContext::Create(vocbase()),
              *col,
              AccessMode::Type::EXCLUSIVE
            );
            trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
            trx.addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
            Result res = trx.begin();

            if (!res.ok()) {
              return Result(res.errorNumber(),
                            std::string("unable to truncate ") + collectionMsg +
                                ": " + res.errorMessage());
            }

            OperationOptions options;

            if (!_state.leaderId.empty()) {
              options.isSynchronousReplicationFrom = _state.leaderId;
            }

            OperationResult opRes = trx.truncate(col->name(), options);

            if (opRes.fail()) {
              return Result(opRes.errorNumber(),
                            std::string("unable to truncate ") + collectionMsg +
                                ": " + TRI_errno_string(opRes.errorNumber()));
            }

            res = trx.finish(opRes.result);

            if (!res.ok()) {
              return Result(res.errorNumber(),
                            std::string("unable to truncate ") + collectionMsg +
                                ": " + res.errorMessage());
            }
          } else {
            // drop a regular collection
            if (_config.applier._skipCreateDrop) {
              _config.progress.set("dropping " + collectionMsg +
                                   " skipped because of configuration");
              return Result();
            }
            _config.progress.set("dropping " + collectionMsg);

            auto res =
                vocbase().dropCollection(col->id(), true, -1.0).errorNumber();

            if (res != TRI_ERROR_NO_ERROR) {
              return Result(res, std::string("unable to drop ") +
                                     collectionMsg + ": " +
                                     TRI_errno_string(res));
            }
          }
        }
      } else {
        // incremental case
        TRI_ASSERT(incremental);

        // collection is already present
        _config.progress.set("checking/changing parameters of " +
                             collectionMsg);
        return changeCollection(col, parameters);
      }
    }

    std::string msg = "creating " + collectionMsg;
    if (_config.applier._skipCreateDrop) {
      msg += " skipped because of configuration";
      _config.progress.set(msg);
      return Result();
    }
    _config.progress.set(msg);

    LOG_TOPIC(DEBUG, Logger::REPLICATION)
        << "Dump is creating collection " << parameters.toJson();

    auto r = createCollection(vocbase(), parameters, &col);

    if (r.fail()) {
      return Result(r.errorNumber(),
                    std::string("unable to create ") + collectionMsg + ": " +
                        TRI_errno_string(r.errorNumber()) +
                        ". Collection info " + parameters.toJson());
    }

    return r;
  }

  // sync collection data
  // -------------------------------------------------------------------------------------

  else if (phase == PHASE_DUMP) {
    _config.progress.set("dumping data for " + collectionMsg);

    auto* col = resolveCollection(vocbase(), parameters).get();

    if (col == nullptr) {
      return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                    std::string("cannot dump: ") + collectionMsg +
                        " not found on slave. Collection info " +
                        parameters.toJson());
    }

    std::string const& masterColl =
        !masterUuid.empty() ? masterUuid : itoa(masterCid);
    auto res = incremental && getSize(*col) > 0
             ? fetchCollectionSync(col, masterColl, _config.master.lastUncommittedLogTick)
             : fetchCollectionDump(col, masterColl, _config.master.lastUncommittedLogTick);

    if (!res.ok()) {
      return res;
    } else if (isAborted()) {
      return res.reset(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }

    if (masterName == TRI_COL_NAME_USERS) {
      reloadUsers();
    }

    // schmutz++ creates indexes on DBServers
    if (_config.applier._skipCreateDrop) {
      _config.progress.set("creating indexes for " + collectionMsg +
                           " skipped because of configuration");
      return res;
    }

    // now create indexes
    TRI_ASSERT(indexes.isArray());
    VPackValueLength const numIdx = indexes.length();
    if (numIdx > 0) {
      if (!_config.isChild()) {
        _config.batch.extend(_config.connection, _config.progress);
        _config.barrier.extend(_config.connection);
      }

      _config.progress.set("creating " + std::to_string(numIdx) + " index(es) for " +
                           collectionMsg);

      try {
        auto physical = col->getPhysical();
        TRI_ASSERT(physical != nullptr);

        for (auto const& idxDef : VPackArrayIterator(indexes)) {
          std::shared_ptr<arangodb::Index> idx;

          if (idxDef.isObject()) {
            VPackSlice const type = idxDef.get(StaticStrings::IndexType);
            if (type.isString()) {
              _config.progress.set("creating index of type " +
                                   type.copyString() + " for " + collectionMsg);
            }
          }

          bool created = false;
          idx = physical->createIndex(idxDef, /*restore*/true, created);
          TRI_ASSERT(idx != nullptr);
        }
      } catch (arangodb::basics::Exception const& ex) {
        return res.reset(ex.code(), ex.what());
      } catch (std::exception const& ex) {
        return res.reset(TRI_ERROR_INTERNAL, ex.what());
      } catch (...) {
        return res.reset(TRI_ERROR_INTERNAL);
      }
    }

    return res;
  }

  // we won't get here
  TRI_ASSERT(false);
  return Result(TRI_ERROR_INTERNAL);
}

/// @brief fetch the server's inventory
arangodb::Result DatabaseInitialSyncer::fetchInventory(VPackBuilder& builder) {
  std::string url = replutils::ReplicationUrl +
                    "/inventory?serverId=" + _state.localServerIdString +
                    "&batchId=" + std::to_string(_config.batch.id);
  if (_config.applier._includeSystem) {
    url += "&includeSystem=true";
  }

  // send request
  _config.progress.set("fetching master inventory from " + url);
  std::unique_ptr<httpclient::SimpleHttpResult> response;
  _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
    response.reset(client->retryRequest(rest::RequestType::GET, url, nullptr, 0));
  });
  
  if (replutils::hasFailed(response.get())) {
    if (!_config.isChild()) {
      _config.batch.finish(_config.connection, _config.progress);
    }
    return replutils::buildHttpError(response.get(), url, _config.connection);
  }

  Result r = replutils::parseResponse(builder, response.get());

  if (r.fail()) {
    return Result(
        r.errorNumber(),
        std::string("got invalid response from master at ") +
            _config.master.endpoint + url +
            ": invalid response type for initial data. expecting array");
  }

  VPackSlice const slice = builder.slice();
  if (!slice.isObject()) {
    LOG_TOPIC(DEBUG, Logger::REPLICATION)
        << "client: DatabaseInitialSyncer::run - inventoryResponse is not an "
           "object";

    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      _config.master.endpoint + url + ": invalid JSON");
  }

  return Result();
}

/// @brief handle the inventory response of the master
Result DatabaseInitialSyncer::handleCollectionsAndViews(VPackSlice const& collSlices,
                                                        VPackSlice const& viewSlices,
                                                        bool incremental) {
  TRI_ASSERT(collSlices.isArray());

  std::vector<std::pair<VPackSlice, VPackSlice>> collections;
  for (VPackSlice it : VPackArrayIterator(collSlices)) {
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
        basics::VelocyPackHelper::getStringValue(parameters, "name", "");

    if (masterName.empty()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "collection name is missing in response");
    }

    if (TRI_ExcludeCollectionReplication(masterName,
                                         _config.applier._includeSystem)) {
      continue;
    }

    if (basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                  false)) {
      // we don't care about deleted collections
      continue;
    }

    if (_config.applier._restrictType != ReplicationApplierConfiguration::RestrictType::None) {
      auto const it = _config.applier._restrictCollections.find(masterName);
      bool found = (it != _config.applier._restrictCollections.end());

      if (_config.applier._restrictType == ReplicationApplierConfiguration::RestrictType::Include && !found) {
        // collection should not be included
        continue;
      } else if (_config.applier._restrictType == ReplicationApplierConfiguration::RestrictType::Exclude && found) {
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

  // iterate over all collections from the master...
  std::array<SyncPhase, 2> phases{{PHASE_VALIDATE, PHASE_DROP_CREATE}};
  for (auto const& phase : phases) {
    Result r = iterateCollections(collections, incremental, phase);

    if (r.fail()) {
      return r;
    }
  }
  
  // STEP 3: now that the collections exist create the views
  // this should be faster than re-indexing afterwards
  // ----------------------------------------------------------------------------------
  
  if (!_config.applier._skipCreateDrop &&
      _config.applier._restrictCollections.empty() &&
      viewSlices.isArray()) {
    // views are optional, and 3.3 and before will not send any view data
    Result r = handleViewCreation(viewSlices); // no requests to master
    if (r.fail()) {
      LOG_TOPIC(ERR, Logger::REPLICATION)
      << "Error during intial sync view creation: " << r.errorMessage();
      return r;
    }
  } else {
    _config.progress.set("view creation skipped because of configuration");
  }
  
  // STEP 4: sync collection data from master and create initial indexes
  // ----------------------------------------------------------------------------------
  
  // now load the data into the collections
  return iterateCollections(collections, incremental, PHASE_DUMP);
}

/// @brief iterate over all collections from an array and apply an action
Result DatabaseInitialSyncer::iterateCollections(
    std::vector<std::pair<VPackSlice, VPackSlice>> const& collections,
    bool incremental, SyncPhase phase) {
  std::string phaseMsg("starting phase " + translatePhase(phase) + " with " +
                       std::to_string(collections.size()) + " collections");
  _config.progress.set(phaseMsg);

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

/// @brief create non-existing views locally
Result DatabaseInitialSyncer::handleViewCreation(VPackSlice const& views) {
  for (VPackSlice slice  : VPackArrayIterator(views)) {
    Result res = createView(vocbase(), slice);
    if (res.fail()) {
      return res;
    }
  }
  return {};
}

}  // namespace arangodb
