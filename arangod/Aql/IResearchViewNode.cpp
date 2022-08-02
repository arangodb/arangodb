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
#include "Basics/DownCast.h"

#include "IResearchViewNode.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/EmptyExecutorInfos.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/IResearchViewExecutor.h"
#include "Aql/NoResultsExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortCondition.h"
#include "Aql/types.h"
#include "Basics/NumberUtils.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Containers/FlatHashSet.h"
#include "Containers/HashSet.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/Search.h"
#include "IResearch/ViewSnapshot.h"
#include "RegisterPlan.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "types.h"

#include <frozen/map.h>
#include <velocypack/Iterator.h>

namespace arangodb::iresearch {
namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief surrogate root for all queries without a filter
////////////////////////////////////////////////////////////////////////////////
aql::AstNode const kAll{aql::AstNodeValue{true}};

////////////////////////////////////////////////////////////////////////////////
/// @brief helpers for std::vector<iresearch::IResearchSort>
////////////////////////////////////////////////////////////////////////////////
void toVelocyPack(velocypack::Builder& builder,
                  std::vector<Scorer> const& scorers, bool verbose) {
  VPackArrayBuilder arrayScope(&builder);
  for (auto const& scorer : scorers) {
    VPackObjectBuilder objectScope(&builder);
    builder.add("id", VPackValue(scorer.var->id));
    builder.add("name", VPackValue(scorer.var->name));  // for explainer.js
    builder.add(VPackValue("node"));
    scorer.node->toVelocyPack(builder, verbose);
  }
}

std::vector<Scorer> fromVelocyPack(aql::ExecutionPlan& plan,
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
  std::vector<Scorer> scorers;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief helpers for IResearchViewNode::Options
////////////////////////////////////////////////////////////////////////////////
constexpr auto conditionOptimizationTypeMap =
    frozen::make_map<std::string_view, aql::ConditionOptimization>({
        {"auto", aql::ConditionOptimization::Auto},
        {"nodnf", aql::ConditionOptimization::NoDNF},
        {"noneg", aql::ConditionOptimization::NoNegation},
        {"none", aql::ConditionOptimization::None},
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
      aql::ExecutionPlan::invalidOptionAttribute(
          query, "unknown", "FOR", attributeName.data(), attributeName.size());
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

////////////////////////////////////////////////////////////////////////////////
/// @brief other helpers
////////////////////////////////////////////////////////////////////////////////
// in loop or non-deterministic
bool hasDependencies(aql::ExecutionPlan const& plan, aql::AstNode const& node,
                     aql::Variable const& ref, aql::VarSet& vars) {
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
      case aql::ExecutionNode::SHORTEST_PATH:
      case aql::ExecutionNode::ENUMERATE_PATHS:
      case aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
        // we're in the loop with dependent context
        return true;
      default:
        break;
    }
    if (!setter->isDeterministic() || setter->getLoop() != nullptr) {
      return true;
    }
  }
  return false;
}

/// @returns true if a given node is located inside a loop or subquery
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
    irs::set_bit<0>(hasDependencies(plan, filterCondition, outVariable, vars),
                    mask);
  }
  // evaluate sort condition volatility
  auto& scorers = node.scorers();
  if (!scorers.empty() && inDependentScope) {
    vars.clear();
    for (auto const& scorer : scorers) {
      if (hasDependencies(plan, *scorer.node, outVariable, vars)) {
        irs::set_bit<1>(mask);
        break;
      }
    }
  }
  return mask;
}

LogicalView::CollectionVisitor const sEmptyView{
    [](DataSourceId, LogicalView::Indexes*) noexcept { return false; }};

/// @brief Since cluster is not transactional and each distributed
///        part of a query starts it's own transaction associated
///        with global query identifier, there is no single place
///        to store a snapshot and we do the following:
///
///   1. Each query part on DB server gets the list of shards
///      to be included into a query and starts its own transaction
///
///   2. Given the list of shards we take view snapshot according
///      to the list of restricted data sources specified in options
///      of corresponding IResearchViewNode
///
///   3. If waitForSync is specified, we refresh snapshot
///      of each shard we need and finally put it to transaction
///      associated to a part of the distributed query. We use
///      default snapshot key if there are no restricted sources
///      specified in options or IResearchViewNode address otherwise
///
///      Custom key is needed for the following query
///      (assume 'view' is lined with 'c1' and 'c2' in the example below):
///           FOR d IN view OPTIONS { collections : [ 'c1' ] }
///           FOR x IN view OPTIONS { collections : [ 'c2' ] }
///           RETURN {d, x}
///
ViewSnapshotPtr snapshotDBServer(IResearchViewNode const& node,
                                 transaction::Methods& trx) {
  TRI_ASSERT(ServerState::instance()->isDBServer());
  auto const view = node.view();
  auto const& options = node.options();
  // TODO We want transactional cluster, now it's not
  // So we don't make sense to make ViewSnapshotView for restrictSources
  // and we can use node address as key
  void const* key = (options.restrictSources || view == nullptr)
                        ? static_cast<void const*>(&node)
                        : static_cast<void const*>(view.get());
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
    auto const& collection = resolver->getCollection(shard);
    if (!collection) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          absl::StrCat("failed to find shard by id '", shard, "'"));
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

