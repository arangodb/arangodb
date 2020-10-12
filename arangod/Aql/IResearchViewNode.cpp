////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "RegisterPlan.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "types.h"

#include <velocypack/Iterator.h>

namespace {

using namespace arangodb;
using namespace arangodb::iresearch;

////////////////////////////////////////////////////////////////////////////////
/// @brief surrogate root for all queries without a filter
////////////////////////////////////////////////////////////////////////////////
aql::AstNode const ALL(aql::AstNodeValue(true));

inline bool filterConditionIsEmpty(aql::AstNode const* filterCondition) {
  return filterCondition == &ALL;
}

// -----------------------------------------------------------------------------
// --SECTION--       helpers for std::vector<arangodb::iresearch::IResearchSort>
// -----------------------------------------------------------------------------

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

std::vector<Scorer> fromVelocyPack(aql::ExecutionPlan& plan, velocypack::Slice const& slice) {
  if (!slice.isArray()) {
    LOG_TOPIC("b50b2", ERR, arangodb::iresearch::TOPIC)
        << "invalid json format detected while building IResearchViewNode "
           "sorting from velocy pack, array expected";
    return {};
  }

  auto* ast = plan.getAst();
  TRI_ASSERT(ast);
  auto const* vars = plan.getAst()->variables();
  TRI_ASSERT(vars);

  std::vector<Scorer> scorers;

  size_t i = 0;
  for (auto const sortSlice : velocypack::ArrayIterator(slice)) {
    auto const varIdSlice = sortSlice.get("id");

    if (!varIdSlice.isNumber()) {
      LOG_TOPIC("c3790", ERR, arangodb::iresearch::TOPIC)
          << "malformed variable identifier at line '" << i << "', number expected";
      return {};
    }

    auto const varId = varIdSlice.getNumber<aql::VariableId>();
    auto const* var = vars->getVariable(varId);

    if (!var) {
      LOG_TOPIC("4eeb9", ERR, arangodb::iresearch::TOPIC)
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

// -----------------------------------------------------------------------------
// --SECTION--                            helpers for IResearchViewNode::Options
// -----------------------------------------------------------------------------
namespace {
std::map<std::string, arangodb::aql::ConditionOptimization> const conditionOptimizationTypeMap = {
    {"auto", arangodb::aql::ConditionOptimization::Auto},
    {"nodnf", arangodb::aql::ConditionOptimization::NoDNF},
    {"noneg", arangodb::aql::ConditionOptimization::NoNegation},
    {"none", arangodb::aql::ConditionOptimization::None}};
}

void toVelocyPack(velocypack::Builder& builder, IResearchViewNode::Options const& options) {
  VPackObjectBuilder objectScope(&builder);
  builder.add("waitForSync", VPackValue(options.forceSync));
  {
    for (auto const& r : conditionOptimizationTypeMap) {
      if (r.second == options.conditionOptimization) {
        builder.add("conditionOptimization", VPackValue(r.first));
        break;
      }
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
}

bool fromVelocyPack(velocypack::Slice optionsSlice, IResearchViewNode::Options& options) {
  if (optionsSlice.isNone()) {
    // no options specified
    return true;
  }

  if (!optionsSlice.isObject()) {
    return false;
  }

  // forceSync
  {
    auto const optionSlice = optionsSlice.get("waitForSync");

    if (!optionSlice.isNone()) {
      // 'waitForSync' is optional
      if (!optionSlice.isBool()) {
        return false;
      }

      options.forceSync = optionSlice.getBool();
    }
  }

  // conditionOptimization
  {
    auto const conditionOptimizationSlice =
        optionsSlice.get("conditionOptimization");
    if (!conditionOptimizationSlice.isNone() && !conditionOptimizationSlice.isNull()) {
      if (!conditionOptimizationSlice.isString()) {
        return false;
      }
      VPackValueLength l;
      auto type = conditionOptimizationSlice.getString(l);
      irs::string_ref typeStr(type, l);
      auto conditionTypeIt = conditionOptimizationTypeMap.find(typeStr);
      if (conditionTypeIt == conditionOptimizationTypeMap.end()) {
        return false;
      }
      options.conditionOptimization = conditionTypeIt->second;
    }
  }

  // collections
  {
    auto const optionSlice = optionsSlice.get("collections");

    if (!optionSlice.isNone() && !optionSlice.isNull()) {
      if (!optionSlice.isArray()) {
        return false;
      }

      for (auto idSlice : VPackArrayIterator(optionSlice)) {
        if (!idSlice.isNumber()) {
          return false;
        }

        arangodb::DataSourceId const cid{
            idSlice.getNumber<arangodb::DataSourceId::BaseType>()};

        if (!cid) {
          return false;
        }

        options.sources.insert(cid);
      }

      options.restrictSources = true;
    }
  }

  // noMaterialization
  {
    auto const optionSlice = optionsSlice.get("noMaterialization");
    if (!optionSlice.isNone()) {
      // 'noMaterialization' is optional
      if (!optionSlice.isBool()) {
        return false;
      }

      options.noMaterialization = optionSlice.getBool();
    }
  }

  return true;
}

bool parseOptions(aql::QueryContext& query, LogicalView const& view, aql::AstNode const* optionsNode,
                  IResearchViewNode::Options& options, std::string& error) {
  typedef bool (*OptionHandler)(aql::QueryContext&, LogicalView const& view, aql::AstNode const&,
                                IResearchViewNode::Options&, std::string&);

  static std::map<irs::string_ref, OptionHandler> const Handlers{
      // cppcheck-suppress constStatement
      {"collections",
       [](aql::QueryContext& query, LogicalView const& view, aql::AstNode const& value,
          IResearchViewNode::Options& options, std::string& error) {
         if (value.isNullValue()) {
           // have nothing to restrict
           return true;
         }

         if (!value.isArray()) {
           error =
               "null value or array of strings or numbers"
               " is expected for option 'collections'";
           return false;
         }

         auto& resolver = query.resolver();
         ::arangodb::containers::HashSet<DataSourceId> sources;

         // get list of CIDs for restricted collections
         for (size_t i = 0, n = value.numMembers(); i < n; ++i) {
           auto const* sub = value.getMemberUnchecked(i);
           TRI_ASSERT(sub);

           switch (sub->value.type) {
             case aql::VALUE_TYPE_INT: {
               sources.insert(DataSourceId{
                   static_cast<DataSourceId::BaseType>(sub->getIntValue(true))});
               break;
             }

             case aql::VALUE_TYPE_STRING: {
               auto name = sub->getString();

               auto collection = resolver.getCollection(name);

               if (!collection) {
                 // check if DataSourceId is passed as string
                 DataSourceId const cid{NumberUtils::atoi_zero<DataSourceId::BaseType>(
                     name.data(), name.data() + name.size())};

                 collection = resolver.getCollection(cid);

                 if (!collection) {
                   error = "invalid data source name '" + name +
                           "' while parsing option 'collections'";
                   return false;
                 }
               }

               sources.insert(collection->id());
               break;
             }

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
         auto checkCids = [&sources, &sourcesFound](DataSourceId cid) {
           sourcesFound += size_t(sources.contains(cid));
           return true;
         };
         view.visitCollections(checkCids);

         if (sourcesFound != sources.size()) {
           error = "only " + basics::StringUtils::itoa(sourcesFound) +
                   " out of " + basics::StringUtils::itoa(sources.size()) +
                   " provided collection(s) in option 'collections' are "
                   "registered with the view '" +
                   view.name() + "'";
           return false;
         }

         // parsing is done
         options.sources = std::move(sources);
         options.restrictSources = true;

         return true;
       }},
      // cppcheck-suppress constStatement
      {"waitForSync", [](aql::QueryContext& /*query*/, LogicalView const& /*view*/,
                         aql::AstNode const& value,
                         IResearchViewNode::Options& options, std::string& error) {
         if (!value.isValueType(aql::VALUE_TYPE_BOOL)) {
           error = "boolean value expected for option 'waitForSync'";
           return false;
         }

         options.forceSync = value.getBoolValue();
         return true;
       }},
      // cppcheck-suppress constStatement
      {"noMaterialization", [](aql::QueryContext& /*query*/, LogicalView const& /*view*/,
                               aql::AstNode const& value,
                               IResearchViewNode::Options& options, std::string& error) {
         if (!value.isValueType(aql::VALUE_TYPE_BOOL)) {
           error = "boolean value expected for option 'noMaterialization'";
           return false;
         }

         options.noMaterialization = value.getBoolValue();
         return true;
       }},
     // cppcheck-suppress constStatement
     {"conditionOptimization", [](aql::QueryContext& /*query*/, LogicalView const& /*view*/,
                                  aql::AstNode const& value,
                                  IResearchViewNode::Options& options, std::string& error) {
         if (!value.isValueType(aql::VALUE_TYPE_STRING)) {
           error = "string value expected for option 'conditionOptimization'";
           return false;
         }
         auto type = value.getString();
         auto conditionTypeIt = conditionOptimizationTypeMap.find(type);
         if (conditionTypeIt == conditionOptimizationTypeMap.end()) {
           error =
               "unknown value '" + type + "' for option 'conditionOptimization'";
           return false;
         }
         options.conditionOptimization = conditionTypeIt->second;
         return true;
       }}};

  if (!optionsNode) {
    // nothing to parse
    return true;
  }

  if (aql::NODE_TYPE_OBJECT != optionsNode->type) {
    // must be an object
    return false;
  }

  const size_t n = optionsNode->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto const* attribute = optionsNode->getMemberUnchecked(i);

    if (!attribute || attribute->type != aql::NODE_TYPE_OBJECT_ELEMENT ||
        !attribute->isValueType(aql::VALUE_TYPE_STRING) || !attribute->numMembers()) {
      // invalid or malformed node detected
      return false;
    }

    irs::string_ref const attributeName(attribute->getStringValue(),
                                        attribute->getStringLength());

    auto const handler = Handlers.find(attributeName);

    if (handler == Handlers.end()) {
      // no handler found for attribute
      continue;
    }

    auto const* value = attribute->getMemberUnchecked(0);

    if (!value) {
      // can't handle attribute
      return false;
    }

    if (!value->isConstant()) {
      // 'Ast::injectBindParameters` doesn't handle
      // constness of parent nodes correctly, re-evaluate flags
      value->removeFlag(aql::DETERMINED_CONSTANT);

      if (!value->isConstant()) {
        // can't handle non-const values in options
        return false;
      }
    }

    if (!handler->second(query, view, *value, options, error)) {
      // can't handle attribute
      return false;
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     other helpers
// -----------------------------------------------------------------------------
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
      case aql::ExecutionNode::K_SHORTEST_PATHS:
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
      case aql::ExecutionNode::K_SHORTEST_PATHS:
      case aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
        // we're in a loop
        return true;
      default:
        break;
    }

    cur = dep;
  }

  TRI_ASSERT(cur);
  return cur->getType() == aql::ExecutionNode::SINGLETON &&
         cur->id() != aql::ExecutionNodeId{1};  // SINGLETON nodes in subqueries have id != 1
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
  if (!::filterConditionIsEmpty(&filterCondition) && inDependentScope) {
    irs::set_bit<0>(::hasDependencies(plan, filterCondition, outVariable, vars), mask);
  }

  // evaluate sort condition volatility
  auto& scorers = node.scorers();
  if (!scorers.empty() && inDependentScope) {
    vars.clear();

    for (auto const& scorer : scorers) {
      if (::hasDependencies(plan, *scorer.node, outVariable, vars)) {
        irs::set_bit<1>(mask);
        break;
      }
    }
  }

  return mask;
}

std::function<bool(DataSourceId)> const viewIsEmpty = [](DataSourceId) {
  return false;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple irs::index_reader
/// @note it is assumed that ViewState resides in the same
///       TransactionState as the IResearchView ViewState, therefore a separate
///       lock is not required to be held
////////////////////////////////////////////////////////////////////////////////
class Snapshot : public IResearchView::Snapshot, private irs::util::noncopyable {
 public:
  typedef std::vector<std::pair<DataSourceId, irs::sub_reader const*>> readers_t;

  Snapshot(readers_t&& readers, uint64_t docs_count, uint64_t live_docs_count) noexcept
      : _readers(std::move(readers)),
        _docs_count(docs_count),
        _live_docs_count(live_docs_count) {}

  /// @brief constructs snapshot from a given snapshot
  ///        according to specified set of collections
  Snapshot(const IResearchView::Snapshot& rhs,
           ::arangodb::containers::HashSet<DataSourceId> const& collections);

  /// @returns corresponding sub-reader
  virtual const irs::sub_reader& operator[](size_t i) const noexcept override {
    assert(i < readers_.size());
    return *(_readers[i].second);
  }

  virtual DataSourceId cid(size_t i) const noexcept override {
    assert(i < readers_.size());
    return _readers[i].first;
  }

  /// @returns number of documents
  virtual uint64_t docs_count() const noexcept override { return _docs_count; }

  /// @returns number of live documents
  virtual uint64_t live_docs_count() const noexcept override {
    return _live_docs_count;
  }

  /// @returns total number of opened writers
  virtual size_t size() const noexcept override { return _readers.size(); }

 private:
  readers_t _readers;
  uint64_t _docs_count;
  uint64_t _live_docs_count;
};  // Snapshot

Snapshot::Snapshot(const IResearchView::Snapshot& rhs,
                   ::arangodb::containers::HashSet<DataSourceId> const& collections)
    : _docs_count(0), _live_docs_count(0) {
  for (size_t i = 0, size = rhs.size(); i < size; ++i) {
    auto const cid = rhs.cid(i);

    if (!collections.contains(cid)) {
      continue;
    }

    auto& segment = rhs[i];

    _docs_count += segment.docs_count();
    _live_docs_count += segment.live_docs_count();
    _readers.emplace_back(cid, &segment);
  }
}

typedef std::shared_ptr<IResearchView::Snapshot const> SnapshotPtr;

/// @brief Since cluster is not transactional and each distributed
///        part of a query starts it's own trasaction associated
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
SnapshotPtr snapshotDBServer(IResearchViewNode const& node, transaction::Methods& trx) {
  TRI_ASSERT(ServerState::instance()->isDBServer());

  static IResearchView::SnapshotMode const SNAPSHOT[]{IResearchView::SnapshotMode::FindOrCreate,
                                                      IResearchView::SnapshotMode::SyncAndReplace};

  auto& view = LogicalView::cast<IResearchView>(*node.view());
  auto& options = node.options();
  auto* resolver = trx.resolver();
  TRI_ASSERT(resolver);

  ::arangodb::containers::HashSet<DataSourceId> collections;
  for (auto& shard : node.shards()) {
    auto collection = resolver->getCollection(shard);

    if (!collection) {
      THROW_ARANGO_EXCEPTION(
          arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                           std::string("failed to find shard by id '") + shard + "'"));
    }

    if (options.restrictSources && !options.sources.contains(collection->planId())) {
      // skip restricted collections if any
      continue;
    }

    collections.emplace(collection->id());
  }

  void const* snapshotKey = nullptr;

  if (options.restrictSources) {
    // use node address as the snapshot identifier
    snapshotKey = &node;
  }

  // use aliasing ctor
  return {SnapshotPtr(), view.snapshot(trx, SNAPSHOT[size_t(options.forceSync)],
                                       &collections, snapshotKey)};
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
SnapshotPtr snapshotSingleServer(IResearchViewNode const& node, transaction::Methods& trx) {
  TRI_ASSERT(ServerState::instance()->isSingleServer());

  static IResearchView::SnapshotMode const SNAPSHOT[]{IResearchView::SnapshotMode::Find,
                                                      IResearchView::SnapshotMode::SyncAndReplace};

  auto& view = LogicalView::cast<IResearchView>(*node.view());
  auto& options = node.options();

  // use aliasing ctor
  auto reader = SnapshotPtr(SnapshotPtr(),
                            view.snapshot(trx, SNAPSHOT[size_t(options.forceSync)]));

  if (options.restrictSources && reader) {
    // reassemble reader
    reader = std::make_shared<Snapshot>(*reader, options.sources);
  }

  return reader;
}

inline IResearchViewSort const& primarySort(arangodb::LogicalView const& view) {
  if (arangodb::ServerState::instance()->isCoordinator()) {
    auto& viewImpl = arangodb::LogicalView::cast<IResearchViewCoordinator>(view);
    return viewImpl.primarySort();
  }

  auto& viewImpl = arangodb::LogicalView::cast<IResearchView>(view);
  return viewImpl.primarySort();
}

inline IResearchViewStoredValues const& storedValues(arangodb::LogicalView const& view) {
  if (arangodb::ServerState::instance()->isCoordinator()) {
    auto& viewImpl = arangodb::LogicalView::cast<IResearchViewCoordinator>(view);
    return viewImpl.storedValues();
  }

  auto& viewImpl = arangodb::LogicalView::cast<IResearchView>(view);
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
const char* NODE_OPTIONS_PARAM = "options";
const char* NODE_VOLATILITY_PARAM = "volatility";
const char* NODE_PRIMARY_SORT_PARAM = "primarySort";
const char* NODE_PRIMARY_SORT_BUCKETS_PARAM = "primarySortBuckets";
const char* NODE_VIEW_VALUES_VARS = "viewValuesVars";
const char* NODE_VIEW_STORED_VALUES_VARS = "viewStoredValuesVars";
const char* NODE_VIEW_VALUES_VAR_COLUMN_NUMBER = "columnNumber";
const char* NODE_VIEW_VALUES_VAR_FIELD_NUMBER = "fieldNumber";
const char* NODE_VIEW_VALUES_VAR_ID = "id";
const char* NODE_VIEW_VALUES_VAR_NAME = "name";
const char* NODE_VIEW_VALUES_VAR_FIELD = "field";
const char* NODE_VIEW_NO_MATERIALIZATION = "noMaterialization";

void addViewValuesVar(VPackBuilder& nodes, std::string& fieldName,
                      IResearchViewNode::ViewVariable const& fieldVar) {
  nodes.add(NODE_VIEW_VALUES_VAR_FIELD_NUMBER, VPackValue(fieldVar.fieldNum));
  nodes.add(NODE_VIEW_VALUES_VAR_ID, VPackValue(fieldVar.var->id));
  nodes.add(NODE_VIEW_VALUES_VAR_NAME, VPackValue(fieldVar.var->name));  // for explainer.js
  nodes.add(NODE_VIEW_VALUES_VAR_FIELD, VPackValue(fieldName));  // for explainer.js
}

void extractViewValuesVar(aql::VariableGenerator const* vars,
                          IResearchViewNode::ViewValuesVars& viewValuesVars,
                          ptrdiff_t const columnNumber, velocypack::Slice const& fieldVar) {
  auto const fieldNumberSlice = fieldVar.get(NODE_VIEW_VALUES_VAR_FIELD_NUMBER);
  if (!fieldNumberSlice.isNumber<size_t>()) {
    THROW_ARANGO_EXCEPTION_FORMAT(
        TRI_ERROR_BAD_PARAMETER,
        "\"viewValuesVars[*].fieldNumber\" %s should be a number",
        fieldNumberSlice.toString().c_str());
  }
  auto const fieldNumber = fieldNumberSlice.getNumber<size_t>();

  auto const varIdSlice = fieldVar.get(NODE_VIEW_VALUES_VAR_ID);
  if (!varIdSlice.isNumber<aql::VariableId>()) {
    THROW_ARANGO_EXCEPTION_FORMAT(
        TRI_ERROR_BAD_PARAMETER,
        "\"viewValuesVars[*].id\" variable id %s should be a number",
        varIdSlice.toString().c_str());
  }

  auto const varId = varIdSlice.getNumber<aql::VariableId>();
  auto const* var = vars->getVariable(varId);

  if (!var) {
    THROW_ARANGO_EXCEPTION_FORMAT(
        TRI_ERROR_BAD_PARAMETER,
        "\"viewValuesVars[*].id\" unable to find variable by id %d", varId);
  }
  viewValuesVars[columnNumber].emplace_back(IResearchViewNode::ViewVariable{fieldNumber, var});
}

template <MaterializeType materializeType>
constexpr std::unique_ptr<aql::ExecutionBlock> (*executors[])(
    aql::ExecutionEngine*, IResearchViewNode const*, aql::RegisterInfos&&,
    aql::IResearchViewExecutorInfos&&) = {
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos) -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<aql::ExecutionBlockImpl<aql::IResearchViewExecutor<false, materializeType>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos) -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<aql::ExecutionBlockImpl<aql::IResearchViewExecutor<true, materializeType>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos) -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<aql::ExecutionBlockImpl<aql::IResearchViewMergeExecutor<false, materializeType>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    },
    [](aql::ExecutionEngine* engine, IResearchViewNode const* viewNode,
       aql::RegisterInfos&& registerInfos,
       aql::IResearchViewExecutorInfos&& executorInfos) -> std::unique_ptr<aql::ExecutionBlock> {
      return std::make_unique<aql::ExecutionBlockImpl<aql::IResearchViewMergeExecutor<true, materializeType>>>(
          engine, viewNode, std::move(registerInfos), std::move(executorInfos));
    }};

constexpr size_t getExecutorIndex(bool sorted, bool ordered) {
  auto index = static_cast<size_t>(ordered) + 2 * static_cast<size_t>(sorted);
  TRI_ASSERT(index < IRESEARCH_COUNTOF(executors<MaterializeType::Materialize>));
  return index;
}

}  // namespace

namespace arangodb {
namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                  IResearchViewNode implementation
// -----------------------------------------------------------------------------

const ptrdiff_t IResearchViewNode::SortColumnNumber = -1;

IResearchViewNode::IResearchViewNode(aql::ExecutionPlan& plan,
                                     aql::ExecutionNodeId id, TRI_vocbase_t& vocbase,
                                     std::shared_ptr<const LogicalView> const& view,
                                     aql::Variable const& outVariable,
                                     aql::AstNode* filterCondition,
                                     aql::AstNode* options, std::vector<Scorer>&& scorers)
    : aql::ExecutionNode(&plan, id),
      _vocbase(vocbase),
      _view(view),
      _outVariable(&outVariable),
      _outNonMaterializedDocId(nullptr),
      _outNonMaterializedColPtr(nullptr),
      _noMaterialization(false),
      // in case if filter is not specified
      // set it to surrogate 'RETURN ALL' node
      _filterCondition(filterCondition ? filterCondition : &ALL),
      _scorers(std::move(scorers)) {
  TRI_ASSERT(_view);
  TRI_ASSERT(iresearch::DATA_SOURCE_TYPE == _view->type());
  TRI_ASSERT(LogicalView::category() == _view->category());

  auto* ast = plan.getAst();

  // FIXME any other way to validate options before object creation???
  std::string error;
  if (!parseOptions(ast->query(), *_view, options, _options, error)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid ArangoSearch options provided: " + error);
  }
}

IResearchViewNode::IResearchViewNode(aql::ExecutionPlan& plan, velocypack::Slice const& base)
    : aql::ExecutionNode(&plan, base),
      _vocbase(plan.getAst()->query().vocbase()),
      _outVariable(aql::Variable::varFromVPack(plan.getAst(), base, NODE_OUT_VARIABLE_PARAM)),
      _outNonMaterializedDocId(
          aql::Variable::varFromVPack(plan.getAst(), base, NODE_OUT_NM_DOC_PARAM, true)),
      _outNonMaterializedColPtr(
          aql::Variable::varFromVPack(plan.getAst(), base, NODE_OUT_NM_COL_PARAM, true)),
      // in case if filter is not specified
      // set it to surrogate 'RETURN ALL' node
      _filterCondition(&ALL),
      _scorers(fromVelocyPack(plan, base.get(NODE_SCORERS_PARAM))) {
  if ((_outNonMaterializedColPtr != nullptr) != (_outNonMaterializedDocId != nullptr)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        std::string("invalid node config, '")
            .append(NODE_OUT_NM_DOC_PARAM)
            .append("' attribute should be consistent with '")
            .append(NODE_OUT_NM_COL_PARAM)
            .append("' attribute"));
  }

  // view
  auto const viewIdSlice = base.get(NODE_VIEW_ID_PARAM);

  if (!viewIdSlice.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        std::string("invalid vpack format, '").append(NODE_VIEW_ID_PARAM).append("' attribute is intended to be a string"));
  }

  auto const viewId = viewIdSlice.copyString();

  if (ServerState::instance()->isSingleServer()) {
    _view = _vocbase.lookupView(DataSourceId{basics::StringUtils::uint64(viewId)});
  } else {
    // need cluster wide view
    TRI_ASSERT(_vocbase.server().hasFeature<ClusterFeature>());
    _view = _vocbase.server().getFeature<ClusterFeature>().clusterInfo().getView(
        _vocbase.name(), viewId);
  }

  if (!_view || iresearch::DATA_SOURCE_TYPE != _view->type()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        "unable to find ArangoSearch view with id '" + viewId + "'");
  }

  // filter condition
  auto const filterSlice = base.get(NODE_CONDITION_PARAM);

  if (filterSlice.isObject() && !filterSlice.isEmptyObject()) {
    // AST will own the node
    _filterCondition = plan.getAst()->createNode(filterSlice);
  }

  // shards
  auto const shardsSlice = base.get(NODE_SHARDS_PARAM);

  if (shardsSlice.isArray()) {
    TRI_ASSERT(plan.getAst());
    auto const& collections = plan.getAst()->query().collections();

    for (auto const shardSlice : velocypack::ArrayIterator(shardsSlice)) {
      auto const shardId = shardSlice.copyString();  // shardID is collection name on db server
      auto const* shard = collections.get(shardId);

      if (!shard) {
        LOG_TOPIC("6fba2", ERR, arangodb::iresearch::TOPIC)
            << "unable to lookup shard '" << shardId << "' for the view '"
            << _view->name() << "'";
        continue;
      }

      _shards.push_back(shard->name());
    }
  } else {
    LOG_TOPIC("a48f3", ERR, arangodb::iresearch::TOPIC)
        << "invalid 'IResearchViewNode' json format: unable to find 'shards' "
           "array";
  }

  // options
  TRI_ASSERT(plan.getAst());

  auto const options = base.get(NODE_OPTIONS_PARAM);

  if (!::fromVelocyPack(options, _options)) {
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
  auto const primarySortSlice = base.get(NODE_PRIMARY_SORT_PARAM);

  if (!primarySortSlice.isNone()) {
    std::string error;
    IResearchViewSort sort;
    if (!sort.fromVelocyPack(primarySortSlice, error)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "failed to parse 'IResearchViewNode' primary sort: " +
              primarySortSlice.toString() + ", error: '" + error + "'");
    }

    TRI_ASSERT(_view);
    auto& primarySort = LogicalView::cast<IResearchView>(*_view).primarySort();

    if (sort != primarySort) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "primary sort " + primarySortSlice.toString() +
                                         " for 'IResearchViewNode' doesn't "
                                         "match the one specified in view '" +
                                         _view->name() + "'");
    }

    if (!primarySort.empty()) {
      size_t primarySortBuckets = primarySort.size();

      auto const primarySortBucketsSlice = base.get(NODE_PRIMARY_SORT_BUCKETS_PARAM);

      if (!primarySortBucketsSlice.isNone()) {
        if (!primarySortBucketsSlice.isNumber()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "invalid vpack format: 'primarySortBuckets' attribute is "
              "intended to be a number");
        }

        primarySortBuckets = primarySortBucketsSlice.getNumber<size_t>();

        if (primarySortBuckets > primarySort.size()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "invalid vpack format: value of 'primarySortBuckets' attribute "
              "'" +
                  std::to_string(primarySortBuckets) +
                  "' is greater than number of buckets specified in "
                  "'primarySort' attribute '" +
                  std::to_string(primarySort.size()) + "' of the view '" +
                  _view->name() + "'");
        }
      }

