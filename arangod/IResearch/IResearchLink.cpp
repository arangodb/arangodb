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

#include "IResearchCommon.h"
#include "IResearchLinkHelper.h"
#include "IResearchPrimaryKeyFilter.h"
#include "IResearchView.h"
#include "IResearchViewDBServer.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterInfo.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include "IResearchLink.h"

NS_LOCAL

////////////////////////////////////////////////////////////////////////////////
/// @brief the storage format used with IResearch writers
////////////////////////////////////////////////////////////////////////////////
const irs::string_ref IRESEARCH_STORE_FORMAT("1_0");

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief container storing the link state for a given TransactionState
////////////////////////////////////////////////////////////////////////////////
struct LinkTrxState final: public arangodb::TransactionState::Cookie {
  irs::index_writer::documents_context _ctx;
  std::unique_lock<ReadMutex> _linkLock; // prevent data-store deallocation (lock @ AsyncSelf)
  arangodb::iresearch::PrimaryKeyFilterContainer _removals; // list of document removals

  LinkTrxState(
      std::unique_lock<ReadMutex>&& linkLock,
      irs::index_writer& writer
  ) noexcept: _ctx(writer.documents()), _linkLock(std::move(linkLock)) {
    TRI_ASSERT(_linkLock.owns_lock());
  }

  virtual ~LinkTrxState() noexcept {
    if (_removals.empty()) {
      return; // nothing to do
    }

    try {
      // hold references even after transaction
      _ctx.remove(
        irs::filter::make<arangodb::iresearch::PrimaryKeyFilterContainer>(std::move(_removals))
      );
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "caught exception while applying accumulated removals: " << e.what();
      IR_LOG_EXCEPTION();
    } catch (...) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "caught exception while applying accumulated removals";
      IR_LOG_EXCEPTION();
    }
  }

  operator irs::index_writer::documents_context&() noexcept {
    return _ctx;
  }

  void remove(TRI_voc_cid_t cid, TRI_voc_rid_t rid) {
    _ctx.remove(_removals.emplace(cid, rid));
  }

  void reset() noexcept {
    _removals.clear();
    _ctx.reset();
  }
};

