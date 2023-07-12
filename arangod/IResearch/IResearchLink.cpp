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
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#ifdef USE_ENTERPRISE
#include "Cluster/ClusterMethods.h"
#endif
#include "Cluster/ServerState.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchCompression.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchPrimaryKeyFilter.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/IResearch/IResearchLinkEE.hpp"
#endif

#include <velocypack/StringRef.h>

using namespace std::literals;

namespace arangodb::iresearch {
namespace {

// Ensures that all referenced analyzer features are consistent.
[[maybe_unused]] void checkAnalyzerFeatures(IResearchLinkMeta const& meta) {
  auto assertAnalyzerFeatures =
      [version = LinkVersion{meta._version}](auto const& analyzers) {
        for (auto& analyzer : analyzers) {
          irs::type_info::type_id invalidNorm;
          if (version < LinkVersion::MAX) {
            invalidNorm = irs::type<irs::Norm2>::id();
          } else {
            invalidNorm = irs::type<irs::Norm>::id();
          }

          const auto features = analyzer->fieldFeatures();

          TRI_ASSERT(std::end(features) == std::find(std::begin(features),
                                                     std::end(features),
                                                     invalidNorm));
        }
      };

  auto checkFieldFeatures = [&assertAnalyzerFeatures](auto const& fieldMeta,
                                                      auto&& self) -> void {
    assertAnalyzerFeatures(fieldMeta._analyzers);
    for (auto const& entry : fieldMeta._fields) {
      self(*entry.value(), self);
    }
  };
  assertAnalyzerFeatures(meta._analyzerDefinitions);
  checkFieldFeatures(meta, checkFieldFeatures);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief container storing the link state for a given TransactionState
////////////////////////////////////////////////////////////////////////////////
struct LinkTrxState final : public TransactionState::Cookie {
  LinkTrxState(LinkTrxState const&) = delete;
  LinkTrxState(LinkTrxState&&) = delete;
  LinkTrxState& operator=(LinkTrxState const&) = delete;
  LinkTrxState& operator=(LinkTrxState&&) = delete;

  LinkLock _linkLock;  // prevent data-store deallocation (lock @ AsyncSelf)
  irs::index_writer::documents_context _ctx;
  PrimaryKeyFilterContainer _removals;  // list of document removals
  bool _wasCommit{false};

  LinkTrxState(LinkLock linkLock, irs::index_writer& writer) noexcept
      : _linkLock{std::move(linkLock)}, _ctx{writer.documents()} {}

  ~LinkTrxState() final {
    if (!_wasCommit) {
      _removals.clear();
      _ctx.reset();
    }
    TRI_ASSERT(_removals.empty());
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
                      IResearchLinkMeta const& meta, IndexId id,
                      arangodb::StorageEngine* engine) {
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

  if (trx.state()->hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
    ctx.SetLastTick(engine->currentTick());
  }

  return {};
}

class IResearchFlushSubscription final : public FlushSubscription {
 public:
  explicit IResearchFlushSubscription(TRI_voc_tick_t tick,
                                      std::string const& name)
      : _tick{tick}, _name{name} {}

  /// @brief earliest tick that can be released
  TRI_voc_tick_t tick() const noexcept final {
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
  // Only max tick stored. Possibly has uncommitted WAL ticks
  SingleTick = 0,
  // Two stage commit ticks are stored.
  // Min tick is fully committed but between first and second tick
  // uncommitted WAL records may be present. After second tick nothing is
  // committed.
  TwoStageTick = 1
};

bool readTick(irs::bytes_ref const& payload, TRI_voc_tick_t& tickLow,
              TRI_voc_tick_t& tickHigh) noexcept {
  static_assert(sizeof(uint64_t) == sizeof(TRI_voc_tick_t));

  if (payload.size() < sizeof(uint64_t)) {
    LOG_TOPIC("41474", ERR, TOPIC) << "Unexpected segment payload size "
                                   << payload.size() << " for initial check";
    return false;
  }
  std::memcpy(&tickLow, payload.c_str(), sizeof(uint64_t));
  tickLow = TRI_voc_tick_t(irs::numeric_utils::ntoh64(tickLow));
  tickHigh = std::numeric_limits<TRI_voc_tick_t>::max();
  SegmentPayloadVersion version{SegmentPayloadVersion::SingleTick};
  if (payload.size() < sizeof(uint64_t) + sizeof(version)) {
    return true;
  }
  std::memcpy(&version, payload.c_str() + sizeof(uint64_t), sizeof(version));
  version = static_cast<SegmentPayloadVersion>(irs::numeric_utils::ntoh32(
      static_cast<std::underlying_type_t<SegmentPayloadVersion>>(version)));
  switch (version) {
    case SegmentPayloadVersion::TwoStageTick: {
      if (payload.size() == (2 * sizeof(uint64_t)) + sizeof(version)) {
        std::memcpy(&tickHigh,
                    payload.c_str() + sizeof(uint64_t) + sizeof(version),
                    sizeof(uint64_t));
        tickHigh = TRI_voc_tick_t(irs::numeric_utils::ntoh64(tickHigh));
      } else {
        LOG_TOPIC("49b4d", ERR, TOPIC)
            << "Unexpected segment payload size " << payload.size()
            << " for version '"
            << static_cast<std::underlying_type_t<SegmentPayloadVersion>>(
                   version)
            << "'";
        return false;
      }
      break;
    }
    default:
      // falling back to SingleTick as it always present
      LOG_TOPIC("fad1f", WARN, TOPIC)
          << "Unexpected segment payload version '"
          << static_cast<std::underlying_type_t<SegmentPayloadVersion>>(version)
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
        << "scheduled a " << T::typeName() << " task for arangosearch link '"
        << id << "', delay '" << delay.count() << "'";

    LOG_TOPIC("eb0d2", TRACE, TOPIC)
        << T::typeName()
        << " pool: " << ThreadGroupStats{async->stats(T::threadGroup())};

    if (!asyncLink->terminationRequested()) {
      async->queue(T::threadGroup(), delay, static_cast<const T&>(*this));
    }
  }

  std::shared_ptr<MaintenanceState> state;
  IResearchFeature* async{nullptr};
  IResearchLink::AsyncLinkPtr asyncLink;
  IndexId id;
};

template<typename NormyType>
auto getIndexFeatures() {
  return [](irs::type_info::type_id id) {
    TRI_ASSERT(irs::type<NormyType>::id() == id ||
               irs::type<irs::granularity_prefix>::id() == id);
    const irs::column_info info{
        irs::type<irs::compression::none>::get(), {}, false};

    if (irs::type<NormyType>::id() == id) {
      return std::make_pair(info, &NormyType::MakeWriter);
    }

    return std::make_pair(info, irs::feature_writer_factory_t{});
  };
}

}  // namespace

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
  void finalize(IResearchLink& link, IResearchLink::CommitResult code);

  size_t cleanupIntervalCount{};
  std::chrono::milliseconds commitIntervalMsec{};
  std::chrono::milliseconds consolidationIntervalMsec{};
  size_t cleanupIntervalStep{};
};

void CommitTask::finalize(IResearchLink& link,
                          IResearchLink::CommitResult code) {
  constexpr size_t kMaxNonEmptyCommits = 10;
  constexpr size_t kMaxPendingConsolidations = 3;

  if (code != IResearchLink::CommitResult::NO_CHANGES) {
    state->pendingCommits.fetch_add(1, std::memory_order_release);
    schedule(commitIntervalMsec);

    if (code == IResearchLink::CommitResult::DONE) {
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

  if (asyncLink->terminationRequested()) {
    LOG_TOPIC("eba1a", DEBUG, TOPIC)
        << "termination requested while committing the link '" << id
        << "', runId '" << size_t(&runId) << "'";
    return;
  }

  auto linkLock = asyncLink->lock();
  if (!linkLock) {
    LOG_TOPIC("ebada", DEBUG, TOPIC)
        << "link '" << id << "' is no longer valid, run id '" << size_t(&runId)
        << "'";
    return;
  }

  IResearchLink::CommitResult code = IResearchLink::CommitResult::UNDEFINED;

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

    LOG_TOPIC("eba4a", DEBUG, TOPIC) << "sync is disabled for the link '" << id
                                     << "', runId '" << size_t(&runId) << "'";
    return;
  }

  TRI_IF_FAILURE("IResearchCommitTask::commitUnsafe") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  auto start = std::chrono::steady_clock::now();
  auto res = linkLock->commitUnsafe(false, &code);
  // run commit ('_asyncSelf' locked by async task)
  auto timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();

  if (res.ok()) {
    LOG_TOPIC("7e323", TRACE, TOPIC)
        << "successful sync of arangosearch link '" << id << "', run id '"
        << size_t(&runId) << "', took: " << timeMs << "ms";
  } else {
    LOG_TOPIC("8377b", WARN, TOPIC)
        << "error after running for " << timeMs
        << "ms while committing arangosearch link '" << linkLock->id()
        << "', run id '" << size_t(&runId) << "': " << res.errorNumber() << " "
        << res.errorMessage();
  }
  if (cleanupIntervalStep && ++cleanupIntervalCount >= cleanupIntervalStep) {
    cleanupIntervalCount = 0;

    TRI_IF_FAILURE("IResearchCommitTask::cleanupUnsafe") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    start = std::chrono::steady_clock::now();
    res = linkLock->cleanupUnsafe();
    timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - start)
                 .count();
    if (res.ok()) {
      LOG_TOPIC("7e821", TRACE, TOPIC)
          << "successful cleanup of arangosearch link '" << id << "', run id '"
          << size_t(&runId) << "', took: " << timeMs << "ms";
    } else {
      LOG_TOPIC("130de", WARN, TOPIC)
          << "error after running for " << timeMs
          << "ms while cleaning up arangosearch link '" << id << "', run id '"
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

  irs::merge_writer::flush_progress_t progress;
  IResearchViewMeta::ConsolidationPolicy consolidationPolicy;
  std::chrono::milliseconds consolidationIntervalMsec{};
};

void ConsolidationTask::operator()() {
  const char runId = 0;
  state->pendingConsolidations.fetch_sub(1, std::memory_order_release);

  if (asyncLink->terminationRequested()) {
    LOG_TOPIC("eba2a", DEBUG, TOPIC)
        << "termination requested while consolidating the link '" << id
        << "', runId '" << size_t(&runId) << "'";
    return;
  }

  auto linkLock = asyncLink->lock();
  if (!linkLock) {
    LOG_TOPIC("eb0d1", DEBUG, TOPIC)
        << "link '" << id << "' is no longer valid, run id '" << size_t(&runId)
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
        << "consolidation is disabled for the link '" << id << "', runId '"
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

  auto const start = std::chrono::steady_clock::now();
  bool emptyConsolidation = false;
  auto const res = linkLock->consolidateUnsafe(consolidationPolicy, progress,
                                               emptyConsolidation);
  auto const timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - start)
                          .count();
  if (res.ok()) {
    if (emptyConsolidation) {
      state->noopConsolidationCount.fetch_add(1, std::memory_order_release);
    } else {
      state->noopConsolidationCount.store(0, std::memory_order_release);
    }
    LOG_TOPIC("7e828", TRACE, TOPIC)
        << "successful consolidation of arangosearch link '" << linkLock->id()
        << "', run id '" << size_t(&runId) << "', took: " << timeMs << "ms";
  } else {
    LOG_TOPIC("bce4f", DEBUG, TOPIC)
        << "error after running for " << timeMs
        << "ms while consolidating arangosearch link '" << linkLock->id()
        << "', run id '" << size_t(&runId) << "': " << res.errorNumber() << " "
        << res.errorMessage();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     IResearchLink
// -----------------------------------------------------------------------------

AsyncLinkHandle::AsyncLinkHandle(IResearchLink* link) : _link{link} {}

AsyncLinkHandle::~AsyncLinkHandle() = default;

void AsyncLinkHandle::reset() {
  _asyncTerminate.store(true, std::memory_order_release);
  // mark long-running async jobs for termination
  _link.reset();
  // the data-store is being deallocated, link use is no longer valid
  // (wait for all the view users to finish)
}

IResearchLink::IResearchLink(IndexId iid, LogicalCollection& collection)
    : _engine{nullptr},
      _asyncFeature{
          &collection.vocbase().server().getFeature<IResearchFeature>()},
      _asyncSelf{std::make_shared<AsyncLinkHandle>(nullptr)},
      // mark as data store not initialized
      _collection{collection},
      _error(DataStoreError::kNoError),
      _maintenanceState{std::make_shared<MaintenanceState>()},
      _id{iid},
      _lastCommittedTickStageOne{0},
      _lastCommittedTickStageTwo{0},
      _cleanupIntervalCount{0},
      _createdInRecovery{false},
      _commitStageOne{true} {
  // initialize transaction callback
  _postCommitCallback = [this](TransactionState& state) {
    auto prev = state.cookie(this, nullptr);  // get existing cookie
    if (!prev) {
      return;
    }
// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto& ctx = dynamic_cast<LinkTrxState&>(*prev);
#else
    auto& ctx = static_cast<LinkTrxState&>(*prev);
#endif
    uint64_t const lastOperationTick{state.lastOperationTick()};
#if ARANGODB_ENABLE_MAINTAINER_MODE && ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("ArangoSearch::ThreeTransactionsMisorder") {
      LOG_TOPIC("8ac71", DEBUG, TOPIC)
          << lastOperationTick << " arrived to post commit in thread "
          << std::this_thread::get_id();
      bool inList{false};
      while (true) {
        std::unique_lock<std::mutex> sync(_t3Failureync);
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
    if (!ctx._removals.empty()) {
      auto filter =
          std::make_unique<PrimaryKeyFilterContainer>(std::move(ctx._removals));
      ctx._ctx.remove(std::unique_ptr<irs::filter>(std::move(filter)));
    }
    ctx._ctx.SetLastTick(lastOperationTick);
    if (ADB_LIKELY(!_engine->inRecovery())) {
      ctx._ctx.SetFirstTick(lastOperationTick - state.numPrimitiveOperations());
    }
    TRI_ASSERT(!ctx._wasCommit);
    ctx._wasCommit = true;
  };

  _preCommitCallback = [this](TransactionState& state) {
    auto prev = state.cookie(this);  // get existing cookie

    if (!prev) {
      return;
    }
// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto& ctx = dynamic_cast<LinkTrxState&>(*prev);
#else
    auto& ctx = static_cast<LinkTrxState&>(*prev);
#endif
#if ARANGODB_ENABLE_MAINTAINER_MODE && ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("ArangoSearch::ThreeTransactionsMisorder") {
      size_t myNumber{0};
      while (true) {
        std::unique_lock<std::mutex> sync(_t3Failureync);
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
            // Numer 2 should go tho another flush context
            // so wait for commit to start and
            // then give writer some time to switch flush context
            if (!_t3CommitSignal) {
              sync.unlock();
              std::this_thread::sleep_for(100ms);
              continue;
            }
            std::this_thread::sleep_for(5s);
          }
          ctx._ctx.AddToFlush();
          LOG_TOPIC("cdc04", DEBUG, TOPIC) << myNumber << " added to flush";
          ++_t3NumFlushRegistered;
          break;
        }
      }
      // this transaction is flushed. Need to wait for all to arrive
      while (true) {
        std::unique_lock<std::mutex> sync(_t3Failureync);
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
      ctx._ctx.AddToFlush();
    }
#else
    ctx._ctx.AddToFlush();
#endif
  };
}

IResearchLink::~IResearchLink() {
  if (isOutOfSync()) {
    TRI_ASSERT(_asyncFeature != nullptr);
    // count down the number of out of sync links
    _asyncFeature->untrackOutOfSyncLink();
  }

  Result res;
  try {
    res = unload();  // disassociate from view if it has not been done yet
  } catch (...) {
  }

  if (!res.ok()) {
    LOG_TOPIC("2b41f", ERR, TOPIC)
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

void IResearchLink::afterTruncate(TRI_voc_tick_t tick,
                                  transaction::Methods* trx) {
  auto linkLock = _asyncSelf->lock();
  // '_dataStore' can be asynchronously modified

  TRI_IF_FAILURE("ArangoSearchTruncateFailure") {
    CrashHandler::setHardKill();
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (!linkLock) {
    // the current link is no longer valid (checked after ReadLock acquisition)
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
                                   "failed to lock arangosearch link while "
                                   "truncating arangosearch link '" +
                                       std::to_string(id().id()) + "'");
  }
  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->lock() is valid

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
      // throw away all pending operations as clear will overwrite them all
      // force active segment release to allow commit go and avoid deadlock in
      // clear
      state.cookie(key, nullptr);
    }
  }

  std::lock_guard guard{_commitMutex};
  _lastCommittedTickStageTwo = tick;
  bool recoverCommittedTick = true;
  _commitStageOne = true;
  auto lastCommittedTickGuard =
      irs::make_finally([lastCommittedTickStageOne = _lastCommittedTickStageOne,
                         lastCommittedTickStageTwo = _lastCommittedTickStageTwo,
                         this, &recoverCommittedTick] {
        if (recoverCommittedTick) {
          _lastCommittedTickStageOne = lastCommittedTickStageOne;
          _lastCommittedTickStageTwo = lastCommittedTickStageTwo;
        }
      });

  try {
    _dataStore._writer->clear(tick);
    recoverCommittedTick = false;
    // _lastCommittedTick now updated and data is written to storage
    _lastCommittedTickStageTwo = _lastCommittedTickStageOne;
    // get new reader
    auto reader = _dataStore._reader.reopen();

    if (!reader) {
      // nothing more to do
      LOG_TOPIC("1c2c1", WARN, TOPIC)
          << "failed to update snapshot after truncate "
          << ", reuse the existing snapshot for arangosearch link '" << id()
          << "'";
      return;
    }

    // update reader
    _dataStore._reader = reader;

    auto subscription = std::atomic_load(&_flushSubscription);

    if (subscription) {
      auto& impl = static_cast<IResearchFlushSubscription&>(*subscription);

      impl.tick(_lastCommittedTickStageOne);
    }
  } catch (std::exception const& e) {
    LOG_TOPIC("a3c57", ERR, TOPIC)
        << "caught exception while truncating arangosearch link '" << id()
        << "': " << e.what();
    throw;
  } catch (...) {
    LOG_TOPIC("79a7d", WARN, TOPIC)
        << "caught exception while truncating arangosearch link '" << id()
        << "'";
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchLink::cleanupUnsafe() {
  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->lock() is valid

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
  auto linkLock = _asyncSelf->lock();  // '_dataStore' can be async modified
  if (!linkLock) {
    // the current link is no longer valid (checked after ReadLock acquisition)
    return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
            "failed to lock arangosearch link while committing arangosearch "
            "link '" +
                std::to_string(id().id()) + "'"};
  }
  return commit(std::move(linkLock), wait);
}

Result IResearchLink::commit(LinkLock linkLock, bool wait) {
  TRI_ASSERT(linkLock);
  TRI_ASSERT(linkLock->_dataStore);
  // must be valid if _asyncSelf->lock() is valid
  CommitResult code = CommitResult::UNDEFINED;
  auto result = linkLock->commitUnsafe(wait, &code);
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
Result IResearchLink::commitUnsafe(bool wait, CommitResult* code) {
  Result res = commitUnsafeImpl(wait, code);

  TRI_IF_FAILURE("ArangoSearch::FailOnCommit") {
    // intentionally mark the commit as failed
    res.reset(TRI_ERROR_DEBUG);
  }

  if (res.fail() && !isOutOfSync()) {
    // mark link as out of sync if it wasn't marked like that before.
    if (setOutOfSync()) {
      // persist "outOfSync" flag in RocksDB once. note: if this fails, it will
      // throw an exception
      try {
        _engine->changeCollection(collection().vocbase(), collection(), true);
      } catch (std::exception const& ex) {
        // we couldn't persist the outOfSync flag, but we can't mark the link
        // as "not outOfSync" again. not much we can do except logging.
        LOG_TOPIC("211d2", WARN, iresearch::TOPIC)
            << "failed to store 'outOfSync' flag for arangosearch link '"
            << id() << "': " << ex.what();
      }
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchLink::commitUnsafeImpl(bool wait, CommitResult* code) {
  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->lock() is valid
  auto subscription = std::atomic_load(&_flushSubscription);

  if (!subscription) {
    // already released
    *code = CommitResult::NO_CHANGES;
    return {};
  }

  auto& impl = static_cast<IResearchFlushSubscription&>(*subscription);

  try {
    auto commitLock = irs::make_unique_lock(_commitMutex, std::try_to_lock);

    if (!commitLock.owns_lock()) {
      if (!wait) {
        LOG_TOPIC("37bcc", TRACE, TOPIC)
            << "commit for arangosearch link '" << id()
            << "' is already in progress, skipping";

        *code = CommitResult::IN_PROGRESS;
        return {};
      }

      LOG_TOPIC("37bca", TRACE, TOPIC)
          << "commit for arangosearch link '" << id()
          << "' is already in progress, waiting";

      commitLock.lock();
    }
    auto lastTickBeforeCommit = _engine->currentTick();
    _commitStageOne = true;
    auto const lastCommittedTick = _lastCommittedTickStageOne;

#if ARANGODB_ENABLE_MAINTAINER_MODE && ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("ArangoSearch::ThreeTransactionsMisorder") {
      std::unique_lock<std::mutex> sync(_t3Failureync);
      while (_t3NumFlushRegistered < 2) {
        sync.unlock();
        std::this_thread::sleep_for(100ms);
        sync.lock();
      }
      _t3CommitSignal = true;
      LOG_TOPIC("4cb66", DEBUG, TOPIC) << "Commit started";
    }
#endif
    try {
      // _lastCommittedTick is being updated in '_before_commit'
      *code = _dataStore._writer->commit() ? CommitResult::DONE
                                           : CommitResult::NO_CHANGES;
    } catch (...) {
      // restore last committed tick in case of any error
      _lastCommittedTickStageOne = lastCommittedTick;
      throw;
    }
    if (CommitResult::NO_CHANGES == *code) {
      LOG_TOPIC("7e319", TRACE, TOPIC)
          << "no changes registered for arangosearch link '" << id()
          << "' got last operation tick '" << _lastCommittedTickStageOne << "'";

      // no changes, can release the latest tick before commit
      impl.tick(lastTickBeforeCommit);
      _lastCommittedTickStageOne = lastTickBeforeCommit;
      _lastCommittedTickStageTwo = lastTickBeforeCommit;
      return {};
    }

#if ARANGODB_ENABLE_MAINTAINER_MODE && ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("ArangoSearch::ThreeTransactionsMisorder::StageOneKill") {
      std::unique_lock<std::mutex> sync(_t3Failureync);
      // if all candidates gathered and max tick is committed - it is time to
      // crash
      if (_t3Candidates.size() >= 3 &&
          std::find_if(_t3Candidates.begin(), _t3Candidates.end(),
                       [this](uint64_t t) {
                         return t > _lastCommittedTickStageOne;
                       }) == _t3Candidates.end()) {
        TRI_TerminateDebugging("Killed on commit stage one");
      }
    }
#endif
    lastTickBeforeCommit = _engine->currentTick();
    _commitStageOne = false;
    auto lastCommittedTickStageTwo = _lastCommittedTickStageTwo;
    // now commit what has possibly accumulated in the second flush context
    // but will not move the tick as it possibly contains "3rd" generation
    // changes
    try {
      auto secondCommit = _dataStore._writer->commit();
      LOG_TOPIC("21bda", TRACE, TOPIC)
          << "Second commit for arangosearch link '" << id() << "' result is "
          << secondCommit << " tick " << _lastCommittedTickStageTwo;
      if (!secondCommit) {
        _lastCommittedTickStageOne = lastTickBeforeCommit;
        _lastCommittedTickStageTwo = lastTickBeforeCommit;
      }
    } catch (...) {
      // restore last committed tick in case of any error
      _lastCommittedTickStageTwo = lastCommittedTickStageTwo;
      throw;
    }
#if ARANGODB_ENABLE_MAINTAINER_MODE && ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("ArangoSearch::ThreeTransactionsMisorder::StageTwoKill") {
      std::unique_lock<std::mutex> sync(_t3Failureync);
      // if all candidates gathered and max tick is committed - it is time to
      // crash
      if (_t3Candidates.size() >= 3 &&
          std::find_if(_t3Candidates.begin(), _t3Candidates.end(),
                       [this](uint64_t t) {
                         return t > _lastCommittedTickStageOne;
                       }) == _t3Candidates.end()) {
        TRI_TerminateDebugging("Killed on commit stage two");
      }
    }
#endif
    // get new reader
    auto reader = _dataStore._reader.reopen();

    if (!reader) {
      // nothing more to do
      LOG_TOPIC("37bcf", WARN, TOPIC)
          << "failed to update snapshot after commit, reuse "
             "the existing snapshot for arangosearch link '"
          << id() << "'";

      return {};
    }

    // update reader
    TRI_ASSERT(_dataStore._reader != reader);
    _dataStore._reader = reader;

    // update last committed tick
    impl.tick(_lastCommittedTickStageOne);

    // invalidate query cache
    aql::QueryCache::instance()->invalidate(&(_collection.vocbase()),
                                            _viewGuid);

    LOG_TOPIC("7e328", DEBUG, TOPIC)
        << "successful sync of arangosearch link '" << id() << "', segments '"
        << reader->size() << "', docs count '" << reader->docs_count()
        << "', live docs count '" << reader->live_docs_count()
        << "', last operation tick low '" << _lastCommittedTickStageOne << "'"
        << "', last operation tick high '" << _lastCommittedTickStageTwo << "'";
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
Result IResearchLink::consolidateUnsafe(
    IResearchViewMeta::ConsolidationPolicy const& policy,
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
  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->lock() is valid

  try {
    auto const res =
        _dataStore._writer->consolidate(policy.policy(), nullptr, progress);
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
  // the lookup and unlink is valid for single-server only (that is the only
  // scenario where links are persisted) on coordinator and db-server the
  // IResearchView is immutable and lives in ClusterInfo therefore on
  // coordinator and db-server a new plan will already have an IResearchView
  // without the link this avoids deadlocks with ClusterInfo::loadPlan() during
  // lookup in ClusterInfo
  if (ServerState::instance()->isSingleServer()) {
    auto logicalView = _collection.vocbase().lookupView(_viewGuid);
    auto* view = LogicalView::cast<IResearchView>(logicalView.get());

    // may occur if the link was already unlinked from the view via another
    // instance this behavior was seen
    // user-access-right-drop-view-arangosearch-spec.js where the collection
    // drop was called through REST, the link was dropped as a result of the
    // collection drop call then the view was dropped via a separate REST call
    // then the vocbase was destroyed calling
    // collection close()-> link unload() -> link drop() due to collection
    // marked as dropped thus returning an error here will cause
    // ~TRI_vocbase_t() on RocksDB to receive an exception which is not handled
    // in the destructor the reverse happens during drop of a collection with
    // MMFiles i.e. collection drop() -> collection close()-> link unload(),
    // then link drop()
    if (!view) {
      LOG_TOPIC("f4e2c", WARN, TOPIC)
          << "unable to find arangosearch view '" << _viewGuid
          << "' while dropping arangosearch link '" << _id.id() << "'";
    } else {
      view->unlink(_collection.id());
      // unlink before reset() to release lock in view (if any)
    }
  }

  std::atomic_store(&_flushSubscription, {});
  // reset together with '_asyncSelf'
  _asyncSelf->reset();
  // the data-store is being deallocated, link use is no longer valid
  // (wait for all the view users to finish)

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
  return false;  // selectivity can only be determined per query since multiple
                 // fields are indexed
}

Result IResearchLink::init(velocypack::Slice definition,
                           InitCallback const& initCallback) {
  // disassociate from view if it has not been done yet
  if (!unload().ok()) {
    return {TRI_ERROR_INTERNAL, "failed to unload link"};
  }

  std::string error;
  IResearchLinkMeta meta;

  // definition should already be normalized and analyzers created if required
  if (!meta.init(_collection.vocbase().server(), definition, error,
                 _collection.vocbase().name())) {
    return {TRI_ERROR_BAD_PARAMETER,
            "error parsing view link parameters from json: " + error};
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  checkAnalyzerFeatures(meta);
#endif

  if (!definition.isObject()  // not object
      || !definition.get(StaticStrings::ViewIdField).isString()) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            "error finding view for link '" + std::to_string(_id.id()) + "'"};
  }

  if (!ServerState::instance()->isCoordinator()) {
    if (auto s = definition.get(StaticStrings::LinkError); s.isString()) {
      if (s.stringRef() == StaticStrings::LinkErrorOutOfSync) {
        // mark link as out of sync
        setOutOfSync();
      }
    }
  }

  auto viewId = definition.get(StaticStrings::ViewIdField).copyString();
  auto& vocbase = _collection.vocbase();
  bool const sorted = !meta._sort.empty();
  auto const& storedValuesColumns = meta._storedValues.columns();
  TRI_ASSERT(meta._sortCompression);
  auto const primarySortCompression =
      meta._sortCompression ? meta._sortCompression : getDefaultCompression();

  irs::index_reader_options readerOptions;
#ifdef USE_ENTERPRISE
  setupReaderEntepriseOptions(readerOptions, vocbase.server(), meta);
#endif
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
      if (DATA_SOURCE_TYPE != logicalView->type()) {
        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,  // code
                "error finding view: '" + viewId + "' for link '" +
                    std::to_string(_id.id()) + "' : no such view"};
      }

      auto* view =
          LogicalView::cast<IResearchViewCoordinator>(logicalView.get());

      if (!view) {
        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                "error finding view: '" + viewId + "' for link '" +
                    std::to_string(_id.id()) + "'"};
      }

      // ensue that this is a GUID (required by operator==(IResearchView))
      viewId = view->guid();

      // required for IResearchViewCoordinator which calls
      // IResearchLink::properties(...)
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

    bool const clusterWideLink =
        _collection.id() == _collection.planId() && _collection.isAStub();
    auto const clusterIsEnabled =
        vocbase.server().getFeature<ClusterFeature>().isEnabled();
    if (clusterIsEnabled) {
      auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();

      // upgrade step for old link definition without collection name
      // this could be received from  agency while shard of the collection was
      // moved (or added) to the server. New links already has collection name
      // set, but here we must get this name on our own
      if (meta._collectionName.empty()) {
        if (clusterWideLink) {  // could set directly
          LOG_TOPIC("86ecd", TRACE, TOPIC)
              << "Setting collection name '" << _collection.name()
              << "' for new link '" << this->id().id() << "'";
          meta._collectionName = _collection.name();
        } else {
          meta._collectionName =
              ci.getCollectionNameForShard(_collection.name());
          LOG_TOPIC("86ece", TRACE, TOPIC)
              << "Setting collection name '" << meta._collectionName
              << "' for new link '" << this->id().id() << "'";
        }
        if (ADB_UNLIKELY(meta._collectionName.empty())) {
          LOG_TOPIC_IF("67da6", WARN, TOPIC, meta.willIndexIdAttribute())
              << "Failed to init collection name for the link '"
              << this->id().id()
              << "'. Link will not index '_id' attribute. Please recreate the "
                 "link if this is necessary!";
        }

#ifdef USE_ENTERPRISE
        // enterprise name is not used in _id so should not be here!
        if (ADB_LIKELY(!meta._collectionName.empty())) {
          ClusterMethods::realNameFromSmartName(meta._collectionName);
        }
#endif
      }
    }

    if (!clusterWideLink) {
      if (meta._collectionName.empty() && !clusterIsEnabled &&
          vocbase.server()
              .getFeature<EngineSelectorFeature>()
              .engine()
              .inRecovery() &&
          meta.willIndexIdAttribute()) {
        LOG_TOPIC("f25ce", FATAL, TOPIC)
            << "Upgrade conflicts with recovering ArangoSearch link '"
            << this->id().id()
            << "' Please rollback the updated arangodb binary and finish the "
               "recovery "
               "first.";
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "Upgrade conflicts with recovering ArangoSearch link."
            " Please rollback the updated arangodb binary and finish the "
            "recovery "
            "first.");
      }
      // prepare data-store which can then update options
      // via the IResearchView::link(...) call
      auto const res = initDataStore(initCallback, meta._version, sorted,
                                     storedValuesColumns,
                                     primarySortCompression, readerOptions);

      if (!res.ok()) {
        return res;
      }
    }
    if (clusterIsEnabled) {
      auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
      // valid to call ClusterInfo (initialized in ClusterFeature::prepare())
      // even from DatabaseFeature::start()
      auto logicalView = ci.getView(vocbase.name(), viewId);

      // if there is no logicalView present yet then skip this step
      if (logicalView) {
        if (DATA_SOURCE_TYPE != logicalView->type()) {
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

        viewId = view->guid();  // ensue that this is a GUID (required by
                                // operator==(IResearchView))

        if (clusterWideLink) {  // cluster cluster-wide link
          auto shardIds = _collection.shardIds();

          // go through all shard IDs of the collection and try to link any
          // links missing links will be populated when they are created in the
          // per-shard collection
          if (shardIds) {
            for (auto& entry : *shardIds) {
              auto collection = vocbase.lookupCollection(
                  entry
                      .first);  // per-shard collections are always in 'vocbase'

              if (!collection) {
                continue;  // missing collection should be created after Plan
                           // becomes Current
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
      LOG_TOPIC("67dd6", DEBUG, TOPIC) << "Skipped link '" << this->id().id()
                                       << "' due to disabled cluster features.";
    }
  } else if (ServerState::instance()->isSingleServer()) {  // single-server link
    // prepare data-store which can then update options
    // via the IResearchView::link(...) call
    auto const res =
        initDataStore(initCallback, meta._version, sorted, storedValuesColumns,
                      primarySortCompression, readerOptions);

    if (!res.ok()) {
      return res;
    }

    auto logicalView = vocbase.lookupView(viewId);

    // if there is no logicalView present yet then skip this step
    if (logicalView) {
      if (DATA_SOURCE_TYPE != logicalView->type()) {
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

      viewId = view->guid();  // ensue that this is a GUID (required by
                              // operator==(IResearchView))

      auto const linkRes = view->link(_asyncSelf);
      if (!linkRes.ok()) {
        unload();  // unlock the directory
        return linkRes;
      }
    }
  }

  const_cast<std::string&>(_viewGuid) = std::move(viewId);
  const_cast<IResearchLinkMeta&>(_meta) = std::move(meta);
  _comparer.reset(_meta._sort);

  return {};
}

Result IResearchLink::initDataStore(
    InitCallback const& initCallback, uint32_t version, bool sorted,
    std::vector<IResearchViewStoredValues::StoredColumn> const& storedColumns,
    irs::type_info::type_id primarySortCompression,
    irs::index_reader_options const& readerOptions) {
  std::atomic_store(&_flushSubscription, {});
  // reset together with '_asyncSelf'
  _asyncSelf->reset();
  // the data-store is being deallocated, link use is no longer valid
  // (wait for all the view users to finish)

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
                _dataStore._path.u8string() + "' while initializing link '" +
                std::to_string(_id.id()) + "'"};
  }
  if (initCallback) {
    _dataStore._directory = std::make_unique<irs::mmap_directory>(
        _dataStore._path.u8string(), initCallback());
  } else {
    _dataStore._directory =
        std::make_unique<irs::mmap_directory>(_dataStore._path.u8string());
  }

  if (!_dataStore._directory) {
    return {TRI_ERROR_INTERNAL,
            "failed to instantiate data store directory with path '" +
                _dataStore._path.u8string() + "' while initializing link '" +
                std::to_string(_id.id()) + "'"};
  }

  switch (_engine->recoveryState()) {
    case RecoveryState::BEFORE:  // link is being opened before recovery
    case RecoveryState::DONE: {  // link is being created after recovery
      _dataStore._inRecovery.store(
          true, std::memory_order_release);  // will be adjusted in
                                             // post-recovery callback
      _dataStore._recoveryTickHigh = _dataStore._recoveryTickLow =
          _engine->recoveryTick();
      break;
    }

    case RecoveryState::IN_PROGRESS: {  // link is being created during recovery
      // both MMFiles and RocksDB will fill out link based on
      // actual data in linked collections, we can treat recovery as done
      _createdInRecovery = true;

      _dataStore._inRecovery.store(false, std::memory_order_release);
      _dataStore._recoveryTickHigh = _dataStore._recoveryTickLow =
          _engine->releasedTick();
      break;
    }
  }

  if (pathExists) {
    try {
      _dataStore._reader = irs::directory_reader::open(*(_dataStore._directory),
                                                       nullptr, readerOptions);

      if (!readTick(_dataStore._reader.meta().meta.payload(),
                    _dataStore._recoveryTickLow,
                    _dataStore._recoveryTickHigh)) {
        return {TRI_ERROR_INTERNAL,
                "failed to get last committed tick while initializing link '" +
                    std::to_string(id().id()) + "'"};
      }

      LOG_TOPIC("7e028", TRACE, TOPIC)
          << "successfully opened existing data store data store reader for "
          << "link '" << id() << "', docs count '"
          << _dataStore._reader->docs_count() << "', live docs count '"
          << _dataStore._reader->live_docs_count() << "', recovery tick low '"
          << _dataStore._recoveryTickLow << "' , recovery tick high '"
          << _dataStore._recoveryTickHigh << "'";
    } catch (irs::index_not_found const&) {
      // NOOP
    }
  }
  _lastCommittedTickStageOne = _dataStore._recoveryTickLow;
  _lastCommittedTickStageTwo = _lastCommittedTickStageOne;

  std::string name =
      std::string("flush subscription for ArangoSearch index '") +
      std::to_string(id().id()) + "'";
  _flushSubscription = std::make_shared<IResearchFlushSubscription>(
      _dataStore._recoveryTickLow, name);

  irs::index_writer::init_options options;
  // Set 256MB limit during recovery. Actual "operational" limit will be set
  // later when this link will be added to the view.
  options.segment_memory_max = 256 * (size_t{1} << 20);
  // Do not lock index, ArangoDB has its own lock.
  options.lock_repository = false;
  // Set comparator if requested.
  options.comparator = sorted ? &_comparer : nullptr;
  // Set index features.
  if (LinkVersion{version} < LinkVersion::MAX) {
    options.features = getIndexFeatures<irs::Norm>();
  } else {
    options.features = getIndexFeatures<irs::Norm2>();
  }
  // initialize commit callback
  options.meta_payload_provider = [this](uint64_t tick, irs::bstring& out) {
    // call from commit under lock _commitMutex (_dataStore._writer->commit())
    // update last tick
    auto lastCommittedTick = _commitStageOne ? &_lastCommittedTickStageOne
                                             : &_lastCommittedTickStageTwo;
    *lastCommittedTick = std::max(*lastCommittedTick, TRI_voc_tick_t{tick});
    // convert to BE
    tick = irs::numeric_utils::hton64(
        uint64_t{_commitStageOne ? _lastCommittedTickStageTwo
                                 : _lastCommittedTickStageOne});

    out.append(reinterpret_cast<irs::byte_type const*>(&tick), sizeof tick);
    if (_asyncFeature && !_asyncFeature->writeHighTick()) {
      return true;
    }
    auto version = irs::numeric_utils::hton32(
        static_cast<std::underlying_type_t<SegmentPayloadVersion>>(
            SegmentPayloadVersion::TwoStageTick));
    out.append(reinterpret_cast<irs::byte_type const*>(&version),
               sizeof version);
    tick = std::max(_lastCommittedTickStageOne, _lastCommittedTickStageTwo);
    tick = irs::numeric_utils::hton64(tick);
    out.append(reinterpret_cast<irs::byte_type const*>(&tick), sizeof tick);
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
  auto const encrypt =
      (nullptr != _dataStore._directory->attributes().encryption());
  options.column_info = [encrypt, compressionMap = std::move(compressionMap),
                         primarySortCompression](
                            const irs::string_ref& name) -> irs::column_info {
    if (name.null()) {
      return {primarySortCompression(), {}, encrypt};
    }
    auto compress = compressionMap.find(
        static_cast<std::string>(name));  // FIXME: remove cast after C++20
    if (compress != compressionMap.end()) {
      // do not waste resources to encrypt primary key column
      return {compress->second(),
              {},
              encrypt && (DocumentPrimaryKey::PK() != name)};
    }
    return {getDefaultCompression()(),
            {},
            encrypt && (DocumentPrimaryKey::PK() != name)};
  };

  auto openFlags = irs::OM_APPEND;
  if (!_dataStore._reader) {
    openFlags |= irs::OM_CREATE;
  }

  _dataStore._writer = irs::index_writer::make(*(_dataStore._directory), format,
                                               openFlags, options);

  if (!_dataStore._writer) {
    return {TRI_ERROR_INTERNAL,
            "failed to instantiate data store writer with path '" +
                _dataStore._path.u8string() + "' while initializing link '" +
                std::to_string(_id.id()) + "'"};
  }

  if (!_dataStore._reader) {
    _dataStore._writer->commit();  // initialize 'store'
    _dataStore._reader = irs::directory_reader::open(*(_dataStore._directory),
                                                     nullptr, readerOptions);
  }

  if (!_dataStore._reader) {
    _dataStore._writer.reset();

    return {TRI_ERROR_INTERNAL,
            "failed to instantiate data store reader with path '" +
                _dataStore._path.u8string() + "' while initializing link '" +
                std::to_string(_id.id()) + "'"};
  }

  if (!readTick(_dataStore._reader.meta().meta.payload(),
                _dataStore._recoveryTickLow, _dataStore._recoveryTickHigh)) {
    return {TRI_ERROR_INTERNAL,
            "failed to get last committed tick while initializing link '" +
                std::to_string(id().id()) + "'"};
  }

  LOG_TOPIC("7e128", TRACE, TOPIC)
      << "data store reader for link '" << id()
      << "' is initialized with recovery tick low '"
      << _dataStore._recoveryTickLow << "' and recovery tick high '"
      << _dataStore._recoveryTickHigh << "'";

  // reset data store meta, will be updated at runtime via properties(...)
  _dataStore._meta._cleanupIntervalStep = 0;        // 0 == disable
  _dataStore._meta._commitIntervalMsec = 0;         // 0 == disable
  _dataStore._meta._consolidationIntervalMsec = 0;  // 0 == disable
  _dataStore._meta._consolidationPolicy =
      IResearchViewMeta::ConsolidationPolicy();  // disable
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
      [asyncSelf = _asyncSelf, &flushFeature,
       asyncFeature = _asyncFeature]() -> Result {
        auto linkLock = asyncSelf->lock();
        // ensure link does not get deallocated before callback finishes
        if (!linkLock) {
          return {};  // link no longer in recovery state, i.e. during recovery
          // it was created and later dropped
        }
        if (!linkLock->_flushSubscription) {
          return {
              TRI_ERROR_INTERNAL,
              "failed to register flush subscription for arangosearch link '" +
                  std::to_string(linkLock->id().id()) + "'"};
        }

        auto& dataStore = linkLock->_dataStore;

        // recovery finished
        dataStore._inRecovery.store(linkLock->_engine->inRecovery(),
                                    std::memory_order_release);

        bool outOfSync = false;
        if (asyncFeature->linkSkippedDuringRecovery(linkLock->id())) {
          LOG_TOPIC("2721a", WARN, iresearch::TOPIC)
              << "marking link '" << linkLock->id().id() << "' as out of sync. "
              << "consider to drop and re-create the link in order to "
                 "synchronize it.";

          outOfSync = true;
        } else if (dataStore._recoveryTickLow >
                   linkLock->_engine->recoveryTick()) {
          LOG_TOPIC("5b59f", WARN, TOPIC)
              << "arangosearch link '" << linkLock->id()
              << "' is recovered at tick '" << dataStore._recoveryTickLow
              << "' less than storage engine tick '"
              << linkLock->_engine->recoveryTick()
              << "', it seems WAL tail was lost and link '" << linkLock->id()
              << "' is out of sync with the underlying collection '"
              << linkLock->collection().name()
              << "', consider to re-create the link in order to synchronize "
                 "them.";
          outOfSync = true;
        }

        if (outOfSync) {
          // mark link as out of sync
          linkLock->setOutOfSync();
          // persist "out of sync" flag in RocksDB. note: if this fails, it will
          // throw an exception and abort the recovery & startup.
          linkLock->_engine->changeCollection(linkLock->collection().vocbase(),
                                              linkLock->collection(), true);

          if (asyncFeature->failQueriesOnOutOfSync()) {
            // we cannot return an error from here as this would abort the
            // entire recovery and fail the startup.
            return {};
          }
        }

        LOG_TOPIC("5b59c", TRACE, TOPIC)
            << "starting sync for arangosearch link '" << linkLock->id() << "'";

        CommitResult code{CommitResult::UNDEFINED};
        auto res = linkLock->commitUnsafe(true, &code);

        LOG_TOPIC("0e0ca", TRACE, TOPIC)
            << "finished sync for arangosearch link '" << linkLock->id() << "'";

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

void IResearchLink::scheduleCommit(std::chrono::milliseconds delay) {
  CommitTask task;
  task.asyncLink = _asyncSelf;
  task.async = _asyncFeature;
  task.id = id();
  task.state = _maintenanceState;

  _maintenanceState->pendingCommits.fetch_add(1, std::memory_order_release);
  task.schedule(delay);
}

void IResearchLink::scheduleConsolidation(std::chrono::milliseconds delay) {
  ConsolidationTask task;
  task.asyncLink = _asyncSelf;
  task.async = _asyncFeature;
  task.id = id();
  task.state = _maintenanceState;
  task.progress = [link = _asyncSelf.get()] {
    return !link->terminationRequested();
  };

  _maintenanceState->pendingConsolidations.fetch_add(1,
                                                     std::memory_order_release);
  task.schedule(delay);
}

bool IResearchLink::exists(IResearchLink::Snapshot const& snapshot,
                           LocalDocumentId documentId,
                           TRI_voc_tick_t const* recoveryTick) const {
  if (recoveryTick) {
    if (*recoveryTick <= _dataStore._recoveryTickLow) {
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
  }
  PrimaryKeyFilter filter(documentId);
  auto prepared = filter.prepare(snapshot.getDirectoryReader());
  TRI_ASSERT(prepared);
  for (auto const& segment : snapshot.getDirectoryReader()) {
    auto executed = prepared->execute(segment);
    if (executed->next()) {
      return true;
    }
  }
  return false;
}

Result IResearchLink::insert(transaction::Methods& trx,
                             LocalDocumentId documentId, velocypack::Slice doc,
                             TRI_voc_tick_t const* recoveryTick) {
  TRI_ASSERT(_engine);
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

  auto insertImpl = [this, doc, documentId, &trx, engine = _engine]  //
      (irs::index_writer::documents_context & ctx) -> Result {
    try {
      FieldIterator body(trx, _meta._collectionName, _id);

      return insertDocument(ctx, trx, body, doc, documentId, _meta, id(),
                            engine);
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
    auto linkLock = _asyncSelf->lock();
    TRI_ASSERT(_dataStore);
    auto ctx = _dataStore._writer->documents();

    TRI_IF_FAILURE("ArangoSearch::MisreportCreationInsertAsFailed") {
      auto res = insertImpl(ctx);  // we need insert to succeed, so  we have
                                   // things to clean up in storage
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
    auto linkLock = _asyncSelf->lock();
    if (!linkLock) {
      // the current link is no longer valid
      // (checked after ReadLock acquisition)
      return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
              "failed to lock arangosearch link while inserting a "
              "document into arangosearch link '" +
                  std::to_string(id().id()) + "'"};
    }
    TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->lock() is valid

    // FIXME try to preserve optimization
    //// optimization for single-document insert-only transactions
    // if (trx.isSingleOperationTransaction()  // only for single-document
    // transactions
    //     && !_dataStore._inRecovery.load(std::memory_order_acquire)) {
    //   auto ctx = _dataStore._writer->documents();
    //   return insertImpl(ctx);
    // }

    auto ptr = std::make_unique<LinkTrxState>(std::move(linkLock),
                                              *(_dataStore._writer));

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx) {
      return {TRI_ERROR_INTERNAL,
              "failed to store state into a TransactionState for insert into "
              "arangosearch link '" +
                  std::to_string(id().id()) + "', tid '" +
                  std::to_string(state.id().id()) + "', revision '" +
                  std::to_string(documentId.id()) + "'"};
    }
    state.addPreCommitCallback(&_preCommitCallback);
    state.addPostCommitCallback(&_postCommitCallback);
  }

  return insertImpl(ctx->_ctx);
}

bool IResearchLink::isHidden() {
  // hide links unless we are on a DBServer
  return !ServerState::instance()->isDBServer();
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
    return false;  // IResearch View identifiers of current object and slice do
                   // not match
  }

  IResearchLinkMeta other;
  std::string errorField;

  // for db-server analyzer validation should have already passed on coordinator
  // (missing analyzer == no match)
  return other.init(_collection.vocbase().server(), slice, errorField,
                    _collection.vocbase().name()) &&
         _meta == other;
}

Result IResearchLink::properties(velocypack::Builder& builder,
                                 bool forPersistence) const {
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

  if (isOutOfSync()) {
    // link is out of sync - we need to report that
    builder.add(StaticStrings::LinkError,
                VPackValue(StaticStrings::LinkErrorOutOfSync));
  }

  return {};
}

Result IResearchLink::properties(IResearchViewMeta const& meta) {
  auto linkLock = _asyncSelf->lock();
  // '_dataStore' can be asynchronously modified
  if (!linkLock) {
    // the current link is no longer valid (checked after ReadLock acquisition)
    return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
            "failed to lock arangosearch link while modifying properties "
            "of arangosearch link '" +
                std::to_string(id().id()) + "'"};
  }
  IResearchLink::properties(std::move(linkLock), meta);
  return {};
}

void IResearchLink::properties(LinkLock linkLock,
                               IResearchViewMeta const& meta) {
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

  irs::index_writer::segment_options properties;
  properties.segment_count_max = meta._writebufferActive;
  properties.segment_memory_max = meta._writebufferSizeMax;

  static_assert(noexcept(linkLock->_dataStore._writer->options(properties)));
  linkLock->_dataStore._writer->options(properties);
}

Result IResearchLink::remove(transaction::Methods& trx,
                             LocalDocumentId documentId,
                             velocypack::Slice /*doc*/,
                             TRI_voc_tick_t const* recoveryTick) {
  TRI_ASSERT(_engine);
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
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* ctx = dynamic_cast<LinkTrxState*>(state.cookie(key));
#else
  auto* ctx = static_cast<LinkTrxState*>(state.cookie(key));
#endif

  if (!ctx) {
    // '_dataStore' can be asynchronously modified
    auto linkLock = _asyncSelf->lock();
    if (!linkLock) {  // the current link is no longer valid
      // (checked after ReadLock acquisition)
      return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
              "failed to lock arangosearch link while removing a document from "
              "arangosearch link '" +
                  std::to_string(id().id()) + "', tid '" +
                  std::to_string(state.id().id()) + "', revision '" +
                  std::to_string(documentId.id()) + "'"};
    }
    TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->lock() is valid

    auto ptr = irs::memory::make_unique<LinkTrxState>(std::move(linkLock),
                                                      *(_dataStore._writer));

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx) {
      return {TRI_ERROR_INTERNAL,
              "failed to store state into a TransactionState for remove from "
              "arangosearch link '" +
                  std::to_string(id().id()) + "', tid '" +
                  std::to_string(state.id().id()) + "', revision '" +
                  std::to_string(documentId.id()) + "'"};
    }
    state.addPreCommitCallback(&_preCommitCallback);
    state.addPostCommitCallback(&_postCommitCallback);
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
  auto linkLock = _asyncSelf->lock();
  // '_dataStore' can be asynchronously modified
  if (!linkLock) {
    LOG_TOPIC("f42dc", WARN, TOPIC)
        << "failed to lock arangosearch link while retrieving snapshot from "
           "arangosearch link '"
        << id() << "'";
    return {};  // return an empty reader
  }
  if (failQueriesOnOutOfSync() && linkLock->isOutOfSync()) {
    // link is out of sync, we cannot use it for querying
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC,
        std::string("link ") + std::to_string(linkLock->id().id()) +
            " is out of sync and needs to be recreated");
  }
  return snapshot(std::move(linkLock));
}

IResearchLink::Snapshot IResearchLink::snapshot(LinkLock linkLock) {
  TRI_ASSERT(linkLock);
  TRI_ASSERT(linkLock->_dataStore);
  // must be valid if _asyncSelf->lock() is valid
  return {std::move(linkLock),
          irs::directory_reader(linkLock->_dataStore._reader)};
  // return a copy of the current reader
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
  LOG_TOPIC_IF("5573c", ERR, TOPIC, name != _meta._collectionName)
      << "Collection name mismatch for arangosearch link '" << id() << "'."
      << " Meta name '" << _meta._collectionName << "' setting name '" << name
      << "'";
  TRI_ASSERT(name == _meta._collectionName);
  return false;
}

Result IResearchLink::unload() {
  // this code is used by the MMFilesEngine
  // if the collection is in the process of being removed then drop it from the
  // view
  // FIXME TODO remove once LogicalCollection::drop(...) will drop its indexes
  // explicitly
  if (_collection.deleted()  // collection deleted
      || TRI_vocbase_col_status_e::TRI_VOC_COL_STATUS_DELETED ==
             _collection.status()) {
    return drop();
  }

  std::atomic_store(&_flushSubscription, {});
  // reset together with '_asyncSelf'
  _asyncSelf->reset();
  // the data-store is being deallocated, link use is no longer valid
  // (wait for all the view users to finish)

  try {
    if (_dataStore) {
      _dataStore.resetDataStore();
    }
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
AnalyzerPool::ptr IResearchLink::findAnalyzer(
    AnalyzerPool const& analyzer) const {
  auto const it =
      _meta._analyzerDefinitions.find(irs::string_ref(analyzer.name()));

  if (it == _meta._analyzerDefinitions.end()) {
    return nullptr;
  }

  auto pool = *it;

  if (pool && analyzer == *pool) {
    return pool;
  }

  return nullptr;
}

IResearchLink::Stats IResearchLink::stats() const {
  auto lock = _asyncSelf->lock();
  if (!lock) {
    return {};
  }
  TRI_ASSERT(_dataStore);
  // copy of 'reader' is important to hold
  // reference to the current snapshot
  auto reader = _dataStore._reader;
  if (!reader) {
    return {};
  }
  Stats stats;
  stats.numBufferedDocs = _dataStore._writer->buffered_docs();
  stats.numSegments = reader->size();
  stats.numDocs = reader->docs_count();
  stats.numLiveDocs = reader->live_docs_count();
  stats.numFiles = 1;  // +1 for segments file
  auto visitor = [&stats](std::string const& /*name*/,
                          irs::segment_meta const& segment) noexcept {
    stats.indexSize += segment.size;
    stats.numFiles += segment.files.size();
    return true;
  };
  reader->meta().meta.visit_segments(visitor);
  return stats;
}

bool IResearchLink::failQueriesOnOutOfSync() const noexcept {
  return _asyncFeature->failQueriesOnOutOfSync();
}

bool IResearchLink::setOutOfSync() noexcept {
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

bool IResearchLink::isOutOfSync() const noexcept {
  // the out of sync flag is expected to be set either during the
  // recovery phase, or when a commit goes wrong.
  return _error.load(std::memory_order_acquire) == DataStoreError::kOutOfSync;
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

  dataPath /= "databases";
  dataPath /= "database-";
  dataPath += std::to_string(link.collection().vocbase().id());
  dataPath /= DATA_SOURCE_TYPE.name();
  dataPath += "-";
  // has to be 'id' since this can be a per-shard collection
  dataPath += std::to_string(link.collection().id().id());
  dataPath += "_";
  dataPath += std::to_string(link.id().id());

  return dataPath;
}

}  // namespace arangodb::iresearch
