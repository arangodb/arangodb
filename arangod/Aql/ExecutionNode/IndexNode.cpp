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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "IndexNode.h"

#include "Aql/Ast.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/IndexExecutor.h"
#include "Aql/Expression.h"
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

IndexNode::IndexNode(
    ExecutionPlan* plan, ExecutionNodeId id, Collection const* collection,
    Variable const* outVariable,
    std::vector<transaction::Methods::IndexHandle> const& indexes,
    bool allCoveredByOneIndex, std::unique_ptr<Condition> condition,
    IndexIteratorOptions const& opts)
    : ExecutionNode(plan, id),
      DocumentProducingNode(outVariable),
      CollectionAccessingNode(collection),
      _indexes(indexes),
      _condition(std::move(condition)),
      _needsGatherNodeSort(false),
      _allCoveredByOneIndex(allCoveredByOneIndex),
      _options(opts),
      _outNonMaterializedDocId(nullptr) {
  TRI_ASSERT(_condition != nullptr);

  prepareProjections();
}

/// @brief constructor for IndexNode
IndexNode::IndexNode(ExecutionPlan* plan,
                     arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      DocumentProducingNode(plan, base),
      CollectionAccessingNode(plan, base),
      _indexes(),
      _needsGatherNodeSort(basics::VelocyPackHelper::getBooleanValue(
          base, "needsGatherNodeSort", false)),
      _options(),
      _outNonMaterializedDocId(aql::Variable::varFromVPack(
          plan->getAst(), base, "outNmDocId", true)) {
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

  if (auto indexCoversProjections = base.get("indexCoversProjections");
      indexCoversProjections.isBool()) {
    _indexCoversProjections = indexCoversProjections.getBool();
  }

  VPackSlice indexes = base.get("indexes");

  if (!indexes.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "\"indexes\" attribute should be an array");
  }

  _indexes.reserve(indexes.length());

  aql::Collections const& collections = plan->getAst()->query().collections();
  auto* coll = collections.get(collection()->name());
  if (!coll) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  for (VPackSlice it : VPackArrayIterator(indexes)) {
    std::string iid = it.get("id").copyString();
    _indexes.emplace_back(coll->indexByIdentifier(iid));
  }

  VPackSlice condition = base.get("condition");
  if (!condition.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER, "\"condition\" attribute should be an object");
  }

  _condition = Condition::fromVPack(plan, condition);
  _allCoveredByOneIndex = basics::VelocyPackHelper::getBooleanValue(
      base, "allCoveredByOneIndex", false);

  TRI_ASSERT(_condition != nullptr);

  if (_outNonMaterializedDocId != nullptr && base.hasKey("indexIdOfVars")) {
    auto const* vars = plan->getAst()->variables();
    TRI_ASSERT(vars);

    auto const indexIdSlice = base.get("indexIdOfVars");
    if (!indexIdSlice.isNumber<IndexId::BaseType>()) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                    "\"indexIdOfVars\" %s should be a number",
                                    indexIdSlice.toString().c_str());
    }

    IndexValuesVars oldIndexValuesVars;
    oldIndexValuesVars.first =
        IndexId(indexIdSlice.getNumber<IndexId::BaseType>());

    auto const indexValuesVarsSlice = base.get("indexValuesVars");
    if (!indexValuesVarsSlice.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "\"indexValuesVars\" attribute should be an array");
    }

    oldIndexValuesVars.second.reserve(indexValuesVarsSlice.length());
    for (auto const indVar : velocypack::ArrayIterator(indexValuesVarsSlice)) {
      auto const fieldNumberSlice = indVar.get("fieldNumber");
      if (!fieldNumberSlice.isNumber<size_t>()) {
        THROW_ARANGO_EXCEPTION_FORMAT(
            TRI_ERROR_BAD_PARAMETER,
            "\"indexValuesVars[*].fieldNumber\" %s should be a number",
            fieldNumberSlice.toString().c_str());
      }
      auto const fieldNumber = fieldNumberSlice.getNumber<size_t>();

      auto const varIdSlice = indVar.get("id");
      if (!varIdSlice.isNumber<aql::VariableId>()) {
        THROW_ARANGO_EXCEPTION_FORMAT(
            TRI_ERROR_BAD_PARAMETER,
            "\"indexValuesVars[*].id\" variable id %s should be a number",
            varIdSlice.toString().c_str());
      }

      auto const varId = varIdSlice.getNumber<aql::VariableId>();
      auto const* var = vars->getVariable(varId);

      if (!var) {
        THROW_ARANGO_EXCEPTION_FORMAT(
            TRI_ERROR_BAD_PARAMETER,
            "\"indexValuesVars[*].id\" unable to find variable by id %d",
            varId);
      }
      oldIndexValuesVars.second.try_emplace(var, fieldNumber);
    }

    // convert old index value vars to projections
    utils::translateLMIndexVarsToProjections(plan, oldIndexValuesVars,
                                             getSingleIndex());
  }
  _options.forLateMaterialization = isLateMaterialized();

  updateProjectionsIndexInfo();
}