////////////////////////////////////////////////////////////////////////////////
// @brief approximate data store directory instance size
////////////////////////////////////////////////////////////////////////////////
size_t directoryMemory(
    irs::directory const& directory,
    TRI_idx_iid_t id
) noexcept {
  size_t size = 0;

  try {
    directory.visit([&directory, &size](std::string& file)->bool {
      uint64_t length;

      size += directory.length(length, file) ? length : 0;

      return true;
    });
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while calculating size of arangosearch link '" << id << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while calculating size of arangosearch link '" << id << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
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
irs::utf8_path getPersistedPath(
    arangodb::DatabasePathFeature const& dbPathFeature,
    arangodb::LogicalCollection const& collection,
    arangodb::LogicalView const& view
) {
  irs::utf8_path dataPath(dbPathFeature.directory());
  static const std::string subPath("databases");
  static const std::string dbPath("database-");

  dataPath /= subPath;
  dataPath /= dbPath;
  dataPath += std::to_string(collection.vocbase().id());
  dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
  dataPath += "-";
  dataPath += std::to_string(collection.id()); // has to be 'id' since this can be a per-shard collection
  dataPath += "_";
  dataPath += std::to_string(view.planId()); // has to be 'planId' since this is a cluster-wide view

  return dataPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts ArangoDB document into an IResearch data store
////////////////////////////////////////////////////////////////////////////////
inline void insertDocument(
    irs::segment_writer::document& doc,
    arangodb::iresearch::FieldIterator& body,
    TRI_voc_cid_t cid,
    TRI_voc_rid_t rid) {
  using namespace arangodb::iresearch;

  // reuse the 'Field' instance stored
  // inside the 'FieldIterator' after
  auto& field = const_cast<Field&>(*body);

  // User fields
  while (body.valid()) {
    if (ValueStorage::NONE == field._storeValues) {
      doc.insert(irs::action::index, field);
    } else {
      doc.insert(irs::action::index_store, field);
    }

    ++body;
  }

  // System fields
  DocumentPrimaryKey const primaryKey(cid, rid);

  // Indexed and Stored: CID + RID
  Field::setPkValue(field, primaryKey, Field::init_stream_t());
  doc.insert(irs::action::index_store, field);

  // Indexed: CID
  Field::setCidValue(field, primaryKey.first);
  doc.insert(irs::action::index, field);
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchLink::IResearchLink(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection
): _asyncSelf(irs::memory::make_unique<AsyncLinkPtr::element_type>(nullptr)), // mark as data store not initialized
   _collection(collection),
   _id(iid),
   _inRecovery(false) {
  auto* key = this;

  // initialize transaction callback
  _trxCallback = [key](
      arangodb::transaction::Methods& trx,
      arangodb::transaction::Status status
  )->void {
    auto* state = trx.state();

    // check state of the top-most transaction only
    if (!state) {
      return; // NOOP
    }

    auto prev = state->cookie(key, nullptr); // get existing cookie
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
  auto res = unload(); // disassociate from view if it has not been done yet

  if (!res.ok()) {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
      << "failed to unload arangosearch link in link destructor: " << res.errorNumber() << " " << res.errorMessage();
  }
}

bool IResearchLink::operator==(LogicalView const& view) const noexcept {
  return _viewGuid == view.guid();
}

bool IResearchLink::operator==(IResearchLinkMeta const& meta) const noexcept {
  return _meta == meta;
}

void IResearchLink::afterTruncate() {
  SCOPED_LOCK(_asyncSelf->mutex()); // '_dataStore' can be asynchronously modified

  if (!*_asyncSelf) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_ARANGO_INDEX_HANDLE_BAD, // the current link is no longer valid (checked after ReadLock aquisition)
      std::string("failed to lock arangosearch link while truncating arangosearch link '") + std::to_string(id()) + "'"
    );
  }

  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

  try {
    _dataStore._writer->clear();
    _dataStore._reader = _dataStore._reader.reopen();
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
      << "caught exception while truncating arangosearch link '" << id() << "': " << e.what();
    IR_LOG_EXCEPTION();
    throw;
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while truncating arangosearch link '" << id() << "'";
    IR_LOG_EXCEPTION();
    throw;
  }
}

void IResearchLink::batchInsert(
    arangodb::transaction::Methods& trx,
    std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& batch,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue
) {
  if (batch.empty()) {
    return; // nothing to do
  }

  if (!queue) {
    throw std::runtime_error(std::string("failed to report status during batch insert for arangosearch link '") + arangodb::basics::StringUtils::itoa(_id) + "'");
  }

  if (!trx.state()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
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
    SCOPED_LOCK_NAMED(_asyncSelf->mutex(), lock); // '_dataStore' can be asynchronously modified

    if (!*_asyncSelf) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to lock arangosearch link while inserting a batch into arangosearch link '" << id() << "', tid '" << state.id() << "'";
      queue->setStatus(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD); // the current link is no longer valid (checked after ReadLock aquisition)

      return;
    }

    TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

    auto ptr = irs::memory::make_unique<LinkTrxState>(
      std::move(lock), *(_dataStore._writer)
    );

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx || !trx.addStatusChangeCallback(&_trxCallback)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to store state into a TransactionState for batch insert into arangosearch link '" << id() << "', tid '" << state.id() << "'";
      queue->setStatus(TRI_ERROR_INTERNAL);

      return;
    }
  }

  if (_inRecovery) {
    for (auto const& doc: batch) {
      ctx->remove(_collection.id(), doc.first.id());
    }
  }

  auto begin = batch.begin();
  auto const end = batch.end();

  try {
    for (FieldIterator body; begin != end; ++begin) {
      body.reset(begin->second, _meta);

      if (!body.valid()) {
        continue; // find first valid document
      }

      auto doc = ctx->_ctx.insert();

      insertDocument(doc, body, _collection.id(), begin->first.id());

      if (!doc) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failed inserting batch into arangosearch link '" << id() << "'";
        queue->setStatus(TRI_ERROR_INTERNAL);

        return;
      }
    }
  } catch (arangodb::basics::Exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while inserting batch into arangosearch link '" << id() << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
    queue->setStatus(e.code());
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while inserting batch into arangosearch link '" << id() << "': " << e.what();
    IR_LOG_EXCEPTION();
    queue->setStatus(TRI_ERROR_INTERNAL);
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while inserting batch into arangosearch link '" << id() << "'";
    IR_LOG_EXCEPTION();
    queue->setStatus(TRI_ERROR_INTERNAL);
  }
}

