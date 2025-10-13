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
#include "Basics/Exceptions.h"
#include "Indexes/Index.h"
#include "Aql/Ast.h"
#include "Inspection/VPack.h"

#include <functional>

namespace arangodb::aql {

namespace {
constexpr std::string_view kInVariableName = "inVariable";
constexpr std::string_view kDocumentOutVariable = "documentOutVariable";
constexpr std::string_view kDistanceOutVariable = "distanceOutVariable";
constexpr std::string_view kOldDocumentVariable = "oldDocumentVariable";
constexpr std::string_view kLimit = "limit";
constexpr std::string_view kOffset = "offset";
constexpr std::string_view kSearchParameters = "searchParameters";
constexpr std::string_view kFilterExpression = "filterExpression";
}  // namespace

EnumerateNearVectorNode::EnumerateNearVectorNode(
    ExecutionPlan* plan, arangodb::aql::ExecutionNodeId id,
    Variable const* inVariable, Variable const* oldDocumentVariable,
    Variable const* documentOutVariable, Variable const* distanceOutVariable,
    std::size_t limit, bool ascending, std::size_t offset,
    SearchParameters searchParameters, aql::Collection const* collection,
    transaction::Methods::IndexHandle indexHandle,
    std::unique_ptr<Expression> filterExpression)
    : ExecutionNode(plan, id),
      CollectionAccessingNode(collection),
      _inVariable(inVariable),
      _oldDocumentVariable(oldDocumentVariable),
      _documentOutVariable(documentOutVariable),
      _distanceOutVariable(distanceOutVariable),
      _limit(limit),
      _ascending(ascending),
      _offset(offset),
      _searchParameters(std::move(searchParameters)),
      _index(std::move(indexHandle)),
      _filterExpression(std::move(filterExpression)) {}

ExecutionNode::NodeType EnumerateNearVectorNode::getType() const {
  return ENUMERATE_NEAR_VECTORS;
}

size_t EnumerateNearVectorNode::getMemoryUsedBytes() const {
  return sizeof(*this);
}

std::vector<std::pair<VariableId, RegisterId>>
EnumerateNearVectorNode::extractFilterVarsToRegs() const {
  VarSet inVars;
  _filterExpression->variables(inVars);
  std::vector<std::pair<VariableId, RegisterId>> filterVarsToRegs;
  filterVarsToRegs.reserve(inVars.size());

  // Here we take all variables in the expression
  for (auto const& var : inVars) {
    TRI_ASSERT(var != nullptr);
    if (var->id == _oldDocumentVariable->id) {
      continue;
    }
    auto regId = variableToRegisterId(var);
    filterVarsToRegs.emplace_back(var->id, regId);
  }

  return filterVarsToRegs;
}

std::unique_ptr<ExecutionBlock> EnumerateNearVectorNode::createBlock(
    ExecutionEngine& engine) const {
  auto writableOutputRegisters = RegIdSet{};
  containers::FlatHashMap<VariableId, RegisterId> varsToRegs;

  RegisterId outDocumentRegId = variableToRegisterId(_documentOutVariable);
  writableOutputRegisters.emplace(outDocumentRegId);
  RegisterId outDistanceRegId = variableToRegisterId(_distanceOutVariable);
  writableOutputRegisters.emplace(outDistanceRegId);

  RegisterId inNmDocIdRegId = variableToRegisterId(_inVariable);
  RegIdSet readableInputRegisters;
  readableInputRegisters.emplace(inNmDocIdRegId);

  // check which variables are used by the node's post-filter
  std::vector<std::pair<VariableId, RegisterId>> filterVarsToRegs;
  if (_filterExpression) {
    filterVarsToRegs = extractFilterVarsToRegs();
  }

  auto executorInfos = EnumerateNearVectorsExecutorInfos(
      inNmDocIdRegId, outDocumentRegId, outDistanceRegId, _index,
      engine.getQuery(), _collectionAccess.collection(), _limit, _offset,
      _searchParameters, _filterExpression.get(), std::move(filterVarsToRegs),
      _oldDocumentVariable);
  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writableOutputRegisters));

  return std::make_unique<ExecutionBlockImpl<EnumerateNearVectorsExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* EnumerateNearVectorNode::clone(ExecutionPlan* plan,
                                              bool withDependencies) const {
  auto filterExpression = std::invoke([&]() -> std::unique_ptr<Expression> {
    if (_filterExpression) {
      return _filterExpression->clone(plan->getAst(), true);
    }
    return nullptr;
  });

  auto c = std::make_unique<EnumerateNearVectorNode>(
      plan, _id, _inVariable, _oldDocumentVariable, _documentOutVariable,
      _distanceOutVariable, _limit, _ascending, _offset, _searchParameters,
      collection(), _index, std::move(filterExpression));
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
}

