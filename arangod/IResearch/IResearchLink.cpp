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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <index/column_info.hpp>
#include <store/mmap_directory.hpp>
#include <utils/encryption.hpp>
#include <utils/file_utils.hpp>
#include <utils/singleton.hpp>

#include "IResearchDocument.h"
#include "IResearchLink.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#ifdef USE_ENTERPRISE
#include "Cluster/ClusterMethods.h"
#endif
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchCompression.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchPrimaryKeyFilter.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/VelocyPackHelper.h"
#include "Metrics/BatchBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

using namespace std::literals;

namespace {

using namespace arangodb;
using namespace arangodb::iresearch;

DECLARE_GAUGE(arangosearch_num_buffered_docs, uint64_t,
              "Number of buffered documents");
DECLARE_GAUGE(arangosearch_num_docs, uint64_t, "Number of documents");
DECLARE_GAUGE(arangosearch_num_live_docs, uint64_t, "Number of live documents");
DECLARE_GAUGE(arangosearch_num_segments, uint64_t, "Number of segments");
DECLARE_GAUGE(arangosearch_num_files, uint64_t, "Number of files");
DECLARE_GAUGE(arangosearch_index_size, uint64_t, "Size of the index in bytes");
DECLARE_GAUGE(arangosearch_num_failed_commits, uint64_t,
              "Number of failed commits");
DECLARE_GAUGE(arangosearch_num_failed_cleanups, uint64_t,
              "Number of failed cleanups");
DECLARE_GAUGE(arangosearch_num_failed_consolidations, uint64_t,
              "Number of failed consolidations");
DECLARE_GAUGE(arangosearch_commit_time, uint64_t,
              "Average time of few last commits");
DECLARE_GAUGE(arangosearch_cleanup_time, uint64_t,
              "Average time of few last cleanups");
DECLARE_GAUGE(arangosearch_consolidation_time, uint64_t,
              "Average time of few last consolidations");

////////////////////////////////////////////////////////////////////////////////
/// @brief container storing the link state for a given TransactionState
////////////////////////////////////////////////////////////////////////////////
struct LinkTrxState final : public TransactionState::Cookie {
  LinkTrxState(LinkTrxState const&) = delete;
  LinkTrxState(LinkTrxState&&) = delete;
  LinkTrxState& operator=(LinkTrxState const&) = delete;
  LinkTrxState& operator=(LinkTrxState&&) = delete;

  irs::index_writer::documents_context _ctx;
  AsyncValue<IResearchLink>::Value _linkLock;  // prevent data-store deallocation (lock @ AsyncSelf)
  PrimaryKeyFilterContainer _removals;  // list of document removals

  LinkTrxState(AsyncValue<IResearchLink>::Value&& linkLock, irs::index_writer& writer) noexcept
      : _ctx{writer.documents()}, _linkLock{std::move(linkLock)} {
    TRI_ASSERT(_linkLock.ownsLock());
  }

  ~LinkTrxState() final {
    if (_removals.empty()) {
      return;  // nothing to do
    }

    try {
      // hold references even after transaction
      auto filter = std::make_unique<PrimaryKeyFilterContainer>(std::move(_removals));
      _ctx.remove(std::unique_ptr<irs::filter>(std::move(filter)));
    } catch (std::exception const& e) {
      LOG_TOPIC("eb463", ERR, arangodb::iresearch::TOPIC)
          << "caught exception while applying accumulated removals: " << e.what();
    } catch (...) {
      LOG_TOPIC("14917", WARN, arangodb::iresearch::TOPIC)
          << "caught exception while applying accumulated removals";
    }
  }

  void remove(StorageEngine& engine, LocalDocumentId const& value) {
    _ctx.remove(_removals.emplace(engine, value));
  }