bool IResearchLink::canBeDropped() const {
  return true; // valid for a link to be dropped from an iResearch view
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

  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

  try {
    _dataStore._writer->commit();

    SCOPED_LOCK(_readerMutex);
    auto reader = _dataStore._reader.reopen(); // update reader

    if (!reader) {
      // nothing more to do
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to update snapshot after commit, reuse the existing snapshot for arangosearch link '" << id() << "'";

      return arangodb::Result();
    }

    if (_dataStore._reader != reader) {
       _dataStore._reader = reader; // update reader
    }
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result(
      e.code(),
      std::string("caught exception while committing arangosearch link '") + std::to_string(id()) + "': " + e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while committing arangosearch link '") + std::to_string(id()) + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while committing arangosearch link '") + std::to_string(id()) + "'"
    );
  }

  return arangodb::Result();
}

arangodb::Result IResearchLink::consolidate(
  IResearchViewMeta::ConsolidationPolicy const& policy,
  irs::merge_writer::flush_progress_t const& progress,
  bool runCleanupAfterConsolidation
) {
  char runId = 0; // value not used
  SCOPED_LOCK(_asyncSelf->mutex()); // '_dataStore' can be asynchronously modified

  if (!*_asyncSelf) {
    return arangodb::Result(
      TRI_ERROR_ARANGO_INDEX_HANDLE_BAD, // the current link is no longer valid (checked after ReadLock aquisition)
      std::string("failed to lock arangosearch link while consolidating arangosearch link '") + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "'"
    );
  }

  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

  // ...........................................................................
  // apply consolidation policy
  // ...........................................................................

  // skip if no valid policy to execute
  if (policy.policy()) {
    LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
      << "start execution of consolidation policy '" << policy.properties().toString() << "' on arangosearch link '" << id() << "' run id '" << size_t(&runId) << "'";

    try {
      _dataStore._writer->consolidate(policy.policy(), nullptr, progress);
    } catch (arangodb::basics::Exception const& e) {
      return arangodb::Result(
        e.code(),
        std::string("caught exception while executing consolidation policy '") + policy.properties().toString() + "' on arangosearch link '" + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what()
      );
    } catch (std::exception const& e) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception while executing consolidation policy '") + policy.properties().toString() + "' on arangosearch link '" + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what()
      );
    } catch (...) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception while executing consolidation policy '") + policy.properties().toString() + "' on arangosearch link '" + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "'"
      );
    }

    LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
      << "finish execution of consolidation policy '" << policy.properties().toString() << "' on arangosearch link '" << id() << "' run id '" << size_t(&runId) << "'";
  }

  if (!runCleanupAfterConsolidation) {
    return arangodb::Result(); // done
  }

  // ...........................................................................
  // apply cleanup
  // ...........................................................................

  LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
    << "starting cleanup of arangosearch link '" << id() << "' run id '" << size_t(&runId) << "'";

  try {
    irs::directory_utils::remove_all_unreferenced(*(_dataStore._directory));
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result(
      e.code(),
      std::string("caught exception during cleanup of arangosearch link '") + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception during cleanup of arangosearch link '") + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception during cleanup of arangosearch link '") + std::to_string(id()) + "' run id '" + std::to_string(size_t(&runId)) + "'"
    );
  }

  LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
    << "finish cleanup of arangosearch link '" << id() << "' run id '" << size_t(&runId) << "'";

  return arangodb::Result();
}

