////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchDataStore.h"
#include "IResearchDocument.h"
#include "IResearchKludge.h"
#include "IResearchFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/DownCast.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Metrics/Gauge.h"
#include "Metrics/Guard.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/IResearch/IResearchDocumentEE.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ClusterFeature.h"
#endif

#include <index/column_info.hpp>
#include <store/caching_directory.hpp>
#include <store/mmap_directory.hpp>
#include <store/store_utils.hpp>
#include <utils/encryption.hpp>
#include <utils/singleton.hpp>
#include <chrono>
#include <string>

#include <absl/cleanup/cleanup.h>
#include <absl/strings/str_cat.h>
#include <filesystem>

using namespace std::literals;

namespace arangodb::iresearch {

#ifdef USE_ENTERPRISE
size_t getPrimaryDocsCount(irs::DirectoryReader const& reader);
#endif

namespace {

class IResearchFlushSubscription final : public FlushSubscription {
 public:
  explicit IResearchFlushSubscription(TRI_voc_tick_t tick, std::string&& name)
      : _tick{tick}, _name{std::move(name)} {}

  /// @brief earliest tick that can be released
  [[nodiscard]] TRI_voc_tick_t tick() const noexcept final {
    return _tick.load(std::memory_order_acquire);
  }

  void tick(TRI_voc_tick_t tick) noexcept {
    auto value = _tick.load(std::memory_order_acquire);
    TRI_ASSERT(value <= tick);

    // tick value must never go backwards
    while (tick > value) {
      if (_tick.compare_exchange_weak(value, tick, std::memory_order_release,
                                      std::memory_order_acquire)) {
        break;
      }
    }
  }

  std::string const& name() const final { return _name; }

 private:
  std::atomic<TRI_voc_tick_t> _tick;
  std::string const _name;
};

enum class SegmentPayloadVersion : uint32_t {
  // Only low tick stored.
  // Possibly has committed WAL records after it, but mostly uncommitted.
  SingleTick = 0,
  // Two stage commit ticks are stored.
  // low tick is fully committed but between low and high tick
  // uncommitted WAL records may be present.
  // After high tick nothing is committed.
  TwoStageTick = 1
};

bool readTick(irs::bytes_view payload, uint64_t& tickLow,
              uint64_t& tickHigh) noexcept {
  if (payload.size() < sizeof(uint64_t)) {
    LOG_TOPIC("41474", ERR, TOPIC) << "Unexpected segment payload size "
                                   << payload.size() << " for initial check";
    return false;
  }
  std::memcpy(&tickLow, payload.data(), sizeof(uint64_t));
  tickLow = irs::numeric_utils::ntoh64(tickLow);
  tickHigh = std::numeric_limits<uint64_t>::max();
  using Version = std::underlying_type_t<SegmentPayloadVersion>;
  Version version;
  if (payload.size() < sizeof(uint64_t) + sizeof(version)) {
    return true;
  }
  std::memcpy(&version, payload.data() + sizeof(uint64_t), sizeof(version));
  version = irs::numeric_utils::ntoh32(version);
  switch (version) {
    case (static_cast<Version>(SegmentPayloadVersion::TwoStageTick)): {
      if (payload.size() == (2 * sizeof(uint64_t)) + sizeof(version)) {
        std::memcpy(&tickHigh,
                    payload.data() + sizeof(uint64_t) + sizeof(version),
                    sizeof(uint64_t));
        tickHigh = irs::numeric_utils::ntoh64(tickHigh);
      } else {
        LOG_TOPIC("49b4d", ERR, TOPIC)
            << "Unexpected segment payload size " << payload.size()
            << " for version '" << version << "'";
      }
    } break;
    default:
      // falling back to SingleTick as it always present
      LOG_TOPIC("fad1f", WARN, TOPIC)
          << "Unexpected segment payload version '" << version
          << "' fallback to minimal version";
  }
  return true;
}

struct ThreadGroupStats : std::tuple<size_t, size_t, size_t> {
  explicit ThreadGroupStats(std::tuple<size_t, size_t, size_t>&& stats) noexcept
      : std::tuple<size_t, size_t, size_t>{std::move(stats)} {}
};

std::ostream& operator<<(std::ostream& out, ThreadGroupStats const& stats) {
  out << "Active=" << std::get<0>(stats) << ", Pending=" << std::get<1>(stats)
      << ", Threads=" << std::get<2>(stats);
  return out;
}

////////////////////////////////////////////////////////////////////////////////
/// @struct Task
/// @brief base class for asynchronous maintenance tasks
////////////////////////////////////////////////////////////////////////////////
template<typename T>
struct Task {
  void schedule(std::chrono::milliseconds delay) const {
    LOG_TOPIC("eb0da", TRACE, TOPIC)
        << "scheduled a " << T::typeName() << " task for ArangoSearch index '"
        << id << "', delay '" << delay.count() << "'";

    LOG_TOPIC("eb0d2", TRACE, TOPIC)
        << T::typeName()
        << " pool: " << ThreadGroupStats{async->stats(T::threadGroup())};

    if (!asyncLink->empty()) {
      async->queue(T::threadGroup(), delay, static_cast<T const&>(*this));
    }
  }

