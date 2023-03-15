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
#endif

#include <index/column_info.hpp>
#include <store/caching_directory.hpp>
#include <store/mmap_directory.hpp>
#include <store/store_utils.hpp>
#include <utils/encryption.hpp>
#include <utils/singleton.hpp>
#include <utils/file_utils.hpp>
#include <chrono>

#include <absl/cleanup/cleanup.h>
#include <absl/strings/str_cat.h>

using namespace std::literals;

namespace arangodb::iresearch {

#ifdef USE_ENTERPRISE
size_t getPrimaryDocsCount(irs::DirectoryReader const& reader);
#endif

namespace {

class IResearchFlushSubscription final : public FlushSubscription {
 public:
  explicit IResearchFlushSubscription(TRI_voc_tick_t tick = 0) noexcept
      : _tick{tick} {}

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

 private:
  std::atomic<TRI_voc_tick_t> _tick;
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

std::ostream& operator<<(std::ostream& out, const ThreadGroupStats& stats) {
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
      async->queue(T::threadGroup(), delay, static_cast<const T&>(*this));
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
                      transaction::Methods const& trx,
                      velocypack::Slice document, LocalDocumentId documentId,
                      MetaType const& meta, IndexId id) {
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
  FieldIteratorType body{collection, dataStore.index().id()};
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
    SortedValue field{collection, dataStore.index().id(), document};
    for (auto& sortField : meta._sort.fields()) {
      field.slice = get(document, sortField, VPackSlice::nullSlice());
      doc.template Insert<irs::Action::STORE_SORTED>(field);
    }
  }