      // set sort from corresponding view
      _sort.first = &primarySort;
      _sort.second = primarySortBuckets;
    }
  }

  if (base.hasKey(NODE_VIEW_NO_MATERIALIZATION)) {
    auto const noMaterializationSlice = base.get(NODE_VIEW_NO_MATERIALIZATION);
    if (!noMaterializationSlice.isBool()) {
      THROW_ARANGO_EXCEPTION_FORMAT(
          TRI_ERROR_BAD_PARAMETER,
          "\"noMaterialization\" %s should be a bool value",
          noMaterializationSlice.toString().c_str());
    }
    _noMaterialization = noMaterializationSlice.getBool();
  } else {
    _noMaterialization = false;
  }

  if (isLateMaterialized() || noMaterialization()) {
    auto const* vars = plan.getAst()->variables();
    TRI_ASSERT(vars);

    auto const viewValuesVarsSlice = base.get(NODE_VIEW_VALUES_VARS);
    if (!viewValuesVarsSlice.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "\"viewValuesVars\" attribute should be an array");
    }
    ViewValuesVars viewValuesVars;
    for (auto const columnFieldsVars : velocypack::ArrayIterator(viewValuesVarsSlice)) {
      if (columnFieldsVars.hasKey(NODE_VIEW_VALUES_VAR_COLUMN_NUMBER)) {  // not SortColumnNumber
        auto const columnNumberSlice =
            columnFieldsVars.get(NODE_VIEW_VALUES_VAR_COLUMN_NUMBER);
        if (!columnNumberSlice.isNumber<size_t>()) {
          THROW_ARANGO_EXCEPTION_FORMAT(
              TRI_ERROR_BAD_PARAMETER,
              "\"viewValuesVars[*].columnNumber\" %s should be a number",
              columnNumberSlice.toString().c_str());
        }
        auto const columnNumber = columnNumberSlice.getNumber<ptrdiff_t>();
        auto const viewStoredValuesVarsSlice =
            columnFieldsVars.get(NODE_VIEW_STORED_VALUES_VARS);
        if (!viewStoredValuesVarsSlice.isArray()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "\"viewValuesVars[*].viewStoredValuesVars\" attribute should be "
              "an array");
        }
        for (auto const fieldVar : velocypack::ArrayIterator(viewStoredValuesVarsSlice)) {
          extractViewValuesVar(vars, viewValuesVars, columnNumber, fieldVar);
        }
      } else {  // SortColumnNumber
        extractViewValuesVar(vars, viewValuesVars, SortColumnNumber, columnFieldsVars);
      }
    }
    if (!viewValuesVars.empty()) {
      _outNonMaterializedViewVars = std::move(viewValuesVars);
    }
  }
}

