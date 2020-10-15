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
#include "Basics/HybridLogicalClock.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/system-functions.h"
#include "Indexes/Index.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "Logger/Logger.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/utilities.h"
#include "Rest/CommonDefines.h"
#include "RestHandler/RestReplicationHandler.h"
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

arangodb::Result removeRevisions(arangodb::transaction::Methods& trx,
                                 arangodb::LogicalCollection& collection,
                                 std::vector<std::size_t>& toRemove,
                                 arangodb::InitialSyncerIncrementalSyncStats& stats) {
  using arangodb::PhysicalCollection;
  using arangodb::Result;

  if (toRemove.empty()) {
    // no need to do anything
    return Result();
  }

  PhysicalCollection* physical = collection.getPhysical();

  arangodb::ManagedDocumentResult mdr;
  arangodb::OperationOptions options;
  options.silent = true;
  options.ignoreRevs = true;
  options.isRestore = true;
  options.waitForSync = false;

  for (std::size_t rid : toRemove) {
    double t = TRI_microtime();
    auto r = physical->remove(trx, arangodb::LocalDocumentId::create(rid), mdr, options);

    stats.waitedForRemovals += TRI_microtime() - t;
    if (r.fail() && r.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
      // ignore not found, we remove conflicting docs ahead of time
      return r;
    }

    if (r.ok()) {
      ++stats.numDocsRemoved;
    }
  }

  return Result();
}

arangodb::Result fetchRevisions(arangodb::transaction::Methods& trx,
                                arangodb::DatabaseInitialSyncer::Configuration& config,
                                arangodb::Syncer::SyncerState& state,
                                arangodb::LogicalCollection& collection,
                                std::string const& leader, std::vector<std::size_t>& toFetch,
                                arangodb::InitialSyncerIncrementalSyncStats& stats) {
  using arangodb::PhysicalCollection;
  using arangodb::RestReplicationHandler;
  using arangodb::Result;

  if (toFetch.empty()) {
    return Result();  // nothing to do
  }

  arangodb::transaction::BuilderLeaser keyBuilder(&trx);
  arangodb::ManagedDocumentResult mdr, previous;
  arangodb::OperationOptions options;
  options.silent = true;
  options.ignoreRevs = true;
  options.isRestore = true;
  options.validate = false; // no validation during replication
  options.indexOperationMode = arangodb::Index::OperationMode::internal;
  options.ignoreUniqueConstraints = true;
  options.waitForSync = false; // no waitForSync during replication
  if (!state.leaderId.empty()) {
    options.isSynchronousReplicationFrom = state.leaderId;
  }

  PhysicalCollection* physical = collection.getPhysical();

  std::string url =
      arangodb::replutils::ReplicationUrl + "/" +
      RestReplicationHandler::Revisions + "/" + RestReplicationHandler::Documents +
      "?collection=" + arangodb::basics::StringUtils::urlEncode(leader) +
      "&serverId=" + state.localServerIdString +
      "&batchId=" + std::to_string(config.batch.id);
  auto headers = arangodb::replutils::createHeaders();

  std::string msg = "fetching documents by revision for collection '" +
                    collection.name() + "' from " + url;
  config.progress.set(msg);

  auto removeConflict = [&](std::string const& conflictingKey) -> Result {
    keyBuilder->clear();
    keyBuilder->add(VPackValue(conflictingKey));

    auto res = physical->remove(trx, keyBuilder->slice(), mdr, options);

    if (res.ok()) {
      ++stats.numDocsRemoved;
    }

    return res;
  };

  std::size_t current = 0;
  auto guard = arangodb::scopeGuard(
      [&current, &stats]() -> void { stats.numDocsRequested += current; });
  char ridBuffer[11];
  std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response;
  while (current < toFetch.size()) {
    arangodb::transaction::BuilderLeaser requestBuilder(&trx);
    {
      VPackArrayBuilder list(requestBuilder.get());
      for (std::size_t i = 0; i < 5000 && current + i < toFetch.size(); ++i) {
        requestBuilder->add(arangodb::basics::HybridLogicalClock::encodeTimeStampToValuePair(
            toFetch[current + i], ridBuffer));
      }
    }
    std::string request = requestBuilder->slice().toJson();

    double t = TRI_microtime();
    config.connection.lease([&](arangodb::httpclient::SimpleHttpClient* client) {
      response.reset(client->retryRequest(arangodb::rest::RequestType::PUT, url,
                                          request.data(), request.size(), headers));
    });
    stats.waitedForDocs += TRI_microtime() - t;
    ++stats.numDocsRequests;

    if (arangodb::replutils::hasFailed(response.get())) {
      return arangodb::replutils::buildHttpError(response.get(), url, config.connection);
    }

    arangodb::transaction::BuilderLeaser responseBuilder(&trx);
    Result r =
        arangodb::replutils::parseResponse(*responseBuilder.get(), response.get());
    if (r.fail()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    std::string("got invalid response from master at ") +
                        config.master.endpoint + url + ": " + r.errorMessage());
    }

    VPackSlice const docs = responseBuilder->slice();
    if (!docs.isArray()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    std::string("got invalid response from master at ") +
                        config.master.endpoint + url +
                        ": response is not an array");
    }

    for (VPackSlice masterDoc : VPackArrayIterator(docs)) {
      if (!masterDoc.isObject()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          config.master.endpoint + url +
                          ": response document entry is not an object");
      }

      VPackSlice const keySlice = masterDoc.get(arangodb::StaticStrings::KeyString);
      if (!keySlice.isString()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          state.master.endpoint + ": document key is invalid");
      }

      VPackSlice const revSlice = masterDoc.get(arangodb::StaticStrings::RevString);
      if (!revSlice.isString()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          state.master.endpoint +
                          ": document revision is invalid");
      }

      TRI_ASSERT(options.indexOperationMode == arangodb::Index::OperationMode::internal);

      Result res = physical->insert(&trx, masterDoc, mdr, options);
      
      if (res.fail()) {
        if (res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) &&
            res.errorMessage() > keySlice.copyString()) {
          TRI_voc_rid_t rid =
              arangodb::transaction::helpers::extractRevFromDocument(masterDoc);
          if (physical->readDocument(&trx, arangodb::LocalDocumentId(rid), mdr)) {
            // already have exactly this revision no need to insert
            continue;
          }
          // remove conflict and retry
          // errorMessage() is this case contains the conflicting key
          auto inner = removeConflict(res.errorMessage());
          if (inner.fail()) {
            return res;
          }
          options.indexOperationMode = arangodb::Index::OperationMode::normal;
          res = physical->insert(&trx, masterDoc, mdr, options);
          
          options.indexOperationMode = arangodb::Index::OperationMode::internal;
          if (res.fail()) {
            return res;
          }
          // fall-through
        } else {
          int errorNumber = res.errorNumber();
          res.reset(errorNumber, std::string(TRI_errno_string(errorNumber)) +
                                     ": " + res.errorMessage());
          return res;
        }
      }

      ++stats.numDocsInserted;
    }
    current += docs.length();
  }

  return Result();
}
}  // namespace

