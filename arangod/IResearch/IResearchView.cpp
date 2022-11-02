////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstNode.h"
#include "Aql/PlanCache.h"
#include "Aql/QueryCache.h"
#include "Basics/DownCast.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Containers/FlatHashSet.h"
#include "IResearch/ViewSnapshot.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "RestServer/ViewTypesFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"

namespace arangodb::iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchView-specific implementation of a ViewFactory
////////////////////////////////////////////////////////////////////////////////
struct IResearchView::ViewFactory final : public arangodb::ViewFactory {
  Result create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                VPackSlice definition, bool isUserRequest) const final {
    auto& engine =
        vocbase.server().getFeature<EngineSelectorFeature>().engine();
    auto properties = definition.isObject()
                          ? definition
                          : velocypack::Slice::emptyObjectSlice();
    // if no 'info' then assume defaults
    auto links = properties.hasKey(StaticStrings::LinksField)
                     ? properties.get(StaticStrings::LinksField)
                     : velocypack::Slice::emptyObjectSlice();
    auto r = engine.inRecovery()
                 ? Result{}  // do not validate if in recovery
                 : IResearchLinkHelper::validateLinks(vocbase, links);

    if (!r.ok()) {
      std::string name;
      if (definition.isObject()) {
        name = basics::VelocyPackHelper::getStringValue(
            definition, arangodb::StaticStrings::DataSourceName, "");
      }
      events::CreateView(vocbase.name(), name, r.errorNumber());
      return r;
    }

    LogicalView::ptr impl;

    r = ServerState::instance()->isSingleServer()
            ? storage_helper::construct(impl, vocbase, definition,
                                        isUserRequest)
            : cluster_helper::construct(impl, vocbase, definition,
                                        isUserRequest);
    if (!r.ok()) {
      std::string name;
      if (definition.isObject()) {
        name = basics::VelocyPackHelper::getStringValue(
            definition, arangodb::StaticStrings::DataSourceName, "");
      }
      events::CreateView(vocbase.name(), name, r.errorNumber());
      return r;
    }

    if (!impl) {
      std::string name;
      if (definition.isObject()) {
        name = basics::VelocyPackHelper::getStringValue(
            definition, arangodb::StaticStrings::DataSourceName, "");
      }
      events::CreateView(vocbase.name(), name, TRI_ERROR_INTERNAL);
      return {TRI_ERROR_INTERNAL,
              "failure during instantiation while creating arangosearch View "
              "in database '" +
                  vocbase.name() + "'"};
    }

    // create links on a best-effort basis
    // link creation failure does not cause view creation failure
    try {
      // only we have access to *impl, so don't need lock
      std::unordered_set<DataSourceId> collections;  // TODO remove
      r = IResearchLinkHelper::updateLinks(collections, *impl, links,
                                           getDefaultVersion(isUserRequest));
      if (!r.ok()) {
        LOG_TOPIC("d683b", WARN, TOPIC)
            << "failed to create links while creating arangosearch view '"
            << impl->name() << "': " << r.errorNumber() << " "
            << r.errorMessage();
      }
    } catch (basics::Exception const& e) {
      std::string name;
      if (definition.isObject()) {
        name = basics::VelocyPackHelper::getStringValue(
            definition, arangodb::StaticStrings::DataSourceName, "");
      }
      events::CreateView(vocbase.name(), name, e.code());
      LOG_TOPIC("eddb2", WARN, TOPIC)
          << "caught exception while creating links while creating "
             "arangosearch view '"
          << impl->name() << "': " << e.code() << " " << e.what();
    } catch (std::exception const& e) {
      std::string name;
      if (definition.isObject()) {
        name = basics::VelocyPackHelper::getStringValue(
            definition, arangodb::StaticStrings::DataSourceName, "");
      }
      events::CreateView(vocbase.name(), name, TRI_ERROR_INTERNAL);
      LOG_TOPIC("dc829", WARN, TOPIC)
          << "caught exception while creating links while creating "
             "arangosearch view '"
          << impl->name() << "': " << e.what();
    } catch (...) {
      std::string name;
      if (definition.isObject()) {
        name = basics::VelocyPackHelper::getStringValue(
            definition, arangodb::StaticStrings::DataSourceName, "");
      }
      events::CreateView(vocbase.name(), name, TRI_ERROR_INTERNAL);
      LOG_TOPIC("6491c", WARN, TOPIC)
          << "caught exception while creating links while creating "
             "arangosearch view '"
          << impl->name() << "'";
    }
    view = impl;
    return {};
  }

  Result instantiate(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                     VPackSlice definition,
                     bool /*isUserRequest*/) const final {
    std::string error;
    IResearchViewMeta meta;
    IResearchViewMetaState metaState;

    // parse definition
    if (!meta.init(definition, error)
        // ensure version is valid
        || meta._version > static_cast<uint32_t>(ViewVersion::MAX)
        // init metaState for SingleServer
        || (ServerState::instance()->isSingleServer() &&
            !metaState.init(definition, error))) {
      return {TRI_ERROR_BAD_PARAMETER,
              error.empty()
                  ? (std::string("failed to initialize arangosearch View from "
                                 "definition: ") +
                     definition.toString())
                  : (std::string("failed to initialize arangosearch View from "
                                 "definition, error in attribute '") +
                     error + "': " + definition.toString())};
    }

    auto impl = std::shared_ptr<IResearchView>(
        new IResearchView(vocbase, definition, std::move(meta)));

    // NOTE: for single-server must have full list of collections to lock
    //       for cluster the shards to lock come from coordinator and are not in
    //       the definition
    for (auto cid : metaState._collections) {
      // always look up in vocbase
      // (single server or cluster per-shard collection)
      auto collection = vocbase.lookupCollection(cid);
      // add placeholders to links,
      // when the collection comes up it'll bring up the link
      auto link =
          collection ? IResearchLinkHelper::find(*collection, *impl) : nullptr;
      // add placeholders to links, when the link comes up it'll call link(...)
      impl->_links.try_emplace(cid, link ? link->self() : nullptr);
    }
    view = impl;
    return {};
  }
};

