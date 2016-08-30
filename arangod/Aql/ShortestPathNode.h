////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SHORTEST_PATH_NODE_H
#define ARANGOD_AQL_SHORTEST_PATH_NODE_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/Graphs.h"
#include "Aql/ShortestPathOptions.h"

#include <velocypack/Builder.h>

namespace arangodb {

namespace traverser {
struct ShortestPathOptions;
}
namespace aql {

/// @brief class ShortestPathNode
class ShortestPathNode : public ExecutionNode {
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;
  friend class ShortestPathBlock;

  /// @brief constructor with a vocbase and a collection name
 public:
  ShortestPathNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
                uint64_t direction, AstNode const* start, AstNode const* target,
                AstNode const* graph, ShortestPathOptions const& options);

  ShortestPathNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base);

  ~ShortestPathNode() {}

  /// @brief Internal constructor to clone the node.
 private:
  ShortestPathNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
                   std::vector<std::string> const& edgeColls,
                   std::vector<TRI_edge_direction_e> const& directions,
                   Variable const* inStartVariable,
                   std::string const& startVertexId,
                   Variable const* inTargetVariable,
                   std::string const& targetVertexId,
                   ShortestPathOptions const& options);

 public:
  /// @brief return the type of the node
  NodeType getType() const override final { return SHORTEST_PATH; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&,
                          bool) const override final;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief the cost of a traversal node
  double estimateCost(size_t&) const override final;

  /// @brief Test if this node uses an in variable or constant for start
  bool usesStartInVariable() const {
    return _inStartVariable != nullptr;
  }

  /// @brief return the start variable
  Variable const* startInVariable() const { return _inStartVariable; }

  std::string const getStartVertex() const { return _startVertexId; }

  /// @brief Test if this node uses an in variable or constant for target
  bool usesTargetInVariable() const {
    return _inTargetVariable != nullptr;
  }

  /// @brief return the target variable
  Variable const* targetInVariable() const { return _inTargetVariable; }

  std::string const getTargetVertex() const { return _targetVertexId; }

  /// @brief return the vertex out variable
  Variable const* vertexOutVariable() const { return _vertexOutVariable; }

  /// @brief checks if the vertex out variable is used
  bool usesVertexOutVariable() const { return _vertexOutVariable != nullptr; }

  /// @brief set the vertex out variable
  void setVertexOutput(Variable const* outVar) { _vertexOutVariable = outVar; }

  /// @brief return the edge out variable
  Variable const* edgeOutVariable() const { return _edgeOutVariable; }

  /// @brief checks if the edge out variable is used
  bool usesEdgeOutVariable() const { return _edgeOutVariable != nullptr; }

  /// @brief set the edge out variable
  void setEdgeOutput(Variable const* outVar) { _edgeOutVariable = outVar; }

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final {
    std::vector<Variable const*> vars;
    if (_vertexOutVariable != nullptr) {
      vars.emplace_back(_vertexOutVariable);
    }
    if (_edgeOutVariable != nullptr) {
      vars.emplace_back(_edgeOutVariable);
    }
    return vars;
  }

  /// @brief getVariablesUsedHere, returning a vector
  std::vector<Variable const*> getVariablesUsedHere() const override {
    std::vector<Variable const*> vars;
    if (_inStartVariable != nullptr) {
      vars.emplace_back(_inStartVariable);
    }
    if (_inTargetVariable != nullptr) {
      vars.emplace_back(_inTargetVariable);
    }
    return vars;
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(
      std::unordered_set<Variable const*>& vars) const override {
    if (_inStartVariable != nullptr) {
      vars.emplace(_inStartVariable);
    }
    if (_inTargetVariable != nullptr) {
      vars.emplace(_inTargetVariable);
    }
  }

  void fillOptions(arangodb::traverser::ShortestPathOptions&) const;

 private:

  /// @brief the database
  TRI_vocbase_t* _vocbase;

  /// @brief vertex output variable
  Variable const* _vertexOutVariable;

  /// @brief vertex output variable
  Variable const* _edgeOutVariable;

  /// @brief input variable only used if _vertexId is unused
  Variable const* _inStartVariable;

  /// @brief input vertexId only used if _inVariable is unused
  std::string _startVertexId;

  /// @brief input variable only used if _vertexId is unused
  Variable const* _inTargetVariable;

  /// @brief input vertexId only used if _inVariable is unused
  std::string _targetVertexId;

  /// @brief input graphInfo only used for serialisation & info
  arangodb::velocypack::Builder _graphInfo;

  /// @brief The directions edges are followed
  std::vector<TRI_edge_direction_e> _directions;

  /// @brief the edge collection names
  std::vector<std::string> _edgeColls;

  /// @brief our graph...
  Graph const* _graphObj;

  /// @brief Options for traversals
  ShortestPathOptions _options;
};

} // namespace arangodb::aql
} // namespace arangodb

#endif
