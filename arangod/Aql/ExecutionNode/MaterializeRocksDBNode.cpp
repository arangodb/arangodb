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

#include "MaterializeRocksDBNode.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionEngine.h"
#include "Aql/Executor/MaterializeExecutor.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/RegisterPlan.h"
#include "Aql/Variable.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace materialize;

namespace {
constexpr std::string_view kMaterializeNodeInLocalDocIdParam = "inLocalDocId";
}  // namespace

MaterializeRocksDBNode::MaterializeRocksDBNode(
    ExecutionPlan* plan, ExecutionNodeId id, aql::Collection const* collection,
    aql::Variable const& inDocId, aql::Variable const& outVariable,
    aql::Variable const& oldDocVariable)
    : MaterializeNode(plan, id, inDocId, outVariable, oldDocVariable),
      CollectionAccessingNode(collection) {}

MaterializeRocksDBNode::MaterializeRocksDBNode(ExecutionPlan* plan,
                                               arangodb::velocypack::Slice base)
    : MaterializeNode(plan, base),
      CollectionAccessingNode(plan, base),
      _projections(Projections::fromVelocyPack(
          plan->getAst(), base, plan->getAst()->query().resourceMonitor())) {}

void MaterializeRocksDBNode::doToVelocyPack(velocypack::Builder& nodes,
                                            unsigned flags) const {
  // call base class method
  MaterializeNode::doToVelocyPack(nodes, flags);

  // add collection information
  CollectionAccessingNode::toVelocyPack(nodes, flags);
  nodes.add(kMaterializeNodeInLocalDocIdParam, true);

  if (!_projections.empty()) {
    _projections.toVelocyPack(nodes);
  }
}

std::vector<Variable const*> MaterializeRocksDBNode::getVariablesSetHere()
    const {
  if (projections().empty() || !projections().hasOutputRegisters()) {
    return std::vector<Variable const*>{_outVariable};
  } else {
    std::vector<Variable const*> vars;
    vars.reserve(projections().size());
    std::transform(projections().projections().begin(),
                   projections().projections().end(), std::back_inserter(vars),
                   [](auto const& p) { return p.variable; });
    return vars;
  }
}

std::unique_ptr<ExecutionBlock> MaterializeRocksDBNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto writableOutputRegisters = RegIdSet{};
  containers::FlatHashMap<VariableId, RegisterId> varsToRegs;

  RegisterId outDocumentRegId;
  if (projections().empty() || !projections().hasOutputRegisters()) {
    auto it = getRegisterPlan()->varInfo.find(_outVariable->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end())
        << "variable not found = " << _outVariable->id;
    outDocumentRegId = it->second.registerId;
    writableOutputRegisters.emplace(outDocumentRegId);
  } else {
    for (auto const& p : projections().projections()) {
      auto reg = getRegisterPlan()->variableToRegisterId(p.variable);
      varsToRegs.emplace(p.variable->id, reg);
      writableOutputRegisters.emplace(reg);
    }
  }
  RegisterId inNmDocIdRegId;
  {
    auto it = getRegisterPlan()->varInfo.find(_inNonMaterializedDocId->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    inNmDocIdRegId = it->second.registerId;
  }
  RegIdSet readableInputRegisters;
  if (inNmDocIdRegId.isValid()) {
    readableInputRegisters.emplace(inNmDocIdRegId);
  }

  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writableOutputRegisters));

  auto executorInfos = MaterializerExecutorInfos(
      inNmDocIdRegId, outDocumentRegId, engine.getQuery(), collection(),
      _projections, std::move(varsToRegs));

  return std::make_unique<ExecutionBlockImpl<MaterializeRocksDBExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* MaterializeRocksDBNode::clone(ExecutionPlan* plan,
                                             bool withDependencies) const {
  TRI_ASSERT(plan);

  auto c = std::make_unique<MaterializeRocksDBNode>(
      plan, _id, collection(), *_inNonMaterializedDocId, *_outVariable,
      *_oldDocVariable);
  c->_projections = _projections;
  CollectionAccessingNode::cloneInto(*c);
  return cloneHelper(std::move(c), withDependencies);
}