IResearchView::IResearchView(TRI_vocbase_t& vocbase,
                             velocypack::Slice const& info,
                             IResearchViewMeta&& meta)
    : LogicalView(*this, vocbase, info),
      _asyncSelf(std::make_shared<AsyncViewPtr::element_type>(this)),
      _meta(IResearchViewMeta::FullTag{}, std::move(meta)),
      _inRecovery(false) {
  // set up in-recovery insertion hooks
  if (vocbase.server().hasFeature<DatabaseFeature>()) {
    auto& databaseFeature = vocbase.server().getFeature<DatabaseFeature>();
    databaseFeature.registerPostRecoveryCallback([view = _asyncSelf] {
      // ensure view does not get deallocated before call back finishes
      auto viewLock = view->lock();
      if (viewLock) {
        viewLock->verifyKnownCollections();
      }
      return Result{};
    });
  }
  // initialize transaction read callback
  _trxCallback = [asyncSelf = _asyncSelf](transaction::Methods& trx,
                                          transaction::Status status) {
    if (!ServerState::instance()->isSingleServer() ||
        status != transaction::Status::RUNNING) {
      return;
    }
    if (auto viewLock = asyncSelf->lock(); viewLock) {  // populate snapshot
      TRI_ASSERT(trx.state());
      void const* key = static_cast<LogicalView*>(viewLock.get());
      auto* snapshot = getViewSnapshot(trx, key);
      if (snapshot == nullptr) {
        makeViewSnapshot(trx, key, false, viewLock->name(),
                         viewLock->getLinks());
      }
    }
  };
}

IResearchView::~IResearchView() {
  _asyncSelf->reset();  // the view is being deallocated,
  // its use is no longer valid (wait for all the view users to finish)
}

