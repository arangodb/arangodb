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
#include <store/store_utils.hpp>
#include <utils/encryption.hpp>
#include <utils/singleton.hpp>

#include "IResearchLink.h"
#include "IResearchDocument.h"

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
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"


#include "RestServer/FlushFeature.h" // FIXME: remove!

using namespace std::literals;

namespace {

using namespace arangodb;
using namespace arangodb::iresearch;

using irs::async_utils::read_write_mutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts ArangoDB document into an IResearch data store
////////////////////////////////////////////////////////////////////////////////
inline Result insertDocument(irs::index_writer::documents_context& ctx,
                             transaction::Methods const& trx,
                             FieldIterator& body,
                             velocypack::Slice const& document,
                             LocalDocumentId const& documentId,
                             IResearchLinkMeta const& meta,
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

}  // namespace

namespace arangodb {
namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                                     IResearchLink
// -----------------------------------------------------------------------------


IResearchLink::IResearchLink(IndexId iid, LogicalCollection& collection)
    : IResearchDataStore(iid, collection) {
}

IResearchLink::~IResearchLink() {
  Result res;
  try {
    res = unload();  // disassociate from view if it has not been done yet
  } catch (...) { }

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
          << "' while dropping arangosearch link '" << _id.id() << "'";
    } else {
      view->unlink(_collection.id()); // unlink before reset() to release lock in view (if any)
    }
  }

  std::atomic_store(&_flushSubscription, {}); // reset together with '_asyncSelf'
  _asyncSelf->reset(); // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  try {
    if (_dataStore) {
      _dataStore.resetDataStore();
    }

    bool exists;

    // remove persisted data store directory if present
    if (!_dataStore._path.exists_directory(exists)
        || (exists && !_dataStore._path.remove())) {
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

bool IResearchLink::hasSelectivityEstimate() const {
  return false; // selectivity can only be determined per query since multiple fields are indexed
}

void IResearchLink::afterCommit() {
   // invalidate query cache
   aql::QueryCache::instance()->invalidate(&(_collection.vocbase()), _viewGuid);
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
  if (!meta.init(_collection.vocbase().server(), definition, true, error,
                 _collection.vocbase().name())) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "error parsing view link parameters from json: " + error
    };
  }