std::pair<bool, bool> IResearchViewNode::volatility(bool force /*=false*/) const {
  if (force || _volatilityMask < 0) {
    _volatilityMask = evaluateVolatility(*this);
  }

  return std::make_pair(irs::check_bit<0>(_volatilityMask),   // filter
                        irs::check_bit<1>(_volatilityMask));  // sort
}

/// @brief toVelocyPack, for EnumerateViewNode
void IResearchViewNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags,
                                           std::unordered_set<ExecutionNode const*>& seen) const {
  // call base class method
  aql::ExecutionNode::toVelocyPackHelperGeneric(nodes, flags, seen);

  // system info
  nodes.add(NODE_DATABASE_PARAM, VPackValue(_vocbase.name()));
  // need 'view' field to correctly print view name in JS explanation
  nodes.add(NODE_VIEW_NAME_PARAM, VPackValue(_view->name()));
  nodes.add(NODE_VIEW_ID_PARAM, VPackValue(basics::StringUtils::itoa(_view->id().id())));

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

  // stored values
  {
    auto const& primarySort = ::primarySort(*_view);
    auto const& storedValues = ::storedValues(*_view);
    VPackArrayBuilder arrayScope(&nodes, NODE_VIEW_VALUES_VARS);
    std::string fieldName;
    for (auto const& columnFieldsVars : _outNonMaterializedViewVars) {
      if (SortColumnNumber != columnFieldsVars.first) {
        VPackObjectBuilder objectScope(&nodes);
        auto const& columns = storedValues.columns();
        auto const storedColumnNumber = static_cast<size_t>(columnFieldsVars.first);
        TRI_ASSERT(storedColumnNumber < columns.size());
        nodes.add(NODE_VIEW_VALUES_VAR_COLUMN_NUMBER,
                  VPackValue(static_cast<size_t>(columnFieldsVars.first)));
        VPackArrayBuilder arrayScope(&nodes, NODE_VIEW_STORED_VALUES_VARS);
        for (auto const& fieldVar : columnFieldsVars.second) {
          VPackObjectBuilder objectScope(&nodes);
          fieldName.clear();
          TRI_ASSERT(fieldVar.fieldNum < columns[storedColumnNumber].fields.size());
          fieldName = columns[storedColumnNumber].fields[fieldVar.fieldNum].first;
          addViewValuesVar(nodes, fieldName, fieldVar);
        }
      } else {
        TRI_ASSERT(SortColumnNumber == columnFieldsVars.first);
        for (auto const& fieldVar : columnFieldsVars.second) {
          VPackObjectBuilder objectScope(&nodes);
          fieldName.clear();
          TRI_ASSERT(fieldVar.fieldNum < primarySort.fields().size());
          basics::TRI_AttributeNamesToString(primarySort.fields()[fieldVar.fieldNum],
                                             fieldName, true);
          addViewValuesVar(nodes, fieldName, fieldVar);
        }
      }
    }
  }

  // filter condition
  nodes.add(VPackValue(NODE_CONDITION_PARAM));
  if (!::filterConditionIsEmpty(_filterCondition)) {
    _filterCondition->toVelocyPack(nodes, flags != 0);
  } else {
    nodes.openObject();
    nodes.close();
  }

  // sort condition
  nodes.add(VPackValue(NODE_SCORERS_PARAM));
  ::toVelocyPack(nodes, _scorers, flags != 0);

  // shards
  {
    VPackArrayBuilder arrayScope(&nodes, NODE_SHARDS_PARAM);
    for (auto& shard : _shards) {
      nodes.add(VPackValue(shard));
    }
  }

  // options
  nodes.add(VPackValue(NODE_OPTIONS_PARAM));
  ::toVelocyPack(nodes, _options);

  // volatility mask
  nodes.add(NODE_VOLATILITY_PARAM, VPackValue(_volatilityMask));

  // primarySort
  if (_sort.first && !_sort.first->empty()) {
    {
      VPackArrayBuilder arrayScope(&nodes, NODE_PRIMARY_SORT_PARAM);
      _sort.first->toVelocyPack(nodes);
    }
    nodes.add(NODE_PRIMARY_SORT_BUCKETS_PARAM, VPackValue(_sort.second));
  }

  nodes.close();
}

