////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "JoinNode.h"

#include "Aql/Ast.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/IndexExecutor.h"
#include "Aql/JoinExecutor.h"
#include "Aql/NonConstExpressionContainer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Projections.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Containers/FlatHashSet.h"
#include "Indexes/Index.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/CountCache.h"
#include "Transaction/Methods.h"

#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::aql;

JoinNode::JoinNode(ExecutionPlan* plan, ExecutionNodeId id,
                   std::vector<JoinNode::IndexInfo> indexInfos,
                   IndexIteratorOptions const& opts)
    : ExecutionNode(plan, id), _indexInfos(std::move(indexInfos)) {
  TRI_ASSERT(_indexInfos.size() >= 2);
}

JoinNode::JoinNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base) {
  // TODO: this code is the same in IndexNode. move into a sharable function
  _options.sorted =
      basics::VelocyPackHelper::getBooleanValue(base, "sorted", true);
  _options.ascending =
      basics::VelocyPackHelper::getBooleanValue(base, "ascending", false);
  _options.evaluateFCalls =
      basics::VelocyPackHelper::getBooleanValue(base, "evalFCalls", true);
  _options.useCache = basics::VelocyPackHelper::getBooleanValue(
      base, StaticStrings::UseCache, true);
  _options.waitForSync = basics::VelocyPackHelper::getBooleanValue(
      base, StaticStrings::WaitForSyncString, false);
  _options.limit = basics::VelocyPackHelper::getNumericValue(base, "limit", 0);
  _options.lookahead = basics::VelocyPackHelper::getNumericValue(
      base, StaticStrings::IndexLookahead, IndexIteratorOptions{}.lookahead);

  if (_options.sorted && base.isObject() && base.get("reverse").isBool()) {
    // legacy
    _options.sorted = true;
    _options.ascending = !base.get("reverse").getBool();
  }

  VPackSlice indexInfos = base.get("indexInfos");

  if (!indexInfos.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "\"indexes\" attribute should be an array");
  }

  _indexInfos.reserve(indexInfos.length());

  aql::Collections const& collections = plan->getAst()->query().collections();

  for (VPackSlice it : VPackArrayIterator(indexInfos)) {
    // collection
    std::string collection = it.get("collection").copyString();
    auto* coll = collections.get(collection);
    if (!coll) {
      TRI_ASSERT(false) << "collection: " << collection << " not found.";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    }

    auto outVariable =
        Variable::varFromVPack(plan->getAst(), it, "outVariable");

    // condition
    VPackSlice condition = it.get("condition");
    if (!condition.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "\"condition\" attribute should be an object");
    }

    // index
    std::string iid = it.get("index").get("id").copyString();

    auto projections =
        Projections::fromVelocyPack(plan->getAst(), it, "projections",
                                    plan->getAst()->query().resourceMonitor());
    auto filterProjections =
        Projections::fromVelocyPack(plan->getAst(), it, "filterProjections",
                                    plan->getAst()->query().resourceMonitor());

    bool const usedAsSatellite = it.get("usedAsSatellite").isTrue();

    std::unique_ptr<Expression> filter = nullptr;
    if (auto fs = it.get("filter"); !fs.isNone()) {
      filter = std::make_unique<Expression>(plan->getAst(),
                                            plan->getAst()->createNode(fs));
    }

    auto& idx = _indexInfos.emplace_back(
        IndexInfo{.collection = coll,
                  .outVariable = outVariable,
                  .condition = Condition::fromVPack(plan, condition),
                  .filter = std::move(filter),
                  .index = coll->indexByIdentifier(iid),
                  .projections = projections,
                  .filterProjections = filterProjections,
                  .usedAsSatellite = usedAsSatellite});

    VPackSlice usedShard = it.get("usedShard");
    if (usedShard.isString()) {
      idx.usedShard = usedShard.copyString();
    }

    auto const prepareProjections = [&](aql::Projections& proj, bool prohibited,
                                        bool expectation) {
      if (!proj.empty()) {
        if (!prohibited && idx.index->covers(proj)) {
          proj.setCoveringContext(coll->id(), idx.index);
        }
        TRI_ASSERT(proj.usesCoveringIndex(idx.index) == expectation)
            << "expectation = " << std::boolalpha << expectation
            << " prohibited = " << prohibited;
      }
    };

    prepareProjections(idx.filterProjections, false,
                       it.get("indexCoversFilterProjections").isTrue());
    prepareProjections(
        idx.projections,
        idx.filter != nullptr && !idx.filterProjections.usesCoveringIndex(),
        it.get("indexCoversProjections").isTrue());
  }

  TRI_ASSERT(_indexInfos.size() >= 2);
}