  // Stored value field
  {
    StoredValue field{collection, dataStore.index().id(), document};
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

  if (trx.state()->hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
    ctx.SetLastTick(dataStore.engine()->currentTick());
  }
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

  static constexpr const char* typeName() noexcept { return "commit"; }

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
  const char runId = 0;
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
  static constexpr const char* typeName() noexcept { return "consolidation"; }
  void operator()();
  irs::MergeWriter::FlushProgress progress;
  IResearchDataStoreMeta::ConsolidationPolicy consolidationPolicy;
  std::chrono::milliseconds consolidationIntervalMsec{};
};

void ConsolidationTask::operator()() {
  const char runId = 0;
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

IResearchDataStore::IResearchDataStore(ArangodServer& server)
    : _asyncFeature(&server.getFeature<IResearchFeature>()),
      // mark as data store not initialized
      _asyncSelf(std::make_shared<AsyncLinkHandle>(nullptr)),
      _error(DataStoreError::kNoError),
      _maintenanceState(std::make_shared<MaintenanceState>()) {
  // initialize transaction callback
  _beforeCommitCallback = [this](TransactionState& state) {
    auto prev = state.cookie(this);  // get existing cookie
    if (!prev) {
      return;
    }
    // TODO FIXME find a better way to look up a ViewState
    auto& ctx = basics::downCast<IResearchTrxState>(*prev);
#if ARANGODB_ENABLE_MAINTAINER_MODE && ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("ArangoSearch::ThreeTransactionsMisorder") {
      size_t myNumber{0};
      while (true) {
        std::unique_lock sync{_t3FailureSync};
        if (_t3PreCommit < 3) {
          if (!myNumber) {
            myNumber = ++_t3PreCommit;
            LOG_TOPIC("76f6a", DEBUG, TOPIC)
                << myNumber << " arrived to preCommit";
          } else {
            sync.unlock();
            std::this_thread::sleep_for(100ms);
          }
        } else {
          if (myNumber == 2) {
            // Number 2 should go tho another flush context
            // so wait for commit to start and
            // then give writer some time to switch flush context
            if (!_t3CommitSignal) {
              sync.unlock();
              std::this_thread::sleep_for(100ms);
              continue;
            }
            std::this_thread::sleep_for(5s);
          }
          ctx._ctx.ForceFlush();
          LOG_TOPIC("cdc04", DEBUG, TOPIC) << myNumber << " added to flush";
          ++_t3NumFlushRegistered;
          break;
        }
      }
      // this transaction is flushed. Need to wait for all to arrive
      while (true) {
        std::unique_lock sync{_t3FailureSync};
        if (_t3NumFlushRegistered == 3) {
          // all are fluhing. Now release in order 1 2 3
          sync.unlock();
          std::this_thread::sleep_for(std::chrono::seconds(myNumber));
          LOG_TOPIC("0a90a", DEBUG, TOPIC)
              << myNumber << " released  thread " << std::this_thread::get_id();
          break;
        } else {
          sync.unlock();
          std::this_thread::sleep_for(50ms);
        }
      }
    }
    else {
      ctx._ctx.ForceFlush();
    }
#else
    ctx._ctx.ForceFlush();
#endif
  };
  _afterCommitCallback = [this](TransactionState& state) {
    auto prev = state.cookie(this, nullptr);  // extract existing cookie
    if (!prev) {
      return;
    }
    // TODO FIXME find a better way to look up a ViewState
    auto& ctx = basics::downCast<IResearchTrxState>(*prev);
    if (!ctx._removals.empty()) {
      // hold references even after transaction
      auto filter =
          std::make_unique<PrimaryKeyFilterContainer>(std::move(ctx._removals));
      ctx._ctx.Remove(std::unique_ptr<irs::filter>(std::move(filter)));
    }
    auto const lastOperationTick = state.lastOperationTick();
#if ARANGODB_ENABLE_MAINTAINER_MODE && ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("ArangoSearch::ThreeTransactionsMisorder") {
      LOG_TOPIC("8ac71", DEBUG, TOPIC)
          << lastOperationTick << " arrived to post commit in thread "
          << std::this_thread::get_id();
      bool inList{false};
      while (true) {
        std::unique_lock sync{_t3FailureSync};
        if (_t3Candidates.size() >= 3) {
          if (inList) {
            // decide who we are  - 1/3 or 2?
            bool lessTick{false}, moreTick{false};
            for (auto t : _t3Candidates) {
              if (t > lastOperationTick) {
                moreTick = true;
              } else if (t < lastOperationTick) {
                lessTick = true;
              }
            }
            if (lessTick && moreTick) {
              sync.unlock();
              // we are number 2. We must wait for 1/3 to commit
              std::this_thread::sleep_for(10s);
              TRI_IF_FAILURE(
                  "ArangoSearch::ThreeTransactionsMisorder::Number2Crash") {
                TRI_TerminateDebugging(
                    "ArangoSearch::ThreeTransactionsMisorder Number2");
              }
            }
          }
          LOG_TOPIC("182ec", DEBUG, TOPIC) << lastOperationTick << " released";
          break;
        }
        if (!inList) {
          LOG_TOPIC("d3db7", DEBUG, TOPIC)
              << lastOperationTick << " in wait list";
          _t3Candidates.push_back(lastOperationTick);
          inList = true;
        } else {
          // just arbitrary wait. We need all 3 candidates to gather
          sync.unlock();
          std::this_thread::sleep_for(500ms);
        }
      }
    }
#endif
    ctx._ctx.SetLastTick(lastOperationTick);
    if (ADB_LIKELY(!_engine->inRecovery())) {
      ctx._ctx.SetFirstTick(lastOperationTick - state.numPrimitiveOperations());
    }
    TRI_ASSERT(ctx._wasCommit == false);
    ctx._wasCommit = true;
  };
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

