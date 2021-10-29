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

#pragma once

#include "Aql/ExecutionNode.h"

namespace arangodb {
namespace aql {

class DistributeConsumerNode;
class ExecutionLocation;

/// @brief class MutexNode
class MutexNode : public ExecutionNode {
  friend class ExecutionBlock;

  /// @brief constructor with an id
 public:
  MutexNode(ExecutionPlan* plan, ExecutionNodeId id);

  MutexNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return MUTEX; }
  
  [[nodiscard]] ExecutionLocation getAllowedLocation() const override;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief the cost of a AsyncNode is whatever is 0
  CostEstimate estimateCost() const override final;

  void addClient(DistributeConsumerNode const* client);

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&, unsigned flags) const override final;

 private:
  std::vector<std::string> _clients;
};

}
}

