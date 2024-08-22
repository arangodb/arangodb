////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/system-functions.h"
#include "Indexes/Index.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "Logger/Logger.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationFeature.h"
#include "Replication/utilities.h"
#include "Rest/CommonDefines.h"
#include "RestHandler/RestReplicationHandler.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/IndexesSnapshot.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/OperationOptions.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <absl/strings/str_cat.h>
#include <absl/strings/escaping.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Validator.h>

#include <array>
#include <cstring>
#include <string>
#include <string_view>

namespace arangodb {
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

bool isVelocyPack(httpclient::SimpleHttpResult const& response) {
  bool found = false;
  std::string const& cType =
      response.getHeaderField(StaticStrings::ContentTypeHeader, found);
  return found && cType == StaticStrings::MimeTypeVPack;
}

std::string const kTypeString = "type";
std::string const kDataString = "data";

Result removeRevisions(transaction::Methods& trx, LogicalCollection& collection,
                       std::vector<RevisionId>& toRemove,
                       ReplicationMetricsFeature::InitialSyncStats& stats) {
  Result res;

  if (!toRemove.empty()) {
    TRI_ASSERT(
        trx.state()->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS));

    PhysicalCollection* physical = collection.getPhysical();

    auto indexesSnapshot = physical->getIndexesSnapshot();

    OperationOptions options;
    options.silent = true;
    options.ignoreRevs = true;
    options.isRestore = true;
    options.waitForSync = false;

    transaction::BuilderLeaser tempBuilder(&trx);
    auto callback = IndexIterator::makeDocumentCallback(*tempBuilder);

    double t = TRI_microtime();
    for (auto const& rid : toRemove) {
      auto documentId = LocalDocumentId::create(rid);

      tempBuilder->clear();
      res = physical->lookup(
          &trx, documentId, callback,
          {.fillCache = false, .readOwnWrites = true, .countBytes = false});

      if (res.ok()) {
        res = physical->remove(trx, indexesSnapshot, documentId, rid,
                               tempBuilder->slice(), options);
      }

      if (res.ok()) {
        ++stats.numDocsRemoved;
      } else if (res.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
        // ignore not found, we remove conflicting docs ahead of time
        res.reset();
      } else {
        // another error. this is severe and we abort.
        break;
      }
    }

    if (res.ok()) {
      auto fut =
          trx.state()->performIntermediateCommitIfRequired(collection.id());
      TRI_ASSERT(fut.isReady());
      res = fut.waitAndGet();
    }

    stats.waitedForRemovals += TRI_microtime() - t;

    toRemove.clear();
  }

  TRI_ASSERT(toRemove.empty());
  return res;
}

Result fetchRevisions(NetworkFeature& netFeature, transaction::Methods& trx,
                      DatabaseInitialSyncer::Configuration& config,
                      Syncer::SyncerState& state, LogicalCollection& collection,
                      std::string const& leader, bool encodeAsHLC,
                      std::vector<RevisionId>& toFetch,
                      ReplicationMetricsFeature::InitialSyncStats& stats) {
  if (toFetch.empty()) {
    return Result();  // nothing to do
  }

  OperationOptions options;
  options.silent = true;
  options.ignoreRevs = true;
  options.isRestore = true;
  options.validate = false;  // no validation during replication
  options.indexOperationMode = IndexOperationMode::internal;
  options.checkUniqueConstraintsInPreflight = true;
  options.waitForSync = false;  // no waitForSync during replication
  if (!state.leaderId.empty()) {
    options.isSynchronousReplicationFrom = state.leaderId;
  }

  PhysicalCollection* physical = collection.getPhysical();
  auto indexesSnapshot = physical->getIndexesSnapshot();

  std::string path = absl::StrCat(replutils::ReplicationUrl, "/",
                                  RestReplicationHandler::Revisions, "/",
                                  RestReplicationHandler::Documents);

  network::Headers headers;
  if (ServerState::instance()->isSingleServer() &&
      config.applier._jwt.empty()) {
    // if we are the single-server replication and there is no JWT
    // present, inject the username/password credentials into the
    // requests.
    // this is not state-of-the-art, but fixes a problem in single
    // server replication when the leader uses authentication with
    // username/password
    headers.emplace(StaticStrings::Authorization,
                    "Basic " + absl::Base64Escape(
                                   absl::StrCat(config.applier._username, ":",
                                                config.applier._password)));
  }

  config.progress.set(
      absl::StrCat("fetching documents by revision for collection '",
                   collection.name(), "' from ", path));

  transaction::BuilderLeaser tempBuilder(&trx);
  auto callback = IndexIterator::makeDocumentCallback(*tempBuilder);

  auto removeConflict = [&](auto const& conflictingKey) -> Result {
    std::pair<LocalDocumentId, RevisionId> lookupResult;
    auto r = physical->lookupKey(&trx, conflictingKey, lookupResult,
                                 ReadOwnWrites::yes);

    if (r.ok()) {
      TRI_ASSERT(lookupResult.first.isSet());
      TRI_ASSERT(lookupResult.second.isSet());
      auto [documentId, revisionId] = lookupResult;

      tempBuilder->clear();
      r = physical->lookup(
          &trx, documentId, callback,
          {.fillCache = false, .readOwnWrites = true, .countBytes = false});

      if (r.ok()) {
        TRI_ASSERT(tempBuilder->slice().isObject());
        r = physical->remove(trx, indexesSnapshot, documentId, revisionId,
                             tempBuilder->slice(), options);
      }
    }

    // if a conflict document cannot be removed because it doesn't exist,
    // we do not care, because the goal is deletion anyway. if it fails
    // for some other reason, the following re-insert will likely complain.
    // so intentionally no special error handling here.
    if (r.ok()) {
      ++stats.numDocsRemoved;
    }

    return r;
  };

  std::size_t numUniqueIndexes = [&]() {
    std::size_t numUnique = 0;
    for (auto const& idx : collection.getPhysical()->getReadyIndexes()) {
      numUnique += idx->unique() ? 1 : 0;
    }
    return numUnique;
  }();

  std::size_t current = 0;
  auto guard = scopeGuard(
      [&current, &stats]() noexcept { stats.numDocsRequested += current; });

  char ridBuffer[basics::maxUInt64StringSize];
  std::deque<network::FutureRes> futures;
  std::deque<std::unordered_set<RevisionId>> shoppingLists;

  network::ConnectionPool* pool = netFeature.pool();

  std::size_t queueSize = 10;
  if ((config.leader.majorVersion < 3) ||
      (config.leader.majorVersion == 3 && config.leader.minorVersion < 9) ||
      (config.leader.majorVersion == 3 && config.leader.minorVersion == 9 &&
       config.leader.patchVersion < 1)) {
    queueSize = 1;
  }

  while (current < toFetch.size() || !futures.empty()) {
    // Send some requests off if not enough in flight and something to go
    while (futures.size() < queueSize && current < toFetch.size()) {
      VPackBuilder requestBuilder;
      std::unordered_set<RevisionId> shoppingList;
      uint64_t count = 0;
      {
        VPackArrayBuilder list(&requestBuilder);
        std::size_t i;
        for (i = 0; i < 5000 && current + i < toFetch.size(); ++i) {
          if (encodeAsHLC) {
            requestBuilder.add(VPackValue(toFetch[current + i].toHLC()));
          } else {
            // deprecated, ambiguous format
            requestBuilder.add(toFetch[current + i].toValuePair(ridBuffer));
          }
          shoppingList.insert(toFetch[current + i]);
          ++count;
        }
        current += i;
      }

      network::RequestOptions reqOptions;
      reqOptions.param("collection", leader)
          .param("serverId", state.localServerIdString)
          .param("batchId", std::to_string(config.batch.id))
          .param("encodeAsHLC", encodeAsHLC ? "true" : "false");
      reqOptions.database = config.vocbase.name();
      reqOptions.timeout = network::Timeout(25.0);
      auto buffer = requestBuilder.steal();
      auto f = network::sendRequestRetry(
          pool, config.leader.endpoint, fuerte::RestVerb::Put, path,
          std::move(*buffer), reqOptions, headers);
      futures.emplace_back(std::move(f));
      shoppingLists.emplace_back(std::move(shoppingList));
      ++stats.numDocsRequests;
      LOG_TOPIC("eda42", DEBUG, Logger::REPLICATION)
          << "Have requested a chunk of " << count << " revision(s) from "
          << config.leader.serverId << " at " << config.leader.endpoint
          << " for collection " << leader
          << " length of queue: " << futures.size();
    }

    if (!futures.empty()) {
      TRI_ASSERT(futures.size() == shoppingLists.size());
      auto& f = futures.front();
      double tWait = TRI_microtime();
      auto& val = f.waitAndGet();
      stats.waitedForDocs += TRI_microtime() - tWait;
      Result res = val.combinedResult();
      if (res.fail()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      absl::StrCat("got invalid response from leader at ",
                                   config.leader.endpoint, path, ": ",
                                   res.errorMessage()));
      }

      VPackSlice docs = val.slice();
      if (!docs.isArray()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      absl::StrCat("got invalid response from leader at ",
                                   config.leader.endpoint, path,
                                   ": response is not an array"));
      }

      config.progress.set("applying documents by revision for collection '" +
                          collection.name() + "'");

      auto& sl = shoppingLists.front();  // corresponds to future
      for (VPackSlice leaderDoc : VPackArrayIterator(docs)) {
        if (!leaderDoc.isObject()) {
          return Result(
              TRI_ERROR_REPLICATION_INVALID_RESPONSE,
              absl::StrCat("got invalid response from leader at ",
                           config.leader.endpoint, path,
                           ": response document entry is not an object"));
        }

        VPackSlice keySlice = leaderDoc.get(StaticStrings::KeyString);
        if (!keySlice.isString()) {
          return Result(
              TRI_ERROR_REPLICATION_INVALID_RESPONSE,
              absl::StrCat("got invalid response from leader at ",
                           state.leader.endpoint, ": document key is invalid"));
        }

        VPackSlice revSlice = leaderDoc.get(StaticStrings::RevString);
        if (!revSlice.isString()) {
          return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                        absl::StrCat("got invalid response from leader at ",
                                     state.leader.endpoint,
                                     ": document revision is invalid"));
        }

        options.indexOperationMode = IndexOperationMode::internal;

        // we need a retry loop here for unique indexes (we will always have at
        // least one unique index, which is the primary index, but there can be
        // more). as documents can be presented in any state on the follower,
        // simply inserting them in leader order may trigger a unique constraint
        // violation on the follower. in this case we may need to remove the
        // conflicting document. this can happen multiple times if there are
        // multiple unique indexes! we can only stop trying once we have tried
        // often enough, or if inserting succeeds.
        std::size_t tries = 1 + numUniqueIndexes;
        while (tries-- > 0) {
          if (tries == 0) {
            options.indexOperationMode = IndexOperationMode::normal;
          }

          RevisionId rid = RevisionId::fromSlice(leaderDoc);

          double tInsert = TRI_microtime();
          Result res = trx.insert(collection.name(), leaderDoc, options).result;
          stats.waitedForInsertions += TRI_microtime() - tInsert;

          options.indexOperationMode = IndexOperationMode::internal;

          // We must see our own writes, because we may have to remove
          if (res.ok()) {
            sl.erase(rid);
            ++stats.numDocsInserted;
            break;
          }

          if (!res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)) {
            return res;
          }

          // conflicting documents (that we just inserted), as documents may be
          // replicated in unexpected order.
          if (physical
                  ->lookup(&trx, LocalDocumentId{rid.id()},
                           [](LocalDocumentId, aql::DocumentData&&,
                              VPackSlice) { return true; },
                           {.readOwnWrites = true, .countBytes = false})
                  .ok()) {
            // already have exactly this revision. no need to insert
            sl.erase(rid);
            break;
          }

          // remove conflict and retry
          // errorMessage() is this case contains the conflicting key
          auto inner = removeConflict(res.errorMessage());
          if (inner.fail()) {
            return res;
          }
        }
      }
      // Here we expect that a lot if not all of our shopping list has come
      // back from the leader and the shopping list is empty, however, it
      // is possible that this is not the case, since the leader reserves
      // the right to send fewer documents. In this case, we have to order
      // another batch:
      if (!sl.empty()) {
        // Take out the nonempty list:
        std::unordered_set<RevisionId> newList = std::move(sl);
        // Sort in ascending order to speed up iterator lookup on leader:
        std::vector<RevisionId> v;
        v.reserve(newList.size());
        for (auto it = newList.begin(); it != newList.end(); ++it) {
          v.push_back(*it);
        }
        std::sort(v.begin(), v.end());
        VPackBuilder requestBuilder;
        {
          VPackArrayBuilder list(&requestBuilder);
          for (auto const& r : v) {
            if (encodeAsHLC) {
              requestBuilder.add(VPackValue(r.toHLC()));
            } else {
              // deprecated, ambiguous format
              requestBuilder.add(r.toValuePair(ridBuffer));
            }
          }
        }

        network::RequestOptions reqOptions;
        reqOptions.param("collection", leader)
            .param("serverId", state.localServerIdString)
            .param("batchId", std::to_string(config.batch.id))
            .param("encodeAsHLC", encodeAsHLC ? "true" : "false");
        reqOptions.timeout = network::Timeout(25.0);
        reqOptions.database = config.vocbase.name();
        auto buffer = requestBuilder.steal();
        auto f = network::sendRequestRetry(
            pool, config.leader.endpoint, fuerte::RestVerb::Put, path,
            std::move(*buffer), reqOptions, headers);
        futures.emplace_back(std::move(f));
        shoppingLists.emplace_back(std::move(newList));
        ++stats.numDocsRequests;
        LOG_TOPIC("eda45", DEBUG, Logger::REPLICATION)
            << "Have re-requested a chunk of " << shoppingLists.back().size()
            << " revision(s) from " << config.leader.serverId << " at "
            << config.leader.endpoint << " for collection " << leader
            << " queue length: " << futures.size();
      }
      LOG_TOPIC("eda44", DEBUG, Logger::REPLICATION)
          << "Have applied a chunk of " << docs.length() << " documents from "
          << config.leader.serverId << " at " << config.leader.endpoint
          << " for collection " << leader
          << " queue length: " << futures.size();
      futures.pop_front();
      shoppingLists.pop_front();
      TRI_ASSERT(futures.size() == shoppingLists.size());

      TRI_ASSERT(
          trx.state()->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS));

      auto fut =
          trx.state()->performIntermediateCommitIfRequired(collection.id());
      TRI_ASSERT(fut.isReady());
      res = fut.waitAndGet();

      if (res.fail()) {
        return res;
      }
    }
  }

  toFetch.clear();
  return {};
}

}  // namespace

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
    ReplicationApplierConfiguration const& a, replutils::BatchInfo& bat,
    replutils::Connection& c, bool f, replutils::LeaderInfo& l,
    replutils::ProgressInfo& p, SyncerState& s, TRI_vocbase_t& v)
    : applier{a},
      batch{bat},
      connection{c},
      leader{l},
      progress{p},
      state{s},
      vocbase{v} {}

