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

#include "IResearchCommon.h"
#include "IResearchFeature.h"
#include "IResearchLink.h"
#include "IResearchLinkHelper.h"
#include "VelocyPackHelper.h"

#include "Aql/AstNode.h"
#include "Aql/PlanCache.h"
#include "Aql/QueryCache.h"
#include "Basics/StaticStrings.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"

#include "IResearchView.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief surrogate root for all queries without a filter
////////////////////////////////////////////////////////////////////////////////
arangodb::aql::AstNode ALL(arangodb::aql::AstNodeValue(true));

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple irs::index_reader
///        the container storing the view state for a given TransactionState
/// @note it is assumed that DBServer ViewState resides in the same
///       TransactionState as the IResearchView ViewState, therefore a separate
///       lock is not required to be held by the DBServer CompoundReader
////////////////////////////////////////////////////////////////////////////////
class ViewTrxState final : public arangodb::TransactionState::Cookie,
                           public arangodb::iresearch::IResearchView::Snapshot {
 public:
  irs::sub_reader const& operator[](size_t subReaderId) const noexcept override {
    TRI_ASSERT(subReaderId < _subReaders.size());
    return *(_subReaders[subReaderId].second);
  }

  void add(TRI_voc_cid_t cid, arangodb::iresearch::IResearchLink::Snapshot&& snapshot);

  TRI_voc_cid_t cid(size_t offset) const noexcept override {
    return offset < _subReaders.size() ? _subReaders[offset].first : 0;
  }

  void clear() noexcept {
    _collections.clear();
    _subReaders.clear();
    _snapshots.clear();
    _live_docs_count = 0;
    _docs_count = 0;
  }

  template <typename Itr>
  bool equalCollections(Itr begin, Itr end) {
    size_t count = 0;

    for (; begin != end; ++count, ++begin) {
      if (_collections.find(*begin) == _collections.end() ||
          count > _collections.size()) {
        return false;
      }
    }

    return _collections.size() == count;
  }

  virtual uint64_t docs_count() const noexcept override { return _docs_count; }
  virtual uint64_t live_docs_count() const noexcept override { return _live_docs_count; }
  virtual size_t size() const noexcept override { return _subReaders.size(); }

 private:
  size_t _docs_count{};
  size_t _live_docs_count{};
  std::unordered_set<TRI_voc_cid_t> _collections;
  std::vector<arangodb::iresearch::IResearchLink::Snapshot> _snapshots;  // prevent data-store deallocation (lock @ AsyncSelf)
  std::vector<std::pair<TRI_voc_cid_t, irs::sub_reader const*>> _subReaders;
};