namespace arangodb {

/// @brief helper struct to prevent multiple starts of the replication
/// in the same database. used for single-server replication only.
struct MultiStartPreventer {
  MultiStartPreventer(MultiStartPreventer const&) = delete;
  MultiStartPreventer& operator=(MultiStartPreventer const&) = delete;

  MultiStartPreventer(TRI_vocbase_t& vocbase, bool preventStart)
      : vocbase(vocbase), preventedStart(false) {
    if (preventStart) {
      TRI_ASSERT(!ServerState::instance()->isClusterRole());

      auto res = vocbase.replicationApplier()->preventStart();
      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
      preventedStart = true;
    }
  }

  ~MultiStartPreventer() {
    if (preventedStart) {
      // reallow starting
      TRI_ASSERT(!ServerState::instance()->isClusterRole());
      vocbase.replicationApplier()->allowStart();
    }
  }

  TRI_vocbase_t& vocbase;
  bool preventedStart;
};


DatabaseInitialSyncer::Configuration::Configuration(
    ReplicationApplierConfiguration const& a, replutils::BarrierInfo& bar,
    replutils::BatchInfo& bat, replutils::Connection& c, bool f, replutils::MasterInfo& m,
    replutils::ProgressInfo& p, SyncerState& s, TRI_vocbase_t& v)
    : applier{a}, barrier{bar}, batch{bat}, connection{c}, flushed{f}, master{m}, progress{p}, state{s}, vocbase{v} {}

bool DatabaseInitialSyncer::Configuration::isChild() const {
  return state.isChildSyncer;
}

DatabaseInitialSyncer::DatabaseInitialSyncer(TRI_vocbase_t& vocbase,
                                             ReplicationApplierConfiguration const& configuration)
    : InitialSyncer(configuration,
                    [this](std::string const& msg) -> void { setProgress(msg); }),
      _config{_state.applier,    _state.barrier, _batch,
              _state.connection, false,          _state.master,
              _progress,         _state,         vocbase},
      _isClusterRole(ServerState::instance()->isClusterRole()) {
  _state.vocbases.try_emplace(vocbase.name(), vocbase);

  if (configuration._database.empty()) {
    _state.databaseName = vocbase.name();
  }
}

/// @brief run method, performs a full synchronization
Result DatabaseInitialSyncer::runWithInventory(bool incremental, VPackSlice dbInventory,
                                               char const* context) {
  if (!_config.connection.valid()) {
    return Result(TRI_ERROR_INTERNAL, "invalid endpoint");
  }

  double const startTime = TRI_microtime();

  try {
    bool const preventMultiStart = !_isClusterRole;
    MultiStartPreventer p(vocbase(), preventMultiStart);
      
    setAborted(false);

    _config.progress.set("fetching master state");

    LOG_TOPIC("0a10d", DEBUG, Logger::REPLICATION)
        << "client: getting master state to dump " << vocbase().name();

    Result r = sendFlush();
    if (r.fail()) {
      return r;
    }

    if (!_config.isChild()) {
      r = _config.master.getState(_config.connection, _config.isChild(), context);

      if (r.fail()) {
        return r;
      }
    }

    TRI_ASSERT(!_config.master.endpoint.empty());
    TRI_ASSERT(_config.master.serverId.isSet());
    TRI_ASSERT(_config.master.majorVersion != 0);

    LOG_TOPIC("6fd2b", DEBUG, Logger::REPLICATION) << "client: got master state";
    if (incremental) {
      if (_config.master.majorVersion == 1 ||
          (_config.master.majorVersion == 2 && _config.master.minorVersion <= 6)) {
        LOG_TOPIC("15183", WARN, Logger::REPLICATION) << "incremental replication is "
                                                "not supported with a master < "
                                                "ArangoDB 2.7";
        incremental = false;
      }
    }


    if (!_config.isChild()) {
      // create a WAL logfile barrier that prevents WAL logfile collection
      r = _config.barrier.create(_config.connection, _config.master.lastLogTick);
      if (r.fail()) {
        return r;
      }

      // enable patching of collection count for ShardSynchronization Job
      std::string patchCount = StaticStrings::Empty;
      if (_config.applier._skipCreateDrop &&
          _config.applier._restrictType == ReplicationApplierConfiguration::RestrictType::Include &&
          _config.applier._restrictCollections.size() == 1) {
        patchCount = *_config.applier._restrictCollections.begin();
      }

      r = batchStart(patchCount);
      if (r.fail()) {
        return r;
      }

      startRecurringBatchExtension();
    }

    VPackSlice collections, views;
    if (dbInventory.isObject()) {
      collections = dbInventory.get("collections");  // required
      views = dbInventory.get("views");              // optional
    }
    VPackBuilder inventoryResponse;  // hold response data
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
      batchFinish();
    }

    if (r.fail()) {
      LOG_TOPIC("12556", DEBUG, Logger::REPLICATION)
          << "Error during initial sync: " << r.errorMessage();
    }

    LOG_TOPIC("055df", DEBUG, Logger::REPLICATION)
        << "initial synchronization with master took: "
        << Logger::FIXED(TRI_microtime() - startTime, 6) << " s. status: "
        << (r.errorMessage().empty() ? "all good" : r.errorMessage());

    return r;
  } catch (arangodb::basics::Exception const& ex) {
    if (!_config.isChild()) {
      batchFinish();
    }
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    if (!_config.isChild()) {
      batchFinish();
    }
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    if (!_config.isChild()) {
      batchFinish();
    }
    return Result(TRI_ERROR_NO_ERROR, "an unknown exception occurred");
  }
}

