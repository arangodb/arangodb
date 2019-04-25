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

#include "store/mmap_directory.hpp"
#include "store/store_utils.hpp"
#include "index/index_writer.hpp"

#include "IResearchCommon.h"
#include "IResearchFeature.h"
#include "IResearchLinkHelper.h"
#include "IResearchPrimaryKeyFilter.h"
#include "IResearchView.h"
#include "IResearchViewCoordinator.h"
#include "Aql/QueryCache.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterInfo.h"
#include "MMFiles/MMFilesCollection.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include "IResearchLink.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief the suffix appened to the index_meta filename to generate the
///        corresponding checkpoint file
////////////////////////////////////////////////////////////////////////////////
const irs::string_ref IRESEARCH_CHECKPOINT_SUFFIX(".checkpoint");

////////////////////////////////////////////////////////////////////////////////
/// @brief the storage format used with IResearch writers
////////////////////////////////////////////////////////////////////////////////
const irs::string_ref IRESEARCH_STORE_FORMAT("1_1");

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
// @brief approximate data store directory instance size
////////////////////////////////////////////////////////////////////////////////
size_t directoryMemory(irs::directory const& directory, TRI_idx_iid_t id) noexcept {
  size_t size = 0;

  try {
    directory.visit([&directory, &size](std::string& file) -> bool {
      uint64_t length;

      size += directory.length(length, file) ? length : 0;

      return true;
    });
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC("e6cb2", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while calculating size of arangosearch link '"
        << id << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception const& e) {
    LOG_TOPIC("7e623", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while calculating size of arangosearch link '"
        << id << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC("21d6a", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while calculating size of arangosearch link '" << id << "'";
    IR_LOG_EXCEPTION();
  }

  return size;
}

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
    return arangodb::Result();  // no fields to index
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

  // System fields

  // Indexed and Stored: LocalDocumentId
  auto docPk = arangodb::iresearch::DocumentPrimaryKey::encode(documentId);

  // reuse the 'Field' instance stored inside the 'FieldIterator'
  arangodb::iresearch::Field::setPkValue(const_cast<arangodb::iresearch::Field&>(field), docPk);
  doc.insert<irs::Action::INDEX_AND_STORE>(field);

  if (!doc) {
    return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to insert document into arangosearch link '") +
            std::to_string(id) + "', revision '" +
            std::to_string(documentId.id()) + "'");
  }

  return arangodb::Result();
}

}  // namespace

namespace arangodb {
namespace iresearch {

IResearchLink::IResearchLink( // constructor
    TRI_idx_iid_t iid, // index id
    arangodb::LogicalCollection& collection // index collection
): _asyncFeature(nullptr),
   _asyncSelf(irs::memory::make_unique<AsyncLinkPtr::element_type>(nullptr)), // mark as data store not initialized
   _asyncTerminate(false),
   _collection(collection),
   _id(iid) {
  auto* key = this;

  // initialize transaction callback
  _trxCallback = [key]( // callback
      arangodb::transaction::Methods& trx, // transaction
      arangodb::transaction::Status status // transaction status
  )->void {
    auto* state = trx.state();

    // check state of the top-most transaction only
    if (!state) {
      return;  // NOOP
    }

    auto prev = state->cookie(key, nullptr);  // get existing cookie
    auto rollback = arangodb::transaction::Status::COMMITTED != status;

    if (rollback && prev) {
// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      auto& ctx = dynamic_cast<LinkTrxState&>(*prev);
#else
      auto& ctx = static_cast<LinkTrxState&>(*prev);
#endif

      ctx.reset();
    }

    prev.reset();
  };
}

IResearchLink::~IResearchLink() {
  auto res = unload();  // disassociate from view if it has not been done yet

  if (!res.ok()) {
    LOG_TOPIC("2b41f", ERR, arangodb::iresearch::TOPIC)
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
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,  // the current link is no longer
                                            // valid (checked after ReadLock
                                            // aquisition)
        std::string("failed to lock arangosearch link while truncating "
                    "arangosearch link '") +
            std::to_string(id()) + "'");
  }

  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  try {
    _dataStore._writer->clear();
    _dataStore._reader = _dataStore._reader.reopen();
  } catch (std::exception const& e) {
    LOG_TOPIC("a3c57", ERR, arangodb::iresearch::TOPIC)
        << "caught exception while truncating arangosearch link '" << id()
        << "': " << e.what();
    IR_LOG_EXCEPTION();
    throw;
  } catch (...) {
    LOG_TOPIC("79a7d", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while truncating arangosearch link '" << id() << "'";
    IR_LOG_EXCEPTION();
    throw;
  }
}

void IResearchLink::batchInsert( // insert documents
    arangodb::transaction::Methods& trx, // transaction
    std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& batch, // documents
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue // task queue
) {
  if (batch.empty()) {
    return; // nothing to do
  }

  if (!queue) {
    throw std::runtime_error(std::string("failed to report status during batch insert for arangosearch link '") + arangodb::basics::StringUtils::itoa(_id) + "'");
  }

  if (!trx.state()) {
    LOG_TOPIC("c6795", WARN, arangodb::iresearch::TOPIC)
      << "failed to get transaction state while inserting a document into arangosearch link '" << id() << "'";
    queue->setStatus(TRI_ERROR_BAD_PARAMETER); // transaction state required

    return;
  }

  auto& state = *(trx.state());
  auto* key = this;

// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* ctx = dynamic_cast<LinkTrxState*>(state.cookie(key));
#else
  auto* ctx = static_cast<LinkTrxState*>(state.cookie(key));
#endif

  if (!ctx) {
    SCOPED_LOCK_NAMED(_asyncSelf->mutex(),
                      lock);  // '_dataStore' can be asynchronously modified

    if (!*_asyncSelf) {
      LOG_TOPIC("7d258", WARN, arangodb::iresearch::TOPIC)
          << "failed to lock arangosearch link while inserting a batch into "
             "arangosearch link '"
          << id() << "', tid '" << state.id() << "'";
      queue->setStatus(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);  // the current link is no longer
                                                            // valid (checked after ReadLock
                                                            // aquisition)

      return;
    }

    TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

    auto ptr = irs::memory::make_unique<LinkTrxState>(std::move(lock),
                                                      *(_dataStore._writer));

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx || !trx.addStatusChangeCallback(&_trxCallback)) {
      LOG_TOPIC("61780", WARN, arangodb::iresearch::TOPIC)
          << "failed to store state into a TransactionState for batch insert "
             "into arangosearch link '"
          << id() << "', tid '" << state.id() << "'";
      queue->setStatus(TRI_ERROR_INTERNAL);

      return;
    }
  }