  if (result.fail() && !isOutOfSync()) {
    // mark DataStore as out of sync if it wasn't marked like that before.

    if (setOutOfSync()) {
      // persist "outOfSync" flag in RocksDB once. note: if this fails, it will
      // throw an exception
      try {
        _engine->changeCollection(index().collection().vocbase(),
                                  index().collection());
      } catch (std::exception const& ex) {
        // we couldn't persist the outOfSync flag, but we can't mark the data
        // store as "not outOfSync" again. not much we can do except logging.
        LOG_TOPIC("211d2", WARN, iresearch::TOPIC)
            << "failed to store 'outOfSync' flag for ArangoSearch index '"
            << index().id() << "': " << ex.what();
      }
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

  auto subscription = std::atomic_load(&_flushSubscription);
  if (!subscription) {  // already released
    return {};
  }

  auto& impl = basics::downCast<IResearchFlushSubscription>(*subscription);

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
    _commitStageOne = true;
    auto stageOneGuard = absl::Cleanup{
        [&, lastCommittedTickOne = _lastCommittedTickOne]() noexcept {
          _lastCommittedTickOne = lastCommittedTickOne;
        }};
    auto engineSnapshot = _engine->currentSnapshot();
    if (ADB_UNLIKELY(!engineSnapshot)) {
      return {TRI_ERROR_INTERNAL,
              absl::StrCat("Failed to get engine snapshot while committing "
                           "ArangoSearch index '",
                           index().id().id(), "'")};
    }
    auto const lastTickBeforeCommitOne = engineSnapshot->tick();
#if ARANGODB_ENABLE_MAINTAINER_MODE && ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("ArangoSearch::ThreeTransactionsMisorder") {
      std::unique_lock sync{_t3FailureSync};
      while (_t3NumFlushRegistered < 2) {
        sync.unlock();
        std::this_thread::sleep_for(100ms);
        sync.lock();
      }
      _t3CommitSignal = true;
      LOG_TOPIC("4cb66", DEBUG, TOPIC) << "Commit started";
    }
#endif
    auto const commitOne = _dataStore._writer->Commit(
        std::numeric_limits<uint64_t>::max(), progress);
    std::move(stageOneGuard).Cancel();
    if (!commitOne) {
      LOG_TOPIC("7e319", TRACE, TOPIC)
          << "First commit for ArangoSearch index '" << index().id()
          << "' is no changes tick " << lastTickBeforeCommitOne;
      TRI_ASSERT(_lastCommittedTickOne <= lastTickBeforeCommitOne);
      TRI_ASSERT(_lastCommittedTickTwo <= lastTickBeforeCommitOne);
      // no changes, can release the latest tick before commit one
      _lastCommittedTickOne = lastTickBeforeCommitOne;
      _lastCommittedTickTwo = lastTickBeforeCommitOne;
      impl.tick(_lastCommittedTickOne);
      auto reader = _dataStore.loadSnapshot()->_reader;
      _dataStore.storeSnapshot(
          std::make_shared<DataSnapshot>(std::move(reader), engineSnapshot));
      return {};
    } else {
      code = CommitResult::DONE;
    }
#if ARANGODB_ENABLE_MAINTAINER_MODE && ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("ArangoSearch::ThreeTransactionsMisorder::StageOneKill") {
      std::unique_lock sync{_t3FailureSync};
      // if all candidates gathered and max tick is committed it's time to crash
      if (_t3Candidates.size() >= 3 &&
          std::find_if(_t3Candidates.begin(), _t3Candidates.end(),
                       [this](uint64_t t) {
                         return t > _lastCommittedTickOne;
                       }) == _t3Candidates.end()) {
        TRI_TerminateDebugging("Killed on commit stage one");
      }
    }
#endif
    // now commit what has possibly accumulated in the second flush context
    // but won't move the tick as it possibly contains "3rd" generation changes
    _commitStageOne = false;
    auto stageTwoGuard = absl::Cleanup{
        [&, lastCommittedTickTwo = _lastCommittedTickTwo]() noexcept {
          _lastCommittedTickTwo = lastCommittedTickTwo;
        }};
    auto const lastTickBeforeCommitTwo = _engine->currentTick();
    auto const commitTwo = _dataStore._writer->Commit(
        std::numeric_limits<uint64_t>::max(), progress);
    std::move(stageTwoGuard).Cancel();
    if (!commitTwo) {
      LOG_TOPIC("21bda", TRACE, TOPIC)
          << "Second commit for ArangoSearch index '" << index().id()
          << "' is no changes tick " << lastTickBeforeCommitTwo;
      TRI_ASSERT(_lastCommittedTickOne <= lastTickBeforeCommitTwo);
      TRI_ASSERT(_lastCommittedTickTwo <= lastTickBeforeCommitTwo);
      // no changes, can release the latest tick before commit two
      _lastCommittedTickOne = lastTickBeforeCommitTwo;
      _lastCommittedTickTwo = lastTickBeforeCommitTwo;
    }
#if ARANGODB_ENABLE_MAINTAINER_MODE && ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("ArangoSearch::ThreeTransactionsMisorder::StageTwoKill") {
      std::unique_lock sync{_t3FailureSync};
      // if all candidates gathered and max tick is committed - it is time to
      // crash
      if (_t3Candidates.size() >= 3 &&
          std::find_if(_t3Candidates.begin(), _t3Candidates.end(),
                       [this](uint64_t t) {
                         return t > _lastCommittedTickOne;
                       }) == _t3Candidates.end()) {
        TRI_TerminateDebugging("Killed on commit stage two");
      }
    }
#endif
    // get new reader
    auto reader = _dataStore._writer->GetSnapshot();