std::vector<std::reference_wrapper<aql::Collection const>> IResearchViewNode::collections() const {
  TRI_ASSERT(_plan && _plan->getAst());
 auto const& collections = _plan->getAst()->query().collections();

  std::vector<std::reference_wrapper<aql::Collection const>> viewCollections;

  auto visitor = [&viewCollections, &collections](DataSourceId cid) -> bool {
    auto const id = basics::StringUtils::itoa(cid.id());
    auto const* collection = collections.get(id);

    if (collection) {
      viewCollections.push_back(*collection);
    } else {
      LOG_TOPIC("ee270", WARN, arangodb::iresearch::TOPIC)
          << "collection with id '" << id << "' is not registered with the query";
    }

    return true;
  };

  if (_options.restrictSources) {
    viewCollections.reserve(_options.sources.size());
    for (auto const cid : _options.sources) {
      visitor(cid);
    }
  } else {
    _view->visitCollections(visitor);
  }

  return viewCollections;
}

/// @brief clone ExecutionNode recursively
aql::ExecutionNode* IResearchViewNode::clone(aql::ExecutionPlan* plan, bool withDependencies,
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
        fieldVar.var = plan->getAst()->variables()->createVariable(fieldVar.var);
      }
    }
  }

  auto node =
      std::make_unique<IResearchViewNode>(*plan, _id, _vocbase, _view, *outVariable,
                                          const_cast<aql::AstNode*>(_filterCondition),
                                          nullptr, decltype(_scorers)(_scorers));
  node->_shards = _shards;
  node->_options = _options;
  node->_volatilityMask = _volatilityMask;
  node->_sort = _sort;
  node->_optState = _optState;
  if (outNonMaterializedColId != nullptr && outNonMaterializedDocId != nullptr) {
    node->setLateMaterialized(*outNonMaterializedColId, *outNonMaterializedDocId);
  }
  node->_noMaterialization = _noMaterialization;
  node->_outNonMaterializedViewVars = std::move(outNonMaterializedViewVars);
  return cloneHelper(std::move(node), withDependencies, withProperties);
}