  void reset() noexcept {
    _removals.clear();
    _ctx.reset();
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts ArangoDB document into an IResearch data store
////////////////////////////////////////////////////////////////////////////////
Result insertDocument(irs::index_writer::documents_context& ctx,
                      transaction::Methods const& trx, FieldIterator& body,
                      velocypack::Slice document, LocalDocumentId documentId,
                      IResearchLinkMeta const& meta, IndexId id) {
  body.reset(document, meta);  // reset reusable container to doc

  if (!body.valid()) {
    return {};  // no fields to index
  }

  auto doc = ctx.insert();
  auto const& field = *body;

  // User fields
  while (body.valid()) {
    if (ValueStorage::NONE == field._storeValues) {
      doc.insert<irs::Action::INDEX>(field);
    } else {
      doc.insert<irs::Action::INDEX | irs::Action::STORE>(field);
    }

    ++body;
  }

  // Sorted field
  {
    struct SortedField {
      bool write(irs::data_output& out) const {
        out.write_bytes(slice.start(), slice.byteSize());
        return true;
      }

      VPackSlice slice;
    } sorted;

    for (auto const& sortField : meta._sort.fields()) {
      sorted.slice = get(document, sortField, VPackSlice::nullSlice());
      doc.insert<irs::Action::STORE_SORTED>(sorted);
    }
  }

  // Stored value field
  {
    StoredValue stored(trx, meta._collectionName, document, id);
    for (auto const& column : meta._storedValues.columns()) {
      stored.fieldName = column.name;
      stored.fields = &column.fields;
      doc.insert<irs::Action::STORE>(stored);
    }
  }

  // System fields

  // Indexed and Stored: LocalDocumentId
  auto docPk = DocumentPrimaryKey::encode(documentId);

  // reuse the 'Field' instance stored inside the 'FieldIterator'
  Field::setPkValue(const_cast<Field&>(field), docPk);
  doc.insert<irs::Action::INDEX | irs::Action::STORE>(field);

  if (!doc) {
    return {TRI_ERROR_INTERNAL,
            "failed to insert document into arangosearch link '" +
                std::to_string(id.id()) + "', revision '" +
                std::to_string(documentId.id()) + "'"};
  }

  return {};
}

class IResearchFlushSubscription final : public FlushSubscription {
 public:
  explicit IResearchFlushSubscription(TRI_voc_tick_t tick = 0) noexcept
      : _tick{tick} {}

  /// @brief earliest tick that can be released
  TRI_voc_tick_t tick() const noexcept final {
    return _tick.load(std::memory_order_acquire);
  }

  void tick(TRI_voc_tick_t tick) noexcept {
    _tick.store(tick, std::memory_order_release);
  }

 private:
  std::atomic<TRI_voc_tick_t> _tick;
};

bool readTick(irs::bytes_ref const& payload, TRI_voc_tick_t& tick) noexcept {
  static_assert(sizeof(uint64_t) == sizeof(TRI_voc_tick_t));

  if (payload.size() != sizeof(uint64_t)) {
    return false;
  }

  std::memcpy(&tick, payload.c_str(), sizeof(uint64_t));
  tick = TRI_voc_tick_t(irs::numeric_utils::ntoh64(tick));

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
template <typename T>
struct Task {
  void schedule(std::chrono::milliseconds delay) const {
    LOG_TOPIC("eb0da", TRACE, arangodb::iresearch::TOPIC)
        << "scheduled a " << T::typeName() << " task for arangosearch link '"
        << id << "', delay '" << delay.count() << "'";

    LOG_TOPIC("eb0d2", TRACE, arangodb::iresearch::TOPIC)
        << T::typeName()
        << " pool: " << ThreadGroupStats{async->stats(T::threadGroup())};

    if (!link->terminationRequested()) {
      async->queue(T::threadGroup(), delay, static_cast<const T&>(*this));
    }
  }

  std::shared_ptr<MaintenanceState> state;
  IResearchFeature* async{nullptr};
  IResearchLink::AsyncLinkPtr link;
  IndexId id;
};

template <typename T>
T getMetric(const IResearchLink& link) {
  T metric;
  metric.addLabel("view", link.getViewId());
  metric.addLabel("collection", link.getCollectionName());
  metric.addLabel("shard", link.getShardName());
  metric.addLabel("db", link.getDbName());
  return metric;
}

uint64_t computeAvg(std::atomic<uint64_t>& timeNum, uint64_t newTime) {
  constexpr uint64_t kWindowSize{10};
  auto const oldTimeNum = timeNum.fetch_add((newTime << 32U) + 1, std::memory_order_relaxed);
  auto const oldTime = oldTimeNum >> 32U;
  auto const oldNum = oldTimeNum & std::numeric_limits<uint32_t>::max();
  if (oldNum >= kWindowSize) {
    timeNum.fetch_sub((oldTime / oldNum) + 1, std::memory_order_relaxed);
  }
  return (oldTime + newTime) / (oldNum + 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief total number of loaded links
////////////////////////////////////////////////////////////////////////////////
std::atomic_size_t sLinksCount{0};  // TODO Why?

}  // namespace

namespace arangodb::iresearch {

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
  void finalize(IResearchLink* link, IResearchLink::CommitResult code);

  size_t cleanupIntervalCount{};
  std::chrono::milliseconds commitIntervalMsec{};
  std::chrono::milliseconds consolidationIntervalMsec{};
  size_t cleanupIntervalStep{};
};

void CommitTask::finalize(IResearchLink* link, IResearchLink::CommitResult code) {
  constexpr size_t kMaxNonEmptyCommits = 10;
  constexpr size_t kMaxPendingConsolidations = 3;

  if (code != IResearchLink::CommitResult::NO_CHANGES) {
    state->pendingCommits.fetch_add(1, std::memory_order_release);
    schedule(commitIntervalMsec);

    if (code == IResearchLink::CommitResult::DONE) {
      state->noopCommitCount.store(0, std::memory_order_release);
      state->noopConsolidationCount.store(0, std::memory_order_release);

      if (state->pendingConsolidations.load(std::memory_order_acquire) < kMaxPendingConsolidations &&
          state->nonEmptyCommits.fetch_add(1, std::memory_order_acq_rel) >= kMaxNonEmptyCommits) {
        link->scheduleConsolidation(consolidationIntervalMsec);
        state->nonEmptyCommits.store(0, std::memory_order_release);
      }
    }
  } else {
    state->nonEmptyCommits.store(0, std::memory_order_release);
    state->noopCommitCount.fetch_add(1, std::memory_order_release);

    for (auto count = state->pendingCommits.load(std::memory_order_acquire); count < 1;) {
      if (state->pendingCommits.compare_exchange_weak(count, 1, std::memory_order_acq_rel)) {
        schedule(commitIntervalMsec);
        break;
      }
    }
  }
}

void CommitTask::operator()() {
  const char runId = 0;
  state->pendingCommits.fetch_sub(1, std::memory_order_release);

  if (link->terminationRequested()) {
    LOG_TOPIC("eba1a", DEBUG, iresearch::TOPIC)
        << "termination requested while committing the link '" << id
        << "', runId '" << size_t(&runId) << "'";
    return;
  }

  auto linkPtr = link->tryLock();

  if (!linkPtr.ownsLock()) {
    LOG_TOPIC("eb0de", DEBUG, iresearch::TOPIC)
        << "failed to acquire the lock while committing the link '" << id
        << "', runId '" << size_t(&runId) << "'";

    // blindly reschedule commit task
    state->pendingCommits.fetch_add(1, std::memory_order_release);
    schedule(commitIntervalMsec);
    return;
  }

  if (!linkPtr) {
    LOG_TOPIC("ebada", DEBUG, iresearch::TOPIC)
        << "link '" << id << "' is no longer valid, run id '" << size_t(&runId) << "'";
    return;
  }

  IResearchLink::CommitResult code = IResearchLink::CommitResult::UNDEFINED;

  auto reschedule = scopeGuard([&code, link = linkPtr.get(), this]() noexcept {
    try {
      finalize(link, code);
    } catch (std::exception const& ex) {
      LOG_TOPIC("ad67d", ERR, iresearch::TOPIC)
          << "failed to call finalize: " << ex.what();
    }
  });

  // reload RuntimeState
  {
    TRI_IF_FAILURE("IResearchCommitTask::lockDataStore") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_ASSERT(linkPtr->_dataStore);  // must be valid if _asyncSelf->get() is valid
    READ_LOCKER(lock, linkPtr->_dataStore._mutex);  // '_meta' can be asynchronously modified
    auto& meta = linkPtr->_dataStore._meta;

    commitIntervalMsec = std::chrono::milliseconds(meta._commitIntervalMsec);
    consolidationIntervalMsec = std::chrono::milliseconds(meta._consolidationIntervalMsec);
    cleanupIntervalStep = meta._cleanupIntervalStep;
  }

  if (std::chrono::milliseconds::zero() == commitIntervalMsec) {
    reschedule.cancel();

    LOG_TOPIC("eba4a", DEBUG, iresearch::TOPIC)
        << "sync is disabled for the link '" << id << "', runId '"
        << size_t(&runId) << "'";
    return;
  }

  TRI_IF_FAILURE("IResearchCommitTask::commitUnsafe") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  auto [res, timeMs] =
      linkPtr->commitUnsafe(false, &code);  // run commit ('_asyncSelf' locked by async task)

  if (res.ok()) {
    LOG_TOPIC("7e323", TRACE, iresearch::TOPIC)
        << "successful sync of arangosearch link '" << id << "', run id '"
        << size_t(&runId) << "', took: " << timeMs << "ms";
  } else {
    LOG_TOPIC("8377b", WARN, iresearch::TOPIC)
        << "error after running for " << timeMs << "ms while committing arangosearch link '"
        << linkPtr->id() << "', run id '" << size_t(&runId)
        << "': " << res.errorNumber() << " " << res.errorMessage();
  }

  if (cleanupIntervalStep && ++cleanupIntervalCount >= cleanupIntervalStep) {  // if enabled
    cleanupIntervalCount = 0;

    TRI_IF_FAILURE("IResearchCommitTask::cleanupUnsafe") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    auto [res, timeMs] = linkPtr->cleanupUnsafe();  // run cleanup ('_asyncSelf' locked by async task)

    if (res.ok()) {
      LOG_TOPIC("7e821", TRACE, iresearch::TOPIC)
          << "successful cleanup of arangosearch link '" << id << "', run id '"
          << size_t(&runId) << "', took: " << timeMs << "ms";
    } else {
      LOG_TOPIC("130de", WARN, iresearch::TOPIC)
          << "error after running for " << timeMs << "ms while cleaning up arangosearch link '"
          << id << "', run id '" << size_t(&runId) << "': " << res.errorNumber()
          << " " << res.errorMessage();
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

  irs::merge_writer::flush_progress_t progress;
  IResearchViewMeta::ConsolidationPolicy consolidationPolicy;
  std::chrono::milliseconds consolidationIntervalMsec{};
};

void ConsolidationTask::operator()() {
  const char runId = 0;
  state->pendingConsolidations.fetch_sub(1, std::memory_order_release);

  if (link->terminationRequested()) {
    LOG_TOPIC("eba2a", DEBUG, iresearch::TOPIC)
        << "termination requested while consolidating the link '" << id
        << "', runId '" << size_t(&runId) << "'";
    return;
  }

  auto linkPtr = link->tryLock();

  if (!linkPtr.ownsLock()) {
    LOG_TOPIC("eb0dc", DEBUG, iresearch::TOPIC)
        << "failed to acquire the lock while consolidating the link '" << id
        << "', run id '" << size_t(&runId) << "'";

    // blindly reschedule consolidation task
    state->pendingConsolidations.fetch_add(1, std::memory_order_release);
    schedule(consolidationIntervalMsec);
    return;
  }

  if (!linkPtr) {
    LOG_TOPIC("eb0d1", DEBUG, iresearch::TOPIC)
        << "link '" << id << "' is no longer valid, run id '" << size_t(&runId) << "'";
    return;
  }

  auto reschedule = scopeGuard([this]() noexcept {
    try {
      for (auto count = state->pendingConsolidations.load(std::memory_order_acquire);
           count < 1;) {
        if (state->pendingConsolidations.compare_exchange_weak(count, count + 1,
                                                               std::memory_order_acq_rel)) {
          schedule(consolidationIntervalMsec);
          break;
        }
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("2642a", ERR, iresearch::TOPIC) << "failed to reschedule: " << ex.what();
    }
  });

  // reload RuntimeState
  {
    TRI_IF_FAILURE("IResearchConsolidationTask::lockDataStore") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_ASSERT(linkPtr->_dataStore);  // must be valid if _asyncSelf->get() is valid
    READ_LOCKER(lock, linkPtr->_dataStore._mutex);  // '_meta' can be asynchronously modified
    auto& meta = linkPtr->_dataStore._meta;

    consolidationPolicy = meta._consolidationPolicy;
    consolidationIntervalMsec = std::chrono::milliseconds(meta._consolidationIntervalMsec);
  }

  if (std::chrono::milliseconds::zero() == consolidationIntervalMsec  // disabled via interval
      || !consolidationPolicy.policy()) {  // disabled via policy
    reschedule.cancel();

    LOG_TOPIC("eba3a", DEBUG, iresearch::TOPIC)
        << "consolidation is disabled for the link '" << id << "', runId '"
        << size_t(&runId) << "'";
    return;
  }

  constexpr size_t kMaxNoopCommits = 10;
  constexpr size_t kMaxNoopConsolidations = 10;

  if (state->noopCommitCount.load(std::memory_order_acquire) < kMaxNoopCommits &&
      state->noopConsolidationCount.load(std::memory_order_acquire) < kMaxNoopConsolidations) {
    state->pendingConsolidations.fetch_add(1, std::memory_order_release);
    schedule(consolidationIntervalMsec);
  }

  TRI_IF_FAILURE("IResearchConsolidationTask::consolidateUnsafe") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // run consolidation ('_asyncSelf' locked by async task)
  bool emptyConsolidation = false;
  auto const [res, timeMs] =
      linkPtr->consolidateUnsafe(consolidationPolicy, progress, emptyConsolidation);

  if (res.ok()) {
    if (emptyConsolidation) {
      state->noopConsolidationCount.fetch_add(1, std::memory_order_release);
    } else {
      state->noopConsolidationCount.store(0, std::memory_order_release);
    }
    LOG_TOPIC("7e828", TRACE, iresearch::TOPIC)
        << "successful consolidation of arangosearch link '" << linkPtr->id()
        << "', run id '" << size_t(&runId) << "', took: " << timeMs << "ms";
  } else {
    LOG_TOPIC("bce4f", DEBUG, iresearch::TOPIC)
        << "error after running for " << timeMs << "ms while consolidating arangosearch link '"
        << linkPtr->id() << "', run id '" << size_t(&runId)
        << "': " << res.errorNumber() << " " << res.errorMessage();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     IResearchLink
// -----------------------------------------------------------------------------

AsyncLinkHandle::AsyncLinkHandle(IResearchLink* link) : _link{link} {
  sLinksCount.fetch_add(1, std::memory_order_release);
}

AsyncLinkHandle::~AsyncLinkHandle() {
  sLinksCount.fetch_sub(1, std::memory_order_release);
}

void AsyncLinkHandle::reset() {
  _asyncTerminate.store(true, std::memory_order_release);  // mark long-running async jobs for termination
  _link.reset();  // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)
}

IResearchLink::IResearchLink(IndexId iid, LogicalCollection& collection)
    : _engine{nullptr},
      _asyncFeature{&collection.vocbase().server().getFeature<IResearchFeature>()},
      _asyncSelf{std::make_shared<AsyncLinkHandle>(nullptr)},  // mark as data store not initialized
      _collection{collection},
      _maintenanceState{std::make_shared<MaintenanceState>()},
      _id{iid},
      _lastCommittedTick{0},
      _cleanupIntervalCount{0},
      _createdInRecovery{false},
      _linkStats{nullptr},
      _numFailedCommits{nullptr},
      _numFailedCleanups{nullptr},
      _numFailedConsolidations{nullptr},
      _commitTimeNum{0},
      _avgCommitTimeMs{nullptr},
      _cleanupTimeNum{0},
      _avgCleanupTimeMs{nullptr},
      _consolidationTimeNum{0},
      _avgConsolidationTimeMs{nullptr} {
  auto* key = this;

  // initialize transaction callback
  _trxCallback = [key](transaction::Methods& trx, transaction::Status status) {
    auto* state = trx.state();
    TRI_ASSERT(state != nullptr);

    // check state of the top-most transaction only
    if (!state) {
      return;  // NOOP
    }

    auto prev = state->cookie(key, nullptr);  // get existing cookie

    if (prev) {
// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      auto& ctx = dynamic_cast<LinkTrxState&>(*prev);
#else
      auto& ctx = static_cast<LinkTrxState&>(*prev);
#endif

      if (transaction::Status::COMMITTED != status) {  // rollback
        ctx.reset();
      } else {
        ctx._ctx.tick(state->lastOperationTick());
      }
    }

    prev.reset();
  };
}

IResearchLink::~IResearchLink() {
  Result res;
  try {
    res = unload();  // disassociate from view if it has not been done yet
  } catch (...) {
  }

  if (!res.ok()) {
    LOG_TOPIC("2b41f", ERR, iresearch::TOPIC)
        << "failed to unload arangosearch link in link destructor: "
        << res.errorNumber() << " " << res.errorMessage();
  }
}

bool IResearchLink::operator==(LogicalView const& view) const noexcept {
  return _viewGuid == view.guid();
}

bool IResearchLink::operator==(IResearchLinkMeta const& meta) const noexcept {
  return _meta == meta;
}

void IResearchLink::afterTruncate(TRI_voc_tick_t tick, transaction::Methods* trx) {
  auto lock = _asyncSelf->lock();  // '_dataStore' can be asynchronously modified

  bool ok{false};
  auto computeMetrics = irs::make_finally([&]() noexcept {
    // We don't measure time because we believe that it should tend to zero
    if (!ok && _numFailedCommits != nullptr) {
      _numFailedCommits->fetch_add(1, std::memory_order_relaxed);
    }
  });

  TRI_IF_FAILURE("ArangoSearchTruncateFailure") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (!_asyncSelf) {
    // the current link is no longer valid (checked after ReadLock acquisition)
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
                                   "failed to lock arangosearch link while "
                                   "truncating arangosearch link '" +
                                       std::to_string(id().id()) + "'");
  }

  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  if (trx != nullptr) {
    auto* key = this;

    auto& state = *(trx->state());

    // TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* ctx = dynamic_cast<LinkTrxState*>(state.cookie(key));
#else
    auto* ctx = static_cast<LinkTrxState*>(state.cookie(key));
#endif

    if (ctx) {
      ctx->reset();  // throw away all pending operations as clear will overwrite them all
      state.cookie(key, nullptr);  // force active segment release to allow commit go and avoid deadlock in clear
    }
  }

  auto const lastCommittedTick = _lastCommittedTick;
  bool recoverCommittedTick = true;

  auto lastCommittedTickGuard =
      irs::make_finally([lastCommittedTick, this, &recoverCommittedTick] {
        if (recoverCommittedTick) {
          _lastCommittedTick = lastCommittedTick;
        }
      });

  try {
    _dataStore._writer->clear(tick);
    recoverCommittedTick = false;  //_lastCommittedTick now updated and data is written to storage

    // get new reader
    auto reader = _dataStore._reader.reopen();

    if (!reader) {
      // nothing more to do
      LOG_TOPIC("1c2c1", WARN, iresearch::TOPIC)
          << "failed to update snapshot after truncate "
          << ", reuse the existing snapshot for arangosearch link '" << id() << "'";
      return;
    }

    // update reader
    _dataStore._reader = reader;

    _linkStats->store(statsUnsafe());

    auto subscription = std::atomic_load(&_flushSubscription);

    if (subscription) {
      auto& impl = static_cast<IResearchFlushSubscription&>(*subscription);

      impl.tick(_lastCommittedTick);
    }
    ok = true;
  } catch (std::exception const& e) {
    LOG_TOPIC("a3c57", ERR, iresearch::TOPIC)
        << "caught exception while truncating arangosearch link '" << id()
        << "': " << e.what();
    throw;
  } catch (...) {
    LOG_TOPIC("79a7d", WARN, iresearch::TOPIC)
        << "caught exception while truncating arangosearch link '" << id() << "'";
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
IResearchLink::UnsafeOpResult IResearchLink::cleanupUnsafe() {
  auto begin = std::chrono::steady_clock::now();
  auto result = cleanupUnsafeImpl();
  uint64_t timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - begin)
                        .count();
  if (bool ok = result.ok(); ok && _avgCleanupTimeMs != nullptr) {
    _avgCleanupTimeMs->store(computeAvg(_cleanupTimeNum, timeMs), std::memory_order_relaxed);
  } else if (!ok && _numFailedCleanups != nullptr) {
    _numFailedCleanups->fetch_add(1, std::memory_order_relaxed);
  }
  return {result, timeMs};
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchLink::cleanupUnsafeImpl() {
  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  try {
    irs::directory_utils::remove_all_unreferenced(*(_dataStore._directory));
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while cleaning up arangosearch link '" +
                std::to_string(id().id()) + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while cleaning up arangosearch link '" +
                std::to_string(id().id()) + "'"};
  }
  return {};
}

Result IResearchLink::commit(bool wait /*= true*/) {
  auto lock = _asyncSelf->lock();  // '_dataStore' can be asynchronously modified

  if (!_asyncSelf.get()) {
    // the current link is no longer valid (checked after ReadLock acquisition)

    return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
            "failed to lock arangosearch link while committing arangosearch "
            "link '" +
                std::to_string(id().id()) + "'"};
  }

  CommitResult code{CommitResult::UNDEFINED};
  auto result = commitUnsafe(wait, &code).result;
  READ_LOCKER(metaLock, _dataStore._mutex);  // '_meta' can be asynchronously modified
  if (auto& meta = _dataStore._meta; meta._commitIntervalMsec == 0) {
    // If auto commit is disabled, we want to manually trigger the cleanup for the consistent API
    if (meta._cleanupIntervalStep != 0 && ++_cleanupIntervalCount >= meta._cleanupIntervalStep) {
      metaLock.unlock();
      _cleanupIntervalCount = 0;
      (void)cleanupUnsafe();
    }
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
IResearchLink::UnsafeOpResult IResearchLink::commitUnsafe(bool wait, CommitResult* code) {
  auto begin = std::chrono::steady_clock::now();
  auto result = commitUnsafeImpl(wait, code);
  uint64_t timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - begin)
                        .count();

  if (bool ok = result.ok(); !ok && _numFailedCommits != nullptr) {
    _numFailedCommits->fetch_add(1, std::memory_order_relaxed);
  } else if (ok && *code == CommitResult::DONE && _avgCommitTimeMs != nullptr) {
    _avgCommitTimeMs->store(computeAvg(_commitTimeNum, timeMs), std::memory_order_relaxed);
  }
  return {result, timeMs};
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchLink::commitUnsafeImpl(bool wait, CommitResult* code) {
  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  auto subscription = std::atomic_load(&_flushSubscription);

  if (!subscription) {
    // already released
    *code = CommitResult::NO_CHANGES;
    return {};
  }

  auto& impl = static_cast<IResearchFlushSubscription&>(*subscription);

  try {
    auto const lastTickBeforeCommit = _engine->currentTick();

    auto commitLock = irs::make_unique_lock(_commitMutex, std::try_to_lock);

    if (!commitLock.owns_lock()) {
      if (!wait) {
        LOG_TOPIC("37bcc", TRACE, iresearch::TOPIC)
            << "commit for arangosearch link '" << id()
            << "' is already in progress, skipping";

        *code = CommitResult::IN_PROGRESS;
        return {};
      }

      LOG_TOPIC("37bca", TRACE, iresearch::TOPIC)
          << "commit for arangosearch link '" << id()
          << "' is already in progress, waiting";

      commitLock.lock();
    }

    auto const lastCommittedTick = _lastCommittedTick;

    try {
      // _lastCommittedTick is being updated in '_before_commit'
      *code = _dataStore._writer->commit() ? CommitResult::DONE : CommitResult::NO_CHANGES;
    } catch (...) {
      // restore last committed tick in case of any error
      _lastCommittedTick = lastCommittedTick;
      throw;
    }

    if (CommitResult::NO_CHANGES == *code) {
      LOG_TOPIC("7e319", TRACE, iresearch::TOPIC)
          << "no changes registered for arangosearch link '" << id()
          << "' got last operation tick '" << _lastCommittedTick << "'";

      // no changes, can release the latest tick before commit
      impl.tick(lastTickBeforeCommit);

      return {};
    }

    // get new reader
    auto reader = _dataStore._reader.reopen();

    if (!reader) {
      // nothing more to do
      LOG_TOPIC("37bcf", WARN, iresearch::TOPIC)
          << "failed to update snapshot after commit, reuse "
             "the existing snapshot for arangosearch link '"
          << id() << "'";

      return {};
    }

    // update reader
    TRI_ASSERT(_dataStore._reader != reader);
    _dataStore._reader = reader;

    // update stats of the link
    _linkStats->store(statsUnsafe());

    // update last committed tick
    impl.tick(_lastCommittedTick);

    // invalidate query cache
    aql::QueryCache::instance()->invalidate(&(_collection.vocbase()), _viewGuid);

    LOG_TOPIC("7e328", DEBUG, iresearch::TOPIC)
        << "successful sync of arangosearch link '" << id() << "', segments '"
        << reader->size() << "', docs count '" << reader->docs_count()
        << "', live docs count '" << reader->docs_count()
        << "', live docs count '" << reader->live_docs_count()
        << "', last operation tick '" << _lastCommittedTick << "'";
  } catch (basics::Exception const& e) {
    return {e.code(), "caught exception while committing arangosearch link '" +
                          std::to_string(id().id()) + "': " + e.what()};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while committing arangosearch link '" +
                std::to_string(id().id()) + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while committing arangosearch link '" +
                std::to_string(id().id())};
  }
  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
IResearchLink::UnsafeOpResult IResearchLink::consolidateUnsafe(
    IResearchViewMeta::ConsolidationPolicy const& policy,
    irs::merge_writer::flush_progress_t const& progress, bool& emptyConsolidation) {
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
Result IResearchLink::consolidateUnsafeImpl(IResearchViewMeta::ConsolidationPolicy const& policy,
                                            irs::merge_writer::flush_progress_t const& progress,
                                            bool& emptyConsolidation) {
  emptyConsolidation = false;  // TODO Why?

  if (!policy.policy()) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        "unset consolidation policy while executing consolidation policy '" +
            policy.properties().toString() + "' on arangosearch link '" +
            std::to_string(id().id()) + "'"};
  }

  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  try {
    auto const res = _dataStore._writer->consolidate(policy.policy(), nullptr, progress);
    if (!res) {
      return {TRI_ERROR_INTERNAL,
              "failure while executing consolidation policy '" +
                  policy.properties().toString() + "' on arangosearch link '" +
                  std::to_string(id().id()) + "'"};
    }

    emptyConsolidation = (res.size == 0);
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while executing consolidation policy '" +
                policy.properties().toString() + "' on arangosearch link '" +
                std::to_string(id().id()) + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while executing consolidation policy '" +
                policy.properties().toString() + "' on arangosearch link '" +
                std::to_string(id().id()) + "'"};
  }
  return {};
}

Result IResearchLink::drop() {
  // the lookup and unlink is valid for single-server only (that is the only scenario where links are persisted)
  // on coordinator and db-server the IResearchView is immutable and lives in ClusterInfo
  // therefore on coordinator and db-server a new plan will already have an IResearchView without the link
  // this avoids deadlocks with ClusterInfo::loadPlan() during lookup in ClusterInfo
  if (ServerState::instance()->isSingleServer()) {
    auto logicalView = _collection.vocbase().lookupView(_viewGuid);
    auto* view = LogicalView::cast<IResearchView>(logicalView.get());

    // may occur if the link was already unlinked from the view via another instance
    // this behavior was seen user-access-right-drop-view-arangosearch-spec.js
    // where the collection drop was called through REST,
    // the link was dropped as a result of the collection drop call
    // then the view was dropped via a separate REST call
    // then the vocbase was destroyed calling
    // collection close()-> link unload() -> link drop() due to collection marked as dropped
    // thus returning an error here will cause ~TRI_vocbase_t() on RocksDB to
    // receive an exception which is not handled in the destructor
    // the reverse happens during drop of a collection with MMFiles
    // i.e. collection drop() -> collection close()-> link unload(), then link drop()
    if (!view) {
      LOG_TOPIC("f4e2c", WARN, iresearch::TOPIC)
          << "unable to find arangosearch view '" << _viewGuid
          << "' while dropping arangosearch link '" << _id.id() << "'";
    } else {
      view->unlink(_collection.id());  // unlink before reset() to release lock in view (if any)
    }
  }

  std::atomic_store(&_flushSubscription, {});  // reset together with '_asyncSelf'
  _asyncSelf->reset();  // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  if (_dataStore) {
    removeStats();
  }

  try {
    if (_dataStore) {
      _dataStore.resetDataStore();
    }

    bool exists{false};

    // remove persisted data store directory if present
    if (!irs::file_utils::exists_directory(exists, _dataStore._path.c_str()) ||
        (exists && !irs::file_utils::remove(_dataStore._path.c_str()))) {
      return {TRI_ERROR_INTERNAL, "failed to remove arangosearch link '" +
                                      std::to_string(id().id()) + "'"};
    }
  } catch (basics::Exception& e) {
    return {e.code(), "caught exception while removing arangosearch link '" +
                          std::to_string(id().id()) + "': " + e.what()};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while removing arangosearch link '" +
                std::to_string(id().id()) + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while removing arangosearch link '" +
                std::to_string(id().id()) + "'"};
  }

  return {};
}

bool IResearchLink::hasSelectivityEstimate() {
  return false;  // selectivity can only be determined per query since multiple fields are indexed
}

Result IResearchLink::init(velocypack::Slice const& definition,
                           InitCallback const& initCallback) {
  // disassociate from view if it has not been done yet
  if (!unload().ok()) {
    return {TRI_ERROR_INTERNAL, "failed to unload link"};
  }

  std::string error;
  IResearchLinkMeta meta;

  // definition should already be normalized and analyzers created if required
  if (!meta.init(_collection.vocbase().server(), definition, true, error,
                 _collection.vocbase().name())) {
    return {TRI_ERROR_BAD_PARAMETER,
            "error parsing view link parameters from json: " + error};
  }

  if (!definition.isObject()  // not object
      || !definition.get(StaticStrings::ViewIdField).isString()) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            "error finding view for link '" + std::to_string(_id.id()) + "'"};
  }

  auto viewId = definition.get(StaticStrings::ViewIdField).copyString();
  auto& vocbase = _collection.vocbase();
  bool const sorted = !meta._sort.empty();
  auto const& storedValuesColumns = meta._storedValues.columns();
  TRI_ASSERT(meta._sortCompression);
  auto const primarySortCompression =
      meta._sortCompression ? meta._sortCompression : getDefaultCompression();
  bool clusterWideLink{true};
  if (ServerState::instance()->isCoordinator()) {  // coordinator link
    if (!vocbase.server().hasFeature<ClusterFeature>()) {
      return {
          TRI_ERROR_INTERNAL,
          "failure to get cluster info while initializing arangosearch link '" +
              std::to_string(_id.id()) + "'"};
    }
    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();

    auto logicalView = ci.getView(vocbase.name(), viewId);

    // if there is no logicalView present yet then skip this step
    if (logicalView) {
      if (iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,  // code
                "error finding view: '" + viewId + "' for link '" +
                    std::to_string(_id.id()) + "' : no such view"};
      }

      auto* view = LogicalView::cast<IResearchViewCoordinator>(logicalView.get());

      if (!view) {
        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                "error finding view: '" + viewId + "' for link '" +
                    std::to_string(_id.id()) + "'"};
      }

      viewId = view->guid();  // ensue that this is a GUID (required by operator==(IResearchView))

      // required for IResearchViewCoordinator which calls IResearchLink::properties(...)
      std::swap(const_cast<IResearchLinkMeta&>(_meta), meta);
      auto revert = irs::make_finally([this, &meta] {
        std::swap(const_cast<IResearchLinkMeta&>(_meta), meta);
      });

      auto res = view->link(*this);

      if (!res.ok()) {
        return res;
      }
    }
  } else if (ServerState::instance()->isDBServer()) {  // db-server link
    if (!vocbase.server().hasFeature<ClusterFeature>()) {
      return {
          TRI_ERROR_INTERNAL,
          "failure to get cluster info while initializing arangosearch link '" +
              std::to_string(_id.id()) + "'"};
    }
    if (vocbase.server().getFeature<ClusterFeature>().isEnabled()) {
      auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();

      clusterWideLink = _collection.id() == _collection.planId() && _collection.isAStub();

      // upgrade step for old link definition without collection name
      // this could be received from  agency while shard of the collection was moved (or added)
      // to the server.
      // New links already has collection name set, but here we must get this name on our own
      if (meta._collectionName.empty()) {
        if (clusterWideLink) {  // could set directly
          LOG_TOPIC("86ecd", TRACE, iresearch::TOPIC)
              << "Setting collection name '" << _collection.name()
              << "' for new link '" << this->id().id() << "'";
          meta._collectionName = _collection.name();
        } else {
          meta._collectionName = ci.getCollectionNameForShard(_collection.name());
          LOG_TOPIC("86ece", TRACE, iresearch::TOPIC)
              << "Setting collection name '" << meta._collectionName
              << "' for new link '" << this->id().id() << "'";
        }
        if (ADB_UNLIKELY(meta._collectionName.empty())) {
          LOG_TOPIC("67da6", WARN, iresearch::TOPIC)
              << "Failed to init collection name for the link '"
              << this->id().id() << "'. Link will not index '_id' attribute. Please recreate the link if this is necessary!";
        }

#ifdef USE_ENTERPRISE
        // enterprise name is not used in _id so should not be here!
        if (ADB_LIKELY(!meta._collectionName.empty())) {
          arangodb::ClusterMethods::realNameFromSmartName(meta._collectionName);
        }
#endif
      }

      if (!clusterWideLink) {
        // prepare data-store which can then update options
        // via the IResearchView::link(...) call
        auto res = initDataStore(initCallback, meta._version, sorted,
                                 storedValuesColumns, primarySortCompression);

        if (!res.ok()) {
          return res;
        }
      }

      // valid to call ClusterInfo (initialized in ClusterFeature::prepare()) even from DatabaseFeature::start()
      auto logicalView = ci.getView(vocbase.name(), viewId);

      // if there is no logicalView present yet then skip this step
      if (logicalView) {
        if (iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
          unload();  // unlock the data store directory
          return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "error finding view: '" + viewId + "' for link '" +
                      std::to_string(_id.id()) + "' : no such view"};
        }

        auto* view = LogicalView::cast<IResearchView>(logicalView.get());

        if (!view) {
          unload();  // unlock the data store directory
          return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "error finding view: '" + viewId + "' for link '" +
                      std::to_string(_id.id()) + "'"};
        }

        viewId = view->guid();  // ensue that this is a GUID (required by operator==(IResearchView))

        if (clusterWideLink) {  // cluster cluster-wide link
          auto shardIds = _collection.shardIds();

          // go through all shard IDs of the collection and try to link any
          // links missing links will be populated when they are created in the
          // per-shard collection
          if (shardIds) {
            for (auto& entry : *shardIds) {
              auto collection = vocbase.lookupCollection(
                  entry.first);  // per-shard collections are always in 'vocbase'

              if (!collection) {
                continue;  // missing collection should be created after Plan becomes Current
              }

              auto link = IResearchLinkHelper::find(*collection, *view);

              if (link) {
                auto res = view->link(link->self());

                if (!res.ok()) {
                  return res;
                }
              }
            }
          }
        } else {  // cluster per-shard link
          auto res = view->link(_asyncSelf);

          if (!res.ok()) {
            unload();  // unlock the data store directory

            return res;
          }
        }
      }
    } else {
      LOG_TOPIC("67dd6", DEBUG, iresearch::TOPIC)
          << "Skipped link '" << this->id().id() << "' due to disabled cluster features.";
    }
  } else if (ServerState::instance()->isSingleServer()) {  // single-server link
    // prepare data-store which can then update options
    // via the IResearchView::link(...) call
    auto res = initDataStore(initCallback, meta._version, sorted,
                             storedValuesColumns, primarySortCompression);

    if (!res.ok()) {
      return res;
    }

    auto logicalView = vocbase.lookupView(viewId);

    // if there is no logicalView present yet then skip this step
    if (logicalView) {
      if (iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
        unload();  // unlock the data store directory

        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                "error finding view: '" + viewId + "' for link '" +
                    std::to_string(_id.id()) + "' : no such view"};
      }

      auto* view = LogicalView::cast<IResearchView>(logicalView.get());

      if (!view) {
        unload();  // unlock the data store directory

        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                "error finding view: '" + viewId + "' for link '" +
                    std::to_string(_id.id()) + "'"};
      }

