//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <index/column_info.hpp>
#include <store/mmap_directory.hpp>
#include <store/store_utils.hpp>
#include <utils/encryption.hpp>
#include <utils/lz4compression.hpp>
#include <utils/singleton.hpp>

#include "IResearchLink.h"

#include "Aql/QueryCache.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchPrimaryKeyFilter.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/VelocyPackHelper.h"
#include "MMFiles/MMFilesCollection.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

using namespace std::literals;

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief the storage format used with IResearch writers
////////////////////////////////////////////////////////////////////////////////
const irs::string_ref IRESEARCH_STORE_FORMAT("1_2");

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief container storing the link state for a given TransactionState
////////////////////////////////////////////////////////////////////////////////
struct LinkTrxState final : public arangodb::TransactionState::Cookie {
  irs::index_writer::documents_context _ctx;
  std::unique_lock<ReadMutex> _linkLock;  // prevent data-store deallocation (lock @ AsyncSelf)
  arangodb::iresearch::PrimaryKeyFilterContainer _removals;  // list of document removals

  LinkTrxState(std::unique_lock<ReadMutex>&& linkLock, irs::index_writer& writer) noexcept
      : _ctx(writer.documents()), _linkLock(std::move(linkLock)) {
    TRI_ASSERT(_linkLock.owns_lock());
  }

  virtual ~LinkTrxState() noexcept {
    if (_removals.empty()) {
      return;  // nothing to do
    }

    try {
      // hold references even after transaction
      _ctx.remove(irs::filter::make<arangodb::iresearch::PrimaryKeyFilterContainer>(
          std::move(_removals)));
    } catch (std::exception const& e) {
      LOG_TOPIC("eb463", ERR, arangodb::iresearch::TOPIC)
          << "caught exception while applying accumulated removals: " << e.what();
      IR_LOG_EXCEPTION();
    } catch (...) {
      LOG_TOPIC("14917", WARN, arangodb::iresearch::TOPIC)
          << "caught exception while applying accumulated removals";
      IR_LOG_EXCEPTION();
    }
  }

  operator irs::index_writer::documents_context&() noexcept { return _ctx; }

  void remove(arangodb::LocalDocumentId const& value) {
    _ctx.remove(_removals.emplace(value));
  }