JoinNode::~JoinNode() = default;

void JoinNode::doToVelocyPack(VPackBuilder& builder, unsigned flags) const {
  builder.add(VPackValue("indexInfos"));
  builder.openArray();

  for (auto const& it : _indexInfos) {
    builder.openObject();
    // TODO: check if this works in cluster, where we likely need shards and not
    // collections
    if (!it.usedShard.empty()) {
      builder.add("collection", VPackValue(it.usedShard));
      builder.add("usedShard", VPackValue(it.usedShard));
    } else {
      builder.add("collection", VPackValue(it.collection->name()));
    }

    // out variable
    builder.add(VPackValue("outVariable"));
    it.outVariable->toVelocyPack(builder);
    // condition
    builder.add(VPackValue("condition"));
    it.condition->toVelocyPack(builder, flags);
    // projections
    it.projections.toVelocyPack(builder);
    builder.add("indexCoversProjections",
                VPackValue(it.projections.usesCoveringIndex(it.index)));
    builder.add("usedAsSatellite", VPackValue(it.usedAsSatellite));
    // filter
    if (it.filter) {
      builder.add(VPackValue("filter"));
      it.filter->toVelocyPack(builder, true);
      it.filterProjections.toVelocyPack(builder, "filterProjections");
      builder.add("indexCoversFilterProjections",
                  VPackValue(it.filterProjections.usesCoveringIndex(it.index)));
    }
    // index
    builder.add(VPackValue("index"));
    it.index->toVelocyPack(builder,
                           Index::makeFlags(Index::Serialize::Estimates));

    // estimatedCost & estimatedNrItems
    if (flags & ExecutionNode::SERIALIZE_ESTIMATES) {
      Index::FilterCosts costs = costsForIndexInfo(it);
      builder.add("estimatedCost", VPackValue(costs.estimatedCosts));
      builder.add("estimatedNrItems", VPackValue(costs.estimatedItems));
    }

    builder.close();
  }

  builder.close();  // indexInfos

  // TODO: this is shared with IndexNode: move into a shared function
  builder.add("sorted", VPackValue(_options.sorted));
  builder.add("ascending", VPackValue(_options.ascending));
  builder.add("evalFCalls", VPackValue(_options.evaluateFCalls));
  builder.add(StaticStrings::UseCache, VPackValue(_options.useCache));
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> JoinNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  RegIdSet writableOutputRegisters;

  JoinExecutorInfos infos;
  infos.query = &engine.getQuery();
  infos.indexes.reserve(_indexInfos.size());
  for (auto const& idx : _indexInfos) {
    auto documentOutputRegister = variableToRegisterId(idx.outVariable);
    writableOutputRegisters.emplace(documentOutputRegister);

    // TODO probably those data structures can become one
    auto& data = infos.indexes.emplace_back();
    data.documentOutputRegister = documentOutputRegister;
    data.index = idx.index;
    data.collection = idx.collection;
    data.projections = idx.projections;

    if (idx.filter) {
      auto& filter = data.filter.emplace();
      filter.documentVariable = idx.outVariable;
      filter.expression = idx.filter->clone(engine.getQuery().ast());
      filter.projections = idx.filterProjections;

      VarSet inVars;
      filter.expression->variables(inVars);

      filter.filterVarsToRegs.reserve(inVars.size());

      for (auto& var : inVars) {
        TRI_ASSERT(var != nullptr);
        auto regId = variableToRegisterId(var);
        filter.filterVarsToRegs.emplace_back(var->id, regId);
      }
      if (filter.projections.usesCoveringIndex()) {
        for (auto const& p : filter.projections.projections()) {
          auto const& path = p.path.get();
          auto var = infos.query->ast()->variables()->createTemporaryVariable();
          std::vector<std::string_view> pathView;
          std::transform(
              path.begin(), path.begin() + p.coveringIndexCutoff,
              std::back_inserter(pathView),
              [](std::string const& str) -> std::string_view { return str; });
          filter.expression->replaceAttributeAccess(filter.documentVariable,
                                                    pathView, var);
          filter.filterProjectionVars.push_back(var);
        }
      }
    }
  }

  auto registerInfos = createRegisterInfos({}, writableOutputRegisters);

  return std::make_unique<ExecutionBlockImpl<JoinExecutor>>(
      &engine, this, registerInfos, std::move(infos));
}