void IndexNode::setProjections(arangodb::aql::Projections projections) {
  DocumentProducingNode::setProjections(std::move(projections));
  prepareProjections();
}

/// @brief remember the condition to execute for early filtering
void IndexNode::setFilter(std::unique_ptr<Expression> filter) {
  DocumentProducingNode::setFilter(std::move(filter));
  prepareProjections();
}

/// @brief doToVelocyPack, for IndexNode
void IndexNode::doToVelocyPack(VPackBuilder& builder, unsigned flags) const {
  // add outvariable and projections
  DocumentProducingNode::toVelocyPack(builder, flags);

  // add collection information
  CollectionAccessingNode::toVelocyPack(builder, flags);

  // "strategy" is not read back by the C++ code, but it is exposed for
  // convenience and testing
  builder.add("strategy", VPackValue(strategyName(strategy())));
  builder.add("needsGatherNodeSort", VPackValue(_needsGatherNodeSort));

  // this attribute is never read back by arangod, but it is used a lot
  // in tests, so it can't be removed easily
  builder.add(
      "indexCoversProjections",
      VPackValue(_projections.usesCoveringIndex() &&
                 (!hasFilter() || _filterProjections.usesCoveringIndex())));
  builder.add("indexCoversOutProjections",
              VPackValue(_projections.usesCoveringIndex()));
  builder.add(
      "indexCoversFilterProjections",
      VPackValue(hasFilter() && _filterProjections.usesCoveringIndex()));

  builder.add(VPackValue("indexes"));
  {
    VPackArrayBuilder guard(&builder);
    for (auto const& index : _indexes) {
      index->toVelocyPack(builder,
                          Index::makeFlags(Index::Serialize::Estimates));
    }
  }
  builder.add(VPackValue("condition"));
  _condition->toVelocyPack(builder, flags);
  builder.add("allCoveredByOneIndex", VPackValue(_allCoveredByOneIndex));
  // IndexIteratorOptions
  builder.add("sorted", VPackValue(_options.sorted));
  builder.add("ascending", VPackValue(_options.ascending));
  builder.add("evalFCalls", VPackValue(_options.evaluateFCalls));
  builder.add(StaticStrings::UseCache, VPackValue(_options.useCache));
  builder.add(StaticStrings::WaitForSyncString,
              VPackValue(_options.waitForSync));
  builder.add("limit", VPackValue(_options.limit));
  builder.add("isLateMaterialized", VPackValue(isLateMaterialized()));
  builder.add(StaticStrings::IndexLookahead, VPackValue(_options.lookahead));

  if (isLateMaterialized()) {
    builder.add(VPackValue("outNmDocId"));
    _outNonMaterializedDocId->toVelocyPack(builder);
  }
}