      viewId = view->guid();  // ensue that this is a GUID (required by operator==(IResearchView))

      auto linkRes = view->link(_asyncSelf);

      if (!linkRes.ok()) {
        unload();  // unlock the directory

        return linkRes;
      }
    }
  }

  const_cast<std::string&>(_viewGuid) = std::move(viewId);
  const_cast<IResearchLinkMeta&>(_meta) = std::move(meta);
  _comparer.reset(_meta._sort);

  // we should create stats for link only for Single Server or
  // of for DB Server. But in case of DB Server we must check that
  // link created for actual dataStore and not for ClusterInfo
  if (ServerState::instance()->isSingleServer() || !clusterWideLink) {
    auto& metric = _collection.vocbase().server().getFeature<metrics::MetricsFeature>();
    auto builder = getMetric<metrics::BatchBuilder<LinkStats>>(*this);
    builder.setName("arangosearch_link_stats");
    _linkStats = &metric.add(std::move(builder));
    _numFailedCommits = &metric.add(getMetric<arangosearch_num_failed_commits>(*this));
    _numFailedCleanups = &metric.add(getMetric<arangosearch_num_failed_cleanups>(*this));
    _numFailedConsolidations =
        &metric.add(getMetric<arangosearch_num_failed_consolidations>(*this));
    _avgCommitTimeMs = &metric.add(getMetric<arangosearch_commit_time>(*this));
    _avgCleanupTimeMs = &metric.add(getMetric<arangosearch_cleanup_time>(*this));
    _avgConsolidationTimeMs =
        &metric.add(getMetric<arangosearch_consolidation_time>(*this));
  }

  return {};
}