/// @brief fetch the server's inventory, public method for TailingSyncer
Result DatabaseInitialSyncer::getInventory(VPackBuilder& builder) {
  if (!_state.connection.valid()) {
    return Result(TRI_ERROR_INTERNAL, "invalid endpoint");
  }

  auto r = batchStart();
  if (r.fail()) {
    return r;
  }

  TRI_DEFER(batchFinish());

  // caller did not supply an inventory, we need to fetch it
  return fetchInventory(builder);
}

/// @brief check whether the initial synchronization should be aborted
bool DatabaseInitialSyncer::isAborted() const {
  if (vocbase().server().isStopping() ||
      (vocbase().replicationApplier() != nullptr &&
       vocbase().replicationApplier()->stopInitialSynchronization())) {
    return true;
  }

  return Syncer::isAborted();
}

void DatabaseInitialSyncer::setProgress(std::string const& msg) {
  _config.progress.message = msg;

  if (_config.applier._verbose) {
    LOG_TOPIC("c6f5f", INFO, Logger::REPLICATION) << msg;
  } else {
    LOG_TOPIC("d15ed", DEBUG, Logger::REPLICATION) << msg;
  }

  if (!_isClusterRole) {
    auto* applier = _config.vocbase.replicationApplier();

    if (applier != nullptr) {
      applier->setProgress(msg);
    }
  }
}

