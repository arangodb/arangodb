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

#include "Aql/VariableGenerator.h"

#include <span>
#include <unordered_map>

namespace arangodb {

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
struct AstNode;
struct Variable;

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

  /// @brief indicator if we have attached the _from or _to condition to
  /// _modCondition
  bool _containsCondition;

  EdgeConditionBuilder(Ast*, EdgeConditionBuilder const&);
  explicit EdgeConditionBuilder(AstNode*,
                                arangodb::ResourceMonitor& resourceMonitor);

  // Create the _fromCondition for the first time.
  virtual void buildFromCondition() = 0;

  // Create the _toCondition for the first time.
  virtual void buildToCondition() = 0;

 public:
  virtual ~EdgeConditionBuilder() = default;

  EdgeConditionBuilder(EdgeConditionBuilder const&) = delete;
  EdgeConditionBuilder(EdgeConditionBuilder&&) = delete;

  void replaceVariables(
      std::unordered_map<VariableId, Variable const*> const& replacements);

  void replaceAttributeAccess(Ast* ast, Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              Variable const* replaceVariable);

  // Add a condition on the edges that is not related to
  // the direction e.g. `label == foo`
  void addConditionPart(AstNode const*);

  // Get the complete condition for outbound edges
  AstNode* getOutboundCondition();

  // Get the complete condition for inbound edges
  AstNode* getInboundCondition();

  // Get the resource monitor
  arangodb::ResourceMonitor& resourceMonitor();

 private:
  // Internal helper to swap _from and _to parts
  void swapSides(AstNode* condition);

  arangodb::ResourceMonitor& _resourceMonitor;
};

}  // namespace aql
}  // namespace arangodb