bool DatabaseInitialSyncer::Configuration::isChild() const noexcept {
  return state.isChildSyncer;
}

DatabaseInitialSyncer::DatabaseInitialSyncer(
    TRI_vocbase_t& vocbase,
    ReplicationApplierConfiguration const& configuration)
    : InitialSyncer(
          configuration,
          [this](std::string const& msg) -> void { setProgress(msg); }),
      _config{_state.applier, _batch,        _state.connection,
              false,          _state.leader, _progress,
              _state,         vocbase},
      _lastCancellationCheck(std::chrono::steady_clock::now()),
      _isClusterRole(ServerState::instance()->isClusterRole()),
      _quickKeysNumDocsLimit(
          vocbase.server().getFeature<ReplicationFeature>().quickKeysLimit()) {
  _state.vocbases.try_emplace(vocbase.name(), vocbase);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  adjustQuickKeysNumDocsLimit();
#endif
  if (configuration._database.empty()) {
    _state.databaseName = vocbase.name();
  }
}

std::shared_ptr<DatabaseInitialSyncer> DatabaseInitialSyncer::create(
    TRI_vocbase_t& vocbase,
    ReplicationApplierConfiguration const& configuration) {
  // enable make_shared on a class with a private constructor
  struct Enabler final : public DatabaseInitialSyncer {
    Enabler(TRI_vocbase_t& vocbase,
            ReplicationApplierConfiguration const& configuration)
        : DatabaseInitialSyncer(vocbase, configuration) {}
  };

  return std::make_shared<Enabler>(vocbase, configuration);
}

/// @brief return information about the leader
replutils::LeaderInfo DatabaseInitialSyncer::leaderInfo() const {
  return _config.leader;
}

