////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/AstNode.h"
#include "Aql/EdgeConditionBuilder.h"
#include "Aql/Variable.h"
#include "Aql/VariableGenerator.h"

#include "Basics/ResourceUsage.h"

namespace arangodb {

namespace aql {
struct AstNode;

// Wrapper around EdgeConditionBuilder that takes responsibility for all
// AstNodes created with it. Can be used outside of an AQL query.
class EdgeConditionBuilderContainer final : public EdgeConditionBuilder {
 public:
  EdgeConditionBuilderContainer(arangodb::ResourceMonitor& resourceMonitor);

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