void ViewTrxState::add(TRI_voc_cid_t cid,
                       arangodb::iresearch::IResearchLink::Snapshot&& snapshot) {
  auto& reader = static_cast<irs::index_reader const&>(snapshot);
  for (auto& entry : reader) {
    _subReaders.emplace_back(std::piecewise_construct, std::forward_as_tuple(cid),
                             std::forward_as_tuple(&entry));
  }

  _docs_count += reader.docs_count();
  _live_docs_count += reader.live_docs_count();
  _collections.emplace(cid);
  _snapshots.emplace_back(std::move(snapshot));
}

}

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchView-specific implementation of a ViewFactory
////////////////////////////////////////////////////////////////////////////////
struct IResearchView::ViewFactory : public arangodb::ViewFactory {
  virtual arangodb::Result create(arangodb::LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                                  arangodb::velocypack::Slice const& definition) const override {
    auto* engine = arangodb::EngineSelectorFeature::ENGINE;
    auto& properties = definition.isObject()
                           ? definition
                           : arangodb::velocypack::Slice::emptyObjectSlice();  // if no 'info' then assume defaults
    auto links = properties.hasKey(StaticStrings::LinksField)
                     ? properties.get(StaticStrings::LinksField)
                     : arangodb::velocypack::Slice::emptyObjectSlice();
    auto res = engine && engine->inRecovery()
                   ? arangodb::Result()  // do not validate if in recovery
                   : IResearchLinkHelper::validateLinks(vocbase, links);

    if (!res.ok()) {
      return res;
    }

    arangodb::LogicalView::ptr impl;

    res = arangodb::ServerState::instance()->isSingleServer()
              ? arangodb::LogicalViewHelperStorageEngine::construct(impl, vocbase, definition)
              : arangodb::LogicalViewHelperClusterInfo::construct(impl, vocbase, definition);

    if (!res.ok()) {
      return res;
    }

    if (!impl) {
      return arangodb::Result(TRI_ERROR_INTERNAL,
                              std::string(
                                  "failure during instantiation while creating "
                                  "arangosearch View in database '") +
                                  vocbase.name() + "'");
    }

    // create links on a best-effor basis
    // link creation failure does not cause view creation failure
    try {
      std::unordered_set<TRI_voc_cid_t> collections;

      res = IResearchLinkHelper::updateLinks(collections, *impl, links);

      if (!res.ok()) {
        LOG_TOPIC("d683b", WARN, arangodb::iresearch::TOPIC)
          << "failed to create links while creating arangosearch view '" << impl->name() <<  "': " << res.errorNumber() << " " <<  res.errorMessage();
      }
    } catch (arangodb::basics::Exception const& e) {
      IR_LOG_EXCEPTION();
      LOG_TOPIC("eddb2", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while creating links while creating arangosearch view '" << impl->name() << "': " << e.code() << " " << e.what();
    } catch (std::exception const& e) {
      IR_LOG_EXCEPTION();
      LOG_TOPIC("dc829", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while creating links while creating arangosearch view '" << impl->name() << "': " << e.what();
    } catch (...) {
      IR_LOG_EXCEPTION();
      LOG_TOPIC("6491c", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while creating links while creating arangosearch view '" << impl->name() << "'";
    }

    view = impl;

    return arangodb::Result();
  }

  virtual arangodb::Result instantiate(arangodb::LogicalView::ptr& view,
                                       TRI_vocbase_t& vocbase,
                                       arangodb::velocypack::Slice const& definition,
                                       uint64_t planVersion) const override {
    auto* databaseFeature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
            "Database");
    std::string error;
    bool inUpgrade = databaseFeature ? databaseFeature->upgrade() : false; // check if DB is currently being upgraded (skip validation checks)
    IResearchViewMeta meta;
    IResearchViewMetaState metaState;

    if (!meta.init(definition, error) // parse definition
        || (meta._version == 0 && !inUpgrade) // version 0 must be upgraded to split data-store on a per-link basis
        || meta._version > LATEST_VERSION // ensure version is valid
        || (ServerState::instance()->isSingleServer() // init metaState for SingleServer
            && !metaState.init(definition, error))) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        error.empty()
        ? (std::string("failed to initialize arangosearch View from definition: ") + definition.toString())
        : (std::string("failed to initialize arangosearch View from definition, error in attribute '") + error + "': " + definition.toString())
      );
    }

    auto impl = std::shared_ptr<IResearchView>(
        new IResearchView(vocbase, definition, planVersion, std::move(meta)));

    // NOTE: for single-server must have full list of collections to lock
    //       for cluster the shards to lock come from coordinator and are not in
    //       the definition
    for (auto cid : metaState._collections) {
      auto collection = vocbase.lookupCollection(cid);  // always look up in vocbase (single server or cluster
                                                        // per-shard collection)
      auto link = collection ? IResearchLinkHelper::find(*collection, *impl)
                             : nullptr;  // add placeholders to links, when the
                                         // collection comes up it'll bring up the link

      impl->_links.emplace(cid, link ? link->self()
                                     : nullptr);  // add placeholders to links, when the link
                                                  // comes up it'll call link(...)
    }

    view = impl;

    return arangodb::Result();
  }
};

IResearchView::IResearchView(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& info,
                             uint64_t planVersion, IResearchViewMeta&& meta)
    : LogicalView(vocbase, info, planVersion),
      _asyncSelf(irs::memory::make_unique<AsyncViewPtr::element_type>(this)),
      _meta(std::move(meta)),
      _inRecovery(false) {
  // set up in-recovery insertion hooks
  auto* databaseFeature = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::DatabaseFeature // type
  >("Database");

  if (databaseFeature) {
    auto view = _asyncSelf; // create copy for lambda

    databaseFeature->registerPostRecoveryCallback([view]()->arangodb::Result {
      auto& viewMutex = view->mutex();
      SCOPED_LOCK(viewMutex); // ensure view does not get deallocated before call back finishes
      auto* viewPtr = view->get();

      if (viewPtr) {
        viewPtr->verifyKnownCollections();
      }

      return arangodb::Result();
    });
  }

  auto self = _asyncSelf;

  // initialize transaction read callback
  _trxCallback = [self]( // callback
      arangodb::transaction::Methods& trx, // transaction
      arangodb::transaction::Status status // transaction status
  )->void {
    if (arangodb::transaction::Status::RUNNING != status) {
      return; // NOOP
    }

    SCOPED_LOCK(self->mutex());
    auto* view = self->get();

    // populate snapshot when view is registred with a transaction on single-server
    if (view && arangodb::ServerState::instance()->isSingleServer()) {
      view->snapshot(trx, IResearchView::SnapshotMode::FindOrCreate);
    }
  };
}