  switch (_dataStore._recovery) {
   case RecoveryState::BEFORE_CHECKPOINT:
    return; // ignore all insertions before 'checkpoint'
   case RecoveryState::AFTER_CHECKPOINT:
    // FIXME TODO find a better way to force MMFiles WAL recovery
    // workaround MMFilesWalRecoverState not replaying document insertion
    // markers, but instead reinserting all documents into the index just before
    // the end of recovery
    if (!dynamic_cast<arangodb::MMFilesCollection*>(collection().getPhysical())) {
      break; // skip for non-MMFiles (fallthough for MMFiles)
    }
    // intentionally falls through
   case RecoveryState::DURING_CHECKPOINT:
    for (auto const& doc: batch) {
      ctx->remove(doc.first);
    }
   default: // fall through
    {} // NOOP
  }

  // ...........................................................................
  // below only during recovery after the 'checkpoint' marker, or post recovery
  // ...........................................................................

  auto begin = batch.begin();
  auto const end = batch.end();

  try {
    for (FieldIterator body(trx); begin != end; ++begin) {
      auto res =
          insertDocument(ctx->_ctx, body, begin->second, begin->first, _meta, id());

      if (!res.ok()) {
        LOG_TOPIC("e5eb1", WARN, arangodb::iresearch::TOPIC) << res.errorMessage();
        queue->setStatus(res.errorNumber());

        return;
      }
    }
  } catch (arangodb::basics::Exception const& e) {
    LOG_TOPIC("72aa5", WARN, arangodb::iresearch::TOPIC)
      << "caught exception while inserting batch into arangosearch link '" << id() << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
    queue->setStatus(e.code());
  } catch (std::exception const& e) {
    LOG_TOPIC("3cbae", WARN, arangodb::iresearch::TOPIC)
      << "caught exception while inserting batch into arangosearch link '" << id() << "': " << e.what();
    IR_LOG_EXCEPTION();
    queue->setStatus(TRI_ERROR_INTERNAL);
  } catch (...) {
    LOG_TOPIC("3da8d", WARN, arangodb::iresearch::TOPIC)
      << "caught exception while inserting batch into arangosearch link '" << id() << "'";
    IR_LOG_EXCEPTION();
    queue->setStatus(TRI_ERROR_INTERNAL);
  }
}

bool IResearchLink::canBeDropped() const {
  return true; // valid for a link to be dropped from an iResearch view
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
arangodb::Result IResearchLink::cleanupUnsafe() {
  char runId = 0; // value not used

  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

  try {
    irs::directory_utils::remove_all_unreferenced(*(_dataStore._directory));
  } catch (std::exception const& e) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while cleaning up arangosearch link '") + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while cleaning up arangosearch link '") + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "'"
    );
  }

  return arangodb::Result();
}

arangodb::LogicalCollection& IResearchLink::collection() const noexcept {
  return _collection;
}

