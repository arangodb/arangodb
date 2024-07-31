////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "IResearchViewNode.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/IResearchViewExecutor.h"
#include "Aql/Executor/IResearchViewHeapSortExecutor.h"
#include "Aql/Executor/IResearchViewMergeExecutor.h"
#include "Aql/Executor/NoResultsExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SortCondition.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/DownCast.h"
#include "Basics/NumberUtils.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Containers/FlatHashSet.h"
#include "Containers/HashSet.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/Search.h"
#include "IResearch/ViewSnapshot.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include "utils/misc.hpp"

#include <absl/strings/str_cat.h>
#include <frozen/map.h>
#include <velocypack/Iterator.h>

namespace arangodb::iresearch {
namespace {

// Surrogate root for all queries without a filter
aql::AstNode const kAll{aql::AstNodeValue{true}};

// Helpers for std::vector<iresearch::IResearchSort>
void toVelocyPack(velocypack::Builder& builder,
                  std::vector<SearchFunc> const& scorers, bool verbose) {
  VPackArrayBuilder arrayScope(&builder);
  for (auto const& scorer : scorers) {
    VPackObjectBuilder objectScope(&builder);
    builder.add("id", VPackValue(scorer.var->id));
    builder.add("name", VPackValue(scorer.var->name));  // for explainer.js
    builder.add(VPackValue("node"));
    scorer.node->toVelocyPack(builder, verbose);
  }
}

std::vector<SearchFunc> fromVelocyPack(aql::ExecutionPlan& plan,
                                       velocypack::Slice slice) {
  if (!slice.isArray()) {
    LOG_TOPIC("b50b2", ERR, iresearch::TOPIC)
        << "invalid json format detected while building IResearchViewNode "
           "sorting from velocy pack, array expected";
    return {};
  }

  auto* ast = plan.getAst();
  TRI_ASSERT(ast);
  auto const* vars = plan.getAst()->variables();
  TRI_ASSERT(vars);
  velocypack::ArrayIterator scorersArray{slice};
  std::vector<SearchFunc> scorers;
  scorers.reserve(scorersArray.size());
  size_t i = 0;
  for (auto const sortSlice : scorersArray) {
    auto const varIdSlice = sortSlice.get("id");

    if (!varIdSlice.isNumber()) {
      LOG_TOPIC("c3790", ERR, iresearch::TOPIC)
          << "malformed variable identifier at line '" << i
          << "', number expected";
      return {};
    }

    auto const varId = varIdSlice.getNumber<aql::VariableId>();
    auto const* var = vars->getVariable(varId);

    if (!var) {
      LOG_TOPIC("4eeb9", ERR, iresearch::TOPIC)
          << "unable to find variable '" << varId << "' at line '" << i
          << "' while building IResearchViewNode sorting from velocy pack";
      return {};
    }

    // will be owned by Ast
    auto* node = ast->createNode(sortSlice.get("node"));
    scorers.emplace_back(var, node);
    ++i;
  }

  return scorers;
}

// Helpers for IResearchViewNode::Options
constexpr auto conditionOptimizationTypeMap =
    frozen::make_map<std::string_view, aql::ConditionOptimization>({
        {"auto", aql::ConditionOptimization::kAuto},
        {"nodnf", aql::ConditionOptimization::kNoDNF},
        {"noneg", aql::ConditionOptimization::kNoNegation},
        {"none", aql::ConditionOptimization::kNone},
    });

constexpr auto countApproximationTypeMap =
    frozen::make_map<std::string_view, iresearch::CountApproximate>({
        {"exact", iresearch::CountApproximate::Exact},
        {"cost", iresearch::CountApproximate::Cost},
    });

void toVelocyPack(velocypack::Builder& builder,
                  IResearchViewNode::Options const& options) {
  VPackObjectBuilder objectScope(&builder);
  builder.add("waitForSync", VPackValue(options.forceSync));

  for (auto const& r : conditionOptimizationTypeMap) {
    if (r.second == options.conditionOptimization) {
      builder.add("conditionOptimization", VPackValue(r.first));
      break;
    }
  }

  if (!options.restrictSources) {
    builder.add("collections", VPackValue(VPackValueType::Null));
  } else {
    VPackArrayBuilder arrayScope(&builder, "collections");
    for (auto const cid : options.sources) {
      builder.add(VPackValue(cid.id()));
    }
  }

  if (!options.noMaterialization) {
    builder.add("noMaterialization", VPackValue(options.noMaterialization));
  }

  if (options.countApproximate != CountApproximate::Exact) {
    // to be backward compatible - do not write default value
    for (auto const& r : countApproximationTypeMap) {
      if (r.second == options.countApproximate) {
        builder.add("countApproximate", VPackValue(r.first));
        break;
      }
    }
  }

  if (options.filterOptimization != iresearch::FilterOptimization::MAX) {
    builder.add("filterOptimization",
                VPackValue(static_cast<int64_t>(options.filterOptimization)));
  }

  if (options.parallelism != 1) {
    builder.add("parallelism", VPackValue(options.parallelism));
  }
}

bool fromVelocyPack(velocypack::Slice optionsSlice,
                    IResearchViewNode::Options& options) {
  if (optionsSlice.isNone()) {  // no options specified
    return true;
  }
  if (!optionsSlice.isObject()) {
    return false;
  }
  {  // forceSync
    auto const optionSlice = optionsSlice.get("waitForSync");
    if (!optionSlice.isNone()) {  // 'waitForSync' is optional
      if (!optionSlice.isBool()) {
        return false;
      }
      options.forceSync = optionSlice.getBool();
    }
  }
  {  // conditionOptimization
    auto const conditionOptimizationSlice =
        optionsSlice.get("conditionOptimization");
    if (!conditionOptimizationSlice.isNone() &&
        !conditionOptimizationSlice.isNull()) {
      if (!conditionOptimizationSlice.isString()) {
        return false;
      }
      auto type = conditionOptimizationSlice.stringView();
      auto conditionTypeIt = conditionOptimizationTypeMap.find(type);
      if (conditionTypeIt == conditionOptimizationTypeMap.end()) {
        return false;
      }
      options.conditionOptimization = conditionTypeIt->second;
    }
  }
  {  // collections
    auto const collectionsSlice = optionsSlice.get("collections");
    if (!collectionsSlice.isNone() && !collectionsSlice.isNull()) {
      if (!collectionsSlice.isArray()) {
        return false;
      }
      VPackArrayIterator collectionIt{collectionsSlice};
      options.sources.reserve(collectionIt.size());
      for (; collectionIt.valid(); ++collectionIt) {
        auto const idSlice = *collectionIt;
        if (!idSlice.isNumber()) {
          return false;
        }
        DataSourceId const cid{idSlice.getNumber<DataSourceId::BaseType>()};
        if (!cid) {
          return false;
        }
        options.sources.insert(cid);
      }
      options.restrictSources = true;
    }
  }
  {  // noMaterialization
    auto const noMaterializationSlice = optionsSlice.get("noMaterialization");
    if (!noMaterializationSlice.isNone()) {  // 'noMaterialization' is optional
      if (!noMaterializationSlice.isBool()) {
        return false;
      }
      options.noMaterialization = noMaterializationSlice.getBool();
    }
  }
  {  // countApproximate
    auto const countApproximateSlice = optionsSlice.get("countApproximate");
    if (!countApproximateSlice.isNone()) {  // 'countApproximate' is optional
      if (!countApproximateSlice.isString()) {
        return false;
      }
      auto type = countApproximateSlice.stringView();
      auto conditionTypeIt = countApproximationTypeMap.find(type);
      if (conditionTypeIt == countApproximationTypeMap.end()) {
        return false;
      }
      options.countApproximate = conditionTypeIt->second;
    }
  }
  {  // filterOptimization
    auto const filterOptimizationSlice = optionsSlice.get("filterOptimization");
    if (!filterOptimizationSlice.isNone()) {
      // 'filterOptimization' is optional. Missing means MAX
      if (!filterOptimizationSlice.isNumber()) {
        return false;
      }
      options.filterOptimization = static_cast<iresearch::FilterOptimization>(
          filterOptimizationSlice.getNumber<int>());
    }
  }
  {  // parallelism
    auto const parallelismSlice = optionsSlice.get("parallelism");
    if (!parallelismSlice.isNone()) {
      if (!parallelismSlice.isNumber<size_t>()) {
        return false;
      }
      options.parallelism = parallelismSlice.getNumber<size_t>();
    }
  }
  return true;
}

using OptionHandler = bool (*)(aql::QueryContext&, LogicalView const& view,
                               aql::AstNode const&, IResearchViewNode::Options&,
                               std::string&);

constexpr auto kHandlers = frozen::make_map<std::string_view, OptionHandler>(
    {{"collections",
      [](aql::QueryContext& query, LogicalView const& view,
         aql::AstNode const& value, IResearchViewNode::Options& options,
         std::string& error) {
        if (value.isNullValue()) {  // have nothing to restrict
          return true;
        }
        if (!value.isArray()) {
          error =
              "null value or array of strings or numbers is expected for "
              "option 'collections'";
          return false;
        }
        auto& resolver = query.resolver();
        containers::FlatHashSet<DataSourceId> sources;
        auto const n = value.numMembers();
        sources.reserve(n);
        // get list of CIDs for restricted collections
        for (size_t i = 0; i != n; ++i) {
          auto const* sub = value.getMemberUnchecked(i);
          TRI_ASSERT(sub);

          switch (sub->value.type) {
            case aql::VALUE_TYPE_INT: {
              sources.emplace(
                  static_cast<DataSourceId::BaseType>(sub->getIntValue(true)));
            } break;
            case aql::VALUE_TYPE_STRING: {
              auto name = sub->getString();
              auto collection = resolver.getCollection(name);
              if (!collection) {
                // check if DataSourceId is passed as string
                DataSourceId const cid{
                    NumberUtils::atoi_zero<DataSourceId::BaseType>(
                        name.data(), name.data() + name.size())};
                collection = resolver.getCollection(cid);
                if (!collection) {
                  error = "invalid data source name '" + name +
                          "' while parsing option 'collections'";
                  return false;
                }
              }
              sources.emplace(collection->id());
            } break;
            default: {
              error =
                  "null value or array of strings or numbers"
                  " is expected for option 'collections'";
              return false;
            }
          }
        }
        // check if CIDs are valid
        size_t sourcesFound = 0;
        auto checkCids = [&sources, &sourcesFound](DataSourceId cid,
                                                   LogicalView::Indexes*) {
          sourcesFound += size_t(sources.contains(cid));
          return true;
        };
        view.visitCollections(checkCids);
        if (sourcesFound != sources.size()) {
          error =
              absl::StrCat("only ", sourcesFound, " out of ", sources.size(),
                           " provided collection(s) in option 'collections' "
                           "are registered with the view '",
                           view.name(), "'");
          return false;
        }
        // parsing is done
        options.sources = std::move(sources);
        options.restrictSources = true;
        return true;
      }},
     {"waitForSync",
      [](aql::QueryContext& /*query*/, LogicalView const& /*view*/,
         aql::AstNode const& value, IResearchViewNode::Options& options,
         std::string& error) {
        if (!value.isValueType(aql::VALUE_TYPE_BOOL)) {
          error = "boolean value expected for option 'waitForSync'";
          return false;
        }
        options.forceSync = value.getBoolValue();
        return true;
      }},
     {"noMaterialization",
      [](aql::QueryContext& /*query*/, LogicalView const& /*view*/,
         aql::AstNode const& value, IResearchViewNode::Options& options,
         std::string& error) {
        if (!value.isValueType(aql::VALUE_TYPE_BOOL)) {
          error = "boolean value expected for option 'noMaterialization'";
          return false;
        }

        options.noMaterialization = value.getBoolValue();
        return true;
      }},
     {"countApproximate",
      [](aql::QueryContext& /*query*/, LogicalView const& /*view*/,
         aql::AstNode const& value, IResearchViewNode::Options& options,
         std::string& error) {
        if (!value.isValueType(aql::VALUE_TYPE_STRING)) {
          error = "string value expected for option 'countApproximate'";
          return false;
        }
        auto type = value.getString();
        auto countTypeIt = countApproximationTypeMap.find(type);
        if (countTypeIt == countApproximationTypeMap.end()) {
          error = "unknown value '" + type + "' for option 'countApproximate'";
          return false;
        }
        options.countApproximate = countTypeIt->second;
        return true;
      }},
     {"conditionOptimization",
      [](aql::QueryContext& /*query*/, LogicalView const& /*view*/,
         aql::AstNode const& value, IResearchViewNode::Options& options,
         std::string& error) {
        if (!value.isValueType(aql::VALUE_TYPE_STRING)) {
          error = "string value expected for option 'conditionOptimization'";
          return false;
        }
        auto type = value.getString();
        auto conditionTypeIt = conditionOptimizationTypeMap.find(type);
        if (conditionTypeIt == conditionOptimizationTypeMap.end()) {
          error = absl::StrCat("unknown value '", type,
                               "' for option 'conditionOptimization'");
          return false;
        }
        options.conditionOptimization = conditionTypeIt->second;
        return true;
      }},
     {"filterOptimization",
      [](aql::QueryContext& /*query*/, LogicalView const& /*view*/,
         aql::AstNode const& value, IResearchViewNode::Options& options,
         std::string& error) {
        if (!value.isValueType(aql::VALUE_TYPE_INT)) {
          error = "int value expected for option 'filterOptimization'";
          return false;
        }
        options.filterOptimization =
            static_cast<iresearch::FilterOptimization>(value.getIntValue());
        return true;
      }},
     {"parallelism",
      [](aql::QueryContext& /*query*/, LogicalView const& /*view*/,
         aql::AstNode const& value, IResearchViewNode::Options& options,
         std::string& error) {
        if (!value.isValueType(aql::VALUE_TYPE_INT)) {
          error = "int value expected for option 'parallelism'";
          return false;
        }
        auto intValue = value.getIntValue();
        if (intValue <= 0) {
          error = "positive value expected for option 'parallelism'";
          return false;
        }
        options.parallelism = static_cast<size_t>(intValue);
        return true;
      }}});

bool parseOptions(aql::QueryContext& query, LogicalView const& view,
                  aql::AstNode const* optionsNode,
                  IResearchViewNode::Options& options, std::string& error) {
  if (!optionsNode) {  // nothing to parse
    return true;
  }
  if (aql::NODE_TYPE_OBJECT != optionsNode->type) {  // must be an object
    return false;
  }
  size_t const n = optionsNode->numMembers();
  for (size_t i = 0; i != n; ++i) {
    auto const* attribute = optionsNode->getMemberUnchecked(i);
    if (!attribute || attribute->type != aql::NODE_TYPE_OBJECT_ELEMENT ||
        !attribute->isValueType(aql::VALUE_TYPE_STRING) ||
        !attribute->numMembers()) {
      // invalid or malformed node detected
      return false;
    }
    auto const attributeName = std::string_view{attribute->getStringValue(),
                                                attribute->getStringLength()};
    auto const handler = kHandlers.find(attributeName);
    if (handler == kHandlers.end()) {  // no handler found for attribute
      aql::ExecutionPlan::invalidOptionAttribute(query, "unknown", "FOR",
                                                 attributeName);
      continue;
    }
    auto const* value = attribute->getMemberUnchecked(0);
    if (!value) {  // can't handle attribute
      return false;
    }
    if (!value->isConstant()) {
      // 'Ast::injectBindParameters` doesn't handle dynamic of parent nodes
      // correctly, re-evaluate flags
      value->removeFlag(aql::DETERMINED_CONSTANT);
      if (!value->isConstant()) {  // can't handle dynamic values in options
        return false;
      }
    }
    if (!handler->second(query, view, *value, options, error)) {
      return false;  // can't handle attribute
    }
  }
  return true;
}

/// negative value - value is dirty
/// _volatilityMask & 1 == volatile filter
/// _volatilityMask & 2 == volatile sort
int evaluateVolatility(IResearchViewNode const& node) {
  auto const inDependentScope = isInInnerLoopOrSubquery(node);
  auto const& plan = *node.plan();
  auto const& outVariable = node.outVariable();
  aql::VarSet vars;
  int mask = 0;
  // evaluate filter condition volatility
  auto& filterCondition = node.filterCondition();
  if (!isFilterConditionEmpty(&filterCondition) && inDependentScope) {
    irs::set_bit<0>(hasDependencies(plan, filterCondition, outVariable, vars,
                                    [](aql::Variable const*) { return true; }),
                    mask);
  }
  // evaluate sort condition volatility
  auto& scorers = node.scorers();
  if (!scorers.empty() && inDependentScope) {
    vars.clear();
    for (auto const& scorer : scorers) {
      if (hasDependencies(plan, *scorer.node, outVariable, vars,
                          [](aql::Variable const*) { return true; })) {
        irs::set_bit<1>(mask);
        break;
      }
    }
  }
  return mask;
}

LogicalView::CollectionVisitor const sEmptyView{
    [](DataSourceId, LogicalView::Indexes*) noexcept { return false; }};

// Since cluster is not transactional and each distributed
//        part of a query starts it's own transaction associated
//        with global query identifier, there is no single place
//        to store a snapshot and we do the following:
//
//   1. Each query part on DB server gets the list of shards
//      to be included into a query and starts its own transaction
//
//   2. Given the list of shards we take view snapshot according
//      to the list of restricted data sources specified in options
//      of corresponding IResearchViewNode
//
//   3. If waitForSync is specified, we refresh snapshot
//      of each shard we need and finally put it to transaction
//      associated to a part of the distributed query. We use
//      default snapshot key if there are no restricted sources
//      specified in options or IResearchViewNode address otherwise
//
//      Custom key is needed for the following query
//      (assume 'view' is lined with 'c1' and 'c2' in the example below):
//           FOR d IN view OPTIONS { collections : [ 'c1' ] }
//           FOR x IN view OPTIONS { collections : [ 'c2' ] }
//           RETURN {d, x}
//
ViewSnapshotPtr snapshotDBServer(IResearchViewNode const& node,
                                 transaction::Methods& trx) {
  TRI_ASSERT(ServerState::instance()->isDBServer());
  auto const view = node.view();
  auto const& options = node.options();
  auto* const key = node.getSnapshotKey();
  auto* snapshot = getViewSnapshot(trx, key);
  if (snapshot != nullptr) {
    if (options.forceSync) {
      syncViewSnapshot(*snapshot, view ? view->name() : "");
    }
    return {ViewSnapshotPtr{}, snapshot};
  }
  auto const viewImpl = basics::downCast<IResearchView>(view);
  auto* resolver = trx.resolver();
  TRI_ASSERT(resolver);
  auto const& shards = node.shards();
  ViewSnapshot::Links links;
  links.reserve(shards.size());
  // TODO fu2::function_view
  std::function<void(ViewSnapshot::Links&, LogicalCollection const&,
                     LogicalView::Indexes const&)>
      linksLock;
  auto searchLinksLock = [&](ViewSnapshot::Links& links,
                             LogicalCollection const& collection,
                             LogicalView::Indexes const& indexes) {
    for (auto indexId : indexes) {
      auto index = std::dynamic_pointer_cast<IResearchInvertedIndex>(
          collection.lookupIndex(indexId));
      if (index) {
        links.emplace_back(index->self()->lock());
      }
    }
  };
  std::shared_lock<boost::upgrade_mutex> viewLock;
  auto viewLinksLock = [&](ViewSnapshot::Links& links,
                           LogicalCollection const& collection,
                           LogicalView::Indexes const&) {
    links.emplace_back(viewImpl->linkLock(viewLock, collection.id()));
  };
  if (viewImpl) {
    viewLock = viewImpl->linksReadLock();
    linksLock = viewLinksLock;
  } else {
    linksLock = searchLinksLock;
  }
  for (auto const& [shard, indexes] : shards) {
    auto const& collection = resolver->getCollection(std::string{shard});
    if (!collection) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          absl::StrCat("failed to find shard by id '", std::string{shard},
                       "'"));
    }
    if (options.restrictSources &&
        !options.sources.contains(collection->planId())) {
      // TODO(MBkkt) Maybe we want check sources on Coordinator?
      continue;  // skip restricted collections if any
    }
    linksLock(links, *collection, indexes);
  }
  return {ViewSnapshotPtr{},
          makeViewSnapshot(trx, key, options.forceSync,
                           view ? view->name() : "", std::move(links))};
}