arangodb::Result IResearchLink::drop() {
  auto view = IResearchLink::view();

  if (view) {
    view->unlink(_collection.id()); // unlink before reset() to release lock in view (if any)
  }

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

bool IResearchLink::hasBatchInsert() const {
  return true;
}

bool IResearchLink::hasSelectivityEstimate() const {
  return false; // selectivity can only be determined per query since multiple fields are indexed
}

TRI_idx_iid_t IResearchLink::id() const noexcept {
  return _id;
}

arangodb::Result IResearchLink::init(
    arangodb::velocypack::Slice const& definition
) {
  // disassociate from view if it has not been done yet
  if (!unload().ok()) {
    return arangodb::Result(TRI_ERROR_INTERNAL, "failed to unload link");
  }

  std::string error;
  IResearchLinkMeta meta;

  if (!meta.init(definition, error)) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error parsing view link parameters from json: ") + error
    );
  }

  if (!definition.isObject()
      || !definition.get(StaticStrings::ViewIdField).isString()) {
    return arangodb::Result(
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
      std::string("error finding view for link '") + std::to_string(_id) + "'"
    );
  }

  // we continue to support the old and new ID format
  auto idSlice = definition.get(StaticStrings::ViewIdField);
  auto viewId = idSlice.copyString();
  auto& vocbase = _collection.vocbase();
  auto logicalView = arangodb::ServerState::instance()->isCoordinator()
                     && arangodb::ClusterInfo::instance()
    ? arangodb::ClusterInfo::instance()->getView(vocbase.name(), viewId)
    : vocbase.lookupView(viewId) // will only contain IResearchView (even for a DBServer)
    ;

  // creation of link on a DBServer
  if (!logicalView && arangodb::ServerState::instance()->isDBServer()) {
    auto* ci = ClusterInfo::instance();

    if (!ci) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failure to find 'ClusterInfo' instance for lookup of link '") + std::to_string(_id) + "'"
      );
    }

    auto logicalWiew = ci->getView(vocbase.name(), viewId);
    auto* wiew = LogicalView::cast<IResearchViewDBServer>(logicalWiew.get());

    if (wiew) {
      // FIXME figure out elegant way of testing for cluster wide LogicalCollection
      if (_collection.id() == _collection.planId() && _collection.isAStub()) {
      // this is a cluster-wide collection/index/link (per-cid view links have their corresponding collections in vocbase)
        auto clusterCol = ci->getCollectionCurrent(
          vocbase.name(), std::to_string(_collection.id())
        );

        if (clusterCol) {
          for (auto& entry: clusterCol->errorNum()) {
            auto collection = vocbase.lookupCollection(entry.first); // find shard collection

            if (collection) {
              // ensure the shard collection is registered with the cluster-wide view
              // required from creating snapshots for per-cid views loaded from WAL
              // only register existing per-cid view instances, do not create new per-cid view
              // instances since they will be created/registered  by their per-cid links just below
              wiew->ensure(collection->id(), false);
            }
          }
        }

        return arangodb::Result(); // leave '_view' uninitialized to mark the index as unloaded/unusable
      }

      logicalView = wiew->ensure(_collection.id()); // repoint LogicalView at the per-cid instance
    }
  }

  if (!logicalView
      || arangodb::iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
    return arangodb::Result(
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
      std::string("error finding view: '") + viewId + "' for link '" + std::to_string(_id) + "' : no such view"
    );
  }

  if (!arangodb::ServerState::instance()->isCoordinator()) { // link on coordinator does not have a data-store
    auto* dbServerView = dynamic_cast<IResearchViewDBServer*>(logicalView.get());

    // dbserver has both IResearchViewDBServer and IResearchView instances
    auto* view = LogicalView::cast<IResearchView>(
      dbServerView
        ? dbServerView->ensure(_collection.id()).get()
        : logicalView.get()
    );

    if (!view) {
      return arangodb::Result(
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        std::string("error finding view: '") + viewId + "' for link '" + std::to_string(_id) + "'"
      );
    }

    auto res = initDataStore(*view);

    if (!res.ok()) {
      return res;
    }

    if (!view->link(_asyncSelf)) {
      unload(); // unlock the directory
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to link with view '") + view->name() + "' while initializing link '" + std::to_string(_id) + "'"
      );
    }
  }

  const_cast<std::string&>(_viewGuid) = logicalView->guid(); // ensue that this is a GUID (required by operator==(IResearchView))
  const_cast<IResearchLinkMeta&>(_meta) = std::move(meta);

  return arangodb::Result();
}

