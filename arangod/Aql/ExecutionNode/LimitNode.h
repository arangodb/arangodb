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

#include <memory>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

class Index;

namespace aql {
class Ast;
class ExecutionBlock;
class ExecutionEngine;
class ExecutionNode;
class ExecutionPlan;

/// @brief class LimitNode
class LimitNode : public ExecutionNode {
  friend class ExecutionBlock;

 public:
  LimitNode(ExecutionPlan* plan, ExecutionNodeId id, size_t offset,
            size_t limit);

  LimitNode(ExecutionPlan*, arangodb::velocypack::Slice base);

  /// @brief return the type of the node
  NodeType getType() const override final;

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  AsyncPrefetchEligibility canUseAsyncPrefetching()
      const noexcept override final;

  /// @brief tell the node to fully count what it will limit
  void setFullCount(bool enable = true);

  bool fullCount() const noexcept;

  /// @brief return the offset value
  size_t offset() const;

  /// @brief return the limit value
  size_t limit() const;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  /// @brief the offset
  size_t _offset;

  /// @brief the limit
  size_t _limit;

  /// @brief whether or not the node should fully count what it limits
  bool _fullCount;
};

}  // namespace aql
}  // namespace arangodb