  std::shared_ptr<MaintenanceState> state;
  IResearchFeature* async{nullptr};
  IResearchDataStore::AsyncLinkPtr asyncLink;
  IndexId id;
};

uint64_t computeAvg(std::atomic<uint64_t>& timeNum, uint64_t newTime) {
  constexpr uint64_t kWindowSize{10};
  auto const oldTimeNum =
      timeNum.fetch_add((newTime << 32U) + 1, std::memory_order_relaxed);
  auto const oldTime = oldTimeNum >> 32U;
  auto const oldNum = oldTimeNum & std::numeric_limits<uint32_t>::max();
  if (oldNum >= kWindowSize) {
    timeNum.fetch_sub(((oldTime / oldNum) << 32U) + 1,
                      std::memory_order_relaxed);
  }
  return (oldTime + newTime) / (oldNum + 1);
}

template<typename NormyType>
auto getIndexFeatures() {
  return [](irs::type_info::type_id id) {
    TRI_ASSERT(irs::type<NormyType>::id() == id ||
               irs::type<irs::granularity_prefix>::id() == id);
    const irs::ColumnInfo info{
        irs::type<irs::compression::none>::get(), {}, false};

    if (irs::type<NormyType>::id() == id) {
      return std::make_pair(info, &NormyType::MakeWriter);
    }

    return std::make_pair(info, irs::FeatureWriterFactory{});
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts ArangoDB document into an IResearch data store
////////////////////////////////////////////////////////////////////////////////
template<typename FieldIteratorType, typename MetaType>
Result insertDocument(IResearchDataStore const& dataStore,
                      irs::IndexWriter::Transaction& ctx,
                      velocypack::Slice document, LocalDocumentId documentId,
                      MetaType const& meta) {
  auto const id = dataStore.index().id();
  auto const collectionNameString = [&] {
    if (!ServerState::instance()->isSingleServer()) {
      return std::string{};
    }
    auto name = dataStore.index().collection().name();
#ifdef USE_ENTERPRISE
    ClusterMethods::realNameFromSmartName(name);
#endif
    return name;
  }();
  std::string_view collection = collectionNameString.empty()
                                    ? meta.collectionName()
                                    : collectionNameString;
  FieldIteratorType body{collection, id};
  body.reset(document, meta);  // reset reusable container to doc

  if (!body.valid()) {
    return {};  // no fields to index
  }
  auto& field = *body;
#ifdef USE_ENTERPRISE
  auto eeRes = insertDocumentEE(ctx, body, id, documentId);
  if (eeRes.fail()) {
    return eeRes;
  }
#endif
  auto doc = ctx.Insert(body.disableFlush());
  if (!doc) {
    return {TRI_ERROR_INTERNAL,
            absl::StrCat("failed to insert document into ArangoSearch index '",
                         id.id(), "', documentId '", documentId.id(), "'")};
  }

  // User fields
  while (body.valid()) {
#ifdef USE_ENTERPRISE
    if (field._root) {
      handleNestedRoot(doc, field);
    } else
#endif
        if (ValueStorage::NONE == field._storeValues) {
      doc.template Insert<irs::Action::INDEX>(field);
    } else {
      doc.template Insert<irs::Action::INDEX | irs::Action::STORE>(field);
    }
    ++body;
  }

  // Sorted field
  {
    SortedValue field{collection, id, document};
    for (auto& sortField : meta._sort.fields()) {
      field.slice = get(document, sortField, VPackSlice::nullSlice());
      doc.template Insert<irs::Action::STORE_SORTED>(field);
    }
  }

  // Stored value field
  {
    StoredValue field{collection, id, document};
    for (auto const& column : meta._storedValues.columns()) {
      field.fieldName = column.name;
      field.fields = &column.fields;
      doc.template Insert<irs::Action::STORE>(field);
    }
  }

  // System fields
  // Indexed and Stored: LocalDocumentId
  auto docPk = DocumentPrimaryKey::encode(documentId);

  // reuse the 'Field' instance stored inside the 'FieldIterator'
  Field::setPkValue(const_cast<Field&>(field), docPk);
  doc.template Insert<irs::Action::INDEX | irs::Action::STORE>(field);

  return {};
}

std::string toString(irs::DirectoryReader const& reader, bool isNew, IndexId id,
                     uint64_t tickLow, uint64_t tickHigh) {
  auto const& meta = reader.Meta();
  std::string_view const newOrExisting{isNew ? "new" : "existing"};

  return absl::StrCat("Successfully opened data store reader for the ",
                      newOrExisting, " ArangoSearchIndex '", id.id(),
                      "', snapshot '", meta.filename, "', docs count '",
                      reader.docs_count(), "', live docs count '",
                      reader.live_docs_count(), "', number of segments '",
                      meta.index_meta.segments.size(), "', recovery tick low '",
                      tickLow, "' and recovery tick high '", tickHigh, "'");
}

template<bool WasCreated>
auto makeAfterCommitCallback(void* key) {
  return [key](TransactionState& state) {
    auto prev = state.cookie(key, nullptr);  // extract existing cookie
    if (!prev) {
      return;
    }
    // TODO FIXME find a better way to look up a ViewState
    auto& ctx = basics::downCast<IResearchTrxState>(*prev);
    if constexpr (WasCreated) {
      auto const lastOperationTick = state.lastOperationTick();
      ctx._ctx.Commit(lastOperationTick);
    } else {
      ctx._ctx.Commit();
    }
  };
}

std::string MakeMessage(std::string_view begin, Index const& index,
                        TransactionState& state, LocalDocumentId documentId,
                        auto&&... args) {
  return absl::StrCat(begin, " ArangoSearch index: ", index.id().id(),
                      ", tid: ", state.id().id(),
                      ", documentId: ", documentId.id(),
                      std::forward<decltype(args)>(args)...);
};

static std::atomic_bool kHasClusterMetrics{false};

}  // namespace

void clusterCollectionName(LogicalCollection const& collection, ClusterInfo* ci,
                           uint64_t id, bool indexIdAttribute,
                           std::string& name) {
  // Upgrade step for old link definition without collection name
  // could be received from agency while shard of the collection was moved
  // or added to the server. New links already has collection name set,
  // but here we must get this name on our own.
  if (name.empty()) {
    name = ci ? ci->getCollectionNameForShard(collection.name())
              : collection.name();
    LOG_TOPIC("86ece", TRACE, TOPIC) << "Setting collection name '" << name
                                     << "' for new index '" << id << "'";
    if (ADB_UNLIKELY(name.empty())) {
      LOG_TOPIC_IF("67da6", WARN, TOPIC, indexIdAttribute)
          << "Failed to init collection name for the index '" << id
          << "'. Index will not index '_id' attribute."
             "Please recreate the index if this is necessary!";
    }
#ifdef USE_ENTERPRISE
    ClusterMethods::realNameFromSmartName(name);
#endif
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @struct MaintenanceState
////////////////////////////////////////////////////////////////////////////////
struct MaintenanceState {
  std::atomic_size_t pendingCommits{0};
  std::atomic_size_t nonEmptyCommits{0};
  std::atomic_size_t pendingConsolidations{0};
  std::atomic_size_t noopConsolidationCount{0};
  std::atomic_size_t noopCommitCount{0};
};

////////////////////////////////////////////////////////////////////////////////
/// @struct CommitTask
/// @brief represents a commit task
/// @note thread group 0 is dedicated to commit
////////////////////////////////////////////////////////////////////////////////
struct CommitTask : Task<CommitTask> {
  static constexpr ThreadGroup threadGroup() noexcept {
    return ThreadGroup::_0;
  }

  static constexpr char const* typeName() noexcept { return "commit"; }

  void operator()();
  void finalize(IResearchDataStore& link,
                IResearchDataStore::CommitResult code);

  size_t cleanupIntervalCount{};
  std::chrono::milliseconds commitIntervalMsec{};
  std::chrono::milliseconds consolidationIntervalMsec{};
  size_t cleanupIntervalStep{};
};

void CommitTask::finalize(IResearchDataStore& link,
                          IResearchDataStore::CommitResult code) {
  constexpr size_t kMaxNonEmptyCommits = 10;
  constexpr size_t kMaxPendingConsolidations = 3;

  if (code != IResearchDataStore::CommitResult::NO_CHANGES) {
    state->pendingCommits.fetch_add(1, std::memory_order_release);
    schedule(commitIntervalMsec);

    if (code == IResearchDataStore::CommitResult::DONE) {
      state->noopCommitCount.store(0, std::memory_order_release);
      state->noopConsolidationCount.store(0, std::memory_order_release);

      if (state->pendingConsolidations.load(std::memory_order_acquire) <
              kMaxPendingConsolidations &&
          state->nonEmptyCommits.fetch_add(1, std::memory_order_acq_rel) >=
              kMaxNonEmptyCommits) {
        link.scheduleConsolidation(consolidationIntervalMsec);
        state->nonEmptyCommits.store(0, std::memory_order_release);
      }
    }
  } else {
    state->nonEmptyCommits.store(0, std::memory_order_release);
    state->noopCommitCount.fetch_add(1, std::memory_order_release);

    for (auto count = state->pendingCommits.load(std::memory_order_acquire);
         count < 1;) {
      if (state->pendingCommits.compare_exchange_weak(
              count, 1, std::memory_order_acq_rel)) {
        schedule(commitIntervalMsec);
        break;
      }
    }
  }
}

void CommitTask::operator()() {
  char const runId = 0;
  state->pendingCommits.fetch_sub(1, std::memory_order_release);

  auto linkLock = asyncLink->lock();
  if (!linkLock) {
    LOG_TOPIC("ebada", DEBUG, TOPIC)
        << "index '" << id << "' is no longer valid, run id '" << size_t(&runId)
        << "'";
    return;
  }

  auto code = IResearchDataStore::CommitResult::UNDEFINED;
  auto reschedule = scopeGuard([&code, link = linkLock.get(), this]() noexcept {
    try {
      finalize(*link, code);
    } catch (std::exception const& ex) {
      LOG_TOPIC("ad67d", ERR, TOPIC)
          << "failed to call finalize: " << ex.what();
    }
  });

  // reload RuntimeState
  {
    TRI_IF_FAILURE("IResearchCommitTask::lockDataStore") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    TRI_ASSERT(linkLock->_dataStore);
    // must be valid if linkLock->lock() is valid
    READ_LOCKER(lock, linkLock->_dataStore._mutex);
    // '_meta' can be asynchronously modified
    auto& meta = linkLock->_dataStore._meta;

    commitIntervalMsec = std::chrono::milliseconds(meta._commitIntervalMsec);
    consolidationIntervalMsec =
        std::chrono::milliseconds(meta._consolidationIntervalMsec);
    cleanupIntervalStep = meta._cleanupIntervalStep;
  }

  if (std::chrono::milliseconds::zero() == commitIntervalMsec) {
    reschedule.cancel();
    LOG_TOPIC("eba4a", DEBUG, TOPIC) << "sync is disabled for the index '" << id
                                     << "', runId '" << size_t(&runId) << "'";
    return;
  }

  TRI_IF_FAILURE("IResearchCommitTask::commitUnsafe") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  // run commit ('_asyncSelf' locked by async task)
  auto [res, timeMs] = linkLock->commitUnsafe(false, nullptr, code);

  if (res.ok()) {
    LOG_TOPIC("7e323", TRACE, TOPIC)
        << "successful sync of ArangoSearch index '" << id << "', run id '"
        << size_t(&runId) << "', took: " << timeMs << "ms";
  } else {
    LOG_TOPIC("8377b", WARN, TOPIC)
        << "error after running for " << timeMs
        << "ms while committing ArangoSearch index '" << linkLock->index().id()
        << "', run id '" << size_t(&runId) << "': " << res.errorNumber() << " "
        << res.errorMessage();
  }
  if (cleanupIntervalStep &&
      ++cleanupIntervalCount >= cleanupIntervalStep) {  // if enabled
    cleanupIntervalCount = 0;
    TRI_IF_FAILURE("IResearchCommitTask::cleanupUnsafe") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    // run cleanup ('_asyncSelf' locked by async task)
    auto [res, timeMs] = linkLock->cleanupUnsafe();

    if (res.ok()) {
      LOG_TOPIC("7e821", TRACE, TOPIC)
          << "successful cleanup of ArangoSearch index '" << id << "', run id '"
          << size_t(&runId) << "', took: " << timeMs << "ms";
    } else {
      LOG_TOPIC("130de", WARN, TOPIC)
          << "error after running for " << timeMs
          << "ms while cleaning up ArangoSearch index '" << id << "', run id '"
          << size_t(&runId) << "': " << res.errorNumber() << " "
          << res.errorMessage();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @struct ConsolidationTask
/// @brief represents a consolidation task
/// @note thread group 1 is dedicated to commit
////////////////////////////////////////////////////////////////////////////////
struct ConsolidationTask : Task<ConsolidationTask> {
  static constexpr ThreadGroup threadGroup() noexcept {
    return ThreadGroup::_1;
  }
  static constexpr char const* typeName() noexcept { return "consolidation"; }
  void operator()();
  irs::MergeWriter::FlushProgress progress;
  IResearchDataStoreMeta::ConsolidationPolicy consolidationPolicy;
  std::chrono::milliseconds consolidationIntervalMsec{};
};

void ConsolidationTask::operator()() {
  char const runId = 0;
  state->pendingConsolidations.fetch_sub(1, std::memory_order_release);

  auto linkLock = asyncLink->lock();
  if (!linkLock) {
    LOG_TOPIC("eb0d1", DEBUG, TOPIC)
        << "index '" << id << "' is no longer valid, run id '" << size_t(&runId)
        << "'";
    return;
  }
  auto reschedule = scopeGuard([this]() noexcept {
    try {
      for (auto count =
               state->pendingConsolidations.load(std::memory_order_acquire);
           count < 1;) {
        if (state->pendingConsolidations.compare_exchange_weak(
                count, count + 1, std::memory_order_acq_rel)) {
          schedule(consolidationIntervalMsec);
          break;
        }
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("2642a", ERR, TOPIC) << "failed to reschedule: " << ex.what();
    }
  });

  // reload RuntimeState
  {
    TRI_IF_FAILURE("IResearchConsolidationTask::lockDataStore") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_ASSERT(linkLock->_dataStore);
    // must be valid if _asyncSelf->lock() is valid
    READ_LOCKER(lock, linkLock->_dataStore._mutex);
    // '_meta' can be asynchronously modified
    auto& meta = linkLock->_dataStore._meta;

    consolidationPolicy = meta._consolidationPolicy;
    consolidationIntervalMsec =
        std::chrono::milliseconds(meta._consolidationIntervalMsec);
  }
  if (std::chrono::milliseconds::zero() ==
          consolidationIntervalMsec        // disabled via interval
      || !consolidationPolicy.policy()) {  // disabled via policy
    reschedule.cancel();

    LOG_TOPIC("eba3a", DEBUG, TOPIC)
        << "consolidation is disabled for the index '" << id << "', runId '"
        << size_t(&runId) << "'";
    return;
  }
  constexpr size_t kMaxNoopCommits = 10;
  constexpr size_t kMaxNoopConsolidations = 10;
  if (state->noopCommitCount.load(std::memory_order_acquire) <
          kMaxNoopCommits &&
      state->noopConsolidationCount.load(std::memory_order_acquire) <
          kMaxNoopConsolidations) {
    state->pendingConsolidations.fetch_add(1, std::memory_order_release);
    schedule(consolidationIntervalMsec);
  }
  TRI_IF_FAILURE("IResearchConsolidationTask::consolidateUnsafe") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // run consolidation ('_asyncSelf' locked by async task)
  bool emptyConsolidation = false;
  auto const [res, timeMs] = linkLock->consolidateUnsafe(
      consolidationPolicy, progress, emptyConsolidation);

  if (res.ok()) {
    if (emptyConsolidation) {
      state->noopConsolidationCount.fetch_add(1, std::memory_order_release);
    } else {
      state->noopConsolidationCount.store(0, std::memory_order_release);
    }
    LOG_TOPIC("7e828", TRACE, TOPIC)
        << "successful consolidation of ArangoSearch index '"
        << linkLock->index().id() << "', run id '" << size_t(&runId)
        << "', took: " << timeMs << "ms";
  } else {
    LOG_TOPIC("bce4f", DEBUG, TOPIC)
        << "error after running for " << timeMs
        << "ms while consolidating ArangoSearch index '"
        << linkLock->index().id() << "', run id '" << size_t(&runId)
        << "': " << res.errorNumber() << " " << res.errorMessage();
  }
}

IResearchDataStore::IResearchDataStore(
    ArangodServer& server, [[maybe_unused]] LogicalCollection& collection)
    : _asyncFeature(&server.getFeature<IResearchFeature>()),
      // mark as data store not initialized
      _asyncSelf(std::make_shared<AsyncLinkHandle>(nullptr)),
      _maintenanceState(std::make_shared<MaintenanceState>()) {
  // initialize transaction callback
#ifdef USE_ENTERPRISE
  if (!collection.isAStub() && _asyncFeature->columnsCacheOnlyLeaders()) {
    auto& ci = server.getFeature<ClusterFeature>().clusterInfo();
    auto r = ci.getShardLeadership(ServerState::instance()->getId(),
                                   collection.name());
    _useSearchCache = r == ClusterInfo::ShardLeadership::kLeader;
  }
#endif
  _beforeCommitCallback = [this](TransactionState& state) {
    auto prev = state.cookie(this);  // get existing cookie
    if (!prev) {
      return;
    }
    // TODO FIXME find a better way to look up a ViewState
    auto& ctx = basics::downCast<IResearchTrxState>(*prev);
    TRI_ASSERT(ctx._removals != nullptr);
    if (!ctx._removals->empty()) {
      ctx._ctx.Remove(std::move(ctx._removals));
    }
    ctx._removals.reset();
    ctx._ctx.RegisterFlush();
  };
  _afterCommitCallback = makeAfterCommitCallback<false>(this);
}

IResearchDataStore::~IResearchDataStore() {
  if (isOutOfSync()) {
    TRI_ASSERT(_asyncFeature != nullptr);
    // count down the number of out of sync links
    _asyncFeature->untrackOutOfSyncLink();
  }
  // if triggered  - no unload was called prior to deleting index object
  TRI_ASSERT(!_dataStore);
}

IResearchDataStore::Snapshot IResearchDataStore::snapshot() const {
  auto linkLock = _asyncSelf->lock();
  // '_dataStore' can be asynchronously modified
  if (!linkLock) {
    LOG_TOPIC("f42dc", WARN, TOPIC)
        << "failed to lock ArangoSearch index '" << index().id()
        << "' while retrieving snapshot from it";
    return {};  // return an empty reader
  }
  if (failQueriesOnOutOfSync() && linkLock->isOutOfSync()) {
    // link has failed, we cannot use it for querying
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC,
        absl::StrCat("ArangoSearch index '", linkLock->index().id().id(),
                     "' is out of sync and needs to be recreated"));
  }

  auto reader = IResearchDataStore::reader(linkLock);
  return {std::move(linkLock), std::move(reader)};
}

IResearchDataStore::DataSnapshotPtr IResearchDataStore::reader(
    LinkLock const& linkLock) {
  TRI_ASSERT(linkLock);
  TRI_ASSERT(linkLock->_dataStore);
  auto snapshot = linkLock->_dataStore.loadSnapshot();
  TRI_ASSERT(snapshot);
  return snapshot;
}

void IResearchDataStore::scheduleCommit(std::chrono::milliseconds delay) {
  CommitTask task;
  task.asyncLink = _asyncSelf;
  task.async = _asyncFeature;
  task.id = index().id();
  task.state = _maintenanceState;

  _maintenanceState->pendingCommits.fetch_add(1, std::memory_order_release);
  task.schedule(delay);
}

void IResearchDataStore::scheduleConsolidation(
    std::chrono::milliseconds delay) {
  ConsolidationTask task;
  task.asyncLink = _asyncSelf;
  task.async = _asyncFeature;
  task.id = index().id();
  task.state = _maintenanceState;
  task.progress = [link = _asyncSelf.get()] { return !link->empty(); };

  _maintenanceState->pendingConsolidations.fetch_add(1,
                                                     std::memory_order_release);
  task.schedule(delay);
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
IResearchDataStore::UnsafeOpResult IResearchDataStore::cleanupUnsafe() {
  auto begin = std::chrono::steady_clock::now();
  auto result = cleanupUnsafeImpl();
  uint64_t timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - begin)
                        .count();
  if (bool ok = result.ok(); ok && _avgCleanupTimeMs != nullptr) {
    _avgCleanupTimeMs->store(computeAvg(_cleanupTimeNum, timeMs),
                             std::memory_order_relaxed);
  } else if (!ok && _numFailedCleanups != nullptr) {
    _numFailedCleanups->fetch_add(1, std::memory_order_relaxed);
  }
  return {result, timeMs};
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchDataStore::cleanupUnsafeImpl() {
  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->lock() is valid

  try {
    irs::directory_utils::RemoveAllUnreferenced(*(_dataStore._directory));
  } catch (std::exception const& e) {
    return {
        TRI_ERROR_INTERNAL,
        absl::StrCat("caught exception while cleaning up ArangoSearch index '",
                     index().id().id(), "': ", e.what())};
  } catch (...) {
    return {
        TRI_ERROR_INTERNAL,
        absl::StrCat("caught exception while cleaning up ArangoSearch index '",
                     index().id().id(), "'")};
  }
  return {};
}

Result IResearchDataStore::commit(bool wait /*= true*/) {
  auto linkLock = _asyncSelf->lock();  // '_dataStore' can be async modified
  if (!linkLock) {
    // the current link is no longer valid (checked after ReadLock acquisition)
    return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
            absl::StrCat("failed to lock ArangoSearch index '",
                         index().id().id(), "' while committing it")};
  }
  return commit(linkLock, wait);
}

Result IResearchDataStore::commit(LinkLock& linkLock, bool wait) {
  TRI_ASSERT(linkLock);
  TRI_ASSERT(linkLock->_dataStore);

  // must be valid if _asyncSelf->lock() is valid
  CommitResult code = CommitResult::UNDEFINED;
  auto result = linkLock->commitUnsafe(wait, nullptr, code).result;
  size_t commitMsec = 0;
  size_t cleanupStep = 0;
  {
    READ_LOCKER(metaLock, linkLock->_dataStore._mutex);
    // '_meta' can be asynchronously modified
    commitMsec = linkLock->_dataStore._meta._commitIntervalMsec;
    cleanupStep = linkLock->_dataStore._meta._cleanupIntervalStep;
  }
  // If auto commit is disabled,
  // we want to manually trigger the cleanup for the consistent API
  if (commitMsec == 0 && cleanupStep != 0 &&
      ++linkLock->_cleanupIntervalCount >= cleanupStep) {
    linkLock->_cleanupIntervalCount = 0;
    std::ignore = linkLock->cleanupUnsafe();
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
IResearchDataStore::UnsafeOpResult IResearchDataStore::commitUnsafe(
    bool wait, irs::ProgressReportCallback const& progress,
    CommitResult& code) {
  auto begin = std::chrono::steady_clock::now();
  auto result = commitUnsafeImpl(wait, progress, code);
  uint64_t timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - begin)
                        .count();

  TRI_IF_FAILURE("ArangoSearch::FailOnCommit") {
    // intentionally mark the commit as failed
    result.reset(TRI_ERROR_DEBUG);
  }

  if (result.fail() && setOutOfSync()) {
    try {
      markOutOfSyncUnsafe();
    } catch (std::exception const& e) {
      // We couldn't persist the outOfSync flag,
      // but we can't mark the data store as "not outOfSync" again.
      // Not much we can do except logging.
      LOG_TOPIC("211d2", WARN, TOPIC)
          << "failed to store 'outOfSync' flag for ArangoSearch index '"
          << index().id() << "': " << e.what();
    }
  }

  if (bool ok = result.ok(); !ok && _numFailedCommits != nullptr) {
    _numFailedCommits->fetch_add(1, std::memory_order_relaxed);
  } else if (ok && code == CommitResult::DONE && _avgCommitTimeMs != nullptr) {
    _avgCommitTimeMs->store(computeAvg(_commitTimeNum, timeMs),
                            std::memory_order_relaxed);
  }
  return {result, timeMs};
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchDataStore::commitUnsafeImpl(
    bool wait, irs::ProgressReportCallback const& progress,
    CommitResult& code) {
  code = CommitResult::NO_CHANGES;
  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  try {
    std::unique_lock commitLock{_commitMutex, std::try_to_lock};
    if (!commitLock.owns_lock()) {
      if (!wait) {
        LOG_TOPIC("37bcc", TRACE, iresearch::TOPIC)
            << "Commit for ArangoSearch index '" << index().id()
            << "' is already in progress, skipping";

        code = CommitResult::IN_PROGRESS;
        return {};
      }

      LOG_TOPIC("37bca", TRACE, iresearch::TOPIC)
          << "Commit for ArangoSearch index '" << index().id()
          << "' is already in progress, waiting";

      commitLock.lock();
    }

#ifdef USE_ENTERPRISE
    bool const reopenColumnstore = [&] {
      if (!_asyncFeature->columnsCacheOnlyLeaders()) {
        return false;
      }
      auto& collection = index().collection();
      auto& ci = collection.vocbase()
                     .server()
                     .getFeature<ClusterFeature>()
                     .clusterInfo();
      auto r = ci.getShardLeadership(ServerState::instance()->getId(),
                                     collection.name());
      if (r == ClusterInfo::ShardLeadership::kUnclear) {
        return false;
      }
      bool curr = r == ClusterInfo::ShardLeadership::kLeader;
      bool prev = _useSearchCache.exchange(curr, std::memory_order_relaxed);
      return prev != curr;
    }();
#endif
    auto engineSnapshot = _engine->currentSnapshot();
    TRI_IF_FAILURE("ArangoSearch::DisableMoveTickInCommit") { return {}; }
    if (ADB_UNLIKELY(!engineSnapshot)) {
      return {TRI_ERROR_INTERNAL,
              absl::StrCat("Failed to get engine snapshot while committing "
                           "ArangoSearch index '",
                           index().id().id(), "'")};
    }
    auto const beforeCommit = engineSnapshot->tick();
    TRI_ASSERT(_lastCommittedTick <= beforeCommit);
    absl::Cleanup commitGuard = [&, last = _lastCommittedTick]() noexcept {
      _lastCommittedTick = last;
    };
    bool const wereChanges = _dataStore._writer->Commit({
        .tick = _isCreation ? irs::writer_limits::kMaxTick : beforeCommit,
        .progress = progress,
#ifdef USE_ENTERPRISE
        .reopen_columnstore = reopenColumnstore,
#endif
    });
    // get new reader
    auto reader = _dataStore._writer->GetSnapshot();
    TRI_ASSERT(reader != nullptr);
    std::move(commitGuard).Cancel();
    auto& subscription =
        basics::downCast<IResearchFlushSubscription>(*_flushSubscription);
    if (!wereChanges) {
      LOG_TOPIC("7e319", TRACE, TOPIC)
          << "Commit for ArangoSearch index '" << index().id()
          << "' is no changes, tick " << beforeCommit << "'";
      _lastCommittedTick = beforeCommit;
      // no changes, can release the latest tick before commit
      subscription.tick(_lastCommittedTick);
      // TODO(MBkkt) make_shared can throw!
      _dataStore.storeSnapshot(std::make_shared<DataSnapshot>(
          std::move(reader), std::move(engineSnapshot)));
      return {};
    }
    TRI_ASSERT(_isCreation || _lastCommittedTick == beforeCommit);
    code = CommitResult::DONE;

    // update reader
    TRI_ASSERT(_dataStore.loadSnapshot()->_reader != reader);
    auto const readerSize = reader->size();
    auto const docsCount = reader->docs_count();
    auto const liveDocsCount = reader->live_docs_count();

    _dataStore.storeSnapshot(std::make_shared<DataSnapshot>(
        std::move(reader), std::move(engineSnapshot)));

    // update last committed tick
    subscription.tick(_lastCommittedTick);

    // update stats
    updateStatsUnsafe();

    invalidateQueryCache(&index().collection().vocbase());

    LOG_TOPIC("7e328", DEBUG, iresearch::TOPIC)
        << "successful sync of ArangoSearch index '" << index().id()
        << "', segments '" << readerSize << "', docs count '" << docsCount
        << "', live docs count '" << liveDocsCount << "', last operation tick '"
        << _lastCommittedTick << "'";
  } catch (basics::Exception const& e) {
    return {
        e.code(),
        absl::StrCat("caught exception while committing ArangoSearch index '",
                     index().id().id(), "': ", e.message())};
  } catch (std::exception const& e) {
    return {
        TRI_ERROR_INTERNAL,
        absl::StrCat("caught exception while committing ArangoSearch index '",
                     index().id().id(), "': ", e.what())};
  } catch (...) {
    return {
        TRI_ERROR_INTERNAL,
        absl::StrCat("caught exception while committing ArangoSearch index '",
                     index().id().id(), "'")};
  }
  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
IResearchDataStore::UnsafeOpResult IResearchDataStore::consolidateUnsafe(
    IResearchDataStoreMeta::ConsolidationPolicy const& policy,
    irs::MergeWriter::FlushProgress const& progress, bool& emptyConsolidation) {
  auto begin = std::chrono::steady_clock::now();
  auto result = consolidateUnsafeImpl(policy, progress, emptyConsolidation);
  uint64_t timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - begin)
                        .count();
  if (bool ok = result.ok(); ok && _avgConsolidationTimeMs != nullptr) {
    _avgConsolidationTimeMs->store(computeAvg(_consolidationTimeNum, timeMs),
                                   std::memory_order_relaxed);
  } else if (!ok && _numFailedConsolidations != nullptr) {
    _numFailedConsolidations->fetch_add(1, std::memory_order_relaxed);
  }
  return {result, timeMs};
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchDataStore::consolidateUnsafeImpl(
    IResearchDataStoreMeta::ConsolidationPolicy const& policy,
    irs::MergeWriter::FlushProgress const& progress, bool& emptyConsolidation) {
  emptyConsolidation = false;  // TODO Why?

  if (!policy.policy()) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat(
            "unset consolidation policy while executing consolidation policy '",
            policy.properties().toString(), "' on ArangoSearch index '",
            index().id().id(), "'")};
  }

  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  try {
    auto const res =
        _dataStore._writer->Consolidate(policy.policy(), nullptr, progress);
    if (!res) {
      return {
          TRI_ERROR_INTERNAL,
          absl::StrCat("failure while executing consolidation policy '",
                       policy.properties().toString(),
                       "' on ArangoSearch index '", index().id().id(), "'")};
    }

    emptyConsolidation = (res.size == 0);
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            absl::StrCat(
                "caught exception while executing consolidation policy '",
                policy.properties().toString(), "' on ArangoSearch index '",
                index().id().id(), "': ", e.what())};
  } catch (...) {
    return {
        TRI_ERROR_INTERNAL,
        absl::StrCat("caught exception while executing consolidation policy '",
                     policy.properties().toString(),
                     "' on ArangoSearch index '", index().id().id(), "'")};
  }
  return {};
}

void IResearchDataStore::shutdownDataStore() noexcept {
  // the data-store is being deallocated, link use is no longer valid
  _asyncSelf->reset();         // wait for all the view users to finish
  _flushSubscription.reset();  // reset together with _asyncSelf
  try {
    if (_dataStore) {
      removeMetrics();  // TODO(MBkkt) Should be noexcept?
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("bad00", ERR, TOPIC)
        << "caught exception while removeMetrics arangosearch data store '"
        << index().id().id() << "': " << e.what();
  } catch (...) {
    LOG_TOPIC("bad01", ERR, TOPIC)
        << "caught something while removeMetrics arangosearch data store '"
        << index().id().id() << "'";
  }
  _recoveryRemoves.reset();
  _recoveryTrx.Abort();
  _dataStore.resetDataStore();
}

Result IResearchDataStore::deleteDataStore() noexcept {
  shutdownDataStore();
  std::error_code error;
  std::filesystem::remove_all(_dataStore._path, error);
  // remove persisted data store directory if present
  if (error) {
    return {
        TRI_ERROR_INTERNAL,
        absl::StrCat("failed to remove ArangoSearch index '", index().id().id(),
                     "' with error code '", error.message(), "'")};
  }
  return {};
}

bool IResearchDataStore::failQueriesOnOutOfSync() const noexcept {
  return _asyncFeature->failQueriesOnOutOfSync();
}

bool IResearchDataStore::setOutOfSync() noexcept {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto error = _error.load(std::memory_order_relaxed);
  return error == DataStoreError::kNoError &&
         _error.compare_exchange_strong(error, DataStoreError::kOutOfSync,
                                        std::memory_order_relaxed,
                                        std::memory_order_relaxed);
}

void IResearchDataStore::markOutOfSyncUnsafe() {
  // Only once per link:
  // 1. increase metric for number of OutOfSync links
  // 2. persist OutOfSync flag in RocksDB
  // note: if this fails, it will throw an exception
  TRI_ASSERT(_asyncFeature != nullptr);
  TRI_ASSERT(_engine != nullptr);
  _asyncFeature->trackOutOfSyncLink();
  _engine->changeCollection(index().collection().vocbase(),
                            index().collection());
}

bool IResearchDataStore::isOutOfSync() const noexcept {
  // The OutOfSync flag is expected to be set either
  // during the recovery phase, or when a commit goes wrong
  return _error.load(std::memory_order_relaxed) == DataStoreError::kOutOfSync;
}

void IResearchDataStore::initAsyncSelf() {
  _asyncSelf->reset();
  _asyncSelf = std::make_shared<AsyncLinkHandle>(this);
}

irs::IndexWriterOptions IResearchDataStore::getWriterOptions(
    irs::IndexReaderOptions const& readerOptions, uint32_t version, bool sorted,
    bool nested,
    std::span<const IResearchViewStoredValues::StoredColumn> storedColumns,
    irs::type_info::type_id primarySortCompression) {
  irs::IndexWriterOptions options;
  options.reader_options = readerOptions;
  // Set 256MB limit during recovery. Actual "operational" limit will be set
  // later when this link will be added to the view.
  options.segment_memory_max = 256 * (size_t{1} << 20);
  // Do not lock index, ArangoDB has its own lock.
  options.lock_repository = false;
  // Set comparator if requested.
  options.comparator = sorted ? getComparator() : nullptr;
  // Set index features.
  if (LinkVersion{version} < LinkVersion::MAX) {
    options.features = getIndexFeatures<irs::Norm>();
  } else {
    options.features = getIndexFeatures<irs::Norm2>();
  }
  // initialize commit callback
  options.meta_payload_provider = [this](uint64_t tick, irs::bstring& out) {
    if (_isCreation) {
      tick = engine()->currentTick();
    }
    // convert version to BE
    auto const v = irs::numeric_utils::hton32(
        static_cast<uint32_t>(SegmentPayloadVersion::TwoStageTick));
    // compute and update tick
    _lastCommittedTick = std::max(_lastCommittedTick, tick);
    // convert tick to BE
    tick = irs::numeric_utils::hton64(_lastCommittedTick);
    // write low tick
    out.append(reinterpret_cast<irs::byte_type const*>(&tick), sizeof(tick));
    // write version
    out.append(reinterpret_cast<irs::byte_type const*>(&v), sizeof(v));
    // write high tick TODO(MBkkt) unnecessary, can be changed in new version
    out.append(reinterpret_cast<irs::byte_type const*>(&tick), sizeof(tick));
    return true;
  };

  // as meta is still not filled at this moment
  // we need to store all compression mapping there
  // as values provided may be temporary
  std::map<std::string, irs::type_info::type_id, std::less<>> compressionMap;
  for (auto const& c : storedColumns) {
    if (ADB_LIKELY(c.compression != nullptr)) {
      compressionMap.emplace(c.name, c.compression);
    } else {
      TRI_ASSERT(false);
      compressionMap.emplace(c.name, getDefaultCompression());
    }
  }
  // setup columnstore compression/encryption if requested by storage engine
  auto const encrypt =
      (nullptr != _dataStore._directory->attributes().encryption());
  options.column_info =
      [nested, encrypt, compressionMap = std::move(compressionMap),
       primarySortCompression](std::string_view name) -> irs::ColumnInfo {
    if (irs::IsNull(name)) {
      return {.compression = primarySortCompression(),
              .options = {},
              .encryption = encrypt,
              .track_prev_doc = false};
    }
    if (auto compress = compressionMap.find(name);
        compress != compressionMap.end()) {
      // do not waste resources to encrypt primary key column
      return {.compression = compress->second(),
              .options = {},
              .encryption = encrypt && (DocumentPrimaryKey::PK() != name),
              .track_prev_doc = kludge::needTrackPrevDoc(name, nested)};
    }
    return {.compression = getDefaultCompression()(),
            .options = {},
            .encryption = encrypt && (DocumentPrimaryKey::PK() != name),
            .track_prev_doc = kludge::needTrackPrevDoc(name, nested)};
  };
  return options;
}

Result IResearchDataStore::initDataStore(
    bool& pathExists, InitCallback const& initCallback, uint32_t version,
    bool sorted, bool nested,
    std::span<const IResearchViewStoredValues::StoredColumn> storedColumns,
    irs::type_info::type_id primarySortCompression,
    irs::IndexReaderOptions const& readerOptions) {
  // The data-store is being deallocated, link use is no longer valid
  // (wait for all the view users to finish)
  _asyncSelf->reset();
  // Reset together with '_asyncSelf'
  _flushSubscription.reset();
  _hasNestedFields = nested;
  auto& server = index().collection().vocbase().server();
  if (!server.hasFeature<DatabasePathFeature>()) {
    return {TRI_ERROR_INTERNAL,
            absl::StrCat("Failed to find feature 'DatabasePath' while "
                         "initializing data store '",
                         index().id().id(), "'")};
  }
  if (!server.hasFeature<FlushFeature>()) {
    return {TRI_ERROR_INTERNAL,
            absl::StrCat("Failed to find feature 'FlushFeature' while "
                         "initializing data store '",
                         index().id().id(), "'")};
  }

  auto& dbPathFeature = server.getFeature<DatabasePathFeature>();

  auto const formatId = getFormat(LinkVersion{version});
  auto format = irs::formats::get(formatId);
  if (!format) {
    return {TRI_ERROR_INTERNAL,
            absl::StrCat("Failed to get data store codec '", formatId,
                         "' while initializing ArangoSearch index '",
                         index().id().id(), "'")};
  }

  _engine = &server.getFeature<EngineSelectorFeature>().engine();

  _dataStore._path = getPersistedPath(dbPathFeature, *this);

  // Must manually ensure that the data store directory exists
  // (since not using a lockfile)
  std::error_code error;
  pathExists = std::filesystem::exists(_dataStore._path, error);
  auto createResult = [&](std::string_view what) -> Result {
    return {TRI_ERROR_CANNOT_CREATE_DIRECTORY,
            absl::StrCat(what, " failed for data store directory with path '",
                         _dataStore._path.string(),
                         "' while initializing ArangoSearch index '",
                         index().id().id(), "' with error '", error.message(),
                         "'")};
  };
  if (error) {
    pathExists = true;  // we don't want to remove directory in unknown state
    return createResult("Exists");
  }
  if (!pathExists) {
    std::filesystem::create_directories(_dataStore._path, error);
    if (error) {
      return createResult("Create");
    }
  }

  _dataStore._directory = std::make_unique<irs::MMapDirectory>(
      _dataStore._path,
      initCallback ? initCallback() : irs::directory_attributes{});

  switch (_engine->recoveryState()) {
    case RecoveryState::BEFORE:  // Link is being opened before recovery
      [[fallthrough]];
    case RecoveryState::DONE: {  // Link is being created after recovery
      // Will be adjusted in post-recovery callback
      _dataStore._recoveryTickHigh = _dataStore._recoveryTickLow =
          _engine->recoveryTick();
    } break;
    case RecoveryState::IN_PROGRESS: {  // Link is being created during recovery
      _dataStore._recoveryTickHigh = _dataStore._recoveryTickLow =
          _engine->releasedTick();
    } break;
  }
  _lastCommittedTick = _dataStore._recoveryTickLow;

  auto const openMode =
      pathExists ? (irs::OM_CREATE | irs::OM_APPEND) : irs::OM_CREATE;
  auto const options = getWriterOptions(readerOptions, version, sorted, nested,
                                        storedColumns, primarySortCompression);
  _dataStore._writer = irs::IndexWriter::Make(
      *(_dataStore._directory), std::move(format), openMode, options);
  IRS_ASSERT(_dataStore._writer);

  if (!pathExists) {
    // Initialize empty store
    _dataStore._writer->Commit();
  }

  auto reader = _dataStore._writer->GetSnapshot();
  TRI_ASSERT(reader);

  if (pathExists) {  // Read payload from the existing ArangoSearch index
    if (!readTick(irs::GetPayload(reader.Meta().index_meta),
                  _dataStore._recoveryTickLow, _dataStore._recoveryTickHigh)) {
      return {
          TRI_ERROR_INTERNAL,
          absl::StrCat("Failed to get last committed tick while initializing "
                       "ArangoSearch index '",
                       index().id().id(), "'")};
    }
    _lastCommittedTick = _dataStore._recoveryTickLow;
  }

  LOG_TOPIC("7e028", DEBUG, TOPIC)
      << toString(reader, !pathExists, index().id(),
                  _dataStore._recoveryTickLow, _dataStore._recoveryTickHigh);

  auto engineSnapshot = _engine->currentSnapshot();
  if (!engineSnapshot) {
    return {TRI_ERROR_INTERNAL,
            absl::StrCat("Failed to get engine snapshot while initializing "
                         "ArangoSearch index '",
                         index().id().id(), "'")};
  }
  _dataStore.storeSnapshot(std::make_shared<DataSnapshot>(
      std::move(reader), std::move(engineSnapshot)));

  _flushSubscription = std::make_shared<IResearchFlushSubscription>(
      _dataStore._recoveryTickLow,
      absl::StrCat("flush subscription for ArangoSearch index '",
                   index().id().id(), "'"));

  // Reset data store meta, will be updated at runtime via properties(...)
  _dataStore._meta._cleanupIntervalStep = 0;        // 0 == disable
  _dataStore._meta._commitIntervalMsec = 0;         // 0 == disable
  _dataStore._meta._consolidationIntervalMsec = 0;  // 0 == disable
  _dataStore._meta._consolidationPolicy =
      IResearchDataStoreMeta::ConsolidationPolicy();  // disable
  _dataStore._meta._writebufferActive = options.segment_count_max;
  _dataStore._meta._writebufferIdle = options.segment_pool_size;
  _dataStore._meta._writebufferSizeMax = options.segment_memory_max;

  // Create a new 'self' (previous was reset during unload() above)
  TRI_ASSERT(_asyncSelf->empty());
  _asyncSelf = std::make_shared<AsyncLinkHandle>(this);

  // Register metrics before starting any background threads
  insertMetrics();
  updateStatsUnsafe();

  // ...........................................................................
  // Set up in-recovery insertion hooks
  // ...........................................................................

  if (!server.hasFeature<DatabaseFeature>()) {
    return {};  // nothing more to do
  }
  auto& dbFeature = server.getFeature<DatabaseFeature>();

  if (_engine->inRecovery()) {
    _recoveryRemoves = makePrimaryKeysFilter(_hasNestedFields);
    _recoveryTrx = _dataStore._writer->GetBatch();
  }

  return dbFeature.registerPostRecoveryCallback(  // register callback
      [asyncSelf = _asyncSelf, asyncFeature = _asyncFeature,
       pathExists]() -> Result {
        auto linkLock = asyncSelf->lock();
        // ensure link does not get deallocated before callback finishes

        if (!linkLock) {
          // link no longer in recovery state, i.e. during recovery
          // it was created and later dropped
          return {};
        }

        auto& dataStore = linkLock->_dataStore;
        auto& index = linkLock->index();

        // recovery finished
        TRI_ASSERT(!linkLock->_engine->inRecovery());

        const auto recoveryTick = linkLock->_engine->recoveryTick();

        bool isOutOfSync = linkLock->isOutOfSync();
        if (isOutOfSync) {
          LOG_TOPIC("2721a", WARN, iresearch::TOPIC)
              << "Marking ArangoSearch index '" << index.id().id()
              << "' as out of sync, consider to drop and re-create the index "
                 "in order to synchronize it";

        } else if (dataStore._recoveryTickHigh > recoveryTick) {
          LOG_TOPIC("5b59f", WARN, iresearch::TOPIC)
              << "ArangoSearch index: " << index.id()
              << " is recovered at tick " << dataStore._recoveryTickHigh
              << " greater than storage engine tick " << recoveryTick
              << ", it seems WAL tail was lost and index is out of sync with "
                 "the underlying collection: "
              << linkLock->index().collection().name()
              << ", consider to re-create the index in order to synchronize it";

          isOutOfSync = linkLock->setOutOfSync();
          TRI_ASSERT(isOutOfSync);
        }

        if (isOutOfSync) {
          // note: if this fails,
          // it will throw an exception and abort the recovery & startup.
          linkLock->markOutOfSyncUnsafe();
          if (asyncFeature->failQueriesOnOutOfSync()) {
            // we cannot return an error from here as this would abort the
            // entire recovery and fail the startup.
            return {};
          }
        }

        // Make last batch Commit if necessary
        if (linkLock->_recoveryRemoves != nullptr) {
          linkLock->recoveryCommit(recoveryTick);
        }

        // register flush subscription
        if (pathExists) {
          linkLock->finishCreation();
        }

        auto progress = [id = index.id(), asyncFeature](std::string_view phase,
                                                        size_t current,
                                                        size_t total) {
          // forward progress reporting to asyncFeature
          asyncFeature->reportRecoveryProgress(id, phase, current, total);
        };

        LOG_TOPIC("5b59c", TRACE, iresearch::TOPIC)
            << "Start sync for ArangoSearch index: " << index.id();

        auto code = CommitResult::UNDEFINED;
        auto [res, timeMs] = linkLock->commitUnsafe(true, progress, code);

        LOG_TOPIC("0e0ca", TRACE, iresearch::TOPIC)
            << "Finish sync for ArangoSearch index:" << index.id();

        // setup asynchronous tasks for commit, cleanup if enabled
        if (dataStore._meta._commitIntervalMsec) {
          linkLock->scheduleCommit(0ms);
        }

        // setup asynchronous tasks for consolidation if enabled
        if (dataStore._meta._consolidationIntervalMsec) {
          linkLock->scheduleConsolidation(0ms);
        }

        return res;
      });
}

Result IResearchDataStore::properties(IResearchDataStoreMeta const& meta) {
  auto linkLock = _asyncSelf->lock();
  // '_dataStore' can be asynchronously modified
  if (!linkLock) {
    // the current link is no longer valid (checked after ReadLock
    // acquisition)
    return {
        TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
        absl::StrCat("failed to lock ArangoSearch index '", index().id().id(),
                     "' while modifying properties of it")};
  }
  properties(std::move(linkLock), meta);
  return {};
}

void IResearchDataStore::properties(LinkLock linkLock,
                                    IResearchDataStoreMeta const& meta) {
  TRI_ASSERT(linkLock);
  TRI_ASSERT(linkLock->_dataStore);
  // must be valid if _asyncSelf->lock() is valid
  {
    WRITE_LOCKER(writeLock, linkLock->_dataStore._mutex);
    // '_meta' can be asynchronously modified
    linkLock->_dataStore._meta.storeFull(meta);
  }

  if (linkLock->_engine->recoveryState() == RecoveryState::DONE) {
    if (meta._commitIntervalMsec) {
      linkLock->scheduleCommit(
          std::chrono::milliseconds(meta._commitIntervalMsec));
    }

    if (meta._consolidationIntervalMsec && meta._consolidationPolicy.policy()) {
      linkLock->scheduleConsolidation(
          std::chrono::milliseconds(meta._consolidationIntervalMsec));
    }
  }

  irs::SegmentOptions const properties{
      .segment_count_max = meta._writebufferActive,
      .segment_memory_max = meta._writebufferSizeMax};

  static_assert(noexcept(linkLock->_dataStore._writer->Options(properties)));
  linkLock->_dataStore._writer->Options(properties);
}

IResearchTrxState* IResearchDataStore::getContext(TransactionState& state) {
  void const* key = this;
  auto* context = basics::downCast<IResearchTrxState>(state.cookie(key));
  if (ADB_LIKELY(context)) {
    return context;
  }
  auto linkLock = _asyncSelf->lock();
  if (ADB_UNLIKELY(!linkLock)) {
    return nullptr;
  }
  TRI_ASSERT(_dataStore);
  auto ptr = std::make_unique<IResearchTrxState>(
      std::move(linkLock), *_dataStore._writer, _hasNestedFields);
  context = ptr.get();
  TRI_ASSERT(context);
  state.cookie(key, std::move(ptr));
  state.addBeforeCommitCallback(&_beforeCommitCallback);
  state.addAfterCommitCallback(&_afterCommitCallback);
  return context;
}

Result IResearchDataStore::remove(transaction::Methods& trx,
                                  LocalDocumentId documentId) {
  TRI_ASSERT(trx.state());
  auto& state = *trx.state();
  TRI_ASSERT(!state.hasHint(transaction::Hints::Hint::INDEX_CREATION));
  if (ADB_UNLIKELY(failQueriesOnOutOfSync() && isOutOfSync())) {
    return {};
  }
  auto* ctx = getContext(state);
  if (ADB_UNLIKELY(!ctx)) {
    return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
            MakeMessage("While removing document failed to lock", index(),
                        state, documentId)};
  }
  ctx->remove(documentId);
  return {};
}

void IResearchDataStore::recoveryRemove(LocalDocumentId documentId) {
  TRI_ASSERT(!isOutOfSync());
  TRI_ASSERT(_recoveryRemoves);
  _recoveryRemoves->emplace(documentId);
}

bool IResearchDataStore::exists(LocalDocumentId documentId) const {
  auto snapshot = this->snapshot();
  auto& reader = snapshot.getDirectoryReader();
  if (ADB_UNLIKELY(!reader)) {
    return false;
  }

  auto const encoded = DocumentPrimaryKey::encode(documentId);

  auto const pk =
      irs::numeric_utils::numeric_traits<LocalDocumentId::BaseType>::raw_ref(
          encoded);

  for (auto const& segment : reader) {
    auto const* pkField = segment.field(DocumentPrimaryKey::PK());

    if (ADB_UNLIKELY(!pkField)) {
      continue;
    }
    auto doc = irs::doc_limits::invalid();
    pkField->read_documents(pk, {&doc, 1});
    if (irs::doc_limits::valid(doc)) {
      return true;
    }
  }

  return false;
}

template<typename FieldIteratorType, typename MetaType>
Result IResearchDataStore::insert(transaction::Methods& trx,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc, MetaType const& meta) {
  TRI_ASSERT(trx.state());
  auto& state = *trx.state();
  if (ADB_UNLIKELY(failQueriesOnOutOfSync() && isOutOfSync())) {
    return {};
  }

  auto insertImpl = [&, this](irs::IndexWriter::Transaction& ctx) -> Result {
    auto messege = [&](auto&&... args) {
      return MakeMessage("While inserting document caught exception", index(),
                         state, documentId,
                         std::forward<decltype(args)>(args)...);
    };
    try {
      return insertDocument<FieldIteratorType>(*this, ctx, doc, documentId,
                                               meta);
    } catch (basics::Exception const& e) {
      return {e.code(), messege(", ", e.what())};
    } catch (std::exception const& e) {
      return {TRI_ERROR_INTERNAL, messege(", ", e.what())};
    } catch (...) {
      return {TRI_ERROR_INTERNAL, messege()};
    }
  };

  if (ADB_UNLIKELY(state.hasHint(transaction::Hints::Hint::INDEX_CREATION))) {
    auto linkLock = _asyncSelf->lock();
    if (ADB_UNLIKELY(!linkLock)) {
      return {TRI_ERROR_INTERNAL};
    }
    auto ctx = _dataStore._writer->GetBatch();
    if (auto r = insertImpl(ctx); ADB_UNLIKELY(!r.ok())) {
      ctx.Abort();
      return r;
    }
    TRI_IF_FAILURE("ArangoSearch::MisreportCreationInsertAsFailed") {
      return {TRI_ERROR_DEBUG};
    }
    return {};
  }

  TRI_IF_FAILURE("ArangoSearch::BlockInsertsWithoutIndexCreationHint") {
    return {TRI_ERROR_DEBUG};
  }

  auto* ctx = getContext(state);
  if (ADB_UNLIKELY(!ctx)) {
    return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
            MakeMessage("While inserting document failed to lock", index(),
                        state, documentId)};
  }
  return insertImpl(ctx->_ctx);
}

template<typename FieldIteratorType, typename MetaType>
void IResearchDataStore::recoveryInsert(uint64_t recoveryTick,
                                        LocalDocumentId documentId,
                                        velocypack::Slice doc,
                                        MetaType const& meta) {
  TRI_ASSERT(recoveryTickLow() < recoveryTick);
  TRI_ASSERT(!isOutOfSync());
  TRI_ASSERT(_recoveryRemoves);
  try {
    std::ignore = insertDocument<FieldIteratorType>(*this, _recoveryTrx, doc,
                                                    documentId, meta);
  } catch (std::bad_alloc const&) {
    throw;
  } catch (...) {
  }
  if (!_recoveryTrx.FlushRequired()) {
    return;
  }
  if (!_recoveryRemoves->empty()) {
    _recoveryTrx.Remove(std::move(_recoveryRemoves));
    _recoveryRemoves = makePrimaryKeysFilter(_hasNestedFields);
  }
  _recoveryTrx.Commit(recoveryTick);
  // TODO(MBkkt) IndexWriter::Commit? Probably makes sense only if were removes
}

void IResearchDataStore::afterTruncate(TRI_voc_tick_t tick,
                                       transaction::Methods* trx) {
  // '_dataStore' can be asynchronously modified
  auto linkLock = _asyncSelf->lock();

  bool ok{false};
  irs::Finally computeMetrics = [&]() noexcept {
    // We don't measure time because we believe that it should tend to zero
    if (!ok && _numFailedCommits != nullptr) {
      _numFailedCommits->fetch_add(1, std::memory_order_relaxed);
    }
  };

  TRI_IF_FAILURE("ArangoSearchTruncateFailure") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (!linkLock) {
    // the current link is no longer valid (checked after ReadLock
    // acquisition)
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
        absl::StrCat("failed to lock ArangoSearch index '", index().id().id(),
                     "' while truncating it"));
  }

  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  if (trx != nullptr) {
    auto* key = this;

    auto& state = *(trx->state());
    // TODO FIXME find a better way to look up a ViewState
    auto* ctx = basics::downCast<IResearchTrxState>(state.cookie(key));
    if (ctx) {
      // throw away all pending operations as clear will overwrite them all
      // force active segment release to allow commit go and avoid deadlock in
      // clear
      state.cookie(key, nullptr);
    }
  } else if (_recoveryRemoves != nullptr) {  // TODO(MBkkt) should be assert?
    _recoveryRemoves->clear();
    _recoveryTrx.Abort();
  }

  std::lock_guard commitLock{_commitMutex};
  absl::Cleanup clearGuard = [&, last = _lastCommittedTick]() noexcept {
    _lastCommittedTick = last;
  };
  try {
    _dataStore._writer->Clear(tick);
    std::move(clearGuard).Cancel();
    // payload will not be called is index already empty
    _lastCommittedTick = std::max(tick, _lastCommittedTick);

    auto snapshot = _engine->currentSnapshot();
    if (ADB_UNLIKELY(!snapshot)) {
      // we reuse storage snapshot in this unlikely. Technically this is not
      // right as old storage snapshot is most likely outdated.
      // but index is empty so it makes no sense as we will not
      // materialize anything anyway.
      // get new reader
      snapshot = _dataStore.loadSnapshot()->_snapshot;
    }
    auto reader = _dataStore._writer->GetSnapshot();
    TRI_ASSERT(reader);

    // update reader
    _dataStore.storeSnapshot(
        std::make_shared<DataSnapshot>(std::move(reader), std::move(snapshot)));

    updateStatsUnsafe();

    auto& subscription =
        basics::downCast<IResearchFlushSubscription>(*_flushSubscription);
    subscription.tick(_lastCommittedTick);
    invalidateQueryCache(&index().collection().vocbase());
    ok = true;
  } catch (std::exception const& e) {
    LOG_TOPIC("a3c57", ERR, TOPIC)
        << "caught exception while truncating ArangoSearch index '"
        << index().id() << "': " << e.what();
    throw;
  } catch (...) {
    LOG_TOPIC("79a7d", WARN, TOPIC)
        << "caught exception while truncating ArangoSearch index '"
        << index().id() << "'";
    throw;
  }
}

IResearchDataStore::Stats IResearchDataStore::stats() const {
  auto linkLock = _asyncSelf->lock();
  if (!linkLock) {
    return {};
  }
  if (_metricStats) {
    return _metricStats->load();
  }
  return updateStatsUnsafe();
}

IResearchDataStore::Stats IResearchDataStore::updateStatsUnsafe() const {
  TRI_ASSERT(_dataStore);
  // copy of 'reader' is important to hold reference to the current snapshot
  auto reader = _dataStore.loadSnapshot()->_reader;
  if (!reader) {
    return {};
  }
  Stats stats;
  stats.numSegments = reader->size();
  stats.numDocs = reader->docs_count();
#ifdef USE_ENTERPRISE
  if (_hasNestedFields) {
    stats.numPrimaryDocs = getPrimaryDocsCount(reader);
  } else {
    stats.numPrimaryDocs = stats.numDocs;
  }
#else
  stats.numPrimaryDocs = stats.numDocs;
#endif
  stats.numLiveDocs = reader->live_docs_count();
  stats.numFiles = 1;  // +1 for segments file
  for (auto const& segment : reader->Meta().index_meta.segments) {
    auto const& meta = segment.meta;
    stats.indexSize += meta.byte_size;
    stats.numFiles += meta.files.size();
  }
  if (_metricStats) {
    _metricStats->store(stats);
  }
  return stats;
}

void IResearchDataStore::toVelocyPackStats(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());

  auto const stats = this->stats();

  builder.add("numDocs", VPackValue(stats.numDocs));
  builder.add("numPrimaryDocs", VPackValue(stats.numPrimaryDocs));
  builder.add("numLiveDocs", VPackValue(stats.numLiveDocs));
  builder.add("numSegments", VPackValue(stats.numSegments));
  builder.add("numFiles", VPackValue(stats.numFiles));
  builder.add("indexSize", VPackValue(stats.indexSize));
}

#ifdef ARANGODB_USE_GOOGLE_TESTS

std::tuple<uint64_t, uint64_t, uint64_t> IResearchDataStore::numFailed() const {
  return {_numFailedCommits ? _numFailedCommits->load(std::memory_order_relaxed)
                            : 0,
          _numFailedCleanups
              ? _numFailedCleanups->load(std::memory_order_relaxed)
              : 0,
          _numFailedConsolidations
              ? _numFailedConsolidations->load(std::memory_order_relaxed)
              : 0};
}

std::tuple<uint64_t, uint64_t, uint64_t> IResearchDataStore::avgTime() const {
  return {
      _avgCommitTimeMs ? _avgCommitTimeMs->load(std::memory_order_relaxed) : 0,
      _avgCleanupTimeMs ? _avgCleanupTimeMs->load(std::memory_order_relaxed)
                        : 0,
      _avgConsolidationTimeMs
          ? _avgConsolidationTimeMs->load(std::memory_order_relaxed)
          : 0};
}

#endif

void IResearchDataStore::recoveryCommit(uint64_t tick) {
  TRI_ASSERT(_recoveryRemoves);
  if (!_recoveryRemoves->empty()) {
    _recoveryTrx.Remove(std::move(_recoveryRemoves));
  }
  _recoveryRemoves.reset();
  _recoveryTrx.Commit(tick);
}

void IResearchDataStore::initClusterMetrics() const {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  if (kHasClusterMetrics.load(std::memory_order_relaxed)) {
    return;
  }
  if (kHasClusterMetrics.exchange(true)) {
    return;
  }
  using namespace metrics;
  auto& metric = index()
                     .collection()
                     .vocbase()
                     .server()
                     .getFeature<ClusterMetricsFeature>();

  auto batchToCoordinator = [](ClusterMetricsFeature::Metrics& metrics,
                               std::string_view name, velocypack::Slice labels,
                               velocypack::Slice value) {
    // TODO(MBkkt) remove std::string
    auto& v =
        metrics.values[{std::string{name}, std::string{labels.stringView()}}];
    std::get<uint64_t>(v) += value.getNumber<uint64_t>();
  };
  auto batchToPrometheus = [](std::string& result, std::string_view globals,
                              std::string_view name, std::string_view labels,
                              ClusterMetricsFeature::MetricValue const& value,
                              bool ensureWhitespace) {
    Metric::addMark(result, name, globals, labels);
    absl::StrAppend(&result, std::get<uint64_t>(value), "\n");
  };
  metric.add("arangodb_search_num_docs", batchToCoordinator, batchToPrometheus);
  metric.add("arangodb_search_num_live_docs", batchToCoordinator,
             batchToPrometheus);
  metric.add("arangodb_search_num_primary_docs", batchToCoordinator,
             batchToPrometheus);
  metric.add("arangodb_search_num_segments", batchToCoordinator,
             batchToPrometheus);
  metric.add("arangodb_search_num_files", batchToCoordinator,
             batchToPrometheus);
  metric.add("arangodb_search_index_size", batchToCoordinator,
             batchToPrometheus);
  auto gaugeToCoordinator = [](ClusterMetricsFeature::Metrics& metrics,
                               std::string_view name, velocypack::Slice labels,
                               velocypack::Slice value) {
    auto labelsStr = labels.stringView();
    auto end = labelsStr.find(",shard=\"");
    TRI_ASSERT(end != std::string_view::npos);
    labelsStr = labelsStr.substr(0, end);
    // TODO(MBkkt) remove std::string
    auto& v = metrics.values[{std::string{name}, std::string{labelsStr}}];
    std::get<uint64_t>(v) += value.getNumber<uint64_t>();
  };
  metric.add("arangodb_search_num_failed_commits", gaugeToCoordinator);
  metric.add("arangodb_search_num_failed_cleanups", gaugeToCoordinator);
  metric.add("arangodb_search_num_failed_consolidations", gaugeToCoordinator);
  metric.add("arangodb_search_commit_time", gaugeToCoordinator);
  metric.add("arangodb_search_cleanup_time", gaugeToCoordinator);
  metric.add("arangodb_search_consolidation_time", gaugeToCoordinator);
}

void IResearchDataStore::finishCreation() {
  std::lock_guard lock{_commitMutex};
  if (std::exchange(_isCreation, false)) {
    auto& flushFeature =
        index().collection().vocbase().server().getFeature<FlushFeature>();
    flushFeature.registerFlushSubscription(_flushSubscription);
    _afterCommitCallback = makeAfterCommitCallback<true>(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compute the data path to user for iresearch data store
///        get base path from DatabaseServerFeature (similar to MMFilesEngine)
///        the path is hardcoded to reside under:
///        <DatabasePath>/<IResearchLink::type()>-<link id>
///        similar to the data path calculation for collections
////////////////////////////////////////////////////////////////////////////////
std::filesystem::path getPersistedPath(DatabasePathFeature const& dbPathFeature,
                                       IResearchDataStore const& dataStore) {
  auto& i = dataStore.index();
  auto path = getPersistedPath(dbPathFeature, i.collection().vocbase());
  path /=
      absl::StrCat(StaticStrings::ViewArangoSearchType, "-",
                   // has to be 'id' since this can be a per-shard collection
                   i.collection().id().id(), "_", i.id().id());
  return path;
}

template Result
IResearchDataStore::insert<FieldIterator<FieldMeta>, IResearchLinkMeta>(
    transaction::Methods& trx, LocalDocumentId documentId,
    velocypack::Slice doc, IResearchLinkMeta const& meta);

template Result IResearchDataStore::insert<
    FieldIterator<IResearchInvertedIndexMetaIndexingContext>,
    IResearchInvertedIndexMetaIndexingContext>(
    transaction::Methods& trx, LocalDocumentId documentId,
    velocypack::Slice doc,
    IResearchInvertedIndexMetaIndexingContext const& meta);

template void
IResearchDataStore::recoveryInsert<FieldIterator<FieldMeta>, IResearchLinkMeta>(
    uint64_t tick, LocalDocumentId documentId, velocypack::Slice doc,
    IResearchLinkMeta const& meta);

template void IResearchDataStore::recoveryInsert<
    FieldIterator<IResearchInvertedIndexMetaIndexingContext>,
    IResearchInvertedIndexMetaIndexingContext>(
    uint64_t tick, LocalDocumentId documentId, velocypack::Slice doc,
    IResearchInvertedIndexMetaIndexingContext const& meta);

}  // namespace arangodb::iresearch