arangodb::Result IResearchLink::initDataStore(IResearchView const& view) {
  _asyncSelf->reset(); // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  auto* dbPathFeature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::DatabasePathFeature
  >("DatabasePath");

  if (!dbPathFeature) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to find feature 'DatabasePath' while initializing link '") + std::to_string(_id) + "'"
    );
  }

  auto viewMeta = view.meta();

  if (!viewMeta) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to get arangosearch view meta while initializing link '") + std::to_string(_id) + "'"
    );
  }

  irs::index_writer::options options;

  {
    SCOPED_LOCK(viewMeta->read());

    options.lock_repository = false; // do not lock index, ArangoDB has it's own lock
    options.segment_count_max = viewMeta->_writebufferActive;
    options.segment_memory_max = viewMeta->_writebufferSizeMax;
    options.segment_pool_size = viewMeta->_writebufferIdle;
  }

  auto format = irs::formats::get(IRESEARCH_STORE_FORMAT);

  if (!format) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to get data store codec '") + IRESEARCH_STORE_FORMAT.c_str() + "' while initializing link '" + std::to_string(_id) + "'"
    );
  }

  bool pathExists;

  _dataStore._path = getPersistedPath(*dbPathFeature, _collection, view);

  // must manually ensure that the data store directory exists (since not using a lockfile)
  if (_dataStore._path.exists_directory(pathExists)
      && !pathExists
      && !_dataStore._path.mkdir()) {
    return arangodb::Result(
      TRI_ERROR_CANNOT_CREATE_DIRECTORY,
      std::string("failed to create data store directory with path '") + _dataStore._path.utf8() + "' while initializing link '" + std::to_string(_id) + "'"
    );
  }

  _dataStore._directory =
    irs::directory::make<irs::mmap_directory>(_dataStore._path.utf8());

  if (!_dataStore._directory) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to instantiate data store directory with path '") + _dataStore._path.utf8() + "' while initializing link '" + std::to_string(_id) + "'"
    );
  }

  // create writer before reader to ensure data directory is present
  _dataStore._writer = irs::index_writer::make(
    *(_dataStore._directory), format, irs::OM_CREATE | irs::OM_APPEND, options
  );

  if (!_dataStore._writer) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to instantiate data store writer with path '") + _dataStore._path.utf8() + "' while initializing link '" + std::to_string(_id) + "'"
    );
  }

  _dataStore._writer->commit(); // initialize 'store'
  _dataStore._reader = irs::directory_reader::open(*(_dataStore._directory));

  if (!_dataStore._reader) {
    _dataStore._writer.reset(); // unlock the directory
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to instantiate data store reader with path '") + _dataStore._path.utf8() + "' while initializing link '" + std::to_string(_id) + "'"
    );
  }

  _asyncSelf = irs::memory::make_unique<AsyncLinkPtr::element_type>(this); // create a new 'self' (previous was reset during unload() above)

  auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::DatabaseFeature
  >("Database");

  if (!dbFeature) {
    return arangodb::Result(); // nothing more to do
  }

  // ...........................................................................
  // set up in-recovery insertion hooks
  // ...........................................................................

  auto* engine = arangodb::EngineSelectorFeature::ENGINE;

  if (!engine) {
    _dataStore._writer.reset(); // unlock the directory
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to get storage engine while initializing arangosearch link: ") + std::to_string(id())
    );
  }

  _inRecovery = engine->inRecovery();

  auto asyncSelf = _asyncSelf; // create copy for lambda

  return dbFeature->registerPostRecoveryCallback(
    [asyncSelf]()->arangodb::Result {
      SCOPED_LOCK(asyncSelf->mutex()); // ensure link does not get deallocated before callback finishes
      auto* link = asyncSelf->get();

      if (!link) {
        return arangodb::Result(); // link no longer in recovery state, i.e. during recovery it was created and later dropped
      }

      LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
        << "starting sync for arangosearch link '" << link->id() << "'";

      auto res = link->commit();

      LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
        << "finished sync for arangosearch link '" << link->id() << "'";

      link->_inRecovery = false;

      return res;
    }
  );
}