/// @brief Since single-server is transactional we do the following:
///
///   1. When transaction starts we put index snapshot into it
///
///   2. If waitForSync is specified, we refresh snapshot
///      taken in (1), object itself remains valid
///
///   3. If there are no restricted sources in a query, we reuse
///      snapshot taken in (1),
///      otherwise we reassemble restricted snapshot based on the
///      original one taken in (1) and return it
///
ViewSnapshotPtr snapshotSingleServer(IResearchViewNode const& node,
                                     transaction::Methods& trx) {
  TRI_ASSERT(ServerState::instance()->isSingleServer());
  auto const& view = *node.view();
  auto const& options = node.options();
  void const* key = static_cast<void const*>(&view);
  auto* snapshot = getViewSnapshot(trx, key);
  if (snapshot != nullptr) {
    if (options.forceSync) {
      syncViewSnapshot(*snapshot, view.name());
    }
    if (options.restrictSources) {
      return std::make_shared<ViewSnapshotView>(*snapshot, options.sources);
    }
    return {ViewSnapshotPtr{}, snapshot};
  }
  if (!options.forceSync && trx.isMainTransaction()) {
    return {};
  }
  auto links = [&] {
    if (view.type() == ViewType::kView) {
      auto const& viewImpl = basics::downCast<IResearchView>(view);
      return viewImpl.getLinks();
    } else {
      auto const& viewImpl = basics::downCast<Search>(view);
      return viewImpl.getLinks();
    }
  }();
  snapshot = makeViewSnapshot(trx, key, options.forceSync, view.name(),
                              std::move(links));
  if (options.restrictSources && snapshot != nullptr) {
    return std::make_shared<ViewSnapshotView>(*snapshot, options.sources);
  }
  return {ViewSnapshotPtr{}, snapshot};
}

IResearchSortBase const& primarySort(LogicalView const& view) {
  if (view.type() == ViewType::kSearch) {
    auto const& viewImpl = basics::downCast<Search>(view);
    return viewImpl.primarySort();
  }
  TRI_ASSERT(view.type() == ViewType::kView);
  if (ServerState::instance()->isCoordinator()) {
    auto const& viewImpl = basics::downCast<IResearchViewCoordinator>(view);
    return viewImpl.primarySort();
  }
  auto const& viewImpl = basics::downCast<IResearchView>(view);
  return viewImpl.primarySort();
}

IResearchViewStoredValues const& storedValues(LogicalView const& view) {
  if (view.type() == ViewType::kSearch) {
    auto const& viewImpl = basics::downCast<Search>(view);
    return viewImpl.storedValues();
  }
  TRI_ASSERT(view.type() == ViewType::kView);
  if (ServerState::instance()->isCoordinator()) {
    auto const& viewImpl = basics::downCast<IResearchViewCoordinator>(view);
    return viewImpl.storedValues();
  }
  auto const& viewImpl = basics::downCast<IResearchView>(view);
  return viewImpl.storedValues();
}

const char* NODE_DATABASE_PARAM = "database";
const char* NODE_VIEW_NAME_PARAM = "view";
const char* NODE_VIEW_ID_PARAM = "viewId";
const char* NODE_OUT_VARIABLE_PARAM = "outVariable";
const char* NODE_OUT_NM_DOC_PARAM = "outNmDocId";
const char* NODE_OUT_NM_COL_PARAM = "outNmColPtr";
const char* NODE_CONDITION_PARAM = "condition";
const char* NODE_SCORERS_PARAM = "scorers";
const char* NODE_SHARDS_PARAM = "shards";
const char* NODE_INDEXES_PARAM = "indexes";
const char* NODE_OPTIONS_PARAM = "options";
const char* NODE_VOLATILITY_PARAM = "volatility";
const char* NODE_PRIMARY_SORT_PARAM = "primarySort";
const char* NODE_STORED_VALUES_PARAM = "storedValues";
const char* NODE_PRIMARY_SORT_BUCKETS_PARAM = "primarySortBuckets";
const char* NODE_VIEW_VALUES_VARS = "viewValuesVars";
const char* NODE_VIEW_STORED_VALUES_VARS = "viewStoredValuesVars";
const char* NODE_VIEW_VALUES_VAR_COLUMN_NUMBER = "columnNumber";
const char* NODE_VIEW_VALUES_VAR_FIELD_NUMBER = "fieldNumber";
const char* NODE_VIEW_VALUES_VAR_ID = "id";
const char* NODE_VIEW_VALUES_VAR_NAME = "name";
const char* NODE_VIEW_VALUES_VAR_FIELD = "field";
const char* NODE_VIEW_NO_MATERIALIZATION = "noMaterialization";
const char* NODE_VIEW_SCORERS_SORT = "scorersSort";
const char* NODE_VIEW_SCORERS_SORT_INDEX = "index";
const char* NODE_VIEW_SCORERS_SORT_ASC = "asc";
const char* NODE_VIEW_SCORERS_SORT_LIMIT = "scorersSortLimit";

