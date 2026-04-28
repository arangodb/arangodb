////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#include "EnumerateNearVectorNode.h"

#include "Aql/ExecutionNode/EnumerateNearVectorNode.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/RegisterPlan.h"
#include "Aql/Variable.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/Collection.h"
#include "Aql/Executor/EnumerateNearVectorExecutor.h"
#include "Aql/Query.h"
#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "Indexes/Index.h"
#include "Aql/Ast.h"
#include "Inspection/VPack.h"

#include <functional>

namespace arangodb::aql {

namespace {
static constexpr std::string_view kInVariableName{"inVariable"};
static constexpr std::string_view kDistanceOutVariable{"distanceOutVariable"};
static constexpr std::string_view kLimit{"limit"};
static constexpr std::string_view kOffset{"offset"};
static constexpr std::string_view kIsCoveredByStoredValues{
    "isCoveredByStoredValues"};
static constexpr std::string_view kSearchParameters{"searchParameters"};
static constexpr std::string_view kStrategy{"strategy"};
}  // namespace

EnumerateNearVectorNode::EnumerateNearVectorNode(
    ExecutionPlan* plan, arangodb::aql::ExecutionNodeId id,
    Variable const* inVariable, Variable const* outVariable,
    Variable const* distanceOutVariable, std::size_t limit, bool ascending,
    std::size_t offset, vector::SearchParameters searchParameters,
    aql::Collection const* collection,
    transaction::Methods::IndexHandle indexHandle,
    std::unique_ptr<Expression> filterExpression, bool isCoveredByStoredValues)
    : ExecutionNode(plan, id),
      DocumentProducingNode(outVariable),
      CollectionAccessingNode(collection),
      _inVariable(inVariable),
      _distanceOutVariable(distanceOutVariable),
      _limit(limit),
      _ascending(ascending),
      _offset(offset),
      _searchParameters(std::move(searchParameters)),
      _index(std::move(indexHandle)),
      _isCoveredByStoredValues(isCoveredByStoredValues) {
  TRI_ASSERT(_index->type() == Index::IndexType::TRI_IDX_TYPE_VECTOR_INDEX);
  TRI_ASSERT(filterExpression != nullptr || !_isCoveredByStoredValues);
  if (filterExpression != nullptr) {
    DocumentProducingNode::setFilter(std::move(filterExpression));
  }
}

ExecutionNode::NodeType EnumerateNearVectorNode::getType() const {
  return ENUMERATE_NEAR_VECTORS;
}

size_t EnumerateNearVectorNode::getMemoryUsedBytes() const {
  return sizeof(*this);
}

std::vector<std::pair<VariableId, RegisterId>>
EnumerateNearVectorNode::extractFilterVarsToRegs() const {
  TRI_ASSERT(hasFilter());
  VarSet inVars;
  filter()->variables(inVars);
  std::vector<std::pair<VariableId, RegisterId>> filterVarsToRegs;
  filterVarsToRegs.reserve(inVars.size());

  for (auto const& var : inVars) {
    TRI_ASSERT(var != nullptr);
    if (var->id == _outVariable->id) {
      // the doc variable is bound by the index code when it loads the
      // document or storedValues; it is not read from a register.
      continue;
    }
    auto regId = variableToRegisterId(var);
    filterVarsToRegs.emplace_back(var->id, std::move(regId));
  }

  return filterVarsToRegs;
}

std::unique_ptr<ExecutionBlock> EnumerateNearVectorNode::createBlock(
    ExecutionEngine& engine) const {
  auto writableOutputRegisters = RegIdSet{};
  containers::FlatHashMap<VariableId, RegisterId> varsToRegs;

  // The doc-variable register is written for everything except kCovered.
  RegisterId outDocumentRegId = RegisterId::maxRegisterId;
  if (_strategy != Strategy::kCovered) {
    outDocumentRegId = variableToRegisterId(_outVariable);
    writableOutputRegisters.emplace(outDocumentRegId);
  }

  RegisterId outDistanceRegId = variableToRegisterId(_distanceOutVariable);
  writableOutputRegisters.emplace(outDistanceRegId);

  // per-projection output registers (set by the projections rule)
  containers::FlatHashMap<VariableId, RegisterId> projectionVarsToRegs;
  for (size_t i = 0; i < _projections.size(); ++i) {
    auto const* var = _projections[i].variable;
    if (var != nullptr) {
      RegisterId regId = variableToRegisterId(var);
      writableOutputRegisters.emplace(regId);
      projectionVarsToRegs.try_emplace(var->id, regId);
    }
  }

  RegisterId inNmDocIdRegId = variableToRegisterId(_inVariable);
  RegIdSet readableInputRegisters;
  readableInputRegisters.emplace(inNmDocIdRegId);

  // check which variables are used by the node's post-filter
  std::vector<std::pair<VariableId, RegisterId>> filterVarsToRegs;
  if (hasFilter()) {
    filterVarsToRegs = extractFilterVarsToRegs();
    for (auto const& [_, regId] : filterVarsToRegs) {
      readableInputRegisters.emplace(regId);
    }
  }

  // Capture is only meaningful when there is a filter (the iterator runs
  // and has a doc to surface). For kPassThroughId there is nothing to
  // extract from the doc, so we don't ask the iterator to copy bytes.
  bool const captureDocuments =
      hasFilter() && _strategy != Strategy::kPassThroughId;

  EnumerateNearVectorsExecutorInfos executorInfos{
      .inputReg = inNmDocIdRegId,
      .outDocumentIdReg = outDocumentRegId,
      .outDistancesReg = outDistanceRegId,
      .projectionVarsToRegs = std::move(projectionVarsToRegs),
      .index = _index,
      .queryContext = engine.getQuery(),
      .collection = _collectionAccess.collection(),
      .searchConfig =
          vector::SearchConfig{
              .searchParameters = _searchParameters,
              .topK = _limit + _offset,
              .filterExpression = filter(),
              .filterVarsToRegs = std::move(filterVarsToRegs),
              .documentVariable = _outVariable,
              // Pick the storedValues-only iterator only when *both* filter
              // and projections are coverable -- that's exactly kCovered.
              .useStoredValuesIterator = _strategy == Strategy::kCovered,
              .captureDocuments = captureDocuments,
          },
      .projections = _projections,
      .strategy = _strategy,
  };
  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writableOutputRegisters));

  return std::make_unique<ExecutionBlockImpl<EnumerateNearVectorsExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* EnumerateNearVectorNode::clone(ExecutionPlan* plan,
                                              bool withDependencies) const {
  auto filterExpression = std::invoke([&]() -> std::unique_ptr<Expression> {
    if (hasFilter()) {
      return filter()->clone(plan->getAst(), true);
    }
    return nullptr;
  });

  auto c = std::make_unique<EnumerateNearVectorNode>(
      plan, _id, _inVariable, _outVariable, _distanceOutVariable, _limit,
      _ascending, _offset, _searchParameters, collection(), _index,
      std::move(filterExpression), _isCoveredByStoredValues);
  c->setStrategy(_strategy);
  c->_projections = _projections;
  c->_filterProjections = _filterProjections;
  CollectionAccessingNode::cloneInto(*c);
  return cloneHelper(std::move(c), withDependencies);
}

