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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchDataStore.h"
#include "IResearchDocument.h"
#include "IResearchFeature.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"

#include "RestServer/FlushFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <index/column_info.hpp>
#include <store/mmap_directory.hpp>
#include <store/store_utils.hpp>
#include <utils/encryption.hpp>
#include <utils/singleton.hpp>
#include <utils/file_utils.hpp>
#include <chrono>

namespace {

using namespace arangodb;
using namespace arangodb::iresearch;
using irs::async_utils::read_write_mutex;

class IResearchFlushSubscription final : public FlushSubscription {
 public:
  explicit IResearchFlushSubscription(TRI_voc_tick_t tick = 0) noexcept
    : _tick(tick) {
  }

  /// @brief earliest tick that can be released
  TRI_voc_tick_t tick() const noexcept override final {
    return _tick.load(std::memory_order_acquire);
  }

  void tick(TRI_voc_tick_t tick) noexcept {
    _tick.store(tick, std::memory_order_release);
  }

 private:
  std::atomic<TRI_voc_tick_t> _tick;
};

bool readTick(irs::bytes_ref const& payload, TRI_voc_tick_t& tick) noexcept {
  static_assert(
    // cppcheck-suppress duplicateExpression
    sizeof(uint64_t) == sizeof(TRI_voc_tick_t),
    "sizeof(uint64_t) != sizeof(TRI_voc_tick_t)");

  if (payload.size() != sizeof(uint64_t)) {
    return false;
  }

  std::memcpy(&tick, payload.c_str(), sizeof(uint64_t));
  tick = TRI_voc_tick_t(irs::numeric_utils::ntoh64(tick));

  return true;
}

struct ThreadGroupStats : std::tuple<size_t, size_t, size_t> {
  explicit ThreadGroupStats(
      std::tuple<size_t, size_t, size_t> const& stats)
    : std::tuple<size_t, size_t, size_t>(stats) {
  }
};

std::ostream& operator<<(std::ostream& out, ThreadGroupStats const& stats) {
  out << "Active=" << std::get<0>(stats)
      << ", Pending=" << std::get<1>(stats)
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
    LOG_TOPIC("eb0da", TRACE, arangodb::iresearch::TOPIC)
        << "scheduled a " << T::typeName()
        << " task for arangosearch link '" << id
        << "', delay '" << delay.count() << "'";

    LOG_TOPIC("eb0d2", TRACE, arangodb::iresearch::TOPIC)
        << T::typeName() << " pool: "
        << ThreadGroupStats(async->stats(T::threadGroup()));

    if (!link->terminationRequested()) {
      async->queue(T::threadGroup(), delay, static_cast<const T&>(*this));
    }
  }

  std::shared_ptr<MaintenanceState> state;
  IResearchFeature* async;
  IResearchDataStore::AsyncLinkPtr link;
  IndexId id;
}; // Task