void addViewValuesVar(VPackBuilder& nodes, std::string& fieldName,
                      IResearchViewNode::ViewVariable const& fieldVar) {
  nodes.add(NODE_VIEW_VALUES_VAR_FIELD_NUMBER, VPackValue(fieldVar.fieldNum));
  nodes.add(NODE_VIEW_VALUES_VAR_ID, VPackValue(fieldVar.var->id));
  nodes.add(NODE_VIEW_VALUES_VAR_NAME,
            VPackValue(fieldVar.var->name));  // for explainer.js
  nodes.add(NODE_VIEW_VALUES_VAR_FIELD,
            VPackValue(fieldName));  // for explainer.js
}

void extractViewValuesVar(aql::VariableGenerator const* vars,
                          IResearchViewNode::ViewValuesVars& viewValuesVars,
                          ptrdiff_t const columnNumber,
                          velocypack::Slice fieldVar) {
  auto const fieldNumberSlice = fieldVar.get(NODE_VIEW_VALUES_VAR_FIELD_NUMBER);
  if (!fieldNumberSlice.isNumber<size_t>()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("'viewValuesVars[*].fieldNumber' ",
                     fieldNumberSlice.toString(), " should be a number"));
  }
  auto const fieldNumber = fieldNumberSlice.getNumber<size_t>();
  auto const varIdSlice = fieldVar.get(NODE_VIEW_VALUES_VAR_ID);
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
          aql::IResearchViewExecutor<copyStored, false, materializeType>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<aql::ExecutionBlockImpl<
          aql::IResearchViewExecutor<copyStored, true, materializeType>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<aql::ExecutionBlockImpl<
          aql::IResearchViewMergeExecutor<copyStored, false, materializeType>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<aql::ExecutionBlockImpl<
          aql::IResearchViewMergeExecutor<copyStored, true, materializeType>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<
          aql::ExecutionBlockImpl<aql::IResearchViewHeapSortExecutor<
              copyStored, false, materializeType>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos)
        -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<
          aql::ExecutionBlockImpl<aql::IResearchViewHeapSortExecutor<
              copyStored, true, materializeType>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    }};

constexpr size_t getExecutorIndex(bool sorted, bool ordered, bool heapsort) {
  TRI_ASSERT(!sorted || !heapsort);
  auto index = static_cast<size_t>(ordered) + 2 * static_cast<size_t>(sorted) +
               static_cast<size_t>(heapsort) * 4;
  TRI_ASSERT(index <
             std::size((kExecutors<false, MaterializeType::Materialize>)));
  return index;
}

}  // namespace

bool isFilterConditionEmpty(aql::AstNode const* filterCondition) noexcept {
  return filterCondition == &kAll;
}