IResearchView::~IResearchView() {
  _asyncSelf->reset(); // the view is being deallocated, its use is no longer valid (wait for all the view users to finish)

  if (arangodb::ServerState::instance()->isSingleServer()) {
    arangodb::LogicalViewHelperStorageEngine::destruct(*this); // cleanup of the storage engine
  }
}

arangodb::Result IResearchView::appendVelocyPackImpl( // append JSON
    arangodb::velocypack::Builder& builder, // destrination
    bool detailed, // detail flag
    bool forPersistence // persistence flag
) const {
  if (forPersistence && arangodb::ServerState::instance()->isSingleServer()) {
    auto res = arangodb::LogicalViewHelperStorageEngine::properties( // storage engine properties
      builder, *this // args
    );

    if (!res.ok()) {
      return res;
    }
  }

  if (!detailed) {
    return arangodb::Result();  // nothing more to output
  }

  if (!builder.isOpenObject()) {
    return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
  }

  std::vector<std::string> collections;

  {
    static const std::function<bool(irs::string_ref const& key)> acceptor =
        [](irs::string_ref const& key) -> bool {
      return key != StaticStrings::VersionField;  // ignored fields
    };
    static const std::function<bool(irs::string_ref const& key)> persistenceAcceptor =
        [](irs::string_ref const&) -> bool { return true; };

    ReadMutex mutex(_mutex);  // '_meta'/'_links' can be asynchronously modified
    SCOPED_LOCK(mutex);
    arangodb::velocypack::Builder sanitizedBuilder;

    sanitizedBuilder.openObject();

    if (!_meta.json(sanitizedBuilder) ||
        !mergeSliceSkipKeys(builder, sanitizedBuilder.close().slice(),
                            forPersistence ? persistenceAcceptor : acceptor)) {
      return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("failure to generate definition while generating "
                      "properties jSON for arangosearch View in database '") +
              vocbase().name() + "'");
    }

    if (forPersistence) {
      IResearchViewMetaState metaState;

      for (auto& entry : _links) {
        metaState._collections.emplace(entry.first);
      }

      metaState.json(builder);

      return arangodb::Result();  // nothing more to output (persistent
                                  // configuration does not need links)
    }

    // add CIDs of known collections to list
    for (auto& entry : _links) {
      // skip collections missing from vocbase or UserTransaction constructor
      // will throw an exception
      if (vocbase().lookupCollection(entry.first)) {
        collections.emplace_back(std::to_string(entry.first));
      }
    }
  }

  // open up a read transaction and add all linked collections to verify that
  // the current user has access

  arangodb::velocypack::Builder linksBuilder;
  static std::vector<std::string> const EMPTY;

  // use default lock timeout
  arangodb::transaction::Options options;

  options.waitForSync = false;
  options.allowImplicitCollections = false;

  try {
    arangodb::transaction::Methods trx(transaction::StandaloneContext::Create(vocbase()),
                                       collections,  // readCollections
                                       EMPTY,        // writeCollections
                                       EMPTY,        // exclusiveCollections
                                       options);
    auto res = trx.begin();

    if (!res.ok()) {
      return res; // nothing more to output
    }

    auto* state = trx.state();

    if (!state) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to get transaction state while generating json for arangosearch view '") + name() + "'"
      );
    }

    auto visitor = [this, &linksBuilder, &res]( // visit collections
      arangodb::TransactionCollection& trxCollection // transaction collection
    )->bool {
      auto collection = trxCollection.collection();

      if (!collection) {
        return true; // skip missing collections
      }

      auto link = IResearchLinkHelper::find(*collection, *this);

      if (!link) {
        return true; // no links for the current view
      }

      arangodb::velocypack::Builder linkBuilder;

      linkBuilder.openObject();

      if (!link->properties(linkBuilder, false).ok()) { // link definitions are not output if forPersistence
        LOG_TOPIC("713ad", WARN, arangodb::iresearch::TOPIC)
          << "failed to generate json for arangosearch link '" << link->id() << "' while generating json for arangosearch view '" << name() << "'";

        return true; // skip invalid link definitions
      }

      linkBuilder.close();

      static const auto acceptor = [](irs::string_ref const& key)->bool {
        return key != arangodb::StaticStrings::IndexId
            && key != arangodb::StaticStrings::IndexType
            && key != StaticStrings::ViewIdField; // ignored fields
      };

      linksBuilder.add(
        collection->name(),
        arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object)
      );

      if (!mergeSliceSkipKeys(linksBuilder, linkBuilder.slice(), acceptor)) {
        res = arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("failed to generate arangosearch link '") + std::to_string(link->id()) + "' definition while generating json for arangosearch view '" + name() + "'"
        );

        return false; // terminate generation
      }

      linksBuilder.close();

      return true; // done with this collection
    };

    linksBuilder.openObject();
      state->allCollections(visitor);
    linksBuilder.close();

    if (!res.ok()) {
      return res;
    }

    trx.commit();
  } catch (arangodb::basics::Exception& e) {
    IR_LOG_EXCEPTION();

    return arangodb::Result(
        e.code(),
        std::string(
            "caught exception while generating json for arangosearch view '") +
            name() + "': " + e.what());
  } catch (std::exception const& e) {
    IR_LOG_EXCEPTION();

    return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string(
            "caught exception while generating json for arangosearch view '") +
            name() + "': " + e.what());
  } catch (...) {
    IR_LOG_EXCEPTION();

    return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string(
            "caught exception while generating json for arangosearch view '") +
            name() + "'");
  }

  builder.add(StaticStrings::LinksField, linksBuilder.slice());

  return arangodb::Result();
}