  if (!definition.isObject() // not object
      || !definition.get(StaticStrings::ViewIdField).isString()) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            "error finding view for link '" + std::to_string(_id.id()) + "'"};
  }

  auto viewId = definition.get(StaticStrings::ViewIdField).copyString();
  auto& vocbase = _collection.vocbase();
  bool const sorted = !meta._sort.empty();
  auto const& storedValuesColumns = meta._storedValues.columns();
  TRI_ASSERT(meta._sortCompression);
  auto const primarySortCompression = meta._sortCompression
      ? meta._sortCompression
      : getDefaultCompression();
  if (ServerState::instance()->isCoordinator()) { // coordinator link
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
    if (!vocbase.server().hasFeature<ClusterFeature>()) {
      return {
          TRI_ERROR_INTERNAL,
          "failure to get cluster info while initializing arangosearch link '" +
              std::to_string(_id.id()) + "'"};
    }
    if (vocbase.server().getFeature<ClusterFeature>().isEnabled()) {
      auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();

      // cluster-wide link
      auto clusterWideLink = _collection.id() == _collection.planId() && _collection.isAStub();

      // upgrade step for old link definition without collection name
      // this could be received from  agency while shard of the collection was moved (or added)
      // to the server.
      // New links already has collection name set, but here we must get this name on our own
      if (meta._collectionName.empty()) {
        if (clusterWideLink) {// could set directly
          LOG_TOPIC("86ecd", TRACE, iresearch::TOPIC) << "Setting collection name '" << _collection.name() << "' for new link '"
            << this->id().id() << "'";
          meta._collectionName = _collection.name();
        } else {
          meta._collectionName = ci.getCollectionNameForShard(_collection.name());
          LOG_TOPIC("86ece", TRACE, iresearch::TOPIC) << "Setting collection name '" << meta._collectionName << "' for new link '"
            << this->id().id() << "'";
        }
        if (ADB_UNLIKELY(meta._collectionName.empty())) {
          LOG_TOPIC("67da6", WARN, iresearch::TOPIC) << "Failed to init collection name for the link '"
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
        auto const res = initDataStore(initCallback, sorted, storedValuesColumns, primarySortCompression);

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
          return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "error finding view: '" + viewId + "' for link '" +
                      std::to_string(_id.id()) + "' : no such view"};
        }

        auto* view = LogicalView::cast<IResearchView>(logicalView.get());

        if (!view) {
          unload(); // unlock the data store directory
          return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "error finding view: '" + viewId + "' for link '" +
                      std::to_string(_id.id()) + "'"};
        }

        viewId = view->guid(); // ensue that this is a GUID (required by operator==(IResearchView))

        if (clusterWideLink) { // cluster cluster-wide link
          auto shardIds = _collection.shardIds();

          // go through all shard IDs of the collection and try to link any links
          // missing links will be populated when they are created in the
          // per-shard collection
          if (shardIds) {
            for (auto& entry : *shardIds) {
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
    } else {
      LOG_TOPIC("67dd6", DEBUG, iresearch::TOPIC) << "Skipped link '"
        << this->id().id() << "' due to disabled cluster features.";
    }
  } else if (ServerState::instance()->isSingleServer()) {  // single-server link
    // prepare data-store which can then update options
    // via the IResearchView::link(...) call
    auto const res = initDataStore(initCallback, sorted, storedValuesColumns, primarySortCompression);

    if (!res.ok()) {
      return res;
    }

    auto logicalView = vocbase.lookupView(viewId);

    // if there is no logicalView present yet then skip this step
    if (logicalView) {
      if (iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
        unload(); // unlock the data store directory

        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                "error finding view: '" + viewId + "' for link '" +
                    std::to_string(_id.id()) + "' : no such view"};
      }

      auto* view = LogicalView::cast<IResearchView>(logicalView.get());

      if (!view) {
        unload(); // unlock the data store directory

        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                "error finding view: '" + viewId + "' for link '" +
                    std::to_string(_id.id()) + "'"};
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

Result IResearchLink::insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const doc) {
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

  return other.init(_collection.vocbase().server(), slice, true, errorField,
                    _collection.vocbase().name())  // for db-server analyzer validation should have already apssed on coordinator (missing analyzer == no match)
         && _meta == other;
}

Result IResearchLink::properties(
    velocypack::Builder& builder,
    bool forPersistence) const {
  if (!builder.isOpenObject()  // not an open object
      || !_meta.json(_collection.vocbase().server(), builder, forPersistence,
                     nullptr, &(_collection.vocbase()))) {
    return Result(TRI_ERROR_BAD_PARAMETER);
  }

  builder.add(arangodb::StaticStrings::IndexId,
              velocypack::Value(std::to_string(_id.id())));
  builder.add(
    arangodb::StaticStrings::IndexType,
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

Result IResearchLink::remove(
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

Index::IndexType IResearchLink::type() const {
  // TODO: don't use enum
  return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
}

char const* IResearchLink::typeName() const {
  return IResearchLinkHelper::type().c_str();
}

bool IResearchLink::setCollectionName(irs::string_ref name) noexcept {
  TRI_ASSERT(!name.empty());
  if (_meta._collectionName.empty()) {
    auto nonConstMeta = const_cast<IResearchLinkMeta*>(&_meta);
    nonConstMeta->_collectionName = name;
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
  if (_collection.deleted() // collection deleted
      || TRI_vocbase_col_status_e::TRI_VOC_COL_STATUS_DELETED == _collection.status()) {
    return drop();
  }

  std::atomic_store(&_flushSubscription, {}); // reset together with '_asyncSelf'
  _asyncSelf->reset(); // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

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

IResearchViewStoredValues const& IResearchLink::storedValues() const noexcept {
  return _meta._storedValues;
}
}  // namespace iresearch
}  // namespace arangodb
