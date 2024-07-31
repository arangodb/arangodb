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

#pragma once

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/Variable.h"
#include "Aql/types.h"

#include <memory>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
namespace aql {
class DistributeConsumerNode;
class ExecutionBlock;
class ExecutionEngine;
class ExecutionPlan;
struct Collection;

/// @brief class ScatterNode
class ScatterNode : public ExecutionNode {
 public:
  enum ScatterType { SERVER = 0, SHARD = 1 };

  /// @brief constructor with an id
  ScatterNode(ExecutionPlan* plan, ExecutionNodeId id, ScatterType type);

  ScatterNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override { return SCATTER; }

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override;

  /// @brief estimateCost
  CostEstimate estimateCost() const override;

  void addClient(DistributeConsumerNode const& client);

  /// @brief drop the current clients.
  void clearClients() { _clients.clear(); }

  std::vector<std::string> const& clients() const noexcept { return _clients; }

  /// @brief get the scatter type, this defines the variation on how we
  /// distribute the data onto our clients.
  ScatterType getScatterType() const { return _type; };

  void setScatterType(ScatterType targetType) { _type = targetType; }

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override;

  void writeClientsToVelocyPack(velocypack::Builder& builder) const;
  bool readClientsFromVelocyPack(velocypack::Slice base);

  void copyClients(std::vector<std::string> const& other) {
    TRI_ASSERT(_clients.empty());
    _clients = other;
  }

 private:
  std::vector<std::string> _clients;

  ScatterType _type;
};

}  // namespace aql
}  // namespace arangodb
