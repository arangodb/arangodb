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

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/WalkerWorker.h"
#include "Aql/types.h"
#include "Containers/HashSet.h"

#include <memory>

namespace arangodb::aql {
class Condition;
class ExecutionPlan;

/// @brief Traversal condition finder
class TraversalConditionFinder final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
 public:
  TraversalConditionFinder(ExecutionPlan* plan, bool* planAltered);

  ~TraversalConditionFinder();

  bool before(ExecutionNode*) override final;

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final;

 private:
  bool isTrueOnNull(AstNode* condition, Variable const* pathVar) const;

 private:
  ExecutionPlan* _plan;
  std::unique_ptr<Condition> _condition;
  ::arangodb::containers::HashSet<VariableId> _filterVariables;
  bool* _planAltered;
};

}  // namespace arangodb::aql
