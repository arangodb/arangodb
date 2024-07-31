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

#include "RemoteNode.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/RemoteExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterInfos.h"
#include "Aql/types.h"
#include "Basics/StringUtils.h"

#include <memory>
#include <string_view>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::aql;

/// @brief constructor for RemoteNode
RemoteNode::RemoteNode(ExecutionPlan* plan,
                       arangodb::velocypack::Slice const& base)
    : DistributeConsumerNode(plan, base),
      _vocbase(&(plan->getAst()->query().vocbase())),
      _server(base.get("server").copyString()),
      _queryId(base.get("queryId").copyString()) {}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> RemoteNode::createBlock(
    ExecutionEngine& engine) const {
  auto const nrOutRegs = getRegisterPlan()->nrRegs[getDepth()];
  auto const nrInRegs = nrOutRegs;

  auto regsToKeep = getRegsToKeepStack();
  auto regsToClear = getRegsToClear();

  // Everything that is cleared here could and should have been cleared before,
  // i.e. before sending it over the network.
  TRI_ASSERT(regsToClear.empty());

  auto infos = RegisterInfos({}, {}, nrInRegs, nrOutRegs,
                             std::move(regsToClear), std::move(regsToKeep));

  return std::make_unique<ExecutionBlockImpl<RemoteExecutor>>(
      &engine, this, std::move(infos), server(), getDistributeId(), queryId());
}

/// @brief clone ExecutionNode recursively
ExecutionNode* RemoteNode::clone(ExecutionPlan* plan,
                                 bool withDependencies) const {
  return cloneHelper(std::make_unique<RemoteNode>(plan, _id, _vocbase, _server,
                                                  getDistributeId(), _queryId),
                     withDependencies);
}

/// @brief doToVelocyPack, for RemoteNode
void RemoteNode::doToVelocyPack(VPackBuilder& nodes, unsigned flags) const {
  // call base class method
  DistributeConsumerNode::doToVelocyPack(nodes, flags);

  nodes.add("server", VPackValue(_server));
  nodes.add("queryId", VPackValue(_queryId));
}

/// @brief estimateCost
CostEstimate RemoteNode::estimateCost() const {
  if (_dependencies.size() == 1) {
    CostEstimate estimate = _dependencies[0]->getCost();
    estimate.estimatedCost += estimate.estimatedNrItems;
    return estimate;
  }
  // We really should not get here, but if so, do something bordering on
  // sensible:
  CostEstimate estimate = CostEstimate::empty();
  estimate.estimatedNrItems = 1;
  estimate.estimatedCost = 1.0;
  return estimate;
}

/// @brief set the query id
void RemoteNode::queryId(QueryId queryId) {
  _queryId = arangodb::basics::StringUtils::itoa(queryId);
}

size_t RemoteNode::getMemoryUsedBytes() const { return sizeof(*this); }
