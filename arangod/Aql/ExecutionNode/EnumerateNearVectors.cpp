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
////////////////////////////////////////////////////////////////////////////////

#include "EnumerateNearVectors.h"

#include "Aql/ExecutionPlan.h"
#include "Aql/Collection.h"
#include "Aql/Query.h"
#include "Indexes/Index.h"
#include "Aql/Ast.h"

namespace arangodb::aql {

namespace {
constexpr std::string_view kInVariableName = "inVariable";
constexpr std::string_view kDocumentOutVariable = "documentOutVariable";
constexpr std::string_view kDistanceOutVariable = "distanceOutVariable";
constexpr std::string_view kOldDocumentVariable = "oldDocumentVariable";
constexpr std::string_view kLimit = "limit";
}  // namespace

EnumerateNearVectors::EnumerateNearVectors(
    ExecutionPlan* plan, arangodb::aql::ExecutionNodeId id,
    Variable const* inVariable, Variable const* oldDocumentVariable,
    Variable const* documentOutVariable, Variable const* distanceOutVariable,
    std::size_t limit, aql::Collection const* collection,
    transaction::Methods::IndexHandle indexHandle)
    : ExecutionNode(plan, id),
      CollectionAccessingNode(collection),
      _inVariable(inVariable),
      _oldDocumentVariable(oldDocumentVariable),
      _documentOutVariable(documentOutVariable),
      _distanceOutVariable(distanceOutVariable),
      _limit(limit),
      _index(std::move(indexHandle)) {}

ExecutionNode::NodeType EnumerateNearVectors::getType() const {
  return ENUMERATE_NEAR_VECTORS;
}

size_t EnumerateNearVectors::getMemoryUsedBytes() const { return 0; }

std::unique_ptr<ExecutionBlock> EnumerateNearVectors::createBlock(
    ExecutionEngine& engine) const {
  abort();
}

ExecutionNode* EnumerateNearVectors::clone(ExecutionPlan* plan,
                                           bool withDependencies) const {
  auto c = std::make_unique<EnumerateNearVectors>(
      plan, _id, _inVariable, _oldDocumentVariable, _documentOutVariable,
      _distanceOutVariable, _limit, collection(), _index);
  CollectionAccessingNode::cloneInto(*c);
  return cloneHelper(std::move(c), withDependencies);
}

CostEstimate EnumerateNearVectors::estimateCost() const {
  // TODO
  CostEstimate estimate = _dependencies.at(0)->getCost();
  estimate.estimatedNrItems *= _limit;
  return estimate;
}

void EnumerateNearVectors::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_inVariable);
}

std::vector<const Variable*> EnumerateNearVectors::getVariablesSetHere() const {
  return {_documentOutVariable, _distanceOutVariable};
}

void EnumerateNearVectors::doToVelocyPack(velocypack::Builder& builder,
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

  CollectionAccessingNode::toVelocyPack(builder, flags);

  builder.add(VPackValue("index"));
  _index->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Estimates));
}

EnumerateNearVectors::EnumerateNearVectors(ExecutionPlan* plan,
                                           arangodb::velocypack::Slice base)
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
      _limit(base.get(kLimit).getNumericValue<std::size_t>()) {
  std::string iid = base.get("index").get("id").copyString();

  _index = collection()->indexByIdentifier(iid);
}

void EnumerateNearVectors::replaceVariables(
    const std::unordered_map<VariableId, const Variable*>& replacements) {
  _inVariable = Variable::replace(_inVariable, replacements);
  _documentOutVariable = Variable::replace(_documentOutVariable, replacements);
  _distanceOutVariable = Variable::replace(_distanceOutVariable, replacements);
}

}  // namespace arangodb::aql