// Since single-server is transactional we do the following:
//
//   1. When transaction starts we put index snapshot into it
//
//   2. If waitForSync is specified, we refresh snapshot
//      taken in (1), object itself remains valid
//
//   3. If there are no restricted sources in a query, we reuse
//      snapshot taken in (1),
//      otherwise we reassemble restricted snapshot based on the
//      original one taken in (1) and return it
//
ViewSnapshotPtr snapshotSingleServer(IResearchViewNode const& node,
                                     transaction::Methods& trx) {
  TRI_ASSERT(ServerState::instance()->isSingleServer());
  auto const& view = *node.view();
  auto const& options = node.options();
  auto* const key = node.getSnapshotKey();
  auto* snapshot = getViewSnapshot(trx, key);
  if (snapshot != nullptr) {
    if (options.forceSync) {
      syncViewSnapshot(*snapshot, view.name());
    }
    // if (options.restrictSources) {
    //  return std::make_shared<ViewSnapshotView>(*snapshot, options.sources);
    //}
    return {ViewSnapshotPtr{}, snapshot};
  }

  // TODO: remove here restrictSources condition when switching back to
  // ViewSnapshotView
  if (!options.restrictSources && !options.forceSync &&
      trx.isMainTransaction()) {
    return {};
  }
  auto links = [&] {
    if (view.type() == ViewType::kArangoSearch) {
      auto const& viewImpl = basics::downCast<IResearchView>(view);
      return viewImpl.getLinks(options.restrictSources ? &options.sources
                                                       : nullptr);
    } else {
      auto const& viewImpl = basics::downCast<Search>(view);
      return viewImpl.getLinks(options.restrictSources ? &options.sources
                                                       : nullptr);
    }
  }();
  snapshot = makeViewSnapshot(trx, key, options.forceSync, view.name(),
                              std::move(links));
  // if (options.restrictSources && snapshot != nullptr) {
  //  return std::make_shared<ViewSnapshotView>(*snapshot, options.sources);
  //}
  return {ViewSnapshotPtr{}, snapshot};
}

