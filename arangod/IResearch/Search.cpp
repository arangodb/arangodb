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

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#include "utils/automaton.hpp"

using Weight = fst::fsa::BooleanWeight;

namespace fst {

inline Weight Times(const Weight& lhs, const Weight& rhs) {
  if (!lhs.Member() || !rhs.Member()) {
    return Weight::NoWeight();
  }
  return Weight{lhs && rhs};
}

inline Weight Plus(const Weight& lhs, const Weight& rhs) {
  if (!lhs.Member() || !rhs.Member()) {
    return Weight::NoWeight();
  }
  return Weight{lhs || rhs};
}

inline Weight DivideLeft(const Weight& lhs, const Weight& rhs) {
  if (!lhs.Member() || !rhs.Member()) {
    return Weight::NoWeight();
  }
  return Weight{
      static_cast<bool>(static_cast<bool>(lhs) - static_cast<bool>(rhs))};
}

inline Weight Divide(const Weight& lhs, const Weight& rhs,
                     DivideType typ = DIVIDE_ANY) {
  return DivideLeft(lhs, rhs);
}

}  // namespace fst

#include "utils/fstext/fst_builder.hpp"
#include "utils/fstext/fst_matcher.hpp"

using Arc = fst::ArcTpl<Weight>;
using VectorFst = fst::VectorFst<Arc>;
using FstBuilder = irs::fst_builder<char, VectorFst>;
using ExplicitMatcher = fst::explicit_matcher<fst::SortedMatcher<VectorFst>>;

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

template<typename V>
auto Find(SearchMeta::Map const& map, V&& v) noexcept {
  return map.find(std::forward<V>(v));
}

template<typename V>
auto Find(auto const& vector, V const& v) noexcept {
  auto last = end(vector);
  auto it = std::lower_bound(
      begin(vector), last, v,
      [](auto const& field, auto const& value) { return field.first < value; });
  if (it != last && it->first == v) {
    return it;
  }
  return last;
}

std::string checkFields(SearchMeta const& search,
                        IResearchInvertedIndexMeta const& index) {
  auto check = [](auto const& lhs, auto const& rhs, bool lhsView) {
    std::string_view const lhsIs = lhsView ? "view" : "index";
    std::string_view const rhsIs = lhsView ? "Index" : "View";

    VectorFst fst;
    FstBuilder builder{fst};
    for (auto const& field : lhs) {
      builder.add(field.first, true);
    }
    builder.finish();

    auto start = fst.Start();
    ExplicitMatcher matcher{&fst, fst::MATCH_INPUT};
    for (auto const& field : rhs) {
      auto const fieldName = field.first;
      matcher.SetState(start);

      size_t lastKey = 0;
      for (size_t matched = 0; matched < fieldName.size();) {
        if (!matcher.Find(fieldName[matched])) {
          break;
        }
        ++matched;
        auto const nextstate = matcher.Value().nextstate;
        if (fst.Final(nextstate)) {
          lastKey = matched;
        }
        matcher.SetState(nextstate);
      }
      auto it = Find(lhs, fieldName.substr(0, lastKey));
      if (it == lhs.end()) {
        TRI_ASSERT(lastKey == 0);
        continue;
      }
      if (it->second.analyzer == field.second.analyzer) {
        continue;
      }
      if (it->first == fieldName) {
        return absl::StrCat(rhsIs, " field '", fieldName, "' analyzer '",
                            field.second.analyzer, "' mismatches ", lhsIs,
                            " field analyzer '", it->second.analyzer, "'");
      } else if (it->second.includeAllFields) {
        return absl::StrCat(
            rhsIs, " field '", fieldName, "' analyzer '", field.second.analyzer,
            "' mismatches ", lhsIs, " field '", it->first,
            "' with includeAllFields analyzer '", it->second.analyzer, "'");
      }
    }
    return std::string{};
  };
  std::vector<std::pair<std::string, SearchMeta::Field>> fields;
  fields.reserve(index._fields.size() + index._includeAllFields);
  for (auto const& field : index._fields) {
    fields.emplace_back(field.path(),
                        SearchMeta::Field{field.analyzer()._shortName,
                                          field._includeAllFields});
  }
  if (index._includeAllFields) {
    fields.emplace_back("",
                        SearchMeta::Field{index.analyzer()._shortName, true});
  }
  std::sort(fields.begin(), fields.end(), [](auto const& lhs, auto const& rhs) {
    return lhs.first < rhs.first;
  });
  auto error = check(search.fieldToAnalyzer, fields, true);
  if (error.empty()) {
    error = check(fields, search.fieldToAnalyzer, false);
  }
  return error;
}

std::string check(SearchMeta const& search,
                  IResearchInvertedIndexMeta const& index) {
  if (search.primarySort != index._sort) {
    return "index primary sort mismatches view primary sort";
  }
  if (search.storedValues != index._storedValues) {
    return "index stored values mismatches view stored values";
  }
  return {};
}

void add(SearchMeta& search, IResearchInvertedIndexMeta const& index) {
  for (auto const& field : index._fields) {
    auto it = search.fieldToAnalyzer.lower_bound(field.path());
    if (it == search.fieldToAnalyzer.end() || it->first != field.path()) {
      search.fieldToAnalyzer.emplace_hint(
          it, field.path(),
          SearchMeta::Field{field.analyzer()._shortName,
                            field._includeAllFields});
    } else {
      it->second.includeAllFields |= field._includeAllFields;
    }
  }
  if (index._includeAllFields) {
    search.fieldToAnalyzer.emplace(
        "", SearchMeta::Field{index.analyzer()._shortName, true});
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
    if (auto const& ctx = ExecContext::current(); !ctx.isSuperuser()) {
      if (!ctx.canUseCollection(vocbase().name(), collection->name(),
                                auth::Level::RO)) {
        return {TRI_ERROR_FORBIDDEN,
                absl::StrCat("Current user cannot use collection '",
                             collection->name(), "'")};
      }
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

  auto iterate = [&](auto const& init, auto const& next) -> Result {
    bool first = true;
    for (auto const& [_, handles] : _indexes) {
      for (auto const& handle : handles) {
        if (auto index = handle->lock(); index) {
          auto const& inverted =
              basics::downCast<IResearchInvertedIndex>(*index.get());
          auto const& indexMeta = inverted.meta();
          if (first) {
            init(indexMeta);
            first = false;
          } else {
            auto error = next(indexMeta);
            if (!error.empty()) {
              // TODO Remove dynamic_cast
              auto const& arangodbIndex = dynamic_cast<Index const&>(inverted);
              absl::StrAppend(&error, ". Collection name '",
                              arangodbIndex.collection().name(),
                              "', index name '", arangodbIndex.name(), "'.");
              return {TRI_ERROR_BAD_PARAMETER, std::move(error)};
            }
          }
        }
      }
    }
    return {};
  };

  auto searchMeta = std::make_shared<SearchMeta>();
  auto r = iterate(
      [&](auto const& indexMeta) {
        searchMeta->primarySort = indexMeta._sort;
        searchMeta->storedValues = indexMeta._storedValues;
      },
      [&](auto const& indexMeta) { return check(*searchMeta, indexMeta); });
  if (!r.ok()) {
    return r;
  }
  r = iterate([&](auto const& indexMeta) { add(*searchMeta, indexMeta); },
              [&](auto const& indexMeta) {
                auto error = checkFields(*searchMeta, indexMeta);
                if (error.empty()) {
                  add(*searchMeta, indexMeta);
                }
                return error;
              });
  if (r.ok()) {
    _meta = std::move(searchMeta);
  }
  return r;
}

}  // namespace arangodb::iresearch

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
