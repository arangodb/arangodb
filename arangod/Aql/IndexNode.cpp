////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/IndexExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/CountCache.h"
#include "Transaction/Methods.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

/// @brief constructor
IndexNode::IndexNode(ExecutionPlan* plan, ExecutionNodeId id,
                     Collection const* collection, Variable const* outVariable,
                     std::vector<transaction::Methods::IndexHandle> const& indexes,
                     std::unique_ptr<Condition> condition, IndexIteratorOptions const& opts)
    : ExecutionNode(plan, id),
      DocumentProducingNode(outVariable),
      CollectionAccessingNode(collection),
      _indexes(indexes),
      _condition(std::move(condition)),
      _needsGatherNodeSort(false),
      _options(opts),
      _outNonMaterializedDocId(nullptr) {
  TRI_ASSERT(_condition != nullptr);

  initIndexCoversProjections();
}

/// @brief constructor for IndexNode
IndexNode::IndexNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      DocumentProducingNode(plan, base),
      CollectionAccessingNode(plan, base),
      _indexes(),
      _needsGatherNodeSort(
          basics::VelocyPackHelper::getBooleanValue(base, "needsGatherNodeSort", false)),
      _options(),
      _outNonMaterializedDocId(
          aql::Variable::varFromVPack(plan->getAst(), base, "outNmDocId", true)) {
  _options.sorted = basics::VelocyPackHelper::getBooleanValue(base, "sorted", true);
  _options.ascending =
      basics::VelocyPackHelper::getBooleanValue(base, "ascending", false);
  _options.evaluateFCalls =
      basics::VelocyPackHelper::getBooleanValue(base, "evalFCalls", true);
  _options.limit = basics::VelocyPackHelper::getNumericValue(base, "limit", 0);

  if (_options.sorted && base.isObject() && base.get("reverse").isBool()) {
    // legacy
    _options.sorted = true;
    _options.ascending = !(base.get("reverse").getBool());
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

  TRI_ASSERT(_condition != nullptr);

  initIndexCoversProjections();

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
            "\"indexValuesVars[*].id\" unable to find variable by id %d", varId);
      }
      _outNonMaterializedIndVars.second.try_emplace(var, fieldNumber);
    }
  }
}

/// @brief called to build up the matching positions of the index values for
/// the projection attributes (if any)
void IndexNode::initIndexCoversProjections() {
  _coveringIndexAttributePositions.clear();

  if (_indexes.empty()) {
    // no indexes used
    return;
  }

  // cannot apply the optimization if we use more than one different index
  auto const& idx = _indexes[0];
  for (size_t i = 1; i < _indexes.size(); ++i) {
    if (_indexes[i] != idx) {
      // different index used => optimization not possible
      return;
    }
  }

  // note that we made sure that if we have multiple index instances, they
  // are actually all of the same index

  if (!idx->hasCoveringIterator()) {
    // index does not have a covering index iterator
    return;
  }

  // check if we can use covering indexes
  auto const& fields = idx->coveredFields();

  if (fields.size() < projections().size()) {
    // we will not be able to satisfy all requested projections with this index
    return;
  }

  std::vector<size_t> coveringAttributePositions;
  // test if the index fields are the same fields as used in the projection
  std::string result;
  for (auto const& it : projections()) {
    bool found = false;
    for (size_t j = 0; j < fields.size(); ++j) {
      result.clear();
      TRI_AttributeNamesToString(fields[j], result, false);
      if (result == it) {
        found = true;
        coveringAttributePositions.emplace_back(j);
        break;
      }
    }
    if (!found) {
      return;
    }
  }

  _coveringIndexAttributePositions = std::move(coveringAttributePositions);
  _options.forceProjection = true;
}

