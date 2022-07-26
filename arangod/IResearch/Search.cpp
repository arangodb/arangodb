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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "Basics/Exceptions.h"
#include "Basics/DownCast.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Indexes/Index.h"
#include "IResearch/Search.h"
#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/ViewSnapshot.h"
#include "RestServer/ViewTypesFeature.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"

#ifdef USE_PLAN_CACHE
#include "Aql/PlanCache.h"
#endif

#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>

namespace arangodb::iresearch {
namespace {

std::shared_ptr<LogicalCollection> getCollection(
    CollectionNameResolver& resolver, velocypack::Slice cidOrName) {
  if (cidOrName.isString()) {
    return resolver.getCollection(cidOrName.stringView());
  } else if (cidOrName.isNumber<DataSourceId::BaseType>()) {
    auto const cid = cidOrName.getNumber<DataSourceId::BaseType>();
    absl::AlphaNum const cidStr{cid};  // TODO make resolve by id?
    return resolver.getCollection(cidStr.Piece());
  }
  return nullptr;
}

std::shared_ptr<Index> getIndex(LogicalCollection const& collection,
                                velocypack::Slice index) {
  auto indexId = IndexId::none().id();
  if (index.isString()) {
    auto const indexNameOrId = index.stringView();
    if (auto handle = collection.lookupIndex(indexNameOrId); handle) {
      return handle;
    } else if (absl::SimpleAtoi(indexNameOrId, &indexId)) {
      return collection.lookupIndex(IndexId{indexId});
    }
  } else if (index.isNumber<IndexId::BaseType>()) {
    indexId = index.getNumber<IndexId::BaseType>();
    return collection.lookupIndex(IndexId{indexId});
  }
  return nullptr;
}

}  // namespace

class SearchFactory : public ViewFactory {
  // LogicalView factory for end-user validation instantiation and
  // persistence.
  // Return if success then 'view' is set, else 'view' state is undefined
  Result create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                velocypack::Slice definition, bool isUserRequest) const final;

  // LogicalView factory for internal instantiation only
  Result instantiate(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                     velocypack::Slice definition) const final;
};

Result SearchFactory::create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                             velocypack::Slice definition,
                             bool isUserRequest) const {
  if (!definition.isObject()) {
    return {TRI_ERROR_BAD_PARAMETER};
  }
  auto const nameSlice = definition.get("name");
  if (nameSlice.isNone()) {
    return {TRI_ERROR_BAD_PARAMETER};
  }
  if (ServerState::instance()->isCoordinator()) {
    // auto indexesSlice = definition.get("indexes");
    // auto r = Some::validateIndexes(vocbase, indexesSlice);
    // if (!r.ok()) {
    //   return r;
    // }
    LogicalView::ptr impl;
    auto r = cluster_helper::construct(impl, vocbase, definition);
    if (!r.ok()) {
      return r;
    }
    view = impl;
  } else {
    TRI_ASSERT(ServerState::instance()->isSingleServer());
    // auto& server = vocbase.server();
    // TRI_ASSERT(server.hasFeature<EngineSelectorFeature>());
    // auto& engine = server.getFeature<EngineSelectorFeature>().engine();
    // auto indexesSlice = definition.get("indexes");
    // auto r = engine.inRecovery() ? Result{}  // don't validate if in recovery
    //                              : Some::validateLinks(vocbase, indexesSlice)
    // if (!r.ok()) {
    //   auto name = definition.get("name");
    //   events::CreateView(vocbase.name(), name, r.errorNumber());
    //   return r;
    // }
    LogicalView::ptr impl;
    auto r = storage_helper::construct(impl, vocbase, definition);
    if (!r.ok()) {
      auto name = nameSlice.copyString();  // TODO stringView()
      events::CreateView(vocbase.name(), name, r.errorNumber());
      return r;
    }
    view = impl;
  }
  TRI_ASSERT(view);
  return {};
}

Result SearchFactory::instantiate(LogicalView::ptr& view,
                                  TRI_vocbase_t& vocbase,
                                  velocypack::Slice definition) const {
  TRI_ASSERT(ServerState::instance()->isCoordinator() ||
             ServerState::instance()->isSingleServer());
  auto impl = std::make_shared<Search>(vocbase, definition);
  auto indexesSlice = definition.get("indexes");
  if (indexesSlice.isNone()) {
    view = impl;
    return {};
  }
  if (!indexesSlice.isArray()) {
    return {TRI_ERROR_BAD_PARAMETER, "indexes field should be array"};
  }
  velocypack::ArrayIterator it{indexesSlice};
  CollectionNameResolver resolver{vocbase};
  for (; it.valid(); ++it) {
    auto value = *it;
    auto const operation =
        value.hasKey("operation") ? value.get("operation").stringView() : "";
    TRI_ASSERT(operation.empty() || operation == "add");
    if (!(operation.empty() || operation == "add")) {
      // TODO maybe log and skip?
      return {TRI_ERROR_BAD_PARAMETER, "Invalid type of operation"};
    }
    auto collection = getCollection(resolver, value.get("collection"));
    if (!collection) {
      return {TRI_ERROR_BAD_PARAMETER, "Cannot find collection"};
    }
    auto index = getIndex(*collection, value.get("index"));
    // TODO Remove dynamic_cast
    auto* inverted = dynamic_cast<IResearchInvertedIndex*>(index.get());
    if (!inverted) {
      return {TRI_ERROR_BAD_PARAMETER, "Cannot parse index"};
    }
    auto& indexes = impl->_indexes[collection->id()];
    indexes.emplace_back(inverted->self());
    TRI_ASSERT(indexes.back());
  }
  view = impl;
  return {};
}