    if (!reader) {
      // nothing more to do
      LOG_TOPIC("37bcf", WARN, TOPIC)
          << "Failed to update snapshot after commit, reuse "
             "the existing snapshot for ArangoSearch index '"
          << index().id() << "'";

      return {};
    }

    // update reader
    TRI_ASSERT(_dataStore.loadSnapshot()->_reader != reader);
    auto const readerSize = reader->size();
    auto const docsCount = reader->docs_count();
    auto const liveDocsCount = reader->live_docs_count();

    // For now we want to retain old behavior that
    // storage snapshot is not older than iresearch snapshot
    // TODO: Remove this as soon as index_writer will start accepting limiting
    // tick for commit
    auto engineSnapshotAfterCommit = _engine->currentSnapshot();
    if (ADB_UNLIKELY(!engineSnapshotAfterCommit)) {
      return {
          TRI_ERROR_INTERNAL,
          absl::StrCat("Failed to get engine snapshot while finishing commit "
                       "ArangoSearch index '",
                       index().id().id(), "'")};
    }

    _dataStore.storeSnapshot(std::make_shared<DataSnapshot>(
        std::move(reader), std::move(engineSnapshotAfterCommit)));

    // update stats
    updateStatsUnsafe();

    // update last committed tick
    impl.tick(_lastCommittedTickOne);

    invalidateQueryCache(&index().collection().vocbase());