Result IResearchView::appendVPackImpl(velocypack::Builder& build,
                                      Serialization ctx, bool safe) const {
  if (ctx == Serialization::List) {
    return {};  // nothing more to output
  }
  if (!build.isOpenObject()) {
    return {TRI_ERROR_BAD_PARAMETER};
  }
  velocypack::Builder sanitizedBuild;
  sanitizedBuild.openObject();
  IResearchViewMeta::Mask mask{true};
  auto const propertiesAccept = +[](std::string_view key) noexcept {
    return key != StaticStrings::VersionField;  // ignored fields
  };
  auto const persistenceAccept = +[](std::string_view) noexcept {
    return true;  // for breakpoint
  };
  bool const persistence = ctx == Serialization::Persistence ||
                           ctx == Serialization::PersistenceWithInProgress;
  try {
    std::shared_lock lock{_mutex, std::defer_lock};
    if (!safe) {
      lock.lock();
    }
    if (!_meta.json(sanitizedBuild, nullptr, &mask) ||
        !mergeSliceSkipKeys(
            build, sanitizedBuild.close().slice(),
            *(persistence ? persistenceAccept : propertiesAccept))) {
      return {TRI_ERROR_INTERNAL,
              "failure to generate definition while generating properties jSON "
              "for arangosearch View in database '" +
                  vocbase().name() + "'"};
    }
    if (persistence) {
      IResearchViewMetaState metaState;
      for (auto& entry : _links) {
        metaState._collections.emplace(entry.first);
      }
      metaState.json(build);
      // nothing more to output (persistent configuration does not need links)
      return {};
    }
    std::vector<std::string> collections;
    // add CIDs of known collections to list
    for (auto& entry : _links) {
      // skip collections missing from vocbase or
      // UserTransaction constructor will throw an exception
      if (vocbase().lookupCollection(entry.first)) {
        collections.emplace_back(std::to_string(entry.first.id()));
      }
    }
    if (!safe) {
      lock.unlock();
    }
    // open up a read transaction and
    // add all linked collections to verify that the current user has access
    std::vector<std::string> const EMPTY;
    transaction::Options options;  // use default lock timeout
    options.waitForSync = false;
    options.allowImplicitCollectionsForRead = false;
    transaction::Methods trx(transaction::StandaloneContext::Create(vocbase()),
                             collections,  // readCollections
                             EMPTY,        // writeCollections
                             EMPTY,        // exclusiveCollections
                             options);
    auto r = trx.begin();
    if (!r.ok()) {
      return r;
    }
    auto* state = trx.state();
    if (!state) {
      return {TRI_ERROR_INTERNAL,
              "failed to get transaction state while generating json for "
              "arangosearch view '" +
                  name() + "'"};
    }
    velocypack::Builder linksBuild;
    auto const accept = [](std::string_view key) noexcept {
      return key != arangodb::StaticStrings::IndexId &&
             key != arangodb::StaticStrings::IndexType &&
             key != StaticStrings::ViewIdField;
    };
    auto visitor = [&](TransactionCollection& trxCollection) {
      auto const& collection = trxCollection.collection();
      if (!collection) {
        return true;  // skip missing collections
      }
      auto link = IResearchLinkHelper::find(*collection, *this);
      if (!link) {
        return true;  // no links for the current view
      }
      velocypack::Builder linkBuild;
      linkBuild.openObject();
      if (!link->properties(linkBuild, ctx == Serialization::Inventory).ok()) {
        // link definitions are not output if forPersistence
        LOG_TOPIC("713ad", WARN, TOPIC)
            << "failed to generate json for arangosearch link '" << link->id()
            << "' while generating json for arangosearch view '" << name()
            << "'";
        return true;  // skip invalid link definitions
      }
      linksBuild.add(collection->name(),
                     velocypack::Value(velocypack::ValueType::Object));
      if (!mergeSliceSkipKeys(linksBuild, linkBuild.close().slice(), accept)) {
        r = {TRI_ERROR_INTERNAL,
             "failed to generate arangosearch link '" +
                 std::to_string(link->id().id()) +
                 "' definition while generating json for arangosearch view '" +
                 name() + "'"};
        return false;  // terminate generation
      }
      linksBuild.close();
      return true;  // done with this collection
    };
    linksBuild.openObject();
    state->allCollections(visitor);
    linksBuild.close();
    if (!r.ok()) {
      return r;
    }
    r = trx.commit();
    build.add(StaticStrings::LinksField, linksBuild.slice());
    return r;
  } catch (basics::Exception& e) {
    return {e.code(),
            "caught exception while generating json for arangosearch view '" +
                name() + "': " + e.what()};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while generating json for arangosearch view '" +
                name() + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while generating json for arangosearch view '" +
                name() + "'"};
  }
}