  void reset() noexcept {
    _removals.clear();
    _ctx.reset();
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief compute the data path to user for iresearch data store
///        get base path from DatabaseServerFeature (similar to MMFilesEngine)
///        the path is hardcoded to reside under:
///        <DatabasePath>/<IResearchLink::type()>-<link id>
///        similar to the data path calculation for collections
////////////////////////////////////////////////////////////////////////////////
irs::utf8_path getPersistedPath(arangodb::DatabasePathFeature const& dbPathFeature,
                                arangodb::iresearch::IResearchLink const& link) {
  irs::utf8_path dataPath(dbPathFeature.directory());
  static const std::string subPath("databases");
  static const std::string dbPath("database-");

  dataPath /= subPath;
  dataPath /= dbPath;
  dataPath += std::to_string(link.collection().vocbase().id());
  dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
  dataPath += "-";
  dataPath += std::to_string(link.collection().id());  // has to be 'id' since this can be a per-shard collection
  dataPath += "_";
  dataPath += std::to_string(link.id());

  return dataPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts ArangoDB document into an IResearch data store
////////////////////////////////////////////////////////////////////////////////
inline arangodb::Result insertDocument(irs::index_writer::documents_context& ctx,
                                       arangodb::iresearch::FieldIterator& body,
                                       arangodb::velocypack::Slice const& document,
                                       arangodb::LocalDocumentId const& documentId,
                                       arangodb::iresearch::IResearchLinkMeta const& meta,
                                       TRI_idx_iid_t id) {
  body.reset(document, meta);  // reset reusable container to doc

  if (!body.valid()) {
    return {};  // no fields to index
  }

  auto doc = ctx.insert();
  auto& field = *body;

  // User fields
  while (body.valid()) {
    if (arangodb::iresearch::ValueStorage::NONE == field._storeValues) {
      doc.insert<irs::Action::INDEX>(field);
    } else {
      doc.insert<irs::Action::INDEX_AND_STORE>(field);
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
      field.slice = arangodb::iresearch::get(document, sortField, VPackSlice::nullSlice());
      doc.insert<irs::Action::STORE_SORTED>(field);
    }
  }

  // System fields

  // Indexed and Stored: LocalDocumentId
  auto docPk = arangodb::iresearch::DocumentPrimaryKey::encode(documentId);

  // reuse the 'Field' instance stored inside the 'FieldIterator'
  arangodb::iresearch::Field::setPkValue(const_cast<arangodb::iresearch::Field&>(field), docPk);
  doc.insert<irs::Action::INDEX_AND_STORE>(field);

  if (!doc) {
    return {
      TRI_ERROR_INTERNAL,
      "failed to insert document into arangosearch link '" + std::to_string(id) +
      "', revision '" + std::to_string(documentId.id()) + "'"
    };
  }

  return {};
}

class IResearchFlushSubscription final : public arangodb::FlushSubscription {
 public:
  explicit IResearchFlushSubscription(TRI_voc_tick_t tick = 0) noexcept
    : _tick(tick) {
  }

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
  static_assert(
    // cppcheck-suppress duplicateExpression
    sizeof(uint64_t) == sizeof(TRI_voc_tick_t),
    "sizeof(uint64_t) != sizeof(TRI_voc_tick_t)"
  );

  if (payload.size() != sizeof(uint64_t)) {
    return false;
  }

  std::memcpy(&tick, payload.c_str(), sizeof(uint64_t));
  tick = TRI_voc_tick_t(irs::numeric_utils::ntoh64(tick));

  return true;
}

}  // namespace

namespace arangodb {
namespace iresearch {

IResearchLink::IResearchLink(
    TRI_idx_iid_t iid,
    LogicalCollection& collection)
  : _engine(nullptr),
    _asyncFeature(nullptr),
    _asyncSelf(irs::memory::make_unique<AsyncLinkPtr::element_type>(nullptr)), // mark as data store not initialized
    _asyncTerminate(false),
    _collection(collection),
    _id(iid),
    _lastCommittedTick(0),
    _createdInRecovery(false) {
  auto* key = this;

  // initialize transaction callback
  _trxCallback = [key](transaction::Methods& trx, transaction::Status status)->void {
    auto* state = trx.state();
    TRI_ASSERT(state != nullptr);
    
    // check state of the top-most transaction only
    if (!state || !state->isTopLevelTransaction()) {
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

      if (transaction::Status::COMMITTED != status) { // rollback
        ctx.reset();
      } else {
        ctx._ctx.tick(state->lastOperationTick());
      }
    }

    prev.reset();
  };

  // initialize commit callback
  _before_commit = [this](uint64_t tick, irs::bstring& out) {
    _lastCommittedTick = std::max(_lastCommittedTick, TRI_voc_tick_t(tick)); // update last tick
    tick = irs::numeric_utils::hton64(uint64_t(_lastCommittedTick)); // convert to BE

    out.append(reinterpret_cast<irs::byte_type const*>(&tick), sizeof(uint64_t));

    return true;
  };
}

IResearchLink::~IResearchLink() {
  auto res = unload();  // disassociate from view if it has not been done yet

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

void IResearchLink::afterTruncate() {
  SCOPED_LOCK(_asyncSelf->mutex());  // '_dataStore' can be asynchronously modified

  if (!*_asyncSelf) {
    // the current link is no longer valid (checked after ReadLock acquisition)
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
      "failed to lock arangosearch link while truncating arangosearch link '" +
      std::to_string(id()) + "'");
  }

  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  try {
    _dataStore._writer->clear();
    _dataStore._reader = _dataStore._reader.reopen();
  } catch (std::exception const& e) {
    LOG_TOPIC("a3c57", ERR, iresearch::TOPIC)
        << "caught exception while truncating arangosearch link '" << id()
        << "': " << e.what();
    IR_LOG_EXCEPTION();
    throw;
  } catch (...) {
    LOG_TOPIC("79a7d", WARN, iresearch::TOPIC)
        << "caught exception while truncating arangosearch link '" << id() << "'";
    IR_LOG_EXCEPTION();
    throw;
  }
}

void IResearchLink::batchInsert(
    transaction::Methods& trx,
    std::vector<std::pair<LocalDocumentId, velocypack::Slice>> const& batch,
    std::shared_ptr<basics::LocalTaskQueue> queue) {
  if (batch.empty()) {
    return; // nothing to do
  }

  TRI_ASSERT(_engine);
  TRI_ASSERT(queue);
  TRI_ASSERT(trx.state());

  auto& state = *(trx.state());

  TRI_IF_FAILURE("ArangoSearch::BlockInsertsWithoutIndexCreationHint") {
    if (!state.hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
      queue->setStatus(TRI_ERROR_DEBUG);
      return;
    }
  }

  if (_dataStore._inRecovery && _engine->recoveryTick() <= _dataStore._recoveryTick) {
    LOG_TOPIC("7e228", TRACE, iresearch::TOPIC)
      << "skipping 'batchInsert', operation tick '" << _engine->recoveryTick()
      << "', recovery tick '" << _dataStore._recoveryTick << "'";

    return;
  }

  auto batchInsertImpl = [this, &batch, &trx, &queue](irs::index_writer::documents_context& ctx) {
    auto begin = batch.begin();
    auto const end = batch.end();
    try {
      for (FieldIterator body(trx); begin != end; ++begin) {
        auto const res = insertDocument(ctx, body, begin->second, begin->first, _meta, id());

        if (!res.ok()) {
          LOG_TOPIC("e5eb1", WARN, iresearch::TOPIC) << res.errorMessage();
          queue->setStatus(res.errorNumber());

          return;
        }
      }
    }
    catch (basics::Exception const& e) {
      LOG_TOPIC("72aa5", WARN, iresearch::TOPIC)
        << "caught exception while inserting batch into arangosearch link '" << id()
        << "': " << e.code() << " " << e.what();
      IR_LOG_EXCEPTION();
      queue->setStatus(e.code());
    }
    catch (std::exception const& e) {
      LOG_TOPIC("3cbae", WARN, iresearch::TOPIC)
        << "caught exception while inserting batch into arangosearch link '" << id()
        << "': " << e.what();
      IR_LOG_EXCEPTION();
      queue->setStatus(TRI_ERROR_INTERNAL);
    }
    catch (...) {
      LOG_TOPIC("3da8d", WARN, iresearch::TOPIC)
        << "caught exception while inserting batch into arangosearch link '" << id()
        << "'";
      IR_LOG_EXCEPTION();
      queue->setStatus(TRI_ERROR_INTERNAL);
    }
  };

  if (state.hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
    SCOPED_LOCK_NAMED(_asyncSelf->mutex(), lock);
    auto ctx = _dataStore._writer->documents();
    batchInsertImpl(ctx); // we need insert to succeed, so  we have things to cleanup in storage
    TRI_IF_FAILURE("ArangoSearch::MisreportCreationInsertAsFailed") {
      if (queue->status() == TRI_ERROR_NO_ERROR) {
        queue->setStatus(TRI_ERROR_DEBUG);
      }
    }
    return;
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
    SCOPED_LOCK_NAMED(_asyncSelf->mutex(), lock);

    if (!*_asyncSelf) {
      LOG_TOPIC("7d258", WARN, iresearch::TOPIC)
          << "failed to lock arangosearch link while inserting a batch into "
             "arangosearch link '"
          << id() << "', tid '" << state.id() << "'";

      // the current link is no longer valid (checked after ReadLock acquisition)
      queue->setStatus(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);

      return;
    }

    TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

    auto ptr = irs::memory::make_unique<LinkTrxState>(std::move(lock),
                                                      *(_dataStore._writer));

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx || !trx.addStatusChangeCallback(&_trxCallback)) {
      LOG_TOPIC("61780", WARN, iresearch::TOPIC)
          << "failed to store state into a TransactionState for batch insert "
             "into arangosearch link '"
          << id() << "', tid '" << state.id() << "'";
      queue->setStatus(TRI_ERROR_INTERNAL);

      return;
    }
  }

  // ...........................................................................
  // below only during recovery after the 'checkpoint' marker, or post recovery
  // ...........................................................................
  batchInsertImpl(ctx->_ctx);
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchLink::cleanupUnsafe() {
  char runId = 0; // value not used

  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

  try {
    irs::directory_utils::remove_all_unreferenced(*(_dataStore._directory));
  } catch (std::exception const& e) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while cleaning up arangosearch link '" + std::to_string(id()) +
      "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what()
    };
  } catch (...) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while cleaning up arangosearch link '" + std::to_string(id()) +
      "' run id '" + std::to_string(size_t(&runId)) + "'"
    };
  }

