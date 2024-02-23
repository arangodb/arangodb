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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"

namespace arangodb {
namespace aql {

struct Variable;

class SubqueryStartNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;

 public:
  SubqueryStartNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);
  SubqueryStartNode(ExecutionPlan* plan, ExecutionNodeId id,
                    Variable const* subqueryOutVariable)
      : ExecutionNode(plan, id), _subqueryOutVariable(subqueryOutVariable) {}

  CostEstimate estimateCost() const override final;

  NodeType getType() const override final { return SUBQUERY_START; }

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

  bool isEqualTo(ExecutionNode const& other) const override final;

 protected:
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  /// @brief This is only required for Explain output.
  ///        it has no practical usage other then to print this information
  ///        during explain.
  Variable const* _subqueryOutVariable;
};

}  // namespace aql
}  // namespace arangodb