Result IResearchLink::initDataStore(InitCallback const& initCallback,
                                    uint32_t version, bool sorted,
                                    std::vector<IResearchViewStoredValues::StoredColumn> const& storedColumns,
                                    irs::type_info::type_id primarySortCompression) {
  std::atomic_store(&_flushSubscription, {});  // reset together with '_asyncSelf'
  _asyncSelf->reset();  // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  auto& server = _collection.vocbase().server();
  if (!server.hasFeature<DatabasePathFeature>()) {
    return {TRI_ERROR_INTERNAL,
            "failure to find feature 'DatabasePath' while initializing link '" +
                std::to_string(_id.id()) + "'"};
  }
  if (!server.hasFeature<FlushFeature>()) {
    return {TRI_ERROR_INTERNAL,
            "failure to find feature 'FlushFeature' while initializing link '" +
                std::to_string(_id.id()) + "'"};
  }

  auto& dbPathFeature = server.getFeature<DatabasePathFeature>();
  auto& flushFeature = server.getFeature<FlushFeature>();

  auto const formatId = getFormat(LinkVersion{version});
  auto format = irs::formats::get(formatId);

  if (!format) {
    return {TRI_ERROR_INTERNAL,
            "failed to get data store codec '"s + formatId.data() +
                "' while initializing link '" + std::to_string(_id.id()) + "'"};
  }

  _engine = &server.getFeature<EngineSelectorFeature>().engine();

  bool pathExists{false};

  _dataStore._path = getPersistedPath(dbPathFeature, *this);

  // must manually ensure that the data store directory exists (since not using
  // a lockfile)
  if (irs::file_utils::exists_directory(pathExists, _dataStore._path.c_str()) &&
      !pathExists && !irs::file_utils::mkdir(_dataStore._path.c_str(), true)) {
    return {TRI_ERROR_CANNOT_CREATE_DIRECTORY,
            "failed to create data store directory with path '" +
                _dataStore._path.string() + "' while initializing link '" +
                std::to_string(_id.id()) + "'"};
  }
  if (initCallback) {
    _dataStore._directory =
        std::make_unique<irs::mmap_directory>(_dataStore._path.u8string(), initCallback());
  } else {
    _dataStore._directory =
        std::make_unique<irs::mmap_directory>(_dataStore._path.u8string());
  }

  if (!_dataStore._directory) {
    return {TRI_ERROR_INTERNAL,
            "failed to instantiate data store directory with path '" +
                _dataStore._path.string() + "' while initializing link '" +
                std::to_string(_id.id()) + "'"};
  }

  switch (_engine->recoveryState()) {
    case RecoveryState::BEFORE:  // link is being opened before recovery
    case RecoveryState::DONE: {  // link is being created after recovery
      _dataStore._inRecovery.store(true, std::memory_order_release);  // will be adjusted in post-recovery callback
      _dataStore._recoveryTick = _engine->recoveryTick();
      break;
    }

    case RecoveryState::IN_PROGRESS: {  // link is being created during recovery
      // both MMFiles and RocksDB will fill out link based on
      // actual data in linked collections, we can treat recovery as done
      _createdInRecovery = true;

      _dataStore._inRecovery.store(false, std::memory_order_release);
      _dataStore._recoveryTick = _engine->releasedTick();
      break;
    }
  }

  if (pathExists) {
    try {
      _dataStore._reader = irs::directory_reader::open(*(_dataStore._directory));

      if (!::readTick(_dataStore._reader.meta().meta.payload(), _dataStore._recoveryTick)) {
        return {TRI_ERROR_INTERNAL,
                "failed to get last committed tick while initializing link '" +
                    std::to_string(id().id()) + "'"};
      }

      LOG_TOPIC("7e028", TRACE, iresearch::TOPIC)
          << "successfully opened existing data store data store reader for "
          << "link '" << id() << "', docs count '" << _dataStore._reader->docs_count()
          << "', live docs count '" << _dataStore._reader->live_docs_count()
          << "', recovery tick '" << _dataStore._recoveryTick << "'";
    } catch (irs::index_not_found const&) {
      // NOOP
    }
  }

  _lastCommittedTick = _dataStore._recoveryTick;
  _flushSubscription =
      std::make_shared<IResearchFlushSubscription>(_dataStore._recoveryTick);

  irs::index_writer::init_options options;
  options.lock_repository = false;  // do not lock index, ArangoDB has its own lock
  options.comparator = sorted ? &_comparer : nullptr;  // set comparator if requested
  options.features[irs::type<irs::granularity_prefix>::id()] = nullptr;
  if (LinkVersion(version) < LinkVersion::MAX) {
    options.features[irs::type<irs::norm>::id()] = &irs::norm::compute;
  } else {
    options.features[irs::type<irs::norm2>::id()] = &irs::norm2::compute;
  }
  // initialize commit callback
  options.meta_payload_provider = [this](uint64_t tick, irs::bstring& out) {
    _lastCommittedTick = std::max(_lastCommittedTick, TRI_voc_tick_t(tick));  // update last tick
    tick = irs::numeric_utils::hton64(uint64_t(_lastCommittedTick));  // convert to BE

    out.append(reinterpret_cast<irs::byte_type const*>(&tick), sizeof(uint64_t));

    return true;
  };

  // as meta is still not filled at this moment
  // we need to store all compression mapping there
  // as values provided may be temporary
  std::map<std::string, irs::type_info::type_id> compressionMap;
  for (auto const& c : storedColumns) {
    if (ADB_LIKELY(c.compression != nullptr)) {
      compressionMap.emplace(c.name, c.compression);
    } else {
      TRI_ASSERT(false);
      compressionMap.emplace(c.name, getDefaultCompression());
    }
  }
  // setup columnstore compression/encryption if requested by storage engine
  auto const encrypt = (nullptr != _dataStore._directory->attributes().encryption());
  options.column_info = [encrypt, compressionMap = std::move(compressionMap),
                         primarySortCompression](const irs::string_ref& name) -> irs::column_info {
    if (name.null()) {
      return {primarySortCompression(), {}, encrypt};
    }
    auto compress = compressionMap.find(static_cast<std::string>(name));  // FIXME: remove cast after C++20
    if (compress != compressionMap.end()) {
      // do not waste resources to encrypt primary key column
      return {compress->second(), {}, encrypt && (DocumentPrimaryKey::PK() != name)};
    }
    return {getDefaultCompression()(), {}, encrypt && (DocumentPrimaryKey::PK() != name)};
  };

  auto openFlags = irs::OM_APPEND;
  if (!_dataStore._reader) {
    openFlags |= irs::OM_CREATE;
  }

  _dataStore._writer =
      irs::index_writer::make(*(_dataStore._directory), format, openFlags, options);

  if (!_dataStore._writer) {
    return {TRI_ERROR_INTERNAL,
            "failed to instantiate data store writer with path '" +
                _dataStore._path.string() + "' while initializing link '" +
                std::to_string(_id.id()) + "'"};
  }

  if (!_dataStore._reader) {
    _dataStore._writer->commit();  // initialize 'store'
    _dataStore._reader = irs::directory_reader::open(*(_dataStore._directory));
  }

  if (!_dataStore._reader) {
    _dataStore._writer.reset();

    return {TRI_ERROR_INTERNAL,
            "failed to instantiate data store reader with path '" +
                _dataStore._path.string() + "' while initializing link '" +
                std::to_string(_id.id()) + "'"};
  }

  if (!::readTick(_dataStore._reader.meta().meta.payload(), _dataStore._recoveryTick)) {
    return {TRI_ERROR_INTERNAL,
            "failed to get last committed tick while initializing link '" +
                std::to_string(id().id()) + "'"};
  }

  LOG_TOPIC("7e128", TRACE, iresearch::TOPIC)
      << "data store reader for link '" << id()
      << "' is initialized with recovery tick '" << _dataStore._recoveryTick << "'";

  // reset data store meta, will be updated at runtime via properties(...)
  _dataStore._meta._cleanupIntervalStep = 0;        // 0 == disable
  _dataStore._meta._commitIntervalMsec = 0;         // 0 == disable
  _dataStore._meta._consolidationIntervalMsec = 0;  // 0 == disable
  _dataStore._meta._consolidationPolicy = IResearchViewMeta::ConsolidationPolicy();  // disable
  _dataStore._meta._writebufferActive = options.segment_count_max;
  _dataStore._meta._writebufferIdle = options.segment_pool_size;
  _dataStore._meta._writebufferSizeMax = options.segment_memory_max;

  // create a new 'self' (previous was reset during unload() above)
  _asyncSelf = std::make_shared<AsyncLinkHandle>(this);

  // ...........................................................................
  // set up in-recovery insertion hooks
  // ...........................................................................

  if (!server.hasFeature<DatabaseFeature>()) {
    return {};  // nothing more to do
  }
  auto& dbFeature = server.getFeature<DatabaseFeature>();

  return dbFeature.registerPostRecoveryCallback(  // register callback
      [asyncSelf = _asyncSelf, &flushFeature]() -> Result {
        auto link = asyncSelf->lock();  // ensure link does not get deallocated before callback finishes

        if (!link) {
          return {};  // link no longer in recovery state, i.e. during recovery it was created and later dropped
        }

        if (!link->_flushSubscription) {
          return {
              TRI_ERROR_INTERNAL,
              "failed to register flush subscription for arangosearch link '" +
                  std::to_string(link->id().id()) + "'"};
        }

        auto& dataStore = link->_dataStore;

        if (dataStore._recoveryTick > link->_engine->recoveryTick()) {
          LOG_TOPIC("5b59f", WARN, iresearch::TOPIC)
              << "arangosearch link '" << link->id() << "' is recovered at tick '"
              << dataStore._recoveryTick << "' less than storage engine tick '"
              << link->_engine->recoveryTick() << "', it seems WAL tail was lost and link '"
              << link->id() << "' is out of sync with the underlying collection '"
              << link->collection().name() << "', consider to re-create the link in order to synchronize them.";
        }

        // recovery finished
        dataStore._inRecovery.store(link->_engine->inRecovery(), std::memory_order_release);

        LOG_TOPIC("5b59c", TRACE, iresearch::TOPIC)
            << "starting sync for arangosearch link '" << link->id() << "'";

        CommitResult code{CommitResult::UNDEFINED};
        auto [res, timeMs] = link->commitUnsafe(true, &code);

        LOG_TOPIC("0e0ca", TRACE, iresearch::TOPIC)
            << "finished sync for arangosearch link '" << link->id() << "'";

        // register flush subscription
        flushFeature.registerFlushSubscription(link->_flushSubscription);

        // setup asynchronous tasks for commit, cleanup if enabled
        if (dataStore._meta._commitIntervalMsec) {
          link->scheduleCommit(0ms);
        }

        // setup asynchronous tasks for consolidation if enabled
        if (dataStore._meta._consolidationIntervalMsec) {
          link->scheduleConsolidation(0ms);
        }

        return res;
      });
}