  LOG_TOPIC("7e821", TRACE, iresearch::TOPIC)
    << "successful cleanup of arangosearch link '" << id()
    << "', run id '" << size_t(&runId) << "'";

  return {};
}

Result IResearchLink::commit(bool wait /*= true*/) {
  SCOPED_LOCK(_asyncSelf->mutex()); // '_dataStore' can be asynchronously modified

  if (!*_asyncSelf) {
    // the current link is no longer valid (checked after ReadLock acquisition)

    return {
      TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
      "failed to lock arangosearch link while commiting arangosearch link '"
       + std::to_string(id()) + "'"
    };
  }

  return commitUnsafe(wait);
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchLink::commitUnsafe(bool wait) {
  char runId = 0; // value not used

  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

  auto* engine = EngineSelectorFeature::ENGINE;

  if (!engine) {
    return {
      TRI_ERROR_INTERNAL,
      "failure to get storage engine while committing arangosearch link '" + std::to_string(id()) + "'"
    };
  }

  auto subscription = std::static_pointer_cast<IResearchFlushSubscription>(_flushSubscription);

  if (!subscription) {
    // already released
    return {};
  }

  try {
    auto const lastTickBeforeCommit = engine->currentTick();

    TRY_SCOPED_LOCK_NAMED(_commitMutex, commitLock);

    if (!commitLock.owns_lock()) {
      // commit is already in progress
      if (!wait) {
        return {};
      }

      commitLock.lock();
    }

    auto const lastCommittedTick = _lastCommittedTick;

    try {
      // _lastCommittedTick is being updated in '_before_commit'
      _dataStore._writer->commit(_before_commit);
    } catch (...) {
      // restore last committed tick in case of any error
      _lastCommittedTick = lastCommittedTick;
      throw;
    }

    // get new reader
    auto reader = _dataStore._reader.reopen();

    if (!reader) {
      // nothing more to do
      LOG_TOPIC("37bcf", WARN, iresearch::TOPIC)
        << "failed to update snapshot after commit, run id '" << size_t(&runId)
        << "', reuse the existing snapshot for arangosearch link '" << id() << "'";

      return {};
    }

    if (_dataStore._reader == reader) {
      LOG_TOPIC("7e319", TRACE, iresearch::TOPIC)
        << "no changes registered for arangosearch link '" << id()
        << "' got last operation tick '" << _lastCommittedTick
        << "', run id '" << size_t(&runId) << "'";

      // no changes, can release the latest tick before commit
      subscription->tick(lastTickBeforeCommit);

      return {};
    }

    // update reader
    _dataStore._reader = reader;

    // update last committed tick
    subscription->tick(_lastCommittedTick);

    // invalidate query cache
    aql::QueryCache::instance()->invalidate(&(_collection.vocbase()), _viewGuid);

    LOG_TOPIC("7e328", TRACE, iresearch::TOPIC)
      << "successful sync of arangosearch link '" << id()
      << "', docs count '" << reader->docs_count()
      << "', live docs count '" << reader->live_docs_count()
      << "', last operation tick '" << _lastCommittedTick
      << "', run id '" << size_t(&runId) << "'";
  } catch (basics::Exception const& e) {
    return {
      e.code(),
      "caught exception while committing arangosearch link '" + std::to_string(id()) +
      "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what() };
  } catch (std::exception const& e) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while committing arangosearch link '" + std::to_string(id()) +
      "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what() };
  } catch (...) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while committing arangosearch link '" + std::to_string(id()) +
      "' run id '" + std::to_string(size_t(&runId)) + "'" };
  }

  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
Result IResearchLink::consolidateUnsafe(
    IResearchViewMeta::ConsolidationPolicy const& policy,
    irs::merge_writer::flush_progress_t const& progress) {
  char runId = 0; // value not used

  if (!policy.policy()) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "unset consolidation policy while executing consolidation policy '" + policy.properties().toString() +
      "' on arangosearch link '" + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "'"
    };
  }

  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

  try {
    if (!_dataStore._writer->consolidate(policy.policy(), nullptr, progress)) {
      return {
        TRI_ERROR_INTERNAL,
        "failure while executing consolidation policy '" + policy.properties().toString() +
        "' on arangosearch link '" + std::to_string(id()) +
        "' run id '" + std::to_string(size_t(&runId)) + "'"
      };
    }
  } catch (std::exception const& e) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while executing consolidation policy '" + policy.properties().toString() +
      "' on arangosearch link '" + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what()
    };
  } catch (...) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while executing consolidation policy '" + policy.properties().toString() +
      "' on arangosearch link '" + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "'"
    };
  }

  LOG_TOPIC("7e828", TRACE, iresearch::TOPIC)
    << "successful consolidation of arangosearch link '" << id()
    << "', run id '" << size_t(&runId) << "'";

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
    // the reverse happends during drop of a collection with MMFiles
    // i.e. collection drop() -> collection close()-> link unload(), then link drop()
    if (!view) {
      LOG_TOPIC("f4e2c", WARN, iresearch::TOPIC)
        << "unable to find arangosearch view '" << _viewGuid
        << "' while dropping arangosearch link '" << _id << "'";
    } else {
      view->unlink(_collection.id()); // unlink before reset() to release lock in view (if any)
    }
  }

  _asyncTerminate.store(true); // mark long-running async jobs for terminatation

  if (_asyncFeature) {
    _asyncFeature->asyncNotify(); // trigger reload of settings for async jobs
  }

  _flushSubscription.reset(); // reset together with '_asyncSelf'
  _asyncSelf->reset(); // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  try {
    if (_dataStore) {
      _dataStore.resetDataStore();
    }

    bool exists;

    // remove persisted data store directory if present
    if (!_dataStore._path.exists_directory(exists)
        || (exists && !_dataStore._path.remove())) {
      return {
        TRI_ERROR_INTERNAL,
        "failed to remove arangosearch link '" + std::to_string(id()) + "'"
      };
    }
  } catch (basics::Exception& e) {
    return {
      e.code(),
      "caught exception while removing arangosearch link '" + std::to_string(id()) + "': " + e.what()
    };
  } catch (std::exception const& e) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while removing arangosearch link '" + std::to_string(id()) + "': " + e.what()
    };
  } catch (...) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while removing arangosearch link '" + std::to_string(id()) + "'"
    };
  }

  return {};
}