/// @brief toVelocyPack, for IndexNode
void IndexNode::toVelocyPackHelper(VPackBuilder& builder, unsigned flags,
                                   std::unordered_set<ExecutionNode const*>& seen) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(builder, flags, seen);

  // add outvariable and projections
  DocumentProducingNode::toVelocyPack(builder, flags);

  // add collection information
  CollectionAccessingNode::toVelocyPack(builder, flags);

  // Now put info about vocbase and cid in there
  builder.add("needsGatherNodeSort", VPackValue(_needsGatherNodeSort));
  builder.add("indexCoversProjections",
              VPackValue(!_coveringIndexAttributePositions.empty()));

  builder.add(VPackValue("indexes"));
  {
    VPackArrayBuilder guard(&builder);
    for (auto const& index : _indexes) {
      index->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Estimates));
    }
  }
  builder.add(VPackValue("condition"));
  _condition->toVelocyPack(builder, flags);
  // IndexIteratorOptions
  builder.add("sorted", VPackValue(_options.sorted));
  builder.add("ascending", VPackValue(_options.ascending));
  builder.add("reverse", VPackValue(!_options.ascending));  // legacy
  builder.add("evalFCalls", VPackValue(_options.evaluateFCalls));
  builder.add("limit", VPackValue(_options.limit));

  if (isLateMaterialized()) {
    builder.add(VPackValue("outNmDocId"));
    _outNonMaterializedDocId->toVelocyPack(builder);

    builder.add("indexIdOfVars", VPackValue(_outNonMaterializedIndVars.first.id()));
    // container _indexes contains a few items
    auto indIt = std::find_if(_indexes.cbegin(), _indexes.cend(), [this](auto const& index) {
      return index->id() == _outNonMaterializedIndVars.first;
    });
    TRI_ASSERT(indIt != _indexes.cend());
    auto const& fields = (*indIt)->fields();
    VPackArrayBuilder arrayScope(&builder, "indexValuesVars");
    for (auto const& fieldVar : _outNonMaterializedIndVars.second) {
      VPackObjectBuilder objectScope(&builder);
      builder.add("fieldNumber", VPackValue(fieldVar.second));
      builder.add("id", VPackValue(fieldVar.first->id));
      builder.add("name", VPackValue(fieldVar.first->name));  // for explainer.js
      std::string fieldName;
      TRI_ASSERT(fieldVar.second < fields.size());
      basics::TRI_AttributeNamesToString(fields[fieldVar.second], fieldName, true);
      builder.add("field", VPackValue(fieldName));  // for explainer.js
    }
  }

  // And close it:
  builder.close();
}

/// @brief adds a UNIQUE() to a dynamic IN condition
arangodb::aql::AstNode* IndexNode::makeUnique(arangodb::aql::AstNode* node) const {
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
      return ast->createNodeFunctionCall(TRI_CHAR_LENGTH_PAIR("SORTED_UNIQUE"), array);
    }
    // a regular UNIQUE will do
    return ast->createNodeFunctionCall(TRI_CHAR_LENGTH_PAIR("UNIQUE"), array);
  }

  // presumably an array with no or a single member
  return node;
}