/// @brief adds a UNIQUE() to a dynamic IN condition
arangodb::aql::AstNode* IndexNode::makeUnique(
    arangodb::aql::AstNode* node) const {
  if (node->type != arangodb::aql::NODE_TYPE_ARRAY || node->numMembers() >= 2) {
    // an non-array or an array with more than 1 member
    auto ast = _plan->getAst();
    auto array = _plan->getAst()->createNodeArray();
    array->addMember(node);

    // Here it does not matter which index we choose for the isSorted/isSparse
    // check, we need them all sorted here.

    auto idx = _indexes.at(0);
    if (!idx) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "The index id cannot be empty.");
    }

    if (idx->sparse() || idx->isSorted()) {
      // the index is sorted. we need to use SORTED_UNIQUE to get the
      // result back in index order
      return ast->createNodeFunctionCall("SORTED_UNIQUE", array, true);
    }
    // a regular UNIQUE will do
    return ast->createNodeFunctionCall("UNIQUE", array, true);
  }

  // presumably an array with no or a single member
  return node;
}

NonConstExpressionContainer IndexNode::buildNonConstExpressions() const {
  if (_condition->root() != nullptr) {
    auto idx = _indexes.at(0);
    if (!idx) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "The index id cannot be empty.");
    }

    return utils::extractNonConstPartsOfIndexCondition(
        _plan->getAst(), getRegisterPlan()->varInfo, options().evaluateFCalls,
        idx.get(), _condition->root(), _outVariable);
  }
  return {};
}

/// @brief determine the IndexNode strategy
IndexNode::Strategy IndexNode::strategy() const {
  if (doCount()) {
    TRI_ASSERT(_indexes.size() == 1);
    return Strategy::kCount;
  }

  if (isLateMaterialized()) {
    return Strategy::kLateMaterialized;
  }

  bool produceResult = isProduceResult();

  if (!produceResult && !hasFilter()) {
    return Strategy::kNoResult;
  }

  if (_indexes.size() == 1) {
    if (projections().usesCoveringIndex(_indexes[0]) &&
        (filterProjections().usesCoveringIndex(_indexes[0]) ||
         filterProjections().empty())) {
      return Strategy::kCovering;
    }
    if (filterProjections().usesCoveringIndex(_indexes[0])) {
      if (produceResult) {
        return Strategy::kCoveringFilterOnly;
      }
      return Strategy::kCoveringFilterScanOnly;
    }
  }

  TRI_ASSERT(_indexes.size() >= 1);
  return Strategy::kDocument;
}

