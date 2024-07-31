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

#include "ScatterNode.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNode/DistributeConsumerNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/ScatterExecutor.h"
#include "Aql/Query.h"
#include "Aql/types.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Iterator.h>

#include <string_view>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::aql;

/// @brief construct a scatter node
ScatterNode::ScatterNode(ExecutionPlan* plan, ExecutionNodeId id,
                         ScatterType type)
    : ExecutionNode(plan, id), _type(type) {}

ScatterNode::ScatterNode(ExecutionPlan* plan,
                         arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base) {
  readClientsFromVelocyPack(base);
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> ScatterNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  auto registerInfos = createRegisterInfos({}, {});
  auto executorInfos = ScatterExecutorInfos(_clients);

  return std::make_unique<ExecutionBlockImpl<ScatterExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* ScatterNode::clone(ExecutionPlan* plan,
                                  bool withDependencies) const {
  auto c = std::make_unique<ScatterNode>(plan, _id, getScatterType());
  c->copyClients(clients());
  return cloneHelper(std::move(c), withDependencies);
}

/// @brief doToVelocyPack, for ScatterNode
void ScatterNode::doToVelocyPack(VPackBuilder& nodes, unsigned flags) const {
  // serialize clients
  writeClientsToVelocyPack(nodes);
}

size_t ScatterNode::getMemoryUsedBytes() const { return sizeof(*this); }

bool ScatterNode::readClientsFromVelocyPack(VPackSlice base) {
  auto const clientsSlice = base.get("clients");

  if (!clientsSlice.isArray()) {
    LOG_TOPIC("49ba1", ERR, Logger::AQL)
        << "invalid serialized ScatterNode definition, 'clients' attribute is "
           "expected to be an array of string";
    return false;
  }

  size_t pos = 0;
  for (auto clientSlice : velocypack::ArrayIterator(clientsSlice)) {
    if (!clientSlice.isString()) {
      LOG_TOPIC("c6131", ERR, Logger::AQL)
          << "invalid serialized ScatterNode definition, 'clients' attribute "
             "is expected to be an array of string but got not a string at "
             "line "
          << pos;
      _clients.clear();  // clear malformed node
      return false;
    }

    _clients.emplace_back(clientSlice.copyString());
    ++pos;
  }

  _type = static_cast<ScatterNode::ScatterType>(
      basics::VelocyPackHelper::getNumericValue<uint64_t>(base, "scatterType",
                                                          0));

  return true;
}

void ScatterNode::writeClientsToVelocyPack(VPackBuilder& builder) const {
  builder.add("scatterType",
              VPackValue(static_cast<uint64_t>(getScatterType())));
  VPackArrayBuilder arrayScope(&builder, "clients");
  for (auto const& client : _clients) {
    builder.add(VPackValue(client));
  }
}

void ScatterNode::addClient(DistributeConsumerNode const& client) {
  auto const& distId = client.getDistributeId();
  // We cannot add the same distributeId twice, data is delivered exactly once
  // for each id
  TRI_ASSERT(std::find(_clients.begin(), _clients.end(), distId) ==
             _clients.end());
  _clients.emplace_back(distId);
}

/// @brief estimateCost
CostEstimate ScatterNode::estimateCost() const {
  CostEstimate estimate = _dependencies[0]->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems * _clients.size();
  return estimate;
}
