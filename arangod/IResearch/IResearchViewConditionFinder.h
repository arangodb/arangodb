////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_CONDITION_FINDER_H
#define ARANGOD_IRESEARCH__IRESEARCH_CONDITION_FINDER_H 1

#include "Aql/Condition.h"
#include "Aql/ExecutionNode.h"
#include "Aql/WalkerWorker.h"

namespace arangodb {

namespace aql {
struct Variable;
} // aql

namespace iresearch {

/// @brief condition finder
class IResearchViewConditionFinder final : public aql::WalkerWorker<aql::ExecutionNode> {
 public:
  IResearchViewConditionFinder(
      aql::ExecutionPlan* plan,
      std::unordered_map<size_t, aql::ExecutionNode*>* changes,
      bool* hasEmptyResult)
   : _plan(plan),
     _variableDefinitions(),
     _filters(),
     _sorts(),
     _changes(changes),
     _hasEmptyResult(hasEmptyResult) {
  };

  bool before(aql::ExecutionNode*) override;

  bool enterSubquery(aql::ExecutionNode*, aql::ExecutionNode*) override {
    return false;
  }

 protected:
  bool handleFilterCondition(
    aql::ExecutionNode* en,
    std::unique_ptr<aql::Condition>& condition
  );

  void handleSortCondition(
    aql::ExecutionNode* en,
    aql::Variable const* outVar,
    std::unique_ptr<aql::Condition>& condition,
    std::unique_ptr<aql::SortCondition>& sortCondition
  );

 private:
  aql::ExecutionPlan* _plan;
  std::unordered_map<aql::VariableId, aql::AstNode const*> _variableDefinitions;
  std::unordered_set<aql::VariableId> _filters;
  std::vector<std::pair<aql::Variable const*, bool>> _sorts;
  // note: this class will never free the contents of this map
  std::unordered_map<size_t, aql::ExecutionNode*>* _changes;
  bool* _hasEmptyResult;
}; // IResearchViewConditionFinder

}  // iresearch
}  // arangodb

#endif // ARANGOD_IRESEARCH__IRESEARCH_CONDITION_FINDER_H
