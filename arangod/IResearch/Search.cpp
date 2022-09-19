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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4291)
#pragma warning(disable : 4244)
#pragma warning(disable : 4245)
#pragma warning(disable : 4706)
#pragma warning(disable : 4305)
#pragma warning(disable : 4267)
#pragma warning(disable : 4018)
#endif

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
      static_cast<bool>(static_cast<bool>(lhs) ^ static_cast<bool>(rhs))};
}

inline Weight Divide(const Weight& lhs, const Weight& rhs,
                     DivideType typ = DIVIDE_ANY) {
  return DivideLeft(lhs, rhs);
}

}  // namespace fst

#include "utils/fstext/fst_builder.hpp"
#include "utils/fstext/fst_matcher.hpp"

namespace arangodb::iresearch {
namespace {

using Arc = fst::ArcTpl<Weight>;
using VectorFst = fst::VectorFst<Arc>;
using FstBuilder = irs::fst_builder<char, VectorFst>;
using ExplicitMatcher = fst::explicit_matcher<fst::SortedMatcher<VectorFst>>;

std::shared_ptr<Index> getIndex(LogicalCollection const& collection,
                                std::string_view indexNameOrId) {
  if (auto handle = collection.lookupIndex(indexNameOrId); handle) {
    return handle;
  }
  auto indexId = IndexId::none().id();
  if (absl::SimpleAtoi(indexNameOrId, &indexId)) {
    return collection.lookupIndex(IndexId{indexId});
  }
  return nullptr;
}

auto find(SearchMeta::Map const& map, std::string_view key) noexcept {
  return map.find(key);
}

auto find(auto const& vector, std::string_view key) noexcept {
  auto last = vector.end();
  auto it = std::lower_bound(
      vector.begin(), last, key,
      [](auto const& field, auto const& value) { return field.first < value; });
  if (it != last && it->first == key) {
    return it;
  }
  return last;
}

std::string_view findLongestCommonPrefix(ExplicitMatcher& matcher,
                                         std::string_view key) noexcept {
  auto const& fst = matcher.GetFst();
  auto const start = fst.Start();
  matcher.SetState(start);
  size_t lastIndex = 0;
  for (size_t matched = 0; matched < key.size();) {
    if (!matcher.Find(key[matched])) {
      break;
    }
    ++matched;
    auto const nextstate = matcher.Value().nextstate;
    if (fst.Final(nextstate)) {
      lastIndex = matched;
    }
    matcher.SetState(nextstate);
  }
  return key.substr(0, lastIndex);
}

template<bool SameCollection>
std::string abstractCheckFields(auto const& lhs, auto const& rhs,
                                bool lhsView) {
  std::string_view const lhsIs = lhsView ? "view" : "index";
  std::string_view const rhsIs = lhsView ? "Index" : "View";
  VectorFst fst;
  FstBuilder builder{fst};
  for (auto const& field : lhs) {
    builder.add(field.first, true);
  }
  builder.finish();

  ExplicitMatcher matcher{&fst, fst::MATCH_INPUT};
  for (auto const& field : rhs) {
    auto const name = field.first;
    auto const prefix = findLongestCommonPrefix(matcher, name);
    auto it = find(lhs, prefix);
    if (it == lhs.end()) {
      TRI_ASSERT(prefix.empty());
      continue;
    }
    TRI_ASSERT(it->first == prefix);

    if (SameCollection ||
        it->second.isSearchField != field.second.isSearchField ||
        it->second.analyzer != field.second.analyzer) {
      if (it->first.size() == name.size()) {
        if (SameCollection) {
          return absl::StrCat("same field '", name, "', collection '");
        } else if (it->second.isSearchField != field.second.isSearchField) {
          return absl::StrCat(rhsIs, " field '", name, "' searchField '",
                              field.second.isSearchField, "' mismatches ",
                              lhsIs, " field searchField '",
                              it->second.isSearchField, "'");
        } else {
          return absl::StrCat(rhsIs, " field '", name, "' analyzer '",
                              field.second.analyzer, "' mismatches ", lhsIs,
                              " field analyzer '", it->second.analyzer, "'");
        }
      } else if (it->second.includeAllFields) {
        if (SameCollection) {
          return absl::StrCat("field '", name, "' and field '", it->first,
                              "' with includeAllFields, collection '");
        } else if (it->second.isSearchField != field.second.isSearchField) {
          return absl::StrCat(rhsIs, " field '", name, "' searchField '",
                              field.second.isSearchField, "' mismatches ",
                              lhsIs, " field '", it->first,
                              "' with includeAllFields searchField '",
                              it->second.isSearchField, "'");
        } else {
          return absl::StrCat(
              rhsIs, " field '", name, "' analyzer '", field.second.analyzer,
              "' mismatches ", lhsIs, " field '", it->first,
              "' with includeAllFields analyzer '", it->second.analyzer, "'");
        }
      }
    }
  }
  return {};
}

auto createSortedFields(IResearchInvertedIndexMeta const& index) {
  std::vector<std::pair<std::string, SearchMeta::Field>> fields;
  fields.reserve(index._fields.size() + index._includeAllFields);
  for (auto const& field : index._fields) {
    fields.emplace_back(
        field.path(),
        SearchMeta::Field{field.analyzer()._shortName, field._includeAllFields,
                          field._isSearchField});
  }
  if (index._includeAllFields) {
    fields.emplace_back("", SearchMeta::Field{index.analyzer()._shortName, true,
                                              index._isSearchField});
  }
  std::sort(fields.begin(), fields.end(), [](auto const& lhs, auto const& rhs) {
    return lhs.first < rhs.first;
  });
  return fields;
}

std::string checkFieldsSameCollection(SearchMeta::Map const& search,
                                      IResearchInvertedIndexMeta const& index) {
  auto const fields = createSortedFields(index);
  auto error = abstractCheckFields<true>(search, fields, true);
  if (error.empty()) {
    error = abstractCheckFields<true>(fields, search, false);
  }
  return error;
}

std::string checkFieldsDifferentCollections(
    SearchMeta::Map const& search, IResearchInvertedIndexMeta const& index) {
  auto const fields = createSortedFields(index);
  auto error = abstractCheckFields<false>(search, fields, true);
  if (error.empty()) {
    error = abstractCheckFields<false>(fields, search, false);
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

void add(SearchMeta::Map& search, IResearchInvertedIndexMeta const& index) {
  for (auto const& field : index._fields) {
    auto it = search.lower_bound(field.path());
    if (it == search.end() || it->first != field.path()) {
      search.emplace_hint(
          it, field.path(),
          SearchMeta::Field{field.analyzer()._shortName,
                            field._includeAllFields, field._isSearchField});
    } else {
      it->second.includeAllFields |= field._includeAllFields;
    }
  }
  if (index._includeAllFields) {
    search.emplace("", SearchMeta::Field{index.analyzer()._shortName, true,
                                         index._isSearchField});
  }
}

}  // namespace

struct MetaFst : VectorFst {};

std::shared_ptr<SearchMeta> SearchMeta::make() {
  return std::make_shared<SearchMeta>();
}

void SearchMeta::createFst() {
  auto fst = std::make_unique<MetaFst>();
  FstBuilder builder{*fst};
  for (auto const& field : fieldToAnalyzer) {
    builder.add(field.first, true);
  }
  builder.finish();
  _fst = std::move(fst);
}

MetaFst const* SearchMeta::getFst() const { return _fst.get(); }

AnalyzerProvider SearchMeta::createProvider(
    std::function<FieldMeta::Analyzer(std::string_view)> getAnalyzer) const {
  struct Field final {
    FieldMeta::Analyzer analyzer;
    bool includeAllFields;
  };
  containers::FlatHashMap<std::string_view, Field> analyzers;
  for (auto const& [name, field] : fieldToAnalyzer) {
    analyzers.emplace(
        name, Field{getAnalyzer(field.analyzer), field.includeAllFields});
  }
  VectorFst const* fst = getFst();
  TRI_ASSERT(fst);
  // we don't use provider in parallel, so create matcher here is thread safe
  auto matcher = std::make_unique<ExplicitMatcher>(fst, fst::MATCH_INPUT);
  return [analyzers = std::move(analyzers), matcher = std::move(matcher)](
             std::string_view field) mutable -> FieldMeta::Analyzer const& {
    auto it = analyzers.find(field);  // fast-path O(1)
    if (it != analyzers.end()) {
      return it->second.analyzer;
    }
    // Omega(prefix.size())
    auto const prefix = findLongestCommonPrefix(*matcher, field);
    it = analyzers.find(prefix);
    if (it != analyzers.end() && it->second.includeAllFields) {
      return it->second.analyzer;
    }
    return emptyAnalyzer();
  };
}

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
      if (auto index = handle->lock(); index) {
        indexes.push_back(std::move(index));
      }
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
        if (auto inverted = handle->lock(); inverted) {
          auto* index = dynamic_cast<Index*>(inverted.get());
          if (!index) {
            TRI_ASSERT(false);
            continue;
          }
          auto collection = resolver.getCollection(cid);
          if (!collection) {
            continue;
          }
          if (ctx == Serialization::Properties ||
              ctx == Serialization::Inventory) {
            build.add(velocypack::Value{velocypack::ValueType::Object});
            build.add("collection", velocypack::Value{collection->name()});
            build.add("index", velocypack::Value{index->name()});
          } else {
            build.add(velocypack::Value{velocypack::ValueType::Object});
            absl::AlphaNum collectionId{collection->id().id()};
            build.add("collection", velocypack::Value{collectionId.Piece()});
            absl::AlphaNum indexId{index->id().id()};
            build.add("index", velocypack::Value{indexId.Piece()});
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
    auto collectionSlice = value.get("collection");
    if (!collectionSlice.isString()) {
      return {TRI_ERROR_BAD_PARAMETER, "'index' should be a string"};
    }
    auto collection = resolver.getCollection(collectionSlice.stringView());
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
    auto indexSlice = value.get("index");
    if (!indexSlice.isString()) {
      return {TRI_ERROR_BAD_PARAMETER, "'index' should be a string"};
    }
    auto index = getIndex(*collection, indexSlice.stringView());
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

  auto searchMeta = SearchMeta::make();
  auto r = iterate(
      [&](auto const& indexMeta) {
        searchMeta->primarySort = indexMeta._sort;
        searchMeta->storedValues = indexMeta._storedValues;
      },
      [&](auto const& indexMeta) { return check(*searchMeta, indexMeta); });
  if (!r.ok()) {
    return r;
  }
  SearchMeta::Map merged;
  for (auto const& [_, handles] : _indexes) {
    if (handles.size() < 2) {
      continue;
    }
    bool first = true;
    for (auto const& handle : handles) {
      if (auto index = handle->lock(); index) {
        auto const& inverted =
            basics::downCast<IResearchInvertedIndex>(*index.get());
        auto const& indexMeta = inverted.meta();
        if (first) {
          add(merged, indexMeta);
          first = false;
        } else if (auto error = checkFieldsSameCollection(merged, indexMeta);
                   !error.empty()) {
          return {TRI_ERROR_BAD_PARAMETER,
                  absl::StrCat(
                      "You cannot add to view indexes to the same collection,"
                      " if them index the same fields. Error for: ",
                      error, inverted.collection().name(), "'")};
        } else {
          add(merged, indexMeta);
        }
      }
    }
    merged.clear();
  }
  // TODO(MBkkt) missed optimization: I check that inverted index not intersects
  // at all, so I can merge indexes meta to same collection without this check
  r = iterate([&](auto const& indexMeta) { add(merged, indexMeta); },
              [&](auto const& indexMeta) {
                auto error = checkFieldsDifferentCollections(merged, indexMeta);
                if (error.empty()) {
                  add(merged, indexMeta);
                }
                return error;
              });
  if (!r.ok()) {
    return r;
  }
  searchMeta->fieldToAnalyzer = std::move(merged);
  if (ServerState::instance()->isSingleServer()) {
    searchMeta->createFst();
  }  // else we don't create provider from this SearchMeta
  _meta = std::move(searchMeta);
  return r;
}

}  // namespace arangodb::iresearch

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