ViewFactory const& Search::factory() {
  static SearchFactory const factory;
  return factory;
}

Search::Search(TRI_vocbase_t& vocbase, velocypack::Slice definition)
    : LogicalView(*this, vocbase, definition) {
  if (ServerState::instance()->isSingleServer()) {
    _asyncSelf = std::make_shared<AsyncSearchPtr::element_type>(this);

    // set up in-recovery insertion hooks
    // TRI_ASSERT(vocbase.server().hasFeature<DatabaseFeature>());
    // auto& databaseFeature = vocbase.server().getFeature<DatabaseFeature>();
    // databaseFeature.registerPostRecoveryCallback([view = _asyncSelf] {
    //   //  ensure view does not get deallocated before call back finishes
    //   auto viewLock = view->lock();
    //   if (viewLock) {
    //     viewLock->verifyKnownCollections();
    //   }
    //   return Result{};
    // });

    // initialize transaction read callback
    _trxCallback = [asyncSelf = _asyncSelf](transaction::Methods& trx,
                                            transaction::Status status) {
      if (!ServerState::instance()->isSingleServer() ||
          status != transaction::Status::RUNNING) {
        return;
      }
      if (auto lock = asyncSelf->lock(); lock) {  // populate snapshot
        TRI_ASSERT(trx.state());
        void const* key = static_cast<LogicalView*>(lock.get());
        auto* snapshot = getViewSnapshot(trx, key);
        if (snapshot == nullptr) {
          makeViewSnapshot(trx, key, false, lock->name(), lock->getLinks());
        }
      }
    };
  }
}

Search::~Search() {
  if (_asyncSelf) {
    _asyncSelf->reset();
  }
}

IResearchInvertedIndexSort const& Search::primarySort() const {
  std::shared_lock lock{_mutex};
  for (auto const& [_, handles] : _indexes) {
    for (auto const& handle : handles) {
      if (auto index = handle->lock(); index) {
        auto* inverted = basics::downCast<IResearchInvertedIndex>(index.get());
        return inverted->meta()._sort;
      }
    }
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                 "Try to query empty view");
}

IResearchViewStoredValues const& Search::storedValues() const {
  std::shared_lock lock{_mutex};
  for (auto const& [_, handles] : _indexes) {
    for (auto const& handle : handles) {
      if (auto index = handle->lock(); index) {
        auto* inverted = basics::downCast<IResearchInvertedIndex>(index.get());
        return inverted->meta()._storedValues;
      }
    }
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                 "Try to query empty view");
}

bool Search::apply(transaction::Methods& trx) {
  // called from Search when this view is added to a transaction
  return trx.addStatusChangeCallback(&_trxCallback);  // add snapshot
}

ViewSnapshot::Links Search::getLinks() const {
  ViewSnapshot::Links indexes;
  std::shared_lock lock{_mutex};
  indexes.reserve(_indexes.size());
  for (auto const& [_, handles] : _indexes) {
    for (auto const& handle : handles) {
      indexes.push_back(handle->lock());
    }
  }
  return indexes;
}