std::shared_ptr<SearchMeta const> getMeta(
    std::shared_ptr<LogicalView const> const& view) {
  if (!view || view->type() != ViewType::kSearchAlias) {
    return {};
  }
  return basics::downCast<Search>(*view).meta();
}

#ifdef USE_ENTERPRISE

IResearchOptimizeTopK const& optimizeTopK(
    std::shared_ptr<SearchMeta const> const& meta,
    std::shared_ptr<LogicalView const> const& view) {
  if (meta) {
    TRI_ASSERT(!view || view->type() == ViewType::kSearchAlias);
    return meta->optimizeTopK;
  }
  TRI_ASSERT(view);
  TRI_ASSERT(view->type() == ViewType::kArangoSearch);
  if (ServerState::instance()->isCoordinator()) {
    auto const& viewImpl = basics::downCast<IResearchViewCoordinator>(*view);
    return viewImpl.meta()._optimizeTopK;
  }
  auto const& viewImpl = basics::downCast<IResearchView>(*view);
  return viewImpl.meta()._optimizeTopK;
}

#endif

IResearchSortBase const& primarySort(
    std::shared_ptr<SearchMeta const> const& meta,
    std::shared_ptr<LogicalView const> const& view) {
  if (meta) {
    TRI_ASSERT(!view || view->type() == ViewType::kSearchAlias);
    return meta->primarySort;
  }
  TRI_ASSERT(view);
  TRI_ASSERT(view->type() == ViewType::kArangoSearch);
  if (ServerState::instance()->isCoordinator()) {
    auto const& viewImpl = basics::downCast<IResearchViewCoordinator>(*view);
    return viewImpl.primarySort();
  }
  auto const& viewImpl = basics::downCast<IResearchView>(*view);
  return viewImpl.primarySort();
}

IResearchViewStoredValues const& storedValues(
    std::shared_ptr<SearchMeta const> const& meta,
    std::shared_ptr<LogicalView const> const& view) {
  if (meta) {
    TRI_ASSERT(!view || view->type() == ViewType::kSearchAlias);
    return meta->storedValues;
  }
  TRI_ASSERT(view);
  TRI_ASSERT(view->type() == ViewType::kArangoSearch);
  if (ServerState::instance()->isCoordinator()) {
    auto const& viewImpl = basics::downCast<IResearchViewCoordinator>(*view);
    return viewImpl.storedValues();
  }
  auto const& viewImpl = basics::downCast<IResearchView>(*view);
  return viewImpl.storedValues();
}

char const* kNodeViewNameParam = "view";
char const* kNodeViewIdParam = "viewId";
char const* kNodeOutVariableParam = "outVariable";
char const* kNodeOutSearchDocParam = "outSearchDocId";
char const* kNodeOutNmDocParam = "outNmDocId";
char const* kNodeConditionParam = "condition";
char const* kNodeScorersParam = "scorers";
char const* kNodeShardsParam = "shards";
char const* kNodeIndexesParam = "indexes";
char const* kNodeOptionsParam = "options";
char const* kNodeVolatilityParam = "volatility";
char const* kNodePrimarySortBucketsParam = "primarySortBuckets";
char const* kNodeViewValuesVars = "viewValuesVars";
char const* kNodeViewStoredValuesVars = "viewStoredValuesVars";
char const* kNodeViewValuesVarColumnNumber = "columnNumber";
char const* kNodeViewValuesVarFieldNumber = "fieldNumber";
char const* kNodeViewValuesVarId = "id";
char const* kNodeViewValuesVarName = "name";
char const* kNodeViewValuesVarField = "field";
char const* kNodeViewNoMaterialization = "noMaterialization";
char const* kNodeViewScorersSort = "scorersSort";
char const* kNodeViewScorersSortIndex = "index";
char const* kNodeViewScorersSortAsc = "asc";
char const* kNodeViewScorersSortLimit = "scorersSortLimit";
char const* kNodeViewHeapSortLimit = "heapSortLimit";
char const* kNodeViewHeapSort = "heapSort";
char const* kNodeViewHeapSortField = "field";
char const* kNodeViewHeapSortSource = "source";
char const* kNodeViewHeapSortPostfix = "postfix";
char const* kNodeViewMetaFields = "metaFields";
char const* kNodeViewMetaSort = "metaSort";
char const* kNodeViewMetaStored = "metaStored";
#ifdef USE_ENTERPRISE
char const* kNodeViewMetaTopK = "metaTopK";
#endif
// it serialization optimized for transfer 0 and uint32_t::max
char const* kNodeViewImmutableParts = "immutableParts";

void toVelocyPack(velocypack::Builder& node, SearchMeta const& meta,
                  bool needSort, [[maybe_unused]] bool needHeapSort) {
  if (needSort) {
    VPackObjectBuilder objectScope{&node, kNodeViewMetaSort};
    [[maybe_unused]] bool const result = meta.primarySort.toVelocyPack(node);
    TRI_ASSERT(result);
  }
#ifdef USE_ENTERPRISE
  if (needHeapSort) {
    VPackArrayBuilder arrayScope{&node, kNodeViewMetaTopK};
    [[maybe_unused]] bool const result = meta.optimizeTopK.toVelocyPack(node);
    TRI_ASSERT(result);
  }
#endif
  {
    VPackArrayBuilder arrayScope{&node, kNodeViewMetaStored};
    [[maybe_unused]] bool const result = meta.storedValues.toVelocyPack(node);
    TRI_ASSERT(result);
  }
  {
    VPackArrayBuilder arrayScope{&node, kNodeViewMetaFields};
    for (auto const& [name, field] : meta.fieldToAnalyzer) {
      node.add(velocypack::Value{name});
      node.add(velocypack::Value{field.analyzer});
      node.add(velocypack::Value{field.includeAllFields});
    }
  }
}

void fromVelocyPack(velocypack::Slice node, SearchMeta& meta) {
  std::string error;
  auto slice = node.get(kNodeViewMetaSort);
  auto checkError = [&](std::string_view key) {
    if (!error.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("Failed to parse 'SearchMeta' ", key, ":",
                       slice.toString(), ", error: ", error));
    }
  };
  if (!slice.isNone()) {
    meta.primarySort.fromVelocyPack(slice, error);
    checkError(kNodeViewMetaSort);
  }

#ifdef USE_ENTERPRISE
  slice = node.get(kNodeViewMetaTopK);
  if (!slice.isNone()) {
    meta.optimizeTopK.fromVelocyPack(slice, error);
    checkError(kNodeViewMetaTopK);
  }
#endif

  slice = node.get(kNodeViewMetaStored);
  meta.storedValues.fromVelocyPack(slice, error);
  checkError(kNodeViewMetaStored);

  slice = node.get(kNodeViewMetaFields);
  if (!slice.isArray() || slice.length() % 3 != 0) {
    error = "should be an array and its length must be a multiple of 3";
    checkError(kNodeViewMetaFields);
  }
  velocypack::Slice value;
  for (velocypack::ArrayIterator it{slice}; it.valid();) {
    value = it.value();
    if (!value.isString()) {
      error = "field name must be a string";
      checkError(kNodeViewMetaFields);
    }
    auto field = value.stringView();
    ++it;

    value = it.value();
    if (!value.isString()) {
      error = "analyzer name must be a string";
      checkError(kNodeViewMetaFields);
    }
    auto analyzer = value.stringView();
    ++it;

    value = it.value();
    if (!value.isBool()) {
      error = "includeAllFields must be a boolean value";
      checkError(kNodeViewMetaFields);
    }
    auto includeAllFields = value.getBool();
    ++it;

    meta.fieldToAnalyzer.emplace(
        field, SearchMeta::Field{std::string{analyzer}, includeAllFields});
  }
}

void addViewValuesVar(VPackBuilder& nodes, std::string& fieldName,
                      IResearchViewNode::ViewVariable const& fieldVar) {
  nodes.add(kNodeViewValuesVarFieldNumber, VPackValue(fieldVar.fieldNum));
  nodes.add(kNodeViewValuesVarId, VPackValue(fieldVar.var->id));
  nodes.add(kNodeViewValuesVarName,
            VPackValue(fieldVar.var->name));  // for explainer.js
  nodes.add(kNodeViewValuesVarField,
            VPackValue(fieldName));  // for explainer.js
}

void extractViewValuesVar(aql::VariableGenerator const* vars,
                          IResearchViewNode::ViewValuesVars& viewValuesVars,
                          ptrdiff_t const columnNumber,
                          velocypack::Slice fieldVar) {
  auto const fieldNumberSlice = fieldVar.get(kNodeViewValuesVarFieldNumber);
  if (!fieldNumberSlice.isNumber<size_t>()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("'viewValuesVars[*].fieldNumber' ",
                     fieldNumberSlice.toString(), " should be a number"));
  }
  auto const fieldNumber = fieldNumberSlice.getNumber<size_t>();
  auto const varIdSlice = fieldVar.get(kNodeViewValuesVarId);
  if (!varIdSlice.isNumber<aql::VariableId>()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("'viewValuesVars[*].id' variable id ",
                     varIdSlice.toString(), " should be a number"));
  }
  auto const varId = varIdSlice.getNumber<aql::VariableId>();
  auto const* var = vars->getVariable(varId);
  if (!var) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("'viewValuesVars[*].id' unable to find variable by id ",
                     varId));
  }
  viewValuesVars[columnNumber].emplace_back(
      IResearchViewNode::ViewVariable{fieldNumber, var});
}