arangodb::Result IResearchLink::insert(
  arangodb::transaction::Methods& trx,
  arangodb::LocalDocumentId const& documentId,
  arangodb::velocypack::Slice const& doc,
  arangodb::Index::OperationMode mode
) {
  if (!trx.state()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("failed to get transaction state while inserting a document into arangosearch link '") + std::to_string(id()) + "'"
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
    SCOPED_LOCK_NAMED(_asyncSelf->mutex(), lock); // '_dataStore' can be asynchronously modified

    if (!*_asyncSelf) {
      return arangodb::Result(
        TRI_ERROR_ARANGO_INDEX_HANDLE_BAD, // the current link is no longer valid (checked after ReadLock aquisition)
        std::string("failed to lock arangosearch link while inserting a document into arangosearch link '") + std::to_string(id()) + "'"
      );
    }

    TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

    auto ptr = irs::memory::make_unique<LinkTrxState>(
      std::move(lock), *(_dataStore._writer)
    );

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx || !trx.addStatusChangeCallback(&_trxCallback)) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to store state into a TransactionState for insert into arangosearch link '") + std::to_string(id()) + "', tid '" + std::to_string(state.id()) + "', revision '" + std::to_string(documentId.id()) + "'"
      );
    }
  }

  if (_inRecovery) {
    ctx->remove(_collection.id(), documentId.id());
  }

  try {
    FieldIterator body(doc, _meta);

    if (!body.valid()) {
      return arangodb::Result(); // nothing to index
    }

    auto doc = ctx->_ctx.insert();

    insertDocument(doc, body, _collection.id(), documentId.id());

    if (!doc) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to insert document into arangosearch link '") + std::to_string(id()) + "', revision '" + std::to_string(documentId.id()) + "'"
      );
    }
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result(
      e.code(),
      std::string("caught exception while inserting document into arangosearch link '") + std::to_string(id()) + "', revision '" + std::to_string(documentId.id()) + "': " + e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while inserting document into arangosearch link '") + std::to_string(id()) + "', revision '" + std::to_string(documentId.id()) + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while inserting document into arangosearch link '") + std::to_string(id()) + "', revision '" + std::to_string(documentId.id()) + "'"
    );
  }

  return arangodb::Result();
}

bool IResearchLink::isPersistent() const {
  auto* engine = arangodb::EngineSelectorFeature::ENGINE;

  // FIXME TODO remove once MMFilesEngine will fillIndex(...) during recovery
  // currently the index is created but fill is deffered untill the end of recovery
  // at the end of recovery only non-persistent indexes are filled
  if (engine && engine->inRecovery()) {
    return false;
  }

  return true; // records persisted into the iResearch view
}