arangodb::Result IResearchLink::commit() {
  SCOPED_LOCK(_asyncSelf->mutex()); // '_dataStore' can be asynchronously modified

  if (!*_asyncSelf) {
    return arangodb::Result(
      TRI_ERROR_ARANGO_INDEX_HANDLE_BAD, // the current link is no longer valid (checked after ReadLock aquisition)
      std::string("failed to lock arangosearch link while commiting arangosearch link '") + std::to_string(id()) + "'"
    );
  }

  return commitUnsafe();
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
arangodb::Result IResearchLink::commitUnsafe() {
  char runId = 0; // value not used

  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

  try {
    _dataStore._writer->commit();

    SCOPED_LOCK(_readerMutex);
    auto reader = _dataStore._reader.reopen(); // update reader

    if (!reader) {
      // nothing more to do
      LOG_TOPIC("37bcf", WARN, arangodb::iresearch::TOPIC)
        << "failed to update snapshot after commit, run id '" << size_t(&runId) << "', reuse the existing snapshot for arangosearch link '" << id() << "'";

      return arangodb::Result();
    }

    if (_dataStore._reader == reader) {
      return arangodb::Result(); // reader not modified
    }

    // if WAL 'Flush' recovery is enabled (must be for recoverable DB scenarios)
    if (_flushCallback && RecoveryState::DONE == _dataStore._recovery) {
      auto& checkpoint = reader.meta().filename;
      auto checkpointFile = // checkpoint file name
        checkpoint + std::string(IRESEARCH_CHECKPOINT_SUFFIX);
      auto ref = irs::directory_utils::reference( // create a reference
        *(_dataStore._directory), checkpointFile, true // args
      );
      arangodb::velocypack::Builder builder;

      builder.add(arangodb::velocypack::Value(checkpoint));

      auto res = _flushCallback(builder.slice()); // write 'Flush' marker

      if (!res.ok()) {
        return res; // the failed 'segments_' file cannot be removed at least on MSVC
      }

      TRI_ASSERT(_dataStore._recovery_reader); // ensured by initDataStore()
      auto previousCheckpoint = _dataStore._recovery_reader.meta().filename; // current checkpoint range start

      try {
        auto out = _dataStore._directory->create(checkpointFile); // create checkpoint file

        if (!out) { // create checkpoint
          return arangodb::Result( // result
            TRI_ERROR_CANNOT_WRITE_FILE, // code
            std::string("failed to write checkpoint file for arangosearch link '") + std::to_string(id()) + "', run id '" + std::to_string(size_t(&runId)) + "', ignoring commit success, path: " + checkpointFile
          );
        }

        irs::write_string(*out, previousCheckpoint); // will flush on deallocation
      } catch (std::exception const& e) {
        _dataStore._directory->remove(checkpointFile); // try to remove failed file

        return arangodb::Result( // result
          TRI_ERROR_ARANGO_IO_ERROR, // code
          std::string("caught exception while writing checkpoint file for arangosearch link '") + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what()
        );
      } catch (...) {
        _dataStore._directory->remove(checkpointFile); // try to remove failed file

        return arangodb::Result( // result
          TRI_ERROR_ARANGO_IO_ERROR, // code
          std::string("caught exception while writing checkpoint file for arangosearch link '") + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "'"
        );
      }

      _dataStore._recovery_range_start = std::move(previousCheckpoint); // remember current checkpoint range start
      _dataStore._recovery_reader = reader; // remember last successful reader
      _dataStore._recovery_ref = ref; // ensure checkpoint file will not get removed
    }

    _dataStore._reader = reader; // update reader
    arangodb::aql::QueryCache::instance()->invalidate(
      &(_collection.vocbase()), _viewGuid
    );
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result( // result
      e.code(), // code
      std::string("caught exception while committing arangosearch link '") + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while committing arangosearch link '") + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while committing arangosearch link '") + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "'"
    );
  }

  return arangodb::Result();
}

////////////////////////////////////////////////////////////////////////////////
/// @note assumes that '_asyncSelf' is read-locked (for use with async tasks)
////////////////////////////////////////////////////////////////////////////////
arangodb::Result IResearchLink::consolidateUnsafe( // consolidate segments
    IResearchViewMeta::ConsolidationPolicy const& policy, // policy to apply
    irs::merge_writer::flush_progress_t const& progress // policy progress to use
) {
  char runId = 0; // value not used

  if (!policy.policy()) {
    return arangodb::Result( // result
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("unset consolidation policy while executing consolidation policy '") + policy.properties().toString() + "' on arangosearch link '" + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "'"
    );
  }

  // NOTE: assumes that '_asyncSelf' is read-locked (for use with async tasks)
  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

  try {
    if (!_dataStore._writer->consolidate(policy.policy(), nullptr, progress)) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failure while executing consolidation policy '") + policy.properties().toString() + "' on arangosearch link '" + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "'"
      );
    }
  } catch (std::exception const& e) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while executing consolidation policy '") + policy.properties().toString() + "' on arangosearch link '" + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while executing consolidation policy '") + policy.properties().toString() + "' on arangosearch link '" + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "'"
    );
  }

  return arangodb::Result();
}

arangodb::Result IResearchLink::drop() {
  // the lookup and unlink is valid for single-server only (that is the only scenario where links are persisted)
  // on coordinator and db-server the IResearchView is immutable and lives in ClusterInfo
  // therefore on coordinator and db-server a new plan will already have an IResearchView without the link
  // this avoids deadlocks with ClusterInfo::loadPlan() during lookup in ClusterInfo
  if (ServerState::instance()->isSingleServer()) {
    auto logicalView = _collection.vocbase().lookupView(_viewGuid);
    auto* view = arangodb::LogicalView::cast<IResearchView>( // IResearchView pointer
      logicalView.get() // args
    );

    // may occur if the link was already unlinked from the view via another instance
    // this behaviour was seen user-access-right-drop-view-arangosearch-spec.js
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
      LOG_TOPIC("f4e2c", WARN, arangodb::iresearch::TOPIC)
        << "unable to find arangosearch view '" << _viewGuid << "' while dropping arangosearch link '" << _id << "'";
    } else {
      view->unlink(_collection.id()); // unlink before reset() to release lock in view (if any)
    }
  }

  _asyncTerminate.store(true); // mark long-running async jobs for terminatation

  if (_asyncFeature) {
    _asyncFeature->asyncNotify(); // trigger reload of settings for async jobs
  }

  _flushCallback = {}; // reset together with '_asyncSelf', it's an std::function so don't use a constructor or ASAN complains
  _asyncSelf->reset(); // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  try {
    if (_dataStore) {
      _dataStore._reader.reset(); // reset reader to release file handles
      _dataStore._writer.reset();
      _dataStore._directory.reset();
    }

    bool exists;

    // remove persisted data store directory if present
    if (!_dataStore._path.exists_directory(exists)
        || (exists && !_dataStore._path.remove())) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to remove arangosearch link '") + std::to_string(id()) + "'"
      );
    }
  } catch (arangodb::basics::Exception& e) {
    return arangodb::Result(
      e.code(),
      std::string("caught exception while removing arangosearch link '") + std::to_string(id()) + "': " + e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while removing arangosearch link '") + std::to_string(id()) + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while removing arangosearch link '") + std::to_string(id()) + "'"
    );
  }

  return arangodb::Result();
}

bool IResearchLink::hasSelectivityEstimate() const {
  return false; // selectivity can only be determined per query since multiple fields are indexed
}