using Executor = std::unique_ptr<aql::ExecutionBlock> (*)(
    aql::ExecutionEngine*, IResearchViewNode const*, aql::RegisterInfos&&,
    aql::IResearchViewExecutorInfos&&);

template<bool copyStored, MaterializeType materializeType>
constexpr Executor kExecutors[] = {
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<aql::ExecutionBlockImpl<
          aql::IResearchViewExecutor<aql::ExecutionTraits<
              copyStored, false, false, materializeType>>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<
          aql::ExecutionBlockImpl<aql::IResearchViewExecutor<
              aql::ExecutionTraits<copyStored, true, false, materializeType>>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<aql::ExecutionBlockImpl<
          aql::IResearchViewMergeExecutor<aql::ExecutionTraits<
              copyStored, false, false, materializeType>>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<
          aql::ExecutionBlockImpl<aql::IResearchViewMergeExecutor<
              aql::ExecutionTraits<copyStored, true, false, materializeType>>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      if constexpr ((materializeType & MaterializeType::LateMaterialize) ==
                    MaterializeType::Undefined) {
        return std::make_unique<aql::ExecutionBlockImpl<
            aql::IResearchViewHeapSortExecutor<aql::ExecutionTraits<
                copyStored, false, false, materializeType>>>>(
            engine, viewNode, std::move(registerInfos),
            std::move(executorInfos));
      }
      TRI_ASSERT(false);
      return nullptr;
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      if constexpr ((materializeType & MaterializeType::LateMaterialize) ==
                    iresearch::MaterializeType::Undefined) {
        return std::make_unique<aql::ExecutionBlockImpl<
            aql::IResearchViewHeapSortExecutor<aql::ExecutionTraits<
                copyStored, true, false, materializeType>>>>(
            engine, viewNode, std::move(registerInfos),
            std::move(executorInfos));
      }
      TRI_ASSERT(false);
      return nullptr;
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<
          aql::ExecutionBlockImpl<aql::IResearchViewExecutor<
              aql::ExecutionTraits<copyStored, false, true, materializeType>>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<
          aql::ExecutionBlockImpl<aql::IResearchViewExecutor<
              aql::ExecutionTraits<copyStored, true, true, materializeType>>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<
          aql::ExecutionBlockImpl<aql::IResearchViewMergeExecutor<
              aql::ExecutionTraits<copyStored, false, true, materializeType>>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<
          aql::ExecutionBlockImpl<aql::IResearchViewMergeExecutor<
              aql::ExecutionTraits<copyStored, true, true, materializeType>>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      if constexpr ((materializeType & MaterializeType::LateMaterialize) ==
                    iresearch::MaterializeType::Undefined) {
        return std::make_unique<aql::ExecutionBlockImpl<
            aql::IResearchViewHeapSortExecutor<aql::ExecutionTraits<
                copyStored, false, true, materializeType>>>>(
            engine, viewNode, std::move(registerInfos),
            std::move(executorInfos));
      }
      TRI_ASSERT(false);
      return nullptr;
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      if constexpr ((materializeType & MaterializeType::LateMaterialize) ==
                    iresearch::MaterializeType::Undefined) {
        return std::make_unique<aql::ExecutionBlockImpl<
            aql::IResearchViewHeapSortExecutor<aql::ExecutionTraits<
                copyStored, true, true, materializeType>>>>(
            engine, viewNode, std::move(registerInfos),
            std::move(executorInfos));
      }
      TRI_ASSERT(false);
      return nullptr;
    }};

constexpr size_t getExecutorIndex(bool sorted, bool ordered, bool heapsort,
                                  bool emitSearchDoc) {
  TRI_ASSERT(!sorted || !heapsort);
  auto const index = size_t{ordered} + 2 * size_t{sorted} +
                     4 * size_t{heapsort} + 6 * size_t{emitSearchDoc};
  TRI_ASSERT(index <
             std::size((kExecutors<false, MaterializeType::Materialize>)));
  return index;
}

}  // namespace

bool isFilterConditionEmpty(aql::AstNode const* filterCondition) noexcept {
  return filterCondition == &kAll;
}

bool isInInnerLoopOrSubquery(aql::ExecutionNode const& node) {
  auto* cur = &node;
  while (true) {
    auto const* dep = cur->getFirstDependency();
    if (!dep) {
      break;
    }
    switch (dep->getType()) {
      case aql::ExecutionNode::ENUMERATE_COLLECTION:
      case aql::ExecutionNode::INDEX:
      case aql::ExecutionNode::JOIN:
      case aql::ExecutionNode::TRAVERSAL:
      case aql::ExecutionNode::ENUMERATE_LIST:
      case aql::ExecutionNode::SHORTEST_PATH:
      case aql::ExecutionNode::ENUMERATE_PATHS:
      case aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
        // we're in a loop
        return true;
      default:
        break;
    }
    cur = dep;
  }
  TRI_ASSERT(cur);
  // SINGLETON nodes in subqueries have id != 1
  return cur->getType() == aql::ExecutionNode::SINGLETON &&
         cur->id() != aql::ExecutionNodeId{1};
}

bool hasDependencies(aql::ExecutionPlan const& plan, aql::AstNode const& node,
                     aql::Variable const& ref, aql::VarSet& vars,
                     std::function<bool(aql::Variable const*)> callback) {
  vars.clear();
  aql::Ast::getReferencedVariables(&node, vars);
  vars.erase(&ref);  // remove "our" variable
  for (auto const* var : vars) {
    auto* setter = plan.getVarSetBy(var->id);
    if (!setter) {
      // unable to find setter
      continue;
    }
    switch (setter->getType()) {
      case aql::ExecutionNode::ENUMERATE_COLLECTION:
      case aql::ExecutionNode::ENUMERATE_LIST:
      case aql::ExecutionNode::SUBQUERY:
      case aql::ExecutionNode::SUBQUERY_END:
      case aql::ExecutionNode::COLLECT:
      case aql::ExecutionNode::TRAVERSAL:
      case aql::ExecutionNode::INDEX:
      case aql::ExecutionNode::JOIN:
      case aql::ExecutionNode::SHORTEST_PATH:
      case aql::ExecutionNode::ENUMERATE_PATHS:
      case aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
        // we're in the loop with dependent context
        if (callback(var)) {
          return true;
        }
        break;
      default:
        if ((!setter->isDeterministic() || setter->getLoop() != nullptr) &&
            callback(var)) {
          return true;
        }
        break;
    }
  }
  return false;
}

IResearchViewNode* IResearchViewNode::getByVar(
    aql::ExecutionPlan const& plan, aql::Variable const& var) noexcept {
  auto* varOwner = plan.getVarSetBy(var.id);

  if (!varOwner ||
      aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW != varOwner->getType()) {
    return nullptr;
  }

  return aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
      varOwner);
}

IResearchViewNode::IResearchViewNode(
    aql::ExecutionPlan& plan, aql::ExecutionNodeId id, TRI_vocbase_t& vocbase,
    std::shared_ptr<LogicalView const> view, aql::Variable const& outVariable,
    aql::AstNode* filterCondition, aql::AstNode* options,
    std::vector<SearchFunc>&& scorers)
    : aql::ExecutionNode{&plan, id},
      _vocbase{vocbase},
      _view{std::move(view)},
      _meta{getMeta(_view)},
      _outVariable{&outVariable},
      // in case if filter is not specified
      // set it to surrogate 'RETURN ALL' node
      _filterCondition{filterCondition ? filterCondition : &kAll},
      _scorers{std::move(scorers)} {
  if (!(ServerState::instance()->isSingleServer() ||
        ServerState::instance()->isCoordinator())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "IResearchViewNode should be constructed on "
                                   "Coordinator or SingleServer");
  }
  auto* ast = plan.getAst();
  // FIXME any other way to validate options before object creation???
  std::string error;
  TRI_ASSERT(_view);
  TRI_ASSERT(_meta || _view->type() != ViewType::kSearchAlias);
  _options.parallelism = ast->query()
                             .vocbase()
                             .server()
                             .getFeature<IResearchFeature>()
                             .defaultParallelism();
  if (!parseOptions(ast->query(), *_view, options, _options, error)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "invalid ArangoSearch options provided: " + error);
  }
}