CostEstimate EnumerateNearVectorNode::estimateCost() const {
  // TODO(jbajic) add nLists and nProbe parameters into play
  CostEstimate estimate = _dependencies.at(0)->getCost();

  if (transaction::Methods& trx = _plan->getAst()->query().trxForOptimization();
      trx.status() == transaction::Status::RUNNING) {
    estimate.estimatedNrItems = std::min(
        _limit,
        estimate.estimatedNrItems *
            collection()->count(&trx, transaction::CountType::kTryCache));
  } else {
    estimate.estimatedNrItems = _limit;
  }
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

void EnumerateNearVectorNode::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_inVariable);
  if (hasFilter()) {
    Ast::getReferencedVariables(filter()->node(), vars);
  }
  // the filter expression and any projections reference _outVariable, but it
  // is not read from a register here: the doc is bound internally by the
  // index code (loaded doc or storedValues).
  vars.erase(_outVariable);
  // projection output variables are produced here, not consumed
  for (size_t i = 0; i < _projections.size(); ++i) {
    if (_projections[i].variable != nullptr) {
      vars.erase(_projections[i].variable);
    }
  }
}

std::vector<const Variable*> EnumerateNearVectorNode::getVariablesSetHere()
    const {
  std::vector<Variable const*> result;
  result.reserve(2 + _projections.size());
  if (_strategy != Strategy::kCovered) {
    result.push_back(_outVariable);
  }
  result.push_back(_distanceOutVariable);
  for (size_t i = 0; i < _projections.size(); ++i) {
    if (_projections[i].variable != nullptr) {
      result.push_back(_projections[i].variable);
    }
  }
  return result;
}