using irs::async_utils::read_write_mutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts ArangoDB document into an IResearch data store
////////////////////////////////////////////////////////////////////////////////
template<typename FieldIteratorType, typename MetaType>
inline Result insertDocument(irs::index_writer::documents_context& ctx,
                             transaction::Methods const& trx,
                             FieldIteratorType& body,
                             velocypack::Slice const& document,
                             LocalDocumentId const& documentId,
                             MetaType const& meta,
                             IndexId id) {
  body.reset(document, meta);  // reset reusable container to doc

  if (!body.valid()) {
    return {};  // no fields to index
  }

  auto doc = ctx.insert();
  auto& field = *body;

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
    } field; // SortedField
    for (auto& sortField : meta._sort.fields()) {
      field.slice = get(document, sortField, VPackSlice::nullSlice());
      doc.insert<irs::Action::STORE_SORTED>(field);
    }
  }

  // Stored value field
  {
    StoredValue field(trx, meta._collectionName, document, id);
    for (auto const& column : meta._storedValues.columns()) {
      field.fieldName = column.name;
      field.fields = &column.fields;
      doc.insert<irs::Action::STORE>(field);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief total number of loaded iresearch indices
////////////////////////////////////////////////////////////////////////////////
std::atomic<size_t> LinksCount{0};

} // namespace

namespace arangodb {
namespace iresearch {

using namespace std::literals;

////////////////////////////////////////////////////////////////////////////////
/// @struct MaintenanceState
////////////////////////////////////////////////////////////////////////////////
struct MaintenanceState {
  std::atomic<size_t> pendingCommits{0};
  std::atomic<size_t> nonEmptyCommits{0};
  std::atomic<size_t> pendingConsolidations{0};
  std::atomic<size_t> noopConsolidationCount{0};
  std::atomic<size_t> noopCommitCount{0};
};

AsyncLinkHandle::AsyncLinkHandle(IResearchDataStore* link)
  : _link(link) {
  ++LinksCount;
}

AsyncLinkHandle::~AsyncLinkHandle() {
  --LinksCount;
}

void AsyncLinkHandle::reset() {
  _asyncTerminate.store(true); // mark long-running async jobs for terminatation
  _link.reset(); // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)
}

////////////////////////////////////////////////////////////////////////////////
/// @struct CommitTask
/// @brief represents a commit task
/// @note thread group 0 is dedicated to commit
////////////////////////////////////////////////////////////////////////////////
struct CommitTask : Task<CommitTask> {
  static constexpr ThreadGroup threadGroup() noexcept {
    return ThreadGroup::_0;
  }

  static constexpr const char* typeName() noexcept {
    return "commit";
  }

  void operator()();
  void finalize(IResearchDataStore* link, IResearchDataStore::CommitResult code);

  size_t cleanupIntervalCount{};
  std::chrono::milliseconds commitIntervalMsec{};
  std::chrono::milliseconds consolidationIntervalMsec{};
  size_t cleanupIntervalStep{};
}; // CommitTask

void CommitTask::finalize(
    IResearchDataStore* link,
    IResearchDataStore::CommitResult code) {
  constexpr size_t MAX_NON_EMPTY_COMMITS = 10;
  constexpr size_t MAX_PENDING_CONSOLIDATIONS = 3;

  if (code != IResearchDataStore::CommitResult::NO_CHANGES) {
    ++state->pendingCommits;
    schedule(commitIntervalMsec);

    if (code == IResearchDataStore::CommitResult::DONE) {
      state->noopCommitCount = 0;
      state->noopConsolidationCount = 0;

      if (state->pendingConsolidations < MAX_PENDING_CONSOLIDATIONS &&
          ++state->nonEmptyCommits > MAX_NON_EMPTY_COMMITS) {
        link->scheduleConsolidation(consolidationIntervalMsec);
        state->nonEmptyCommits = 0;
      }
    }
  } else {
    state->nonEmptyCommits = 0;
    ++state->noopCommitCount;

    for (auto count = state->pendingCommits.load(); count < 1; ) {
      if (state->pendingCommits.compare_exchange_weak(count, 1)) {
        schedule(commitIntervalMsec);
        break;
      }
    }
  }
}

void CommitTask::operator()() {
  const char runId = 0;
  --state->pendingCommits;

  if (link->terminationRequested()) {
    LOG_TOPIC("eba1a", DEBUG, iresearch::TOPIC)
        << "termination requested while committing the link '" << id
        << "', runId '" << size_t(&runId) << "'";
    return;
  }

  auto linkLock = link->try_lock();

  if (!linkLock.owns_lock()) {
    LOG_TOPIC("eb0de", DEBUG, iresearch::TOPIC)
        << "failed to acquire the lock while committing the link '" << id
        << "', runId '" << size_t(&runId) << "'";

    // blindly reschedule commit task
    ++state->pendingCommits;
    schedule(commitIntervalMsec);
    return;
  }

  auto* link = this->link->get();

  if (!link) {
    LOG_TOPIC("ebada", DEBUG, iresearch::TOPIC)
        << "link '" << id << "' is no longer valid, run id '" << size_t(&runId) << "'";
    return;
  }

  IResearchDataStore::CommitResult code = IResearchDataStore::CommitResult::UNDEFINED;

  auto reschedule = scopeGuard([&code, link, this]() noexcept{
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

    TRI_ASSERT(link->_dataStore); // must be valid if _asyncSelf->get() is valid
    READ_LOCKER(lock, link->_dataStore._mutex); // '_meta' can be asynchronously modified
    auto& meta = link->_dataStore._meta;

    commitIntervalMsec = std::chrono::milliseconds(meta._commitIntervalMsec);
    consolidationIntervalMsec = std::chrono::milliseconds(meta._consolidationIntervalMsec);
    cleanupIntervalStep = meta._cleanupIntervalStep;
  }

  if (std::chrono::milliseconds::zero() == commitIntervalMsec) {
    reschedule.cancel();

    LOG_TOPIC("eba4a", DEBUG, iresearch::TOPIC)
        << "sync is disabled for the link '" << id
        << "', runId '" << size_t(&runId) << "'";
    return;
  }

  TRI_IF_FAILURE("IResearchCommitTask::commitUnsafe") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  auto const syncStart = std::chrono::steady_clock::now(); // FIXME: add sync durations to metrics
  auto res = link->commitUnsafe(false, &code); // run commit ('_asyncSelf' locked by async task)

  if (res.ok()) {
    LOG_TOPIC("7e323", TRACE, iresearch::TOPIC)
        << "successful sync of arangosearch link '" << id
        << "', run id '" << size_t(&runId) << "', took: " << 
        std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - syncStart).count()
        << "ms";
    if (code == IResearchDataStore::CommitResult::DONE) {
      if (cleanupIntervalStep && cleanupIntervalCount++ > cleanupIntervalStep) { // if enabled
        cleanupIntervalCount = 0;

        TRI_IF_FAILURE("IResearchCommitTask::cleanupUnsafe") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        auto const cleanupStart = std::chrono::steady_clock::now(); // FIXME: add cleanup durations to metrics
        res = link->cleanupUnsafe(); // run cleanup ('_asyncSelf' locked by async task)

        if (res.ok()) {
          LOG_TOPIC("7e821", TRACE, iresearch::TOPIC)
              << "successful cleanup of arangosearch link '" << id
              << "', run id '" << size_t(&runId) << "', took: " << 
              std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - cleanupStart).count()
              << "ms";
        } else {
          LOG_TOPIC("130de", WARN, iresearch::TOPIC)
              << "error after running for " <<
              std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - cleanupStart).count()
              << "ms while cleaning up arangosearch link '" << id
              << "', run id '" << size_t(&runId)
              << "': " << res.errorNumber() << " " << res.errorMessage();
        }
      }
    }
  } else {
    LOG_TOPIC("8377b", WARN, iresearch::TOPIC)
        << "error after running for " <<
        std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - syncStart).count()
        << "ms while committing arangosearch link '" << link->id()
        << "', run id '" << size_t(&runId)
        << "': " << res.errorNumber() << " " << res.errorMessage();
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

  static constexpr const char* typeName() noexcept {
    return "consolidation";
  }

  void operator()();

  irs::merge_writer::flush_progress_t progress;
  IResearchViewMeta::ConsolidationPolicy consolidationPolicy;
  std::chrono::milliseconds consolidationIntervalMsec{};
}; // ConsolidationTask

void ConsolidationTask::operator()() {
  const char runId = 0;
  --state->pendingConsolidations;

  if (link->terminationRequested()) {
    LOG_TOPIC("eba2a", DEBUG, iresearch::TOPIC)
        << "termination requested while consolidating the link '" << id
        << "', runId '" << size_t(&runId) << "'";
    return;
  }

  auto linkLock = link->try_lock();

  if (!linkLock.owns_lock()) {
    LOG_TOPIC("eb0dc", DEBUG, iresearch::TOPIC)
        << "failed to acquire the lock while consolidating the link '" << id
        << "', run id '" << size_t(&runId) << "'";

    // blindly reschedule consolidation task
    ++state->pendingConsolidations;
    schedule(consolidationIntervalMsec);
    return;
  }

  auto* link = this->link->get();

  if (!link) {
    LOG_TOPIC("eb0d1", DEBUG, iresearch::TOPIC)
        << "link '" << id << "' is no longer valid, run id '" << size_t(&runId) << "'";
    return;
  }

  auto reschedule = scopeGuard([this]() noexcept {
    try {
      for (auto count = state->pendingConsolidations.load(); count < 1; ) {
        if (state->pendingConsolidations.compare_exchange_weak(count, count + 1)) {
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

    TRI_ASSERT(link->_dataStore); // must be valid if _asyncSelf->get() is valid
    READ_LOCKER(lock, link->_dataStore._mutex); // '_meta' can be asynchronously modified
    auto& meta = link->_dataStore._meta;

    consolidationPolicy = meta._consolidationPolicy;
    consolidationIntervalMsec = std::chrono::milliseconds(meta._consolidationIntervalMsec);
  }

  if (std::chrono::milliseconds::zero() == consolidationIntervalMsec // disabled via interval
      || !consolidationPolicy.policy()) { // disabled via policy
    reschedule.cancel();

    LOG_TOPIC("eba3a", DEBUG, iresearch::TOPIC)
        << "consolidation is disabled for the link '" << id
        << "', runId '" << size_t(&runId) << "'";
    return;
  }

  constexpr size_t MAX_NOOP_COMMITS = 10;
  constexpr size_t MAX_NOOP_CONSOLIDATIONS = 10;

  if (state->noopCommitCount < MAX_NOOP_COMMITS &&
      state->noopConsolidationCount < MAX_NOOP_CONSOLIDATIONS) {
    ++state->pendingConsolidations;
    schedule(consolidationIntervalMsec);
  }

  TRI_IF_FAILURE("IResearchConsolidationTask::consolidateUnsafe") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // run consolidation ('_asyncSelf' locked by async task)
  auto const consolidationStart = std::chrono::steady_clock::now(); // FIXME: add consolidation durations to metrics
  bool emptyConsolidation = false;
  auto const res = link->consolidateUnsafe(consolidationPolicy, progress, emptyConsolidation);

  if (res.ok()) {
    if (emptyConsolidation) {
      ++state->noopConsolidationCount;
    } else {
       state->noopConsolidationCount = 0;
    }
    LOG_TOPIC("7e828", TRACE, iresearch::TOPIC)
        << "successful consolidation of arangosearch link '" << link->id()
        << "', run id '" << size_t(&runId) << "', took: " << 
        std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - consolidationStart).count()
        << "ms";
  } else {
    LOG_TOPIC("bce4f", DEBUG, iresearch::TOPIC)
        << "error after running for " <<
        std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - consolidationStart).count()
        << "ms while consolidating arangosearch link '" << link->id()
        << "', run id '" << size_t(&runId)
        << "': " << res.errorNumber() << " " << res.errorMessage();
  }
}


IResearchDataStore::IResearchDataStore(IndexId iid, LogicalCollection& collection) : 
      _collection(collection),
      _engine(nullptr),
      _asyncFeature(&collection.vocbase().server().getFeature<IResearchFeature>()),
      _asyncSelf(std::make_shared<AsyncLinkHandle>(nullptr)),  // mark as data store not initialized
      _maintenanceState(std::make_shared<MaintenanceState>()),
      _id(iid),
      _lastCommittedTick(0) {
    auto* key = this;

  // initialize transaction callback
  _trxCallback = [key](transaction::Methods& trx, transaction::Status status)->void {
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
      auto& ctx = dynamic_cast<IResearchTrxState&>(*prev);
#else
      auto& ctx = static_cast<IResearchTrxState&>(*prev);
#endif

      if (transaction::Status::COMMITTED != status) { // rollback
        ctx.reset();
      } else {
        ctx._ctx.tick(state->lastOperationTick());
      }
    }

    prev.reset();
  };
}

/*
IResearchDataStore::IResearchDataStore(IndexId iid, LogicalCollection& collection,
                                       IResearchLinkMeta&& meta)
  : IResearchDataStore(iid, collection) {
  const_cast<IResearchLinkMeta&>(_meta) = std::move(meta);
  _comparer.reset(_meta._sort);
}
*/
 
IResearchDataStore::Snapshot IResearchDataStore::snapshot() const {
  // '_dataStore' can be asynchronously modified
  auto lock = _asyncSelf->lock();

  if (!_asyncSelf.get()) {
    LOG_TOPIC("f42dc", WARN, iresearch::TOPIC)
        << "failed to lock arangosearch link while retrieving snapshot from "
           "arangosearch link '"
        << id() << "'";

    return Snapshot();  // return an empty reader
  }

  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  return Snapshot(std::move(lock),
                  irs::directory_reader(_dataStore._reader));  // return a copy of the current reader
}


void IResearchDataStore::scheduleCommit(std::chrono::milliseconds delay) {
  CommitTask task;
  task.link = _asyncSelf;
  task.async = _asyncFeature;
  task.id = id();
  task.state = _maintenanceState;

  ++_maintenanceState->pendingCommits;
  task.schedule(delay);
}

void IResearchDataStore::scheduleConsolidation(std::chrono::milliseconds delay) {
  ConsolidationTask task;
  task.link = _asyncSelf;
  task.async = _asyncFeature;
  task.id = id();
  task.state = _maintenanceState;
  task.progress = [link = _asyncSelf.get()](){
    return !link->terminationRequested();
  };

  ++_maintenanceState->pendingConsolidations;
  task.schedule(delay);
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchDataStore::cleanupUnsafe() {
  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

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


Result IResearchDataStore::commit(bool wait /*= true*/) {
  auto lock = _asyncSelf->lock(); // '_dataStore' can be asynchronously modified

  if (!_asyncSelf.get()) {
    // the current link is no longer valid (checked after ReadLock acquisition)

    return {
        TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
        "failed to lock arangosearch store while commiting arangosearch index '" +
            std::to_string(id().id()) + "'"};
  }

  [[maybe_unused]] CommitResult code;
  return commitUnsafe(wait, &code);
}


////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchDataStore::commitUnsafe(bool wait, CommitResult* code) {
  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

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
      *code = _dataStore._writer->commit()
        ? CommitResult::DONE
        : CommitResult::NO_CHANGES;
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
             "the existing snapshot for arangosearch link '" << id() << "'";

      return {};
    }

    // update reader
    TRI_ASSERT(_dataStore._reader != reader);
    _dataStore._reader = reader;

    // update last committed tick
    impl.tick(_lastCommittedTick);

    afterCommit();

    LOG_TOPIC("7e328", DEBUG, iresearch::TOPIC)
        << "successful sync of arangosearch link '" << id()
        << "', segments '" << reader->size()
        << "', docs count '" << reader->docs_count()
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
                std::to_string(id().id()) };
  }

  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchDataStore::consolidateUnsafe(
    IResearchViewMeta::ConsolidationPolicy const& policy,
    irs::merge_writer::flush_progress_t const& progress,
    bool& emptyConsolidation) {
  emptyConsolidation = false;

  if (!policy.policy()) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        "unset consolidation policy while executing consolidation policy '" +
            policy.properties().toString() + "' on arangosearch link '" +
            std::to_string(id().id()) + "'"};
  }

  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

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