arangodb::Result IResearchLink::init(
    arangodb::velocypack::Slice const& definition,
    InitCallback const& initCallback /* = { }*/ ) {
  // disassociate from view if it has not been done yet
  if (!unload().ok()) {
    return arangodb::Result(TRI_ERROR_INTERNAL, "failed to unload link");
  }

  std::string error;
  IResearchLinkMeta meta;

  if (!meta.init(definition, true, error, &(collection().vocbase()))) { // definition should already be normalized and analyzers created if required
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error parsing view link parameters from json: ") + error
    );
  }

  if (!definition.isObject() // not object
      || !definition.get(StaticStrings::ViewIdField).isString()) {
    return arangodb::Result( // result
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, // code
      std::string("error finding view for link '") + std::to_string(_id) + "'" // message
    );
  }

  auto viewId = definition.get(StaticStrings::ViewIdField).copyString();
  auto& vocbase = _collection.vocbase();

  if (arangodb::ServerState::instance()->isCoordinator()) { // coordinator link
    auto* ci = arangodb::ClusterInfo::instance();

    if (!ci) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failure to get storage engine while initializing arangosearch link '") + std::to_string(_id) + "'"
      );
    }

    auto logicalView = ci->getView(vocbase.name(), viewId);

    // if there is no logicalView present yet then skip this step
    if (logicalView) {
      if (arangodb::iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
        return arangodb::Result( // result
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, // code
          std::string("error finding view: '") + viewId + "' for link '" + std::to_string(_id) + "' : no such view"
        );
      }

      auto* view = arangodb::LogicalView::cast<IResearchViewCoordinator>( // cast view
        logicalView.get() // args
      );

      if (!view) {
        return arangodb::Result( // result
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, // code
          std::string("error finding view: '") + viewId + "' for link '" + std::to_string(_id) + "'"
        );
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
  } else if (arangodb::ServerState::instance()->isDBServer()) { // db-server link
    auto* ci = arangodb::ClusterInfo::instance();

    if (!ci) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failure to get storage engine while initializing arangosearch link '") + std::to_string(_id) + "'"
      );
    }

    auto clusterWideLink = // cluster-wide link
       _collection.id() == _collection.planId() && _collection.isAStub();

    if (!clusterWideLink) {
      auto res = initDataStore(initCallback);  // prepare data-store which can then update options
                                   // via the IResearchView::link(...) call

      if (!res.ok()) {
        return res;
      }
    }

    auto logicalView = ci->getView(vocbase.name(), viewId); // valid to call ClusterInfo (initialized in ClusterFeature::prepare()) even from Databasefeature::start()

    // if there is no logicalView present yet then skip this step
    if (logicalView) {
      if (arangodb::iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
        unload(); // unlock the data store directory
        return arangodb::Result( // result
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, // code
          std::string("error finding view: '") + viewId + "' for link '" + std::to_string(_id) + "' : no such view"
        );
      }

      auto* view = // view
        arangodb::LogicalView::cast<IResearchView>(logicalView.get());

      if (!view) {
        unload(); // unlock the data store directory
        return arangodb::Result( // result
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, // code
          std::string("error finding view: '") + viewId + "' for link '" + std::to_string(_id) + "'"
        );
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
  } else if (arangodb::ServerState::instance()->isSingleServer()) {  // single-server link
    auto res = initDataStore(initCallback);  // prepare data-store which can then update options
                                             // via the IResearchView::link(...) call

    if (!res.ok()) {
      return res;
    }

    auto logicalView = vocbase.lookupView(viewId);

    // if there is no logicalView present yet then skip this step
    if (logicalView) {
      if (arangodb::iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
        unload(); // unlock the data store directory

        return arangodb::Result( // result
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, // code
          std::string("error finding view: '") + viewId + "' for link '" + std::to_string(_id) + "' : no such view"
        );
      }

      auto* view = // view
        arangodb::LogicalView::cast<IResearchView>(logicalView.get());

      if (!view) {
        unload(); // unlock the data store directory

        return arangodb::Result( // result
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, // code
          std::string("error finding view: '") + viewId + "' for link '" + std::to_string(_id) + "'"
        );
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

  return arangodb::Result();
}

arangodb::Result IResearchLink::initDataStore(InitCallback const& initCallback) {
  _asyncTerminate.store(true); // mark long-running async jobs for terminatation

  if (_asyncFeature) {
    _asyncFeature->asyncNotify(); // trigger reload of settings for async jobs
  }

  _flushCallback = {}; // reset together with '_asyncSelf', it's an std::function so don't use a constructor or ASAN complains
  _asyncSelf->reset(); // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  auto* dbPathFeature = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::DatabasePathFeature // feature type
  >("DatabasePath");

  if (!dbPathFeature) {
    return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failure to find feature 'DatabasePath' while initializing "
                    "link '") +
            std::to_string(_id) + "'");
  }

  auto format = irs::formats::get(IRESEARCH_STORE_FORMAT);

  if (!format) {
    return arangodb::Result(TRI_ERROR_INTERNAL,
                            std::string("failed to get data store codec '") +
                                IRESEARCH_STORE_FORMAT.c_str() +
                                "' while initializing link '" +
                                std::to_string(_id) + "'");
  }

  bool pathExists;

  _dataStore._path = getPersistedPath(*dbPathFeature, *this);

  // must manually ensure that the data store directory exists (since not using
  // a lockfile)
  if (_dataStore._path.exists_directory(pathExists) && !pathExists &&
      !_dataStore._path.mkdir()) {
    return arangodb::Result(
        TRI_ERROR_CANNOT_CREATE_DIRECTORY,
        std::string("failed to create data store directory with path '") +
            _dataStore._path.utf8() + "' while initializing link '" +
            std::to_string(_id) + "'");
  }

  _dataStore._directory =
      irs::directory::make<irs::mmap_directory>(_dataStore._path.utf8());

  if (!_dataStore._directory) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to instantiate data store directory with path '") + _dataStore._path.utf8() + "' while initializing link '" + std::to_string(_id) + "'"
    );
  }

  if (initCallback) {
    initCallback(*_dataStore._directory);
  }

  _dataStore._recovery = RecoveryState::AFTER_CHECKPOINT; // new empty data store

  irs::directory_reader recovery_reader;

  if (pathExists) {
    try {
      recovery_reader = irs::directory_reader::open(*(_dataStore._directory));
    } catch (irs::index_not_found const&) {
      // ingore
    }
  }

  // if this is an existing datastore then ensure that it has a valid
  // '.checkpoint' file for the last state of the data store
  // if it's missing them probably the WAL tail was lost
  if (recovery_reader) {
    auto& checkpoint = recovery_reader.meta().filename;
    auto checkpointFile = checkpoint + std::string(IRESEARCH_CHECKPOINT_SUFFIX);
    auto ref = irs::directory_utils::reference( // create a reference
      *(_dataStore._directory), checkpointFile, false // args
    );

    if (!ref) {
      return arangodb::Result( // result
        TRI_ERROR_ARANGO_ILLEGAL_STATE, // code
        std::string("failed to find checkpoint file matching the latest data store state for arangosearch link '") + std::to_string(id()) + "', expecting file '" + checkpointFile + "' in path: " + _dataStore._path.utf8()
      );
    }

    auto in = _dataStore._directory->open( // open checkpoint file
      checkpointFile, irs::IOAdvice::NORMAL // args, use 'NORMAL' since the file could be empty
    );

    if (!in) {
      return arangodb::Result( // result
        TRI_ERROR_CANNOT_READ_FILE, // code
        std::string("failed to read checkpoint file for arangosearch link '") + std::to_string(id()) + "', path: " + checkpointFile
      );
    }

    std::string previousCheckpoint;

    // zero-length indicates very first '.checkpoint' file
    if (in->length()) {
      try {
        previousCheckpoint = irs::read_string<std::string>(*in);
      } catch (std::exception const& e) {
        return arangodb::Result( // result
          TRI_ERROR_ARANGO_IO_ERROR, // code
          std::string("caught exception while reading checkpoint file for arangosearch link '") + std::to_string(id()) + "': " + e.what()
        );
      } catch (...) {
        return arangodb::Result( // result
          TRI_ERROR_ARANGO_IO_ERROR, // code
          std::string("caught exception while reading checkpoint file for arangosearch link '") + std::to_string(id()) + "'"
        );
      }

      auto* engine = arangodb::EngineSelectorFeature::ENGINE;

      if (!engine) {
        _dataStore._writer.reset(); // unlock the directory
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("failure to get storage engine while initializing arangosearch link '") + std::to_string(id()) + "'"
        );
      }

      // if in recovery then recovery markers are expected
      // if not in recovery then AFTER_CHECKPOINT will be converted to DONE by
      // the post-recovery-callback (or left untouched if no DatabaseFeature
      if (engine->inRecovery()) {
        _dataStore._recovery = RecoveryState::DURING_CHECKPOINT; // exisitng data store (assume worst case, i.e. replaying just before checkpoint)
      }
    }

    _dataStore._recovery_range_start = std::move(previousCheckpoint); // remember current checkpoint range start
    _dataStore._recovery_reader = recovery_reader; // remember last successful reader
    _dataStore._recovery_ref = ref; // ensure checkpoint file will not get removed
  }

  irs::index_writer::init_options options;

  options.lock_repository = false; // do not lock index, ArangoDB has it's own lock

  // create writer before reader to ensure data directory is present
  _dataStore._writer = irs::index_writer::make( // open writer
    *(_dataStore._directory), format, irs::OM_CREATE | irs::OM_APPEND, options // args
  );

  if (!_dataStore._writer) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("failed to instantiate data store writer with path '") + _dataStore._path.utf8() + "' while initializing link '" + std::to_string(_id) + "'"
    );
  }

  _dataStore._writer->commit(); // initialize 'store'
  _dataStore._reader = irs::directory_reader::open(*(_dataStore._directory));

  if (!_dataStore._reader) {
    _dataStore._writer.reset(); // unlock the directory
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("failed to instantiate data store reader with path '") + _dataStore._path.utf8() + "' while initializing link '" + std::to_string(_id) + "'"
    );
  }

  // if this is a new data store then create a '.checkpoint' file for it
  if (!recovery_reader) {
    recovery_reader = _dataStore._reader; // use current empty reader

    auto& checkpoint = recovery_reader.meta().filename;
    auto checkpointFile = checkpoint + std::string(IRESEARCH_CHECKPOINT_SUFFIX);
    auto ref = irs::directory_utils::reference( // create a reference
      *(_dataStore._directory), checkpointFile, true // args
    );

    try {
      if (!_dataStore._directory->create(checkpointFile)) {
        return arangodb::Result( // result
          TRI_ERROR_CANNOT_WRITE_FILE, // code
          std::string("failed to write checkpoint file for arangosearch link '") + std::to_string(id()) + "', ignoring commit success, path: " + checkpointFile
        );
      }
    } catch (std::exception const& e) {
      return arangodb::Result( // result
        TRI_ERROR_ARANGO_IO_ERROR, // code
        std::string("caught exception while writing checkpoint file for arangosearch link '") + std::to_string(id()) + "': " + e.what()
      );
    } catch (...) {
      return arangodb::Result( // result
        TRI_ERROR_ARANGO_IO_ERROR, // code
        std::string("caught exception while writing checkpoint file for arangosearch link '") + std::to_string(id()) + "'"
      );
    }

    _dataStore._recovery_range_start.clear(); // reset to indicate no range start
    _dataStore._recovery_reader = recovery_reader; // remember last successful reader
    _dataStore._recovery_ref = ref; // ensure checkpoint file will not get removed
  }

  // reset data store meta, will be updated at runtime via properties(...)
  _dataStore._meta._cleanupIntervalStep = 0; // 0 == disable
  _dataStore._meta._commitIntervalMsec = 0; // 0 == disable
  _dataStore._meta._consolidationIntervalMsec = 0; // 0 == disable
  _dataStore._meta._consolidationPolicy = IResearchViewMeta::ConsolidationPolicy(); // disable
  _dataStore._meta._writebufferActive = options.segment_count_max;
  _dataStore._meta._writebufferIdle = options.segment_pool_size;
  _dataStore._meta._writebufferSizeMax = options.segment_memory_max;

  _asyncSelf = irs::memory::make_unique<AsyncLinkPtr::element_type>(this); // create a new 'self' (previous was reset during unload() above)
  _asyncTerminate.store(false); // allow new asynchronous job invocation
  _flushCallback = IResearchFeature::walFlushCallback(*this);

  // ...........................................................................
  // set up asynchronous maintenance tasks (if possible)
  // ...........................................................................

  _asyncFeature = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::iresearch::IResearchFeature // feature type
  >();

  if (_asyncFeature) {
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

        auto res = commitUnsafe(); // run commit ('_asyncSelf' locked by async task)

        if (!res.ok()) {
          LOG_TOPIC("8377b", WARN, arangodb::iresearch::TOPIC)
            << "error while committing arangosearch link '" << id() << "': " << res.errorNumber() << " " <<  res.errorMessage();
        } else if (state._cleanupIntervalStep // if enabled
                   && state._cleanupIntervalCount++ > state._cleanupIntervalStep) {
          state._cleanupIntervalCount = 0; // reset counter
          res = cleanupUnsafe(); // run cleanup ('_asyncSelf' locked by async task)

          if (!res.ok()) {
            LOG_TOPIC("130de", WARN, arangodb::iresearch::TOPIC)
              << "error while cleaning up arangosearch link '" << id() << "': " << res.errorNumber() << " " <<  res.errorMessage();
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

        auto res = // consolidate
          consolidateUnsafe(state._consolidationPolicy, state._progress); // run consolidation ('_asyncSelf' locked by async task)

        if (!res.ok()) {
          LOG_TOPIC("bce4f", WARN, arangodb::iresearch::TOPIC)
            << "error while consolidating arangosearch link '" << id() << "': " << res.errorNumber() << " " <<  res.errorMessage();
        } else if (state._cleanupIntervalStep // if enabled
                   && state._cleanupIntervalCount++ > state._cleanupIntervalStep) {
          state._cleanupIntervalCount = 0; // reset counter
          res = cleanupUnsafe(); // run cleanup ('_asyncSelf' locked by async task)

          if (!res.ok()) {
            LOG_TOPIC("31941", WARN, arangodb::iresearch::TOPIC)
              << "error while cleaning up arangosearch link '" << id() << "': " << res.errorNumber() << " " <<  res.errorMessage();
          }
        }

        return true; // reschedule
      }
    );
  }

  // ...........................................................................
  // set up in-recovery insertion hooks
  // ...........................................................................

  auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::DatabaseFeature // feature type
  >("Database");

  if (!dbFeature) {
    return arangodb::Result(); // nothing more to do
  }

  auto asyncSelf = _asyncSelf; // create copy for lambda

  return dbFeature->registerPostRecoveryCallback( // register callback
    [asyncSelf]()->arangodb::Result {
      SCOPED_LOCK(asyncSelf->mutex()); // ensure link does not get deallocated before callback finishes
      auto* link = asyncSelf->get();

      if (!link) {
        return arangodb::Result(); // link no longer in recovery state, i.e. during recovery it was created and later dropped
      }

      // before commit ensure that the WAL 'Flush' marker for the opened writer
      // was seen, otherwise this indicates a lost WAL tail during recovery
      // i.e. dataStore is ahead of the WAL
      if (RecoveryState::AFTER_CHECKPOINT != link->_dataStore._recovery) {
        return arangodb::Result( // result
          TRI_ERROR_INTERNAL, // code
          std::string("failed to find checkpoint after finishing recovery of arangosearch link '") + std::to_string(link->id()) + "'"
        );
      }

      link->_dataStore._recovery = RecoveryState::DONE; // set before commit() to trigger update of '_recovery_reader'/'_recovery_ref'

      LOG_TOPIC("5b59c", TRACE, arangodb::iresearch::TOPIC)
        << "starting sync for arangosearch link '" << link->id() << "'";

      auto res = link->commit();

      LOG_TOPIC("0e0ca", TRACE, arangodb::iresearch::TOPIC)
        << "finished sync for arangosearch link '" << link->id() << "'";

      return res;
    }
  );
}

