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

#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/ExecutionNode/MaterializeNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/Projections.h"

#include <memory>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
struct Collection;
class ExecutionBlock;
class ExecutionEngine;
class ExecutionNode;
class ExecutionPlan;
struct Variable;

namespace materialize {

class MaterializeRocksDBNode : public MaterializeNode,
                               public CollectionAccessingNode {
 public:
  MaterializeRocksDBNode(ExecutionPlan* plan, ExecutionNodeId id,
                         aql::Collection const* collection,
                         aql::Variable const& inDocId,
                         aql::Variable const& outVariable,
                         aql::Variable const& oldDocVariable);

  MaterializeRocksDBNode(ExecutionPlan* plan, arangodb::velocypack::Slice base);

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override final;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  bool alwaysCopiesRows() const override { return false; }
  bool isIncreaseDepth() const override { return false; }

  std::vector<Variable const*> getVariablesSetHere() const override final;

  void projections(Projections proj) noexcept {
    _projections = std::move(proj);
  }
  Projections& projections() noexcept { return _projections; }
  Projections const& projections() const noexcept { return _projections; }

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder& nodes,
                      unsigned flags) const override final;

  Projections _projections;
};

}  // namespace materialize
}  // namespace aql
}  // namespace arangodb
