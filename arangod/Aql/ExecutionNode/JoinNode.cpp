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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "JoinNode.h"

#include "Aql/Ast.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/IndexExecutor.h"
#include "Aql/Executor/JoinExecutor.h"
#include "Aql/Expression.h"
#include "Aql/NonConstExpressionContainer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Projections.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"
#include "Indexes/Index.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/CountCache.h"
#include "Transaction/Methods.h"

#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::aql;

constexpr const char expressionKey[] = "expression";
constexpr const char expressionsKey[] = "expressions";
constexpr const char usedKeyFieldsKey[] = "usedKeyFields";
constexpr const char constantFieldsKey[] = "constantFields";

JoinNode::JoinNode(ExecutionPlan* plan, ExecutionNodeId id,
                   std::vector<JoinNode::IndexInfo> indexInfos,
                   IndexIteratorOptions const& opts)
    : ExecutionNode(plan, id), _indexInfos(std::move(indexInfos)) {
  TRI_ASSERT(_indexInfos.size() >= 2);
}

JoinNode::JoinNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base) {
  // TODO: this code is almost the same in IndexNode. move into a sharable
  // function
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

    std::vector<std::unique_ptr<Expression>> expressions{};
    if (it.get(expressionsKey).isArray()) {
      auto expressionsSlice = it.get(expressionsKey);
      for (auto const& expr : VPackArrayIterator(expressionsSlice)) {
        expressions.push_back(
            std::make_unique<Expression>(plan->getAst(), expr));
      }
    }

    std::vector<size_t> usedKeyFields{};
    std::vector<size_t> constantFields{};

    VPackSlice usedKeyFieldsSlice = it.get(usedKeyFieldsKey);
    if (!usedKeyFieldsSlice.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "\"usedKeyFields\" attribute should be an array");
    } else {
      for (VPackSlice key : VPackArrayIterator(usedKeyFieldsSlice)) {
        TRI_ASSERT(key.isNumber());
        usedKeyFields.emplace_back(key.getNumber<size_t>());
      }
    }

    VPackSlice constantFieldsSlice = it.get(constantFieldsKey);
    if (!constantFieldsSlice.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "\"constantFields\" attribute should be an array");
    } else {
      for (VPackSlice constant : VPackArrayIterator(constantFieldsSlice)) {
        TRI_ASSERT(constant.isNumber());
        constantFields.emplace_back(constant.getNumber<size_t>());
      }
    }

    bool const usedAsSatellite = it.get("usedAsSatellite").isTrue();
    bool const producesOutput = it.get("producesOutput").isTrue();

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
                  .projections = std::move(projections),
                  .filterProjections = filterProjections,
                  .usedAsSatellite = usedAsSatellite,
                  .producesOutput = producesOutput,
                  .expressions = std::move(expressions),
                  .usedKeyFields = std::move(usedKeyFields),
                  .constantFields = std::move(constantFields)});

    idx.isLateMaterialized = it.get("isLateMaterialized").isTrue();
    if (idx.isLateMaterialized) {
      idx.outDocIdVariable =
          Variable::varFromVPack(plan->getAst(), it, "outDocIdVariable");
    }

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
    if (!it.usedShard.empty()) {
      builder.add("collection", VPackValue(it.usedShard));
      builder.add("usedShard", VPackValue(it.usedShard));
    } else {
      builder.add("collection", VPackValue(it.collection->name()));
    }

    builder.add("producesOutput", VPackValue(it.producesOutput));
    // out variable
    builder.add(VPackValue("outVariable"));
    it.outVariable->toVelocyPack(builder);

    if (it.isLateMaterialized) {
      builder.add("isLateMaterialized", VPackValue(true));
      builder.add(VPackValue("outDocIdVariable"));
      it.outDocIdVariable->toVelocyPack(builder);
    }
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
    if (!it.expressions.empty()) {
      builder.add(VPackValue(expressionsKey));
      {
        VPackArrayBuilder expressionsArray(&builder);
        for (auto const& expr : it.expressions) {
          VPackObjectBuilder expressionObject(&builder);
          builder.add(VPackValue(expressionKey));
          expr->toVelocyPack(builder, true);
        }
      }
    }

    builder.add(VPackValue(usedKeyFieldsKey));
    {
      VPackArrayBuilder usedKeyFieldsArray(&builder);
      for (auto const& key : it.usedKeyFields) {
        builder.add(VPackValue(key));
      }
    }

    builder.add(VPackValue(constantFieldsKey));
    {
      VPackArrayBuilder constantFieldsArray(&builder);
      for (auto const& constant : it.constantFields) {
        builder.add(VPackValue(constant));
      }
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
  containers::FlatHashMap<VariableId, RegisterId> varsToRegs;

  JoinExecutorInfos infos;
  infos.query = &engine.getQuery();
  infos.indexes.reserve(_indexInfos.size());
  for (auto const& idx : _indexInfos) {
    auto const& p = idx.projections;
    // create one register per projection.
    for (size_t i = 0; i < p.size(); ++i) {
      Variable const* var = p[i].variable;
      if (var == nullptr) {
        // the output register can be a nullptr if the "optimize-projections"
        // rule was not executed (potentially because it was disabled).
        continue;
      }
      TRI_ASSERT(var != nullptr);
      auto regId = variableToRegisterId(var);
      varsToRegs.emplace(var->id, regId);
      if (idx.producesOutput) {
        writableOutputRegisters.emplace(regId);
      }
    }

    RegisterId documentOutputRegister = RegisterId::maxRegisterId;
    if (!p.hasOutputRegisters()) {
      if (idx.producesOutput && !idx.isLateMaterialized) {
        documentOutputRegister = variableToRegisterId(idx.outVariable);
        writableOutputRegisters.emplace(documentOutputRegister);
      }
    }

    auto& data = infos.indexes.emplace_back();
    data.documentOutputRegister = documentOutputRegister;
    data.index = idx.index;
    data.collection = idx.collection;
    data.projections = idx.projections;
    data.producesOutput = idx.producesOutput;

    data.isLateMaterialized = idx.isLateMaterialized;
    if (data.isLateMaterialized) {
      data.docIdOutputRegister = variableToRegisterId(idx.outDocIdVariable);
      writableOutputRegisters.emplace(data.docIdOutputRegister);
    }

    if (!idx.expressions.empty()) {
      VarSet varsUsed;

      for (auto& expr : idx.expressions) {
        TRI_ASSERT(expr != nullptr);
        TRI_ASSERT(expr->isDeterministic());
        expr->variables(varsUsed);
        data.constantExpressions.push_back(
            expr->clone(engine.getQuery().ast(), true));
        data.usedKeyFields = idx.usedKeyFields;
        data.constantFields = idx.constantFields;
      }

      for (auto const& var : varsUsed) {
        auto regId = variableToRegisterId(var);
        data.expressionVarsToRegs.emplace_back(var->id, regId);
      }
    } else {
      data.usedKeyFields = {0};
      data.constantFields = {};
    }

    if (idx.filter) {
      auto& filter = data.filter.emplace();
      filter.documentVariable = idx.outVariable;
      filter.expression = idx.filter->clone(engine.getQuery().ast());
      filter.projections = idx.filterProjections;

      VarSet inVars;
      filter.expression->variables(inVars);

      filter.filterVarsToRegs.reserve(inVars.size());

      for (auto const& var : inVars) {
        TRI_ASSERT(var != nullptr);
        if (var->id == idx.outVariable->id &&
            idx.filterProjections.usesCoveringIndex()) {
          // if the index covers the filter projections, then don't add the
          // document variable to the filter vars. It is not used and will cause
          // an error during register planning.
          continue;
        }
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

  infos.varsToRegister = std::move(varsToRegs);
  // TODO: Check the use of first paramter for createRegisterInfos (currently
  // not tracked)
  auto registerInfos = createRegisterInfos({}, writableOutputRegisters);
  return std::make_unique<ExecutionBlockImpl<JoinExecutor>>(
      &engine, this, registerInfos, std::move(infos));
}

ExecutionNode* JoinNode::clone(ExecutionPlan* plan,
                               bool withDependencies) const {
  std::vector<IndexInfo> indexInfos;
  indexInfos.reserve(_indexInfos.size());

  for (auto const& it : _indexInfos) {
    std::vector<std::unique_ptr<Expression>> clonedExpressions{};
    for (auto const& expr : it.expressions) {
      clonedExpressions.emplace_back(expr->clone(plan->getAst(), true));
    }

    indexInfos.emplace_back(IndexInfo{
        .collection = it.collection,
        .usedShard = it.usedShard,
        .outVariable = it.outVariable,
        .condition = it.condition->clone(),
        .filter = it.filter != nullptr ? it.filter->clone(plan->getAst(), true)
                                       : nullptr,
        .index = it.index,
        .projections = it.projections,
        .usedAsSatellite = it.usedAsSatellite,
        .producesOutput = it.producesOutput,
        .isLateMaterialized = it.isLateMaterialized,
        .outDocIdVariable = it.outDocIdVariable,
        .expressions = std::move(clonedExpressions),
        .usedKeyFields = it.usedKeyFields,
        .constantFields = it.constantFields});
  }

  auto c =
      std::make_unique<JoinNode>(plan, _id, std::move(indexInfos), _options);

  return cloneHelper(std::move(c), withDependencies);
}

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void JoinNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  for (auto& it : _indexInfos) {
    if (it.condition) {
      it.condition->replaceVariables(replacements);
    }
    if (it.filter != nullptr) {
      it.filter->replaceVariables(replacements);
    }
    for (auto const& expr : it.expressions) {
      expr->replaceVariables(replacements);
    }
  }
}

void JoinNode::replaceAttributeAccess(ExecutionNode const* self,
                                      Variable const* searchVariable,
                                      std::span<std::string_view> attribute,
                                      Variable const* replaceVariable,
                                      size_t index) {
  size_t i = 0;
  for (auto& it : _indexInfos) {
    if (it.condition && (self != this || i > index)) {
      it.condition->replaceAttributeAccess(searchVariable, attribute,
                                           replaceVariable);
    }
    if (it.filter && (self != this || i > index)) {
      it.filter->replaceAttributeAccess(searchVariable, attribute,
                                        replaceVariable);
    }
    if (!it.expressions.empty() && (self != this || i > index)) {
      for (auto const& expr : it.expressions) {
        expr->replaceAttributeAccess(searchVariable, attribute,
                                     replaceVariable);
      }
    }
    ++i;
  }
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
    totalItems *= costs.estimatedItems;
    totalCost += costs.estimatedCosts;
  }

  estimate.estimatedNrItems *= totalItems;
  estimate.estimatedCost += incoming * totalCost;
  return estimate;
}

AsyncPrefetchEligibility JoinNode::canUseAsyncPrefetching() const noexcept {
  for (auto const& it : _indexInfos) {
    if (it.filter != nullptr &&
        (!it.filter->isDeterministic() || it.filter->willUseV8())) {
      // we cannot use prefetching if the filter employs V8, because the
      // Query object only has a single V8 context, which it can enter and exit.
      // with prefetching, multiple threads can execute calculations in the same
      // Query instance concurrently, and when using V8, they could try to
      // enter/exit the V8 context of the query concurrently. this is currently
      // not thread-safe, so we don't use prefetching.
      // the constraint for determinism is there because we could produce
      // different query results when prefetching is enabled, at least in
      // streaming queries.
      return AsyncPrefetchEligibility::kDisableForNode;
    }
    if (it.condition != nullptr && it.condition->root() != nullptr &&
        (!it.condition->root()->isDeterministic() ||
         it.condition->root()->willUseV8())) {
      // we cannot use prefetching if the lookup employs V8, because the
      // Query object only has a single V8 context, which it can enter and exit.
      // with prefetching, multiple threads can execute calculations in the same
      // Query instance concurrently, and when using V8, they could try to
      // enter/exit the V8 context of the query concurrently. this is currently
      // not thread-safe, so we don't use prefetching.
      // the constraint for determinism is there because we could produce
      // different query results when prefetching is enabled, at least in
      // streaming queries.
      return AsyncPrefetchEligibility::kDisableForNode;
    }
  }
  return AsyncPrefetchEligibility::kEnableForNode;
}

/// @brief getVariablesUsedHere, modifying the set in-place
void JoinNode::getVariablesUsedHere(VarSet& vars) const {
  for (auto const& it : _indexInfos) {
    if (it.condition != nullptr && it.condition->root() != nullptr) {
      // lookup condition
      Ast::getReferencedVariables(it.condition->root(), vars);
    }
    if (it.filter != nullptr && it.filter->node() != nullptr) {
      // lookup condition
      Ast::getReferencedVariables(it.filter->node(), vars);
    }
    for (auto& expr : it.expressions) {
      expr->variables(vars);
    }
  }
  for (auto const& it : _indexInfos) {
    vars.erase(it.outVariable);
    // projection output variables.
    for (size_t i = 0; i < it.projections.size(); ++i) {
      if (it.projections[i].variable != nullptr) {
        vars.erase(it.projections[i].variable);
      }
    }
  }
}

ExecutionNode::NodeType JoinNode::getType() const { return JOIN; }

size_t JoinNode::getMemoryUsedBytes() const { return sizeof(*this); }

IndexIteratorOptions JoinNode::options() const { return _options; }

std::vector<Variable const*> JoinNode::getVariablesSetHere() const {
  std::vector<Variable const*> vars;
  vars.reserve(_indexInfos.size());

  for (auto const& it : _indexInfos) {
    // projection output variables
    for (size_t i = 0; i < it.projections.size(); ++i) {
      // output registers are not necessarily set yet
      if (it.projections[i].variable != nullptr) {
        vars.push_back(it.projections[i].variable);
      }
    }

    if (it.isLateMaterialized) {
      vars.emplace_back(it.outDocIdVariable);
    } else if (!it.projections.hasOutputRegisters() || it.filter != nullptr) {
      vars.emplace_back(it.outVariable);
    }
  }

  return vars;
}

std::vector<JoinNode::IndexInfo> const& JoinNode::getIndexInfos() const {
  return _indexInfos;
}

std::vector<JoinNode::IndexInfo>& JoinNode::getIndexInfos() {
  return _indexInfos;
}

bool JoinNode::isDeterministic() {
  for (auto const& it : _indexInfos) {
    if (it.condition != nullptr && it.condition->root() != nullptr &&
        !it.condition->root()->isDeterministic()) {
      return false;
    }
    if (it.filter != nullptr && !it.filter->isDeterministic()) {
      return false;
    }
  }
  return true;
}

Index::FilterCosts JoinNode::costsForIndexInfo(
    JoinNode::IndexInfo const& info) const {
  transaction::Methods& trx = _plan->getAst()->query().trxForOptimization();
  size_t itemsInCollection =
      info.collection->count(&trx, transaction::CountType::kTryCache);

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