IResearchViewNode::IResearchViewNode(aql::ExecutionPlan& plan,
                                     velocypack::Slice base)
    : aql::ExecutionNode{&plan, base},
      _vocbase{plan.getAst()->query().vocbase()},
      _outSearchDocId{aql::Variable::varFromVPack(
          plan.getAst(), base, kNodeOutSearchDocParam, true)},
      _outVariable{aql::Variable::varFromVPack(plan.getAst(), base,
                                               kNodeOutVariableParam)},
      _outNonMaterializedDocId{aql::Variable::varFromVPack(
          plan.getAst(), base, kNodeOutNmDocParam, true)},
      // in case if filter is not specified
      // set it to surrogate 'RETURN ALL' node
      _filterCondition{&kAll},
      _scorers{fromVelocyPack(plan, base.get(kNodeScorersParam))} {
  auto const viewNameSlice = base.get(kNodeViewNameParam);
  auto const viewName =
      viewNameSlice.isNone() ? "" : viewNameSlice.stringView();
  auto const viewIdSlice = base.get(kNodeViewIdParam);

  if (base.hasKey("outNmColPtr")) {
    // Old coordinator tries to run query on
    // new dbserver. Registers are not compatible.
    // We must abort.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Incompatible View node parameters. This "
                                   "may happen if rolling upgrade is running.");
  }
  if (viewIdSlice.isNone()) {  // handle search-alias view
    auto meta = SearchMeta::make();
    fromVelocyPack(base, *meta);
    meta->createFst();
    _meta = std::move(meta);
  } else if (!viewIdSlice.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("invalid vpack format, '", kNodeViewIdParam,
                     "' attribute is intended to be a string"));
  } else {  // handle arangosearch view
    auto const viewIdOrName = viewIdSlice.stringView();
    if (ServerState::instance()->isSingleServer()) {
      uint64_t viewId = 0;
      if (absl::SimpleAtoi(viewIdOrName, &viewId)) {
        _view = _vocbase.lookupView(DataSourceId{viewId});
      }
    } else {
      // need cluster wide view
      auto& ci = _vocbase.server().getFeature<ClusterFeature>().clusterInfo();
      _view = ci.getView(_vocbase.name(), viewIdOrName);
    }
    if (!_view) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          absl::StrCat("unable to find ArangoSearch view with id '",
                       viewIdOrName, "'"));
    }
  }
  // filter condition
  auto const conditionSlice = base.get(kNodeConditionParam);
  if (conditionSlice.isObject() && !conditionSlice.isEmptyObject()) {
    // AST will own the node
    setFilterCondition(plan.getAst()->createNode(conditionSlice));
  }
  // shards
  auto const shardsSlice = base.get(kNodeShardsParam);
  if (shardsSlice.isArray()) {
    TRI_ASSERT(plan.getAst());
    auto const& collections = plan.getAst()->query().collections();
    velocypack::ArrayIterator shardIt{shardsSlice};
    for (; shardIt.valid(); ++shardIt) {
      auto const shardId = (*shardIt).stringView();
      auto const* shard = collections.get(shardId);
      if (!shard) {
        LOG_TOPIC("6fba2", ERR, iresearch::TOPIC)
            << "unable to lookup shard '" << shardId << "' for the view '"
            << viewName << "'";
        continue;
      }
      _shards[ShardID{shard->name()}];
    }
    if (_meta) {  // handle search-alias view
      auto const indexesSlice = base.get(kNodeIndexesParam);
      if (!indexesSlice.isArray() || indexesSlice.length() % 2 != 0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "invalid 'IResearchViewNode' json format: unable to find 'indexes' "
            "even array");
      }
      velocypack::ArrayIterator indexIt{indexesSlice};
      for (; indexIt.valid(); ++indexIt) {
        auto const indexId = (*indexIt).getNumber<uint64_t>();
        auto const shardIndex = (*++indexIt).getNumber<uint64_t>();
        auto const shardName = shardsSlice.at(shardIndex).stringView();
        auto const* shard = collections.get(shardName);
        if (!shard) {
          continue;
        }
        _shards[ShardID{shard->name()}].emplace_back(indexId);
      }
    }
  } else {
    LOG_TOPIC("a48f3", ERR, iresearch::TOPIC)
        << "invalid 'IResearchViewNode' json format: unable to find 'shards' "
           "array";
  }
  // options
  TRI_ASSERT(plan.getAst());
  auto const options = base.get(kNodeOptionsParam);
  if (!fromVelocyPack(options, _options)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "failed to parse 'IResearchViewNode' options: " + options.toString());
  }
  // volatility mask
  auto const volatilityMaskSlice = base.get(kNodeVolatilityParam);
  if (volatilityMaskSlice.isNumber()) {
    _volatilityMask = volatilityMaskSlice.getNumber<int>();
  }

  auto const immutablePartsSlice = base.get(kNodeViewImmutableParts);
  if (immutablePartsSlice.isInteger()) {
    _immutableParts = immutablePartsSlice.getNumber<uint32_t>() - 1U;
  }

  // parse sort buckets and set them and sort
  auto const sortBucketsSlice = base.get(kNodePrimarySortBucketsParam);
  if (!sortBucketsSlice.isNone()) {
    auto const& sort = primarySort(_meta, _view);
    if (!sortBucketsSlice.isNumber<size_t>()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "invalid vpack format: 'primarySortBuckets' attribute is "
          "intended to be a number");
    }
    auto const sortBuckets = sortBucketsSlice.getNumber<size_t>();
    if (sortBuckets > sort.size()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("invalid vpack format: value of "
                       "'primarySortBuckets' attribute '",
                       sortBuckets,
                       "' is greater than number of buckets specified in "
                       "'primarySort' attribute '",
                       sort.size(), "'"));
    }
    setSort(sort, sortBuckets);
  }

  auto const noMaterializationSlice = base.get(kNodeViewNoMaterialization);
  if (!noMaterializationSlice.isNone()) {
    if (!noMaterializationSlice.isBool()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("'noMaterialization' ",
                       noMaterializationSlice.toString(),
                       " should be a bool value"));
    }
    _noMaterialization = noMaterializationSlice.getBool();
  } else {
    _noMaterialization = false;
  }

  if (isLateMaterialized() || isNoMaterialization()) {
    auto const* vars = plan.getAst()->variables();
    TRI_ASSERT(vars);
    auto const viewValuesVarsSlice = base.get(kNodeViewValuesVars);
    if (!viewValuesVarsSlice.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "'viewValuesVars' attribute should be an array");
    }
    ViewValuesVars viewValuesVars;
    for (auto const columnFieldsVars :
         velocypack::ArrayIterator(viewValuesVarsSlice)) {
      auto const columnNumberSlice =
          columnFieldsVars.get(kNodeViewValuesVarColumnNumber);
      if (columnNumberSlice.isNone()) {
        extractViewValuesVar(vars, viewValuesVars, kSortColumnNumber,
                             columnFieldsVars);
        continue;
      }
      if (!columnNumberSlice.isNumber<size_t>()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("'viewValuesVars[*].columnNumber' ",
                         columnNumberSlice.toString(), " should be a number"));
      }  // TODO Why check size_t but get ptrdiff_t
      auto const columnNumber = columnNumberSlice.getNumber<ptrdiff_t>();
      auto const viewStoredValuesVarsSlice =
          columnFieldsVars.get(kNodeViewStoredValuesVars);
      if (!viewStoredValuesVarsSlice.isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "'viewValuesVars[*].viewStoredValuesVars' attribute should be an "
            "array");
      }
      for (auto const fieldVar :
           velocypack::ArrayIterator(viewStoredValuesVarsSlice)) {
        extractViewValuesVar(vars, viewValuesVars, columnNumber, fieldVar);
      }
    }
    if (!viewValuesVars.empty()) {
      _outNonMaterializedViewVars = std::move(viewValuesVars);
    }
  }
  if ((base.hasKey(kNodeViewScorersSort) ^
       base.hasKey(kNodeViewScorersSortLimit))) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "'scorersSort' and 'scorersSortLimit' attributes should be both "
        "present or both absent");
  }
  auto const heapSortSlice = base.get(kNodeViewHeapSort);
  if (!heapSortSlice.isNone()) {
    if (!heapSortSlice.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "'heapSort' should be an array");
    }
    auto itr = velocypack::ArrayIterator(heapSortSlice);
    for (auto const heapSortElement : itr) {
      if (!heapSortElement.isObject()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("'heapSort[", itr.index(),
                         "]' attribute should be an object"));
      }
      auto source = heapSortElement.get(kNodeViewHeapSortSource);
      auto asc = heapSortElement.get(kNodeViewScorersSortAsc);
      auto field = heapSortElement.get(kNodeViewHeapSortField);
      auto postfix = heapSortElement.get(kNodeViewHeapSortPostfix);
      if (source.isNumber() && asc.isBoolean() && field.isNumber() &&
          postfix.isString()) {
        auto sourceVal = source.getNumber<ptrdiff_t>();
        auto fieldVal = field.getNumber<size_t>();
        auto postfixVal = postfix.copyString();
        _heapSort.emplace_back(HeapSortElement{.postfix = std::move(postfixVal),
                                               .source = sourceVal,
                                               .fieldNumber = fieldVal,
                                               .ascending = asc.getBool()});
        if (_heapSort.back().isScore() &&
            static_cast<size_t>(_heapSort.back().source) >= _scorers.size()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("'heapSort[", itr.index(),
                           "].source' attribute is out of range"));
        }
      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER, absl::StrCat("'heapSort[", itr.index(),
                                                  "]' attribute is invalid "));
      }
    }
    auto const heapSortLimitSlice = base.get(kNodeViewHeapSortLimit);
    if (!heapSortLimitSlice.isNone()) {
      if (!heapSortLimitSlice.isNumber()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "'heapSortLimit' attribute should be a numeric");
      }
      _heapSortLimit = heapSortLimitSlice.getNumber<size_t>();
    }
  } else {
    // TODO remove when 3.11 is deprecated
    auto const scorersSortSlice = base.get(kNodeViewScorersSort);
    if (!scorersSortSlice.isNone()) {
      if (!scorersSortSlice.isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "'scorersSort' should be an array");
      }
      auto itr = velocypack::ArrayIterator(scorersSortSlice);
      for (auto const scorersSortElement : itr) {
        if (!scorersSortElement.isObject()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("'scorersSort[", itr.index(),
                           "]' attribute should be an object"));
        }
        auto index = scorersSortElement.get(kNodeViewScorersSortIndex);
        auto asc = scorersSortElement.get(kNodeViewScorersSortAsc);
        if (index.isNumber<size_t>() && asc.isBoolean()) {
          auto indexVal = index.getNumber<size_t>();
          if (indexVal >= _scorers.size()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_BAD_PARAMETER,
                absl::StrCat("'scorersSort[", itr.index(),
                             "].index' attribute is out of range"));
          }
          _heapSort.emplace_back(
              HeapSortElement{.postfix = "",
                              .source = index.getNumber<ptrdiff_t>(),
                              .fieldNumber = std::numeric_limits<size_t>::max(),
                              .ascending = asc.getBool()});
        } else {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("'scorersSort[", itr.index(),
                           "]' attribute is invalid "));
        }
      }
    }
    auto const scorersSortLimitSlice = base.get(kNodeViewScorersSortLimit);
    if (!scorersSortLimitSlice.isNone()) {
      if (!scorersSortLimitSlice.isNumber()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "'scorersSortLimit' attribute should be a numeric");
      }
      _heapSortLimit = scorersSortLimitSlice.getNumber<size_t>();
    }
  }
}

std::pair<bool, bool> IResearchViewNode::volatility(
    bool force /*=false*/) const {
  if (force || _volatilityMask < 0) {
    _volatilityMask = evaluateVolatility(*this);
  }

  return std::make_pair(irs::check_bit<0>(_volatilityMask),   // filter
                        irs::check_bit<1>(_volatilityMask));  // sort
}

aql::Collection const* IResearchViewNode::collection() const {
  auto c = collections();
  if (c.size() == 1) {
    return &c.front().first.get();
  }
  // this API should not be used for multicollection views
  // doing so might break satellite join assumptions
  TRI_ASSERT(c.empty());
  return nullptr;
}

bool IResearchViewNode::isUsedAsSatellite() const { return _isUsedAsSatellite; }
void IResearchViewNode::useAsSatelliteOf(aql::ExecutionNodeId) {
  _isUsedAsSatellite = true;
}

aql::Collection const* IResearchViewNode::prototypeCollection() const {
  return _prototypeCollection;
}

void IResearchViewNode::setPrototype(aql::Collection const* prototypeCollection,
                                     aql::Variable const*) {
  _prototypeCollection = prototypeCollection;
}

void const* IResearchViewNode::getSnapshotKey() const noexcept {
  // if (ServerState::instance()->isDBServer()) {
  // TODO We want transactional cluster, now it's not
  // So we don't make sense to make ViewSnapshotView for restrictSources
  // and we can use node address as key
  return (_options.restrictSources || _view == nullptr)
             ? static_cast<void const*>(this)
             : static_cast<void const*>(_view.get());
  //} else if (ServerState::instance()->isSingleServer()) {
  //  return _view.get();
  //}
  // TRI_ASSERT(false);
  // return nullptr;
}