arangodb::Result IResearchLink::insert( // insert document
  arangodb::transaction::Methods& trx, // transaction
  arangodb::LocalDocumentId const& documentId, // doc id
  arangodb::velocypack::Slice const& doc, // doc body
  arangodb::Index::OperationMode mode // insertion mode
) {
  if (!trx.state()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("failed to get transaction state while inserting a document into arangosearch link '") + std::to_string(id()) + "'"
    );
  }

  auto insertImpl = [this, &trx, &doc, &documentId](
                        irs::index_writer::documents_context& ctx) -> arangodb::Result {
    try {
      FieldIterator body(trx);

      return insertDocument(ctx, body, doc, documentId, _meta, id());
    } catch (arangodb::basics::Exception const& e) {
      return arangodb::Result(e.code(),
                              std::string("caught exception while inserting "
                                          "document into arangosearch link '") +
                                  std::to_string(id()) + "', revision '" +
                                  std::to_string(documentId.id()) + "': " + e.what());
    } catch (std::exception const& e) {
      return arangodb::Result(TRI_ERROR_INTERNAL,
                              std::string("caught exception while inserting "
                                          "document into arangosearch link '") +
                                  std::to_string(id()) + "', revision '" +
                                  std::to_string(documentId.id()) + "': " + e.what());
    } catch (...) {
      return arangodb::Result(TRI_ERROR_INTERNAL,
                              std::string("caught exception while inserting "
                                          "document into arangosearch link '") +
                                  std::to_string(id()) + "', revision '" +
                                  std::to_string(documentId.id()) + "'");
    }
  };

  auto& state = *(trx.state());
  auto* key = this;

// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* ctx = dynamic_cast<LinkTrxState*>(state.cookie(key));
#else
  auto* ctx = static_cast<LinkTrxState*>(state.cookie(key));
#endif

  if (!ctx) {
    SCOPED_LOCK_NAMED(_asyncSelf->mutex(),
                      lock);  // '_dataStore' can be asynchronously modified

    if (!*_asyncSelf) {
      return arangodb::Result(
          TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,  // the current link is no longer
                                              // valid (checked after ReadLock
                                              // aquisition)
          std::string("failed to lock arangosearch link while inserting a "
                      "document into arangosearch link '") +
              std::to_string(id()) + "'");
    }

    TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

    // optimization for single-document insert-only transactions
    if (trx.isSingleOperationTransaction() // only for single-docuemnt transactions
        && RecoveryState::DONE == _dataStore._recovery) {
      auto ctx = _dataStore._writer->documents();

      return insertImpl(ctx);
    }

    auto ptr = irs::memory::make_unique<LinkTrxState>(std::move(lock),
                                                      *(_dataStore._writer));

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx || !trx.addStatusChangeCallback(&_trxCallback)) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failed to store state into a TransactionState for insert into arangosearch link '") + std::to_string(id()) + "', tid '" + std::to_string(state.id()) + "', revision '" + std::to_string(documentId.id()) + "'"
      );
    }
  }

  switch (_dataStore._recovery) {
   case RecoveryState::BEFORE_CHECKPOINT:
    return arangodb::Result(); // ignore all insertions before 'checkpoint'
   case RecoveryState::DURING_CHECKPOINT:
    ctx->remove(documentId);
   default: // fall through
    {} // NOOP
  }

  // ...........................................................................
  // below only during recovery after the 'checkpoint' marker, or post recovery
  // ...........................................................................

  return insertImpl(ctx->_ctx);
}

