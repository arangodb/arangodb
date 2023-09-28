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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "IndexNode.h"

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

  if (_outNonMaterializedDocId != nullptr) {
    auto const* vars = plan->getAst()->variables();
    TRI_ASSERT(vars);

    auto const indexIdSlice = base.get("indexIdOfVars");
    if (!indexIdSlice.isNumber<IndexId::BaseType>()) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                    "\"indexIdOfVars\" %s should be a number",
                                    indexIdSlice.toString().c_str());
    }

    _outNonMaterializedIndVars.first =
        IndexId(indexIdSlice.getNumber<IndexId::BaseType>());

    auto const indexValuesVarsSlice = base.get("indexValuesVars");
    if (!indexValuesVarsSlice.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "\"indexValuesVars\" attribute should be an array");
    }
    _outNonMaterializedIndVars.second.reserve(indexValuesVarsSlice.length());
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
      _outNonMaterializedIndVars.second.try_emplace(var, fieldNumber);
    }
  }
  _options.forLateMaterialization = isLateMaterialized();
  prepareProjections();
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

  builder.add("needsGatherNodeSort", VPackValue(_needsGatherNodeSort));

  // this attribute is never read back by arangod, but it is used a lot
  // in tests, so it can't be removed easily
  builder.add("indexCoversProjections",
              VPackValue(_projections.usesCoveringIndex()));

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
  builder.add(StaticStrings::IndexLookahead, VPackValue(_options.lookahead));

  if (isLateMaterialized()) {
    builder.add(VPackValue("outNmDocId"));
    _outNonMaterializedDocId->toVelocyPack(builder);

    builder.add("indexIdOfVars",
                VPackValue(_outNonMaterializedIndVars.first.id()));
    // container _indexes contains a few items
    auto indIt = std::find_if(
        _indexes.cbegin(), _indexes.cend(), [this](auto const& index) {
          return index->id() == _outNonMaterializedIndVars.first;
        });
    TRI_ASSERT(indIt != _indexes.cend());
    auto const& coveredFields = (*indIt)->coveredFields();
    VPackArrayBuilder arrayScope(&builder, "indexValuesVars");
    for (auto const& fieldVar : _outNonMaterializedIndVars.second) {
      VPackObjectBuilder objectScope(&builder);
      builder.add("fieldNumber", VPackValue(fieldVar.second));
      builder.add("id", VPackValue(fieldVar.first->id));
      builder.add("name",
                  VPackValue(fieldVar.first->name));  // for explainer.js
      std::string fieldName;
      TRI_ASSERT(fieldVar.second < coveredFields.size());
      basics::TRI_AttributeNamesToString(coveredFields[fieldVar.second],
                                         fieldName, true);
      builder.add("field", VPackValue(fieldName));  // for explainer.js
    }
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

  if (hasFilter()) {
    VarSet inVars;
    filter()->variables(inVars);

    filterVarsToRegs.reserve(inVars.size());

    for (auto& var : inVars) {
      TRI_ASSERT(var != nullptr);
      auto regId = variableToRegisterId(var);
      filterVarsToRegs.emplace_back(var->id, regId);
    }
  }

  auto const outVariable =
      isLateMaterialized() ? _outNonMaterializedDocId : _outVariable;
  auto const outRegister = variableToRegisterId(outVariable);
  auto numIndVarsRegisters =
      static_cast<aql::RegisterCount>(_outNonMaterializedIndVars.second.size());
  TRI_ASSERT(0 == numIndVarsRegisters || isLateMaterialized());

  // We could be asked to produce only document id for later materialization
  // or full document body at once
  aql::RegisterCount numDocumentRegs = 1;

  // if late materialized
  // We have one additional output register for each index variable which is
  // used later, before the output register for document id These must of
  // course fit in the available registers. There may be unused registers
  // reserved for later blocks.
  RegIdSet writableOutputRegisters;
  writableOutputRegisters.reserve(numDocumentRegs + numIndVarsRegisters);
  writableOutputRegisters.emplace(outRegister);

  auto const& varInfos = getRegisterPlan()->varInfo;
  IndexValuesRegisters outNonMaterializedIndRegs;
  outNonMaterializedIndRegs.first = _outNonMaterializedIndVars.first;
  outNonMaterializedIndRegs.second.reserve(
      _outNonMaterializedIndVars.second.size());
  std::transform(_outNonMaterializedIndVars.second.cbegin(),
                 _outNonMaterializedIndVars.second.cend(),
                 std::inserter(outNonMaterializedIndRegs.second,
                               outNonMaterializedIndRegs.second.end()),
                 [&](auto const& indVar) {
                   auto it = varInfos.find(indVar.first->id);
                   TRI_ASSERT(it != varInfos.cend());
                   RegisterId regId = it->second.registerId;

                   writableOutputRegisters.emplace(regId);
                   return std::make_pair(indVar.second, regId);
                 });

  TRI_ASSERT(writableOutputRegisters.size() ==
             numDocumentRegs + numIndVarsRegisters);

  auto registerInfos =
      createRegisterInfos({}, std::move(writableOutputRegisters));

  auto executorInfos = IndexExecutorInfos(
      outRegister, engine.getQuery(), this->collection(), _outVariable,
      isProduceResult(), this->_filter.get(), this->projections(),
      this->filterProjections(), std::move(filterVarsToRegs),
      std::move(nonConstExpressions), doCount(), canReadOwnWrites(),
      _condition->root(), _allCoveredByOneIndex, this->getIndexes(),
      _plan->getAst(), this->options(), _outNonMaterializedIndVars,
      std::move(outNonMaterializedIndRegs));

  return std::make_unique<ExecutionBlockImpl<IndexExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* IndexNode::clone(ExecutionPlan* plan, bool withDependencies,
                                bool withProperties) const {
  auto outVariable = _outVariable;
  auto outNonMaterializedDocId = _outNonMaterializedDocId;
  IndexValuesVars outNonMaterializedIndVars;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
    if (outNonMaterializedDocId != nullptr) {
      outNonMaterializedDocId =
          plan->getAst()->variables()->createVariable(outNonMaterializedDocId);
    }
    outNonMaterializedIndVars.first = _outNonMaterializedIndVars.first;
    outNonMaterializedIndVars.second.reserve(
        _outNonMaterializedIndVars.second.size());
    for (auto& indVar : _outNonMaterializedIndVars.second) {
      outNonMaterializedIndVars.second.try_emplace(
          plan->getAst()->variables()->createVariable(indVar.first),
          indVar.second);
    }
  } else {
    outNonMaterializedIndVars = _outNonMaterializedIndVars;
  }

  auto c = std::make_unique<IndexNode>(plan, _id, collection(), outVariable,
                                       _indexes, _allCoveredByOneIndex,
                                       _condition->clone(), _options);

  c->_projections = _projections;
  c->_filterProjections = _filterProjections;
  c->needsGatherNodeSort(_needsGatherNodeSort);
  c->_outNonMaterializedDocId = outNonMaterializedDocId;
  c->_outNonMaterializedIndVars = std::move(outNonMaterializedIndVars);
  CollectionAccessingNode::cloneInto(*c);
  DocumentProducingNode::cloneInto(plan, *c);
  return cloneHelper(std::move(c), withDependencies, withProperties);
}