bool IResearchLink::hasSelectivityEstimate() const {
  return false; // selectivity can only be determined per query since multiple fields are indexed
}

Result IResearchLink::init(
    velocypack::Slice const& definition,
    InitCallback const& initCallback /* = { }*/ ) {
  // disassociate from view if it has not been done yet
  if (!unload().ok()) {
    return { TRI_ERROR_INTERNAL, "failed to unload link" };
  }

  std::string error;
  IResearchLinkMeta meta;

  // definition should already be normalized and analyzers created if required
  if (!meta.init(definition, true, error, &(collection().vocbase()))) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "error parsing view link parameters from json: " + error
    };
  }

  if (!definition.isObject() // not object
      || !definition.get(StaticStrings::ViewIdField).isString()) {
    return {
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
      "error finding view for link '" + std::to_string(_id) + "'"
    };
  }

  auto viewId = definition.get(StaticStrings::ViewIdField).copyString();
  auto& vocbase = _collection.vocbase();
  bool const sorted = !meta._sort.empty();

  if (ServerState::instance()->isCoordinator()) { // coordinator link
    if (!vocbase.server().hasFeature<arangodb::ClusterFeature>()) {
      return {
        TRI_ERROR_INTERNAL,
        "failure to get cluster info while initializing arangosearch link '" + std::to_string(_id) + "'"
      };
    }
    auto& ci = vocbase.server().getFeature<arangodb::ClusterFeature>().clusterInfo();

    auto logicalView = ci.getView(vocbase.name(), viewId);

    // if there is no logicalView present yet then skip this step
    if (logicalView) {
      if (iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
        return {
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, // code
          "error finding view: '" + viewId + "' for link '" + std::to_string(_id) + "' : no such view"
        };
      }

      auto* view = LogicalView::cast<IResearchViewCoordinator>(logicalView.get());

      if (!view) {
        return {
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          "error finding view: '" + viewId + "' for link '" + std::to_string(_id) + "'"
        };
      }

      viewId = view->guid(); // ensue that this is a GUID (required by operator==(IResearchView))
      std::swap(const_cast<IResearchLinkMeta&>(_meta), meta); // required for IResearchViewCoordinator which calls IResearchLink::properties(...)

      auto revert = irs::make_finally([this, &meta]()->void { // revert '_meta'
        std::swap(const_cast<IResearchLinkMeta&>(_meta), meta);
      });
      auto res = view->link(*this);

      if (!res.ok()) {
        return res;
      }
    }
  } else if (ServerState::instance()->isDBServer()) { // db-server link
    if (!vocbase.server().hasFeature<arangodb::ClusterFeature>()) {
      return {
        TRI_ERROR_INTERNAL,
        "failure to get cluster info while initializing arangosearch link '" + std::to_string(_id) + "'"
      };
    }
    auto& ci = vocbase.server().getFeature<arangodb::ClusterFeature>().clusterInfo();

    // cluster-wide link
    auto clusterWideLink = _collection.id() == _collection.planId() && _collection.isAStub();

    if (!clusterWideLink) {
      // prepare data-store which can then update options
      // via the IResearchView::link(...) call
      auto const res = initDataStore(initCallback, sorted);

      if (!res.ok()) {
        return res;
      }
    }

    // valid to call ClusterInfo (initialized in ClusterFeature::prepare()) even from Databasefeature::start()
    auto logicalView = ci.getView(vocbase.name(), viewId);

    // if there is no logicalView present yet then skip this step
    if (logicalView) {
      if (iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
        unload(); // unlock the data store directory
        return {
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          "error finding view: '" + viewId + "' for link '" + std::to_string(_id) + "' : no such view"
        };
      }

      auto* view = LogicalView::cast<IResearchView>(logicalView.get());

      if (!view) {
        unload(); // unlock the data store directory
        return {
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          "error finding view: '" + viewId + "' for link '" + std::to_string(_id) + "'"
        };
      }

      viewId = view->guid(); // ensue that this is a GUID (required by operator==(IResearchView))

      if (clusterWideLink) { // cluster cluster-wide link
        auto shardIds = _collection.shardIds();

        // go through all shard IDs of the collection and try to link any links
        // missing links will be populated when they are created in the
        // per-shard collection
        if (shardIds) {
          for (auto& entry: *shardIds) {
            auto collection = vocbase.lookupCollection(entry.first); // per-shard collections are always in 'vocbase'

            if (!collection) {
              continue; // missing collection should be created after Plan becomes Current
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
      } else { // cluster per-shard link
        auto res = view->link(_asyncSelf);

        if (!res.ok()) {
          unload(); // unlock the data store directory

          return res;
        }
      }
    }
  } else if (ServerState::instance()->isSingleServer()) {  // single-server link
    // prepare data-store which can then update options
    // via the IResearchView::link(...) call
    auto const res = initDataStore(initCallback, sorted);

    if (!res.ok()) {
      return res;
    }

    auto logicalView = vocbase.lookupView(viewId);

    // if there is no logicalView present yet then skip this step
    if (logicalView) {
      if (iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
        unload(); // unlock the data store directory

        return {
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          "error finding view: '" + viewId + "' for link '" + std::to_string(_id) + "' : no such view"
        };
      }

      auto* view = LogicalView::cast<IResearchView>(logicalView.get());

      if (!view) {
        unload(); // unlock the data store directory

        return {
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          "error finding view: '" + viewId + "' for link '" + std::to_string(_id) + "'"
        };
      }

      viewId = view->guid(); // ensue that this is a GUID (required by operator==(IResearchView))

      auto res = view->link(_asyncSelf);

      if (!res.ok()) {
        unload(); // unlock the directory

        return res;
      }
    }
  }

  const_cast<std::string&>(_viewGuid) = std::move(viewId);
  const_cast<IResearchLinkMeta&>(_meta) = std::move(meta);
  _comparer.reset(_meta._sort);

  return {};
}

Result IResearchLink::initDataStore(InitCallback const& initCallback, bool sorted) {
  _asyncTerminate.store(true); // mark long-running async jobs for terminatation

  if (_asyncFeature) {
    _asyncFeature->asyncNotify(); // trigger reload of settings for async jobs
  }

  _flushSubscription.reset() ; // reset together with '_asyncSelf'
  _asyncSelf->reset(); // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  auto& server = application_features::ApplicationServer::server();
  if (!server.hasFeature<DatabasePathFeature>()) {
    return {
      TRI_ERROR_INTERNAL,
      "failure to find feature 'DatabasePath' while initializing link '" + std::to_string(_id) + "'"
    };
  }
  if (!server.hasFeature<FlushFeature>()) {
    return {
      TRI_ERROR_INTERNAL,
      "failure to find feature 'FlushFeature' while initializing link '" + std::to_string(_id) + "'"
    };
  }

  auto& dbPathFeature = server.getFeature<DatabasePathFeature>();
  auto& flushFeature = server.getFeature<FlushFeature>();

  auto format = irs::formats::get(IRESEARCH_STORE_FORMAT);

  if (!format) {
    return {
      TRI_ERROR_INTERNAL,
      "failed to get data store codec '"s + IRESEARCH_STORE_FORMAT.c_str() +
      "' while initializing link '" + std::to_string(_id) + "'"
    };
  }

  _engine = EngineSelectorFeature::ENGINE;

  if (!_engine) {
    return {
      TRI_ERROR_INTERNAL,
      "failure to get storage engine while initializing arangosearch link '" + std::to_string(id()) + "'"
    };
  }

  bool pathExists;

  _dataStore._path = getPersistedPath(dbPathFeature, *this);

  // must manually ensure that the data store directory exists (since not using
  // a lockfile)
  if (_dataStore._path.exists_directory(pathExists)
      && !pathExists
      && !_dataStore._path.mkdir()) {
    return {
      TRI_ERROR_CANNOT_CREATE_DIRECTORY,
      "failed to create data store directory with path '" + _dataStore._path.utf8() +
      "' while initializing link '" + std::to_string(_id) + "'"
    };
  }

  _dataStore._directory =
      irs::directory::make<irs::mmap_directory>(_dataStore._path.utf8());

  if (!_dataStore._directory) {
    return {
      TRI_ERROR_INTERNAL,
      "failed to instantiate data store directory with path '" + _dataStore._path.utf8() +
      "' while initializing link '" + std::to_string(_id) + "'"
    };
  }

  if (initCallback) {
    initCallback(*_dataStore._directory);
  }

  switch (_engine->recoveryState()) {
    case RecoveryState::BEFORE: // link is being opened before recovery
    case RecoveryState::DONE: { // link is being created after recovery
      _dataStore._inRecovery = true; // will be adjusted in post-recovery callback
      _dataStore._recoveryTick = _engine->recoveryTick();
      break;
    }

    case RecoveryState::IN_PROGRESS: { // link is being created during recovery
      // both MMFiles and RocksDB will fill out link based on
      // actual data in linked collections, we can treat recovery as done
      _createdInRecovery = true;

      _dataStore._inRecovery = false;
      _dataStore._recoveryTick = _engine->releasedTick();
      break;
    }
  }

  if (pathExists) {
    try {
      _dataStore._reader = irs::directory_reader::open(*(_dataStore._directory));

      if (!::readTick(_dataStore._reader.meta().meta.payload(), _dataStore._recoveryTick)) {
        return {
          TRI_ERROR_INTERNAL,
          "failed to get last committed tick while initializing link '" + std::to_string(id()) + "'"
        };
      }

      LOG_TOPIC("7e028", TRACE, iresearch::TOPIC)
        << "successfully opened existing data store data store reader for link '" + std::to_string(id())
        << "', docs count '" << _dataStore._reader->docs_count()
        << "', live docs count '" << _dataStore._reader->live_docs_count()
        << "', recovery tick '" << _dataStore._recoveryTick << "'";

    } catch (irs::index_not_found const&) {
      // NOOP
    }
  }

  _lastCommittedTick = _dataStore._recoveryTick;
  _flushSubscription.reset(new IResearchFlushSubscription(_dataStore._recoveryTick));


  irs::index_writer::init_options options;
  options.lock_repository = false; // do not lock index, ArangoDB has its own lock
  options.comparator = sorted ? &_comparer : nullptr; // set comparator if requested

  // setup columnstore compression/encryption if requested by storage engine
  auto const encrypt = (nullptr != irs::get_encryption(_dataStore._directory->attributes()));
  if (encrypt) {
    options.column_info = [](const irs::string_ref& name) -> irs::column_info {
      // do not waste resources to encrypt primary key column
      return { irs::compression::lz4::type(), {}, DocumentPrimaryKey::PK() != name };
    };
  } else {
    options.column_info = [](const irs::string_ref& /*name*/) -> irs::column_info {
      return { irs::compression::lz4::type(), {}, false };
    };
  }

  auto openFlags = irs::OM_APPEND;
  if (!_dataStore._reader) {
    openFlags |= irs::OM_CREATE;
  }

  _dataStore._writer = irs::index_writer::make(*(_dataStore._directory), format, openFlags, options);

  if (!_dataStore._writer) {
    return {
      TRI_ERROR_INTERNAL,
      "failed to instantiate data store writer with path '" + _dataStore._path.utf8() +
      "' while initializing link '" + std::to_string(_id) + "'"
    };
  }

  if (!_dataStore._reader) {
    _dataStore._writer->commit(_before_commit); // initialize 'store'
    _dataStore._reader = irs::directory_reader::open(*(_dataStore._directory));
  }

  if (!_dataStore._reader) {
    _dataStore._writer.reset();

    return {
      TRI_ERROR_INTERNAL,
      "failed to instantiate data store reader with path '" + _dataStore._path.utf8() +
      "' while initializing link '" + std::to_string(_id) + "'"
    };
  }

  if (!::readTick(_dataStore._reader.meta().meta.payload(), _dataStore._recoveryTick)) {
    return {
      TRI_ERROR_INTERNAL,
      "failed to get last committed tick while initializing link '" + std::to_string(id()) + "'"
    };
  }

  LOG_TOPIC("7e128", TRACE, iresearch::TOPIC)
    << "data store reader for link '" + std::to_string(id())
    << "' is initialized with recovery tick '" << _dataStore._recoveryTick << "'";

  // reset data store meta, will be updated at runtime via properties(...)
  _dataStore._meta._cleanupIntervalStep = 0; // 0 == disable
  _dataStore._meta._commitIntervalMsec = 0; // 0 == disable
  _dataStore._meta._consolidationIntervalMsec = 0; // 0 == disable
  _dataStore._meta._consolidationPolicy = IResearchViewMeta::ConsolidationPolicy(); // disable
  _dataStore._meta._writebufferActive = options.segment_count_max;
  _dataStore._meta._writebufferIdle = options.segment_pool_size;
  _dataStore._meta._writebufferSizeMax = options.segment_memory_max;

  // create a new 'self' (previous was reset during unload() above)
  _asyncSelf = irs::memory::make_unique<AsyncLinkPtr::element_type>(this);
  _asyncTerminate.store(false); // allow new asynchronous job invocation

  // ...........................................................................
  // set up in-recovery insertion hooks
  // ...........................................................................

  if (!server.hasFeature<DatabaseFeature>()) {
    return {}; // nothing more to do
  }
  auto& dbFeature = server.getFeature<DatabaseFeature>();

  auto asyncSelf = _asyncSelf; // create copy for lambda

  return dbFeature.registerPostRecoveryCallback(  // register callback
      [asyncSelf, &flushFeature]() -> Result {
        SCOPED_LOCK(asyncSelf->mutex());  // ensure link does not get deallocated before callback finishes
        auto* link = asyncSelf->get();

        if (!link) {
          return {};  // link no longer in recovery state, i.e. during recovery it was created and later dropped
        }

        if (!link->_flushSubscription) {
          return {
              TRI_ERROR_INTERNAL,
              "failed to register flush subscription for arangosearch link '" +
                  std::to_string(link->id()) + "'"};
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

        auto res = link->commitUnsafe(true);

        LOG_TOPIC("0e0ca", TRACE, iresearch::TOPIC)
            << "finished sync for arangosearch link '" << link->id() << "'";

        // register flush subscription
        flushFeature.registerFlushSubscription(link->_flushSubscription);

        // setup asynchronous tasks for commit, consolidation, cleanup
        link->setupMaintenance();

        return res;
      });
}

void IResearchLink::setupMaintenance() {
  auto& server = application_features::ApplicationServer::server();
  if (!server.hasFeature<iresearch::IResearchFeature>()) {
    return;
  }
  _asyncFeature = &(server.getFeature<iresearch::IResearchFeature>());

  struct CommitState: public IResearchViewMeta {
    size_t _cleanupIntervalCount{ 0 };
    std::chrono::system_clock::time_point _last{ std::chrono::system_clock::now() };
  } commitState;

  _asyncFeature->async( // register asynchronous commit task
    _asyncSelf, // mutex
    [this, commitState](size_t& timeoutMsec, bool) mutable ->bool {
      auto& state = commitState;

      if (_asyncTerminate.load()) {
        return false; // termination requested
      }

      // reload RuntimeState
      {
        TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid
        ReadMutex mutex(_dataStore._mutex); // '_meta' can be asynchronously modified
        SCOPED_LOCK(mutex);
        auto& stateMeta = static_cast<IResearchViewMeta&>(state);

        if (stateMeta != _dataStore._meta) {
          stateMeta = _dataStore._meta;
        }
      }

      if (!state._commitIntervalMsec) {
        timeoutMsec = 0; // task not enabled

        return true; // reschedule
      }

      size_t usedMsec = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - state._last // consumed msec from interval
      ).count();

      if (usedMsec < state._commitIntervalMsec) {
        timeoutMsec = state._commitIntervalMsec - usedMsec; // still need to sleep

        return true; // reschedule (with possibly updated '_commitIntervalMsec')
      }

      state._last = std::chrono::system_clock::now(); // remember last task start time
      timeoutMsec = state._commitIntervalMsec;

      auto res = commitUnsafe(false); // run commit ('_asyncSelf' locked by async task)

      if (!res.ok()) {
        LOG_TOPIC("8377b", WARN, iresearch::TOPIC)
          << "error while committing arangosearch link '" << id() <<
          "': " << res.errorNumber() << " " <<  res.errorMessage();
      } else if (state._cleanupIntervalStep // if enabled
                 && state._cleanupIntervalCount++ > state._cleanupIntervalStep) {
        state._cleanupIntervalCount = 0; // reset counter
        res = cleanupUnsafe(); // run cleanup ('_asyncSelf' locked by async task)

        if (!res.ok()) {
          LOG_TOPIC("130de", WARN, iresearch::TOPIC)
            << "error while cleaning up arangosearch link '" << id()
            << "': " << res.errorNumber() << " " <<  res.errorMessage();
        }
      }

      return true; // reschedule
    }
  );

  struct ConsolidateState: public CommitState {
    irs::merge_writer::flush_progress_t _progress;
  } consolidateState;

  consolidateState._progress = // consolidation termination condition
    [this]()->bool { return !_asyncTerminate.load(); };
  _asyncFeature->async( // register asynchronous consolidation task
    _asyncSelf, // mutex
    [this, consolidateState](size_t& timeoutMsec, bool) mutable ->bool {
      auto& state = consolidateState;

      if (_asyncTerminate.load()) {
        return false; // termination requested
      }

      // reload RuntimeState
      {
        TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid
        ReadMutex mutex(_dataStore._mutex); // '_meta' can be asynchronously modified
        SCOPED_LOCK(mutex);
        auto& stateMeta = static_cast<IResearchViewMeta&>(state);

        if (stateMeta != _dataStore._meta) {
          stateMeta = _dataStore._meta;
        }
      }

      if (!state._consolidationIntervalMsec // disabled via interval
          || !state._consolidationPolicy.policy()) { // disabled via policy
        timeoutMsec = 0; // task not enabled

        return true; // reschedule
      }

      size_t usedMsec = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - state._last
      ).count();

      if (usedMsec < state._consolidationIntervalMsec) {
        timeoutMsec = state._consolidationIntervalMsec - usedMsec; // still need to sleep

        return true; // reschedule (with possibly updated '_consolidationIntervalMsec')
      }

      state._last = std::chrono::system_clock::now(); // remember last task start time
      timeoutMsec = state._consolidationIntervalMsec;

      // run consolidation ('_asyncSelf' locked by async task)
      auto res = consolidateUnsafe(state._consolidationPolicy, state._progress);

      if (!res.ok()) {
        LOG_TOPIC("bce4f", WARN, iresearch::TOPIC)
          << "error while consolidating arangosearch link '" << id()
          << "': " << res.errorNumber() << " " <<  res.errorMessage();
      } else if (state._cleanupIntervalStep // if enabled
                 && state._cleanupIntervalCount++ > state._cleanupIntervalStep) {
        state._cleanupIntervalCount = 0; // reset counter
        res = cleanupUnsafe(); // run cleanup ('_asyncSelf' locked by async task)

        if (!res.ok()) {
          LOG_TOPIC("31941", WARN, iresearch::TOPIC)
            << "error while cleaning up arangosearch link '" << id()
            << "': " << res.errorNumber() << " " <<  res.errorMessage();
        }
      }

      return true; // reschedule
    }
  );
}

Result IResearchLink::insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode /*mode*/) {
  TRI_ASSERT(_engine);
  TRI_ASSERT(trx.state());

  auto& state = *(trx.state());

  if (_dataStore._inRecovery && _engine->recoveryTick() <= _dataStore._recoveryTick) {
    LOG_TOPIC("7c228", TRACE, iresearch::TOPIC)
      << "skipping 'insert', operation tick '" << _engine->recoveryTick()
      << "', recovery tick '" << _dataStore._recoveryTick << "'";

    return {};
  }

  auto insertImpl = [this, &trx, &doc, &documentId](
                        irs::index_writer::documents_context& ctx) -> Result {
    try {
      FieldIterator body(trx);

      return insertDocument(ctx, body, doc, documentId, _meta, id());
    } catch (basics::Exception const& e) {
      return {
        e.code(),
        "caught exception while inserting document into arangosearch link '" +
        std::to_string(id()) + "', revision '" +
        std::to_string(documentId.id()) + "': " + e.what()
      };
    } catch (std::exception const& e) {
      return {
        TRI_ERROR_INTERNAL,
        "caught exception while inserting document into arangosearch link '" +
         std::to_string(id()) + "', revision '" +
         std::to_string(documentId.id()) + "': " + e.what()
      };
    } catch (...) {
      return {
        TRI_ERROR_INTERNAL,
        "caught exception while inserting document into arangosearch link '" +
        std::to_string(id()) + "', revision '" +
        std::to_string(documentId.id()) + "'"
      };
    }
  };

  TRI_IF_FAILURE("ArangoSearch::BlockInsertsWithoutIndexCreationHint") {
    if (!state.hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
      return Result(TRI_ERROR_DEBUG);
    }
  }

  if (state.hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
    SCOPED_LOCK_NAMED(_asyncSelf->mutex(), lock);
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
  auto* ctx = dynamic_cast<LinkTrxState*>(state.cookie(key));
#else
  auto* ctx = static_cast<LinkTrxState*>(state.cookie(key));
#endif

  if (!ctx) {
    // '_dataStore' can be asynchronously modified
    SCOPED_LOCK_NAMED(_asyncSelf->mutex(), lock);

    if (!*_asyncSelf) {
      // the current link is no longer valid (checked after ReadLock acquisition)
      return {
        TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
        "failed to lock arangosearch link while inserting a "
        "document into arangosearch link '" + std::to_string(id()) + "'"
      };
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

    auto ptr = std::make_unique<LinkTrxState>(std::move(lock), *(_dataStore._writer));

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx || !trx.addStatusChangeCallback(&_trxCallback)) {
      return {
        TRI_ERROR_INTERNAL,
        "failed to store state into a TransactionState for insert into arangosearch link '" + std::to_string(id()) +
        "', tid '" + std::to_string(state.id()) + "', revision '" + std::to_string(documentId.id()) + "'"
      };
    }
  }

  return insertImpl(ctx->_ctx);
}

bool IResearchLink::isHidden() const {
  return !ServerState::instance()->isDBServer(); // hide links unless we are on a DBServer
}

bool IResearchLink::isSorted() const {
  return false; // IResearch does not provide a fixed default sort order
}

void IResearchLink::load() {
  // Note: this function is only used by RocksDB
}

bool IResearchLink::matchesDefinition(VPackSlice const& slice) const {
  if (!slice.isObject() || !slice.hasKey(StaticStrings::ViewIdField)) {
    return false; // slice has no view identifier field
  }

  auto viewId = slice.get(StaticStrings::ViewIdField);

  // NOTE: below will not match if 'viewId' is 'id' or 'name',
  //       but ViewIdField should always contain GUID
  if (!viewId.isString() || !viewId.isEqualString(_viewGuid)) {
    return false; // IResearch View identifiers of current object and slice do not match
  }

  IResearchLinkMeta other;
  std::string errorField;

  return other.init(slice, true, errorField, &(collection().vocbase())) // for db-server analyzer validation should have already apssed on coordinator (missing analyzer == no match)
    && _meta == other;
}

Result IResearchLink::properties(
    velocypack::Builder& builder,
    bool forPersistence) const {
  if (!builder.isOpenObject() // not an open object
      || !_meta.json(builder, forPersistence, nullptr, &(collection().vocbase()))) {
    return Result(TRI_ERROR_BAD_PARAMETER);
  }

  builder.add(
    arangodb::StaticStrings::IndexId,
    velocypack::Value(std::to_string(_id)));
  builder.add(
    arangodb::StaticStrings::IndexType,
    velocypack::Value(IResearchLinkHelper::type()));
  builder.add(StaticStrings::ViewIdField, velocypack::Value(_viewGuid));

  return {};
}

Result IResearchLink::properties(IResearchViewMeta const& meta) {
  SCOPED_LOCK(_asyncSelf->mutex());  // '_dataStore' can be asynchronously modified

  if (!*_asyncSelf) {
    // the current link is no longer valid (checked after ReadLock acquisition)
    return {
      TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
      "failed to lock arangosearch link while modifying properties "
      "of arangosearch link '" + std::to_string(id()) + "'"
    };
  }

  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  {
    WriteMutex mutex(_dataStore._mutex); // '_meta' can be asynchronously read
    SCOPED_LOCK(mutex);
    _dataStore._meta = meta;
  }

  if (_asyncFeature) {
    _asyncFeature->asyncNotify(); // trigger reload of settings for async jobs
  }

  irs::index_writer::segment_options properties;
  properties.segment_count_max = meta._writebufferActive;
  properties.segment_memory_max = meta._writebufferSizeMax;

  try {
    _dataStore._writer->options(properties);
  } catch (std::exception const& e) {
    LOG_TOPIC("c50c8", ERR, iresearch::TOPIC)
        << "caught exception while modifying properties of arangosearch link '"
        << id() << "': " << e.what();
    IR_LOG_EXCEPTION();
    throw;
  } catch (...) {
    LOG_TOPIC("ad1eb", WARN, iresearch::TOPIC)
        << "caught exception while modifying properties of arangosearch link '"
        << id() << "'";
    IR_LOG_EXCEPTION();
    throw;
  }

  return {};
}

Result IResearchLink::remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& /*doc*/,
    Index::OperationMode /*mode*/) {
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
  auto* ctx = dynamic_cast<LinkTrxState*>(state.cookie(key));
#else
  auto* ctx = static_cast<LinkTrxState*>(state.cookie(key));
#endif

  if (!ctx) {
    // '_dataStore' can be asynchronously modified
    SCOPED_LOCK_NAMED(_asyncSelf->mutex(), lock);

    if (!*_asyncSelf) {
      // the current link is no longer valid (checked after ReadLock acquisition)

      return {
        TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
        "failed to lock arangosearch link while removing a document from arangosearch link '" + std::to_string(id()) +
        "', tid '" + std::to_string(state.id()) +
        "', revision '" + std::to_string(documentId.id()) + "'"
      };
    }

    TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

    auto ptr = irs::memory::make_unique<LinkTrxState>(
      std::move(lock), *(_dataStore._writer)
    );

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx || !trx.addStatusChangeCallback(&_trxCallback)) {
      return {
        TRI_ERROR_INTERNAL,
        "failed to store state into a TransactionState for remove from arangosearch link '" + std::to_string(id()) +
        "', tid '" + std::to_string(state.id()) +
        "', revision '" + std::to_string(documentId.id()) + "'"
      };
    }
  }

  // ...........................................................................
  // if an exception occurs below than the transaction is droped including all
  // all of its fid stores, no impact to iResearch View data integrity
  // ...........................................................................
  try {
    ctx->remove(documentId);

    return {TRI_ERROR_NO_ERROR};
  } catch (basics::Exception const& e) {
    return {
      e.code(),
      "caught exception while removing document from arangosearch link '" + std::to_string(id()) +
      "', revision '" + std::to_string(documentId.id()) + "': " + e.what()
    };
  } catch (std::exception const& e) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while removing document from arangosearch link '" + std::to_string(id()) +
      "', revision '" + std::to_string(documentId.id()) + "': " + e.what()
    };
  } catch (...) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while removing document from arangosearch link '" + std::to_string(id()) +
      "', revision '" + std::to_string(documentId.id()) + "'"
    };
  }

  return {};
}