std::string_view IndexNode::strategyName(
    IndexNode::Strategy strategy) noexcept {
  switch (strategy) {
    case Strategy::kNoResult:
      return "no result";
    case Strategy::kCovering:
      return "covering";
    case Strategy::kCoveringFilterScanOnly:
      return "covering, filter only, scan only";
    case Strategy::kCoveringFilterOnly:
      return "covering, filter only";
    case Strategy::kDocument:
      return "document";
    case Strategy::kLateMaterialized:
      return "late materialized";
    case Strategy::kCount:
      return "count";
  }
  TRI_ASSERT(false);
  return "unknown";
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> IndexNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  /// @brief _nonConstExpressions, list of all non const expressions, mapped
  /// by their _condition node path indexes
  auto nonConstExpressions = buildNonConstExpressions();

  // check which variables are used by the node's post-filter
  std::vector<std::pair<VariableId, RegisterId>> filterVarsToRegs;

  IndexNode::IndexFilterCoveringVars filterCoveringVars;
  if (hasFilter()) {
    VarSet inVars;
    filter()->variables(inVars);

    filterVarsToRegs.reserve(inVars.size());

    for (auto const& var : inVars) {
      TRI_ASSERT(var != nullptr);
      if (var->id == outVariable()->id &&
          filterProjections().usesCoveringIndex()) {
        // if the index covers the filter projections, then don't add the
        // document variable to the filter vars. It is not used and will cause
        // an error during register planning.
        continue;
      }
      auto regId = variableToRegisterId(var);
      filterVarsToRegs.emplace_back(var->id, regId);
    }

    if (filterProjections().usesCoveringIndex()) {
      // if the filter projections are covered by the index, optimize
      // the expression by introducing new variables for the projected
      // values
      auto const& filterProj = filterProjections();
      for (auto const& p : filterProj.projections()) {
        auto const& path = p.path.get();
        auto var =
            engine.getQuery().ast()->variables()->createTemporaryVariable();
        std::vector<std::string_view> pathView;
        std::transform(
            path.begin(), path.begin() + p.coveringIndexCutoff,
            std::back_inserter(pathView),
            [](std::string const& str) -> std::string_view { return str; });
        filter()->replaceAttributeAccess(outVariable(), pathView, var);
        filterCoveringVars.emplace(var, p.coveringIndexPosition);
      }
    }
  }

  auto const outVariable =
      isLateMaterialized() ? _outNonMaterializedDocId : _outVariable;
  auto const outRegister = variableToRegisterId(outVariable);

  // if late materialized
  // We have one additional output register for each index variable which is
  // used later, before the output register for document id. These must of
  // course fit in the available registers. There may be unused registers
  // reserved for later blocks.
  RegIdSet writableOutputRegisters;
  auto const& p = projections();
  if (!p.empty()) {
    // projections. no need to produce the full document.
    // create one register per projection.
    for (size_t i = 0; i < p.size(); ++i) {
      Variable const* var = p[i].variable;
      if (var == nullptr) {
        // the output register can be a nullptr if the "optimize-projections"
        // rule was not (yet) executed.
        continue;
      }
      TRI_ASSERT(var != nullptr);
      auto regId = variableToRegisterId(var);
      filterVarsToRegs.emplace_back(var->id, regId);
      writableOutputRegisters.emplace(regId);
      if (hasFilter()) {
        filterCoveringVars.emplace(var, p[i].coveringIndexPosition);
      }
    }

    // in case we do not have any output registers for the projections,
    // we must write them to the main output register, in a velocypack
    // object.
    // this will be handled below by adding the main output register.
  }
  if (isLateMaterialized() ||
      (writableOutputRegisters.empty() && (doCount() || isProduceResult()))) {
    // counting also needs an output register.
    writableOutputRegisters.emplace(outRegister);
  }
  TRI_ASSERT(!writableOutputRegisters.empty() ||
             (!doCount() && !isProduceResult()));

  auto registerInfos =
      createRegisterInfos({}, std::move(writableOutputRegisters));

  auto executorInfos = IndexExecutorInfos(
      strategy(), outRegister, engine.getQuery(), collection(), _outVariable,
      filter(), projections(), filterProjections(), std::move(filterVarsToRegs),
      std::move(nonConstExpressions), canReadOwnWrites(), _condition->root(),
      _allCoveredByOneIndex, getIndexes(), _plan->getAst(), this->options(),
      std::move(filterCoveringVars));
  return std::make_unique<ExecutionBlockImpl<IndexExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* IndexNode::clone(ExecutionPlan* plan,
                                bool withDependencies) const {
  auto c = std::make_unique<IndexNode>(plan, _id, collection(), _outVariable,
                                       _indexes, _allCoveredByOneIndex,
                                       _condition->clone(), _options);

  c->needsGatherNodeSort(_needsGatherNodeSort);
  c->_outNonMaterializedDocId = _outNonMaterializedDocId;
  CollectionAccessingNode::cloneInto(*c);
  DocumentProducingNode::cloneInto(plan, *c);
  return cloneHelper(std::move(c), withDependencies);
}

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void IndexNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  DocumentProducingNode::replaceVariables(replacements);
  if (_condition) {
    _condition->replaceVariables(replacements);
  }
}

void IndexNode::replaceAttributeAccess(ExecutionNode const* self,
                                       Variable const* searchVariable,
                                       std::span<std::string_view> attribute,
                                       Variable const* replaceVariable,
                                       size_t /*index*/) {
  DocumentProducingNode::replaceAttributeAccess(self, searchVariable, attribute,
                                                replaceVariable);
  if (_condition && self != this) {
    _condition->replaceAttributeAccess(searchVariable, attribute,
                                       replaceVariable);
  }
  if (hasFilter() && self != this) {
    _filter->replaceAttributeAccess(searchVariable, attribute, replaceVariable);
  }
}

/// @brief destroy the IndexNode
IndexNode::~IndexNode() = default;