/// @brief send a WAL flush command
Result DatabaseInitialSyncer::sendFlush() {
  if (isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  if (_state.master.engine == "rocksdb") {
    // no WAL flush required for RocksDB. this is only relevant for MMFiles
    return Result();
  }

  std::string const url = "/_admin/wal/flush";

  VPackBuilder builder;
  builder.openObject();
  builder.add("waitForSync", VPackValue(true));
  builder.add("waitForCollector", VPackValue(true));
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
Result DatabaseInitialSyncer::parseCollectionDumpMarker(transaction::Methods& trx,
                                                        LogicalCollection* coll,
                                                        VPackSlice const& marker) {
  if (!marker.isObject()) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  TRI_replication_operation_e type = REPLICATION_INVALID;
  VPackSlice doc;

  for (auto it : VPackObjectIterator(marker, true)) {
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
                                                  httpclient::SimpleHttpResult* response,
                                                  uint64_t& markersProcessed) {
  TRI_ASSERT(!trx.isSingleOperationTransaction());

  basics::StringBuffer const& data = response->getBody();
  char const* p = data.begin();
  char const* end = p + data.length();

  bool found = false;
  std::string const& cType =
      response->getHeaderField(StaticStrings::ContentTypeHeader, found);
  if (found && (cType == StaticStrings::MimeTypeVPack)) {
    LOG_TOPIC("b9f4d", DEBUG, Logger::REPLICATION) << "using vpack for chunk contents";
    
    VPackValidator validator(&basics::VelocyPackHelper::strictRequestValidationOptions);

    try {
      while (p < end) {
        size_t remaining = static_cast<size_t>(end - p);
        // throws if the data is invalid
        validator.validate(p, remaining, /*isSubPart*/ true);

        VPackSlice marker(reinterpret_cast<uint8_t const*>(p));
        Result r = parseCollectionDumpMarker(trx, coll, marker);
        
        TRI_ASSERT(!r.is(TRI_ERROR_ARANGO_TRY_AGAIN));
        if (r.fail()) {
          r.reset(r.errorNumber(),
                  std::string("received invalid dump data for collection '") +
                      coll->name() + "'");
          return r;
        }
        ++markersProcessed;
        p += marker.byteSize();
      }
    } catch (velocypack::Exception const& e) {
      LOG_TOPIC("b9f4f", ERR, Logger::REPLICATION)
          << "Error parsing VPack response: " << e.what();
      return Result(e.errorCode(), e.what());
    }

  } else {
    // buffer must end with a NUL byte
    TRI_ASSERT(*end == '\0');
    LOG_TOPIC("bad5d", DEBUG, Logger::REPLICATION) << "using json for chunk contents";


    VPackBuilder builder;
    VPackParser parser(builder, &basics::VelocyPackHelper::strictRequestValidationOptions);

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
        LOG_TOPIC("746ea", ERR, Logger::REPLICATION)
            << "while parsing collection dump: " << e.what();
        return Result(e.errorCode(), e.what());
      }

      p = q + 1;

      Result r = parseCollectionDumpMarker(trx, coll, builder.slice());
      TRI_ASSERT(!r.is(TRI_ERROR_ARANGO_TRY_AGAIN));
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
                                           InitialSyncerDumpStats& stats, int batch,
                                           TRI_voc_tick_t fromTick, uint64_t chunkSize) {
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
  bool const useAsync =
      (batch == 1 && _state.master.engine != engineName);

  try {
    std::string const typeString = (coll->type() == TRI_COL_TYPE_EDGE ? "edge" : "document");

    if (!_config.isChild()) {
      batchExtend();
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
    int vv = _config.master.majorVersion * 1000000 + _config.master.minorVersion * 1000;
    if (vv >= 3003009) {
      headers[StaticStrings::Accept] = StaticStrings::MimeTypeVPack;
    }
#endif

    _config.progress.set(
        std::string("fetching master collection dump for collection '") +
        coll->name() + "', type: " + typeString + ", id: " + leaderColl +
        ", batch " + itoa(batch) + ", url: " + url);

    ++stats.numDumpRequests;
    double t = TRI_microtime();

    // send request
    std::unique_ptr<httpclient::SimpleHttpResult> response;
    _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
      response.reset(client->retryRequest(rest::RequestType::GET, url, nullptr, 0, headers));
    });

    t = TRI_microtime() - t;
    if (replutils::hasFailed(response.get())) {
      stats.waitedForDump += t;
      sharedStatus->gotResponse(
          replutils::buildHttpError(response.get(), url, _config.connection), t);
      return;
    }

    // use async mode for first batch
    if (useAsync) {
      bool found = false;
      std::string jobId = response->getHeaderField(StaticStrings::AsyncId, found);

      if (!found) {
        sharedStatus->gotResponse(
            Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                   std::string("got invalid response from master at ") +
                       _config.master.endpoint + url +
                       ": could not find 'X-Arango-Async' header"));
        return;
      }

      double const startTime = TRI_microtime();

      // wait until we get a reasonable response
      while (true) {
        if (!_config.isChild()) {
          batchExtend();
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
            sharedStatus->gotResponse(
                Result(TRI_ERROR_REPLICATION_NO_RESPONSE,
                       std::string("job not found on master at ") +
                           _config.master.endpoint));
            return;
          }
        }

        double waitTime = TRI_microtime() - startTime;

        if (static_cast<uint64_t>(waitTime * 1000.0 * 1000.0) >=
            _config.applier._initialSyncMaxWaitTime) {
          sharedStatus->gotResponse(Result(
              TRI_ERROR_REPLICATION_NO_RESPONSE,
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

    stats.waitedForDump += t;

    if (replutils::hasFailed(response.get())) {
      // failure
      sharedStatus->gotResponse(
          replutils::buildHttpError(response.get(), url, _config.connection), t);
    } else {
      // success!
      sharedStatus->gotResponse(std::move(response), t);
    }
  } catch (basics::Exception const& ex) {
    sharedStatus->gotResponse(Result(ex.code(), ex.what()));
  } catch (std::exception const& ex) {
    sharedStatus->gotResponse(Result(TRI_ERROR_INTERNAL, ex.what()));
  }
}

/// @brief incrementally fetch data from a collection
Result DatabaseInitialSyncer::fetchCollectionDump(arangodb::LogicalCollection* coll,
                                                  std::string const& leaderColl,
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
    std::string header =
        dumpResponse->getHeaderField(StaticStrings::ReplicationHeaderCheckMore, found);
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
      header = dumpResponse->getHeaderField(StaticStrings::ReplicationHeaderLastIncluded, found);
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
      sharedStatus->request([this, self, &stats, &baseUrl, sharedStatus, coll,
                             leaderColl, batch, fromTick, chunkSize]() {
        fetchDumpChunk(sharedStatus, baseUrl, coll, leaderColl, stats,
                       batch + 1, fromTick, chunkSize);
      });
    }

    SingleCollectionTransaction trx(transaction::StandaloneContext::Create(vocbase()),
                                    *coll, AccessMode::Type::EXCLUSIVE);

    // do not index the operations in our own transaction
    trx.addHint(transaction::Hints::Hint::NO_INDEXING);

    // smaller batch sizes should work better here
#if VPACK_DUMP
//    trx.state()->options().intermediateCommitCount = 128;
//    trx.state()->options().intermediateCommitSize = 16 * 1024 * 1024;
#endif

    res = trx.begin();

    if (!res.ok()) {
      return Result(res.errorNumber(),
                    std::string("unable to start transaction: ") + res.errorMessage());
    }

    double t = TRI_microtime();
    TRI_ASSERT(!trx.isSingleOperationTransaction());
    res = parseCollectionDump(trx, coll, dumpResponse.get(), markersProcessed);

    if (res.fail()) {
      TRI_ASSERT(!res.is(TRI_ERROR_ARANGO_TRY_AGAIN));
      return res;
    }

    res = trx.commit();

    double applyTime = TRI_microtime() - t;
    stats.waitedForApply += applyTime;

    _config.progress.set(
        std::string("fetched master collection dump for collection '") +
        coll->name() + "', type: " + typeString + ", id: " + leaderColl +
        ", batch " + itoa(batch) + ", markers processed: " + itoa(markersProcessed) +
        ", bytes received: " + itoa(bytesReceived) +
        ", apply time: " + std::to_string(applyTime) + " s");

    if (!res.ok()) {
      return res;
    }

    if (!checkMore || fromTick == 0) {
      // done
      _config.progress.set(
          std::string("finished initial dump for collection '") + coll->name() +
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
Result DatabaseInitialSyncer::fetchCollectionSync(arangodb::LogicalCollection* coll,
                                                  std::string const& leaderColl,
                                                  TRI_voc_tick_t maxTick) {
  if (coll->syncByRevision() &&
      (_config.master.majorVersion > 3 ||
       (_config.master.majorVersion == 3 && _config.master.minorVersion >= 7))) {
    // local collection should support revisions, and master is at least aware
    // of the revision-based protocol, so we can query it to find out if we
    // can use the new protocol; will fall back to old one if master collection
    // is an old variant
    return fetchCollectionSyncByRevisions(coll, leaderColl, maxTick);
  }
  return fetchCollectionSyncByKeys(coll, leaderColl, maxTick);
}

/// @brief incrementally fetch data from a collection using keys as the primary
/// document identifier
Result DatabaseInitialSyncer::fetchCollectionSyncByKeys(arangodb::LogicalCollection* coll,
                                                        std::string const& leaderColl,
                                                        TRI_voc_tick_t maxTick) {
  using ::arangodb::basics::StringUtils::urlEncode;

  if (!_config.isChild()) {
    batchExtend();
    _config.barrier.extend(_config.connection);
  }

  std::string const baseUrl = replutils::ReplicationUrl + "/keys";
  std::string url = baseUrl + "/keys" + "?collection=" + urlEncode(leaderColl) +
                    "&to=" + std::to_string(maxTick) +
                    "&serverId=" + _state.localServerIdString +
                    "&batchId=" + std::to_string(_config.batch.id);

  std::string msg =
      "fetching collection keys for collection '" + coll->name() + "' from " + url;
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
    response.reset(client->retryRequest(rest::RequestType::POST, url, nullptr, 0, headers));
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
      batchExtend();
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

    if (static_cast<uint64_t>(waitTime * 1000.0 * 1000.0) >= _config.applier._initialSyncMaxWaitTime) {
      return Result(TRI_ERROR_REPLICATION_NO_RESPONSE,
                    std::string(
                        "timed out waiting for response from master at ") +
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
                      _config.master.endpoint + url + ": response is no object");
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
      response.reset(client->retryRequest(rest::RequestType::DELETE_REQ, url, nullptr, 0));
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
    SingleCollectionTransaction trx(transaction::StandaloneContext::Create(vocbase()),
                                    *coll, AccessMode::Type::EXCLUSIVE);
    trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
    trx.addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
    Result res = trx.begin();

    if (!res.ok()) {
      return Result(res.errorNumber(),
                    std::string("unable to start transaction (") + std::string(__FILE__) +
                        std::string(":") + std::to_string(__LINE__) +
                        std::string("): ") + res.errorMessage());
    }

    OperationOptions options;

    if (!_state.leaderId.empty()) {
      options.isSynchronousReplicationFrom = _state.leaderId;
    }

    OperationResult opRes = trx.truncate(coll->name(), options);

    if (opRes.fail()) {
      return Result(opRes.errorNumber(),
                    std::string("unable to truncate collection '") + coll->name() +
                        "': " + TRI_errno_string(opRes.errorNumber()));
    }

    return trx.finish(opRes.result);
  }

  // now we can fetch the complete chunk information from the master
  try {
    return EngineSelectorFeature::ENGINE->handleSyncKeys(*this, *coll, keysId.copyString());
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }
}

/// @brief incrementally fetch data from a collection using keys as the primary
/// document identifier
Result DatabaseInitialSyncer::fetchCollectionSyncByRevisions(arangodb::LogicalCollection* coll,
                                                             std::string const& leaderColl,
                                                             TRI_voc_tick_t maxTick) {
  using ::arangodb::basics::StringUtils::urlEncode;
  using ::arangodb::transaction::Hints;

  InitialSyncerIncrementalSyncStats stats;
  double const startTime = TRI_microtime();

  if (!_config.isChild()) {
    batchExtend();
    _config.barrier.extend(_config.connection);
  }

  std::unique_ptr<containers::RevisionTree> treeMaster;
  std::string const baseUrl =
      replutils::ReplicationUrl + "/" + RestReplicationHandler::Revisions;

  // get master tree
  {
    std::string url = baseUrl + "/" + RestReplicationHandler::Tree +
                      "?collection=" + urlEncode(leaderColl) +
                      "&to=" + std::to_string(maxTick) +
                      "&serverId=" + _state.localServerIdString +
                      "&batchId=" + std::to_string(_config.batch.id);

    std::string msg = "fetching collection revision tree for collection '" +
                      coll->name() + "' from " + url;
    _config.progress.set(msg);

    auto headers = replutils::createHeaders();
    std::unique_ptr<httpclient::SimpleHttpResult> response;
    double t = TRI_microtime();
    _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
      response.reset(client->retryRequest(rest::RequestType::GET, url, nullptr, 0, headers));
    });
    stats.waitedForInitial += TRI_microtime() - t;

    if (replutils::hasFailed(response.get())) {
      if (response && response->getHttpReturnCode() ==
                          static_cast<int>(rest::ResponseCode::NOT_IMPLEMENTED)) {
        // collection on master doesn't support revisions-based protocol, fallback
        return fetchCollectionSyncByKeys(coll, leaderColl, maxTick);
      }
      return replutils::buildHttpError(response.get(), url, _config.connection);
    }

    auto body = response->getBodyVelocyPack();
    if (!body) {
      return Result(
          TRI_ERROR_INTERNAL,
          "received improperly formed response when fetching revision tree");
    }
    treeMaster = containers::RevisionTree::deserialize(body->slice());
    if (!treeMaster) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    std::string("got invalid response from master at ") +
                        _config.master.endpoint + url +
                        ": response does not contain a valid revision tree");
    }

    if (treeMaster->count() == 0) {
      // remote collection has no documents. now truncate our local collection
      SingleCollectionTransaction trx(transaction::StandaloneContext::Create(vocbase()),
                                      *coll, AccessMode::Type::EXCLUSIVE);
      trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
      trx.addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
      Result res = trx.begin();

      if (!res.ok()) {
        return Result(res.errorNumber(),
                      std::string("unable to start transaction (") + std::string(__FILE__) +
                          std::string(":") + std::to_string(__LINE__) +
                          std::string("): ") + res.errorMessage());
      }

      OperationOptions options;

      if (!_state.leaderId.empty()) {
        options.isSynchronousReplicationFrom = _state.leaderId;
      }

      OperationResult opRes = trx.truncate(coll->name(), options);

      if (opRes.fail()) {
        return Result(opRes.errorNumber(),
                      std::string("unable to truncate collection '") + coll->name() +
                          "': " + TRI_errno_string(opRes.errorNumber()));
      }

      return trx.finish(opRes.result);
    }
  }

  if (isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  PhysicalCollection* physical = coll->getPhysical();
  TRI_ASSERT(physical);
  auto context = arangodb::transaction::StandaloneContext::Create(coll->vocbase());
  TRI_voc_tid_t blockerId = context->generateId();
  physical->placeRevisionTreeBlocker(blockerId);
  std::unique_ptr<arangodb::SingleCollectionTransaction> trx;
  try {
    trx = std::make_unique<arangodb::SingleCollectionTransaction>(
        context, *coll, arangodb::AccessMode::Type::EXCLUSIVE);
  } catch (arangodb::basics::Exception const& ex) {
    if (ex.code() == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
      bool locked = false;
      TRI_vocbase_col_status_e status = coll->tryFetchStatus(locked);
      if (!locked) {
        // fall back to unsafe method as last resort
        status = coll->status();
      }
      if (status == TRI_vocbase_col_status_e::TRI_VOC_COL_STATUS_DELETED) {
        // TODO handle
        setAborted(true); // probably wrong?
        return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
      }
    }
    return Result(ex.code());
  }
  trx->addHint(Hints::Hint::NO_INDEXING);
  // turn on intermediate commits as the number of keys to delete can be huge
  // here
  trx->addHint(Hints::Hint::INTERMEDIATE_COMMITS);
  Result res = trx->begin();
  if (!res.ok()) {
    return Result(res.errorNumber(),
                  std::string("unable to start transaction: ") + res.errorMessage());
  }
  auto guard = scopeGuard(
      [trx = trx.get()]() -> void { 
        if (trx->status() == transaction::Status::RUNNING) {
          trx->abort();
        }
       });


  // diff with local tree
  //std::pair<std::size_t, std::size_t> fullRange = treeMaster->range();
  std::unique_ptr<containers::RevisionTree> treeLocal =
      physical->revisionTree(*trx);
  physical->removeRevisionTreeBlocker(blockerId);
  std::vector<std::pair<std::size_t, std::size_t>> ranges = treeMaster->diff(*treeLocal);
  if (ranges.empty()) {
    // no differences, done!
    setProgress("no differences between two revision trees, ending");
    return Result{};
  }

  // now lets get the actual ranges and handle the differences

  {
    VPackBuilder requestBuilder;
    {
      char ridBuffer[11];
      VPackArrayBuilder list(&requestBuilder);
      for (auto& pair : ranges) {
        VPackArrayBuilder range(&requestBuilder);
        requestBuilder.add(
            basics::HybridLogicalClock::encodeTimeStampToValuePair(pair.first, ridBuffer));
        requestBuilder.add(
            basics::HybridLogicalClock::encodeTimeStampToValuePair(pair.second, ridBuffer));
      }
    }
    std::string request = requestBuilder.slice().toJson();

    std::string url = baseUrl + "/" + RestReplicationHandler::Ranges +
                      "?collection=" + urlEncode(leaderColl) +
                      "&serverId=" + _state.localServerIdString +
                      "&batchId=" + std::to_string(_config.batch.id);
    auto headers = replutils::createHeaders();
    std::unique_ptr<httpclient::SimpleHttpResult> response;
    TRI_voc_rid_t requestResume = ranges[0].first;  // start with beginning
    TRI_ASSERT(requestResume >= coll->minRevision());
    TRI_voc_rid_t iterResume = static_cast<TRI_voc_rid_t>(requestResume);
    std::size_t chunk = 0;
    std::unique_ptr<ReplicationIterator> iter =
        physical->getReplicationIterator(ReplicationIterator::Ordering::Revision, *trx);
    if (!iter) {
      return Result(TRI_ERROR_INTERNAL, "could not get replication iterator");
    }

    std::vector<std::size_t> toFetch;
    std::vector<std::size_t> toRemove;
    const uint64_t documentsFound = treeLocal->count();
    RevisionReplicationIterator& local =
        *static_cast<RevisionReplicationIterator*>(iter.get());

    while (requestResume < std::numeric_limits<TRI_voc_rid_t>::max()) {
      if (isAborted()) {
        return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
      }

      if (!_config.isChild()) {
        batchExtend();
        _config.barrier.extend(_config.connection);
      }

      std::string batchUrl =
          url + "&" + StaticStrings::RevisionTreeResume + "=" +
          basics::HybridLogicalClock::encodeTimeStamp(requestResume);
      std::string msg = "fetching collection revision ranges for collection '" +
                        coll->name() + "' from " + batchUrl;
      _config.progress.set(msg);
      double t = TRI_microtime();
      _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
        response.reset(client->retryRequest(rest::RequestType::PUT, batchUrl,
                                            request.data(), request.size(), headers));
      });
      stats.waitedForKeys += TRI_microtime() - t;
      ++stats.numKeysRequests;

      if (replutils::hasFailed(response.get())) {
        return replutils::buildHttpError(response.get(), batchUrl, _config.connection);
      }

      VPackBuilder responseBuilder;
      Result r = replutils::parseResponse(responseBuilder, response.get());
      if (r.fail()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          _config.master.endpoint + batchUrl + ": " + r.errorMessage());
      }

      VPackSlice const slice = responseBuilder.slice();
      if (!slice.isObject()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          _config.master.endpoint + batchUrl +
                          ": response is not an object");
      }

      VPackSlice const resumeSlice = slice.get("resume");
      if (!resumeSlice.isNone() && !resumeSlice.isString()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          _config.master.endpoint + batchUrl +
                          ": response field 'resume' is not a number");
      }
      requestResume = resumeSlice.isNone()
                          ? std::numeric_limits<TRI_voc_rid_t>::max()
                          : basics::HybridLogicalClock::decodeTimeStamp(resumeSlice);

      VPackSlice const rangesSlice = slice.get("ranges");
      if (!rangesSlice.isArray()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      std::string("got invalid response from master at ") +
                          _config.master.endpoint + batchUrl +
                          ": response field 'ranges' is not an array");
      }

      for (VPackSlice masterSlice : VPackArrayIterator(rangesSlice)) {
        if (!masterSlice.isArray()) {
          return Result(
              TRI_ERROR_REPLICATION_INVALID_RESPONSE,
              std::string("got invalid response from master at ") +
                  _config.master.endpoint + batchUrl +
                  ": response field 'ranges' entry is not a revision range");
        }
        auto& currentRange = ranges[chunk];
        if (!local.hasMore() ||
            local.revision() < static_cast<TRI_voc_rid_t>(currentRange.first)) {
          local.seek(std::max(iterResume,
                              static_cast<TRI_voc_rid_t>(currentRange.first)));
        }

        TRI_voc_rid_t removalBound =
            masterSlice.isEmptyArray()
                ? currentRange.second + 1
                : basics::HybridLogicalClock::decodeTimeStamp(masterSlice.at(0));
        TRI_ASSERT(currentRange.first <= removalBound);
        TRI_ASSERT(removalBound <= currentRange.second + 1);
        std::size_t mixedBound = masterSlice.isEmptyArray()
                                     ? currentRange.second
                                     : basics::HybridLogicalClock::decodeTimeStamp(
                                           masterSlice.at(masterSlice.length() - 1));
        TRI_ASSERT(currentRange.first <= mixedBound);
        TRI_ASSERT(mixedBound <= currentRange.second);

        while (local.hasMore() && local.revision() < removalBound) {
          toRemove.emplace_back(local.revision());
          iterResume = std::max(iterResume, local.revision() + 1);
          local.next();
        }

        std::size_t index = 0;
        while (local.hasMore() && local.revision() <= mixedBound) {
          TRI_voc_rid_t masterRev =
              basics::HybridLogicalClock::decodeTimeStamp(masterSlice.at(index));

          if (local.revision() < masterRev) {
            toRemove.emplace_back(local.revision());
            iterResume = std::max(iterResume, local.revision() + 1);
            local.next();
          } else if (masterRev < local.revision()) {
            toFetch.emplace_back(masterRev);
            ++index;
            iterResume = std::max(iterResume, masterRev + 1);
          } else {
            TRI_ASSERT(local.revision() == masterRev);
            // match, no need to remove local or fetch from master
            ++index;
            iterResume = std::max(iterResume, masterRev + 1);
            local.next();
          }
        }
        for (; index < masterSlice.length(); ++index) {
          TRI_voc_rid_t masterRev =
              basics::HybridLogicalClock::decodeTimeStamp(masterSlice.at(index));
          // fetch any leftovers
          toFetch.emplace_back(masterRev);
          iterResume = std::max(iterResume, masterRev + 1);
        }

        while (local.hasMore() &&
               local.revision() <= std::min(requestResume - 1, static_cast<TRI_voc_rid_t>(currentRange.second))) {
          toRemove.emplace_back(local.revision());
          iterResume = std::max(iterResume, local.revision() + 1);
          local.next();
        }

        if (requestResume > currentRange.second) {
          ++chunk;
        }
      }

      Result res = ::removeRevisions(*trx, *coll, toRemove, stats);
      if (res.fail()) {
        return res;
      }
      toRemove.clear();

      if (!_state.isChildSyncer) {
        _state.barrier.extend(_state.connection);
      }

      res = ::fetchRevisions(*trx, _config, _state, *coll, leaderColl, toFetch,
                             /*removed,*/ stats);
      if (res.fail()) {
        return res;
      }
      toFetch.clear();
    }

    // adjust counts
    {
      uint64_t numberDocumentsAfterSync =
          documentsFound + stats.numDocsInserted - stats.numDocsRemoved;
      uint64_t numberDocumentsDueToCounter =
          coll->numberDocuments(trx.get(), transaction::CountType::Normal);

      setProgress(std::string("number of remaining documents in collection '") +
                  coll->name() + "': " + std::to_string(numberDocumentsAfterSync) +
                  ", number of documents due to collection count: " +
                  std::to_string(numberDocumentsDueToCounter));

      if (numberDocumentsAfterSync != numberDocumentsDueToCounter) {
        LOG_TOPIC("118bf", WARN, Logger::REPLICATION)
            << "number of remaining documents in collection '" + coll->name() +
                   "' is " + std::to_string(numberDocumentsAfterSync) +
                   " and differs from number of documents returned by "
                   "collection count " +
                   std::to_string(numberDocumentsDueToCounter);

        // patch the document counter of the collection and the transaction
        int64_t diff = static_cast<int64_t>(numberDocumentsAfterSync) -
                       static_cast<int64_t>(numberDocumentsDueToCounter);

        trx->documentCollection()->getPhysical()->adjustNumberDocuments(*trx, diff);
      }
    }

    res = trx->commit();
    if (res.fail()) {
      return res;
    }
    TRI_ASSERT(requestResume == std::numeric_limits<std::size_t>::max());
  }

  setProgress(
      std::string("incremental sync statistics for collection '") + coll->name() +
      "': " + "keys requests: " + std::to_string(stats.numKeysRequests) + ", " +
      "docs requests: " + std::to_string(stats.numDocsRequests) + ", " +
      "number of documents requested: " + std::to_string(stats.numDocsRequested) +
      ", " + "number of documents inserted: " + std::to_string(stats.numDocsInserted) +
      ", " + "number of documents removed: " + std::to_string(stats.numDocsRemoved) +
      ", " + "waited for initial: " + std::to_string(stats.waitedForInitial) +
      " s, " + "waited for keys: " + std::to_string(stats.waitedForKeys) +
      " s, " + "waited for docs: " + std::to_string(stats.waitedForDocs) +
      " s, " + "waited for insertions: " + std::to_string(stats.waitedForInsertions) +
      " s, " + "waited for removals: " + std::to_string(stats.waitedForRemovals) +
      " s, " + "waited for key lookups: " + std::to_string(stats.waitedForKeyLookups) +
      " s, " + "total time: " + std::to_string(TRI_microtime() - startTime) +
      " s");

  return Result{};
}