bool EnumerateNearVectorNode::isProduceResult() const {
  // The vector index always produces a result (either the doc, projection
  // values, or a doc-id label for downstream materialization).
  return true;
}

std::string_view EnumerateNearVectorNode::strategyName(Strategy s) noexcept {
  switch (s) {
    case Strategy::kPassThroughId:
      return "pass-through-id";
    case Strategy::kCovered:
      return "covering";
    case Strategy::kDocument:
      return "document";
  }
}

void EnumerateNearVectorNode::doToVelocyPack(velocypack::Builder& builder,
                                             unsigned int flags) const {
  // outVariable, projections, filter, filterProjections, count, etc.
  DocumentProducingNode::toVelocyPack(builder, flags);

  builder.add(VPackValue(kInVariableName));
  _inVariable->toVelocyPack(builder);

  builder.add(VPackValue(kDistanceOutVariable));
  _distanceOutVariable->toVelocyPack(builder);

  builder.add(kLimit, VPackValue(_limit));
  builder.add(kOffset, VPackValue(_offset));
  builder.add(kIsCoveredByStoredValues, VPackValue(_isCoveredByStoredValues));
  builder.add(kStrategy, VPackValue(strategyName(_strategy)));

  builder.add(VPackValue(kSearchParameters));
  builder.add(velocypack::serialize(_searchParameters));

  CollectionAccessingNode::toVelocyPack(builder, flags);

  builder.add(VPackValue("index"));
  _index->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Estimates));
}

EnumerateNearVectorNode::EnumerateNearVectorNode(
    ExecutionPlan* plan, arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      DocumentProducingNode(plan, base),
      CollectionAccessingNode(plan, base),
      _inVariable(
          Variable::varFromVPack(plan->getAst(), base, kInVariableName)),
      _distanceOutVariable(
          Variable::varFromVPack(plan->getAst(), base, kDistanceOutVariable)),
      _limit(base.get(kLimit).getNumericValue<std::size_t>()),
      _offset(base.get(kOffset).getNumericValue<std::size_t>()),
      _isCoveredByStoredValues(base.get(kIsCoveredByStoredValues).getBool()) {
  std::string iid = base.get("index").get("id").copyString();

  if (auto const res = velocypack::deserializeWithStatus(
          base.get(kSearchParameters), _searchParameters);
      !res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "Deserialization of searchParameters has failed!");
  }

  _index = collection()->indexByIdentifier(iid);

  if (auto strategySlice = base.get(kStrategy); strategySlice.isString()) {
    auto const name = strategySlice.stringView();
    if (name == "covering") {
      _strategy = Strategy::kCovered;
    } else if (name == "document") {
      _strategy = Strategy::kDocument;
    } else {
      _strategy = Strategy::kPassThroughId;
    }
  }
}

void EnumerateNearVectorNode::replaceVariables(
    const std::unordered_map<VariableId, const Variable*>& replacements) {
  DocumentProducingNode::replaceVariables(replacements);
  _inVariable = Variable::replace(_inVariable, replacements);
  _distanceOutVariable = Variable::replace(_distanceOutVariable, replacements);
}

bool EnumerateNearVectorNode::isAscending() const noexcept {
  return _ascending;
}

void EnumerateNearVectorNode::setIndex(
    transaction::Methods::IndexHandle indexHandle) {
  TRI_ASSERT(indexHandle->type() ==
             Index::IndexType::TRI_IDX_TYPE_VECTOR_INDEX);
  _index = std::move(indexHandle);
}

void EnumerateNearVectorNode::setIsCoveredByStoredValues(
    bool isCoveredByStoredValues) noexcept {
  _isCoveredByStoredValues = isCoveredByStoredValues;
}

}  // namespace arangodb::aql
