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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_CALCULATION_BLOCK_H
#define ARANGOD_AQL_CALCULATION_BLOCK_H 1

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Utils/AqlTransaction.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;

class ExecutionEngine;

class CalculationBlock : public ExecutionBlock {
 public:
  CalculationBlock(ExecutionEngine*, CalculationNode const*);

  ~CalculationBlock();

 private:
  /// @brief fill the target register in the item block with a reference to
  /// another variable
  void fillBlockWithReference(AqlItemBlock*);

  /// @brief shared code for executing a simple or a V8 expression
  void executeExpression(AqlItemBlock*);

  /// @brief doEvaluation, private helper to do the work
  void doEvaluation(AqlItemBlock*);

 public:
  /// @brief getSome
  AqlItemBlock* getSome(size_t atLeast, size_t atMost) override final;

 private:
  /// @brief we hold a pointer to the expression in the plan
  Expression* _expression;

  /// @brief info about input variables
  std::vector<Variable const*> _inVars;

  /// @brief info about input registers
  std::vector<RegisterId> _inRegs;

  /// @brief output register
  RegisterId _outReg;

  /// @brief condition variable register
  RegisterId _conditionReg;

  /// @brief whether or not the expression is a simple variable reference
  bool _isReference;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