    LOG_TOPIC("7e328", DEBUG, iresearch::TOPIC)
        << "successful sync of ArangoSearch index '" << index().id()
        << "', segments '" << readerSize << "', docs count '" << docsCount
        << "', live docs count '" << liveDocsCount
        << "', last operation tick low '" << _lastCommittedTickOne << "'"
        << "', last operation tick high '" << _lastCommittedTickTwo << "'";
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
  std::atomic_store(&_flushSubscription, {});  // reset together with _asyncSelf
  // the data-store is being deallocated, link use is no longer valid
  _asyncSelf->reset();  // wait for all the view users to finish
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
  _dataStore.resetDataStore();
}

Result IResearchDataStore::deleteDataStore() noexcept {
  shutdownDataStore();
  bool exists;
  // remove persisted data store directory if present
  if (!irs::file_utils::exists_directory(exists, _dataStore._path.c_str()) ||
      (exists && !irs::file_utils::remove(_dataStore._path.c_str()))) {
    return {TRI_ERROR_INTERNAL,
            absl::StrCat("failed to remove ArangoSearch index '",
                         index().id().id(), "'")};
  }
  return {};
}

bool IResearchDataStore::failQueriesOnOutOfSync() const noexcept {
  return _asyncFeature->failQueriesOnOutOfSync();
}

bool IResearchDataStore::setOutOfSync() noexcept {
  // should never be called on coordinators, only on DB servers and
  // single servers
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto error = _error.load(std::memory_order_acquire);
  if (error == DataStoreError::kNoError) {
    if (_error.compare_exchange_strong(error, DataStoreError::kOutOfSync,
                                       std::memory_order_release)) {
      // increase metric for number of out-of-sync links, only once per link
      TRI_ASSERT(_asyncFeature != nullptr);
      _asyncFeature->trackOutOfSyncLink();

      return true;
    }
  }

  return false;
}

bool IResearchDataStore::isOutOfSync() const noexcept {
  // the out of sync flag is expected to be set either during the
  // recovery phase, or when a commit goes wrong.
  return _error.load(std::memory_order_acquire) == DataStoreError::kOutOfSync;
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
    const auto version = irs::numeric_utils::hton32(
        static_cast<std::underlying_type_t<SegmentPayloadVersion>>(
            SegmentPayloadVersion::TwoStageTick));

    // call from commit under lock _commitMutex (_dataStore._writer->Commit())
    // update current stage commit tick
    auto lastCommittedTick =
        _commitStageOne ? &_lastCommittedTickOne : &_lastCommittedTickTwo;
    *lastCommittedTick = std::max(*lastCommittedTick, tick);
    // convert to BE and write low tick
    tick = irs::numeric_utils::hton64(uint64_t{
        _commitStageOne ? _lastCommittedTickTwo : _lastCommittedTickOne});
    out.append(reinterpret_cast<irs::byte_type const*>(&tick), sizeof(tick));
    // write converted to BE version
    out.append(reinterpret_cast<irs::byte_type const*>(&version),
               sizeof(version));
    // convert to BE and write high tick
    tick = irs::numeric_utils::hton64(
        std::max(_lastCommittedTickOne, _lastCommittedTickTwo));
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
  // Reset together with '_asyncSelf'
  std::atomic_store(&_flushSubscription, {});
  // The data-store is being deallocated, link use is no longer valid
  // (wait for all the view users to finish)
  _asyncSelf->reset();
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
  auto& flushFeature = server.getFeature<FlushFeature>();

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

  // Must manually ensure that the data store directory exists (since not
  // using a lockfile)
  if (!irs::file_utils::exists_directory(pathExists,
                                         _dataStore._path.c_str()) ||
      (!pathExists &&
       !irs::file_utils::mkdir(_dataStore._path.c_str(), true))) {
    return {TRI_ERROR_CANNOT_CREATE_DIRECTORY,
            absl::StrCat("Failed to create data store directory with path '",
                         _dataStore._path.string(),
                         "' while initializing ArangoSearch index '",
                         index().id().id(), "'")};
  }

  _dataStore._directory = std::make_unique<irs::MMapDirectory>(
      _dataStore._path.u8string(),
      initCallback ? initCallback() : irs::directory_attributes{});

  switch (_engine->recoveryState()) {
    case RecoveryState::BEFORE:  // Link is being opened before recovery
      [[fallthrough]];
    case RecoveryState::DONE: {  // Link is being created after recovery
      // Will be adjusted in post-recovery callback
      _dataStore._inRecovery.store(true, std::memory_order_release);
      _dataStore._recoveryTickHigh = _dataStore._recoveryTickLow =
          _engine->recoveryTick();
    } break;
    case RecoveryState::IN_PROGRESS: {  // Link is being created during recovery
      _dataStore._inRecovery.store(false, std::memory_order_release);
      _dataStore._recoveryTickHigh = _dataStore._recoveryTickLow =
          _engine->releasedTick();
    } break;
  }
  _lastCommittedTickTwo = _lastCommittedTickOne = _dataStore._recoveryTickLow;

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
    _lastCommittedTickTwo = _lastCommittedTickOne = _dataStore._recoveryTickLow;
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

  _flushSubscription =
      std::make_shared<IResearchFlushSubscription>(_dataStore._recoveryTickLow);

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

  return dbFeature.registerPostRecoveryCallback(  // register callback
      [asyncSelf = _asyncSelf, &flushFeature,
       asyncFeature = _asyncFeature]() -> Result {
        auto linkLock = asyncSelf->lock();
        // ensure link does not get deallocated before callback finishes

        if (!linkLock) {
          // link no longer in recovery state, i.e. during recovery
          // it was created and later dropped
          return {};
        }

        if (!linkLock->_flushSubscription) {
          return {TRI_ERROR_INTERNAL,
                  absl::StrCat("Failed to register flush subscription for "
                               "ArangoSearch index '",
                               linkLock->index().id().id(), "'")};
        }

        auto& dataStore = linkLock->_dataStore;

        // recovery finished
        dataStore._inRecovery.store(linkLock->_engine->inRecovery(),
                                    std::memory_order_release);

        bool outOfSync = false;
        if (asyncFeature->linkSkippedDuringRecovery(linkLock->index().id())) {
          LOG_TOPIC("2721a", WARN, iresearch::TOPIC)
              << "Marking ArangoSearch index '" << linkLock->index().id().id()
              << "' as out of sync, consider to drop and re-create the index "
                 "in order to synchronize it";

          outOfSync = true;
        } else if (dataStore._recoveryTickLow >
                   linkLock->_engine->recoveryTick()) {
          LOG_TOPIC("5b59f", WARN, iresearch::TOPIC)
              << "ArangoSearch index '" << linkLock->index().id()
              << "' is recovered at tick '" << dataStore._recoveryTickLow
              << "' less than storage engine tick '"
              << linkLock->_engine->recoveryTick()
              << "', it seems WAL tail was lost and index is out of sync with "
                 "the underlying collection '"
              << linkLock->index().collection().name()
              << "', consider to re-create the index in order to synchronize "
                 "it";

          outOfSync = true;
        }

        if (outOfSync) {
          // mark link as out of sync
          linkLock->setOutOfSync();
          // persist "out of sync" flag in RocksDB. note: if this fails, it
          // will throw an exception and abort the recovery & startup.
          linkLock->_engine->changeCollection(
              linkLock->index().collection().vocbase(),
              linkLock->index().collection());

          if (asyncFeature->failQueriesOnOutOfSync()) {
            // we cannot return an error from here as this would abort the
            // entire recovery and fail the startup.
            return {};
          }
        }

        irs::ProgressReportCallback progress =
            [id = linkLock->index().id(), asyncFeature](
                std::string_view phase, size_t current, size_t total) {
              // forward progress reporting to asyncFeature
              asyncFeature->reportRecoveryProgress(id, phase, current, total);
            };

        LOG_TOPIC("5b59c", TRACE, iresearch::TOPIC)
            << "Start sync for ArangoSearch index '" << linkLock->index().id()
            << "'";

        CommitResult code{CommitResult::UNDEFINED};
        auto [res, timeMs] = linkLock->commitUnsafe(true, progress, code);

        LOG_TOPIC("0e0ca", TRACE, iresearch::TOPIC)
            << "Finish sync for ArangoSearch index '" << linkLock->index().id()
            << "'";

        // register flush subscription
        flushFeature.registerFlushSubscription(linkLock->_flushSubscription);

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

Result IResearchDataStore::remove(transaction::Methods& trx,
                                  LocalDocumentId documentId, bool nested,
                                  uint64_t const* recoveryTick) {
  TRI_ASSERT(trx.state());

  auto& state = *(trx.state());

  TRI_ASSERT(!state.hasHint(transaction::Hints::Hint::INDEX_CREATION));

  if (recoveryTick && *recoveryTick <= _dataStore._recoveryTickLow) {
    LOG_TOPIC("7d228", TRACE, TOPIC)
        << "skipping 'removal', operation tick '" << *recoveryTick
        << "', recovery tick '" << _dataStore._recoveryTickLow << "'";

    return {};
  }

  if (_asyncFeature->failQueriesOnOutOfSync() && isOutOfSync()) {
    return {};
  }

  auto* key = this;
  // TODO FIXME find a better way to look up a ViewState
  auto* ctx = basics::downCast<IResearchTrxState>(state.cookie(key));
  if (!ctx) {
    // '_dataStore' can be asynchronously modified
    auto linkLock = _asyncSelf->lock();
    if (!linkLock) {
      // the current link is no longer valid
      // (checked after ReadLock acquisition)

      return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
              absl::StrCat(
                  "failed to lock ArangoSearch index '", index().id().id(),
                  "'while removing a document from it: tid '", state.id().id(),
                  "', documentId '", documentId.id(), "'")};
    }

    TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

    auto ptr = std::make_unique<IResearchTrxState>(std::move(linkLock),
                                                   *(_dataStore._writer));

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));
    if (!ctx) {
      return {
          TRI_ERROR_INTERNAL,
          absl::StrCat(
              "failed to store state into a TransactionState for remove from "
              "ArangoSearch index '",
              index().id().id(), "', tid '", state.id().id(), "', documentId '",
              documentId.id(), "'")};
    }
    state.addBeforeCommitCallback(&_beforeCommitCallback);
    state.addAfterCommitCallback(&_afterCommitCallback);
  }

  // ...........................................................................
  // if an exception occurs below than the transaction is dropped including
  // all all of its fid stores, no impact to iResearch View data integrity
  // ...........................................................................
  try {
    ctx->remove(documentId, nested);

    return {TRI_ERROR_NO_ERROR};
  } catch (basics::Exception const& e) {
    return {e.code(), absl::StrCat("caught exception while removing document "
                                   "from ArangoSearch index '",
                                   index().id().id(), "', documentId '",
                                   documentId.id(), "': ", e.message())};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            absl::StrCat("caught exception while removing document from "
                         "ArangoSearch index '",
                         index().id().id(), "', documentId '", documentId.id(),
                         "': ", e.what())};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            absl::StrCat("caught exception while removing document from "
                         "ArangoSearch index '",
                         index().id().id(), "', documentId '", documentId.id(),
                         "'")};
  }

  return {};
}