void IResearchViewNode::doToVelocyPack(VPackBuilder& nodes,
                                       unsigned flags) const {
  // system info
  TRI_ASSERT(_view);
  TRI_ASSERT(_meta || _view->type() != ViewType::kSearchAlias);
  // need 'view' field to correctly print view name in JS explanation
  nodes.add(kNodeViewNameParam, VPackValue(_view->name()));
  if (!_meta) {
    absl::AlphaNum viewId{_view->id().id()};
    nodes.add(kNodeViewIdParam, VPackValue(viewId.Piece()));
  }

  // our variable
  nodes.add(VPackValue(kNodeOutVariableParam));
  _outVariable->toVelocyPack(nodes);

  if (_outSearchDocId != nullptr) {
    nodes.add(VPackValue(kNodeOutSearchDocParam));
    _outSearchDocId->toVelocyPack(nodes);
  }

  if (_outNonMaterializedDocId != nullptr) {
    nodes.add(VPackValue(kNodeOutNmDocParam));
    _outNonMaterializedDocId->toVelocyPack(nodes);
  }

  if (_noMaterialization) {
    nodes.add(kNodeViewNoMaterialization, VPackValue(_noMaterialization));
  }

  bool const needHeapSort = !_heapSort.empty() && _heapSortLimit;
  if (needHeapSort) {
    nodes.add(kNodeViewHeapSortLimit, VPackValue(_heapSortLimit));
    VPackArrayBuilder scorersSort(&nodes, kNodeViewHeapSort);
    for (auto const& s : _heapSort) {
      VPackObjectBuilder scorer(&nodes);
      nodes.add(kNodeViewHeapSortSource, VPackValue(s.source));
      nodes.add(kNodeViewHeapSortField, VPackValue(s.fieldNumber));
      nodes.add(kNodeViewScorersSortAsc, VPackValue(s.ascending));
      nodes.add(kNodeViewHeapSortPostfix, VPackValue(s.postfix));
    }
  }

  // stored values
  {
    auto const& values = storedValues(_meta, _view);
    auto const& sort = primarySort(_meta, _view);
    VPackArrayBuilder arrayScope(&nodes, kNodeViewValuesVars);
    std::string fieldName;
    for (auto const& columnFieldsVars : _outNonMaterializedViewVars) {
      if (columnFieldsVars.first != kSortColumnNumber) {
        VPackObjectBuilder objectScope(&nodes);
        auto const& columns = values.columns();
        auto const storedColumnNumber =
            static_cast<size_t>(columnFieldsVars.first);
        TRI_ASSERT(storedColumnNumber < columns.size());
        nodes.add(kNodeViewValuesVarColumnNumber,
                  VPackValue(static_cast<size_t>(columnFieldsVars.first)));
        VPackArrayBuilder arrayScope(&nodes, kNodeViewStoredValuesVars);
        for (auto const& fieldVar : columnFieldsVars.second) {
          VPackObjectBuilder objectScope(&nodes);
          fieldName.clear();
          TRI_ASSERT(fieldVar.fieldNum <
                     columns[storedColumnNumber].fields.size());
          fieldName =
              columns[storedColumnNumber].fields[fieldVar.fieldNum].first;
          addViewValuesVar(nodes, fieldName, fieldVar);
        }
      } else {
        TRI_ASSERT(columnFieldsVars.first == kSortColumnNumber);
        for (auto const& fieldVar : columnFieldsVars.second) {
          VPackObjectBuilder objectScope(&nodes);
          fieldName.clear();
          TRI_ASSERT(fieldVar.fieldNum < sort.fields().size());
          basics::TRI_AttributeNamesToString(sort.fields()[fieldVar.fieldNum],
                                             fieldName, true);
          addViewValuesVar(nodes, fieldName, fieldVar);
        }
      }
    }
  }

  // filter condition
  nodes.add(VPackValue(kNodeConditionParam));
  if (!isFilterConditionEmpty(_filterCondition)) {
    _filterCondition->toVelocyPack(nodes, flags != 0);
  } else {
    nodes.openObject();
    nodes.close();
  }

  // sort condition
  nodes.add(VPackValue(kNodeScorersParam));
  iresearch::toVelocyPack(nodes, _scorers, flags != 0);
  // TODO object<shard, array<index>>
  // shards
  {
    VPackArrayBuilder arrayScope(&nodes, kNodeShardsParam);
    for (auto const& [shard, _] : _shards) {
      nodes.add(VPackValue(shard));
    }
  }
  // indexes
  {
    VPackArrayBuilder arrayScope(&nodes, kNodeIndexesParam, true);
    uint64_t i = 0;
    for (auto const& [_, indexes] : _shards) {
      for (auto const& index : indexes) {
        // TODO(MBkkt) check options.sources
        nodes.add(VPackValue{index.id()});
        nodes.add(VPackValue{i});
      }
      ++i;
    }
  }

  // options
  nodes.add(VPackValue(kNodeOptionsParam));
  iresearch::toVelocyPack(nodes, _options);

  // volatility mask
  nodes.add(kNodeVolatilityParam, VPackValue(_volatilityMask));

  if (_immutableParts != 0) {
    nodes.add(kNodeViewImmutableParts, VPackValue{_immutableParts + 1U});
  }

  // primary sort buckets
  bool const needSort = _sort && !_sort->empty();
  if (needSort) {
    nodes.add(kNodePrimarySortBucketsParam, VPackValue(_sortBuckets));
  }
  if (_meta) {
    iresearch::toVelocyPack(nodes, *_meta, needSort, needHeapSort);
  }
}

IResearchViewNode::Collections IResearchViewNode::collections() const {
  TRI_ASSERT(_plan && _plan->getAst());
  auto const& collections = _plan->getAst()->query().collections();
  Collections viewCollections;
  auto visitor = [&](DataSourceId cid, LogicalView::Indexes* indexes) {
    if (_options.restrictSources && !_options.sources.contains(cid)) {
      return true;
    }
    absl::AlphaNum const cidStr{cid.id()};
    auto const* collection = collections.get(cidStr.Piece());
    if (collection) {
      viewCollections.emplace_back(
          *collection, indexes ? std::move(*indexes) : LogicalView::Indexes{});
    } else {
      LOG_TOPIC("ee270", WARN, iresearch::TOPIC)
          << "collection with id '" << cidStr.Piece()
          << "' is not registered with the query";
    }
    return true;
  };
  TRI_ASSERT(_view);
  _view->visitCollections(visitor);
  return viewCollections;
}

aql::ExecutionNode* IResearchViewNode::clone(aql::ExecutionPlan* plan,
                                             bool withDependencies) const {
  TRI_ASSERT(plan);

  auto node = std::make_unique<IResearchViewNode>(
      *plan, _id, _vocbase, _view, *_outVariable,
      const_cast<aql::AstNode*>(_filterCondition), nullptr,
      decltype(_scorers)(_scorers));
  node->_shards = _shards;
  node->_options = _options;
  node->_volatilityMask = _volatilityMask;
  node->_immutableParts = _immutableParts;
  node->_sort = _sort;
  node->_optState = _optState;
  if (_outSearchDocId != nullptr) {
    node->setSearchDocIdVar(*_outSearchDocId);
  }
  if (_outNonMaterializedDocId != nullptr) {
    node->setLateMaterialized(*_outNonMaterializedDocId);
  }
  node->_noMaterialization = _noMaterialization;
  node->_outNonMaterializedViewVars = _outNonMaterializedViewVars;
  node->_heapSort = _heapSort;
  node->_heapSortLimit = _heapSortLimit;
  return cloneHelper(std::move(node), withDependencies);
}

bool IResearchViewNode::empty() const noexcept {
  if (_view) {
    return _view->visitCollections(sEmptyView);
  } else {
    TRI_ASSERT(false);
    return _shards.empty();
  }
}

aql::CostEstimate IResearchViewNode::estimateCost() const {
  if (_dependencies.empty()) {
    return aql::CostEstimate::empty();
  }
  TRI_ASSERT(_plan && _plan->getAst());
  transaction::Methods& trx = _plan->getAst()->query().trxForOptimization();
  if (trx.status() != transaction::Status::RUNNING) {
    return aql::CostEstimate::empty();
  }
  auto const& collections = _plan->getAst()->query().collections();
  size_t estimatedNrItems = 0;
  auto visitor = [&](DataSourceId cid, LogicalView::Indexes*) {
    if (_options.restrictSources && !_options.sources.contains(cid)) {
      return true;
    }
    absl::AlphaNum const cidStr{cid.id()};
    auto const* collection = collections.get(cidStr.Piece());
    if (collection) {
      // FIXME better to gather count for multiple collections at once
      estimatedNrItems +=
          collection->count(&trx, transaction::CountType::kTryCache);
    } else {
      LOG_TOPIC("ee276", WARN, iresearch::TOPIC)
          << "collection with id '" << cidStr.Piece()
          << "' is not registered with the query";
    }
    return true;
  };
  TRI_ASSERT(_view);
  _view->visitCollections(visitor);
  aql::CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedNrItems *= estimatedNrItems;
  estimate.estimatedCost += static_cast<double>(estimate.estimatedNrItems);
  return estimate;
}

aql::AsyncPrefetchEligibility IResearchViewNode::canUseAsyncPrefetching()
    const noexcept {
  return aql::AsyncPrefetchEligibility::kEnableForNode;
}

// Replaces variables in the internals of the execution node
// replacements are { old variable id => new variable }.
void IResearchViewNode::replaceVariables(
    std::unordered_map<aql::VariableId, aql::Variable const*> const&
        replacements) {
  aql::Ast* ast = plan()->getAst();
  // replace in filter condition
  aql::AstNode const& search = filterCondition();
  if (!isFilterConditionEmpty(&search)) {
    aql::VarSet variables;
    aql::Ast::getReferencedVariables(&search, variables);
    // check if the search condition uses any of the variables that we want to
    // replace
    aql::AstNode* cloned = nullptr;
    for (auto const& it : variables) {
      if (replacements.contains(it->id)) {
        if (cloned == nullptr) {
          // only clone the original search condition once
          cloned = ast->clone(&search);
        }
        ast->replaceVariables(cloned, replacements, true);
      }
    }

    if (cloned != nullptr) {
      // exchange the filter condition
      setFilterCondition(cloned);
    }
  }

  // replace in scorers
  for (auto& it : _scorers) {
    it.node = ast->replaceVariables(ast->clone(it.node), replacements);
  }
}