Result IResearchDataStore::shutdownDataStore() {
  std::atomic_store(&_flushSubscription, {}); // reset together with '_asyncSelf'
  _asyncSelf->reset(); // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  try {
    if (_dataStore) {
      _dataStore.resetDataStore();
    }
  } catch (basics::Exception const& e) {
    return {e.code(), "caught exception while unloading arangosearch data store '" +
                          std::to_string(id().id()) + "': " + e.what()};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while unloading arangosearch data store '" +
                std::to_string(id().id()) + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while unloading arangosearch data store '" +
                std::to_string(id().id()) + "'"};
  }
  return {};
}

Result IResearchDataStore::deleteDataStore() {
  auto res = shutdownDataStore();
  if (res.fail()) {
    return res;
  }
  bool exists;

  // remove persisted data store directory if present
  if (!irs::file_utils::exists_directory(exists, _dataStore._path.c_str()) ||
      (exists && !irs::file_utils::remove(_dataStore._path.c_str()))) {
    return {TRI_ERROR_INTERNAL, "failed to remove arangosearch data store'" +
            std::to_string(id().id()) + "'"};
  }
  return {};
}

Result IResearchDataStore::initDataStore(InitCallback const& initCallback, uint32_t version, bool sorted,
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

  bool pathExists;

  _dataStore._path = getPersistedPath(dbPathFeature, *this);

  // must manually ensure that the data store directory exists (since not using
  // a lockfile)
  if (irs::file_utils::exists_directory(pathExists, _dataStore._path.c_str()) && !pathExists &&
      !irs::file_utils::mkdir(_dataStore._path.c_str(), true)) {
    return {TRI_ERROR_CANNOT_CREATE_DIRECTORY,
            "failed to create data store directory with path '" +
                _dataStore._path.u8string() + "' while initializing link '" +
                std::to_string(_id.id()) + "'"};
  }

  if (initCallback) {
    _dataStore._directory = std::make_unique<irs::mmap_directory>(_dataStore._path.u8string(), initCallback());
  } else {
    _dataStore._directory = std::make_unique<irs::mmap_directory>(_dataStore._path.u8string());
  }

  if (!_dataStore._directory) {
    return {TRI_ERROR_INTERNAL,
            "failed to instantiate data store directory with path '" +
                _dataStore._path.u8string() + "' while initializing link '" +
                std::to_string(_id.id()) + "'"};
  }

  switch (_engine->recoveryState()) {
    case RecoveryState::BEFORE:       // link is being opened before recovery
    case RecoveryState::DONE: {       // link is being created after recovery
      _dataStore._inRecovery = true;  // will be adjusted in post-recovery callback
      _dataStore._recoveryTick = _engine->recoveryTick();
      break;
    }

    case RecoveryState::IN_PROGRESS: {  // link is being created during recovery
      _dataStore._inRecovery = false;
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
  _flushSubscription.reset(new IResearchFlushSubscription(_dataStore._recoveryTick));

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
  for (auto c : storedColumns) {
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
  options.column_info = [encrypt, comprMap = std::move(compressionMap),
                         primarySortCompression](const irs::string_ref& name) -> irs::column_info {
    if (name.null()) {
      return {primarySortCompression(), {}, encrypt};
    }
    auto compress = comprMap.find(static_cast<std::string>(name));  // FIXME: remove cast after C++20
    if (compress != comprMap.end()) {
      // do not waste resources to encrypt primary key column
      return {compress->second(), {}, encrypt && (DocumentPrimaryKey::PK() != name)};
    } else {
      return {getDefaultCompression()(), {}, encrypt && (DocumentPrimaryKey::PK() != name)};
    }
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
                _dataStore._path.u8string() + "' while initializing link '" +
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
                _dataStore._path.u8string() + "' while initializing link '" +
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
        auto lock = asyncSelf->lock();  // ensure link does not get deallocated before callback finishes
        auto* link = asyncSelf->get();

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
        dataStore._inRecovery = link->_engine->inRecovery();

        LOG_TOPIC("5b59c", TRACE, iresearch::TOPIC)
            << "starting sync for arangosearch link '" << link->id() << "'";

        [[maybe_unused]] CommitResult code;
        auto res = link->commitUnsafe(true, &code);

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

Result IResearchDataStore::properties(IResearchViewMeta const& meta) {
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
    WRITE_LOCKER(lock, _dataStore._mutex); // '_meta' can be asynchronously modified
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

Result IResearchDataStore::remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const /*doc*/) {
  TRI_ASSERT(_engine);
  TRI_ASSERT(trx.state());

  auto& state = *(trx.state());

  TRI_ASSERT(!state.hasHint(transaction::Hints::Hint::INDEX_CREATION));

  if (_dataStore._inRecovery && _engine->recoveryTick() <= _dataStore._recoveryTick) {
    LOG_TOPIC("7d228", TRACE, iresearch::TOPIC)
      << "skipping 'removal', operation tick '" << _engine->recoveryTick()
      << "', recovery tick '" << _dataStore._recoveryTick << "'";

    return {};
  }

  auto* key = this;

// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* ctx = dynamic_cast<IResearchTrxState*>(state.cookie(key));
#else
  auto* ctx = static_cast<IResearchTrxState*>(state.cookie(key));
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

    TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

    auto ptr = irs::memory::make_unique<IResearchTrxState>(
      std::move(lock), *(_dataStore._writer));

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
  // if an exception occurs below than the transaction is droped including all
  // all of its fid stores, no impact to iResearch View data integrity
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
template<typename FieldIteratorType, typename MetaType>
Result IResearchDataStore::insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const doc,
    MetaType const& meta) {
  TRI_ASSERT(_engine);
  TRI_ASSERT(trx.state());

  auto& state = *(trx.state());

  if (_dataStore._inRecovery && _engine->recoveryTick() <= _dataStore._recoveryTick) {
    LOG_TOPIC("7c228", TRACE, iresearch::TOPIC)
      << "skipping 'insert', operation tick '" << _engine->recoveryTick()
      << "', recovery tick '" << _dataStore._recoveryTick << "'";

    return {};
  }

  auto insertImpl = [&meta, &trx, &doc, &documentId, id = id()](
                        irs::index_writer::documents_context& ctx) -> Result {
    try {
      FieldIteratorType body(trx, meta._collectionName, id);

      return insertDocument(ctx, trx, body, doc, documentId, meta, id);
    } catch (basics::Exception const& e) {
      return {
          e.code(),
          "caught exception while inserting document into arangosearch index '" +
              std::to_string(id.id()) + "', revision '" +
              std::to_string(documentId.id()) + "': " + e.what()};
    } catch (std::exception const& e) {
      return {
          TRI_ERROR_INTERNAL,
          "caught exception while inserting document into arangosearch index '" +
              std::to_string(id.id()) + "', revision '" +
              std::to_string(documentId.id()) + "': " + e.what()};
    } catch (...) {
      return {
          TRI_ERROR_INTERNAL,
          "caught exception while inserting document into arangosearch index '" +
              std::to_string(id.id()) + "', revision '" +
              std::to_string(documentId.id()) + "'"};
    }
  };

  TRI_IF_FAILURE("ArangoSearch::BlockInsertsWithoutIndexCreationHint") {
    if (!state.hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
      return Result(TRI_ERROR_DEBUG);
    }
  }

  if (state.hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
    auto lock = _asyncSelf->lock();
    auto ctx = _dataStore._writer->documents();

    TRI_IF_FAILURE("ArangoSearch::MisreportCreationInsertAsFailed") {
      auto res = insertImpl(ctx); // we need insert to succeed, so  we have things to cleanup in storage
      if (res.fail()) {
        return res;
      }
      return Result(TRI_ERROR_DEBUG);
    }
    return insertImpl(ctx);
  }
  auto* key = this;

// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* ctx = dynamic_cast<IResearchTrxState*>(state.cookie(key));
#else
  auto* ctx = static_cast<IResearchTrxState*>(state.cookie(key));
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

    TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

//FIXME try to preserve optimization
//    // optimization for single-document insert-only transactions
//    if (trx.isSingleOperationTransaction() // only for single-docuemnt transactions
//        && !_dataStore._inRecovery) {
//      auto ctx = _dataStore._writer->documents();
//
//      return insertImpl(ctx);
//    }

    auto ptr = std::make_unique<IResearchTrxState>(std::move(lock), *(_dataStore._writer));

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

void IResearchDataStore::afterTruncate(TRI_voc_tick_t tick,
                                  transaction::Methods* trx) {
  auto lock = _asyncSelf->lock();  // '_dataStore' can be asynchronously modified

  TRI_IF_FAILURE("ArangoSearchTruncateFailure") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (!_asyncSelf.get()) {
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
    auto* ctx = dynamic_cast<IResearchTrxState*>(state.cookie(key));
#else
    auto* ctx = static_cast<IResearchTrxState*>(state.cookie(key));
#endif

    if (ctx) {
      ctx->reset(); // throw away all pending operations as clear will overwrite them all
      state.cookie(key, nullptr); // force active segment release to allow commit go and avoid deadlock in clear
    }
  }

  auto const lastCommittedTick = _lastCommittedTick;
  bool recoverCommittedTick = true;

  auto lastCommittedTickGuard = irs::make_finally([lastCommittedTick, this, &recoverCommittedTick]()->void {
      if (recoverCommittedTick) {
        _lastCommittedTick = lastCommittedTick;
      }
    });

  try {
    _dataStore._writer->clear(tick);
    recoverCommittedTick = false; //_lastCommittedTick now updated and data is written to storage

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

    auto subscription = std::atomic_load(&_flushSubscription);

    if (subscription) {
      auto& impl = static_cast<IResearchFlushSubscription&>(*subscription);

      impl.tick(_lastCommittedTick);
    }
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

IResearchDataStore::Stats IResearchDataStore::stats() const {
  Stats stats;

  // '_dataStore' can be asynchronously modified
  auto lock = _asyncSelf->lock();

  if (_dataStore) {
    stats.numBufferedDocs = _dataStore._writer->buffered_docs();

    // copy of 'reader' is important to hold
    // reference to the current snapshot
    auto reader = _dataStore._reader;

    if (!reader) {
      return {};
    }

    stats.numSegments = reader->size();
    stats.docsCount = reader->docs_count();
    stats.liveDocsCount = reader->live_docs_count();
    stats.numFiles = 1; // +1 for segments file

    auto visitor = [&stats](std::string const& /*name*/,
                            irs::segment_meta const& segment) noexcept {
      stats.indexSize += segment.size;
      stats.numFiles += segment.files.size();
      return true;
    };

    reader->meta().meta.visit_segments(visitor);
  }

  return stats;
}

void IResearchDataStore::toVelocyPackStats(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());

  auto const stats = this->stats();

  builder.add("numBufferedDocs", VPackValue(stats.numBufferedDocs));
  builder.add("numDocs", VPackValue(stats.docsCount));
  builder.add("numLiveDocs", VPackValue(stats.liveDocsCount));
  builder.add("numSegments", VPackValue(stats.numSegments));
  builder.add("numFiles", VPackValue(stats.numFiles));
  builder.add("indexSize", VPackValue(stats.indexSize));
}


////////////////////////////////////////////////////////////////////////////////
/// @brief compute the data path to user for iresearch data store
///        get base path from DatabaseServerFeature (similar to MMFilesEngine)
///        the path is hardcoded to reside under:
///        <DatabasePath>/<IResearchLink::type()>-<link id>
///        similar to the data path calculation for collections
////////////////////////////////////////////////////////////////////////////////
irs::utf8_path getPersistedPath(DatabasePathFeature const& dbPathFeature,
                                IResearchDataStore const& link) {
  irs::utf8_path dataPath(dbPathFeature.directory());
  static const std::string subPath("databases");
  static const std::string dbPath("database-");

  dataPath /= subPath;
  dataPath /= dbPath;
  dataPath += std::to_string(link.collection().vocbase().id());
  dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
  dataPath += "-";
  dataPath += std::to_string(link.collection().id().id());  // has to be 'id' since this can be a per-shard collection
  dataPath += "_";
  dataPath += std::to_string(link.id().id());

  return dataPath;
}

template Result IResearchDataStore::insert<FieldIterator, IResearchLinkMeta>(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const doc,
    IResearchLinkMeta const& meta);

template Result IResearchDataStore::insert<InvertedIndexFieldIterator, InvertedIndexFieldMeta>(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const doc,
    InvertedIndexFieldMeta const& meta);

} // namespace iresearch
} // namespace arangodb