bool IResearchDataStore::exists(IResearchDataStore::Snapshot const& snapshot,
                                LocalDocumentId documentId,
                                TRI_voc_tick_t const* recoveryTick) const {
  if (recoveryTick == nullptr) {
    // skip recovery check
  } else if (*recoveryTick <= _dataStore._recoveryTickLow) {
    LOG_TOPIC("6e128", TRACE, TOPIC)
        << "skipping 'exists', operation tick '" << *recoveryTick
        << "', recovery tick low '" << _dataStore._recoveryTickLow << "'";

    return true;
  } else if (*recoveryTick > _dataStore._recoveryTickHigh) {
    LOG_TOPIC("6e129", TRACE, TOPIC)
        << "skipping 'exists', operation tick '" << *recoveryTick
        << "', recovery tick high '" << _dataStore._recoveryTickHigh << "'";
    return false;
  }

  auto const encoded = DocumentPrimaryKey::encode(documentId);

  auto const pk =
      irs::numeric_utils::numeric_traits<LocalDocumentId::BaseType>::raw_ref(
          encoded);

  for (auto const& segment : snapshot.getDirectoryReader()) {
    auto const* pkField = segment.field(DocumentPrimaryKey::PK());

    if (ADB_UNLIKELY(!pkField)) {
      continue;
    }

    auto const meta = pkField->term(pk);

    if (meta.docs_count) {
      return true;
    }
  }

  return false;
}

