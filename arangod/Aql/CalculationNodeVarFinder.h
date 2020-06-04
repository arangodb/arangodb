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

#ifndef ARANGOD_AQL_CALCULATION_NODE_VAR_FINDER_H
#define ARANGOD_AQL_CALCULATION_NODE_VAR_FINDER_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/WalkerWorker.h"
#include "Containers/SmallVector.h"

namespace arangodb {
namespace aql {

class CalculationNodeVarFinder final : public WalkerWorker<ExecutionNode> {
  Variable const* _lookingFor;

  ::arangodb::containers::SmallVector<ExecutionNode*>& _out;

  VarSet _currentUsedVars;

 public:
  CalculationNodeVarFinder(Variable const* var, ::arangodb::containers::SmallVector<ExecutionNode*>& out) noexcept;

  bool before(ExecutionNode*) override final;
};

class CalculationNodeVarExistenceFinder final : public WalkerWorker<ExecutionNode> {
  Variable const* _lookingFor;

  VarSet _currentUsedVars;

  bool _isCalculationNodesFound;

 public:
  CalculationNodeVarExistenceFinder(Variable const* var) noexcept;

  bool before(ExecutionNode*) override final;

  bool isCalculationNodesFound() const noexcept {
    return _isCalculationNodesFound;
  }
};
}  // namespace aql
}  // namespace arangodb

#endif // ARANGOD_AQL_CALCULATION_NODE_VAR_FINDER_H