bool IResearchLink::isSorted() const {
  return false; // iResearch does not provide a fixed default sort order
}

bool IResearchLink::json(arangodb::velocypack::Builder& builder) const {
  if (!builder.isOpenObject() || !_meta.json(builder)) {
    return false;
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

  return true;
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

  return other.init(slice, errorField) && _meta == other;
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

arangodb::Result IResearchLink::remove(
  arangodb::transaction::Methods& trx,
  arangodb::LocalDocumentId const& documentId,
  arangodb::velocypack::Slice const& /*doc*/,
  arangodb::Index::OperationMode /*mode*/
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
    SCOPED_LOCK_NAMED(_asyncSelf->mutex(), lock); // '_dataStore' can be asynchronously modified

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
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to store state into a TransactionState for remove from arangosearch link '") + std::to_string(id()) + "', tid '" + std::to_string(state.id()) + "', revision '" + std::to_string(documentId.id()) + "'"
      );
    }
  }

  // ...........................................................................
  // if an exception occurs below than the transaction is droped including all
  // all of its fid stores, no impact to iResearch View data integrity
  // ...........................................................................
  try {
    ctx->remove(_collection.id(), documentId.id());

    return TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result(
      e.code(),
     std::string("caught exception while removing document from arangosearch link '") + std::to_string(id()) + "', revision '" + std::to_string(documentId.id()) + "': " + e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while removing document from arangosearch link '") + std::to_string(id()) + "', revision '" + std::to_string(documentId.id()) + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while removing document from arangosearch link '") + std::to_string(id()) + "', revision '" + std::to_string(documentId.id()) + "'"
    );
  }

  return arangodb::Result();
}

IResearchLink::Snapshot IResearchLink::snapshot() const {
  SCOPED_LOCK_NAMED(_asyncSelf->mutex(), lock); // '_dataStore' can be asynchronously modified

  if (!*_asyncSelf) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to lock arangosearch link while retrieving snapshot from arangosearch link '" << id() << "'";

    return Snapshot(); // return an empty reader
  }

  TRI_ASSERT(_dataStore); // must be valid if _asyncSelf->get() is valid

  return Snapshot(std::move(lock), irs::directory_reader(_dataStore._reader)); // return a copy of the current reader
}

Index::IndexType IResearchLink::type() const {
  // TODO: don't use enum
  return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
}

char const* IResearchLink::typeName() const {
  return IResearchLinkHelper::type().c_str();
}

arangodb::Result IResearchLink::unload() {
  // this code is used by the MMFilesEngine
  // if the collection is in the process of being removed then drop it from the view
  // FIXME TODO remove once LogicalCollection::drop(...) will drop its indexes explicitly
  if (_collection.deleted()
      || TRI_vocbase_col_status_e::TRI_VOC_COL_STATUS_DELETED == _collection.status()) {
    return drop();
  }

  _asyncSelf->reset(); // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  try {
    if (_dataStore) {
      _dataStore._reader.reset(); // reset reader to release file handles
      _dataStore._writer.reset();
      _dataStore._directory.reset();
    }
  } catch (arangodb::basics::Exception& e) {
    return arangodb::Result(
      e.code(),
      std::string("caught exception while unloading arangosearch link '") + std::to_string(id()) + "': " + e.what()
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

std::shared_ptr<IResearchView> IResearchLink::view() const {
  // FIXME TODO change to lookup in CollectionNameResolver once per-shard views are removed
  return std::dynamic_pointer_cast<IResearchView>(
    arangodb::ServerState::instance()->isCoordinator()
    && arangodb::ClusterInfo::instance()
    ? arangodb::ClusterInfo::instance()->getView(_collection.vocbase().name(), _viewGuid)
    : _collection.vocbase().lookupView(_viewGuid) // always look up in vocbase (single server or cluster per-shard view)
  );
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------