Result Search::properties(velocypack::Slice definition, bool /*isUserRequest*/,
                          bool partialUpdate) {
  auto indexesSlice = definition.get("indexes");
  if (indexesSlice.isNone()) {
    indexesSlice = velocypack::Slice::emptyArraySlice();
  }
  velocypack::ArrayIterator it{indexesSlice};
  if (it.size() == 0 && partialUpdate) {
    return {};
  }
  CollectionNameResolver resolver{vocbase()};
  std::unique_lock lock{_mutex};
  if (!partialUpdate) {
    _indexes.clear();
  }
  for (; it.valid(); ++it) {
    auto value = *it;
    auto collection = getCollection(resolver, value.get("collection"));
    if (!collection) {
      return {TRI_ERROR_BAD_PARAMETER, "Cannot find collection"};
    }
    auto const cid = collection->id();
    auto const operation =
        value.hasKey("operation") ? value.get("operation").stringView() : "";
    TRI_ASSERT(operation.empty() || operation == "add" || operation == "del");
    if (operation == "del" && !_indexes.contains(cid)) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Cannot find collection for index to delete"};
    }
    auto index = getIndex(*collection, value.get("index"));
    // TODO Remove dynamic_cast
    auto* inverted = dynamic_cast<IResearchInvertedIndex*>(index.get());
    if (!inverted) {
      return {TRI_ERROR_BAD_PARAMETER, "Cannot parse index"};
    }
    auto const indexId = inverted->id();
    auto& indexes = _indexes[cid];
    auto indexIt = std::find_if(begin(indexes), end(indexes),
                                [indexId](const auto& handle) {
                                  auto other = handle->lock();
                                  return other && other->id() == indexId;
                                });
    if (operation == "del") {
      if (indexIt == end(indexes)) {
        // TODO log and skip?
        return {TRI_ERROR_BAD_PARAMETER, "Cannot find index to delete"};
      }
      indexes.back().swap(*indexIt);
      indexes.pop_back();
    } else {
      if (indexIt != end(indexes)) {
        // TODO log and skip?
        return {TRI_ERROR_BAD_PARAMETER,
                "This index already exit in that view"};
      }
      indexes.emplace_back(inverted->self());
      TRI_ASSERT(indexes.back());
    }
  }
  if (ServerState::instance()->isCoordinator()) {
    return cluster_helper::properties(*this, true /*means under lock*/);
  }
  TRI_ASSERT(ServerState::instance()->isSingleServer());
#ifdef USE_PLAN_CACHE
  aql::PlanCache::instance()->invalidate(&vocbase());
#endif
  aql::QueryCache::instance()->invalidate(&vocbase());
  return storage_helper::properties(*this, true /*means under lock*/);
}

void Search::open() {
  // if (ServerState::instance()->isSingleServer()) {
  //   auto& engine =
  //       vocbase().server().getFeature<EngineSelectorFeature>().engine();
  //   _inRecovery.store(engine.inRecovery(), std::memory_order_seq_cst);
  // }
}

bool Search::visitCollections(CollectionVisitor const& visitor) const {
  std::shared_lock lock{_mutex};
  for (auto& [cid, handles] : _indexes) {
    LogicalView::Indexes indexes;
    indexes.reserve(handles.size());
    for (auto& handle : handles) {
      if (auto index = handle->lock(); index) {
        indexes.push_back(index->id());
      }
    }
    if (!visitor(cid, &indexes)) {
      return false;
    }
  }
  return true;
}

Result Search::appendVPackImpl(velocypack::Builder& build, Serialization ctx,
                               bool safe) const {
  if (ctx == Serialization::List) {
    return {};  // nothing more to output
  }
  if (!build.isOpenObject()) {
    return {TRI_ERROR_BAD_PARAMETER};
  }
  try {
    CollectionNameResolver resolver{vocbase()};  // Cheap ctor
    build.add("indexes", velocypack::Value{velocypack::ValueType::Array});
    std::shared_lock sharedLock{_mutex, std::defer_lock};
    if (!safe) {
      sharedLock.lock();
    }
    for (auto& [cid, handles] : _indexes) {
      for (auto& handle : handles) {
        if (auto index = handle->lock(); index) {
          if (ctx == Serialization::Properties) {
            auto collectionName = resolver.getCollectionNameCluster(cid);
            // TODO(MBkkt) remove dynamic_cast
            auto* rawIndex = dynamic_cast<Index*>(index.get());
            if (collectionName.empty() || !rawIndex) {
              TRI_ASSERT(false);
              continue;
            }
            build.add(velocypack::Value{velocypack::ValueType::Object});
            build.add("collection", velocypack::Value{collectionName});
            build.add("index", velocypack::Value{rawIndex->name()});
          } else {
            build.add(velocypack::Value{velocypack::ValueType::Object});
            build.add("collection", velocypack::Value{cid.id()});
            // TODO Maybe guid or planId?
            build.add("index", velocypack::Value{index->id().id()});
          }
          build.close();
        }
      }
    }
    build.close();
    return {};
  } catch (basics::Exception& e) {
    return {
        e.code(),
        absl::StrCat("caught exception while generating json for search view '",
                     name(), "': ", e.what())};
  } catch (std::exception const& e) {
    return {
        TRI_ERROR_INTERNAL,
        absl::StrCat("caught exception while generating json for search view '",
                     name(), "': ", e.what())};
  } catch (...) {
    return {
        TRI_ERROR_INTERNAL,
        absl::StrCat("caught exception while generating json for search view '",
                     name(), "'")};
  }
}

Result Search::dropImpl() {
  {
    std::shared_lock lock{_mutex};
    _indexes = {};
  }
  if (ServerState::instance()->isSingleServer()) {
    return storage_helper::drop(*this);
  }
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  return cluster_helper::drop(*this);
}

Result Search::renameImpl(std::string const& oldName) {
  if (ServerState::instance()->isSingleServer()) {
    return storage_helper::rename(*this, oldName);
  }
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  return {TRI_ERROR_CLUSTER_UNSUPPORTED};
}

}  // namespace arangodb::iresearch