IResearchViewNode::IResearchViewNode(
    aql::ExecutionPlan& plan, aql::ExecutionNodeId id, TRI_vocbase_t& vocbase,
    std::shared_ptr<const LogicalView> view, aql::Variable const& outVariable,
    aql::AstNode* filterCondition, aql::AstNode* options,
    std::vector<Scorer>&& scorers)
    : aql::ExecutionNode{&plan, id},
      _vocbase{vocbase},
      _view{std::move(view)},
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
      _outVariable{aql::Variable::varFromVPack(plan.getAst(), base,
                                               NODE_OUT_VARIABLE_PARAM)},
      _outNonMaterializedDocId{aql::Variable::varFromVPack(
          plan.getAst(), base, NODE_OUT_NM_DOC_PARAM, true)},
      _outNonMaterializedColPtr{aql::Variable::varFromVPack(
          plan.getAst(), base, NODE_OUT_NM_COL_PARAM, true)},
      // in case if filter is not specified
      // set it to surrogate 'RETURN ALL' node
      _filterCondition{&kAll},
      _scorers{fromVelocyPack(plan, base.get(NODE_SCORERS_PARAM))} {
  if ((_outNonMaterializedColPtr != nullptr) !=
      (_outNonMaterializedDocId != nullptr)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("invalid node config, '", NODE_OUT_NM_DOC_PARAM,
                     "' attribute should be consistent with '",
                     NODE_OUT_NM_COL_PARAM, "' attribute"));
  }
  auto const viewIdSlice = base.get(NODE_VIEW_ID_PARAM);
  if (!viewIdSlice.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("invalid vpack format, '", NODE_VIEW_ID_PARAM,
                     "' attribute is intended to be a string"));
  }
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
  if (!ServerState::instance()->isDBServer() && !_view) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        absl::StrCat("unable to find ArangoSearch view with id '", viewIdOrName,
                     "'"));
  }
  // filter condition
  auto const conditionSlice = base.get(NODE_CONDITION_PARAM);
  if (conditionSlice.isObject() && !conditionSlice.isEmptyObject()) {
    // AST will own the node
    setFilterCondition(plan.getAst()->createNode(conditionSlice));
  }
  // shards
  auto const shardsSlice = base.get(NODE_SHARDS_PARAM);
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
            << (_view ? _view->name() : "") << "'";
        continue;
      }
      _shards[shard->name()];
    }
    auto const indexesSlice = base.get(NODE_INDEXES_PARAM);
    if (indexesSlice.isArray()) {
      velocypack::ArrayIterator indexIt{indexesSlice};
      TRI_ASSERT(indexIt.size() % 2 == 0);
      for (; indexIt.valid(); ++indexIt) {
        auto const indexId = (*indexIt).getNumber<uint64_t>();
        auto const shardIndex = (*++indexIt).getNumber<uint64_t>();
        auto const shardName = shardsSlice.at(shardIndex).stringView();
        auto const* shard = collections.get(shardName);
        if (!shard) {
          continue;
        }
        _shards[shard->name()].emplace_back(indexId);
      }
    } else if (!_view || _view->type() == ViewType::kSearch) {
      LOG_TOPIC("a48f5", ERR, iresearch::TOPIC)
          << "invalid 'IResearchViewNode' json format: unable to find "
             "'indexes' array";
    }
  } else {
    LOG_TOPIC("a48f3", ERR, iresearch::TOPIC)
        << "invalid 'IResearchViewNode' json format: unable to find 'shards' "
           "array";
  }
  // options
  TRI_ASSERT(plan.getAst());
  auto const options = base.get(NODE_OPTIONS_PARAM);
  if (!fromVelocyPack(options, _options)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "failed to parse 'IResearchViewNode' options: " + options.toString());
  }
  // volatility mask
  auto const volatilityMaskSlice = base.get(NODE_VOLATILITY_PARAM);
  if (volatilityMaskSlice.isNumber()) {
    _volatilityMask = volatilityMaskSlice.getNumber<int>();
  }
  // primary sort
  auto const sortSlice = base.get(NODE_PRIMARY_SORT_PARAM);
  if (!sortSlice.isNone()) {
    std::string error;
    IResearchSortBase vpackSort;
    if (!vpackSort.fromVelocyPack(sortSlice, error)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("failed to parse 'IResearchViewNode' primary sort: ",
                       sortSlice.toString(), ", error: '", error, "'"));
    }
    auto const& sort = *[&]() -> IResearchSortBase const* {
      if (!_view) {
        return &vpackSort;
      }
      auto& viewSort = primarySort(*_view);
      if (vpackSort != viewSort) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("primary sort ", sortSlice.toString(),
                         " for 'IResearchViewNode' doesn't match the one "
                         "specified in view '",
                         _view->name(), "'"));
      }
      return &viewSort;
    }();
    auto sortBuckets = sort.size();
    if (sortBuckets != 0) {
      auto const sortBucketsSlice = base.get(NODE_PRIMARY_SORT_BUCKETS_PARAM);
      if (!sortBucketsSlice.isNone()) {
        if (!sortBucketsSlice.isNumber()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "invalid vpack format: 'primarySortBuckets' attribute is "
              "intended to be a number");
        }
        sortBuckets = sortBucketsSlice.getNumber<size_t>();
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
      }

      if (&sort == &vpackSort) {
        _sortBuckets = sortBuckets;
        _sort = std::make_shared<IResearchSortBase const>(std::move(vpackSort));
      } else {
        setSort(sort, sortBuckets);
      }
    }
  }
  // stored values
  if (!_view) {
    auto const valuesSlice = base.get(NODE_STORED_VALUES_PARAM);
    std::string error;
    IResearchViewStoredValues vpackValues;
    if (!vpackValues.fromVelocyPack(valuesSlice, error)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("failed to parse 'IResearchViewNode' stored values: ",
                       sortSlice.toString(), ", error: '", error, "'"));
    }
    _storedValues = std::make_shared<IResearchViewStoredValues const>(
        std::move(vpackValues));
  }

  auto const noMaterializationSlice = base.get(NODE_VIEW_NO_MATERIALIZATION);
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
    auto const viewValuesVarsSlice = base.get(NODE_VIEW_VALUES_VARS);
    if (!viewValuesVarsSlice.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "'viewValuesVars' attribute should be an array");
    }
    ViewValuesVars viewValuesVars;
    for (auto const columnFieldsVars :
         velocypack::ArrayIterator(viewValuesVarsSlice)) {
      auto const columnNumberSlice =
          columnFieldsVars.get(NODE_VIEW_VALUES_VAR_COLUMN_NUMBER);
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
          columnFieldsVars.get(NODE_VIEW_STORED_VALUES_VARS);
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
  if ((base.hasKey(NODE_VIEW_SCORERS_SORT) ^
       base.hasKey(NODE_VIEW_SCORERS_SORT_LIMIT))) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "'scorersSort' and 'scorersSortLimit' attributes should be both "
        "present or both absent");
  }
  auto const scorersSortSlice = base.get(NODE_VIEW_SCORERS_SORT);
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
      auto index = scorersSortElement.get(NODE_VIEW_SCORERS_SORT_INDEX);
      auto asc = scorersSortElement.get(NODE_VIEW_SCORERS_SORT_ASC);
      if (index.isNumber() && asc.isBoolean()) {
        auto indexVal = index.getNumber<size_t>();
        if (indexVal >= _scorers.size()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("'scorersSort[", itr.index(),
                           "].index' attribute is out of range"));
        }
        _scorersSort.emplace_back(index.getNumber<size_t>(), asc.getBool());
      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER, absl::StrCat("'scorersSort[", itr.index(),
                                                  "]' attribute is invalid "));
      }
    }
  }
  auto const scorersSortLimitSlice = base.get(NODE_VIEW_SCORERS_SORT_LIMIT);
  if (!scorersSortLimitSlice.isNone()) {
    if (!scorersSortLimitSlice.isNumber()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "'scorersSortLimit' attribute should be a numeric");
    }
    _scorersSortLimit = scorersSortLimitSlice.getNumber<size_t>();
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

