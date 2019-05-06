////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "CalculationExecutor.h"

#include "Basics/Common.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::aql;
CalculationExecutorInfos::CalculationExecutorInfos(
    RegisterId outputRegister, RegisterId nrInputRegisters,
    RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep, Query& query, Expression& expression,
    std::vector<Variable const*>&& expInVars, std::vector<RegisterId>&& expInRegs)
    : ExecutorInfos(make_shared_unordered_set(expInRegs.begin(), expInRegs.end()),
                    make_shared_unordered_set({outputRegister}),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _outputRegisterId(outputRegister),
      _query(query),
      _expression(expression),
      _expInVars(std::move(expInVars)),
      _expInRegs(std::move(expInRegs)) {
  TRI_ASSERT(_query.trx() != nullptr);
}

template <CalculationType calculationType>
CalculationExecutor<calculationType>::CalculationExecutor(Fetcher& fetcher,
                                                          CalculationExecutorInfos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _currentRow(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _rowState(ExecutionState::HASMORE),
      _hasEnteredContext(false) {}

template <CalculationType calculationType>
CalculationExecutor<calculationType>::~CalculationExecutor() = default;

template class ::arangodb::aql::CalculationExecutor<CalculationType::Condition>;
template class ::arangodb::aql::CalculationExecutor<CalculationType::V8Condition>;
template class ::arangodb::aql::CalculationExecutor<CalculationType::Reference>;
