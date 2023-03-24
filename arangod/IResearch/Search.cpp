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
#include "Basics/ScopeGuard.h"
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
#include "frozen/unordered_set.h"

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

std::string check(SearchMeta const& search,
                  IResearchInvertedIndex const& index) {
  auto const& meta = index.meta();
  if (search.includeAllFields && meta._includeAllFields &&
      search.rootAnalyzer != meta.analyzer()._shortName) {
    return absl::StrCat("index root analyzer '", meta.analyzer()._shortName,
                        "' mismatches view root analyzer '",
                        search.rootAnalyzer, "'");
  }
  if (search.primarySort != meta._sort) {
    return "index primary sort mismatches view primary sort";
  }
  if (search.storedValues != meta._storedValues) {
    return "index stored values mismatches view stored values";
  }
  // if B.includeAllFields then (A \ B).fields analyzer == B.rootAnalyzer
  for (auto const& field : meta._fields) {
    auto it = search.fieldToAnalyzer.find(field.path());
    if (it != search.fieldToAnalyzer.end()) {
      if (it->second != field.analyzer()._shortName) {
        return absl::StrCat("Index field '", it->first, "' analyzer '",
                            field.analyzer()._shortName,
                            "' mismatches view field analyzer '", it->second,
                            "'");
      }
    } else if (search.includeAllFields &&
               search.rootAnalyzer != field.analyzer()._shortName) {
      return absl::StrCat("Index field '", field.path(), "' analyzer '",
                          field.analyzer()._shortName,
                          "' mismatches view root analyzer '",
                          search.rootAnalyzer, "'");
    }
  }
  if (!meta._includeAllFields) {
    return {};
  }
  auto const& metaAnalyzer = meta.analyzer()._shortName;
  for (auto const& f : search.fieldToAnalyzer) {
    auto it = std::find_if(
        meta._fields.begin(), meta._fields.end(),
        [&](auto const& field) { return field.path() == f.first; });
    if (it == meta._fields.end() && f.second != metaAnalyzer) {
      return absl::StrCat("Index root analyzer '", metaAnalyzer,
                          "' mismatches view field '", f.first, "' analyzer '",
                          f.second, "'");
    }
  }
  return {};
}

void add(SearchMeta& search, IResearchInvertedIndex const& index) {
  auto const& meta = index.meta();
  // '_shortName' because vocbase name unnecessary
  if (!search.includeAllFields && meta._includeAllFields) {
    search.rootAnalyzer = meta.analyzer()._shortName;
    search.includeAllFields = true;
  }
  for (auto const& field : meta._fields) {
    search.fieldToAnalyzer.emplace(field.path(), field.analyzer()._shortName);
  }
}

}  // namespace

class SearchFactory final : public ViewFactory {
  // LogicalView factory for end-user validation instantiation and
  // persistence.
  // Return if success then 'view' is set, else 'view' state is undefined
  Result create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                velocypack::Slice definition, bool isUserRequest) const final;

  // LogicalView factory for internal instantiation only
  Result instantiate(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                     velocypack::Slice definition,
                     bool isUserRequest) const final;
};

Result SearchFactory::create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                             velocypack::Slice definition,
                             bool isUserRequest) const {
  if (!definition.isObject()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "search-alias view definition should be a object"};
  }
  auto const nameSlice = definition.get("name");
  if (nameSlice.isNone()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "search-alias view definition should contains field 'name'"};
  }
  if (ServerState::instance()->isCoordinator()) {
    LogicalView::ptr impl;
    auto r =
        cluster_helper::construct(impl, vocbase, definition, isUserRequest);
    if (!r.ok()) {
      return r;
    }
    view = impl;
  } else {
    TRI_ASSERT(ServerState::instance()->isSingleServer());
    LogicalView::ptr impl;
    auto r =
        storage_helper::construct(impl, vocbase, definition, isUserRequest);
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
                                  velocypack::Slice definition,
                                  bool isUserRequest) const {
  TRI_ASSERT(ServerState::instance()->isCoordinator() ||
             ServerState::instance()->isSingleServer());
  auto impl = std::make_shared<Search>(vocbase, definition);
  auto indexesSlice = definition.get("indexes");
  if (indexesSlice.isNone()) {
    view = impl;
    return {};
  }
  if (!indexesSlice.isArray()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "search-alias view optional field 'indexes' should be array"};
  }
  CollectionNameResolver resolver{vocbase};
  velocypack::ArrayIterator it{indexesSlice};
  auto r = impl->updateProperties(resolver, it, isUserRequest);
  if (r.ok()) {
    view = impl;
  }
  return r;
}

ViewFactory const& Search::factory() {
  static SearchFactory const factory;
  return factory;
}

