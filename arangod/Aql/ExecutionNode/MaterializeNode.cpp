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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "MaterializeNode.h"

#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/ExecutionNode/MaterializeSearchNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Variable.h"

#include <string_view>

using namespace arangodb;
using namespace arangodb::aql;
using namespace materialize;

namespace {
constexpr std::string_view kMaterializeNodeInNmDocParam = "inNmDocId";
constexpr std::string_view kMaterializeNodeOutVariableParam = "outVariable";
constexpr std::string_view kMaterializeNodeOldDocVar = "oldDocVariable";
}  // namespace

MaterializeNode* materialize::createMaterializeNode(
    ExecutionPlan* plan, arangodb::velocypack::Slice base) {
  auto isMulti = base.get(MaterializeNode::kMaterializeNodeMultiNodeParam);
  if (isMulti.isTrue()) {
    return new MaterializeSearchNode(plan, base);
  }
  return new MaterializeRocksDBNode(plan, base);
}

MaterializeNode::MaterializeNode(ExecutionPlan* plan, ExecutionNodeId id,
                                 aql::Variable const& inDocId,
                                 aql::Variable const& outVariable,
                                 aql::Variable const& oldDocVariable)
    : ExecutionNode(plan, id),
      _inNonMaterializedDocId(&inDocId),
      _outVariable(&outVariable),
      _oldDocVariable(&oldDocVariable) {}

MaterializeNode::MaterializeNode(ExecutionPlan* plan,
                                 arangodb::velocypack::Slice base)
    : ExecutionNode(plan, base),
      _inNonMaterializedDocId(aql::Variable::varFromVPack(
          plan->getAst(), base, kMaterializeNodeInNmDocParam, true)),
      _outVariable(aql::Variable::varFromVPack(
          plan->getAst(), base, kMaterializeNodeOutVariableParam)),
      _oldDocVariable(aql::Variable::varFromVPack(plan->getAst(), base,
                                                  kMaterializeNodeOldDocVar)) {}

void MaterializeNode::doToVelocyPack(velocypack::Builder& nodes,
                                     unsigned /*flags*/) const {
  nodes.add(VPackValue(kMaterializeNodeInNmDocParam));
  _inNonMaterializedDocId->toVelocyPack(nodes);

  nodes.add(VPackValue(kMaterializeNodeOutVariableParam));
  _outVariable->toVelocyPack(nodes);

  nodes.add(VPackValue(kMaterializeNodeOldDocVar));
  _oldDocVariable->toVelocyPack(nodes);
}

CostEstimate MaterializeNode::estimateCost() const {
  if (_dependencies.empty()) {
    // we should always have dependency as we need input for materializing
    TRI_ASSERT(false);
    return aql::CostEstimate::empty();
  }
  aql::CostEstimate estimate = _dependencies[0]->getCost();
  // we will materialize all output of our dependency
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

void MaterializeNode::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_inNonMaterializedDocId);
}

size_t MaterializeNode::getMemoryUsedBytes() const { return sizeof(*this); }

std::vector<Variable const*> MaterializeNode::getVariablesSetHere() const {
  return std::vector<Variable const*>{_outVariable};
}