bool IResearchView::apply(arangodb::transaction::Methods& trx) {
  // called from IResearchView when this view is added to a transaction
  return trx.addStatusChangeCallback(&_trxCallback);  // add shapshot
}

arangodb::Result IResearchView::dropImpl() {
  std::unordered_set<TRI_voc_cid_t> collections;
  std::unordered_set<TRI_voc_cid_t> stale;

  // drop all known links
  {
    ReadMutex mutex(_mutex);  // '_metaState' can be asynchronously updated
    SCOPED_LOCK(mutex);

    for (auto& entry : _links) {
      stale.emplace(entry.first);
    }
  }

  if (!stale.empty()) {
    // check link auth as per https://github.com/arangodb/backlog/issues/459
    if (arangodb::ExecContext::CURRENT) {
      for (auto& entry : stale) {
        auto collection = vocbase().lookupCollection(entry);

        if (collection &&
            !arangodb::ExecContext::CURRENT->canUseCollection(
                vocbase().name(), collection->name(), arangodb::auth::Level::RO)) {
          return arangodb::Result(TRI_ERROR_FORBIDDEN);
        }
      }
    }

    arangodb::Result res;

    {
      if (!_updateLinksLock.try_lock()) {
        // FIXME use specific error code
        return arangodb::Result( // result
          TRI_ERROR_FAILED, //code
          std::string("failed to remove arangosearch view '") + name() // message
        );
      }

      ADOPT_SCOPED_LOCK_NAMED(_updateLinksLock, lock);

      res = IResearchLinkHelper::updateLinks( // update links
        collections, // modified collection ids
        *this, // modified view
        arangodb::velocypack::Slice::emptyObjectSlice(), // link definitions to apply
        stale // stale links
      );
    }

    if (!res.ok()) {
      return arangodb::Result( // result
        res.errorNumber(), // code
        std::string("failed to remove links while removing arangosearch view '") + name() + "': " + res.errorMessage()
      );
    }
  }

  _asyncSelf->reset(); // the view data-stores are being deallocated, view use is no longer valid (wait for all the view users to finish)

  WriteMutex mutex(_mutex); // members can be asynchronously updated
  SCOPED_LOCK(mutex);

  for (auto& entry : _links) {
    collections.emplace(entry.first);
  }

  auto collectionsCount = collections.size();

  for (auto& entry : collections) {
    auto collection = vocbase().lookupCollection(entry);

    if (!collection || !IResearchLinkHelper::find(*collection, *this)) {
      --collectionsCount;
    }
  }

  // ArangoDB global consistency check, no known dangling links
  if (collectionsCount) {
    return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("links still present while removing arangosearch view '") +
            std::to_string(id()) + "'");
  }

  return arangodb::ServerState::instance()->isSingleServer()
             ? arangodb::LogicalViewHelperStorageEngine::drop(
                   *this)  // single-server additionaly requires removal from
                           // the StorageEngine
             : arangodb::Result();
}