/// @brief run method, performs a full synchronization
Result DatabaseInitialSyncer::runWithInventory(bool incremental,
                                               VPackSlice dbInventory,
                                               char const* context) {
  if (!_config.connection.valid()) {
    return Result(TRI_ERROR_INTERNAL, "invalid endpoint");
  }

  double const startTime = TRI_microtime();

  try {
    bool const preventMultiStart = !_isClusterRole;
    MultiStartPreventer p(vocbase(), preventMultiStart);

    setAborted(false);

    _config.progress.set("fetching leader state");

    LOG_TOPIC("0a10d", DEBUG, Logger::REPLICATION)
        << "client: getting leader state to dump " << vocbase().name();

    Result r;
    if (!_config.isChild()) {
      // enable patching of collection count for ShardSynchronization Job
      std::string patchCount;
      if (_config.applier._skipCreateDrop &&
          _config.applier._restrictType ==
              ReplicationApplierConfiguration::RestrictType::Include &&
          _config.applier._restrictCollections.size() == 1) {
        patchCount = *_config.applier._restrictCollections.begin();
      }

      // with a 3.8 leader, this call combines fetching the leader state with
      // starting the batch. this saves us one request per shard. a 3.7 leader
      // will does not return the leader state together with this call, so we
      // need to be prepared for not yet getting it here
      r = batchStart(context, patchCount);

      if (r.ok() && !_config.leader.serverId.isSet()) {
        // a 3.7 leader, which does not return leader state when stating a
        // batch. so we need to fetch the leader state in addition
        r = _config.leader.getState(_config.connection, _config.isChild(),
                                    context);
      }

      if (r.fail()) {
        return r;
      }

      TRI_ASSERT(!_config.leader.endpoint.empty());
      TRI_ASSERT(_config.leader.serverId.isSet());
      TRI_ASSERT(_config.leader.majorVersion != 0);

      LOG_TOPIC("6fd2b", DEBUG, Logger::REPLICATION)
          << "client: got leader state";

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
      r = fetchInventory(inventoryResponse);
      if (!r.ok()) {
        return r;
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

    if (r.fail()) {
      LOG_TOPIC("12556", DEBUG, Logger::REPLICATION)
          << "Error during initial sync: " << r.errorMessage();
    }

    LOG_TOPIC("055df", DEBUG, Logger::REPLICATION)
        << "initial synchronization with leader took: "
        << Logger::FIXED(TRI_microtime() - startTime, 6) << " s. status: "
        << (r.errorMessage().empty() ? "all good" : r.errorMessage());

    if (r.ok() && _onSuccess) {
      r = _onSuccess(*this);
    }

    return r;
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "an unknown exception occurred");
  }
}

/// @brief fetch the server's inventory, public method for TailingSyncer
Result DatabaseInitialSyncer::getInventory(VPackBuilder& builder) {
  if (!_state.connection.valid()) {
    return Result(TRI_ERROR_INTERNAL, "invalid endpoint");
  }

  auto r = batchStart(nullptr);
  if (r.fail()) {
    return r;
  }

  auto sg = scopeGuard([&]() noexcept { batchFinish(); });

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

  if (_state.isChildSyncer) {
    // this syncer is used as a child syncer of the GlobalInitialSyncer.
    // now check if parent was aborted
    ReplicationFeature& replication =
        vocbase().server().getFeature<ReplicationFeature>();
    GlobalReplicationApplier* applier = replication.globalReplicationApplier();
    if (applier != nullptr && applier->stopInitialSynchronization()) {
      return true;
    }
  }

  if (_checkCancellation) {
    // execute custom check for abortion only every few seconds, in case
    // it is expensive
    constexpr auto checkFrequency = std::chrono::seconds(5);

    auto now = std::chrono::steady_clock::now();
    TRI_IF_FAILURE("Replication::forceCheckCancellation") {
      // always force the cancellation check!
      _lastCancellationCheck = now - checkFrequency;
    }

    if (now - _lastCancellationCheck >= checkFrequency) {
      _lastCancellationCheck = now;
      if (_checkCancellation()) {
        return true;
      }
    }
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

/// @brief handle a single dump marker
Result DatabaseInitialSyncer::parseCollectionDumpMarker(
    transaction::Methods& trx, LogicalCollection* coll, VPackSlice marker,
    FormatHint& hint) {
  if (!ADB_LIKELY(marker.isObject())) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  // format auto-detection. this is executed only once per batch
  if (hint == FormatHint::AutoDetect) {
    if (marker.get(StaticStrings::KeyString).isString()) {
      // _key present
      hint = FormatHint::NoEnvelope;
    } else if (marker.get(kDataString).isObject()) {
      hint = FormatHint::Envelope;
    } else {
      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }
  }

  TRI_ASSERT(hint == FormatHint::Envelope || hint == FormatHint::NoEnvelope);

  VPackSlice doc;
  TRI_replication_operation_e type = REPLICATION_INVALID;

  if (hint == FormatHint::Envelope) {
    // input is wrapped in a {"type":2300,"data":{...}} envelope
    VPackSlice s = marker.get(kTypeString);
    if (s.isNumber()) {
      type = static_cast<TRI_replication_operation_e>(s.getNumber<int>());
    }
    doc = marker.get(kDataString);
    if (!doc.isObject()) {
      return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
    }
  } else if (hint == FormatHint::NoEnvelope) {
    // input is just a document, without any {"type":2300,"data":{...}} envelope
    type = REPLICATION_MARKER_DOCUMENT;
    doc = marker;
  }

  if (!ADB_LIKELY(doc.isObject())) {
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }

  std::string conflictingDocumentKey;
  return applyCollectionDumpMarker(trx, coll, type, doc,
                                   conflictingDocumentKey);
}

/// @brief apply the data from a collection dump
Result DatabaseInitialSyncer::parseCollectionDump(
    transaction::Methods& trx, LogicalCollection* coll,
    httpclient::SimpleHttpResult* response, uint64_t& markersProcessed) {
  TRI_ASSERT(response != nullptr);
  TRI_ASSERT(!trx.isSingleOperationTransaction());

  FormatHint hint = FormatHint::AutoDetect;

  basics::StringBuffer const& data = response->getBody();
  char const* p = data.begin();
  char const* end = p + data.length();

  if (isVelocyPack(*response)) {
    // received a velocypack response from the leader

    // intentional copy
    VPackOptions validationOptions =
        basics::VelocyPackHelper::strictRequestValidationOptions;

    // allow custom types being sent here
    validationOptions.disallowCustom = false;

    VPackValidator validator(&validationOptions);

    // now check the sub-format of the velocypack data we received...
    VPackSlice s(reinterpret_cast<uint8_t const*>(p));

    if (s.isArray()) {
      // got one big velocypack array with all documents in it.
      // servers >= 3.10 will send this if we send the "array=true" request
      // parameter. older versions are not able to send this format, but will
      // send each document as a single velocypack slice.

      size_t remaining = static_cast<size_t>(end - p);
      // throws if the data is invalid
      validator.validate(p, remaining, /*isSubPart*/ false);

      markersProcessed += s.length();

      OperationOptions options;
      options.silent = true;
      options.ignoreRevs = true;
      options.isRestore = true;
      options.validate = false;
      options.returnOld = false;
      options.returnNew = false;
      options.checkUniqueConstraintsInPreflight = false;
      options.isSynchronousReplicationFrom = _state.leaderId;

      auto opRes = trx.insert(coll->name(), s, options);
      if (opRes.fail()) {
        return opRes.result;
      }

      VPackSlice resultSlice = opRes.slice();
      if (resultSlice.isArray()) {
        for (VPackSlice it : VPackArrayIterator(resultSlice)) {
          VPackSlice s = it.get(StaticStrings::Error);
          if (!s.isTrue()) {
            continue;
          }
          // found an error
          auto errorCode =
              ErrorCode{it.get(StaticStrings::ErrorNum).getNumber<int>()};
          VPackSlice msg = it.get(StaticStrings::ErrorMessage);
          return Result(errorCode, msg.copyString());
        }
      }

    } else {
      // received a VelocyPack response from the leader, with one document
      // per slice (multiple slices in the same response)
      try {
        while (p < end) {
          size_t remaining = static_cast<size_t>(end - p);
          // throws if the data is invalid
          validator.validate(p, remaining, /*isSubPart*/ true);

          VPackSlice marker(reinterpret_cast<uint8_t const*>(p));
          Result r = parseCollectionDumpMarker(trx, coll, marker, hint);

          TRI_ASSERT(!r.is(TRI_ERROR_ARANGO_TRY_AGAIN));
          if (r.fail()) {
            r.reset(r.errorNumber(),
                    absl::StrCat("received invalid dump data for collection '",
                                 coll->name(), "'"));
            return r;
          }
          ++markersProcessed;
          p += marker.byteSize();
        }
      } catch (velocypack::Exception const& e) {
        LOG_TOPIC("b9f4f", ERR, Logger::REPLICATION)
            << "Error parsing VPack response: " << e.what();
        return Result(TRI_ERROR_HTTP_CORRUPTED_JSON, e.what());
      }
    }
  } else {
    // received a JSONL response from the leader, with one document per line
    // buffer must end with a NUL byte
    TRI_ASSERT(*end == '\0');
    LOG_TOPIC("bad5d", DEBUG, Logger::REPLICATION)
        << "using json for chunk contents";

    VPackBuilder builder;
    VPackParser parser(
        builder, &basics::VelocyPackHelper::strictRequestValidationOptions);

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
        return Result(TRI_ERROR_HTTP_CORRUPTED_JSON, e.what());
      }

      p = q + 1;

      Result r = parseCollectionDumpMarker(trx, coll, builder.slice(), hint);
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
void DatabaseInitialSyncer::fetchDumpChunk(
    std::shared_ptr<Syncer::JobSynchronizer> sharedStatus,
    std::string_view baseUrl, LogicalCollection* coll,
    std::string_view leaderColl, int batch, TRI_voc_tick_t fromTick,
    uint64_t chunkSize) {
  if (isAborted()) {
    sharedStatus->gotResponse(Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED));
    return;
  }

  try {
    std::string const typeString =
        (coll->type() == TRI_COL_TYPE_EDGE ? "edge" : "document");

    if (!_config.isChild()) {
      batchExtend();
    }

    // assemble URL to call
    std::string url =
        absl::StrCat(baseUrl, "&from=", fromTick, "&chunkSize=", chunkSize);

    bool isVPack = false;
    auto headers = replutils::createHeaders();
    if (_config.leader.version() >= 30800) {
      // from 3.8 onwards, it is safe and also faster to retrieve vpack-encoded
      // dumps. in previous versions there may be vpack encoding issues for the
      // /_api/replication/dump responses.
      headers[StaticStrings::Accept] = StaticStrings::MimeTypeVPack;
      isVPack = true;
    }

    _config.progress.set(absl::StrCat(
        "fetching leader collection dump for collection '", coll->name(),
        "', type: ", typeString, ", format: ", (isVPack ? "vpack" : "json"),
        ", id: ", leaderColl, ", batch ", batch, ", url: ", url));

    double t = TRI_microtime();

    // send request
    std::unique_ptr<httpclient::SimpleHttpResult> response;
    _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
      response.reset(client->retryRequest(rest::RequestType::GET, url, nullptr,
                                          0, headers));
    });

    t = TRI_microtime() - t;

    if (replutils::hasFailed(response.get())) {
      sharedStatus->gotResponse(
          replutils::buildHttpError(response.get(), url, _config.connection),
          t);
      return;
    }

    // success!
    sharedStatus->gotResponse(std::move(response), t);
  } catch (basics::Exception const& ex) {
    sharedStatus->gotResponse(Result(ex.code(), ex.what()));
  } catch (std::exception const& ex) {
    sharedStatus->gotResponse(Result(TRI_ERROR_INTERNAL, ex.what()));
  }
}

/// @brief incrementally fetch data from a collection
Result DatabaseInitialSyncer::fetchCollectionDump(LogicalCollection* coll,
                                                  std::string const& leaderColl,
                                                  TRI_voc_tick_t maxTick) {
  using basics::StringUtils::boolean;
  using basics::StringUtils::itoa;
  using basics::StringUtils::uint64;
  using basics::StringUtils::urlEncode;

  if (isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  std::string const typeString =
      (coll->type() == TRI_COL_TYPE_EDGE ? "edge" : "document");

  // statistics which will update the global replication metrics,
  // periodically reset
  ReplicationMetricsFeature::InitialSyncStats stats(
      vocbase().server().getFeature<ReplicationMetricsFeature>(), true);

  // local statistics that will be ever increasing inside this method
  ReplicationMetricsFeature::InitialSyncStats cumulativeStats(
      vocbase().server().getFeature<ReplicationMetricsFeature>(), false);

  TRI_ASSERT(_config.batch.id);  // should not be equal to 0

  // assemble base URL
  // note: the "useEnvelope" URL parameter is sent to signal the leader that
  // we don't need the dump data packaged in an extra {"type":2300,"data":{...}}
  // envelope per document.
  // older, incompatible leaders will simply ignore this parameter and will
  // still send the documents inside these envelopes. that means when we receive
  // the documents, we need to disambiguate the two different formats.
  std::string baseUrl = absl::StrCat(
      replutils::ReplicationUrl, "/dump?collection=", urlEncode(leaderColl),
      "&batchId=", _config.batch.id,
      "&includeSystem=", (_config.applier._includeSystem ? "true" : "false"),
      "&useEnvelope=false&serverId=", _state.localServerIdString);

  if (ServerState::instance()->isDBServer() && !_config.isChild() &&
      _config.applier._skipCreateDrop &&
      _config.applier._restrictType ==
          ReplicationApplierConfiguration::RestrictType::Include &&
      _config.applier._restrictCollections.size() == 1 &&
      !hasDocuments(*coll)) {
    // DB server doing shard synchronization. now try to fetch everything in a
    // single VPack array. note: only servers >= 3.10 will honor this URL
    // parameter. servers that are not capable of this format will simply ignore
    // it and send the old format. the syncer has code to tell the two formats
    // apart. note: we can only add this flag if we are sure there are no
    // documents present locally. everything else is not safe.
    baseUrl += "&array=true";
  }

  if (maxTick > 0) {
    baseUrl += "&to=" + itoa(maxTick + 1);
  }

  // state variables for the dump
  TRI_voc_tick_t fromTick = 0;
  int batch = 1;
  uint64_t chunkSize = _config.applier._chunkSize;

  double const startTime = TRI_microtime();

  // the shared status will wait in its destructor until all posted
  // requests have been completed/canceled!
  auto self = shared_from_this();
  Syncer::JobSynchronizerScope sharedStatus(self);

  // order initial chunk. this will block until the initial response
  // has arrived
  fetchDumpChunk(sharedStatus.clone(), baseUrl, coll, leaderColl, batch,
                 fromTick, chunkSize);

  while (true) {
    std::unique_ptr<httpclient::SimpleHttpResult> dumpResponse;

    // block until we either got a response or were shut down
    Result res = sharedStatus->waitForResponse(dumpResponse);

    // update our statistics
    ++stats.numDumpRequests;
    stats.waitedForDump += sharedStatus->time();

    if (res.fail()) {
      // no response or error or shutdown
      return res;
    }

    // now we have got a response!
    TRI_ASSERT(dumpResponse != nullptr);

    if (dumpResponse->hasContentLength()) {
      stats.numDumpBytesReceived += dumpResponse->getContentLength();
    }

    bool found;
    std::string header = dumpResponse->getHeaderField(
        StaticStrings::ReplicationHeaderCheckMore, found);
    if (!found) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    absl::StrCat("got invalid response from leader at ",
                                 _config.leader.endpoint, ": required header ",
                                 StaticStrings::ReplicationHeaderCheckMore,
                                 " is missing in dump response"));
    }

    TRI_voc_tick_t tick;
    bool checkMore = boolean(header);

    if (checkMore) {
      header = dumpResponse->getHeaderField(
          StaticStrings::ReplicationHeaderLastIncluded, found);
      if (!found) {
        return Result(
            TRI_ERROR_REPLICATION_INVALID_RESPONSE,
            absl::StrCat("got invalid response from leader at ",
                         _config.leader.endpoint + ": required header ",
                         StaticStrings::ReplicationHeaderLastIncluded,
                         " is missing in dump response"));
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
    if (chunkSize < maxChunkSize) {
      chunkSize = static_cast<uint64_t>(chunkSize * 1.25);

      if (chunkSize > maxChunkSize) {
        chunkSize = maxChunkSize;
      }
    }

    if (checkMore && !isAborted()) {
      // already fetch next batch in the background, by posting the
      // request to the scheduler, which can run it asynchronously
      sharedStatus->request([self, baseUrl, sharedStatus = sharedStatus.clone(),
                             coll, leaderColl, batch, fromTick, chunkSize]() {
        TRI_IF_FAILURE("Replication::forceCheckCancellation") {
          // we intentionally sleep here for a while, so the next call gets
          // executed after the scheduling thread has thrown its
          // TRI_ERROR_INTERNAL exception for our failure point
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::static_pointer_cast<DatabaseInitialSyncer>(self)->fetchDumpChunk(
            sharedStatus, baseUrl, coll, leaderColl, batch + 1, fromTick,
            chunkSize);
      });
      TRI_IF_FAILURE("Replication::forceCheckCancellation") {
        // forcefully abort replication once we have scheduled the job
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
      }
    }

    auto operationOrigin = transaction::OperationOriginInternal{
        "applying initial changes in replication"};

    SingleCollectionTransaction trx(
        transaction::StandaloneContext::create(vocbase(), operationOrigin),
        *coll, AccessMode::Type::EXCLUSIVE);

    // do not index the operations in our own transaction
    trx.addHint(transaction::Hints::Hint::NO_INDEXING);

    res = trx.begin();

    if (!res.ok()) {
      return Result(
          res.errorNumber(),
          absl::StrCat("unable to start transaction: ", res.errorMessage()));
    }

    double t = TRI_microtime();
    TRI_ASSERT(!trx.isSingleOperationTransaction());
    res = parseCollectionDump(trx, coll, dumpResponse.get(),
                              stats.numDumpDocuments);

    if (res.fail()) {
      TRI_ASSERT(!res.is(TRI_ERROR_ARANGO_TRY_AGAIN));
      return res;
    }

    res = trx.commit();

    double applyTime = TRI_microtime() - t;
    stats.waitedForDumpApply += applyTime;

    cumulativeStats += stats;

    _config.progress.set(absl::StrCat(
        "fetched leader collection dump for collection '", coll->name(),
        "', type: ", typeString, ", id: ", leaderColl, ", batch ", batch,
        ", markers processed so far: ", cumulativeStats.numDumpDocuments,
        ", bytes received so far: ", cumulativeStats.numDumpBytesReceived,
        ", apply time for batch: ", applyTime, " s"));

    if (!res.ok()) {
      return res;
    }

    if (!checkMore || fromTick == 0) {
      // done
      _config.progress.set(absl::StrCat(
          "finished initial dump for collection '", coll->name(),
          "', type: ", typeString, ", id: ", leaderColl,
          ", markers processed: ", cumulativeStats.numDumpDocuments,
          ", bytes received: ", cumulativeStats.numDumpBytesReceived,
          ", dump requests: ", cumulativeStats.numDumpRequests,
          ", waited for dump: ", cumulativeStats.waitedForDump, " s",
          ", apply time: ", cumulativeStats.waitedForDumpApply, " s",
          ", total time: ", (TRI_microtime() - startTime), " s"));
      return Result();
    }

    batch++;

    if (isAborted()) {
      return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }

    // update the global metrics so the changes become visible early
    stats.publish();
    // keep the cumulativeStats intact here!
  }

  TRI_ASSERT(false);
  return Result(TRI_ERROR_INTERNAL);
}

/// @brief incrementally fetch data from a collection
Result DatabaseInitialSyncer::fetchCollectionSync(LogicalCollection* coll,
                                                  std::string const& leaderColl,
                                                  TRI_voc_tick_t maxTick) {
  if (coll->syncByRevision() && _config.leader.version() >= 30800) {
    // local collection should support revisions, and leader is at least aware
    // of the revision-based protocol, so we can query it to find out if we
    // can use the new protocol; will fall back to old one if leader collection
    // is an old variant
    return fetchCollectionSyncByRevisions(coll, leaderColl, maxTick);
  }
  return fetchCollectionSyncByKeys(coll, leaderColl, maxTick);
}

/// @brief incrementally fetch data from a collection using keys as the primary
/// document identifier
Result DatabaseInitialSyncer::fetchCollectionSyncByKeys(
    LogicalCollection* coll, std::string const& leaderColl,
    TRI_voc_tick_t maxTick) {
  using basics::StringUtils::urlEncode;

  if (!_config.isChild()) {
    batchExtend();
  }

  ReplicationMetricsFeature::InitialSyncStats stats(
      coll->vocbase().server().getFeature<ReplicationMetricsFeature>(), true);

  // We'll do one quick attempt at getting the keys on the leader.
  // We might receive the keys or a count of the keys. In the first case we
  // continue with the sync. If we get a document count, we'll estimate a
  // pessimistic wait of roughly 1e9/day and repeat the call without a quick
  // option.
  std::string const baseUrl = replutils::ReplicationUrl + "/keys";
  std::string url = absl::StrCat(
      baseUrl, "?collection=", urlEncode(leaderColl), "&to=", maxTick,
      "&serverId=", _state.localServerIdString, "&batchId=", _config.batch.id);

  std::string msg = "fetching collection keys for collection '" + coll->name() +
                    "' from " + url;
  _config.progress.set(msg);

  // use a lower bound for maxWaitTime of 180M microseconds, i.e. 180 seconds
  constexpr uint64_t lowerBoundForWaitTime = 180000000;

  // note: maxWaitTime has a unit of microseconds
  uint64_t maxWaitTime = _config.applier._initialSyncMaxWaitTime;
  maxWaitTime = std::max<uint64_t>(maxWaitTime, lowerBoundForWaitTime);

  // the following two variables can be modified by the "keysCall" lambda
  VPackBuilder builder;
  VPackSlice slice;

  auto keysCall = [&](bool quick) {
    // send an initial async request to collect the collection keys on the other
    // side
    // sending this request in a blocking fashion may require very long to
    // complete,
    // so we're sending the x-arango-async header here
    auto headers = replutils::createHeaders();
    headers[StaticStrings::Async] = "store";

    std::unique_ptr<httpclient::SimpleHttpResult> response;
    _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
      response.reset(client->retryRequest(rest::RequestType::POST,
                                          (quick) ? url + "&quick=true" : url,
                                          nullptr, 0, headers));
    });
    ++stats.numKeysRequests;

    if (replutils::hasFailed(response.get())) {
      ++stats.numFailedConnects;
      return replutils::buildHttpError(response.get(), url, _config.connection);
    }

    bool found = false;
    std::string jobId = response->getHeaderField(StaticStrings::AsyncId, found);

    if (!found) {
      ++stats.numFailedConnects;
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    absl::StrCat("got invalid response from leader at ",
                                 _config.leader.endpoint, url,
                                 ": could not find 'X-Arango-Async' header"));
    }

    double const startTime = TRI_microtime();

    headers = replutils::createHeaders();

    while (true) {
      if (!_config.isChild()) {
        batchExtend();
      }

      std::string const jobUrl = "/_api/job/" + jobId;
      _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
        response.reset(client->request(rest::RequestType::PUT, jobUrl, nullptr,
                                       0, headers));
      });

      double waitTime = TRI_microtime() - startTime;

      if (response != nullptr && response->isComplete()) {
        if (response->hasContentLength()) {
          stats.numSyncBytesReceived += response->getContentLength();
        }
        if (response->hasHeaderField(StaticStrings::AsyncId)) {
          // job is done, got the actual response
          break;
        }
        if (response->getHttpReturnCode() == 404) {
          // unknown job, we can abort
          ++stats.numFailedConnects;
          stats.waitedForInitial += waitTime;
          return Result(TRI_ERROR_REPLICATION_NO_RESPONSE,
                        absl::StrCat("job not found on leader at ",
                                     _config.leader.endpoint));
        }
      }

      TRI_ASSERT(maxWaitTime >= lowerBoundForWaitTime);

      if (static_cast<uint64_t>(waitTime * 1000.0 * 1000.0) >= maxWaitTime) {
        ++stats.numFailedConnects;
        stats.waitedForInitial += waitTime;
        return Result(
            TRI_ERROR_REPLICATION_NO_RESPONSE,
            absl::StrCat("timed out waiting for response from leader at ",
                         _config.leader.endpoint));
      }

      if (isAborted()) {
        stats.waitedForInitial += waitTime;
        return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
      }

      std::chrono::milliseconds sleepTime = sleepTimeFromWaitTime(waitTime);
      std::this_thread::sleep_for(sleepTime);
    }

    stats.waitedForInitial += TRI_microtime() - startTime;

    if (replutils::hasFailed(response.get())) {
      ++stats.numFailedConnects;
      return replutils::buildHttpError(response.get(), url, _config.connection);
    }

    Result r = replutils::parseResponse(builder, response.get());

    if (r.fail()) {
      ++stats.numFailedConnects;
      return Result(
          TRI_ERROR_REPLICATION_INVALID_RESPONSE,
          absl::StrCat("got invalid response from leader at ",
                       _config.leader.endpoint, url, ": ", r.errorMessage()));
    }

    slice = builder.slice();
    if (!slice.isObject()) {
      ++stats.numFailedConnects;
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    absl::StrCat("got invalid response from leader at ",
                                 _config.leader.endpoint, url,
                                 ": response is no object"));
    }

    return Result();
  };

  auto ck = keysCall(true);
  if (!ck.ok()) {
    return ck;
  }
  VPackSlice const c = slice.get("count");
  if (!c.isNumber()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  absl::StrCat("got invalid response from master at ",
                               _config.leader.endpoint, url,
                               ": response count not a number"));
  }

  uint64_t ndocs = c.getNumber<uint64_t>();

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  if (ndocs > _quickKeysNumDocsLimit && slice.hasKey("id")) {
    LOG_TOPIC("6e1b3", ERR, Logger::REPLICATION)
        << "client: DatabaseInitialSyncer::run - expected only document count "
           "for quick call";
    TRI_ASSERT(false);
  }