std::vector<const Variable*> EnumerateNearVectorNode::getVariablesSetHere()
    const {
  // TODO(jbajic) the distance might not be used, and documents is materialized
  // regardless if it is being used later on
  return {_documentOutVariable, _distanceOutVariable};
}

void EnumerateNearVectorNode::doToVelocyPack(velocypack::Builder& builder,
                                             unsigned int flags) const {
  builder.add(VPackValue(kInVariableName));
  _inVariable->toVelocyPack(builder);

  builder.add(VPackValue(kOldDocumentVariable));
  _oldDocumentVariable->toVelocyPack(builder);
  builder.add(VPackValue(kDocumentOutVariable));
  _documentOutVariable->toVelocyPack(builder);
  builder.add(VPackValue(kDistanceOutVariable));
  _distanceOutVariable->toVelocyPack(builder);

  builder.add(kLimit, VPackValue(_limit));
  builder.add(kOffset, VPackValue(_offset));

  builder.add(VPackValue(kSearchParameters));
  builder.add(velocypack::serialize(_searchParameters));
  if (_filterExpression != nullptr) {
    builder.add(VPackValue(kFilterExpression));
    _filterExpression->toVelocyPack(builder, flags);
  }

  CollectionAccessingNode::toVelocyPack(builder, flags);

  builder.add(VPackValue("index"));
  _index->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Estimates));
}

EnumerateNearVectorNode::EnumerateNearVectorNode(
    ExecutionPlan* plan, arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      CollectionAccessingNode(plan, base),
      _inVariable(
          Variable::varFromVPack(plan->getAst(), base, kInVariableName)),
      _oldDocumentVariable(
          Variable::varFromVPack(plan->getAst(), base, kOldDocumentVariable)),
      _documentOutVariable(
          Variable::varFromVPack(plan->getAst(), base, kDocumentOutVariable)),
      _distanceOutVariable(
          Variable::varFromVPack(plan->getAst(), base, kDistanceOutVariable)),
      _limit(base.get(kLimit).getNumericValue<std::size_t>()),
      _offset(base.get(kOffset).getNumericValue<std::size_t>()) {
  std::string iid = base.get("index").get("id").copyString();

  if (auto const res = velocypack::deserializeWithStatus(
          base.get(kSearchParameters), _searchParameters);
      !res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "Deserialization of searchParameters has failed!");
  }

  VPackSlice p = base.get(kFilterExpression);
  if (!p.isNone()) {
    Ast* ast = plan->getAst();
    // new AstNode is memory-managed by the Ast
    _filterExpression = std::make_unique<Expression>(ast, ast->createNode(p));
  }

  _index = collection()->indexByIdentifier(iid);
}

void EnumerateNearVectorNode::replaceVariables(
    const std::unordered_map<VariableId, const Variable*>& replacements) {
  _inVariable = Variable::replace(_inVariable, replacements);
  _documentOutVariable = Variable::replace(_documentOutVariable, replacements);
  _distanceOutVariable = Variable::replace(_distanceOutVariable, replacements);
  if (_filterExpression != nullptr) {
    _filterExpression->replaceVariables(replacements);
  }
}

bool EnumerateNearVectorNode::isAscending() const noexcept {
  return _ascending;
}

}  // namespace arangodb::aql