bool IResearchView::apply(transaction::Methods& trx) {
  // called from IResearchView when this view is added to a transaction
  return trx.addStatusChangeCallback(&_trxCallback);  // add snapshot
}

Result IResearchView::dropImpl() {
  // drop all known links
  std::unordered_set<DataSourceId> collections;
  std::unordered_set<DataSourceId> stale;
  {
    std::shared_lock lock{_mutex};
    for (auto& entry : _links) {
      stale.emplace(entry.first);
    }
  }
  if (!stale.empty()) {
    // check link auth as per https://github.com/arangodb/backlog/issues/459
    if (!ExecContext::current().isSuperuser()) {
      for (auto& entry : stale) {
        auto collection = vocbase().lookupCollection(entry);
        if (collection &&
            !ExecContext::current().canUseCollection(
                vocbase().name(), collection->name(), auth::Level::RO)) {
          return {TRI_ERROR_FORBIDDEN};
        }
      }
    }
    // TODO Why try lock?
    std::unique_lock lock{_updateLinksLock, std::try_to_lock};
    if (!lock.owns_lock()) {
      return {TRI_ERROR_FAILED,  // FIXME use specific error code
              "failed to remove arangosearch view '" + name()};
    }
    auto r = IResearchLinkHelper::updateLinks(
        collections, *this, velocypack::Slice::emptyObjectSlice(),
        LinkVersion::MAX,
        // we don't care of link version due to removal only request
        stale);
    if (!r.ok()) {
      return {r.errorNumber(),
              "failed to remove links while removing arangosearch view '" +
                  name() + "': " + std::string{r.errorMessage()}};
    }
  }
  _asyncSelf->reset();
  // the view data-stores are being deallocated, view use is no longer valid
  // (wait for all the view users to finish)
  std::lock_guard lock{_mutex};  // TODO Why exclusive lock?
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
    return {TRI_ERROR_INTERNAL,
            "links still present while removing arangosearch view '" +
                std::to_string(id().id()) + "'"};
  }
  return ServerState::instance()->isSingleServer()
             ? storage_helper::drop(*this)
             // single-server additionally requires removal from StorageEngine
             : Result{};
}

arangodb::ViewFactory const& IResearchView::factory() {
  static IResearchView::ViewFactory const factory;
  return factory;
}

Result IResearchView::link(AsyncLinkPtr const& link) {
  if (!link) {
    return {TRI_ERROR_BAD_PARAMETER,
            "invalid link parameter while emplacing collection into "
            "arangosearch View '" +
                name() + "'"};
  }
  // prevent the link from being deallocated
  auto linkLock = link->lock();
  if (!linkLock) {
    return {TRI_ERROR_BAD_PARAMETER,
            "failed to acquire link while emplacing collection into "
            "arangosearch View '" +
                name() + "'"};
  }
  auto const cid = linkLock->collection().id();
  std::lock_guard lock{_mutex};
  auto it = _links.find(cid);
  if (it == _links.end()) {
    bool inserted = false;
    std::tie(it, inserted) = _links.try_emplace(cid, link);
    TRI_ASSERT(inserted);
  } else if ((ServerState::instance()->isSingleServer() && !it->second) ||
             (it->second && it->second->empty())) {
    // single-server persisted cid placeholder substituted with actual link
    // or
    // a previous link instance was unload()ed and a new instance is linking
    it->second = link;
    IResearchDataStore::properties(std::move(linkLock), _meta);
    return {};
  } else {
    return {TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
            std::string("duplicate entry while emplacing collection '") +
                std::to_string(cid.id()) + "' into arangosearch View '" +
                name() + "'"};
  }
  if (ServerState::instance()->isSingleServer()) {
    if (auto r = storage_helper::properties(*this, true); !r.ok()) {
      TRI_ASSERT(it != _links.end());
      _links.erase(it);
      return r;
    }
  }
  IResearchDataStore::properties(std::move(linkLock), _meta);
  return {};
}

