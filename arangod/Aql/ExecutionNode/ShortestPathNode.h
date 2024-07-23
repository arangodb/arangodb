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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode/GraphNode.h"
#include "Aql/Executor/ShortestPathExecutor.h"
#include "Graph/Options/TwoSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathValidatorOptions.h"

namespace arangodb {

namespace velocypack {
class Builder;
}

namespace graph {
struct BaseOptions;
struct ShortestPathOptions;
struct IndexAccessor;
}  // namespace graph
namespace aql {

/// @brief class ShortestPathNode
class ShortestPathNode : public virtual GraphNode {
  friend class ExecutionBlock;

  /// @brief constructor with a vocbase and a collection name
 protected:
  /// @brief Clone constructor, used for constructors of derived classes.
  /// Does not clone recursively, does not clone properties (`other.plan()` is
  /// expected to be the same as `plan)`, and does not register this node in the
  /// plan.
  ShortestPathNode(ExecutionPlan& plan, ShortestPathNode const& node);

 public:
  ShortestPathNode(ExecutionPlan* plan, ExecutionNodeId id,
                   TRI_vocbase_t* vocbase, AstNode const* direction,
                   AstNode const* start, AstNode const* target,
                   AstNode const* graph,
                   std::unique_ptr<graph::BaseOptions> options);

  ShortestPathNode(ExecutionPlan* plan,
                   arangodb::velocypack::Slice const& base);

  ~ShortestPathNode();

  /// @brief Internal constructor to clone the node.
  ShortestPathNode(
      ExecutionPlan* plan, ExecutionNodeId id, TRI_vocbase_t* vocbase,
      std::vector<Collection*> const& edgeColls,
      std::vector<Collection*> const& vertexColls,
      TRI_edge_direction_e defaultDirection,
      std::vector<TRI_edge_direction_e> const& directions,
      Variable const* inStartVariable, std::string const& startVertexId,
      Variable const* inTargetVariable, std::string const& targetVertexId,
      std::unique_ptr<graph::BaseOptions> options, graph::Graph const* graph,
      Variable const* distributeVariable);

 public:
  /// @brief return the type of the node
  NodeType getType() const override final { return SHORTEST_PATH; }

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override;

  /// @brief Test if this node uses an in variable or constant for start
  bool usesStartInVariable() const { return _inStartVariable != nullptr; }

  /// @brief return the start variable
  Variable const* startInVariable() const { return _inStartVariable; }

  std::string const getStartVertex() const { return _startVertexId; }

  void setStartInVariable(Variable const* inVariable);
  void setTargetInVariable(Variable const* inVariable);
  void setDistributeVariable(Variable const* distVariable);

  /// @brief Test if this node uses an in variable or constant for target
  bool usesTargetInVariable() const { return _inTargetVariable != nullptr; }

  /// @brief return the target variable
  Variable const* targetInVariable() const { return _inTargetVariable; }

  std::string const getTargetVertex() const { return _targetVertexId; }

  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  void replaceAttributeAccess(ExecutionNode const* self,
                              Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              Variable const* replaceVariable,
                              size_t index) override;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override;

  /// @brief Compute the shortest path options containing the expressions
  ///        MUST! be called after optimization and before creation
  ///        of blocks.
  void prepareOptions() override;

  /// @brief Overrides GraphNode::options() with a more specific return type
  ///  (casts graph::BaseOptions* into graph::ShortestPathOptions*)
  auto options() const -> graph::ShortestPathOptions*;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

  std::vector<arangodb::graph::IndexAccessor> buildUsedIndexes() const;

  std::vector<arangodb::graph::IndexAccessor> buildReverseUsedIndexes() const;

 private:
  std::vector<arangodb::graph::IndexAccessor> buildIndexes(bool reverse) const;

  void shortestPathCloneHelper(ExecutionPlan& plan, ShortestPathNode& c) const;

  /// @brief input variable only used if _vertexId is unused
  Variable const* _inStartVariable;

  /// @brief input vertexId only used if _inVariable is unused
  std::string _startVertexId;

  /// @brief input variable only used if _vertexId is unused
  Variable const* _inTargetVariable;

  /// @brief input vertexId only used if _inVariable is unused
  std::string _targetVertexId;

  /// @brief The hard coded condition on _from
  AstNode* _fromCondition;

  /// @brief The hard coded condition on _to
  AstNode* _toCondition;

  /// @brief Variable that contains the value on which the distribution is
  /// determined. This is required for hybrid disjoint smart graphs
  /// Note: The variable is not really used here, but we need to make sure
  /// it is retained up to this point node for the query instantiation,
  /// otherwise it will be discarded on coordinator (mapped to server), but the
  /// DBServer cannot map the Vertex to its shard.
  Variable const* _distributeVariable;

  RegIdSet buildVariableInformation() const;

  template<typename ShortestPathEnumeratorType>
  std::pair<RegIdSet, typename ShortestPathExecutorInfos<
                          ShortestPathEnumeratorType>::RegisterMapping>
  _buildOutputRegisters() const;

  template<typename ShortestPathRefactored, typename Provider,
           typename ProviderOptions>
  std::unique_ptr<ExecutionBlock> makeExecutionBlockImpl(
      graph::ShortestPathOptions* opts, ProviderOptions forwardProviderOptions,
      ProviderOptions backwardProviderOptions,
      arangodb::graph::TwoSidedEnumeratorOptions enumeratorOptions,
      arangodb::graph::PathValidatorOptions validatorOptions,
      typename ShortestPathExecutorInfos<
          ShortestPathRefactored>::RegisterMapping&& outputRegister,
      ExecutionEngine& engine, InputVertex sourceInput, InputVertex targetInput,
      RegisterInfos registerInfos) const;
};

}  // namespace aql
}  // namespace arangodb