#endif

  if (!slice.hasKey("id")) {  // we only have count
    // calculate a wait time proportional to the number of documents on the
    // leader if we get 1M documents back, the wait time is 80M microseconds,
    // i.e. 80 seconds if we get 10M documents back, the wait time is 800M
    // microseconds, i.e. 800 seconds
    // ...
    maxWaitTime = std::max<uint64_t>(maxWaitTime, ndocs * 80);

    // there is an additional lower bound for the wait time as defined initially
    // in
    //    _config.applier._initialSyncMaxWaitTime
    // we also apply an additional lower bound of 180 seconds here, in case that
    // value is configured too low, for whatever reason
    maxWaitTime = std::max<uint64_t>(maxWaitTime, lowerBoundForWaitTime);

    TRI_ASSERT(maxWaitTime >= lowerBoundForWaitTime);

    // note: keysCall() can modify the "slice" variable
    ck = keysCall(false);
    if (!ck.ok()) {
      return ck;
    }
  }

  VPackSlice const keysId = slice.get("id");
  if (!keysId.isString()) {
    ++stats.numFailedConnects;
    return Result(
        TRI_ERROR_REPLICATION_INVALID_RESPONSE,
        absl::StrCat("got invalid response from leader at ",
                     _config.leader.endpoint, url,
                     ": response does not contain valid 'id' attribute"));
  }

  auto sg = scopeGuard([&]() noexcept {
    try {
      url = absl::StrCat(baseUrl, "/", keysId.stringView());
      _config.progress.set(absl::StrCat(
          "deleting remote collection keys object for collection '",
          coll->name(), "' from ", url));

      // now delete the keys we ordered
      std::unique_ptr<httpclient::SimpleHttpResult> response;
      _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
        auto headers = replutils::createHeaders();
        response.reset(client->retryRequest(rest::RequestType::DELETE_REQ, url,
                                            nullptr, 0, headers));
      });
    } catch (std::exception const& ex) {
      LOG_TOPIC("f8b31", ERR, Logger::REPLICATION)
          << "Failed to deleting remote collection keys object for collection "
          << coll->name() << ex.what();
    }
  });

  VPackSlice const count = slice.get("count");

  if (!count.isNumber()) {
    ++stats.numFailedConnects;
    return Result(
        TRI_ERROR_REPLICATION_INVALID_RESPONSE,
        absl::StrCat("got invalid response from leader at ",
                     _config.leader.endpoint, url,
                     ": response does not contain valid 'count' attribute"));
  }

  if (count.getNumber<size_t>() <= 0) {
    // remote collection has no documents. now truncate our local collection
    auto operationOrigin = transaction::OperationOriginInternal{
        "truncating collection in replication"};
    SingleCollectionTransaction trx(
        transaction::StandaloneContext::create(vocbase(), operationOrigin),
        *coll, AccessMode::Type::EXCLUSIVE);
    trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
    trx.addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
    Result res = trx.begin();

    if (!res.ok()) {
      return Result(
          res.errorNumber(),
          absl::StrCat("unable to start transaction: ", res.errorMessage()));
    }

    OperationOptions options;

    if (!_state.leaderId.empty()) {
      options.isSynchronousReplicationFrom = _state.leaderId;
    }

    OperationResult opRes = trx.truncate(coll->name(), options);

    if (opRes.fail()) {
      return Result(opRes.errorNumber(),
                    absl::StrCat("unable to truncate collection '",
                                 coll->name(), "': ", res.errorMessage()));
    }

    return trx.finish(opRes.result);
  }

  // now we can fetch the complete chunk information from the leader
  try {
    return coll->vocbase().engine().handleSyncKeys(*this, *coll,
                                                   keysId.copyString());
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }
}