/*static*/ arangodb::ViewFactory const& IResearchView::factory() {
  static const ViewFactory factory;

  return factory;
}

bool IResearchView::link(AsyncLinkPtr const& link) {
  if (!link) {
    return false; // invalid link
  }

  SCOPED_LOCK(link->mutex()); // prevent the link from being deallocated

  if (!link->get()) {
    return false; // invalid link
  }

  auto cid = link->get()->collection().id();
  WriteMutex mutex(_mutex); // '_meta'/'_links' can be asynchronously read
  SCOPED_LOCK(mutex);
  auto itr = _links.find(cid);

  if (itr == _links.end()) {
    _links.emplace(cid, link);
  } else if (arangodb::ServerState::instance()->isSingleServer() // single server
             && !itr->second) {
    _links[cid] = link;
    link->get()->properties(_meta);

    return true; // single-server persisted cid placeholder substituted with actual link
  } else if (itr->second && !itr->second->get()) {
    _links[cid] = link;
    link->get()->properties(_meta);

    return true; // a previous link instance was unload()ed and a new instance is linking
  } else {
    return false; // link already present
  }

  auto res = arangodb::ServerState::instance()->isSingleServer()
    ? arangodb::LogicalViewHelperStorageEngine::properties(*this)
    : arangodb::Result()
    ;

  if (!res.ok()) {
    _links.erase(cid); // undo meta modification
    LOG_TOPIC("faae3", WARN, arangodb::iresearch::TOPIC)
      << "failed to persist logical view while emplacing collection '" << cid << "' into arangosearch View '" << name() << "': " << res.errorMessage();

    return false;
  }

  link->get()->properties(_meta);

  return true;
}

arangodb::Result IResearchView::commit() {
  ReadMutex mutex(_mutex);  // '_links' can be asynchronously updated
  SCOPED_LOCK(mutex);

  for (auto& entry : _links) {
    auto cid = entry.first;

    if (!entry.second) {
      return arangodb::Result(
          TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
          std::string("failed to find an arangosearch link in collection '") +
              std::to_string(cid) + "' while syncing arangosearch view '" +
              name() + "'");
    }

    SCOPED_LOCK(entry.second->mutex());  // ensure link is not deallocated for
                                         // the duration of the operation
    auto* link = entry.second->get();

    if (!link) {
      return arangodb::Result(
          TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
          std::string(
              "failed to find a loaded arangosearch link in collection '") +
              std::to_string(cid) + "' while syncing arangosearch view '" +
              name() + "'");
    }

    auto res = link->commit();

    if (!res.ok()) {
      return res;
    }
  }

  return arangodb::Result();
}

size_t IResearchView::memory() const {
  size_t size = sizeof(IResearchView);
  ReadMutex mutex(_mutex);  // '_meta'/'_links' can be asynchronously updated
  SCOPED_LOCK(mutex);

  size += _meta.memory() - sizeof(IResearchViewMeta);  // sizeof(IResearchViewMeta) already part
                                                       // of sizeof(IResearchView)
  size += sizeof(decltype(_links)::value_type) * _links.size();

  for (auto& entry : _links) {
    if (!entry.second) {
      continue;  // skip link placeholders
    }

    SCOPED_LOCK(entry.second->mutex());  // ensure link is not deallocated for
                                         // the duration of the operation
    auto* link = entry.second->get();

    if (!link) {
      continue;  // skip missing links
    }

    size += link->memory();
  }

  return size;
}

void IResearchView::open() {
  auto* engine = arangodb::EngineSelectorFeature::ENGINE;

  if (engine) {
    _inRecovery = engine->inRecovery();
  } else {
    LOG_TOPIC("8b864", WARN, arangodb::iresearch::TOPIC)
      << "failure to get storage engine while opening arangosearch view: " << name();
    // assume not inRecovery()
  }
}

