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
/// @author Lars Maier
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Indexes/VectorIndexDefinition.h"
#include "Transaction/Methods.h"

#include <memory>

namespace arangodb::aql {
class ExecutionBlock;
class ExecutionEngine;
class ExecutionNode;
class ExecutionPlan;
class Expression;

/// @brief class EnumerateNearVectorNode
class EnumerateNearVectorNode : public ExecutionNode,
                                public CollectionAccessingNode {
 public:
  EnumerateNearVectorNode(ExecutionPlan* plan, ExecutionNodeId id,
                          Variable const* inVariable,
                          Variable const* oldDocumentVariable,
                          Variable const* documentOutVariable,
                          Variable const* distanceOutVariable,
                          std::size_t limit, bool ascending, std::size_t offset,
                          SearchParameters searchParameters,
                          aql::Collection const* collection,
                          transaction::Methods::IndexHandle indexHandle,
                          std::unique_ptr<Expression> filterExpression,
                          bool isCoveredByStoredValues);

  EnumerateNearVectorNode(ExecutionPlan*, arangodb::velocypack::Slice base);

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
  Variable const* oldDocumentVariable() const { return _oldDocumentVariable; }
  Variable const* documentOutVariable() const { return _documentOutVariable; }
  Variable const* distanceOutVariable() const { return _distanceOutVariable; }

  transaction::Methods::IndexHandle const& index() const { return _index; }

  bool isAscending() const noexcept;

  void setFilterExpression(Expression* filterExpression);

  void setIsCoveredByStoredValues(bool isCoveredByStoredValues) noexcept;

 protected:
  CostEstimate estimateCost() const override;

  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder& builder,
                      unsigned flags) const final;

  std::vector<std::pair<VariableId, RegisterId>> extractFilterVarsToRegs()
      const;

 private:
  /// @brief input variable to read the query point from
  Variable const* _inVariable;

  /// @brief old document variable, only used for book keeping
  Variable const* _oldDocumentVariable;

  /// @brief document id and distance out variables
  Variable const* _documentOutVariable;
  Variable const* _distanceOutVariable;

  /// @brief contains the limit, this node only produces the top k results
  std::size_t _limit;

  // @brief specifies which order is set for enumerate
  std::size_t _ascending;

  /// @brief contains the offset, this skips offset number of results
  std::size_t _offset;

  /// @brief contains search parameters used by faiss
  SearchParameters _searchParameters;

  /// @brief selected index for vector search
  /// guaranteed to always be a vector index
  transaction::Methods::IndexHandle _index;

  /// @brief filter expression if filter was pushed down into this node
  std::unique_ptr<Expression> _filterExpression;

  /// @brief indicates if the filter expression is fully covered by stored
  /// values
  bool _isCoveredByStoredValues;

  /// @brief filterVarToRegs is set in optimization rule
  std::vector<std::pair<VariableId, RegisterId>> _filterVarToRegs;
};
}  // namespace arangodb::aql