bool IResearchViewNode::empty() const noexcept {
  return _view->visitCollections(viewIsEmpty);
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
  auto visitor = [&trx, &estimatedNrItems, &collections](DataSourceId cid) -> bool {
    auto const id = basics::StringUtils::itoa(cid.id());
    auto const* collection = collections.get(id);

    if (collection) {
      // FIXME better to gather count for multiple collections at once
      estimatedNrItems += collection->count(&trx, transaction::CountType::TryCache);
    } else {
      LOG_TOPIC("ee276", WARN, arangodb::iresearch::TOPIC)
          << "collection with id '" << id << "' is not registered with the query";
    }

    return true;
  };

  if (_options.restrictSources) {
    for (auto const cid : _options.sources) {
      visitor(cid);
    }
  } else {
    _view->visitCollections(visitor);
  }

  aql::CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedNrItems *= estimatedNrItems;
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

/// @brief getVariablesUsedHere, modifying the set in-place
void IResearchViewNode::getVariablesUsedHere(aql::VarSet& vars) const {
  auto const outVariableAlreadyInVarSet = vars.contains(_outVariable);

  if (!::filterConditionIsEmpty(_filterCondition)) {
    aql::Ast::getReferencedVariables(_filterCondition, vars);
  }

  for (auto& scorer : _scorers) {
    aql::Ast::getReferencedVariables(scorer.node, vars);
  }

  if (!outVariableAlreadyInVarSet) {
    vars.erase(_outVariable);
  }
}

std::vector<arangodb::aql::Variable const*> IResearchViewNode::getVariablesSetHere() const {
  std::vector<arangodb::aql::Variable const*> vars;
  // scorers + vars for late materialization
  auto reserve = _scorers.size() + _outNonMaterializedViewVars.size();
  // collection + docId or document
  if (isLateMaterialized()) {
    reserve += 2;
  } else if (!noMaterialization()) {
    reserve += 1;
  }
  vars.reserve(reserve);

  std::transform(_scorers.cbegin(), _scorers.cend(), std::back_inserter(vars),
                 [](auto const& scorer) { return scorer.var; });
  if (isLateMaterialized() || noMaterialization()) {
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

  if (!::filterConditionIsEmpty(_filterCondition)) {
    aql::VarSet vars;
    aql::Ast::getReferencedVariables(_filterCondition, vars);

    if (noMaterialization()) {
      vars.erase(_outVariable);
    }

    for (auto const& it : vars) {
      aql::RegisterId reg = variableToRegisterId(it);
      // The filter condition may refer to registers that are written here
      if (reg < getNrInputRegisters()) {
        inputRegs.emplace(reg);
      }
    }
  }

  return inputRegs;
}

void IResearchViewNode::filterCondition(aql::AstNode const* node) noexcept {
  _filterCondition = !node ? &ALL : node;
}

bool IResearchViewNode::filterConditionIsEmpty() const noexcept {
  return ::filterConditionIsEmpty(_filterCondition);
}

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#endif

std::unique_ptr<aql::ExecutionBlock> IResearchViewNode::createBlock(
    aql::ExecutionEngine& engine,
    std::unordered_map<aql::ExecutionNode*, aql::ExecutionBlock*> const&) const {
  auto const createNoResultsExecutor = [this](aql::ExecutionEngine& engine) {
    auto emptyRegisterInfos = createRegisterInfos({}, {});
    aql::ExecutionNode const* previousNode = getFirstDependency();
    TRI_ASSERT(previousNode != nullptr);

    return std::make_unique<aql::ExecutionBlockImpl<aql::NoResultsExecutor>>(
        &engine, this, std::move(emptyRegisterInfos), aql::EmptyExecutorInfos{});
  };

  auto const createSnapshot = [this](aql::ExecutionEngine& engine) {
    auto* trx = &engine.getQuery().trxForOptimization();

    if (!trx) {
      LOG_TOPIC("7c905", WARN, arangodb::iresearch::TOPIC)
          << "failed to get transaction while creating IResearchView "
             "ExecutionBlock";

      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "failed to get transaction while creating "
                                     "IResearchView ExecutionBlock");
    }

    if (options().forceSync &&
        trx->state()->hasHint(arangodb::transaction::Hints::Hint::GLOBAL_MANAGED)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "cannot use waitForSync with "
                                     "views and transactions");
    }

    auto& view = LogicalView::cast<IResearchView>(*this->view());

    std::shared_ptr<IResearchView::Snapshot const> reader;

    LOG_TOPIC("82af6", TRACE, arangodb::iresearch::TOPIC)
        << "Start getting snapshot for view '" << view.name() << "'";

    // we manage snapshot differently in single-server/db server,
    // see description of functions below to learn how
    if (ServerState::instance()->isDBServer()) {
      reader = snapshotDBServer(*this, *trx);
    } else {
      reader = snapshotSingleServer(*this, *trx);
    }

    if (!reader) {
      LOG_TOPIC("9bb93", WARN, arangodb::iresearch::TOPIC)
          << "failed to get snapshot while creating arangosearch view "
             "ExecutionBlock for view '"
          << view.name() << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "failed to get snapshot while creating "
                                     "arangosearch view ExecutionBlock");
    }

    return reader;
  };

  auto const buildExecutorInfo = [this](aql::ExecutionEngine& engine,
                                        std::shared_ptr<IResearchView::Snapshot const> reader) {
    // We could be asked to produce only document/collection ids for later materialization or full document body at once
    aql::RegisterCount numDocumentRegs = 0;
    MaterializeType materializeType = MaterializeType::Undefined;
    if (isLateMaterialized()) {
      TRI_ASSERT(!noMaterialization());
      materializeType = MaterializeType::LateMaterialize;
      numDocumentRegs += 2;
    } else if (noMaterialization()) {
      TRI_ASSERT(options().noMaterialization);
      materializeType = MaterializeType::NotMaterialize;
    } else {
      materializeType = MaterializeType::Materialize;
      numDocumentRegs += 1;
    }

    // We have one output register for documents, which is always the first after
    // the input registers.

    auto numScoreRegisters = static_cast<aql::RegisterCount>(_scorers.size());
    auto numViewVarsRegisters =
        std::accumulate(_outNonMaterializedViewVars.cbegin(),
                        _outNonMaterializedViewVars.cend(),
                        static_cast<aql::RegisterCount>(0),
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
    writableOutputRegisters.reserve(numDocumentRegs + numScoreRegisters + numViewVarsRegisters);

    auto const outRegister = std::invoke([&]() -> aql::IResearchViewExecutorInfos::OutRegisters {
      if (isLateMaterialized()) {
        aql::RegisterId documentRegId = variableToRegisterId(_outNonMaterializedDocId);
        aql::RegisterId collectionRegId = variableToRegisterId(_outNonMaterializedColPtr);

        writableOutputRegisters.emplace(documentRegId);
        writableOutputRegisters.emplace(collectionRegId);
        return aql::IResearchViewExecutorInfos::LateMaterializeRegister{documentRegId, collectionRegId};
      } else if (noMaterialization()) {
        return aql::IResearchViewExecutorInfos::NoMaterializeRegisters{};
      } else {
        auto outReg = variableToRegisterId(_outVariable);

        writableOutputRegisters.emplace(outReg);
        return aql::IResearchViewExecutorInfos::MaterializeRegisters{outReg};
      }
    });

    std::vector<aql::RegisterId> scoreRegisters;
    scoreRegisters.reserve(numScoreRegisters);
    std::for_each(_scorers.begin(), _scorers.end(), [&](auto const& scorer) {
      auto registerId = variableToRegisterId(scorer.var);
      writableOutputRegisters.emplace(registerId);
      scoreRegisters.emplace_back(registerId);
    });

    auto const& varInfos = getRegisterPlan()->varInfo;  // TODO remove if not needed

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
               numDocumentRegs + numScoreRegisters + numViewVarsRegisters);

    aql::RegisterInfos registerInfos =
        createRegisterInfos(calcInputRegs(), std::move(writableOutputRegisters));

    auto executorInfos =
        aql::IResearchViewExecutorInfos{std::move(reader),
                                        outRegister,
                                        std::move(scoreRegisters),
                                        engine.getQuery(),
                                        scorers(),
                                        _sort,
                                        ::storedValues(*_view),
                                        *plan(),
                                        outVariable(),
                                        filterCondition(),
                                        volatility(),
                                        getRegisterPlan()->varInfo,   // ??? do we need this?
                                        getDepth(),
                                        std::move(outNonMaterializedViewRegs)};

    return std::make_tuple(materializeType, std::move(executorInfos), std::move(registerInfos));
  };

  if (ServerState::instance()->isCoordinator()) {
    // coordinator in a cluster: empty view case
    return createNoResultsExecutor(engine);
  }

  std::shared_ptr<IResearchView::Snapshot const> reader = createSnapshot(engine);
  if (0 == reader->size()) {
    return createNoResultsExecutor(engine);
  }

  auto [materializeType, executorInfos, registerInfos] = buildExecutorInfo(engine, std::move(reader));

  TRI_ASSERT(_sort.first == nullptr || !_sort.first->empty());  // guaranteed by optimizer rule
  bool const ordered = !_scorers.empty();
  switch (materializeType) {
    case MaterializeType::NotMaterialize:
      return ::executors<MaterializeType::NotMaterialize>[getExecutorIndex(_sort.first != nullptr, ordered)](
          &engine, this, std::move(registerInfos), std::move(executorInfos));
    case MaterializeType::LateMaterialize:
      return ::executors<MaterializeType::LateMaterialize>[getExecutorIndex(_sort.first != nullptr, ordered)](
          &engine, this, std::move(registerInfos), std::move(executorInfos));
    case MaterializeType::Materialize:
      return ::executors<MaterializeType::Materialize>[getExecutorIndex(_sort.first != nullptr, ordered)](
          &engine, this, std::move(registerInfos), std::move(executorInfos));
    case MaterializeType::NotMaterialize | MaterializeType::UseStoredValues:
      return ::executors<MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>[getExecutorIndex(
          _sort.first != nullptr, ordered)](&engine, this, std::move(registerInfos),
                                            std::move(executorInfos));
    case MaterializeType::LateMaterialize | MaterializeType::UseStoredValues:
      return ::executors<MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>[getExecutorIndex(
          _sort.first != nullptr, ordered)](&engine, this, std::move(registerInfos),
                                            std::move(executorInfos));
    default:
      ADB_UNREACHABLE;
  }
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