arangodb::Result IResearchView::properties( // update properties
  arangodb::velocypack::Slice const& properties, // properties definition
  bool partialUpdate // delta or full update flag
) {
  auto res = updateProperties(properties, partialUpdate);

  if (!res.ok()) {
    return res;
  }

#if USE_PLAN_CACHE
  arangodb::aql::PlanCache::instance()->invalidate(&vocbase());
#endif
  arangodb::aql::QueryCache::instance()->invalidate(&vocbase());

  return arangodb::ServerState::instance()->isSingleServer()
             ? arangodb::LogicalViewHelperStorageEngine::properties(*this)
             : arangodb::LogicalViewHelperClusterInfo::properties(*this);
}

arangodb::Result IResearchView::renameImpl(std::string const& oldName) {
  return arangodb::ServerState::instance()->isSingleServer()
             ? arangodb::LogicalViewHelperStorageEngine::rename(*this, oldName)
             : arangodb::LogicalViewHelperClusterInfo::rename(*this, oldName);
}

IResearchView::Snapshot const* IResearchView::snapshot(
    transaction::Methods& trx,
    IResearchView::SnapshotMode mode /*= IResearchView::SnapshotMode::Find*/,
    arangodb::HashSet<TRI_voc_cid_t> const* shards /*= nullptr*/,
    void const* key /*= nullptr*/) const {
  if (!trx.state()) {
    LOG_TOPIC("47098", WARN, arangodb::iresearch::TOPIC)
        << "failed to get transaction state while creating arangosearch view "
           "snapshot";

    return nullptr;
  }

  arangodb::HashSet<TRI_voc_cid_t> restrictedCollections;  // use set to avoid duplicate iteration of same link
  auto const* collections = &restrictedCollections;

  if (shards) {  // set requested shards
    collections = shards;
  } else {  // add all known shards
    for (auto& entry : _links) {
      restrictedCollections.emplace(entry.first);
    }
  }

  if (!key) {
    key = this;
  }

  auto& state = *(trx.state());

// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* ctx = dynamic_cast<ViewTrxState*>(state.cookie(key));
#else
  auto* ctx = static_cast<ViewTrxState*>(state.cookie(key));
#endif

  switch (mode) {
    case SnapshotMode::Find:
      return ctx && ctx->equalCollections(collections->begin(),
                                          collections->end())
                 ? ctx
                 : nullptr;  // ensure same collections
    case SnapshotMode::FindOrCreate:
      if (ctx) {
        if (ctx->equalCollections(collections->begin(), collections->end())) {
          return ctx;  // ensure same collections
        }

        ctx->clear(); // reassemble snapshot
      }
      break;
    case SnapshotMode::SyncAndReplace: {
      if (ctx) {
        ctx->clear();  // ignore existing cookie, recreate snapshot
      }

      auto res = const_cast<IResearchView*>(this)->commit();

      if (!res.ok()) {
        LOG_TOPIC("fd776", WARN, arangodb::iresearch::TOPIC)
            << "failed to sync while creating snapshot for arangosearch view '"
            << name() << "', previous snapshot will be used instead, error: '"
            << res.errorMessage() << "'";
      }

      break;
    }
    default:
      TRI_ASSERT(false);  // all values of the enum should be covered
  }

  if (!ctx) {
    auto ptr = irs::memory::make_unique<ViewTrxState>();

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx) {
      LOG_TOPIC("61271", WARN, arangodb::iresearch::TOPIC)
          << "failed to store state into a TransactionState for snapshot of "
             "arangosearch view '"
          << name() << "', tid '" << state.id() << "'";

      return nullptr;
    }
  }

  ReadMutex mutex(_mutex);  // '_metaState' can be asynchronously modified
  SCOPED_LOCK(mutex);

  try {
    // collect snapshots from all requested links
    for (auto const cid : *collections) {
      auto itr = _links.find(cid);
      auto* link = itr != _links.end() && itr->second
                       ? itr->second->get()
                       : nullptr;  // do not need to lock link since collection
                                   // is part of the transaction

      if (!link) {
        LOG_TOPIC("d63ff", ERR, arangodb::iresearch::TOPIC)
            << "failed to find an arangosearch link in collection '" << cid
            << "' for arangosearch view '" << name() << "', skipping it";
        state.cookie(key, nullptr);  // unset cookie

        return nullptr;  // skip missing links
      }

      auto snapshot = link->snapshot();

      if (!static_cast<irs::directory_reader const&>(snapshot)) {
        LOG_TOPIC("e76eb", ERR, arangodb::iresearch::TOPIC)
            << "failed to get snaphot of arangosearch link in collection '"
            << cid << "' for arangosearch view '" << name() << "', skipping it";
        state.cookie(key, nullptr);  // unset cookie

        return nullptr;  // skip failed readers
      }

      ctx->add(cid, std::move(snapshot));
    }
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC("29b30", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while collecting readers for snapshot of "
           "arangosearch view '"
        << name() << "', tid '" << state.id() << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();

    return nullptr;
  } catch (std::exception const& e) {
    LOG_TOPIC("ffe73", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while collecting readers for snapshot of "
           "arangosearch view '"
        << name() << "', tid '" << state.id() << "': " << e.what();
    IR_LOG_EXCEPTION();

    return nullptr;
  } catch (...) {
    LOG_TOPIC("c54e8", WARN, arangodb::iresearch::TOPIC)
        << "caught exception while collecting readers for snapshot of "
           "arangosearch view '"
        << name() << "', tid '" << state.id() << "'";
    IR_LOG_EXCEPTION();

    return nullptr;
  }

  return ctx;
}

arangodb::Result IResearchView::unlink(TRI_voc_cid_t cid) noexcept {
  try {
    WriteMutex mutex(_mutex);  // '_links' can be asynchronously read
    SCOPED_LOCK(mutex);
    auto itr = _links.find(cid);

    if (itr == _links.end()) {
      return arangodb::Result();  // already unlinked
    }

    auto links = _links;

    _links.erase(itr);

    auto res = arangodb::ServerState::instance()->isSingleServer()
                   ? arangodb::LogicalViewHelperStorageEngine::properties(*this)
                   : arangodb::Result();

    if (!res.ok()) {
      _links.swap(links);  // restore original collections
      LOG_TOPIC("9d678", WARN, arangodb::iresearch::TOPIC)
          << "failed to persist logical view while unlinking collection '" << cid
          << "' from arangosearch view '" << name() << "': " << res.errorMessage();

      return res;
    }
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result(
        e.code(), std::string("caught exception while unlinking collection '") +
                      std::to_string(cid) + "' from arangosearch view '" +
                      name() + "': " + e.what());
  } catch (std::exception const& e) {
    return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception while unlinking collection '") + std::to_string(cid) +
            "' from arangosearch view '" + name() + "': " + e.what());
  } catch (...) {
    return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception while unlinking collection '") +
            std::to_string(cid) + "' from arangosearch view '" + name() + "'");
  }

  return arangodb::Result();
}