/// @brief incrementally fetch data from a collection
/// @brief changes the properties of a collection, based on the VelocyPack
/// provided
Result DatabaseInitialSyncer::changeCollection(arangodb::LogicalCollection* col,
                                               VPackSlice const& slice) {
  arangodb::CollectionGuard guard(&vocbase(), col->id());

  return guard.collection()->properties(slice, false);  // always a full-update
}

/// @brief whether or not the collection has documents
bool DatabaseInitialSyncer::hasDocuments(arangodb::LogicalCollection const& col) {
  return col.getPhysical()->hasDocuments();
}

/// @brief handle the information about a collection
Result DatabaseInitialSyncer::handleCollection(VPackSlice const& parameters,
                                               VPackSlice const& indexes,
                                               bool incremental, SyncPhase phase) {
  using ::arangodb::basics::StringUtils::itoa;

  if (isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  if (!parameters.isObject() || !indexes.isArray()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE);
  }

  if (!_config.isChild()) {
    batchExtend();
    _config.barrier.extend(_config.connection);
  }

  std::string const masterName =
      basics::VelocyPackHelper::getStringValue(parameters, "name", "");

  TRI_voc_cid_t const masterCid = basics::VelocyPackHelper::extractIdValue(parameters);

  if (masterCid == 0) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "collection id is missing in response");
  }

  std::string const masterUuid =
      basics::VelocyPackHelper::getStringValue(parameters, "globallyUniqueId",
                                               "");

  VPackSlice const type = parameters.get("type");

  if (!type.isNumber()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "collection type is missing in response");
  }

  std::string const typeString = (type.getNumber<int>() == 3 ? "edge" : "document");

  std::string const collectionMsg = "collection '" + masterName + "', type " +
                                    typeString + ", id " + itoa(masterCid);

  // phase handling
  if (phase == PHASE_VALIDATE) {
    // validation phase just returns ok if we got here (aborts above if data is
    // invalid)
    _config.progress.processedCollections.try_emplace(masterCid, masterName);

    return Result();
  }

  // drop and re-create collections locally
  // -------------------------------------------------------------------------------------

  if (phase == PHASE_DROP_CREATE) {
    auto* col = resolveCollection(vocbase(), parameters).get();

    if (col == nullptr) {
      // not found...
      col = vocbase().lookupCollection(masterName).get();

      if (col != nullptr && (col->name() != masterName ||
                             (!masterUuid.empty() && col->guid() != masterUuid))) {
        // found another collection with the same name locally.
        // in this case we must drop it because we will run into duplicate
        // name conflicts otherwise
        try {
          auto res = vocbase().dropCollection(col->id(), true, -1.0).errorNumber();

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

            SingleCollectionTransaction trx(transaction::StandaloneContext::Create(vocbase()),
                                            *col, AccessMode::Type::EXCLUSIVE);
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

            auto res = vocbase().dropCollection(col->id(), true, -1.0).errorNumber();

            if (res != TRI_ERROR_NO_ERROR) {
              return Result(res, std::string("unable to drop ") + collectionMsg +
                                     ": " + TRI_errno_string(res));
            }
          }
        }
      } else {
        // incremental case
        TRI_ASSERT(incremental);

        // collection is already present
        _config.progress.set("checking/changing parameters of " + collectionMsg);
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

    LOG_TOPIC("7093d", DEBUG, Logger::REPLICATION)
        << "Dump is creating collection " << parameters.toJson();

    auto r = createCollection(vocbase(), parameters, &col);

    if (r.fail()) {
      return Result(r.errorNumber(), std::string("unable to create ") + collectionMsg +
                                         ": " + TRI_errno_string(r.errorNumber()) +
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
                        " not found on slave. Collection info " + parameters.toJson());
    }

    std::string const& masterColl = !masterUuid.empty() ? masterUuid : itoa(masterCid);
    auto res = incremental && hasDocuments(*col) 
                   ? fetchCollectionSync(col, masterColl, _config.master.lastLogTick)
                   : fetchCollectionDump(col, masterColl, _config.master.lastLogTick);

    if (!res.ok()) {
      return res;
    } else if (isAborted()) {
      return res.reset(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }

    if (masterName == TRI_COL_NAME_USERS) {
      reloadUsers();
    } else if (masterName == StaticStrings::AnalyzersCollection &&
               ServerState::instance()->isSingleServer() &&
               vocbase().server().hasFeature<iresearch::IResearchAnalyzerFeature>()) {
        vocbase().server().getFeature<iresearch::IResearchAnalyzerFeature>().invalidate(vocbase());
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
        batchExtend();
        _config.barrier.extend(_config.connection);
      }

      _config.progress.set("creating " + std::to_string(numIdx) +
                           " index(es) for " + collectionMsg);

      try {
        for (auto const& idxDef : VPackArrayIterator(indexes)) {
          if (idxDef.isObject()) {
            VPackSlice const type = idxDef.get(StaticStrings::IndexType);
            if (type.isString()) {
              _config.progress.set("creating index of type " +
                                   type.copyString() + " for " + collectionMsg);
            }
          }

          createIndexInternal(idxDef, *col);
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
  if (_config.applier._includeFoxxQueues) {
    url += "&includeFoxxQueues=true";
  }

  // send request
  _config.progress.set("fetching master inventory from " + url);
  std::unique_ptr<httpclient::SimpleHttpResult> response;
  _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
    response.reset(client->retryRequest(rest::RequestType::GET, url, nullptr, 0));
  });

  if (replutils::hasFailed(response.get())) {
    if (!_config.isChild()) {
      batchFinish();
    }
    return replutils::buildHttpError(response.get(), url, _config.connection);
  }

  Result r = replutils::parseResponse(builder, response.get());

  if (r.fail()) {
    return Result(
        r.errorNumber(),
        std::string("got invalid response from master at ") + _config.master.endpoint +
            url + ": invalid response type for initial data. expecting array");
  }

  VPackSlice const slice = builder.slice();
  if (!slice.isObject()) {
    LOG_TOPIC("3b1e6", DEBUG, Logger::REPLICATION)
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

  std::vector<std::pair<VPackSlice, VPackSlice>> systemCollections;
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

    if (TRI_ExcludeCollectionReplication(masterName, _config.applier._includeSystem,
                                         _config.applier._includeFoxxQueues)) {
      continue;
    }

    if (basics::VelocyPackHelper::getBooleanValue(parameters, "deleted", false)) {
      // we don't care about deleted collections
      continue;
    }

    if (_config.applier._restrictType != ReplicationApplierConfiguration::RestrictType::None) {
      auto const it = _config.applier._restrictCollections.find(masterName);
      bool found = (it != _config.applier._restrictCollections.end());

      if (_config.applier._restrictType == ReplicationApplierConfiguration::RestrictType::Include &&
          !found) {
        // collection should not be included
        continue;
      } else if (_config.applier._restrictType ==
                     ReplicationApplierConfiguration::RestrictType::Exclude &&
                 found) {
        // collection should be excluded
        continue;
      }
    }

    if (masterName == StaticStrings::AnalyzersCollection) {
      // _analyzers collection has to be restored before view creation
      systemCollections.emplace_back(parameters, indexes);
    } else {
      collections.emplace_back(parameters, indexes);
    }
  }

  // STEP 1: validate collection declarations from master
  // ----------------------------------------------------------------------------------

  // STEP 2: drop and re-create collections locally if they are also present on
  // the master
  //  ------------------------------------------------------------------------------------

  // iterate over all collections from the master...
  std::array<SyncPhase, 2> phases{{PHASE_VALIDATE, PHASE_DROP_CREATE}};
  for (auto const& phase : phases) {
    Result r = iterateCollections(systemCollections, incremental, phase);

    if (r.fail()) {
      return r;
    }

    r = iterateCollections(collections, incremental, phase);

    if (r.fail()) {
      return r;
    }
  }

  // STEP 3: restore data for system collections
  // ----------------------------------------------------------------------------------
  auto const res = iterateCollections(systemCollections, incremental, PHASE_DUMP);

  if (res.fail()) {
    return res;
  }

  // STEP 4: now that the collections exist create the views
  // this should be faster than re-indexing afterwards
  // ----------------------------------------------------------------------------------

  if (!_config.applier._skipCreateDrop &&
      _config.applier._restrictCollections.empty() &&
      viewSlices.isArray()) {
    // views are optional, and 3.3 and before will not send any view data
    Result r = handleViewCreation(viewSlices);  // no requests to master
    if (r.fail()) {
      LOG_TOPIC("96cda", ERR, Logger::REPLICATION)
          << "Error during intial sync view creation: " << r.errorMessage();
      return r;
    }
  } else {
    _config.progress.set("view creation skipped because of configuration");
  }

  // STEP 5: sync collection data from master and create initial indexes
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
  for (VPackSlice slice : VPackArrayIterator(views)) {
    Result res = createView(vocbase(), slice);
    if (res.fail()) {
      return res;
    }
  }
  return {};
}

Result DatabaseInitialSyncer::batchStart(std::string const& patchCount) {
  return _config.batch.start(_config.connection, _config.progress,
                             _config.master, _config.state.syncerId, patchCount);
}

Result DatabaseInitialSyncer::batchExtend() {
  return _config.batch.extend(_config.connection, _config.progress, _config.state.syncerId);
}

Result DatabaseInitialSyncer::batchFinish() {
  return _config.batch.finish(_config.connection, _config.progress, _config.state.syncerId);
}


}  // namespace arangodb
