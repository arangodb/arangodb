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

namespace arangodb::aql {

namespace {
constexpr std::string_view kInVariableName = "inVariable";
constexpr std::string_view kDocumentOutVariable = "documentOutVariable";
constexpr std::string_view kDistanceOutVariable = "distanceOutVariable";
constexpr std::string_view kOldDocumentVariable = "oldDocumentVariable";
constexpr std::string_view kLimit = "limit";
constexpr std::string_view kOffset = "offset";
constexpr std::string_view kSearchParameters = "searchParameters";
}  // namespace

EnumerateNearVectorNode::EnumerateNearVectorNode(
    ExecutionPlan* plan, arangodb::aql::ExecutionNodeId id,
    Variable const* inVariable, Variable const* oldDocumentVariable,
    Variable const* documentOutVariable, Variable const* distanceOutVariable,
    std::size_t limit, bool ascending, std::size_t offset,
    SearchParameters searchParameters, aql::Collection const* collection,
    transaction::Methods::IndexHandle indexHandle)
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
      _index(std::move(indexHandle)) {}

ExecutionNode::NodeType EnumerateNearVectorNode::getType() const {
  return ENUMERATE_NEAR_VECTORS;
}

size_t EnumerateNearVectorNode::getMemoryUsedBytes() const {
  return sizeof(*this);
}

std::unique_ptr<ExecutionBlock> EnumerateNearVectorNode::createBlock(
    ExecutionEngine& engine) const {
  auto writableOutputRegisters = RegIdSet{};
  containers::FlatHashMap<VariableId, RegisterId> varsToRegs;

  RegisterId outDocumentRegId;
  {
    auto itDocument = getRegisterPlan()->varInfo.find(_documentOutVariable->id);
    TRI_ASSERT(itDocument != getRegisterPlan()->varInfo.end());
    outDocumentRegId = itDocument->second.registerId;
    writableOutputRegisters.emplace(outDocumentRegId);
  }

  RegisterId outDistanceRegId;
  {
    auto itDistance = getRegisterPlan()->varInfo.find(_distanceOutVariable->id);
    TRI_ASSERT(itDistance != getRegisterPlan()->varInfo.end());
    outDistanceRegId = itDistance->second.registerId;
    writableOutputRegisters.emplace(outDistanceRegId);
  }

  RegisterId inNmDocIdRegId;
  {
    auto it = getRegisterPlan()->varInfo.find(_inVariable->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    inNmDocIdRegId = it->second.registerId;
  }
  RegIdSet readableInputRegisters;
  readableInputRegisters.emplace(inNmDocIdRegId);

  auto executorInfos = EnumerateNearVectorsExecutorInfos(
      inNmDocIdRegId, outDocumentRegId, outDistanceRegId, _index,
      engine.getQuery(), _collectionAccess.collection(), _limit, _offset,
      _searchParameters);
  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writableOutputRegisters));

  return std::make_unique<ExecutionBlockImpl<EnumerateNearVectorsExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* EnumerateNearVectorNode::clone(ExecutionPlan* plan,
                                              bool withDependencies) const {
  auto c = std::make_unique<EnumerateNearVectorNode>(
      plan, _id, _inVariable, _oldDocumentVariable, _documentOutVariable,
      _distanceOutVariable, _limit, _ascending, _offset, _searchParameters,
      collection(), _index);
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

  _index = collection()->indexByIdentifier(iid);
}

void EnumerateNearVectorNode::replaceVariables(
    const std::unordered_map<VariableId, const Variable*>& replacements) {
  _inVariable = Variable::replace(_inVariable, replacements);
  _documentOutVariable = Variable::replace(_documentOutVariable, replacements);
  _distanceOutVariable = Variable::replace(_distanceOutVariable, replacements);
}

bool EnumerateNearVectorNode::isAscending() const noexcept {
  return _ascending;
}

}  // namespace arangodb::aql
