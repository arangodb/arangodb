////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/GraphNode.h"
#include "Aql/Graphs.h"
#include "Graph/ShortestPathType.h"

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

/// @brief class KShortestPathsNode
class KShortestPathsNode : public virtual GraphNode {
  friend class ExecutionBlock;

  /// @brief constructor with a vocbase and a collection name
 protected:
  /// @brief Clone constructor, used for constructors of derived classes.
  /// Does not clone recursively, does not clone properties (`other.plan()` is
  /// expected to be the same as `plan)`, and does not register this node in the
  /// plan.
  KShortestPathsNode(ExecutionPlan& plan, KShortestPathsNode const& node);

 public:
  KShortestPathsNode(ExecutionPlan* plan, ExecutionNodeId id,
                     TRI_vocbase_t* vocbase,
                     arangodb::graph::ShortestPathType::Type shortestPathType,
                     AstNode const* direction, AstNode const* start,
                     AstNode const* target, AstNode const* graph,
                     std::unique_ptr<graph::BaseOptions> options);

  KShortestPathsNode(ExecutionPlan* plan,
                     arangodb::velocypack::Slice const& base);

  ~KShortestPathsNode();

  /// @brief Internal constructor to clone the node.
  KShortestPathsNode(
      ExecutionPlan* plan, ExecutionNodeId id, TRI_vocbase_t* vocbase,
      arangodb::graph::ShortestPathType::Type shortestPathType,
      std::vector<Collection*> const& edgeColls,
      std::vector<Collection*> const& vertexColls,
      TRI_edge_direction_e defaultDirection,
      std::vector<TRI_edge_direction_e> const& directions,
      Variable const* inStartVariable, std::string const& startVertexId,
      Variable const* inTargetVariable, std::string const& targetVertexId,
      std::unique_ptr<graph::BaseOptions> options, graph::Graph const* graph);

 public:
  /// @brief return the type of the node
  NodeType getType() const override final { return K_SHORTEST_PATHS; }

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&)
      const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override;

  bool usesPathOutVariable() const { return _pathOutVariable != nullptr; }
  Variable const& pathOutVariable() const {
    TRI_ASSERT(_pathOutVariable != nullptr);
    return *_pathOutVariable;
  }

  void setPathOutput(Variable const* outVar) { _pathOutVariable = outVar; }

  /// @brief Test if this node uses an in variable or constant for start
  bool usesStartInVariable() const { return _inStartVariable != nullptr; }

  /// @brief return the start variable
  Variable const& startInVariable() const {
    TRI_ASSERT(_inStartVariable != nullptr);
    return *_inStartVariable;
  }

  void setStartInVariable(Variable const* inVariable);

  std::string const getStartVertex() const { return _startVertexId; }

  /// @brief Test if this node uses an in variable or constant for target
  bool usesTargetInVariable() const { return _inTargetVariable != nullptr; }

  /// @brief return the target variable
  Variable const& targetInVariable() const {
    TRI_ASSERT(_inTargetVariable != nullptr);
    return *_inTargetVariable;
  }

  std::string const getTargetVertex() const { return _targetVertexId; }

  void replaceVariables(std::unordered_map<VariableId, Variable const*> const&
                            replacements) override;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override;

  /// @brief algorithm type (K_SHORTEST_PATHS or K_PATHS)
  arangodb::graph::ShortestPathType::Type shortestPathType() const {
    return _shortestPathType;
  }

  /// @brief Compute the shortest path options containing the expressions
  ///        MUST! be called after optimization and before creation
  ///        of blocks.
  void prepareOptions() override;

  std::vector<arangodb::graph::IndexAccessor> buildUsedIndexes() const;

  std::vector<arangodb::graph::IndexAccessor> buildReverseUsedIndexes() const;

  /// @brief Overrides GraphNode::options() with a more specific return type
  ///  (casts graph::BaseOptions* into graph::ShortestPathOptions*)
  auto options() const -> graph::ShortestPathOptions*;

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  void kShortestPathsCloneHelper(ExecutionPlan& plan, KShortestPathsNode& c,
                                 bool withProperties) const;

 private:
  /// @brief algorithm type (K_SHORTEST_PATHS or K_PATHS)
  arangodb::graph::ShortestPathType::Type _shortestPathType;

  /// @brief path output variable
  Variable const* _pathOutVariable;

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
};

}  // namespace aql
}  // namespace arangodb