template<typename FieldIteratorType, typename MetaType>
Result IResearchDataStore::insert(transaction::Methods& trx,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc, MetaType const& meta,
                                  uint64_t const* recoveryTick) {
  TRI_ASSERT(trx.state());

  auto& state = *(trx.state());

  if (recoveryTick && *recoveryTick <= _dataStore._recoveryTickLow) {
    LOG_TOPIC("7c228", TRACE, TOPIC)
        << "skipping 'insert', operation tick '" << *recoveryTick
        << "', recovery tick '" << _dataStore._recoveryTickLow << "'";

    return {};
  }

  if (_asyncFeature->failQueriesOnOutOfSync() && isOutOfSync()) {
    return {};
  }

  auto insertImpl = [&, &self = *this, id = index().id()](
                        irs::IndexWriter::Transaction& ctx) -> Result {
    try {
      return insertDocument<FieldIteratorType>(self, ctx, trx, doc, documentId,
                                               meta, id);
    } catch (basics::Exception const& e) {
      return {e.code(), absl::StrCat("caught exception while inserting "
                                     "document into arangosearch index '",
                                     id.id(), "', documentId '",
                                     documentId.id(), "': ", e.what())};
    } catch (std::exception const& e) {
      return {TRI_ERROR_INTERNAL,
              absl::StrCat("caught exception while inserting document into "
                           "arangosearch index '",
                           id.id(), "', documentId '", documentId.id(),
                           "': ", e.what())};
    } catch (...) {
      return {TRI_ERROR_INTERNAL,
              absl::StrCat("caught exception while inserting document into "
                           "arangosearch index '",
                           id.id(), "', documentId '", documentId.id(), "'")};
    }
  };

  TRI_IF_FAILURE("ArangoSearch::BlockInsertsWithoutIndexCreationHint") {
    if (!state.hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
      return {TRI_ERROR_DEBUG};
    }
  }

  if (state.hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
    auto linkLock = _asyncSelf->lock();
    if (!linkLock) {
      return {TRI_ERROR_INTERNAL};
    }
    auto ctx = _dataStore._writer->GetBatch();
    TRI_IF_FAILURE("ArangoSearch::MisreportCreationInsertAsFailed") {
      auto res = insertImpl(ctx);  // we need insert to succeed, so  we have
                                   // things to cleanup in storage
      if (res.fail()) {
        return res;
      }
      return {TRI_ERROR_DEBUG};
    }
    return insertImpl(ctx);
  }
  auto* key = this;
  // TODO FIXME find a better way to look up a ViewState
  auto* ctx = basics::downCast<IResearchTrxState>(state.cookie(key));
  if (!ctx) {
    // '_dataStore' can be asynchronously modified
    auto linkLock = _asyncSelf->lock();

    if (!linkLock) {
      // the current link is no longer valid (checked after ReadLock
      // acquisition)
      return {
          TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
          absl::StrCat("failed to lock ArangoSearch index '", index().id().id(),
                       "' while inserting a document into it")};
    }

    TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

    // FIXME try to preserve optimization
    //    // optimization for single-document insert-only transactions
    //    if (trx.isSingleOperationTransaction() // only for single-docuemnt
    //    transactions
    //        && !_dataStore._inRecovery) {
    //      auto ctx = _dataStore._writer->documents();
    //
    //      return insertImpl(ctx);
    //    }

    auto ptr = std::make_unique<IResearchTrxState>(std::move(linkLock),
                                                   *(_dataStore._writer));

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx) {
      return {TRI_ERROR_INTERNAL,
              absl::StrCat("failed to store state into a TransactionState for "
                           "insert into ArangoSearch index '",
                           index().id().id(), "', tid '", state.id().id(),
                           "', revision '", documentId.id(), "'")};
    }
    state.addBeforeCommitCallback(&_beforeCommitCallback);
    state.addAfterCommitCallback(&_afterCommitCallback);
  }

  return insertImpl(ctx->_ctx);
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
  }

  std::lock_guard commitLock{_commitMutex};
  auto clearGuard =
      absl::Cleanup{[&, lastCommittedTickOne = _lastCommittedTickOne,
                     lastCommittedTickTwo = _lastCommittedTickTwo]() noexcept {
        _lastCommittedTickOne = lastCommittedTickOne;
        _lastCommittedTickTwo = lastCommittedTickTwo;
      }};
  _lastCommittedTickTwo = tick;
  _commitStageOne = true;
  try {
    _dataStore._writer->Clear(tick);
    std::move(clearGuard).Cancel();
    // payload will not be called is index already empty
    _lastCommittedTickOne = std::max(tick, _lastCommittedTickOne);
    _lastCommittedTickTwo = _lastCommittedTickOne;

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

    auto subscription = std::atomic_load(&_flushSubscription);

    if (subscription) {
      auto& impl = static_cast<IResearchFlushSubscription&>(*subscription);

      impl.tick(_lastCommittedTickOne);
    }
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
  if (hasNestedFields()) {
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
                              ClusterMetricsFeature::MetricValue const& value) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief compute the data path to user for iresearch data store
///        get base path from DatabaseServerFeature (similar to MMFilesEngine)
///        the path is hardcoded to reside under:
///        <DatabasePath>/<IResearchLink::type()>-<link id>
///        similar to the data path calculation for collections
////////////////////////////////////////////////////////////////////////////////
std::filesystem::path getPersistedPath(DatabasePathFeature const& dbPathFeature,
                                       IResearchDataStore const& dataStore) {
  std::filesystem::path dataPath{dbPathFeature.directory()};

  dataPath /= "databases";
  auto& i = dataStore.index();
  dataPath /= absl::StrCat("database-", i.collection().vocbase().id());
  dataPath /=
      absl::StrCat(StaticStrings::ViewArangoSearchType, "-",
                   // has to be 'id' since this can be a per-shard collection
                   i.collection().id().id(), "_", i.id().id());

  return dataPath;
}

template Result
IResearchDataStore::insert<FieldIterator<FieldMeta>, IResearchLinkMeta>(
    transaction::Methods& trx, LocalDocumentId documentId,
    velocypack::Slice doc, IResearchLinkMeta const& meta,
    uint64_t const* recoveryTick);

template Result IResearchDataStore::insert<
    FieldIterator<IResearchInvertedIndexMetaIndexingContext>,
    IResearchInvertedIndexMetaIndexingContext>(
    transaction::Methods& trx, LocalDocumentId documentId,
    velocypack::Slice doc,
    IResearchInvertedIndexMetaIndexingContext const& meta,
    uint64_t const* recoveryTick);

}  // namespace arangodb::iresearch