arangodb::Result IResearchView::updateProperties(arangodb::velocypack::Slice const& slice,
                                                 bool partialUpdate) {
  try {
    auto links = slice.hasKey(StaticStrings::LinksField)
                     ? slice.get(StaticStrings::LinksField)
                     : arangodb::velocypack::Slice::emptyObjectSlice();
    auto res = _inRecovery ? arangodb::Result()  // do not validate if in recovery
                           : IResearchLinkHelper::validateLinks(vocbase(), links);

    if (!res.ok()) {
      return res;
    }

    WriteMutex mutex(_mutex);  // '_meta'/'_metaState' can be asynchronously read
    SCOPED_LOCK_NAMED(mutex, mtx);

    // check link auth as per https://github.com/arangodb/backlog/issues/459
    if (arangodb::ExecContext::CURRENT) {
      // check existing links
      for (auto& entry : _links) {
        auto collection = vocbase().lookupCollection(entry.first);

        if (collection &&
            !arangodb::ExecContext::CURRENT->canUseCollection(
                vocbase().name(), collection->name(), arangodb::auth::Level::RO)) {
          return arangodb::Result(
              TRI_ERROR_FORBIDDEN,
              std::string("while updating arangosearch definition, error: "
                          "collection '") +
                  collection->name() + "' not authorised for read access");
        }
      }
    }

    std::string error;
    IResearchViewMeta meta;
    auto& initialMeta = partialUpdate ? _meta : IResearchViewMeta::DEFAULT();

    if (!meta.init(slice, error, initialMeta)) {
      return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          error.empty() ? (std::string("failed to update arangosearch view '") +
                           name() + "' from definition: " + slice.toString())
                        : (std::string("failed to update arangosearch view '") +
                           name() + "' from definition, error in attribute '" +
                           error + "': " + slice.toString()));
    }

    // reset non-updatable values to match current meta
    meta._locale = _meta._locale;
    meta._version = _meta._version;
    meta._writebufferActive = _meta._writebufferActive;
    meta._writebufferIdle = _meta._writebufferIdle;
    meta._writebufferSizeMax = _meta._writebufferSizeMax;

    _meta = std::move(meta);

    mutex.unlock(true); // downgrade to a read-lock

    // update properties of links
    for (auto& entry: _links) {
      auto& link = entry.second;
      SCOPED_LOCK(link->mutex()); // prevent the link from being deallocated

      if (link->get()) {
        auto result = link->get()->properties(_meta);

        if (!result.ok()) {
          res = result;
        }
      }
    }

    if (links.isEmptyObject() && (partialUpdate || _inRecovery.load())) { // ignore missing links coming from WAL (inRecovery)
      return res;
    }

    // ...........................................................................
    // update links if requested (on a best-effort basis)
    // indexing of collections is done in different threads so no locks can be held and rollback is not possible
    // as a result it's also possible for links to be simultaneously modified via a different callflow (e.g. from collections)
    // ...........................................................................

    std::unordered_set<TRI_voc_cid_t> collections;

    if (partialUpdate) {
      mtx.unlock(); // release lock

      SCOPED_LOCK(_updateLinksLock);

      return IResearchLinkHelper::updateLinks(collections, *this, links);
    }

    std::unordered_set<TRI_voc_cid_t> stale;

    for (auto& entry: _links) {
      stale.emplace(entry.first);
    }

    mtx.unlock(); // release lock

    SCOPED_LOCK(_updateLinksLock);

    return IResearchLinkHelper::updateLinks(collections, *this, links, stale);
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC("74705", WARN, iresearch::TOPIC)
      << "caught exception while updating properties for arangosearch view '" << name() << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      e.code(),
      std::string("error updating properties for arangosearch view '") + name() + "'"
    );
  } catch (std::exception const& e) {
    LOG_TOPIC("27f54", WARN, iresearch::TOPIC)
      << "caught exception while updating properties for arangosearch view '" << name() << "': " << e.what();
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating properties for arangosearch view '") + name() + "'"
    );
  } catch (...) {
    LOG_TOPIC("99bbe", WARN, iresearch::TOPIC)
      << "caught exception while updating properties for arangosearch view '" << name() << "'";
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating properties for arangosearch view '") + name() + "'"
    );
  }
}

