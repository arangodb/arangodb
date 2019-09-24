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
      _state(ACCUMULATE) {
  resetAccumulator();
}

SubqueryEndExecutor::~SubqueryEndExecutor() = default;

std::pair<ExecutionState, NoStats> SubqueryEndExecutor::produceRows(OutputAqlItemRow& output) {
  ExecutionState state;
  NoStats stats;

  InputAqlItemRow inputRow = InputAqlItemRow{CreateInvalidInputRowHint()};
  ShadowAqlItemRow shadowRow = ShadowAqlItemRow{CreateInvalidShadowRowHint{}};

  while (!output.isFull()) {
    switch (_state) {
      case ACCUMULATE: {
        std::tie(state, inputRow) = _fetcher.fetchRow();

        if (state == ExecutionState::WAITING) {
          TRI_ASSERT(!inputRow.isInitialized());
          return {state, std::move(stats)};
        }

        // We got a data row, put it into the accumulator
        if (inputRow.isInitialized()) {
          // Above is a RETURN which writes to register 0
          // TODO: The RETURN node above is superflous and could be folded
          //       into the SubqueryEndNode
          TRI_ASSERT(_accumulator.isOpenArray());
          AqlValue value = inputRow.getValue(0);
          value.toVelocyPack(_infos.getTrxPtr(), _accumulator, false);
        }

        // We have received DONE on data rows, so now
        // we have to read a relevant shadow row
        if (state == ExecutionState::DONE) {
          _accumulator.close();
          TRI_ASSERT(_accumulator.isClosed());
          _state = RELEVANT_SHADOW_ROW_PENDING;
        }
        break;
      }
      case RELEVANT_SHADOW_ROW_PENDING: {
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
        AqlValue resultDocVec{_accumulator.slice()};
        AqlValueGuard guard{resultDocVec, true};

        // Responsibility is handed over
        output.moveValueInto(_infos.getOutputRegister(), shadowRow, guard);
        TRI_ASSERT(output.produced());

        // Reset the accumulator in case we read more data
        resetAccumulator();
        _state = FORWARD_IRRELEVANT_SHADOW_ROWS;
        break;
      }
      case FORWARD_IRRELEVANT_SHADOW_ROWS: {
        // Forward irrelevant shadow rows (and only those)
        std::tie(state, shadowRow) = _fetcher.fetchShadowRow();

        if (state == ExecutionState::WAITING) {
          return {ExecutionState::WAITING, std::move(stats)};
        }

        if (shadowRow.isInitialized()) {
          // We got a shadow row, it must be irrelevant,
          // because to get another relevant shadowRow we must
          // first call fetchRow again
          TRI_ASSERT(shadowRow.isRelevant() == false);
          output.decreaseShadowRowDepth(shadowRow);
        } else {
          // We did not get another shadowRow; either we
          // are DONE or we are getting another relevant
          // shadow row, but only after we called fetchRow
          // again
          if (state == ExecutionState::HASMORE) {
            _state = ACCUMULATE;
          }
        }

        if (state == ExecutionState::DONE) {
          return {ExecutionState::DONE, std::move(stats)};
        }
        break;
      }

      default: {
        TRI_ASSERT(false);
        break;
      }
    }
  }
  // We should *only* fall through here if output.isFull() is true.
  TRI_ASSERT(output.isFull());
  return {ExecutionState::HASMORE, NoStats{}};
}

void SubqueryEndExecutor::resetAccumulator() {
  _accumulator.clear();
  _accumulator.openArray();
}
