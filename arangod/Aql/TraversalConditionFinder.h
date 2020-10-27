////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_TRAVERSAL_CONDITION_FINDER_H
#define ARANGOD_AQL_TRAVERSAL_CONDITION_FINDER_H 1

#include "Aql/Condition.h"
#include "Aql/ExecutionNode.h"
#include "Aql/WalkerWorker.h"

namespace arangodb {
namespace aql {

/// @brief Traversal condition finder
class TraversalConditionFinder final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
 public:
  TraversalConditionFinder(ExecutionPlan* plan, bool* planAltered);

  ~TraversalConditionFinder() = default;

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
}  // namespace aql
}  // namespace arangodb

#endif