void IResearchView::open() {
  auto& engine =
      vocbase().server().getFeature<EngineSelectorFeature>().engine();
  _inRecovery = engine.inRecovery();
}

Result IResearchView::properties(velocypack::Slice slice, bool isUserRequest,
                                 bool partialUpdate) {
  auto r = updateProperties(slice, isUserRequest, partialUpdate);
  if (!r.ok()) {
    return r;
  }
#if USE_PLAN_CACHE
  aql::PlanCache::instance()->invalidate(&vocbase());
#endif
  aql::QueryCache::instance()->invalidate(&vocbase());
  if (ServerState::instance()->isSingleServer()) {
    return storage_helper::properties(*this, false);
  } else {
    return cluster_helper::properties(*this, false);
  }
}

Result IResearchView::renameImpl(std::string const& oldName) {
  return ServerState::instance()->isSingleServer()
             ? storage_helper::rename(*this, oldName)
             : Result{TRI_ERROR_CLUSTER_UNSUPPORTED};
}

Result IResearchView::unlink(DataSourceId cid) noexcept {
  try {
    std::lock_guard lock{_mutex};
    auto it = _links.find(cid);
    if (it == _links.end()) {
      return {};  // already unlinked
    }
    auto link = _links.extract(it);
    if (ServerState::instance()->isSingleServer()) {
      if (auto r = storage_helper::properties(*this, true); !r.ok()) {
        LOG_TOPIC("9d678", WARN, TOPIC)
            << "failed to persist logical view while unlinking collection '"
            << cid << "' from arangosearch view '" << name()
            << "': " << r.errorMessage();  // noexcept
        _links.insert(move(link));         // noexcept
        return r;
      }
    }
  } catch (basics::Exception const& e) {
    return {e.code(), "caught exception while unlinking collection '" +
                          std::to_string(cid.id()) +
                          "' from arangosearch view '" + name() +
                          "': " + e.what()};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while unlinking collection '" +
                std::to_string(cid.id()) + "' from arangosearch view '" +
                name() + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while unlinking collection '" +
                std::to_string(cid.id()) + "' from arangosearch view '" +
                name() + "'"};
  }
  return {};
}

