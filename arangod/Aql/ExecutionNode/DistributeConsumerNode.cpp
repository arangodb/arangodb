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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "DistributeConsumerNode.h"

#include "Aql/ExecutionBlock.h"
#include "Aql/Executor/IdExecutor.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/VelocyPackHelper.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

DistributeConsumerNode::DistributeConsumerNode(
    ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _distributeId(VelocyPackHelper::getStringValue(base, "distributeId", "")),
      _isResponsibleForInitializeCursor(
          base.get("isResponsibleForInitializeCursor").getBoolean()) {}

void DistributeConsumerNode::doToVelocyPack(VPackBuilder& nodes,
                                            unsigned flags) const {
  nodes.add("distributeId", VPackValue(_distributeId));
  nodes.add("isResponsibleForInitializeCursor",
            VPackValue(_isResponsibleForInitializeCursor));
}

std::unique_ptr<ExecutionBlock> DistributeConsumerNode::createBlock(
    ExecutionEngine& engine) const {
  TRI_ASSERT(hasDependency());
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);
  TRI_ASSERT(getRegisterPlan()->nrRegs[previousNode->getDepth()] ==
             getRegisterPlan()->nrRegs[getDepth()]);
  auto registerInfos = createRegisterInfos({}, {});
  auto executorInfos = IdExecutorInfos(false, RegisterId(0), _distributeId,
                                       _isResponsibleForInitializeCursor);
  return std::make_unique<ExecutionBlockImpl<
      IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* DistributeConsumerNode::clone(ExecutionPlan* plan,
                                             bool withDependencies) const {
  auto clone = cloneHelper(
      std::make_unique<DistributeConsumerNode>(plan, _id, getDistributeId()),
      withDependencies);

  static_cast<DistributeConsumerNode*>(clone)->isResponsibleForInitializeCursor(
      _isResponsibleForInitializeCursor);

  return clone;
}

size_t DistributeConsumerNode::getMemoryUsedBytes() const {
  return sizeof(*this);
}