void IResearchLink::scheduleCommit(std::chrono::milliseconds delay) {
  CommitTask task;
  task.link = _asyncSelf;
  task.async = _asyncFeature;
  task.id = id();
  task.state = _maintenanceState;

  _maintenanceState->pendingCommits.fetch_add(1, std::memory_order_release);
  task.schedule(delay);
}

void IResearchLink::scheduleConsolidation(std::chrono::milliseconds delay) {
  ConsolidationTask task;
  task.link = _asyncSelf;
  task.async = _asyncFeature;
  task.id = id();
  task.state = _maintenanceState;
  task.progress = [link = _asyncSelf.get()] {
    return !link->terminationRequested();
  };

  _maintenanceState->pendingConsolidations.fetch_add(1, std::memory_order_release);
  task.schedule(delay);
}

Result IResearchLink::insert(transaction::Methods& trx,
                             LocalDocumentId documentId, velocypack::Slice doc) {
  TRI_ASSERT(_engine);
  TRI_ASSERT(trx.state());

  auto& state = *(trx.state());

  if (_dataStore._inRecovery.load(std::memory_order_acquire) &&
      _engine->recoveryTick() <= _dataStore._recoveryTick) {
    LOG_TOPIC("7c228", TRACE, iresearch::TOPIC)
        << "skipping 'insert', operation tick '" << _engine->recoveryTick()
        << "', recovery tick '" << _dataStore._recoveryTick << "'";

    return {};
  }

  auto insertImpl = [this, doc, documentId, &trx]  //
      (irs::index_writer::documents_context & ctx) -> Result {
    try {
      FieldIterator body(trx, _meta._collectionName, _id);

      return insertDocument(ctx, trx, body, doc, documentId, _meta, id());
    } catch (basics::Exception const& e) {
      return {
          e.code(),
          "caught exception while inserting document into arangosearch link '" +
              std::to_string(id().id()) + "', revision '" +
              std::to_string(documentId.id()) + "': " + e.what()};
    } catch (std::exception const& e) {
      return {
          TRI_ERROR_INTERNAL,
          "caught exception while inserting document into arangosearch link '" +
              std::to_string(id().id()) + "', revision '" +
              std::to_string(documentId.id()) + "': " + e.what()};
    } catch (...) {
      return {
          TRI_ERROR_INTERNAL,
          "caught exception while inserting document into arangosearch link '" +
              std::to_string(id().id()) + "', revision '" +
              std::to_string(documentId.id()) + "'"};
    }
  };

  TRI_IF_FAILURE("ArangoSearch::BlockInsertsWithoutIndexCreationHint") {
    if (!state.hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
      return {TRI_ERROR_DEBUG};
    }
  }

  if (state.hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
    auto lock = _asyncSelf->lock();
    auto ctx = _dataStore._writer->documents();

    TRI_IF_FAILURE("ArangoSearch::MisreportCreationInsertAsFailed") {
      auto res = insertImpl(ctx);  // we need insert to succeed, so  we have things to clean up in storage
      if (res.fail()) {
        return res;
      }
      return {TRI_ERROR_DEBUG};
    }
    return insertImpl(ctx);
  }
  auto* key = this;

// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* ctx = dynamic_cast<LinkTrxState*>(state.cookie(key));
#else
  auto* ctx = static_cast<LinkTrxState*>(state.cookie(key));
#endif

  if (!ctx) {
    // '_dataStore' can be asynchronously modified
    auto lock = _asyncSelf->lock();

    if (!_asyncSelf.get()) {
      // the current link is no longer valid (checked after ReadLock acquisition)
      return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
              "failed to lock arangosearch link while inserting a "
              "document into arangosearch link '" +
                  std::to_string(id().id()) + "'"};
    }

    TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

    // FIXME try to preserve optimization
    //// optimization for single-document insert-only transactions
    // if (trx.isSingleOperationTransaction()  // only for single-document transactions
    //     && !_dataStore._inRecovery.load(std::memory_order_acquire)) {
    //   auto ctx = _dataStore._writer->documents();
    //   return insertImpl(ctx);
    // }

    auto ptr = std::make_unique<LinkTrxState>(std::move(lock), *(_dataStore._writer));

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx || !trx.addStatusChangeCallback(&_trxCallback)) {
      return {TRI_ERROR_INTERNAL,
              "failed to store state into a TransactionState for insert into "
              "arangosearch link '" +
                  std::to_string(id().id()) + "', tid '" +
                  std::to_string(state.id().id()) + "', revision '" +
                  std::to_string(documentId.id()) + "'"};
    }
  }

  return insertImpl(ctx->_ctx);
}