Result IResearchView::updateProperties(velocypack::Slice slice,
                                       bool isUserRequest, bool partialUpdate) {
  try {
    auto links = slice.hasKey(StaticStrings::LinksField)
                     ? slice.get(StaticStrings::LinksField)
                     : velocypack::Slice::emptyObjectSlice();
    auto r = _inRecovery ? Result{}  // do not validate if in recovery
                         : IResearchLinkHelper::validateLinks(vocbase(), links);
    if (!r.ok()) {
      return r;
    }
    boost::unique_lock uniqueLock{_mutex};
    // check link auth as per https://github.com/arangodb/backlog/issues/459
    if (!ExecContext::current().isSuperuser()) {
      for (auto& entry : _links) {
        auto collection = vocbase().lookupCollection(entry.first);
        if (collection &&
            !ExecContext::current().canUseCollection(
                vocbase().name(), collection->name(), auth::Level::RO)) {
          return {
              TRI_ERROR_FORBIDDEN,
              "while updating arangosearch definition, error: collection '" +
                  collection->name() + "' not authorized for read access"};
        }
      }
    }
    // TODO read necessary members from slice in single call
    std::string error;
    IResearchViewMeta meta;
    auto& defaults = partialUpdate ? _meta : IResearchViewMeta::DEFAULT();
    if (!meta.init(slice, error, defaults)) {
      return {TRI_ERROR_BAD_PARAMETER,
              "failed to update arangosearch view '" + name() +
                  "' from definition" +
                  (error.empty() ? ": "
                                 : (", error in attribute '" + error + "': ")) +
                  slice.toString()};
    }
    _meta.storePartial(std::move(meta));
    boost::shared_lock sharedLock{std::move(uniqueLock)};
    for (auto& entry : _links) {
      auto linkLock = entry.second->lock();
      if (linkLock) {
        IResearchDataStore::properties(std::move(linkLock), _meta);
      }
    }
    if (links.isEmptyObject() && (partialUpdate || _inRecovery.load())) {
      // ignore missing links coming from WAL (inRecovery)
      return r;
    }
    // ...........................................................................
    // update links if requested (on a best-effort basis)
    // indexing of collections is done in different threads
    // so no locks can be held and rollback is not possible as a result
    // it's also possible for links to be simultaneously modified via a
    // different callflow (e.g. from collections)
    // ...........................................................................
    std::unordered_set<DataSourceId> collections;
    std::unordered_set<DataSourceId> stale;
    if (!partialUpdate) {
      for (auto& entry : _links) {
        stale.emplace(entry.first);
      }
    }
    sharedLock.unlock();
    std::lock_guard lock{_updateLinksLock};
    return IResearchLinkHelper::updateLinks(
        collections, *this, links, getDefaultVersion(isUserRequest), stale);
  } catch (basics::Exception& e) {
    LOG_TOPIC("74705", WARN, TOPIC)
        << "caught exception while updating properties for arangosearch view '"
        << name() << "': " << e.code() << " " << e.what();
    return {e.code(),
            "error updating properties for arangosearch view '" + name() + "'"};
  } catch (std::exception const& e) {
    LOG_TOPIC("27f54", WARN, TOPIC)
        << "caught exception while updating properties for arangosearch view '"
        << name() << "': " << e.what();

    return {TRI_ERROR_BAD_PARAMETER,
            "error updating properties for arangosearch view '" + name() + "'"};
  } catch (...) {
    LOG_TOPIC("99bbe", WARN, TOPIC)
        << "caught exception while updating properties for arangosearch view '"
        << name() << "'";
    return {TRI_ERROR_BAD_PARAMETER,
            "error updating properties for arangosearch view '" + name() + "'"};
  }
}

bool IResearchView::visitCollections(
    LogicalView::CollectionVisitor const& visitor) const {
  std::shared_lock lock{_mutex};
  for (auto& entry : _links) {
    if (!visitor(entry.first, nullptr)) {
      return false;
    }
  }
  return true;
}

LinkLock IResearchView::linkLock(
    std::shared_lock<boost::upgrade_mutex> const& guard,
    DataSourceId cid) const noexcept {
  TRI_ASSERT(guard.owns_lock());
  if (auto it = _links.find(cid); it != _links.end() && it->second) {
    return it->second->lock();
  }
  return {};
}

ViewSnapshot::Links IResearchView::getLinks() const noexcept {
  ViewSnapshot::Links links;
  auto const lock = linksReadLock();
  links.reserve(_links.size());
  for (auto const& [_, link] : _links) {
    links.emplace_back(link ? link->lock() : LinkLock{});
  }
  return links;
}

void IResearchView::verifyKnownCollections() {
  bool modified = false;
  std::lock_guard lock{_mutex};
  for (auto it = _links.begin(), end = _links.end(); it != end;) {
    auto remove = [&] {
      auto it_erase = it++;
      _links.erase(it_erase);
      modified = true;
    };
    auto cid = it->first;
    auto collection = vocbase().lookupCollection(cid);
    // always look up in vocbase (single server or cluster per-shard collection)
    if (!collection) {
      LOG_TOPIC("40976", TRACE, TOPIC)
          << "collection '" << cid
          << "' no longer exists! removing from arangosearch view '" << name()
          << "'";
      remove();
      continue;
    }
    auto link = IResearchLinkHelper::find(*collection, *this);
    if (!link) {
      LOG_TOPIC("d0509", TRACE, TOPIC)
          << "collection '" << collection->name()
          << "' no longer linked! removing from arangosearch view '" << name()
          << "'";
      remove();
      continue;
    }
    // all links must be valid even on single-server
    TRI_ASSERT(it->second);
    ++it;
  }

  if (modified && ServerState::instance()->isSingleServer()) {
    std::ignore = storage_helper::properties(*this, true);
  }
}

}  // namespace arangodb::iresearch