void IndexNode::initializeOnce(bool& hasV8Expression, std::vector<Variable const*>& inVars,
                               std::vector<RegisterId>& inRegs,
                               std::vector<std::unique_ptr<NonConstExpression>>& nonConstExpressions) const {
  // instantiate expressions:
  auto instantiateExpression = [&](AstNode* a, std::vector<size_t>&& idxs) -> void {
    // all new AstNodes are registered with the Ast in the Query
    auto e = std::make_unique<Expression>(_plan->getAst(), a);

    TRI_IF_FAILURE("IndexBlock::initialize") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    hasV8Expression |= e->willUseV8();

    VarSet innerVars;
    e->variables(innerVars);

    nonConstExpressions.emplace_back(
        std::make_unique<NonConstExpression>(std::move(e), std::move(idxs)));

    for (auto const& v : innerVars) {
      inVars.emplace_back(v);
      auto it = getRegisterPlan()->varInfo.find(v->id);
      TRI_ASSERT(it != getRegisterPlan()->varInfo.cend());
      TRI_ASSERT(it->second.registerId < RegisterPlan::MaxRegisterId);
      inRegs.emplace_back(it->second.registerId);
    }
  };

  if (_condition->root() != nullptr) {
    auto outVariable = _outVariable;
    std::function<bool(AstNode const*)> hasOutVariableAccess = [&](AstNode const* node) -> bool {
      if (node->isAttributeAccessForVariable(outVariable, true)) {
        return true;
      }

      bool accessedInSubtree = false;
      for (size_t i = 0; i < node->numMembers() && !accessedInSubtree; i++) {
        accessedInSubtree = hasOutVariableAccess(node->getMemberUnchecked(i));
      }

      return accessedInSubtree;
    };

    auto instFCallArgExpressions = [&](AstNode* fcall, std::vector<size_t>&& indexPath) {
      TRI_ASSERT(1 == fcall->numMembers());
      indexPath.emplace_back(0);  // for the arguments array
      AstNode* array = fcall->getMemberUnchecked(0);
      for (size_t k = 0; k < array->numMembers(); k++) {
        AstNode* child = array->getMemberUnchecked(k);
        if (!child->isConstant() && !hasOutVariableAccess(child)) {
          std::vector<size_t> idx = indexPath;
          idx.emplace_back(k);
          instantiateExpression(child, std::move(idx));

          TRI_IF_FAILURE("IndexBlock::initializeExpressions") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        }
      }
    };

    // conditions can be of the form (a [<|<=|>|=>] b) && ...
    // in case of a geo spatial index a might take the form
    // of a GEO_* function. We might need to evaluate fcall arguments
    for (size_t i = 0; i < _condition->root()->numMembers(); ++i) {
      auto andCond = _condition->root()->getMemberUnchecked(i);
      for (size_t j = 0; j < andCond->numMembers(); ++j) {
        auto leaf = andCond->getMemberUnchecked(j);

        // FCALL at this level is most likely a geo index
        if (leaf->type == NODE_TYPE_FCALL) {
          instFCallArgExpressions(leaf, {i, j});
          continue;
        } else if (leaf->numMembers() != 2) {
          continue;
        }

        // We only support binary conditions
        TRI_ASSERT(leaf->numMembers() == 2);
        AstNode* lhs = leaf->getMember(0);
        AstNode* rhs = leaf->getMember(1);

        if (lhs->isAttributeAccessForVariable(outVariable, false)) {
          // Index is responsible for the left side, check if right side
          // has to be evaluated
          if (!rhs->isConstant()) {
            if (leaf->type == NODE_TYPE_OPERATOR_BINARY_IN) {
              rhs = makeUnique(rhs);
            }
            instantiateExpression(rhs, {i, j, 1});
            TRI_IF_FAILURE("IndexBlock::initializeExpressions") {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
            }
          }
        } else {
          // Index is responsible for the right side, check if left side
          // has to be evaluated

          if (lhs->type == NODE_TYPE_FCALL && !options().evaluateFCalls) {
            // most likely a geo index condition
            instFCallArgExpressions(lhs, {i, j, 0});
          } else if (!lhs->isConstant()) {
            instantiateExpression(lhs, {i, j, 0});
            TRI_IF_FAILURE("IndexBlock::initializeExpressions") {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
            }
          }
        }
      }
    }
  }
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> IndexNode::createBlock(
    ExecutionEngine& engine, std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  if (!engine.waitForSatellites(engine.getQuery(), collection())) {
    double maxWait = engine.getQuery().queryOptions().satelliteSyncWait;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC,
                                   "collection " + collection()->name() +
                                       " did not come into sync in time (" +
                                       std::to_string(maxWait) + ")");
  }

  bool hasV8Expression = false;
  /// @brief _inVars, a vector containing for each expression above
  /// a vector of Variable*, used to execute the expression
  std::vector<Variable const*> inVars;

  /// @brief _inRegs, a vector containing for each expression above
  /// a vector of RegisterId, used to execute the expression
  std::vector<RegisterId> inRegs;

  /// @brief _nonConstExpressions, list of all non const expressions, mapped
  /// by their _condition node path indexes
  std::vector<std::unique_ptr<NonConstExpression>> nonConstExpressions;
  initializeOnce(hasV8Expression, inVars, inRegs, nonConstExpressions);

  auto const outVariable = isLateMaterialized() ? _outNonMaterializedDocId : _outVariable;
  auto const outRegister = variableToRegisterId(outVariable);
  auto numIndVarsRegisters =
      static_cast<aql::RegisterCount>(_outNonMaterializedIndVars.second.size());
  TRI_ASSERT(0 == numIndVarsRegisters || isLateMaterialized());

  // We could be asked to produce only document id for later materialization or full document body at once
  aql::RegisterCount numDocumentRegs = 1;

  // if late materialized
  // We have one additional output register for each index variable which is used later, before
  // the output register for document id
  // These must of course fit in the available registers.
  // There may be unused registers reserved for later blocks.
  RegIdSet writableOutputRegisters;
  writableOutputRegisters.reserve(numDocumentRegs + numIndVarsRegisters);
  writableOutputRegisters.emplace(outRegister);

  auto const& varInfos = getRegisterPlan()->varInfo;
  IndexValuesRegisters outNonMaterializedIndRegs;
  outNonMaterializedIndRegs.first = _outNonMaterializedIndVars.first;
  outNonMaterializedIndRegs.second.reserve(_outNonMaterializedIndVars.second.size());
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

  TRI_ASSERT(writableOutputRegisters.size() == numDocumentRegs + numIndVarsRegisters);

  auto registerInfos = createRegisterInfos({}, std::move(writableOutputRegisters));

  auto executorInfos =
      IndexExecutorInfos(outRegister, engine.getQuery(), this->collection(),
                         _outVariable, isProduceResult(), this->_filter.get(),
                         this->projections(), this->coveringIndexAttributePositions(),
                         std::move(nonConstExpressions), std::move(inVars),
                         std::move(inRegs), hasV8Expression, doCount(), _condition->root(),
                         this->getIndexes(), _plan->getAst(), this->options(),
                         _outNonMaterializedIndVars, std::move(outNonMaterializedIndRegs));

  return std::make_unique<ExecutionBlockImpl<IndexExecutor>>(&engine, this,
                                                             std::move(registerInfos),
                                                             std::move(executorInfos));
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
    outNonMaterializedIndVars.second.reserve(_outNonMaterializedIndVars.second.size());
    for (auto& indVar : _outNonMaterializedIndVars.second) {
      outNonMaterializedIndVars.second.try_emplace(
          plan->getAst()->variables()->createVariable(indVar.first), indVar.second);
    }
  } else {
    outNonMaterializedIndVars = _outNonMaterializedIndVars;
  }

  auto c =
      std::make_unique<IndexNode>(plan, _id, collection(), outVariable, _indexes,
                                  std::unique_ptr<Condition>(_condition->clone()), _options);

  c->projections(_projections);
  c->needsGatherNodeSort(_needsGatherNodeSort);
  c->initIndexCoversProjections();
  c->_outNonMaterializedDocId = outNonMaterializedDocId;
  c->_outNonMaterializedIndVars = std::move(outNonMaterializedIndVars);
  CollectionAccessingNode::cloneInto(*c);
  DocumentProducingNode::cloneInto(plan, *c);
  return cloneHelper(std::move(c), withDependencies, withProperties);
}