bool IResearchLink::isHidden() {
  return !ServerState::instance()->isDBServer();  // hide links unless we are on a DBServer
}

bool IResearchLink::isSorted() {
  return false;  // IResearch does not provide a fixed default sort order
}

void IResearchLink::load() {
  // Note: this function is only used by RocksDB
}

bool IResearchLink::matchesDefinition(velocypack::Slice slice) const {
  if (!slice.isObject() || !slice.hasKey(StaticStrings::ViewIdField)) {
    return false;  // slice has no view identifier field
  }

  auto viewId = slice.get(StaticStrings::ViewIdField);

  // NOTE: below will not match if 'viewId' is 'id' or 'name',
  //       but ViewIdField should always contain GUID
  if (!viewId.isString() || !viewId.isEqualString(_viewGuid)) {
    return false;  // IResearch View identifiers of current object and slice do not match
  }

  IResearchLinkMeta other;
  std::string errorField;

  return other.init(_collection.vocbase().server(), slice, true, errorField,
                    _collection.vocbase().name())  // for db-server analyzer validation should have already passed on coordinator (missing analyzer == no match)
         && _meta == other;
}

Result IResearchLink::properties(velocypack::Builder& builder, bool forPersistence) const {
  if (!builder.isOpenObject()  // not an open object
      || !_meta.json(_collection.vocbase().server(), builder, forPersistence,
                     nullptr, &(_collection.vocbase()))) {
    return {TRI_ERROR_BAD_PARAMETER};
  }

  builder.add(arangodb::StaticStrings::IndexId,
              velocypack::Value(std::to_string(_id.id())));
  builder.add(arangodb::StaticStrings::IndexType,
              velocypack::Value(IResearchLinkHelper::type()));
  builder.add(StaticStrings::ViewIdField, velocypack::Value(_viewGuid));

  return {};
}