bool IResearchViewNode::OptimizationState::canVariablesBeReplaced(aql::CalculationNode* calclulationNode) const {
  return _nodesToChange.find(calclulationNode) != _nodesToChange.cend();  // contains()
}

void IResearchViewNode::OptimizationState::saveCalcNodesForViewVariables(
    std::vector<aql::latematerialized::NodeWithAttrsColumn> const& nodesToChange) {
  TRI_ASSERT(!nodesToChange.empty());
  TRI_ASSERT(_nodesToChange.empty());
  _nodesToChange.clear();
  for (auto& node : nodesToChange) {
    auto& calcNodeData = _nodesToChange[node.node];
    calcNodeData.reserve(node.attrs.size());
    std::transform(node.attrs.cbegin(), node.attrs.cend(),
                   std::inserter(calcNodeData, calcNodeData.end()),
                   [](auto const& attrAndField) { return attrAndField.afData; });
  }
}

IResearchViewNode::ViewVarsInfo IResearchViewNode::OptimizationState::replaceViewVariables(
    std::vector<aql::CalculationNode*> const& calcNodes,
    arangodb::containers::HashSet<ExecutionNode*>& toUnlink) {
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
                           ViewVariableWithColumn{{afData.fieldNumber, calcNode->outVariable()},
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
        uniqueVariables.emplace(afData.field,
                                ViewVariableWithColumn{{afData.fieldNumber,
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

IResearchViewNode::ViewVarsInfo IResearchViewNode::OptimizationState::replaceAllViewVariables(
    arangodb::containers::HashSet<ExecutionNode*>& toUnlink) {
  ViewVarsInfo uniqueVariables;
  if (_nodesToChange.empty()) {
    return uniqueVariables;
  }
  // at first use variables from simple expressions
  for (auto calcNode : _nodesToChange) {
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
              .try_emplace(afData.field,
                           ViewVariableWithColumn{{afData.fieldNumber,
                                                   calcNode.first->outVariable()},
                                                  afData.columnNumber})
              .second) {
        toUnlink.emplace(calcNode.first);
      }
    }
  }
  auto* ast = _nodesToChange.begin()->first->expression()->ast();
  TRI_ASSERT(ast);
  // create variables for complex expressions
  for (auto calcNode : _nodesToChange) {
    // a node is already unlinked
    if (calcNode.first->getParents().empty()) {
      continue;
    }
    for (auto const& afData : calcNode.second) {
      // create a variable if necessary
      if ((afData.parentNode != nullptr || !afData.postfix.empty()) &&
          uniqueVariables.find(afData.field) == uniqueVariables.cend()) {
        uniqueVariables.emplace(afData.field,
                                ViewVariableWithColumn{{afData.fieldNumber,
                                                        ast->variables()->createTemporaryVariable()},
                                                       afData.columnNumber});
      }
    }
  }
  for (auto calcNode : _nodesToChange) {
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

}  // namespace iresearch
}  // namespace arangodb