/// @brief replaces variables in the internals of the execution node
/// replacements are { old variable id => new variable }
void IndexNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  DocumentProducingNode::replaceVariables(replacements);
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
      collection()->count(&trx, transaction::CountType::TryCache);
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

  estimate.estimatedNrItems *= totalItems;
  estimate.estimatedCost += incoming * totalCost;
  return estimate;
}

/// @brief getVariablesUsedHere, modifying the set in-place
void IndexNode::getVariablesUsedHere(VarSet& vars) const {
  Ast::getReferencedVariables(_condition->root(), vars);
  if (hasFilter()) {
    Ast::getReferencedVariables(filter()->node(), vars);
  }
  for (auto const& it : _outNonMaterializedIndVars.second) {
    vars.erase(it.first);
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
    return std::vector<Variable const*>{_outVariable};
  }

  std::vector<arangodb::aql::Variable const*> vars;
  vars.reserve(1 + _outNonMaterializedIndVars.second.size());
  vars.emplace_back(_outNonMaterializedDocId);
  std::transform(_outNonMaterializedIndVars.second.cbegin(),
                 _outNonMaterializedIndVars.second.cend(),
                 std::back_inserter(vars),
                 [](auto const& indVar) { return indVar.first; });

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
  _outNonMaterializedIndVars.first = commonIndexId;
  _outNonMaterializedIndVars.second.clear();
  _outNonMaterializedIndVars.second.reserve(indexVariables.size());
  for (auto& indVars : indexVariables) {
    _outNonMaterializedIndVars.second.try_emplace(indVars.second.var,
                                                  indVars.second.indexFieldNum);
  }
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

void IndexNode::prepareProjections() {
  // by default, we do not use projections for the filter condition
  _filterProjections.clear();

  auto idx = getSingleIndex();
  if (idx == nullptr) {
    return;
  }

  if (idx->covers(_projections)) {
    _projections.setCoveringContext(collection()->id(), idx);
  } else if (this->hasFilter()) {
    // if we have a covering index and a post-filter condition,
    // extract which projections we will need just to execute
    // the filter condition
    containers::FlatHashSet<AttributeNamePath> attributes;

    if (Ast::getReferencedAttributesRecursive(
            this->filter()->node(), this->outVariable(),
            /*expectedAttribute*/ "", attributes,
            plan()->getAst()->query().resourceMonitor())) {
      if (!attributes.empty()) {
        Projections filterProjections(std::move(attributes));
        if (idx->covers(filterProjections)) {
          _filterProjections = std::move(filterProjections);
          _filterProjections.setCoveringContext(collection()->id(), idx);

          attributes.clear();
          // try to exclude all projection attributes which are only
          // used for the filter condition
          if (utils::findProjections(
                  this, outVariable(), /*expectedAttribute*/ "",
                  /*excludeStartNodeFilterCondition*/ true, attributes)) {
            _projections = Projections(std::move(attributes));
            // note: idx->covers(...) modifies the projections object!
            idx->covers(_projections);
          }
        }
      }
    }
  }
}