/// @brief the cost of an index node is a multiple of the cost of
/// its unique dependency
CostEstimate IndexNode::estimateCost() const {
  CostEstimate estimate = _dependencies.at(0)->getCost();
  size_t incoming = estimate.estimatedNrItems;

  transaction::Methods& trx = _plan->getAst()->query().trxForOptimization();
  // estimate for the number of documents in the collection. may be
  // outdated...
  size_t const itemsInCollection =
      collection()->count(&trx, transaction::CountType::kTryCache);
  size_t totalItems = 0;
  double totalCost = 0.0;

  auto root = _condition->root();
  TRI_ASSERT(!_allCoveredByOneIndex || _indexes.size() == 1);
  for (size_t i = 0; i < _indexes.size(); ++i) {
    Index::FilterCosts costs =
        Index::FilterCosts::defaultCosts(itemsInCollection);

    if (root != nullptr && root->numMembers() > i) {
      auto const* condition = _allCoveredByOneIndex ? root : root->getMember(i);
      costs = _indexes[i]->supportsFilterCondition(
          trx, {}, condition, _outVariable, itemsInCollection);
    }

    totalItems += costs.estimatedItems;
    totalCost += costs.estimatedCosts;
  }

  if (doCount()) {
    // if "count" mode is set, always hard-code the number of results to 1
    totalItems = 1;
  }

  // weight to apply for each expected result row
  double factor = 1.0;
  if (hasFilter()) {
    // if we have a filter, we assume a weight of 1.25 for applying the
    // filter condition
    if (getIndexes().size() != 1) {
      factor = 1.25;
    } else {
      auto type = getIndexes()[0]->type();
      if (type != Index::IndexType::TRI_IDX_TYPE_GEO_INDEX &&
          type != Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX &&
          type != Index::IndexType::TRI_IDX_TYPE_GEO2_INDEX) {
        // if we only use a single index, and it is a geo index, we don't
        // apply the weight to prioritize geo indexes
        factor = 1.25;
      }
    }
  }

  estimate.estimatedNrItems *= totalItems;
  estimate.estimatedCost += incoming * totalCost * factor;
  return estimate;
}

