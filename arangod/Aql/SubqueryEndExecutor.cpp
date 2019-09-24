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
/// @author Michael Hackstein
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionBlock.h"
#include "SubqueryEndExecutor.h"

#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"

using namespace arangodb;
using namespace arangodb::aql;

SubqueryEndExecutorInfos::SubqueryEndExecutorInfos(
    std::shared_ptr<std::unordered_set<RegisterId>> readableInputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> writeableOutputRegisters,
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> const& registersToClear,
    std::unordered_set<RegisterId> registersToKeep,
    transaction::Methods* trxPtr, RegisterId outReg)
    : ExecutorInfos(readableInputRegisters, writeableOutputRegisters, nrInputRegisters,
                    nrOutputRegisters, registersToClear, std::move(registersToKeep)),
      _trxPtr(trxPtr), _outReg(outReg) { }

SubqueryEndExecutorInfos::SubqueryEndExecutorInfos(SubqueryEndExecutorInfos&& other) = default;

SubqueryEndExecutorInfos::~SubqueryEndExecutorInfos() = default;

SubqueryEndExecutor::SubqueryEndExecutor(Fetcher& fetcher, SubqueryEndExecutorInfos& infos)
    : _fetcher(fetcher),
      _infos(infos),
      _outputPending(false) {
  resetAccumulator();
}

SubqueryEndExecutor::~SubqueryEndExecutor() = default;

std::pair<ExecutionState, NoStats> SubqueryEndExecutor::produceRows(OutputAqlItemRow& output) {
  ExecutionState state;
  NoStats stats;

  InputAqlItemRow inputRow = InputAqlItemRow{CreateInvalidInputRowHint()};
  ShadowAqlItemRow shadowRow = ShadowAqlItemRow{CreateInvalidShadowRowHint{}};

  while (true) {
    std::tie(state, inputRow) = _fetcher.fetchRow();

    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(!inputRow.isInitialized());
      return {state, std::move(stats)};
    }

    // We got a row, put it into the accumulator
    if (inputRow.isInitialized()) {
      // Above is a RETURN which writes to register 0
      TRI_ASSERT(_accumulator.isOpenArray());
      AqlValue value = inputRow.getValue(0);
      value.toVelocyPack(_infos.getTrxPtr(), _accumulator, false);
      _outputPending = true;
    } else {
      // Did not get a row, we had better be done now, because we caught WAITING above.
      TRI_ASSERT(state == ExecutionState::DONE);

      // do this only if pending outputs
      // submit result to output
      if(_outputPending) {
        // Now we expect a shadow row which contains the input
        // to our subquery, i.e. isRelevant().
        std::tie(state, shadowRow) = _fetcher.fetchShadowRow();
        if (state == ExecutionState::WAITING) {
          TRI_ASSERT(!shadowRow.isInitialized());
          return {ExecutionState::WAITING, std::move(stats)};
        }

        TRI_ASSERT(state == ExecutionState::DONE || state == ExecutionState::HASMORE);
        TRI_ASSERT(shadowRow.isInitialized() == true);
        TRI_ASSERT(shadowRow.isRelevant() == true);

        // Here we have all data *and* the relevant shadow row,
        // so we can now submit
        _accumulator.close();
        TRI_ASSERT(_accumulator.isClosed());

        AqlValue resultDocVec{_accumulator.slice()};
        AqlValueGuard guard{resultDocVec, true};

        // Responsibility is handed over
        output.moveValueInto(_infos.getOutputRegister(), shadowRow, guard);
        TRI_ASSERT(output.produced());

        resetAccumulator();
        TRI_ASSERT(_outputPending == false);
      }

      // we have to consume and forward all immediately following shadow rows,
      // and they have to have isRelevant == false
      do {
        std::tie(state, shadowRow) = _fetcher.fetchShadowRow();
        if (state == ExecutionState::WAITING) {
          return {ExecutionState::WAITING, std::move(stats)};
        }
        if (shadowRow.isInitialized()) {
          output.decreaseShadowRowDepth(shadowRow);
        }
      } while(state != ExecutionState::DONE);

      // we are done now
      return {ExecutionState::DONE, std::move(stats)};
    }
  }

  return {ExecutionState::DONE, NoStats{}};
}

void SubqueryEndExecutor::resetAccumulator() {
  _accumulator.clear();
  _accumulator.openArray();
  _outputPending = false;
}