void IResearchViewNode::replaceAttributeAccess(
    aql::ExecutionNode const* self, aql::Variable const* searchVariable,
    std::span<std::string_view> attribute, aql::Variable const* replaceVariable,
    size_t /*index*/) {
  aql::Ast* ast = plan()->getAst();
  // replace in filter condition
  aql::AstNode const& search = filterCondition();
  if (!isFilterConditionEmpty(&search)) {
    aql::AstNode* cloned = ast->clone(&search);
    cloned = aql::Ast::replaceAttributeAccess(ast, cloned, searchVariable,
                                              attribute, replaceVariable);
    setFilterCondition(cloned);
  }

  // replace in scorers
  for (auto& it : _scorers) {
    it.node = ast->replaceAttributeAccess(
        ast, ast->clone(it.node), searchVariable, attribute, replaceVariable);
  }
}

void IResearchViewNode::getVariablesUsedHere(aql::VarSet& vars) const {
  auto const outVariableAlreadyInVarSet = vars.contains(_outVariable);

  if (!isFilterConditionEmpty(_filterCondition)) {
    aql::Ast::getReferencedVariables(_filterCondition, vars);
  }

  for (auto& scorer : _scorers) {
    aql::Ast::getReferencedVariables(scorer.node, vars);
  }

  if (!outVariableAlreadyInVarSet) {
    vars.erase(_outVariable);
  }
}

std::vector<aql::Variable const*> IResearchViewNode::getVariablesSetHere()
    const {
  std::vector<aql::Variable const*> vars;
  // scorers + vars for late materialization
  auto reserve = _scorers.size() + _outNonMaterializedViewVars.size();
  // collection + docId or document
  if (isLateMaterialized()) {
    reserve += 2;
  } else if (!isNoMaterialization()) {
    ++reserve;
  }
  if (_outSearchDocId != nullptr) {
    ++reserve;
  }
  vars.reserve(reserve);

  std::transform(_scorers.cbegin(), _scorers.cend(), std::back_inserter(vars),
                 [](auto const& scorer) { return scorer.var; });
  if (isLateMaterialized() || isNoMaterialization()) {
    if (isLateMaterialized()) {
      vars.emplace_back(_outNonMaterializedDocId);
    }
    for (auto const& columnFieldsVars : _outNonMaterializedViewVars) {
      std::transform(columnFieldsVars.second.cbegin(),
                     columnFieldsVars.second.cend(), std::back_inserter(vars),
                     [](auto const& fieldVar) { return fieldVar.var; });
    }
  } else {
    vars.emplace_back(_outVariable);
  }
  if (_outSearchDocId != nullptr) {
    vars.emplace_back(_outSearchDocId);
  }
  return vars;
}

aql::RegIdSet IResearchViewNode::calcInputRegs() const {
  auto inputRegs = aql::RegIdSet{};

  if (!isFilterConditionEmpty(_filterCondition)) {
    aql::VarSet vars;
    aql::Ast::getReferencedVariables(_filterCondition, vars);

    if (isNoMaterialization()) {
      vars.erase(_outVariable);
    }

    for (auto const& it : vars) {
      aql::RegisterId reg = variableToRegisterId(it);
      // The filter condition may refer to registers that are written here
      if (reg.isConstRegister() || reg < getNrInputRegisters()) {
        inputRegs.emplace(reg);
      }
    }
  }

  return inputRegs;
}

void IResearchViewNode::setFilterCondition(aql::AstNode const* node) noexcept {
  _filterCondition = node ? node : &kAll;
}

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#endif
std::unique_ptr<aql::ExecutionBlock> IResearchViewNode::createBlock(
    aql::ExecutionEngine& engine) const {
  auto const createNoResultsExecutor = [this](aql::ExecutionEngine& engine) {
    auto emptyRegisterInfos = createRegisterInfos({}, {});
    aql::ExecutionNode const* previousNode = getFirstDependency();
    TRI_ASSERT(previousNode != nullptr);

    return std::make_unique<aql::ExecutionBlockImpl<aql::NoResultsExecutor>>(
        &engine, this, std::move(emptyRegisterInfos),
        aql::EmptyExecutorInfos{});
  };

  auto const createSnapshot = [this](aql::ExecutionEngine& engine) {
    auto* trx = &engine.getQuery().trxForOptimization();

    if (!trx) {
      LOG_TOPIC("7c905", WARN, iresearch::TOPIC)
          << "failed to get transaction while creating IResearchView "
             "ExecutionBlock";

      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "failed to get transaction while creating "
                                     "IResearchView ExecutionBlock");
    }
    TRI_ASSERT(trx->state());

    if (options().forceSync &&
        trx->state()->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "cannot use waitForSync with view and streaming or js transaction");
    }

    auto const viewName = this->view() ? this->view()->name() : "";
    LOG_TOPIC("82af6", TRACE, iresearch::TOPIC)
        << "Start getting snapshot for view '" << viewName << "'";
    ViewSnapshotPtr reader;
    // we manage snapshots differently in single-server/db server,
    // see description of functions below to learn how
    if (ServerState::instance()->isDBServer()) {
      reader = snapshotDBServer(*this, *trx);
    } else {
      reader = snapshotSingleServer(*this, *trx);
    }
    if (!reader) {
      LOG_TOPIC("9bb93", WARN, iresearch::TOPIC)
          << "failed to get snapshot while creating arangosearch view '"
          << viewName << "' ExecutionBlock";

      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          absl::StrCat("failed to get snapshot while creating "
                       "arangosearch view '",
                       viewName, "' ExecutionBlock"));
    }
    return reader;
  };

  auto const buildExecutorInfo = [this](aql::ExecutionEngine& engine,
                                        ViewSnapshotPtr reader) {
    // We could be asked to produce only document/collection ids for later
    // materialization or full document body at once
    aql::RegisterCount numDocumentRegs = uint32_t{_outSearchDocId != nullptr};
    MaterializeType materializeType = MaterializeType::Undefined;
    if (isLateMaterialized()) {
      TRI_ASSERT(!isNoMaterialization());
      materializeType = MaterializeType::LateMaterialize;
      numDocumentRegs += 1;
    } else if (isNoMaterialization()) {
      TRI_ASSERT(options().noMaterialization);
      materializeType = MaterializeType::NotMaterialize;
    } else {
      materializeType = MaterializeType::Materialize;
      ++numDocumentRegs;
    }

    // We have one output register for documents, which is always the first
    // after the input registers.

    auto numScoreRegisters = static_cast<aql::RegisterCount>(_scorers.size());
    auto numViewVarsRegisters = std::accumulate(
        _outNonMaterializedViewVars.cbegin(),
        _outNonMaterializedViewVars.cend(), static_cast<aql::RegisterCount>(0),
        [](aql::RegisterCount const sum, auto const& columnFieldsVars) {
          return sum + static_cast<aql::RegisterCount>(
                           columnFieldsVars.second.size());
        });
    if (numViewVarsRegisters > 0) {
      materializeType |= MaterializeType::UseStoredValues;
    }

    // We have one additional output register for each scorer, before
    // the output register(s) for documents (one or two + vars, depending on
    // late materialization) These must of course fit in the available
    // registers. There may be unused registers reserved for later blocks.
    aql::RegIdSet writableOutputRegisters;
    writableOutputRegisters.reserve(numDocumentRegs + numScoreRegisters +
                                    numViewVarsRegisters);

    aql::RegisterId searchDocRegId{aql::RegisterId::makeInvalid()};
    if (_outSearchDocId != nullptr) {
      searchDocRegId = variableToRegisterId(_outSearchDocId);
      writableOutputRegisters.emplace(searchDocRegId);
    }

    auto const outRegister = std::invoke([&]() -> aql::RegisterId {
      if (isLateMaterialized()) {
        aql::RegisterId documentRegId =
            variableToRegisterId(_outNonMaterializedDocId);
        writableOutputRegisters.emplace(documentRegId);
        return documentRegId;
      } else if (isNoMaterialization()) {
        return aql::RegisterId::makeInvalid();
      } else {
        auto outReg = variableToRegisterId(_outVariable);
        writableOutputRegisters.emplace(outReg);
        return outReg;
      }
    });

    std::vector<aql::RegisterId> scoreRegisters;
    scoreRegisters.reserve(numScoreRegisters);
    std::for_each(_scorers.begin(), _scorers.end(), [&](auto const& scorer) {
      auto registerId = variableToRegisterId(scorer.var);
      writableOutputRegisters.emplace(registerId);
      scoreRegisters.emplace_back(registerId);
    });

    // TODO remove if not needed
    auto const& varInfos = getRegisterPlan()->varInfo;

    ViewValuesRegisters outNonMaterializedViewRegs;

    for (auto const& columnFieldsVars : _outNonMaterializedViewVars) {
      for (auto const& fieldsVars : columnFieldsVars.second) {
        auto& fields = outNonMaterializedViewRegs[columnFieldsVars.first];
        auto const it = varInfos.find(fieldsVars.var->id);

        TRI_ASSERT(it != varInfos.cend());
        auto const regId = it->second.registerId;
        writableOutputRegisters.emplace(regId);
        fields.emplace(fieldsVars.fieldNum, regId);
      }
    }
    TRI_ASSERT(writableOutputRegisters.size() ==
               static_cast<std::size_t>(numDocumentRegs) + numScoreRegisters +
                   numViewVarsRegisters);
    aql::RegisterInfos registerInfos = createRegisterInfos(
        calcInputRegs(), std::move(writableOutputRegisters));
    TRI_ASSERT(_view || _meta);
    auto executorInfos = aql::IResearchViewExecutorInfos{
        std::move(reader),
        outRegister,
        searchDocRegId,
        std::move(scoreRegisters),
        engine.getQuery(),
#ifdef USE_ENTERPRISE
        optimizeTopK(_meta, _view),
#endif
        scorers(),
        sort(),
        storedValues(_meta, _view),
        *plan(),
        outVariable(),
        filterCondition(),
        volatility(),
        _immutableParts,
        getRegisterPlan()->varInfo,  // ??? do we need this?
        getDepth(),
        std::move(outNonMaterializedViewRegs),
        _options.countApproximate,
        filterOptimization(),
        _heapSort,
        _heapSortLimit,
        _meta.get(),
        _options.parallelism,
        _vocbase.server().getFeature<IResearchFeature>().getSearchPool()};
    return std::make_tuple(materializeType, std::move(executorInfos),
                           std::move(registerInfos));
  };

  if (ServerState::instance()->isCoordinator()) {
    // coordinator in a cluster: empty view case
    return createNoResultsExecutor(engine);
  }

  auto reader = createSnapshot(engine);
  if (reader->live_docs_count() == 0) {
    return createNoResultsExecutor(engine);
  }

  // auto [materializeType, executorInfos, registerInfos] =
  //    buildExecutorInfo(engine, std::move(reader));
  // Workaround for using clang15 during asan build.
  // FIXME: remove it as soon as we switch to clang16 or higher
  auto infosTuple = buildExecutorInfo(engine, std::move(reader));
  auto& materializeType = std::get<0>(infosTuple);
  auto& executorInfos = std::get<1>(infosTuple);
  auto& registerInfos = std::get<2>(infosTuple);
  // guaranteed by optimizer rule
  TRI_ASSERT(_sort == nullptr || !_sort->empty());
  bool const ordered = !_scorers.empty();
  bool const sorted = _sort != nullptr;
  bool const heapsort = !_heapSort.empty();
  bool const emitSearchDoc = executorInfos.searchDocIdRegId().isValid();
