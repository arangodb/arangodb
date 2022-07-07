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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/VariableGenerator.h"
#include "VocBase/voc-types.h"

namespace arangodb {

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace graph {
struct IndexAccessor;
}

namespace aql {
struct AstNode;
struct Variable;
class ExecutionPlan;
struct VarInfo;

// Helper class to generate AQL AstNode conditions
// that can be handed over to Indexes in order to
// query the data.
// This class does NOT take responsibility
// of the referenced AstNodes, the creater
// has to free them.
// In AQL the AST is responsible to free all nodes.
class EdgeConditionBuilder {
 protected:
  /// @brief a condition checking for _from
  /// not used directly
  AstNode* _fromCondition;

  /// @brief a condition checking for _to
  /// not used directly
  AstNode* _toCondition;

  /// @brief the temporary condition to ask indexes
  /// Is always in Normalized format: One n-ary-and
  /// branch of a DNF => No OR contained.
  AstNode* _modCondition;

  std::unordered_map<uint64_t, std::vector<AstNode const*>> _depthConditions;

  /// @brief indicator if we have attached the _from or _to condition to
  /// _modCondition
  bool _containsCondition;

  EdgeConditionBuilder(Ast*, EdgeConditionBuilder const&);
  explicit EdgeConditionBuilder(AstNode*);

  // Create the _fromCondition for the first time.
  virtual void buildFromCondition(){};

  // Create the _toCondition for the first time.
  virtual void buildToCondition(){};

 public:
  /// @brief Prepares an edge condition builder
  /// this builder just contains the basic
  /// from and to conditions, referencing the given variable as Edge
  /// The exchangeableIdNode will contain the vertex _id value to
  /// search for.
  explicit EdgeConditionBuilder(Ast* ast, Variable const* variable,
                                AstNode const* exchangeableIdNode);

  virtual ~EdgeConditionBuilder() = default;

  EdgeConditionBuilder(EdgeConditionBuilder const&) = delete;
  EdgeConditionBuilder(EdgeConditionBuilder&&) = delete;

  // Add a condition on the edges that is not related to
  // the direction e.g. `label == foo`
  void addConditionPart(AstNode const*);

  // Add a condition on the edges that specific to the given depth
  void addConditionForDepth(AstNode const*, uint64_t depth);

  // Get the complete condition for outbound edges
  AstNode* getOutboundCondition();

  // Get the complete condition for inbound edges
  AstNode* getInboundCondition();

  // Get the complete condition for outbound edges
  // Note: Caller will get a clone, and is allowed to modify it
  AstNode* getOutboundCondition(Ast* ast) const;

  // Get the complete condition for inbound edges
  // Note: Caller will get a clone, and is allowed to modify it
  AstNode* getInboundCondition(Ast* ast) const;

  // Get the complete condition for outbound edges
  // Note: Caller will get a clone, and is allowed to modify it
  AstNode* getOutboundConditionForDepth(uint64_t depth, Ast* ast) const;

  // Get the complete condition for inbound edges
  // Note: Caller will get a clone, and is allowed to modify it
  AstNode* getInboundConditionForDepth(uint64_t depth, Ast* ast) const;

  std::pair<
      std::vector<arangodb::graph::IndexAccessor>,
      std::unordered_map<uint64_t, std::vector<arangodb::graph::IndexAccessor>>>
  buildIndexAccessors(
      ExecutionPlan const* plan, Variable const* tmpVar,
      std::unordered_map<VariableId, VarInfo> const& varInfo,
      std::vector<std::pair<aql::Collection*, TRI_edge_direction_e>> const&
          collections) const;

 private:
  // Internal helper to swap _from and _to parts
  void swapSides(AstNode* condition);
};

// Wrapper around EdgeConditionBuilder that takes responsibility for all
// AstNodes created with it. Can be used outside of an AQL query.
class EdgeConditionBuilderContainer final : public EdgeConditionBuilder {
 public:
  EdgeConditionBuilderContainer();

  ~EdgeConditionBuilderContainer() override;

  // Get a pointer to the used variable
  Variable const* getVariable() const;

  // Set the id of the searched vertex
  // NOTE: This class does not take responsiblity for the string.
  // So caller needs to make sure it does not run out of scope
  // as long as these conditions are used.
  void setVertexId(std::string const&);

 protected:
  // Create the _fromCondition for the first time.
  void buildFromCondition() override;

  // Create the _toCondition for the first time.
  void buildToCondition() override;

 private:
  // Create the equality node using the given access
  AstNode* createEqCheck(AstNode const* access);

  // Create a node with access of attr on the variable
  AstNode* createAttributeAccess(std::string const& attr);

 private:
  // List of AstNodes this container is responsible for
  std::vector<AstNode*> _astNodes;

  // The variable node that is used to hold the edge
  AstNode* _varNode;

  // The value the edge is compared to
  AstNode* _compareNode;

  // Reference to the exchangeable variable node
  Variable* _var;

  // Reference to the VariableGenerator
  VariableGenerator _varGen;
};

}  // namespace aql
}  // namespace arangodb