/// @brief order a new chunk from the /revisions API
void DatabaseInitialSyncer::fetchRevisionsChunk(
    std::shared_ptr<Syncer::JobSynchronizer> sharedStatus,
    std::string const& baseUrl, LogicalCollection* coll,
    std::string const& leaderColl, std::string const& requestPayload,
    RevisionId requestResume) {
  if (isAborted()) {
    sharedStatus->gotResponse(Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED));
    return;
  }

  try {
    std::string const typeString =
        (coll->type() == TRI_COL_TYPE_EDGE ? "edge" : "document");

    if (!_config.isChild()) {
      batchExtend();
    }

    using basics::StringUtils::urlEncode;

    // assemble URL to call.
    // note that the URL contains both the "resume" and "resumeHLC"
    // parameters. "resume" contains the stringified revision id value for
    // where to resume from. that stringification can be ambiguous and is thus
    // deprecated. we also send a "resumeHLC" parameter now, which always
    // contains a base64-encoded logical clock value. this is the preferred
    // way to encode the resume value, and once all leaders support it, we can
    // remove the "resume" parameter from the protocol
    bool appendResumeHLC = true;
    std::string url = baseUrl + "&" + StaticStrings::RevisionTreeResume + "=" +
                      urlEncode(requestResume.toString());

    TRI_IF_FAILURE("SyncerNoEncodeAsHLC") { appendResumeHLC = false; }

    if (appendResumeHLC) {
      url += "&" + StaticStrings::RevisionTreeResumeHLC + "=" +
             urlEncode(requestResume.toHLC()) + "&encodeAsHLC=true";
    }

    bool isVPack = false;
    auto headers = replutils::createHeaders();
    if (_config.leader.version() >= 31000) {
      headers[StaticStrings::Accept] = StaticStrings::MimeTypeVPack;
      isVPack = true;
    }

    _config.progress.set(absl::StrCat(
        "fetching leader collection revision ranges for collection '",
        coll->name(), "', type: ", typeString, ", format: ",
        (isVPack ? "vpack" : "json"), ", id: ", leaderColl, ", url: ", url));

    double t = TRI_microtime();

    // send request
    std::unique_ptr<httpclient::SimpleHttpResult> response;
    _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
      response.reset(client->retryRequest(rest::RequestType::PUT, url,
                                          requestPayload.data(),
                                          requestPayload.size(), headers));
    });

    t = TRI_microtime() - t;

    if (replutils::hasFailed(response.get())) {
      sharedStatus->gotResponse(
          replutils::buildHttpError(response.get(), url, _config.connection),
          t);
      return;
    }

    // success!
    sharedStatus->gotResponse(std::move(response), t);
  } catch (basics::Exception const& ex) {
    sharedStatus->gotResponse(Result(ex.code(), ex.what()));
  } catch (std::exception const& ex) {
    sharedStatus->gotResponse(Result(TRI_ERROR_INTERNAL, ex.what()));
  }
}