bool IResearchLink::isHidden() const {
  return !arangodb::ServerState::instance()->isDBServer(); // hide links unless we are on a DBServer
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

size_t IResearchLink::memory() const {
  auto size = sizeof(IResearchLink); // includes empty members from parent

  size += _meta.memory();

  {
    SCOPED_LOCK(_asyncSelf->mutex()); // '_dataStore' can be asynchronously modified

    if (_dataStore) {
      // FIXME TODO this is incorrect since '_storePersisted' is on disk and not in memory
      size += directoryMemory(*(_dataStore._directory), id());
      size += _dataStore._path.native().size() * sizeof(irs::utf8_path::native_char_t);
    }
  }

  return size;
}

arangodb::Result IResearchLink::properties( // get link properties
    arangodb::velocypack::Builder& builder, // output buffer
    bool forPersistence // properties for persistance
) const {
  if (!builder.isOpenObject() // not an open object
      || !_meta.json(builder, forPersistence, nullptr, &(collection().vocbase()))) {
    return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
  }

  builder.add(
    arangodb::StaticStrings::IndexId,
    arangodb::velocypack::Value(std::to_string(_id))
  );
  builder.add(
    arangodb::StaticStrings::IndexType,
    arangodb::velocypack::Value(IResearchLinkHelper::type())
  );
  builder.add(
    StaticStrings::ViewIdField,
    arangodb::velocypack::Value(_viewGuid)
  );

  return arangodb::Result();
}

arangodb::Result IResearchLink::properties(IResearchViewMeta const& meta) {
  SCOPED_LOCK(_asyncSelf->mutex());  // '_dataStore' can be asynchronously modified

  if (!*_asyncSelf) {
    return arangodb::Result(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,  // the current link is no longer valid (checked after ReadLock aquisition)
                            std::string(
                                "failed to lock arangosearch link while "
                                "modifying properties of arangosearch link '") +
                                std::to_string(id()) + "'");
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
    LOG_TOPIC("c50c8", ERR, arangodb::iresearch::TOPIC)
        << "caught exception while modifying properties of arangosearch link '"
        << id() << "': " << e.what();
    IR_LOG_EXCEPTION();
    throw;
  } catch (...) {
    LOG_TOPIC("ad1eb", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while modifying properties of arangosearch link '"
        << id() << "'";
    IR_LOG_EXCEPTION();
    throw;
  }

  return arangodb::Result();
}

arangodb::Result IResearchLink::remove( // remove document
  arangodb::transaction::Methods& trx, // transaction
  arangodb::LocalDocumentId const& documentId, // doc id
  arangodb::velocypack::Slice const& /*doc*/, // doc body
  arangodb::Index::OperationMode /*mode*/ // removal mode
) {
  if (!trx.state()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("failed to get transaction state while removing a document into arangosearch link '") + std::to_string(id()) + "'"
    );
  }

  auto& state = *(trx.state());
  auto* key = this;

// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* ctx = dynamic_cast<LinkTrxState*>(state.cookie(key));
#else
  auto* ctx = static_cast<LinkTrxState*>(state.cookie(key));
#endif

  if (!ctx) {
    SCOPED_LOCK_NAMED(_asyncSelf->mutex(),
                      lock);  // '_dataStore' can be asynchronously modified

    if (!*_asyncSelf) {
      return arangodb::Result(
        TRI_ERROR_ARANGO_INDEX_HANDLE_BAD, // the current link is no longer valid (checked after ReadLock aquisition)
        std::string("failed to lock arangosearch link while removing a document from arangosearch link '") + std::to_string(id()) + "', tid '" + std::to_string(state.id()) + "', revision '" + std::to_string(documentId.id()) + "'"
      );
    }

    TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

    auto ptr = irs::memory::make_unique<LinkTrxState>(
      std::move(lock), *(_dataStore._writer)
    );

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx || !trx.addStatusChangeCallback(&_trxCallback)) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failed to store state into a TransactionState for remove from arangosearch link '") + std::to_string(id()) + "', tid '" + std::to_string(state.id()) + "', revision '" + std::to_string(documentId.id()) + "'"
      );
    }
  }

  switch (_dataStore._recovery) {
   case RecoveryState::BEFORE_CHECKPOINT:
    return arangodb::Result(); // ignore all removals before 'checkpoint'
   default: // fall through
   {} // NOOP
  }

  // ...........................................................................
  // below only during recovery after the 'checkpoint' marker, or post recovery
  // ...........................................................................

  // ...........................................................................
  // if an exception occurs below than the transaction is droped including all
  // all of its fid stores, no impact to iResearch View data integrity
  // ...........................................................................
  try {
    ctx->remove(documentId);

    return TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result(e.code(),
                            std::string(
                                "caught exception while removing document from "
                                "arangosearch link '") +
                                std::to_string(id()) + "', revision '" +
                                std::to_string(documentId.id()) + "': " + e.what());
  } catch (std::exception const& e) {
    return arangodb::Result(TRI_ERROR_INTERNAL,
                            std::string("caught exception while removing "
                                        "document from arangosearch link '") +
                                std::to_string(id()) + "', revision '" +
                                std::to_string(documentId.id()) + "': " + e.what());
  } catch (...) {
    return arangodb::Result(TRI_ERROR_INTERNAL,
                            std::string("caught exception while removing "
                                        "document from arangosearch link '") +
                                std::to_string(id()) + "', revision '" +
                                std::to_string(documentId.id()) + "'");
  }

  return arangodb::Result();
}

