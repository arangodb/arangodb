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
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Aql/ExecutionNode/ExecutionNode.h"

#include "velocypack/Slice.h"

namespace arangodb::aql::enumerate_neighbours {

struct ExecutionNode : public ::arangodb::aql::ExecutionNode {
 public:
  ExecutionNode(ExecutionPlan* plan, ExecutionNodeId id);
  ExecutionNode(ExecutionPlan* plan, arangodb::velocypack::Slice base);
  ~ExecutionNode();

  /// @brief return the type of the node
  NodeType getType() const override final {
    return ::arangodb::aql::ExecutionNode::NodeType::ENUMERATE_NEIGHBOURS;
  };

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ::arangodb::aql::ExecutionNode* clone(
      ExecutionPlan* plan, bool withDependencies) const override final;

  size_t getMemoryUsedBytes() const override { return 0; }
  void doToVelocyPack(velocypack::Builder&, unsigned flags) const override {}
  CostEstimate estimateCost() const override { return CostEstimate(0.0, 1000); }
};

}  // namespace arangodb::aql::enumerate_neighbours