/// @brief destroy the IndexNode
IndexNode::~IndexNode() = default;

/// @brief the cost of an index node is a multiple of the cost of
/// its unique dependency
CostEstimate IndexNode::estimateCost() const {
  CostEstimate estimate = _dependencies.at(0)->getCost();
  size_t incoming = estimate.estimatedNrItems;

  transaction::Methods& trx = _plan->getAst()->query().trxForOptimization();
  // estimate for the number of documents in the collection. may be outdated...
  size_t const itemsInCollection = collection()->count(&trx, transaction::CountType::TryCache);
  size_t totalItems = 0;
  double totalCost = 0.0;

  auto root = _condition->root();

  for (size_t i = 0; i < _indexes.size(); ++i) {
    Index::FilterCosts costs = Index::FilterCosts::defaultCosts(itemsInCollection);

    if (root != nullptr && root->numMembers() > i) {
      arangodb::aql::AstNode const* condition = root->getMember(i);
      costs = _indexes[i]->supportsFilterCondition(std::vector<std::shared_ptr<Index>>(),
                                                   condition, _outVariable,
                                                   itemsInCollection);
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

  vars.erase(_outVariable);
}
ExecutionNode::NodeType IndexNode::getType() const { return INDEX; }

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
                 _outNonMaterializedIndVars.second.cend(), std::back_inserter(vars),
                 [](auto const& indVar) { return indVar.first; });

  return vars;
}

std::vector<transaction::Methods::IndexHandle> const& IndexNode::getIndexes() const {
  return _indexes;
}

void IndexNode::setLateMaterialized(aql::Variable const* docIdVariable, IndexId commonIndexId,
                                    IndexVarsInfo const& indexVariables) {
  _outNonMaterializedDocId = docIdVariable;
  _outNonMaterializedIndVars.first = commonIndexId;
  _outNonMaterializedIndVars.second.clear();
  _outNonMaterializedIndVars.second.reserve(indexVariables.size());
  for (auto& indVars : indexVariables) {
    _outNonMaterializedIndVars.second.try_emplace(indVars.second.var,
                                                  indVars.second.indexFieldNum);
  }
}

NonConstExpression::NonConstExpression(std::unique_ptr<Expression> exp,
                                       std::vector<size_t>&& idxPath)
    : expression(std::move(exp)), indexPath(std::move(idxPath)) {}
