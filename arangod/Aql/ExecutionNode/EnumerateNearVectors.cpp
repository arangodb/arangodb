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

namespace arangodb::aql {

namespace {
constexpr std::string_view kInVariableName = "inVariable";
constexpr std::string_view kDocumentOutVariable = "documentOutVariable";
constexpr std::string_view kDistanceOutVariable = "distanceOutVariable";
constexpr std::string_view kLimit = "limit";
}  // namespace

EnumerateNearVectors::EnumerateNearVectors(ExecutionPlan* plan,
                                           arangodb::aql::ExecutionNodeId id,
                                           Variable const* inVariable,
                                           Variable const* documentOutVariable,
                                           Variable const* distanceOutVariable,
                                           std::size_t limit)
    : ExecutionNode(plan, id),
      _inVariable(inVariable),
      _documentOutVariable(documentOutVariable),
      _distanceOutVariable(distanceOutVariable),
      _limit(limit) {}

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
  auto c = std::make_unique<EnumerateNearVectors>(plan, _id, _inVariable,
                                                  _documentOutVariable,
                                                  _distanceOutVariable, _limit);

  return cloneHelper(std::move(c), withDependencies);
}

CostEstimate EnumerateNearVectors::estimateCost() const {
  // TODO
  return CostEstimate(1, _limit);
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

  builder.add(VPackValue(kDocumentOutVariable));
  _documentOutVariable->toVelocyPack(builder);
  builder.add(VPackValue(kDistanceOutVariable));
  _distanceOutVariable->toVelocyPack(builder);
  builder.add(kLimit, VPackValue(_limit));
}

EnumerateNearVectors::EnumerateNearVectors(ExecutionPlan* plan,
                                           arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _inVariable(
          Variable::varFromVPack(plan->getAst(), base, kInVariableName)),
      _documentOutVariable(
          Variable::varFromVPack(plan->getAst(), base, kDocumentOutVariable)),
      _distanceOutVariable(
          Variable::varFromVPack(plan->getAst(), base, kDistanceOutVariable)),
      _limit(base.get(kLimit).getNumericValue<std::size_t>()) {}

}  // namespace arangodb::aql
