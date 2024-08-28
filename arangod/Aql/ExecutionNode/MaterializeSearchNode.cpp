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

#include "MaterializeSearchNode.h"

#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/MaterializeExecutor.h"
#include "Aql/RegisterPlan.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace materialize;

MaterializeSearchNode::MaterializeSearchNode(
    ExecutionPlan* plan, ExecutionNodeId id, aql::Variable const& inDocId,
    aql::Variable const& outVariable, aql::Variable const& oldDocVariable)
    : MaterializeNode(plan, id, inDocId, outVariable, oldDocVariable) {}

MaterializeSearchNode::MaterializeSearchNode(ExecutionPlan* plan,
                                             arangodb::velocypack::Slice base)
    : MaterializeNode(plan, base) {}

std::unique_ptr<ExecutionBlock> MaterializeSearchNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  RegisterId inNmDocIdRegId;
  {
    auto it = getRegisterPlan()->varInfo.find(_inNonMaterializedDocId->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    inNmDocIdRegId = it->second.registerId;
  }
  RegisterId outDocumentRegId;
  {
    auto it = getRegisterPlan()->varInfo.find(_outVariable->id);
    TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
    outDocumentRegId = it->second.registerId;
  }

  RegIdSet readableInputRegisters{inNmDocIdRegId};
  auto writableOutputRegisters = RegIdSet{outDocumentRegId};

  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writableOutputRegisters));

  auto executorInfos = MaterializerExecutorInfos(
      inNmDocIdRegId, outDocumentRegId, engine.getQuery(), nullptr, {}, {});

  return std::make_unique<ExecutionBlockImpl<MaterializeSearchExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* MaterializeSearchNode::clone(ExecutionPlan* plan,
                                            bool withDependencies) const {
  TRI_ASSERT(plan);

  return cloneHelper(
      std::make_unique<MaterializeSearchNode>(
          plan, _id, *_inNonMaterializedDocId, *_outVariable, *_oldDocVariable),
      withDependencies);
}

void MaterializeSearchNode::doToVelocyPack(velocypack::Builder& nodes,
                                           unsigned flags) const {
  // call base class method
  MaterializeNode::doToVelocyPack(nodes, flags);
  nodes.add(kMaterializeNodeMultiNodeParam, velocypack::Value(true));
}