/// @brief incrementally fetch data from a collection using revisions as the
/// primary document identifier
Result DatabaseInitialSyncer::fetchCollectionSyncByRevisions(
    LogicalCollection* coll, std::string const& leaderColl,
    TRI_voc_tick_t maxTick) {
  using basics::StringUtils::urlEncode;
  using transaction::Hints;

  ReplicationMetricsFeature::InitialSyncStats stats(
      vocbase().server().getFeature<ReplicationMetricsFeature>(), true);

  double const startTime = TRI_microtime();

  if (!_config.isChild()) {
    batchExtend();
  }

  std::unique_ptr<containers::RevisionTree> treeLeader;
  std::string const baseUrl = absl::StrCat(replutils::ReplicationUrl, "/",
                                           RestReplicationHandler::Revisions);

  // get leader tree
  {
    std::string url = absl::StrCat(baseUrl, "/", RestReplicationHandler::Tree,
                                   "?collection=", urlEncode(leaderColl),
                                   "&onlyPopulated=true&to=", maxTick,
                                   "&serverId=", _state.localServerIdString,
                                   "&batchId=", _config.batch.id);

    _config.progress.set(
        absl::StrCat("fetching collection revision tree for collection '",
                     coll->name(), "' from ", url));

    auto headers = replutils::createHeaders();
    std::unique_ptr<httpclient::SimpleHttpResult> response;
    double t = TRI_microtime();
    _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
      response.reset(client->retryRequest(rest::RequestType::GET, url, nullptr,
                                          0, headers));
    });
    stats.waitedForInitial += TRI_microtime() - t;

    if (replutils::hasFailed(response.get())) {
      if (response &&
          response->getHttpReturnCode() ==
              static_cast<int>(rest::ResponseCode::NOT_IMPLEMENTED)) {
        // collection on leader doesn't support revisions-based protocol,
        // fallback
        return fetchCollectionSyncByKeys(coll, leaderColl, maxTick);
      }
      stats.numFailedConnects++;
      return replutils::buildHttpError(response.get(), url, _config.connection);
    }

    if (response->hasContentLength()) {
      stats.numSyncBytesReceived += response->getContentLength();
    }

    auto body = response->getBodyVelocyPack();
    if (!body) {
      ++stats.numFailedConnects;
      return Result(
          TRI_ERROR_INTERNAL,
          "received improperly formed response when fetching revision tree");
    }
    treeLeader = containers::RevisionTree::deserialize(body->slice());
    if (!treeLeader) {
      ++stats.numFailedConnects;
      return Result(
          TRI_ERROR_REPLICATION_INVALID_RESPONSE,
          absl::StrCat("got invalid response from leader at ",
                       _config.leader.endpoint, url,
                       ": response does not contain a valid revision tree"));
    }

    if (treeLeader->count() == 0) {
      // remote collection has no documents. now truncate our local collection
      auto operationOrigin = transaction::OperationOriginInternal{
          "truncating collection in replication"};
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::create(vocbase(), operationOrigin),
          *coll, AccessMode::Type::EXCLUSIVE);
      trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
      trx.addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
      Result res = trx.begin();

      if (!res.ok()) {
        return Result(
            res.errorNumber(),
            absl::StrCat("unable to start transaction: ", res.errorMessage()));
      }

      OperationOptions options;

      if (!_state.leaderId.empty()) {
        options.isSynchronousReplicationFrom = _state.leaderId;
      }

      OperationResult opRes = trx.truncate(coll->name(), options);

      if (opRes.fail()) {
        return Result(opRes.errorNumber(),
                      absl::StrCat("unable to truncate collection '",
                                   coll->name(), "': ", opRes.errorMessage()));
      }

      return trx.finish(opRes.result);
    }
  }

  if (isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  PhysicalCollection* physical = coll->getPhysical();
  TRI_ASSERT(physical);

  auto operationOrigin = transaction::OperationOriginInternal{
      "applying initial changes in replication"};

  auto context =
      transaction::StandaloneContext::create(coll->vocbase(), operationOrigin);
  TransactionId blockerId = context->generateId();
  physical->placeRevisionTreeBlocker(blockerId);

  auto blockerGuard = scopeGuard([&]() noexcept {  // remove blocker afterwards
    try {
      physical->removeRevisionTreeBlocker(blockerId);
    } catch (std::exception const& ex) {
      LOG_TOPIC("e020c", ERR, Logger::REPLICATION)
          << "Failed to remove revision tree blocker: " << ex.what();
    }
  });
  std::unique_ptr<SingleCollectionTransaction> trx;
  transaction::Options options;
  // We do intermediate commits relatively frequently, since this is good
  // for performance, and we actually have no transactional needs here.
  options.intermediateCommitCount = 10000;
  TRI_IF_FAILURE("IncrementalReplicationFrequentIntermediateCommit") {
    options.intermediateCommitCount = 1000;
  }
  try {
    trx = std::make_unique<SingleCollectionTransaction>(
        std::move(context), *coll, AccessMode::Type::EXCLUSIVE, options);
  } catch (basics::Exception const& ex) {
    if (ex.code() == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
      if (coll->deleted()) {
        // TODO handle
        setAborted(true);  // probably wrong?
        return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
      }
    }
    return Result(ex.code(), ex.what());
  }

  // we must be able to read our own writes here - otherwise the end result
  // can be wrong. do not enable NO_INDEXING here!

  // turn on intermediate commits as the number of keys to delete can be huge
  // here
  trx->addHint(Hints::Hint::INTERMEDIATE_COMMITS);
  Result res = trx->begin();
  if (!res.ok()) {
    return Result(
        res.errorNumber(),
        absl::StrCat("unable to start transaction: ", res.errorMessage()));
  }
  auto guard = scopeGuard([trx = trx.get()]() noexcept {
    auto res = basics::catchToResult([&]() -> Result {
      if (trx->status() == transaction::Status::RUNNING) {
        return trx->abort();
      }
      return {};
    });
    if (res.fail()) {
      LOG_TOPIC("1a537", ERR, Logger::REPLICATION)
          << "Failed to abort transaction: " << res;
    }
  });

  // diff with local tree
  auto treeLocal = physical->revisionTree(*trx);
  if (!treeLocal) {
    // local collection does not support syncing by revision, fall back to
    // keys
    guard.fire();
    return fetchCollectionSyncByKeys(coll, leaderColl, maxTick);
  }
  // make sure revision tree blocker is removed
  blockerGuard.fire();

  std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges =
      treeLeader->diff(*treeLocal);
  if (ranges.empty()) {
    // no differences, done!
    setProgress("no differences between two revision trees, ending");
    return Result{};
  }

  // encoding revisions as HLC timestamps is supported from the following
  // versions:
  // - 3.8.8 or higher
  // - 3.9.4 or higher
  // - 3.10.1 or higher
  // - 3.11.0 or higher
  // - 4.0 higher
  bool encodeAsHLC =
      _config.leader.majorVersion >= 4 ||
      (_config.leader.majorVersion >= 3 && _config.leader.minorVersion >= 11) ||
      (_config.leader.majorVersion >= 3 && _config.leader.minorVersion >= 10 &&
       _config.leader.patchVersion >= 1) ||
      (_config.leader.majorVersion >= 3 && _config.leader.minorVersion >= 9 &&
       _config.leader.patchVersion >= 4) ||
      (_config.leader.majorVersion >= 3 && _config.leader.minorVersion >= 8 &&
       _config.leader.patchVersion >= 8);

  TRI_IF_FAILURE("SyncerNoEncodeAsHLC") { encodeAsHLC = false; }

  // now lets get the actual ranges and handle the differences
  VPackBuilder requestBuilder;
  {
    VPackArrayBuilder list(&requestBuilder);
    for (auto const& pair : ranges) {
      VPackArrayBuilder range(&requestBuilder);
      // ok to use only HLC encoding here.
      requestBuilder.add(VPackValue(RevisionId{pair.first}.toHLC()));
      requestBuilder.add(VPackValue(RevisionId{pair.second}.toHLC()));
    }
  }
  std::string const requestPayload = requestBuilder.slice().toJson();

  std::string const url = absl::StrCat(
      baseUrl, "/", RestReplicationHandler::Ranges,
      "?collection=", urlEncode(leaderColl),
      "&serverId=", _state.localServerIdString, "&batchId=", _config.batch.id);

  {
    std::unique_ptr<ReplicationIterator> iter =
        physical->getReplicationIterator(
            ReplicationIterator::Ordering::Revision, *trx);
    if (!iter) {
      return Result(TRI_ERROR_INTERNAL, "could not get replication iterator");
    }

    RevisionReplicationIterator& local =
        *static_cast<RevisionReplicationIterator*>(iter.get());

    uint64_t const documentsFound = treeLocal->count();
    auto& nf = coll->vocbase().server().getFeature<NetworkFeature>();

    std::vector<RevisionId> toFetch;
    std::vector<RevisionId> toRemove;

    auto handleFetch = [this, &toFetch, &nf, &trx, &coll, &leaderColl,
                        &encodeAsHLC, &stats](RevisionId const& id) -> Result {
      if (toFetch.empty()) {
        toFetch.reserve(4096);
      }
      toFetch.emplace_back(id);

      Result res;
      // check if we need to do something to prevent toFetch from growing too
      // much in memory. note: we are not using the intermediateCommitCount
      // limit here, because fetchRevisions splits the revisions to fetch in
      // chunks of 5000 and fetches them all in parallel. using a too low limit
      // here would counteract that.
      if (toFetch.size() >= 250000) {
        res = fetchRevisions(nf, *trx, _config, _state, *coll, leaderColl,
                             encodeAsHLC, toFetch, stats);
        TRI_ASSERT(res.fail() || toFetch.empty());
      }
      return res;
    };

    auto handleRemoval = [&toRemove, &options, &coll, &trx,
                          &stats](RevisionId const& id) -> Result {
      // make sure we don't realloc uselessly for the initial inserts
      if (toRemove.empty()) {
        toRemove.reserve(4096);
      }
      toRemove.emplace_back(id);

      Result res;
      // check if we need to do something to prevent toRemove from growing too
      // much in memory
      if (toRemove.size() >= options.intermediateCommitCount) {
        res = removeRevisions(*trx, *coll, toRemove, stats);
        TRI_ASSERT(res.fail() || toRemove.empty());
      }
      return res;
    };

    RevisionId requestResume{ranges[0].first};  // start with beginning
    RevisionId iterResume = requestResume;
    std::size_t chunk = 0;

    // the shared status will wait in its destructor until all posted
    // requests have been completed/canceled!
    auto self = shared_from_this();
    Syncer::JobSynchronizerScope sharedStatus(self);

    // order initial chunk. this will block until the initial response
    // has arrived
    fetchRevisionsChunk(sharedStatus.clone(), url, coll, leaderColl,
                        requestPayload, requestResume);

    // Builder will be recycled
    VPackBuilder responseBuilder;

    while (requestResume < RevisionId::max()) {
      std::unique_ptr<httpclient::SimpleHttpResult> chunkResponse;

      // block until we either got a response or were shut down
      Result res = sharedStatus->waitForResponse(chunkResponse);

      // update our statistics
      ++stats.numKeysRequests;
      stats.waitedForKeys += sharedStatus->time();

      if (res.fail()) {
        // no response or error or shutdown
        return res;
      }

      // now we have got a response!
      TRI_ASSERT(chunkResponse != nullptr);

      if (chunkResponse->hasContentLength()) {
        stats.numSyncBytesReceived += chunkResponse->getContentLength();
      }

      VPackSlice slice;

      if (isVelocyPack(*chunkResponse)) {
        // velocypack body...

        // intentional copy of options
        VPackOptions validationOptions =
            basics::VelocyPackHelper::strictRequestValidationOptions;
        // allow custom types being sent here
        validationOptions.disallowCustom = false;
        VPackValidator validator(&validationOptions);

        validator.validate(chunkResponse->getBody().begin(),
                           chunkResponse->getBody().length(),
                           /*isSubPart*/ false);

        slice = VPackSlice(
            reinterpret_cast<uint8_t const*>(chunkResponse->getBody().begin()));
      } else {
        // JSON body...
        // recycle builder
        responseBuilder.clear();
        Result r =
            replutils::parseResponse(responseBuilder, chunkResponse.get());
        if (r.fail()) {
          ++stats.numFailedConnects;
          return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                        absl::StrCat("got invalid response from leader at ",
                                     _config.leader.endpoint, url, ": ",
                                     r.errorMessage()));
        }
        slice = responseBuilder.slice();
      }

      if (!slice.isObject()) {
        ++stats.numFailedConnects;
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      absl::StrCat("got invalid response from leader at ",
                                   _config.leader.endpoint, url,
                                   ": response is not an object"));
      }

      if (VPackSlice s = slice.get(StaticStrings::RevisionTreeResumeHLC);
          s.isString()) {
        // use "resumeHLC" if it is present
        requestResume = RevisionId::fromHLC(s.stringView());
      } else {
        // "resumeHLC" not present.
        // now fall back to using "resume", which is deprecated
        VPackSlice resumeSlice = slice.get(StaticStrings::RevisionTreeResume);
        if (!resumeSlice.isNone() && !resumeSlice.isString()) {
          ++stats.numFailedConnects;
          return Result(
              TRI_ERROR_REPLICATION_INVALID_RESPONSE,
              absl::StrCat("got invalid response from leader at ",
                           _config.leader.endpoint, url,
                           ": response field 'resume' is not a number"));
        }
        requestResume = resumeSlice.isNone()
                            ? RevisionId::max()
                            : RevisionId::fromSlice(resumeSlice);
      }

      if (requestResume < RevisionId::max() && !isAborted()) {
        // already fetch next chunk in the background, by posting the
        // request to the scheduler, which can run it asynchronously
        sharedStatus->request([self, url, sharedStatus = sharedStatus.clone(),
                               coll, leaderColl, requestResume,
                               &requestPayload]() {
          std::static_pointer_cast<DatabaseInitialSyncer>(self)
              ->fetchRevisionsChunk(sharedStatus, url, coll, leaderColl,
                                    requestPayload, requestResume);
        });
      }

      VPackSlice rangesSlice = slice.get("ranges");
      if (!rangesSlice.isArray()) {
        ++stats.numFailedConnects;
        return Result(
            TRI_ERROR_REPLICATION_INVALID_RESPONSE,
            absl::StrCat("got invalid response from leader at ",
                         _config.leader.endpoint, url,
                         ": response field 'ranges' is not an array"));
      }

      for (VPackSlice leaderSlice : VPackArrayIterator(rangesSlice)) {
        if (!leaderSlice.isArray()) {
          ++stats.numFailedConnects;
          return Result(
              TRI_ERROR_REPLICATION_INVALID_RESPONSE,
              absl::StrCat(
                  "got invalid response from leader at ",
                  _config.leader.endpoint, url,
                  ": response field 'ranges' entry is not a revision range"));
        }
        auto& currentRange = ranges[chunk];
        if (!local.hasMore() ||
            local.revision() < RevisionId{currentRange.first}) {
          local.seek(std::max(iterResume, RevisionId{currentRange.first}));
        }

        RevisionId removalBound =
            leaderSlice.isEmptyArray()
                ? RevisionId{currentRange.second}.next()
                : (encodeAsHLC
                       ? RevisionId::fromHLC(leaderSlice.at(0).stringView())
                       : RevisionId::fromSlice(leaderSlice.at(0)));
        TRI_ASSERT(RevisionId{currentRange.first} <= removalBound);
        TRI_ASSERT(removalBound <= RevisionId{currentRange.second}.next());
        RevisionId mixedBound =
            leaderSlice.isEmptyArray()
                ? RevisionId{currentRange.second}
                : (encodeAsHLC ? RevisionId::fromHLC(
                                     leaderSlice.at(leaderSlice.length() - 1)
                                         .stringView())
                               : RevisionId::fromSlice(
                                     leaderSlice.at(leaderSlice.length() - 1)));
        TRI_ASSERT(RevisionId{currentRange.first} <= mixedBound);
        TRI_ASSERT(mixedBound <= RevisionId{currentRange.second});

        while (local.hasMore() && local.revision() < removalBound) {
          res = handleRemoval(local.revision());
          if (res.fail()) {
            return res;
          }

          iterResume = std::max(iterResume, local.revision().next());
          local.next();
        }

        std::size_t index = 0;
        while (local.hasMore() && local.revision() <= mixedBound) {
          RevisionId leaderRev;
          if (encodeAsHLC) {
            leaderRev = RevisionId::fromHLC(leaderSlice.at(index).stringView());
          } else {
            leaderRev = RevisionId::fromSlice(leaderSlice.at(index));
          }

          if (local.revision() < leaderRev) {
            res = handleRemoval(local.revision());
            if (res.fail()) {
              return res;
            }

            iterResume = std::max(iterResume, local.revision().next());
            local.next();
          } else if (leaderRev < local.revision()) {
            res = handleFetch(leaderRev);
            if (res.fail()) {
              return res;
            }
            ++index;
            iterResume = std::max(iterResume, leaderRev.next());
          } else {
            TRI_ASSERT(local.revision() == leaderRev);
            // match, no need to remove local or fetch from leader
            ++index;
            iterResume = std::max(iterResume, leaderRev.next());
            local.next();
          }
        }
        for (; index < leaderSlice.length(); ++index) {
          RevisionId leaderRev;
          if (encodeAsHLC) {
            leaderRev = RevisionId::fromHLC(leaderSlice.at(index).stringView());
          } else {
            leaderRev = RevisionId::fromSlice(leaderSlice.at(index));
          }
          // fetch any leftovers
          res = handleFetch(leaderRev);
          if (res.fail()) {
            return res;
          }
          iterResume = std::max(iterResume, leaderRev.next());
        }

        while (local.hasMore() &&
               local.revision() <= std::min(requestResume.previous(),
                                            RevisionId{currentRange.second})) {
          res = handleRemoval(local.revision());
          if (res.fail()) {
            return res;
          }

          iterResume = std::max(iterResume, local.revision().next());
          local.next();
        }

        if (requestResume > RevisionId{currentRange.second}) {
          ++chunk;
        }
      }

      res = removeRevisions(*trx, *coll, toRemove, stats);
      TRI_ASSERT(res.fail() || toRemove.empty());
      if (res.fail()) {
        return res;
      }

      res = fetchRevisions(nf, *trx, _config, _state, *coll, leaderColl,
                           encodeAsHLC, toFetch, stats);
      TRI_ASSERT(res.fail() || toFetch.empty());
      if (res.fail()) {
        return res;
      }
    }

    // adjust counts
    {
      uint64_t numberDocumentsAfterSync =
          documentsFound + stats.numDocsInserted - stats.numDocsRemoved;
      uint64_t numberDocumentsDueToCounter =
          physical->numberDocuments(trx.get());

      setProgress(
          absl::StrCat("number of remaining documents in collection '",
                       coll->name(), "': ", numberDocumentsAfterSync,
                       ", number of documents due to collection count: ",
                       numberDocumentsDueToCounter));

      if (numberDocumentsAfterSync != numberDocumentsDueToCounter) {
        int64_t diff = static_cast<int64_t>(numberDocumentsAfterSync) -
                       static_cast<int64_t>(numberDocumentsDueToCounter);

        LOG_TOPIC("118bf", WARN, Logger::REPLICATION)
            << "number of remaining documents in collection '" << coll->name()
            << "' is " << numberDocumentsAfterSync << " and differs from "
            << "number of documents returned by collection count "
            << numberDocumentsDueToCounter
            << ", documents found: " << documentsFound
            << ", num docs inserted: " << stats.numDocsInserted
            << ", num docs removed: " << stats.numDocsRemoved << ", a diff of "
            << diff << " will be applied";

        // patch the document counter of the collection and the transaction
        trx->documentCollection()->getPhysical()->adjustNumberDocuments(*trx,
                                                                        diff);
      }
    }

    res = trx->commit();
    if (res.fail()) {
      return res;
    }
    TRI_ASSERT(requestResume == RevisionId::max());
  }

  setProgress(absl::StrCat(
      "incremental tree sync statistics for collection '", coll->name(),
      "': keys requests: ", stats.numKeysRequests, ", docs requests: ",
      stats.numDocsRequests, ", bytes received: ", stats.numSyncBytesReceived,
      ", number of documents requested: ", stats.numDocsRequested,
      ", number of documents inserted: ", stats.numDocsInserted,
      ", number of documents removed: ", stats.numDocsRemoved,
      ", waited for initial: ", stats.waitedForInitial, " s, waited for keys: ",
      stats.waitedForKeys, " s, waited for docs: ", stats.waitedForDocs,
      " s, waited for insertions: ", stats.waitedForInsertions, " s, ",
      "waited for removals: ", stats.waitedForRemovals,
      " s, total time: ", (TRI_microtime() - startTime), " s"));

  return Result{};
}