/// @brief doToVelocyPack, for EnumerateViewNode
void IResearchViewNode::doToVelocyPack(VPackBuilder& nodes,
                                       unsigned flags) const {
  // system info
  nodes.add(NODE_DATABASE_PARAM, VPackValue(_vocbase.name()));
  TRI_ASSERT(_view);
  // need 'view' field to correctly print view name in JS explanation
  nodes.add(NODE_VIEW_NAME_PARAM, VPackValue(_view->name()));
  absl::AlphaNum viewId{_view->id().id()};
  nodes.add(NODE_VIEW_ID_PARAM, VPackValue(viewId.Piece()));

  // our variable
  nodes.add(VPackValue(NODE_OUT_VARIABLE_PARAM));
  _outVariable->toVelocyPack(nodes);

  if (_outNonMaterializedDocId != nullptr) {
    nodes.add(VPackValue(NODE_OUT_NM_DOC_PARAM));
    _outNonMaterializedDocId->toVelocyPack(nodes);
  }

  if (_outNonMaterializedColPtr != nullptr) {
    nodes.add(VPackValue(NODE_OUT_NM_COL_PARAM));
    _outNonMaterializedColPtr->toVelocyPack(nodes);
  }

  if (_noMaterialization) {
    nodes.add(NODE_VIEW_NO_MATERIALIZATION, VPackValue(_noMaterialization));
  }

  if (!_scorersSort.empty() && _scorersSortLimit) {
    nodes.add(NODE_VIEW_SCORERS_SORT_LIMIT, VPackValue(_scorersSortLimit));
    VPackArrayBuilder scorersSort(&nodes, NODE_VIEW_SCORERS_SORT);
    for (auto const& s : _scorersSort) {
      VPackObjectBuilder scorer(&nodes);
      nodes.add(NODE_VIEW_SCORERS_SORT_INDEX, VPackValue(s.first));
      nodes.add(NODE_VIEW_SCORERS_SORT_ASC, VPackValue(s.second));
    }
  }

  // stored values
  {
    TRI_ASSERT(_view);
    auto const& values = storedValues(*_view);
    {
      VPackArrayBuilder arrayScope{&nodes, NODE_STORED_VALUES_PARAM};
      values.toVelocyPack(nodes);
    }
    auto const& sort = primarySort(*_view);
    VPackArrayBuilder arrayScope(&nodes, NODE_VIEW_VALUES_VARS);
    std::string fieldName;
    for (auto const& columnFieldsVars : _outNonMaterializedViewVars) {
      if (columnFieldsVars.first != kSortColumnNumber) {
        VPackObjectBuilder objectScope(&nodes);
        auto const& columns = values.columns();
        auto const storedColumnNumber =
            static_cast<size_t>(columnFieldsVars.first);
        TRI_ASSERT(storedColumnNumber < columns.size());
        nodes.add(NODE_VIEW_VALUES_VAR_COLUMN_NUMBER,
                  VPackValue(static_cast<size_t>(columnFieldsVars.first)));
        VPackArrayBuilder arrayScope(&nodes, NODE_VIEW_STORED_VALUES_VARS);
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
  nodes.add(VPackValue(NODE_CONDITION_PARAM));
  if (!isFilterConditionEmpty(_filterCondition)) {
    _filterCondition->toVelocyPack(nodes, flags != 0);
  } else {
    nodes.openObject();
    nodes.close();
  }

  // sort condition
  nodes.add(VPackValue(NODE_SCORERS_PARAM));
  iresearch::toVelocyPack(nodes, _scorers, flags != 0);
  // TODO object<shard, array<index>>
  // shards
  {
    VPackArrayBuilder arrayScope(&nodes, NODE_SHARDS_PARAM);
    for (auto const& [shard, _] : _shards) {
      nodes.add(VPackValue(shard));
    }
  }
  // indexes
  {
    VPackArrayBuilder arrayScope(&nodes, NODE_INDEXES_PARAM, true);
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
  nodes.add(VPackValue(NODE_OPTIONS_PARAM));
  iresearch::toVelocyPack(nodes, _options);

  // volatility mask
  nodes.add(NODE_VOLATILITY_PARAM, VPackValue(_volatilityMask));

  // primarySort
  if (_sort && !_sort->empty()) {
    {
      VPackArrayBuilder arrayScope{&nodes, NODE_PRIMARY_SORT_PARAM};
      _sort->toVelocyPack(nodes);
    }
    nodes.add(NODE_PRIMARY_SORT_BUCKETS_PARAM, VPackValue(_sortBuckets));
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

/// @brief clone ExecutionNode recursively
aql::ExecutionNode* IResearchViewNode::clone(aql::ExecutionPlan* plan,
                                             bool withDependencies,
                                             bool withProperties) const {
  TRI_ASSERT(plan);

  auto* outVariable = _outVariable;
  auto* outNonMaterializedDocId = _outNonMaterializedDocId;
  auto* outNonMaterializedColId = _outNonMaterializedColPtr;
  auto outNonMaterializedViewVars = _outNonMaterializedViewVars;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
    if (outNonMaterializedDocId != nullptr) {
      TRI_ASSERT(_outNonMaterializedColPtr != nullptr);
      outNonMaterializedDocId =
          plan->getAst()->variables()->createVariable(outNonMaterializedDocId);
    }
    if (outNonMaterializedColId != nullptr) {
      TRI_ASSERT(_outNonMaterializedDocId != nullptr);
      outNonMaterializedColId =
          plan->getAst()->variables()->createVariable(outNonMaterializedColId);
    }
    for (auto& columnFieldsVars : outNonMaterializedViewVars) {
      for (auto& fieldVar : columnFieldsVars.second) {
        fieldVar.var =
            plan->getAst()->variables()->createVariable(fieldVar.var);
      }
    }
  }

  auto node = std::make_unique<IResearchViewNode>(
      *plan, _id, _vocbase, _view, *outVariable,
      const_cast<aql::AstNode*>(_filterCondition), nullptr,
      decltype(_scorers)(_scorers));
  node->_shards = _shards;
  node->_options = _options;
  node->_volatilityMask = _volatilityMask;
  node->_sort = _sort;
  node->_optState = _optState;
  if (outNonMaterializedColId != nullptr &&
      outNonMaterializedDocId != nullptr) {
    node->setLateMaterialized(*outNonMaterializedColId,
                              *outNonMaterializedDocId);
  }
  node->_noMaterialization = _noMaterialization;
  node->_outNonMaterializedViewVars = std::move(outNonMaterializedViewVars);
  node->setScorersSort(std::vector<std::pair<size_t, bool>>(_scorersSort),
                       _scorersSortLimit);
  return cloneHelper(std::move(node), withDependencies, withProperties);
}

bool IResearchViewNode::empty() const noexcept {
  if (_view) {
    return _view->visitCollections(sEmptyView);
  } else {
    TRI_ASSERT(false);
    return _shards.empty();
  }
}

/// @brief the cost of an enumerate view node
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
          collection->count(&trx, transaction::CountType::TryCache);
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

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void IResearchViewNode::replaceVariables(
    std::unordered_map<aql::VariableId, aql::Variable const*> const&
        replacements) {
  aql::AstNode const& search = filterCondition();
  if (isFilterConditionEmpty(&search)) {
    return;
  }
  aql::VarSet variables;
  aql::Ast::getReferencedVariables(&search, variables);
  // check if the search condition uses any of the variables that we want to
  // replace
  aql::AstNode* cloned = nullptr;
  for (auto const& it : variables) {
    if (replacements.find(it->id) != replacements.end()) {
      if (cloned == nullptr) {
        // only clone the original search condition once
        cloned = plan()->getAst()->clone(&search);
      }
      plan()->getAst()->replaceVariables(cloned, replacements);
    }
  }

  if (cloned != nullptr) {
    // exchange the filter condition
    setFilterCondition(cloned);
  }
}

/// @brief getVariablesUsedHere, modifying the set in-place
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
    reserve += 1;
  }
  vars.reserve(reserve);

  std::transform(_scorers.cbegin(), _scorers.cend(), std::back_inserter(vars),
                 [](auto const& scorer) { return scorer.var; });
  if (isLateMaterialized() || isNoMaterialization()) {
    if (isLateMaterialized()) {
      vars.emplace_back(_outNonMaterializedColPtr);
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
    aql::ExecutionEngine& engine,
    std::unordered_map<aql::ExecutionNode*, aql::ExecutionBlock*> const&)
    const {
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
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "cannot use waitForSync with "
                                     "views and transactions");
    }

    auto const viewName = this->view() ? this->view()->name() : "";
    LOG_TOPIC("82af6", TRACE, iresearch::TOPIC)
        << "Start getting snapshot for view '" << viewName << "'";
    ViewSnapshotPtr reader;
    // we manage snapshot differently in single-server/db server,
    // see description of functions below to learn how
    if (ServerState::instance()->isDBServer()) {
      reader = snapshotDBServer(*this, *trx);
    } else {
      reader = snapshotSingleServer(*this, *trx);
    }
    if (!reader) {
      LOG_TOPIC("9bb93", WARN, iresearch::TOPIC)
          << "failed to get snapshot while creating arangosearch view "
             "ExecutionBlock for view '"
          << viewName << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "failed to get snapshot while creating "
                                     "arangosearch view ExecutionBlock");
    }
    return reader;
  };

  auto const buildExecutorInfo = [this](aql::ExecutionEngine& engine,
                                        ViewSnapshotPtr reader) {
    // We could be asked to produce only document/collection ids for later
    // materialization or full document body at once
    aql::RegisterCount numDocumentRegs = 0;
    MaterializeType materializeType = MaterializeType::Undefined;
    if (isLateMaterialized()) {
      TRI_ASSERT(!isNoMaterialization());
      materializeType = MaterializeType::LateMaterialize;
      numDocumentRegs += 2;
    } else if (isNoMaterialization()) {
      TRI_ASSERT(options().noMaterialization);
      materializeType = MaterializeType::NotMaterialize;
    } else {
      materializeType = MaterializeType::Materialize;
      numDocumentRegs += 1;
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
    auto writableOutputRegisters = aql::RegIdSet{};
    writableOutputRegisters.reserve(numDocumentRegs + numScoreRegisters +
                                    numViewVarsRegisters);

    auto const outRegister =
        std::invoke([&]() -> aql::IResearchViewExecutorInfos::OutRegisters {
          if (isLateMaterialized()) {
            aql::RegisterId documentRegId =
                variableToRegisterId(_outNonMaterializedDocId);
            aql::RegisterId collectionRegId =
                variableToRegisterId(_outNonMaterializedColPtr);

            writableOutputRegisters.emplace(documentRegId);
            writableOutputRegisters.emplace(collectionRegId);
            return aql::IResearchViewExecutorInfos::LateMaterializeRegister{
                documentRegId, collectionRegId};
          } else if (isNoMaterialization()) {
            return aql::IResearchViewExecutorInfos::NoMaterializeRegisters{};
          } else {
            auto outReg = variableToRegisterId(_outVariable);
            writableOutputRegisters.emplace(outReg);
            return aql::IResearchViewExecutorInfos::MaterializeRegisters{
                outReg};
          }
        });

    std::vector<aql::RegisterId> scoreRegisters;
    scoreRegisters.reserve(numScoreRegisters);
    std::for_each(_scorers.begin(), _scorers.end(), [&](auto const& scorer) {
      auto registerId = variableToRegisterId(scorer.var);
      writableOutputRegisters.emplace(registerId);
      scoreRegisters.emplace_back(registerId);
    });

    auto const& varInfos =
        getRegisterPlan()->varInfo;  // TODO remove if not needed

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
    TRI_ASSERT(_view || _storedValues);
    auto executorInfos = aql::IResearchViewExecutorInfos{
        std::move(reader),
        outRegister,
        std::move(scoreRegisters),
        engine.getQuery(),
        scorers(),
        sort(),
        _view ? storedValues(*_view) : *_storedValues,
        *plan(),
        outVariable(),
        filterCondition(),
        volatility(),
        (_view && _view->type() == ViewType::kView),
        getRegisterPlan()->varInfo,  // ??? do we need this?
        getDepth(),
        std::move(outNonMaterializedViewRegs),
        _options.countApproximate,
        filterOptimization(),
        _scorersSort,
        _scorersSortLimit};
    return std::make_tuple(materializeType, std::move(executorInfos),
                           std::move(registerInfos));
  };
  if (ServerState::instance()->isCoordinator()) {
    // coordinator in a cluster: empty view case
    return createNoResultsExecutor(engine);
  }

  auto reader = createSnapshot(engine);
  if (reader->size() == 0) {
    return createNoResultsExecutor(engine);
  }

  auto [materializeType, executorInfos, registerInfos] =
      buildExecutorInfo(engine, std::move(reader));
  // guaranteed by optimizer rule
  TRI_ASSERT(_sort == nullptr || !_sort->empty());
  bool const ordered = !_scorers.empty();
  bool const sorted = _sort != nullptr;
  bool const heapsort = !_scorersSort.empty();
#ifdef USE_ENTERPRISE
  auto& engineSelectorFeature =
      _vocbase.server().getFeature<EngineSelectorFeature>();
  bool const encrypted =
      engineSelectorFeature.isRocksDB() &&
      engineSelectorFeature.engine<RocksDBEngine>().isEncryptionEnabled();
#endif

  switch (materializeType) {
    case MaterializeType::NotMaterialize:
      return kExecutors<false,
                        MaterializeType::NotMaterialize>[getExecutorIndex(
          sorted, ordered, heapsort)](&engine, this, std::move(registerInfos),
                                      std::move(executorInfos));
    case MaterializeType::LateMaterialize:
      return kExecutors<false,
                        MaterializeType::LateMaterialize>[getExecutorIndex(
          sorted, ordered, heapsort)](&engine, this, std::move(registerInfos),
                                      std::move(executorInfos));
    case MaterializeType::Materialize:
      return kExecutors<false, MaterializeType::Materialize>[getExecutorIndex(
          sorted, ordered, heapsort)](&engine, this, std::move(registerInfos),
                                      std::move(executorInfos));
    case MaterializeType::NotMaterialize | MaterializeType::UseStoredValues:
#ifdef USE_ENTERPRISE
      if (encrypted) {
        return kExecutors<
            true, MaterializeType::NotMaterialize |
                      MaterializeType::UseStoredValues>[getExecutorIndex(
            sorted, ordered, heapsort)](&engine, this, std::move(registerInfos),
                                        std::move(executorInfos));
      }
#endif
      return kExecutors<false,
                        MaterializeType::NotMaterialize |
                            MaterializeType::UseStoredValues>[getExecutorIndex(
          sorted, ordered, heapsort)](&engine, this, std::move(registerInfos),
                                      std::move(executorInfos));
    case MaterializeType::LateMaterialize | MaterializeType::UseStoredValues:
#ifdef USE_ENTERPRISE
      if (encrypted) {
        return kExecutors<
            true, MaterializeType::LateMaterialize |
                      MaterializeType::UseStoredValues>[getExecutorIndex(
            sorted, ordered, heapsort)](&engine, this, std::move(registerInfos),
                                        std::move(executorInfos));
      }
#endif
      return kExecutors<false,
                        MaterializeType::LateMaterialize |
                            MaterializeType::UseStoredValues>[getExecutorIndex(
          sorted, ordered, heapsort)](&engine, this, std::move(registerInfos),
                                      std::move(executorInfos));
    default:
      ADB_UNREACHABLE;
  }
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
    std::vector<aql::CalculationNode*> const& calcNodes,
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

}  // namespace arangodb::iresearch
