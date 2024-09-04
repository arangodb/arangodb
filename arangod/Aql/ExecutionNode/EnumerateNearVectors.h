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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Transaction/Methods.h"

#include <memory>

namespace arangodb {
namespace aql {
class ExecutionBlock;
class ExecutionEngine;
class ExecutionNode;
class ExecutionPlan;
class Expression;

/// @brief class EnumerateNearVectors
class EnumerateNearVectors : public ExecutionNode {
 public:
  EnumerateNearVectors(ExecutionPlan* plan, ExecutionNodeId id,
                       Variable const* inVariable,
                       Variable const* documentOutVariable,
                       Variable const* distanceOutVariable, std::size_t limit);

  EnumerateNearVectors(ExecutionPlan*, arangodb::velocypack::Slice base);

  NodeType getType() const override;

  size_t getMemoryUsedBytes() const override;

  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override;
  void replaceVariables(const std::unordered_map<VariableId, const Variable*>&
                            replacements) override;
  void getVariablesUsedHere(VarSet& vars) const override;
  std::vector<const Variable*> getVariablesSetHere() const override;

  Variable const* inVariable() const { return _inVariable; }
  Variable const* documentOutVariable() const { return _documentOutVariable; }
  Variable const* distanceOutVariable() const { return _distanceOutVariable; }

 protected:
  CostEstimate estimateCost() const override;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  /// @brief input variable to read the query point from
  Variable const* _inVariable;

  /// @brief document id and distance out variables
  Variable const* _documentOutVariable;
  Variable const* _distanceOutVariable;

  /// @brief contains the limit, this node only produces the top k results
  std::size_t _limit;

  /// @brief
  transaction::Methods::IndexHandle _vectorIndex;
};
}  // namespace aql
}  // namespace arangodb