Result IResearchLink::properties(IResearchViewMeta const& meta) {
  // '_dataStore' can be asynchronously modified
  auto lock = _asyncSelf->lock();

  if (!_asyncSelf.get()) {
    // the current link is no longer valid (checked after ReadLock acquisition)
    return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
            "failed to lock arangosearch link while modifying properties "
            "of arangosearch link '" +
                std::to_string(id().id()) + "'"};
  }

  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  {
    WRITE_LOCKER(writeLock, _dataStore._mutex);  // '_meta' can be asynchronously modified
    _dataStore._meta = meta;
  }

  if (_engine->recoveryState() == RecoveryState::DONE) {
    if (meta._commitIntervalMsec) {
      scheduleCommit(std::chrono::milliseconds(meta._commitIntervalMsec));
    }

    if (meta._consolidationIntervalMsec && meta._consolidationPolicy.policy()) {
      scheduleConsolidation(std::chrono::milliseconds(meta._consolidationIntervalMsec));
    }
  }

  irs::index_writer::segment_options properties;
  properties.segment_count_max = meta._writebufferActive;
  properties.segment_memory_max = meta._writebufferSizeMax;

  static_assert(noexcept(_dataStore._writer->options(properties)));
  _dataStore._writer->options(properties);

  return {};
}

Result IResearchLink::remove(transaction::Methods& trx,
                             LocalDocumentId documentId, velocypack::Slice doc) {
  TRI_ASSERT(_engine);
  TRI_ASSERT(trx.state());

  auto& state = *(trx.state());

  TRI_ASSERT(!state.hasHint(transaction::Hints::Hint::INDEX_CREATION));

  if (_dataStore._inRecovery.load(std::memory_order_acquire) &&
      _engine->recoveryTick() <= _dataStore._recoveryTick) {
    LOG_TOPIC("7d228", TRACE, iresearch::TOPIC)
        << "skipping 'removal', operation tick '" << _engine->recoveryTick()
        << "', recovery tick '" << _dataStore._recoveryTick << "'";

    return {};
  }

  auto* key = this;

// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* ctx = dynamic_cast<LinkTrxState*>(state.cookie(key));
#else
  auto* ctx = static_cast<LinkTrxState*>(state.cookie(key));
#endif

  if (!ctx) {
    // '_dataStore' can be asynchronously modified
    auto lock = _asyncSelf->lock();

    if (!_asyncSelf.get()) {
      // the current link is no longer valid (checked after ReadLock acquisition)

      return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
              "failed to lock arangosearch link while removing a document from "
              "arangosearch link '" +
                  std::to_string(id().id()) + "', tid '" +
                  std::to_string(state.id().id()) + "', revision '" +
                  std::to_string(documentId.id()) + "'"};
    }

    TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

    auto ptr = irs::memory::make_unique<LinkTrxState>(std::move(lock),
                                                      *(_dataStore._writer));

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx || !trx.addStatusChangeCallback(&_trxCallback)) {
      return {TRI_ERROR_INTERNAL,
              "failed to store state into a TransactionState for remove from "
              "arangosearch link '" +
                  std::to_string(id().id()) + "', tid '" +
                  std::to_string(state.id().id()) + "', revision '" +
                  std::to_string(documentId.id()) + "'"};
    }
  }

  // ...........................................................................
  // if an exception occurs below than the transaction is dropped including all
  // of its fid stores, no impact to iResearch View data integrity
  // ...........................................................................
  try {
    ctx->remove(*_engine, documentId);

    return {TRI_ERROR_NO_ERROR};
  } catch (basics::Exception const& e) {
    return {
        e.code(),
        "caught exception while removing document from arangosearch link '" +
            std::to_string(id().id()) + "', revision '" +
            std::to_string(documentId.id()) + "': " + e.what()};
  } catch (std::exception const& e) {
    return {
        TRI_ERROR_INTERNAL,
        "caught exception while removing document from arangosearch link '" +
            std::to_string(id().id()) + "', revision '" +
            std::to_string(documentId.id()) + "': " + e.what()};
  } catch (...) {
    return {
        TRI_ERROR_INTERNAL,
        "caught exception while removing document from arangosearch link '" +
            std::to_string(id().id()) + "', revision '" +
            std::to_string(documentId.id()) + "'"};
  }

  return {};
}

IResearchLink::Snapshot IResearchLink::snapshot() const {
  // '_dataStore' can be asynchronously modified
  auto link = _asyncSelf->lock();

  if (!link) {
    LOG_TOPIC("f42dc", WARN, iresearch::TOPIC)
        << "failed to lock arangosearch link while retrieving snapshot from "
           "arangosearch link '"
        << id() << "'";

    return {};  // return an empty reader
  }

  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  return {std::move(link), irs::directory_reader(_dataStore._reader)};  // return a copy of the current reader
}

Index::IndexType IResearchLink::type() {
  // TODO: don't use enum
  return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
}

char const* IResearchLink::typeName() {
  return IResearchLinkHelper::type().c_str();
}