ExecutionNode* JoinNode::clone(ExecutionPlan* plan, bool withDependencies,
                               bool withProperties) const {
  std::vector<IndexInfo> indexInfos;
  indexInfos.reserve(_indexInfos.size());

  for (auto const& it : _indexInfos) {
    auto outVariable = it.outVariable;
    if (withProperties) {
      outVariable = plan->getAst()->variables()->createVariable(outVariable);
    }
    indexInfos.emplace_back(IndexInfo{.collection = it.collection,
                                      .usedShard = it.usedShard,
                                      .outVariable = outVariable,
                                      .condition = it.condition->clone(),
                                      .index = it.index,
                                      .projections = it.projections,
                                      .usedAsSatellite = it.usedAsSatellite});
  }

  auto c =
      std::make_unique<JoinNode>(plan, _id, std::move(indexInfos), _options);

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void JoinNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  // TODO: replace variables inside index conditions.
  // IndexNode doesn't do this either, which seems questionable
}

/// @brief the cost of an join node is a multiple of the cost of
/// its unique dependency
CostEstimate JoinNode::estimateCost() const {
  CostEstimate estimate = _dependencies.at(0)->getCost();
  size_t incoming = estimate.estimatedNrItems;

  size_t totalItems = 1;
  double totalCost = 1.0;

  for (auto const& it : _indexInfos) {
    Index::FilterCosts costs = costsForIndexInfo(it);
    // TODO: perhaps we should multiply here
    totalItems *= costs.estimatedItems;
    totalCost *= costs.estimatedCosts;
  }

  estimate.estimatedNrItems *= totalItems;
  estimate.estimatedCost += incoming * totalCost;
  return estimate;
}

/// @brief getVariablesUsedHere, modifying the set in-place
void JoinNode::getVariablesUsedHere(VarSet& vars) const {
  for (auto const& it : _indexInfos) {
    if (it.condition->root() != nullptr) {
      Ast::getReferencedVariables(it.condition->root(), vars);
    }
  }
  for (auto const& it : _indexInfos) {
    vars.erase(it.outVariable);
  }
}

ExecutionNode::NodeType JoinNode::getType() const { return JOIN; }

size_t JoinNode::getMemoryUsedBytes() const { return sizeof(*this); }

IndexIteratorOptions JoinNode::options() const { return _options; }

std::vector<Variable const*> JoinNode::getVariablesSetHere() const {
  std::vector<Variable const*> vars;
  vars.reserve(_indexInfos.size());

  for (auto const& it : _indexInfos) {
    vars.emplace_back(it.outVariable);
  }
  return vars;
}

std::vector<JoinNode::IndexInfo> const& JoinNode::getIndexInfos() const {
  return _indexInfos;
}

std::vector<JoinNode::IndexInfo>& JoinNode::getIndexInfos() {
  return _indexInfos;
}

Index::FilterCosts JoinNode::costsForIndexInfo(
    JoinNode::IndexInfo const& info) const {
  transaction::Methods& trx = _plan->getAst()->query().trxForOptimization();
  size_t itemsInCollection =
      info.collection->count(&trx, transaction::CountType::TryCache);

  Index::FilterCosts costs =
      Index::FilterCosts::defaultCosts(itemsInCollection);

  auto root = info.condition->root();
  if (root != nullptr) {
    if (root->type == NODE_TYPE_OPERATOR_NARY_OR) {
      TRI_ASSERT(root->numMembers() == 1);
      root = root->getMember(0);
      TRI_ASSERT(root->type == NODE_TYPE_OPERATOR_NARY_AND);
    }
    costs = info.index->supportsFilterCondition(trx, {}, root, info.outVariable,
                                                itemsInCollection);
  }
  return costs;
}

std::ostream& arangodb::operator<<(std::ostream& os,
                                   IndexStreamOptions const& opts) {
  os << "{";
  os << "keyFields = [ " << opts.usedKeyFields << "], ";
  os << "projectedFields = [ " << opts.projectedFields << "]";
  os << "}";
  return os;
}