IResearchLink::Snapshot IResearchLink::snapshot() const {
  // '_dataStore' can be asynchronously modified
  SCOPED_LOCK_NAMED(_asyncSelf->mutex(), lock);

  if (!*_asyncSelf) {
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

Index::IndexType IResearchLink::type() const {
  // TODO: don't use enum
  return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
}

char const* IResearchLink::typeName() const {
  return IResearchLinkHelper::type().c_str();
}

Result IResearchLink::unload() {
  // this code is used by the MMFilesEngine
  // if the collection is in the process of being removed then drop it from the view
  // FIXME TODO remove once LogicalCollection::drop(...) will drop its indexes explicitly
  if (_collection.deleted() // collection deleted
      || TRI_vocbase_col_status_e::TRI_VOC_COL_STATUS_DELETED == _collection.status()) {
    return drop();
  }

  _asyncTerminate.store(true); // mark long-running async jobs for terminatation

  if (_asyncFeature) {
    _asyncFeature->asyncNotify(); // trigger reload of settings for async jobs
  }

  _flushSubscription.reset() ; // reset together with '_asyncSelf'
  _asyncSelf->reset(); // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  try {
    if (_dataStore) {
      _dataStore.resetDataStore();
    }
  } catch (basics::Exception const& e) {
    return {
      e.code(),
      "caught exception while unloading arangosearch link '" + std::to_string(id()) + "': " + e.what()
    };
  } catch (std::exception const& e) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while removing arangosearch link '" + std::to_string(id()) + "': " + e.what()
    };
  } catch (...) {
    return {
      TRI_ERROR_INTERNAL,
      "caught exception while removing arangosearch link '" + std::to_string(id()) + "'"
    };
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

  auto const pool = *it;

  if (pool && analyzer == *pool) {
    return pool;
  }

  return nullptr;
}

IResearchLink::Stats IResearchLink::stats() const {
  Stats stats;

  // '_dataStore' can be asynchronously modified
  SCOPED_LOCK_NAMED(_asyncSelf->mutex(), lock);

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

void IResearchLink::toVelocyPackStats(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());

  auto const stats = this->stats();

  builder.add("numBufferedDocs", VPackValue(stats.numBufferedDocs));
  builder.add("numDocs", VPackValue(stats.docsCount));
  builder.add("numLiveDocs", VPackValue(stats.liveDocsCount));
  builder.add("numSegments", VPackValue(stats.numSegments));
  builder.add("numFiles", VPackValue(stats.numFiles));
  builder.add("indexSize", VPackValue(stats.indexSize));
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