IResearchLink::Snapshot IResearchLink::snapshot() const {
  SCOPED_LOCK_NAMED(_asyncSelf->mutex(),
                    lock);  // '_dataStore' can be asynchronously modified

  if (!*_asyncSelf) {
    LOG_TOPIC("f42dc", WARN, arangodb::iresearch::TOPIC)
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

arangodb::Result IResearchLink::walFlushMarker( // process marker
    arangodb::velocypack::Slice const& value // marker value
) {
  if (!value.isString()) {
    return arangodb::Result( // result
      TRI_ERROR_BAD_PARAMETER, // code
      "non-string WAL 'Flush' marker value" // message
    );
  }

  auto valueRef = getStringRef(value);

  SCOPED_LOCK_NAMED(_asyncSelf->mutex(), lock); // '_dataStore' can be asynchronously modified

  if (!*_asyncSelf) {
    return arangodb::Result( // result
      TRI_ERROR_ARANGO_INDEX_HANDLE_BAD, // the current link is no longer valid (checked after ReadLock aquisition)
      std::string("failed to lock arangosearch link while processing 'Flush' marker arangosearch link '") + std::to_string(id()) + "'"
    );
  }

  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

  switch (_dataStore._recovery) {
   case RecoveryState::BEFORE_CHECKPOINT:
    if (valueRef == _dataStore._recovery_range_start) {
      _dataStore._recovery = RecoveryState::DURING_CHECKPOINT; // do insert with matching remove
    }

    break;
   case RecoveryState::DURING_CHECKPOINT:
    if (valueRef == _dataStore._recovery_reader.meta().filename) {
      _dataStore._recovery = RecoveryState::AFTER_CHECKPOINT; // do insert without matching remove
    } else if (valueRef != _dataStore._recovery_range_start) { // fake 'DURING_CHECKPOINT' set in initDataStore(...) as initial value to cover worst case and not in that case (seen another checkpoint)
      _dataStore._recovery = RecoveryState::BEFORE_CHECKPOINT; // ignore inserts (scenario for initial state before any checkpoints were seen)
    } // else fake 'DURING_CHECKPOINT' set in initDataStore(...) as initial value to cover worst case and actually in that case (leave as is)
    break;
   case RecoveryState::AFTER_CHECKPOINT:
    break; // NOOP
   case RecoveryState::DONE:
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL,
      std::string("arangosearch link not in recovery while processing 'Flush' marker arangosearch link '") + std::to_string(id()) + "'"
    );
  }

  return arangodb::Result();
}

arangodb::Result IResearchLink::unload() {
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

  _flushCallback = {}; // reset together with '_asyncSelf', it's an std::function so don't use a constructor or ASAN complains
  _asyncSelf->reset(); // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  try {
    if (_dataStore) {
      _dataStore._reader.reset(); // reset reader to release file handles
      _dataStore._writer.reset();
      _dataStore._directory.reset();
    }
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result( // result
      e.code(), // code
      std::string("caught exception while unloading arangosearch link '") + std::to_string(id()) + "': " + e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while removing arangosearch link '") + std::to_string(id()) + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("caught exception while removing arangosearch link '") + std::to_string(id()) + "'"
    );
  }

  return arangodb::Result();
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