bool IResearchLink::setCollectionName(irs::string_ref name) noexcept {
  TRI_ASSERT(!name.empty());
  if (_meta._collectionName.empty()) {
    auto& nonConstMeta = const_cast<IResearchLinkMeta&>(_meta);
    nonConstMeta._collectionName = name;
    return true;
  }
  LOG_TOPIC_IF("5573c", ERR, iresearch::TOPIC, name != _meta._collectionName)
      << "Collection name mismatch for arangosearch link '" << id() << "'."
      << " Meta name '" << _meta._collectionName << "' setting name '" << name << "'";
  TRI_ASSERT(name == _meta._collectionName);
  return false;
}

Result IResearchLink::unload() {
  // this code is used by the MMFilesEngine
  // if the collection is in the process of being removed then drop it from the view
  // FIXME TODO remove once LogicalCollection::drop(...) will drop its indexes explicitly
  if (_collection.deleted()  // collection deleted
      || TRI_vocbase_col_status_e::TRI_VOC_COL_STATUS_DELETED == _collection.status()) {
    return drop();
  }

  std::atomic_store(&_flushSubscription, {});  // reset together with '_asyncSelf'
  _asyncSelf->reset();  // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  if (!_dataStore) {
    return {};
  }
  removeStats();

  try {
    _dataStore.resetDataStore();
  } catch (basics::Exception const& e) {
    return {e.code(), "caught exception while unloading arangosearch link '" +
                          std::to_string(id().id()) + "': " + e.what()};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while removing arangosearch link '" +
                std::to_string(id().id()) + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while removing arangosearch link '" +
                std::to_string(id().id()) + "'"};
  }

  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup referenced analyzer
////////////////////////////////////////////////////////////////////////////////
AnalyzerPool::ptr IResearchLink::findAnalyzer(AnalyzerPool const& analyzer) const {
  auto const it = _meta._analyzerDefinitions.find(irs::string_ref(analyzer.name()));

  if (it == _meta._analyzerDefinitions.end()) {
    return nullptr;
  }

  auto pool = *it;

  if (pool && analyzer == *pool) {
    return pool;
  }

  return nullptr;
}

IResearchLink::LinkStats IResearchLink::stats() const {
  // '_dataStore' can be asynchronously modified
  auto lock = _asyncSelf->lock();
  return statsUnsafe();
}

IResearchLink::LinkStats IResearchLink::statsUnsafe() const {
  LinkStats stats;
  if (!_dataStore) {
    return {};
  }
  stats.numBufferedDocs = _dataStore._writer->buffered_docs();

  // copy of 'reader' is important to hold reference to the current snapshot
  auto reader = _dataStore._reader;
  if (!reader) {
    return {};
  }

  stats.numSegments = reader->size();
  stats.numDocs = reader->docs_count();
  stats.numLiveDocs = reader->live_docs_count();
  stats.numFiles = 1;  // +1 for segments file

  auto visitor = [&stats](std::string const& /*name*/, irs::segment_meta const& segment) noexcept {
    stats.indexSize += segment.size;
    stats.numFiles += segment.files.size();
    return true;
  };

  reader->meta().meta.visit_segments(visitor);
  return stats;
}

void IResearchLink::toVelocyPackStats(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());

  auto const stats = this->stats();

  builder.add("numBufferedDocs", VPackValue(stats.numBufferedDocs));
  builder.add("numDocs", VPackValue(stats.numDocs));
  builder.add("numLiveDocs", VPackValue(stats.numLiveDocs));
  builder.add("numSegments", VPackValue(stats.numSegments));
  builder.add("numFiles", VPackValue(stats.numFiles));
  builder.add("indexSize", VPackValue(stats.indexSize));
}

std::string_view IResearchLink::format() const noexcept {
  return getFormat(LinkVersion{_meta._version});
}

IResearchViewStoredValues const& IResearchLink::storedValues() const noexcept {
  return _meta._storedValues;
}

void IResearchLink::removeStats() {
  auto& metricFeature =
      _collection.vocbase().server().getFeature<metrics::MetricsFeature>();
  if (_linkStats) {
    _linkStats = nullptr;
    auto builder = getMetric<metrics::BatchBuilder<LinkStats>>(*this);
    builder.setName("arangosearch_link_stats");
    metricFeature.remove(std::move(builder));
  }
  if (_numFailedCommits) {
    _numFailedCommits = nullptr;
    metricFeature.remove(getMetric<arangosearch_num_failed_commits>(*this));
  }
  if (_numFailedCleanups) {
    _numFailedCleanups = nullptr;
    metricFeature.remove(getMetric<arangosearch_num_failed_cleanups>(*this));
  }
  if (_numFailedConsolidations) {
    _numFailedConsolidations = nullptr;
    metricFeature.remove(getMetric<arangosearch_num_failed_consolidations>(*this));
  }
  if (_avgCommitTimeMs) {
    _avgCommitTimeMs = nullptr;
    metricFeature.remove(getMetric<arangosearch_commit_time>(*this));
  }
  if (_avgCleanupTimeMs) {
    _avgCleanupTimeMs = nullptr;
    metricFeature.remove(getMetric<arangosearch_cleanup_time>(*this));
  }
  if (_avgConsolidationTimeMs) {
    _avgConsolidationTimeMs = nullptr;
    metricFeature.remove(getMetric<arangosearch_consolidation_time>(*this));
  }
}

const std::string& IResearchLink::getViewId() const noexcept {
  return _viewGuid;
}

std::string IResearchLink::getDbName() const {
  return std::to_string(_collection.vocbase().id());
}

const std::string& IResearchLink::getShardName() const noexcept {
  if (ServerState::instance()->isDBServer()) {
    return _collection.name();
  }
  return arangodb::StaticStrings::Empty;
}

std::string IResearchLink::getCollectionName() const {
  if (ServerState::instance()->isDBServer()) {
    return _meta._collectionName;
  }
  if (ServerState::instance()->isSingleServer()) {
    return std::to_string(_collection.id().id());
  }
  TRI_ASSERT(false);
  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compute the data path to user for iresearch data store
///        get base path from DatabaseServerFeature (similar to MMFilesEngine)
///        the path is hardcoded to reside under:
///        <DatabasePath>/<IResearchLink::type()>-<link id>
///        similar to the data path calculation for collections
////////////////////////////////////////////////////////////////////////////////
irs::utf8_path getPersistedPath(DatabasePathFeature const& dbPathFeature,
                                IResearchLink const& link) {
  irs::utf8_path dataPath(dbPathFeature.directory());
  static constexpr std::string_view kSubPath{"databases"};
  static constexpr std::string_view kDbPath{"database-"};

  dataPath /= kSubPath;
  dataPath /= kDbPath;
  dataPath += std::to_string(link.collection().vocbase().id());
  dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
  dataPath += "-";
  dataPath += std::to_string(link.collection().id().id());  // has to be 'id' since this can be a per-shard collection
  dataPath += "_";
  dataPath += std::to_string(link.id().id());

  return dataPath;
}

void IResearchLink::LinkStats::needName() const { _needName = true; }

void IResearchLink::LinkStats::toPrometheus(std::string& result,       //
                                            std::string_view globals,  //
                                            std::string_view labels) const {
  auto writeAnnotation = [&] {
    result.push_back('{');
    result.append(globals);
    if (!labels.empty()) {
      if (!globals.empty()) {
        result.push_back(',');
      }
      result.append(labels);
    }
    result.push_back('}');
  };
  auto writeMetric = [&](std::string_view name, std::string_view help, size_t value) {
    if (_needName) {
      result.append("# HELP ");
      result.append(name);
      result.push_back(' ');
      result.append(help);
      result.push_back('\n');
      result.append("# TYPE ");
      result.append(name);
      result.append(" gauge\n");
    }
    result.append(name);
    writeAnnotation();
    result.append(std::to_string(value));
    result.push_back('\n');
  };
  writeMetric(arangosearch_num_buffered_docs::kName,
              "Number of buffered documents", numBufferedDocs);
  writeMetric(arangosearch_num_docs::kName, "Number of documents", numDocs);
  writeMetric(arangosearch_num_live_docs::kName, "Number of live documents", numLiveDocs);
  writeMetric(arangosearch_num_segments::kName, "Number of segments", numSegments);
  writeMetric(arangosearch_num_files::kName, "Number of files", numFiles);
  writeMetric(arangosearch_index_size::kName, "Size of the index in bytes", indexSize);
  _needName = false;
}

std::tuple<uint64_t, uint64_t, uint64_t> IResearchLink::numFailed() const {
  return {_numFailedCommits ? _numFailedCommits->load(std::memory_order_relaxed) : 0,
          _numFailedCleanups ? _numFailedCleanups->load(std::memory_order_relaxed) : 0,
          _numFailedConsolidations ? _numFailedConsolidations->load(std::memory_order_relaxed)
                                   : 0};
}

std::tuple<uint64_t, uint64_t, uint64_t> IResearchLink::avgTime() const {
  return {_avgCommitTimeMs ? _avgCommitTimeMs->load(std::memory_order_relaxed) : 0,
          _avgCleanupTimeMs ? _avgCleanupTimeMs->load(std::memory_order_relaxed) : 0,
          _avgConsolidationTimeMs ? _avgConsolidationTimeMs->load(std::memory_order_relaxed)
                                  : 0};
}

}  // namespace arangodb::iresearch