/// @brief incrementally fetch data from a collection
/// @brief changes the properties of a collection, based on the VelocyPack
/// provided
Result DatabaseInitialSyncer::changeCollection(LogicalCollection* col,
                                               velocypack::Slice slice) {
  CollectionGuard guard(&vocbase(), col->id());

  return guard.collection()->properties(slice);
}

/// @brief whether or not the collection has documents
bool DatabaseInitialSyncer::hasDocuments(LogicalCollection const& col) {
  return col.getPhysical()->hasDocuments();
}

/// @brief handle the information about a collection
Result DatabaseInitialSyncer::handleCollection(velocypack::Slice parameters,
                                               velocypack::Slice indexes,
                                               bool incremental,
                                               SyncPhase phase) {
  using basics::StringUtils::itoa;

  if (isAborted()) {
    return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
  }

  if (!parameters.isObject() || !indexes.isArray()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE);
  }

  if (!_config.isChild()) {
    batchExtend();
  }

  std::string const leaderName =
      basics::VelocyPackHelper::getStringValue(parameters, "name", "");

  DataSourceId const leaderCid{
      basics::VelocyPackHelper::extractIdValue(parameters)};

  if (leaderCid.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "collection id is missing in response");
  }

  std::string const leaderUuid = basics::VelocyPackHelper::getStringValue(
      parameters, "globallyUniqueId", "");

  VPackSlice const type = parameters.get("type");

  if (!type.isNumber()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "collection type is missing in response");
  }

  std::string const typeString =
      (type.getNumber<int>() == 3 ? "edge" : "document");

  std::string const collectionMsg =
      absl::StrCat("collection '", leaderName, "', type ", typeString, ", id ",
                   leaderCid.id());

  // phase handling
  if (phase == PHASE_VALIDATE) {
    // validation phase just returns ok if we got here (aborts above if data
    // is invalid)
    _config.progress.processedCollections.try_emplace(leaderCid, leaderName);

    return Result();
  }

  // drop and re-create collections locally
  // -------------------------------------------------------------------------------------

  if (phase == PHASE_DROP_CREATE) {
    auto col = resolveCollection(vocbase(), parameters);

    if (col == nullptr) {
      // not found...
      col = vocbase().lookupCollection(leaderName);

      if (col != nullptr &&
          (col->name() != leaderName ||
           (!leaderUuid.empty() && col->guid() != leaderUuid))) {
        // found another collection with the same name locally.
        // in this case we must drop it because we will run into duplicate
        // name conflicts otherwise
        try {
          auto res = vocbase().dropCollection(col->id(), true);

          if (res.ok()) {
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

          if (col->name() == StaticStrings::UsersCollection) {
            // better not throw away the _users collection. otherwise it is
            // gone and this may be a problem if the server crashes
            // in-between.
            truncate = true;
          }

          if (truncate) {
            // system collection
            _config.progress.set("truncating " + collectionMsg);

            auto operationOrigin = transaction::OperationOriginInternal{
                "truncating collection for replication"};

            SingleCollectionTransaction trx(
                transaction::StandaloneContext::create(vocbase(),
                                                       operationOrigin),
                *col, AccessMode::Type::EXCLUSIVE);
            trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
            trx.addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
            Result res = trx.begin();

            if (!res.ok()) {
              return Result(res.errorNumber(),
                            absl::StrCat("unable to truncate ", collectionMsg,
                                         ": ", res.errorMessage()));
            }

            OperationOptions options;

            if (!_state.leaderId.empty()) {
              options.isSynchronousReplicationFrom = _state.leaderId;
            }

            OperationResult opRes = trx.truncate(col->name(), options);

            if (opRes.fail()) {
              return Result(opRes.errorNumber(),
                            absl::StrCat("unable to truncate ", collectionMsg,
                                         ": ", opRes.errorMessage()));
            }

            res = trx.finish(opRes.result);

            if (!res.ok()) {
              return Result(res.errorNumber(),
                            absl::StrCat("unable to truncate ", collectionMsg,
                                         ": ", res.errorMessage()));
            }
          } else {
            // drop a regular collection
            if (_config.applier._skipCreateDrop) {
              _config.progress.set("dropping " + collectionMsg +
                                   " skipped because of configuration");
              return Result();
            }
            _config.progress.set("dropping " + collectionMsg);

            auto res = vocbase().dropCollection(col->id(), true);

            if (res.fail()) {
              return Result(res.errorNumber(),
                            absl::StrCat("unable to drop ", collectionMsg, ": ",
                                         res.errorMessage()));
            }
          }
        }
      } else {
        // incremental case
        TRI_ASSERT(incremental);

        // collection is already present
        _config.progress.set("checking/changing parameters of " +
                             collectionMsg);
        return changeCollection(col.get(), parameters);
      }
    }
    // When we get here, col is a nullptr anyway!

    std::string msg = "creating " + collectionMsg;
    if (_config.applier._skipCreateDrop) {
      msg += " skipped because of configuration";
      _config.progress.set(msg);
      return Result();
    }
    _config.progress.set(msg);

    LOG_TOPIC("7093d", DEBUG, Logger::REPLICATION)
        << "Dump is creating collection " << parameters.toJson();

    LogicalCollection* col2 = nullptr;
    auto r = createCollection(vocbase(), parameters, &col2);

    if (r.fail()) {
      return Result(r.errorNumber(),
                    absl::StrCat("unable to create ", collectionMsg, ": ",
                                 r.errorMessage(), ". Collection info ",
                                 parameters.toJson()));
    }

    return r;
  }

  // sync collection data
  // -------------------------------------------------------------------------------------

  else if (phase == PHASE_DUMP) {
    _config.progress.set("dumping data for " + collectionMsg);

    std::shared_ptr<LogicalCollection> col =
        resolveCollection(vocbase(), parameters);

    if (col == nullptr) {
      return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                    absl::StrCat("cannot dump: ", collectionMsg,
                                 " not found on leader. Collection info ",
                                 parameters.toJson()));
    }

    std::string const leaderColl =
        !leaderUuid.empty() ? leaderUuid : itoa(leaderCid.id());
    auto res = incremental && hasDocuments(*col)
                   ? fetchCollectionSync(col.get(), leaderColl,
                                         _config.leader.lastLogTick)
                   : fetchCollectionDump(col.get(), leaderColl,
                                         _config.leader.lastLogTick);

    if (!res.ok()) {
      return res;
    } else if (isAborted()) {
      return res.reset(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
    }

    if (leaderName == StaticStrings::UsersCollection) {
      reloadUsers();
    } else if (leaderName == StaticStrings::AnalyzersCollection &&
               ServerState::instance()->isSingleServer() &&
               vocbase()
                   .server()
                   .hasFeature<iresearch::IResearchAnalyzerFeature>()) {
      vocbase()
          .server()
          .getFeature<iresearch::IResearchAnalyzerFeature>()
          .invalidate(vocbase(), transaction::OperationOriginInternal{
                                     "invalidating analyzers"});
    }

    // schmutz++ creates indexes on DBServers
    if (_config.applier._skipCreateDrop) {
      _config.progress.set(absl::StrCat("creating indexes for ", collectionMsg,
                                        " skipped because of configuration"));
      return res;
    }

    // now create indexes
    TRI_ASSERT(indexes.isArray());
    VPackValueLength const numIdx = indexes.length();
    if (numIdx > 0) {
      if (!_config.isChild()) {
        batchExtend();
      }

      _config.progress.set(
          absl::StrCat("creating ", numIdx, " index(es) for ", collectionMsg));

      try {
        for (auto idxDef : VPackArrayIterator(indexes)) {
          if (idxDef.isObject()) {
            VPackSlice type = idxDef.get(StaticStrings::IndexType);
            if (type.isString()) {
              _config.progress.set(absl::StrCat("creating index of type ",
                                                type.stringView(), " for ",
                                                collectionMsg));
            }
          }

          createIndexInternal(idxDef, *col);
        }
      } catch (basics::Exception const& ex) {
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
Result DatabaseInitialSyncer::fetchInventory(VPackBuilder& builder) {
  std::string url =
      absl::StrCat(replutils::ReplicationUrl,
                   "/inventory?serverId=", _state.localServerIdString,
                   "&batchId=", _config.batch.id);
  if (_config.applier._includeSystem) {
    url += "&includeSystem=true";
  }
  if (_config.applier._includeFoxxQueues) {
    url += "&includeFoxxQueues=true";
  }

  // use an optmization here for shard synchronization: only fetch the
  // inventory including a single shard. this can greatly reduce the size of
  // the response.
  if (ServerState::instance()->isDBServer() && !_config.isChild() &&
      _config.applier._skipCreateDrop &&
      _config.applier._restrictType ==
          ReplicationApplierConfiguration::RestrictType::Include &&
      _config.applier._restrictCollections.size() == 1) {
    url += "&collection=" + basics::StringUtils::urlEncode(*(
                                _config.applier._restrictCollections.begin()));
  }

  // send request
  _config.progress.set(absl::StrCat("fetching leader inventory from ", url));
  std::unique_ptr<httpclient::SimpleHttpResult> response;
  _config.connection.lease([&](httpclient::SimpleHttpClient* client) {
    auto headers = replutils::createHeaders();
    response.reset(
        client->retryRequest(rest::RequestType::GET, url, nullptr, 0, headers));
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
        absl::StrCat(
            "got invalid response from leader at ", _config.leader.endpoint,
            url, ": invalid response type for initial data. expecting array"));
  }

  VPackSlice slice = builder.slice();
  if (!slice.isObject()) {
    LOG_TOPIC("3b1e6", DEBUG, Logger::REPLICATION)
        << "client: DatabaseInitialSyncer::run - inventoryResponse is not an "
           "object";

    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  absl::StrCat("got invalid response from leader at ",
                               _config.leader.endpoint, url, ": invalid JSON"));
  }

  return r;
}

/// @brief handle the inventory response of the leader
Result DatabaseInitialSyncer::handleCollectionsAndViews(
    velocypack::Slice collSlices, velocypack::Slice viewSlices,
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

    std::string const leaderName =
        basics::VelocyPackHelper::getStringValue(parameters, "name", "");

    if (leaderName.empty()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "collection name is missing in response");
    }

    if (TRI_ExcludeCollectionReplication(leaderName,
                                         _config.applier._includeSystem,
                                         _config.applier._includeFoxxQueues)) {
      continue;
    }

    if (basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                  false)) {
      // we don't care about deleted collections
      continue;
    }

    if (_config.applier._restrictType !=
        ReplicationApplierConfiguration::RestrictType::None) {
      auto const it = _config.applier._restrictCollections.find(leaderName);
      bool found = (it != _config.applier._restrictCollections.end());

      if (_config.applier._restrictType ==
              ReplicationApplierConfiguration::RestrictType::Include &&
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

    if (leaderName == StaticStrings::AnalyzersCollection) {
      // _analyzers collection has to be restored before view creation
      systemCollections.emplace_back(parameters, indexes);
    } else {
      collections.emplace_back(parameters, indexes);
    }
  }

  // STEP 1: validate collection declarations from leader
  // ----------------------------------------------------------------------------------

  // STEP 2: drop and re-create collections locally if they are also present
  // on the leader
  //  ------------------------------------------------------------------------------------

  // iterate over all collections from the leader...
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
  auto const res =
      iterateCollections(systemCollections, incremental, PHASE_DUMP);

  if (res.fail()) {
    return res;
  }

  // STEP 4: now that the collections exist create the "arangosearch" views
  // this should be faster than re-indexing afterwards
  // We don't create "search-alias" view because inverted indexes don't exist
  // yet
  // ----------------------------------------------------------------------------------

  if (!_config.applier._skipCreateDrop &&
      _config.applier._restrictCollections.empty() && viewSlices.isArray()) {
    // views are optional, and 3.3 and before will not send any view data
    auto r = handleViewCreation(viewSlices,
                                iresearch::StaticStrings::ViewArangoSearchType);
    if (r.fail()) {
      LOG_TOPIC("96cda", ERR, Logger::REPLICATION)
          << "Error during initial sync view creation: " << r.errorMessage();
      return r;
    }
  } else {
    _config.progress.set("view creation skipped because of configuration");
  }

  // STEP 5: sync collection data from leader and create initial indexes
  // ----------------------------------------------------------------------------------
  // now load the data into the collections
  auto r = iterateCollections(collections, incremental, PHASE_DUMP);
  if (r.fail()) {
    return r;
  }

  // STEP 6 load "search-alias" views
  // ----------------------------------------------------------------------------------
  return handleViewCreation(viewSlices,
                            iresearch::StaticStrings::ViewSearchAliasType);
}

/// @brief iterate over all collections from an array and apply an action
Result DatabaseInitialSyncer::iterateCollections(
    std::vector<std::pair<velocypack::Slice, velocypack::Slice>> const&
        collections,
    bool incremental, SyncPhase phase) {
  std::string phaseMsg(absl::StrCat("starting phase ", translatePhase(phase),
                                    " with ", collections.size(),
                                    " collections"));
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
Result DatabaseInitialSyncer::handleViewCreation(velocypack::Slice views,
                                                 std::string_view type) {
  if (!views.isArray()) {
    return {TRI_ERROR_BAD_PARAMETER};
  }
  auto check = [&](VPackSlice definition) noexcept {
    // I want to cause fail in createView if definition is invalid
    if (!definition.isObject()) {
      return true;
    }
    if (!definition.hasKey(StaticStrings::DataSourceType)) {
      return true;
    }
    auto sliceType = definition.get(StaticStrings::DataSourceType);
    if (!sliceType.isString()) {
      return true;
    }
    return sliceType.stringView() != type;
  };
  for (VPackSlice slice : VPackArrayIterator(views)) {
    if (check(slice)) {
      continue;
    }
    Result res = createView(vocbase(), slice);
    if (res.fail()) {
      return res;
    }
  }
  return {};
}

Result DatabaseInitialSyncer::batchStart(char const* context,
                                         std::string const& patchCount) {
  return _config.batch.start(_config.connection, _config.progress,
                             _config.leader, _config.state.syncerId, context,
                             patchCount);
}

Result DatabaseInitialSyncer::batchExtend() {
  return _config.batch.extend(_config.connection, _config.progress,
                              _config.state.syncerId);
}

Result DatabaseInitialSyncer::batchFinish() noexcept {
  return _config.batch.finish(_config.connection, _config.progress,
                              _config.state.syncerId);
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
/// @brief patch quickKeysNumDocsLimit for testing
void DatabaseInitialSyncer::adjustQuickKeysNumDocsLimit() {
  TRI_IF_FAILURE("RocksDBRestReplicationHandler::quickKeysNumDocsLimit100") {
    _quickKeysNumDocsLimit = 100;
  }
}
#endif

}  // namespace arangodb
