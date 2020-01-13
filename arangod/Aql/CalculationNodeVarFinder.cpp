////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#include "Aql/CalculationNodeVarFinder.h"

namespace arangodb {
namespace aql {

CalculationNodeVarFinder::CalculationNodeVarFinder(
    Variable const* lookingFor,
    ::arangodb::containers::SmallVector<ExecutionNode*>& out) noexcept
    : _lookingFor(lookingFor), _out(out) {}

bool CalculationNodeVarFinder::before(ExecutionNode* en) {
  auto type = en->getType();
  // will enter a subquery anyway later
  if (ExecutionNode::SUBQUERY == type) {
    TRI_ASSERT(enterSubquery(nullptr, nullptr));
    return false;
  }
  _currentUsedVars.clear();
  en->getVariablesUsedHere(_currentUsedVars);
  if (_currentUsedVars.find(_lookingFor) != _currentUsedVars.end()) {
    if (ExecutionNode::CALCULATION != type) {
      _out.clear();
      return true;
    }
    _out.emplace_back(en);
  }

  return false;
}

CalculationNodeVarExistenceFinder::CalculationNodeVarExistenceFinder(
    Variable const* lookingFor) noexcept
    : _lookingFor(lookingFor), _isCalculationNodesFound(false) {}

bool CalculationNodeVarExistenceFinder::before(ExecutionNode* en) {
  auto type = en->getType();
  // will enter a subquery anyway later
  if (ExecutionNode::SUBQUERY == type) {
    TRI_ASSERT(enterSubquery(nullptr, nullptr));
    return false;
  }
  _currentUsedVars.clear();
  en->getVariablesUsedHere(_currentUsedVars);
  if (_currentUsedVars.find(_lookingFor) != _currentUsedVars.end()) {
    if (ExecutionNode::CALCULATION != type) {
      return true;
    }
    _isCalculationNodesFound = true;
  }

  return false;
}

}  // namespace aql
}  // namespace arangodb