bool IResearchView::visitCollections( // visit collections
    LogicalView::CollectionVisitor const& visitor // visitor to call
) const {
  ReadMutex mutex(_mutex); // '_links' can be asynchronously modified
  SCOPED_LOCK(mutex);

  for (auto& entry: _links) {
    if (!visitor(entry.first)) {
      return false;
    }
  }

  return true;
}

void IResearchView::verifyKnownCollections() {
  bool modified = false;
  WriteMutex mutex(_mutex);  // '_links' can be asynchronously read
  SCOPED_LOCK(mutex);

  // verify existence of all known links
  for (auto itr = _links.begin(); itr != _links.end();) {
    auto cid = itr->first;
    auto collection = vocbase().lookupCollection(
        cid);  // always look up in vocbase (single server or cluster per-shard
               // collection)

    if (!collection) {
      LOG_TOPIC("40976", TRACE, arangodb::iresearch::TOPIC)
          << "collection '" << cid
          << "' no longer exists! removing from arangosearch view '" << name() << "'";
      itr = _links.erase(itr);
      modified = true;

      continue;
    }

    auto link = IResearchLinkHelper::find(*collection, *this);

    if (!link) {
      LOG_TOPIC("d0509", TRACE, arangodb::iresearch::TOPIC)
          << "collection '" << collection->name()
          << "' no longer linked! removing from arangosearch view '" << name() << "'";
      itr = _links.erase(itr);
      modified = true;

      continue;
    }

    TRI_ASSERT(itr->second);  // all links must be valid even on single-server
    ++itr;
  }

  if (modified && arangodb::ServerState::instance()->isSingleServer()) {
    arangodb::LogicalViewHelperStorageEngine::properties(*this);
  }
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------