#ifdef USE_ENTERPRISE
  bool const encrypted = _vocbase.engine<RocksDBEngine>().isEncryptionEnabled();
#endif

  auto const executorIdx =
      getExecutorIndex(sorted, ordered, heapsort, emitSearchDoc);
  return irs::ResolveBool(_options.parallelism > 1, [&]<bool copyStored>() {
    switch (materializeType) {
      case MaterializeType::NotMaterialize:
        return kExecutors<copyStored,
                          MaterializeType::NotMaterialize>[executorIdx](
            &engine, this, std::move(registerInfos), std::move(executorInfos));
      case MaterializeType::LateMaterialize:
        return kExecutors<copyStored,
                          MaterializeType::LateMaterialize>[executorIdx](
            &engine, this, std::move(registerInfos), std::move(executorInfos));
      case MaterializeType::Materialize:
        return kExecutors<copyStored,
                          MaterializeType::Materialize>[executorIdx](
            &engine, this, std::move(registerInfos), std::move(executorInfos));
      case MaterializeType::NotMaterialize | MaterializeType::UseStoredValues:
#ifdef USE_ENTERPRISE
        if (encrypted) {
          return kExecutors<true,
                            MaterializeType::NotMaterialize |
                                MaterializeType::UseStoredValues>[executorIdx](
              &engine, this, std::move(registerInfos),
              std::move(executorInfos));
        }
#endif
        return kExecutors<copyStored,
                          MaterializeType::NotMaterialize |
                              MaterializeType::UseStoredValues>[executorIdx](
            &engine, this, std::move(registerInfos), std::move(executorInfos));
      case MaterializeType::LateMaterialize | MaterializeType::UseStoredValues:
#ifdef USE_ENTERPRISE
        if (encrypted) {
          return kExecutors<true,
                            MaterializeType::LateMaterialize |
                                MaterializeType::UseStoredValues>[executorIdx](
              &engine, this, std::move(registerInfos),
              std::move(executorInfos));
        }
#endif
        return kExecutors<copyStored,
                          MaterializeType::LateMaterialize |
                              MaterializeType::UseStoredValues>[executorIdx](
            &engine, this, std::move(registerInfos), std::move(executorInfos));
      default:
        ADB_UNREACHABLE;
    }
  });
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

bool IResearchViewNode::OptimizationState::canVariablesBeReplaced(
    aql::CalculationNode* calculationNode) const {
  return _nodesToChange.contains(calculationNode);
}

void IResearchViewNode::OptimizationState::saveCalcNodesForViewVariables(
    std::vector<aql::latematerialized::NodeWithAttrsColumn> const&
        nodesToChange) {
  TRI_ASSERT(!nodesToChange.empty());
  TRI_ASSERT(_nodesToChange.empty());
  _nodesToChange.clear();
  for (auto& node : nodesToChange) {
    auto& calcNodeData = _nodesToChange[node.node];
    calcNodeData.reserve(node.attrs.size());
    std::transform(
        node.attrs.cbegin(), node.attrs.cend(),
        std::inserter(calcNodeData, calcNodeData.end()),
        [](auto const& attrAndField) { return attrAndField.afData; });
  }
}

IResearchViewNode::ViewVarsInfo
IResearchViewNode::OptimizationState::replaceViewVariables(
    std::span<aql::CalculationNode* const> calcNodes,
    containers::HashSet<ExecutionNode*>& toUnlink) {
  TRI_ASSERT(!calcNodes.empty());
  ViewVarsInfo uniqueVariables;
  // at first use variables from simple expressions
  for (auto* calcNode : calcNodes) {
    TRI_ASSERT(calcNode);
    // a node is already unlinked
    if (calcNode->getParents().empty()) {
      continue;
    }
    TRI_ASSERT(_nodesToChange.find(calcNode) != _nodesToChange.cend());
    auto const& calcNodeData = _nodesToChange[calcNode];
    TRI_ASSERT(!calcNodeData.empty());
    auto const& afData = calcNodeData[0];
    if (afData.parentNode == nullptr && afData.postfix.empty()) {
      TRI_ASSERT(calcNodeData.size() == 1);
      // we can unlink one redundant variable only for each field
      if (uniqueVariables
              .try_emplace(afData.field,
                           ViewVariableWithColumn{
                               {afData.fieldNumber, calcNode->outVariable()},
                               afData.columnNumber})
              .second) {
        toUnlink.emplace(calcNode);
      }
    }
  }
  auto* ast = calcNodes.back()->expression()->ast();
  TRI_ASSERT(ast);
  // create variables for complex expressions
  for (auto* calcNode : calcNodes) {
    TRI_ASSERT(calcNode);
    // a node is already unlinked
    if (calcNode->getParents().empty()) {
      continue;
    }
    TRI_ASSERT(_nodesToChange.find(calcNode) != _nodesToChange.cend());
    auto const& calcNodeData = _nodesToChange[calcNode];
    for (auto const& afData : calcNodeData) {
      // create a variable if necessary
      if ((afData.parentNode != nullptr || !afData.postfix.empty()) &&
          uniqueVariables.find(afData.field) == uniqueVariables.cend()) {
        uniqueVariables.emplace(
            afData.field, ViewVariableWithColumn{
                              {afData.fieldNumber,
                               ast->variables()->createTemporaryVariable()},
                              afData.columnNumber});
      }
    }
  }
  for (auto* calcNode : calcNodes) {
    TRI_ASSERT(calcNode);
    // a node is already unlinked
    if (calcNode->getParents().empty()) {
      continue;
    }
    TRI_ASSERT(_nodesToChange.find(calcNode) != _nodesToChange.cend());
    auto const& calcNodeData = _nodesToChange[calcNode];
    for (auto const& afData : calcNodeData) {
      auto const it = uniqueVariables.find(afData.field);
      TRI_ASSERT(it != uniqueVariables.cend());
      auto* newNode = ast->createNodeReference(it->second.var);
      TRI_ASSERT(newNode);
      if (!afData.postfix.empty()) {
        newNode = ast->createNodeAttributeAccess(newNode, afData.postfix);
        TRI_ASSERT(newNode);
      }
      if (afData.parentNode != nullptr) {
        TEMPORARILY_UNLOCK_NODE(afData.parentNode);
        afData.parentNode->changeMember(afData.childNumber, newNode);
      } else {
        TRI_ASSERT(calcNodeData.size() == 1);
        calcNode->expression()->replaceNode(newNode);
      }
    }
  }
  return uniqueVariables;
}

IResearchViewNode::ViewVarsInfo
IResearchViewNode::OptimizationState::replaceAllViewVariables(
    containers::HashSet<ExecutionNode*>& toUnlink) {
  ViewVarsInfo uniqueVariables;
  if (_nodesToChange.empty()) {
    return uniqueVariables;
  }
  // at first use variables from simple expressions
  for (auto const& calcNode : _nodesToChange) {
    // a node is already unlinked
    if (calcNode.first->getParents().empty()) {
      continue;
    }
    TRI_ASSERT(!calcNode.second.empty());
    auto const& afData = calcNode.second[0];
    if (afData.parentNode == nullptr && afData.postfix.empty()) {
      TRI_ASSERT(calcNode.second.size() == 1);
      // we can unlink one redundant variable only for each field
      if (uniqueVariables
              .try_emplace(
                  afData.field,
                  ViewVariableWithColumn{
                      {afData.fieldNumber, calcNode.first->outVariable()},
                      afData.columnNumber})
              .second) {
        toUnlink.emplace(calcNode.first);
      }
    }
  }
  auto* ast = _nodesToChange.begin()->first->expression()->ast();
  TRI_ASSERT(ast);
  // create variables for complex expressions
  for (auto const& calcNode : _nodesToChange) {
    // a node is already unlinked
    if (calcNode.first->getParents().empty()) {
      continue;
    }
    for (auto const& afData : calcNode.second) {
      // create a variable if necessary
      if ((afData.parentNode != nullptr || !afData.postfix.empty()) &&
          uniqueVariables.find(afData.field) == uniqueVariables.cend()) {
        uniqueVariables.emplace(
            afData.field, ViewVariableWithColumn{
                              {afData.fieldNumber,
                               ast->variables()->createTemporaryVariable()},
                              afData.columnNumber});
      }
    }
  }
  for (auto const& calcNode : _nodesToChange) {
    // a node is already unlinked
    if (calcNode.first->getParents().empty()) {
      continue;
    }
    for (auto const& afData : calcNode.second) {
      auto const it = uniqueVariables.find(afData.field);
      TRI_ASSERT(it != uniqueVariables.cend());
      auto* newNode = ast->createNodeReference(it->second.var);
      TRI_ASSERT(newNode);
      if (!afData.postfix.empty()) {
        newNode = ast->createNodeAttributeAccess(newNode, afData.postfix);
        TRI_ASSERT(newNode);
      }
      if (afData.parentNode != nullptr) {
        TEMPORARILY_UNLOCK_NODE(afData.parentNode);
        afData.parentNode->changeMember(afData.childNumber, newNode);
      } else {
        TRI_ASSERT(calcNode.second.size() == 1);
        calcNode.first->expression()->replaceNode(newNode);
      }
    }
  }
  return uniqueVariables;
}

bool IResearchViewNode::isBuilding() const {
  return _view && _view->isBuilding();
}

size_t IResearchViewNode::getMemoryUsedBytes() const { return sizeof(*this); }

std::pair<ptrdiff_t, size_t> IResearchViewNode::getSourceColumnInfo(
    aql::VariableId id) const noexcept {
  for (auto const& column : _outNonMaterializedViewVars) {
    for (auto const& var : column.second) {
      if (var.var->id == id) {
        return {column.first, var.fieldNum};
      }
    }
  }
  return {std::numeric_limits<ptrdiff_t>::max(), 0};
}

}  // namespace arangodb::iresearch

#ifdef ARANGODB_USE_GOOGLE_TESTS
// need this variant for tests only
template class ::arangodb::aql::IResearchViewMergeExecutor<
    ::arangodb::aql::ExecutionTraits<
        false, false, false,
        ::arangodb::iresearch::MaterializeType::NotMaterialize>>;
#endif