Search::Search(TRI_vocbase_t& vocbase, velocypack::Slice definition)
    : LogicalView{*this, vocbase, definition},
      _meta{std::make_shared<SearchMeta const>()} {
  if (ServerState::instance()->isSingleServer()) {
    _asyncSelf = std::make_shared<AsyncSearchPtr::element_type>(this);

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

std::shared_ptr<SearchMeta const> Search::meta() const {
  std::shared_lock lock{_mutex};
  return _meta;
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

Result Search::properties(velocypack::Slice definition, bool isUserRequest,
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
  auto oldIndexes = [&] {
    if (partialUpdate) {
      return _indexes;
    }
    return std::move(_indexes);
  }();
  auto oldMeta = std::move(_meta);
  ScopeGuard revert{[&]() noexcept {
    _indexes = std::move(oldIndexes);
    _meta = std::move(oldMeta);
  }};
  auto r = updateProperties(resolver, it, isUserRequest);
  if (!r.ok()) {
    return r;
  }
  if (ServerState::instance()->isCoordinator()) {
    r = cluster_helper::properties(*this, true /*means under lock*/);
  } else {
    TRI_ASSERT(ServerState::instance()->isSingleServer());
#ifdef USE_PLAN_CACHE
    aql::PlanCache::instance()->invalidate(&vocbase());
#endif
    aql::QueryCache::instance()->invalidate(&vocbase());
    r = storage_helper::properties(*this, true /*means under lock*/);
  }
  if (r.ok()) {
    revert.cancel();
  }
  return r;
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

Result Search::updateProperties(CollectionNameResolver& resolver,
                                velocypack::ArrayIterator it,
                                bool isUserRequest) {
  constexpr auto kOperations =
      frozen::make_unordered_set<frozen::string>({"", "add", "del"});
  for (; it.valid(); ++it) {
    auto value = *it;
    auto collection = getCollection(resolver, value.get("collection"));
    if (!collection) {
      if (!isUserRequest) {
        continue;
      }
      return {TRI_ERROR_BAD_PARAMETER, "Cannot find collection"};
    }
    auto const cid = collection->id();
    auto operationSlice = value.get("operation");
    auto const operation =
        operationSlice.isNone() ? "" : operationSlice.stringView();
    TRI_ASSERT(operation.empty() || isUserRequest);
    if (isUserRequest) {
      if (!kOperations.count(operation)) {
        return {TRI_ERROR_BAD_PARAMETER, "Invalid type of operation"};
      }
      if (operation == "del" && !_indexes.contains(cid)) {
        return {TRI_ERROR_BAD_PARAMETER,
                "Cannot find collection for index to delete"};
      }
    }
    auto index = getIndex(*collection, value.get("index"));
    // TODO Remove dynamic_cast
    auto* inverted = dynamic_cast<IResearchInvertedIndex*>(index.get());
    if (!inverted) {
      if (!isUserRequest) {
        continue;
      }
      return {TRI_ERROR_BAD_PARAMETER, "Cannot find index"};
    }
    auto& indexes = _indexes[cid];
    if (operation != "del") {
      indexes.emplace_back(inverted->self());
      TRI_ASSERT(indexes.back());
    } else {
      auto indexIt = std::find(begin(indexes), end(indexes), inverted->self());
      if (indexIt == end(indexes)) {
        // TODO log and skip?
        return {TRI_ERROR_BAD_PARAMETER, "Cannot find index to delete"};
      }
      indexes.back().swap(*indexIt);
      indexes.pop_back();
    }
  }
  auto meta = std::make_shared<SearchMeta>();
  bool first = true;
  for (auto const& [_, handles] : _indexes) {
    for (auto const& handle : handles) {
      if (auto index = handle->lock(); index) {
        auto const& inverted =
            basics::downCast<IResearchInvertedIndex>(*index.get());
        if (first) {
          meta->primarySort = inverted.meta()._sort;
          meta->storedValues = inverted.meta()._storedValues;
          meta->fieldToAnalyzer.reserve(inverted.meta()._fields.size());
          first = false;
        } else {
          auto error = check(*meta, inverted);
          if (!error.empty()) {
            // TODO Remove dynamic_cast
            auto const& arangodbIndex = dynamic_cast<Index const&>(inverted);
            absl::StrAppend(&error, ". Collection name '",
                            arangodbIndex.collection().name(),
                            "', index name '", arangodbIndex.name(), "'.");
            return {TRI_ERROR_BAD_PARAMETER, std::move(error)};
          }
        }
        add(*meta, inverted);
      }
    }
  }
  _meta = std::move(meta);
  return {};
}

}  // namespace arangodb::iresearch
