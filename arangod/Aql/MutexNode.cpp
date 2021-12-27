////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "MutexNode.h"

#include "Aql/DistributeConsumerNode.h"
#include "Aql/ExecutionLocation.h"
#include "Aql/MutexExecutor.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::aql;

MutexNode::MutexNode(ExecutionPlan* plan, ExecutionNodeId id)
    : ExecutionNode(plan, id) {}

MutexNode::MutexNode(ExecutionPlan* plan,
                     arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base) {
  auto const clientsSlice = base.get("clients");
  if (clientsSlice.isArray()) {
    for (auto const clientSlice : velocypack::ArrayIterator(clientsSlice)) {
      if (clientSlice.isString()) {
        _clients.emplace_back(clientSlice.copyString());
      }
    }
  }
}

ExecutionLocation MutexNode::getAllowedLocation() const {
  return ExecutionLocation(ExecutionLocation::LocationType::ANYWHERE);
}

ExecutionNode* MutexNode::clone(ExecutionPlan* plan, bool withDependencies,
                                bool withProperties) const {
  return cloneHelper(std::make_unique<MutexNode>(plan, _id), withDependencies,
                     withProperties);
}

/// @brief doToVelocyPack, for MutexNode
void MutexNode::doToVelocyPack(VPackBuilder& nodes, unsigned flags) const {
  VPackArrayBuilder arrayScope(&nodes, "clients");
  for (auto const& client : _clients) {
    nodes.add(VPackValue(client));
  }
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> MutexNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto registerInfos = createRegisterInfos({}, {});
  return std::make_unique<ExecutionBlockImpl<MutexExecutor>>(
      &engine, this, std::move(registerInfos), MutexExecutorInfos(_clients));
}

/// @brief estimateCost, the cost of a NoResults is nearly 0
CostEstimate MutexNode::estimateCost() const {
  if (_dependencies.empty()) {
    return aql::CostEstimate::empty();
  }
  CostEstimate estimate = CostEstimate::empty();
  return estimate;
}

void MutexNode::addClient(DistributeConsumerNode const* client) {
  auto const& distId = client->getDistributeId();
  // We cannot add the same distributeId twice, data is delivered exactly once
  // for each id
  TRI_ASSERT(std::find(_clients.begin(), _clients.end(), distId) ==
             _clients.end());
  _clients.emplace_back(distId);
}