AsyncPrefetchEligibility IndexNode::canUseAsyncPrefetching() const noexcept {
  if (_condition != nullptr && _condition->root() != nullptr &&
      (!_condition->root()->isDeterministic() ||
       _condition->root()->willUseV8())) {
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
  return DocumentProducingNode::canUseAsyncPrefetching();
}

/// @brief getVariablesUsedHere, modifying the set in-place
void IndexNode::getVariablesUsedHere(VarSet& vars) const {
  TRI_ASSERT(_condition != nullptr);
  // lookup condition
  Ast::getReferencedVariables(_condition->root(), vars);
  if (hasFilter()) {
    // post-filter
    Ast::getReferencedVariables(filter()->node(), vars);
  }
  // projection output variables.
  for (size_t i = 0; i < _projections.size(); ++i) {
    if (_projections[i].variable != nullptr) {
      vars.erase(_projections[i].variable);
    }
  }
  vars.erase(_outVariable);
}

ExecutionNode::NodeType IndexNode::getType() const { return INDEX; }

size_t IndexNode::getMemoryUsedBytes() const { return sizeof(*this); }

Condition* IndexNode::condition() const { return _condition.get(); }

IndexIteratorOptions IndexNode::options() const { return _options; }

void IndexNode::setAscending(bool value) { _options.ascending = value; }

bool IndexNode::needsGatherNodeSort() const { return _needsGatherNodeSort; }

void IndexNode::needsGatherNodeSort(bool value) {
  _needsGatherNodeSort = value;
}

std::vector<Variable const*> IndexNode::getVariablesSetHere() const {
  if (!isLateMaterialized()) {
    return DocumentProducingNode::getVariablesSetHere();
  }

  // add variables for late materialization
  std::vector<arangodb::aql::Variable const*> vars;

  vars.emplace_back(_outNonMaterializedDocId);
  // projection output variables
  for (size_t i = 0; i < _projections.size(); ++i) {
    // output registers are not necessarily set yet
    if (_projections[i].variable != nullptr) {
      vars.push_back(_projections[i].variable);
    }
  }

  return vars;
}

std::vector<transaction::Methods::IndexHandle> const& IndexNode::getIndexes()
    const {
  return _indexes;
}

void IndexNode::setLateMaterialized(aql::Variable const* docIdVariable,
                                    IndexId commonIndexId,
                                    IndexVarsInfo const& indexVariables) {
  _outNonMaterializedDocId = docIdVariable;
  IndexValuesVars oldIndexValuesVars;
  oldIndexValuesVars.first = commonIndexId;
  oldIndexValuesVars.second.clear();
  oldIndexValuesVars.second.reserve(indexVariables.size());
  for (auto& indVars : indexVariables) {
    oldIndexValuesVars.second.try_emplace(indVars.second.var,
                                          indVars.second.indexFieldNum);
  }

  _projections = utils::translateLMIndexVarsToProjections(
      plan(), oldIndexValuesVars, getSingleIndex());
  _options.forLateMaterialization = true;
}

transaction::Methods::IndexHandle IndexNode::getSingleIndex() const {
  if (_indexes.empty()) {
    return nullptr;
  }
  // cannot apply the optimization if we use more than one different index
  auto const& idx = _indexes[0];
  for (size_t i = 1; i < _indexes.size(); ++i) {
    if (_indexes[i] != idx) {
      // different indexes used => cannot use projections
      return nullptr;
    }
  }

  return idx;
}

void IndexNode::prepareProjections() { recalculateProjections(plan()); }

bool IndexNode::recalculateProjections(ExecutionPlan* plan) {
  auto idx = getSingleIndex();
  if (idx == nullptr) {
    // by default, we do not use projections for the filter condition
    _projections.clear();
    _filterProjections.clear();
    return false;
  }

  // this call will clear _projections and _filterProjections
  bool wasUpdated = DocumentProducingNode::recalculateProjections(plan);

  updateProjectionsIndexInfo();

  return wasUpdated;
}

void IndexNode::updateProjectionsIndexInfo() {
  auto idx = getSingleIndex();
  if (idx == nullptr) {
    return;
  }
  if (hasFilter() && idx->covers(_filterProjections)) {
    bool containsId = false;
    for (auto const& p : _filterProjections.projections()) {
      if (p.path.type() == AttributeNamePath::Type::IdAttribute) {
        containsId = true;
        break;
      }
    }

    if (!containsId) {
      _filterProjections.setCoveringContext(collection()->id(), idx);
    }
  }

  bool filterCoveredByIndex =
      !hasFilter() || _filterProjections.usesCoveringIndex();

  if (filterCoveredByIndex && idx->covers(_projections)) {
    _projections.setCoveringContext(collection()->id(), idx);
  }

  // If we use projections to create the document, the filter projections
  // have to be covered by the index. Otherwise, we have to load the
  // document anyways.
  TRI_ASSERT(!_projections.usesCoveringIndex() || !hasFilter() ||
             _filterProjections.usesCoveringIndex());
}

bool IndexNode::isProduceResult() const {
  if (doCount()) {
    return false;
  }
  bool filterRequiresDocument =
      hasFilter() && !_filterProjections.usesCoveringIndex();
  if (filterRequiresDocument) {
    return true;
  }
  auto const& p = projections();
  // check individual output registers of projections
  for (size_t i = 0; i < p.size(); ++i) {
    Variable const* var = p[i].variable;
    // the output register can be a nullptr if the "optimize-projections"
    // rule was not (yet) executed
    if (var != nullptr && isVarUsedLater(var)) {
      return true;
    }
  }
  if (!p.hasOutputRegisters()) {
    // projections do not use output registers. now check
    // if the full document output variable will be used later.
    return isVarUsedLater(_outVariable);
  }
  return false;
}

aql::Variable const* IndexNode::getLateMaterializedDocIdOutVar() const {
  TRI_ASSERT(isLateMaterialized());
  return _outNonMaterializedDocId;
}
