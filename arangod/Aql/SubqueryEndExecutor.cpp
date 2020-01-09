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

#include "Aql/SubqueryEndExecutor.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

SubqueryEndExecutorInfos::SubqueryEndExecutorInfos(
    std::shared_ptr<std::unordered_set<RegisterId>> readableInputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> writeableOutputRegisters,
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> const& registersToClear,
    std::unordered_set<RegisterId> registersToKeep,
    velocypack::Options const* const options, RegisterId inReg, RegisterId outReg)
    : ExecutorInfos(std::move(readableInputRegisters),
                    std::move(writeableOutputRegisters), nrInputRegisters,
                    nrOutputRegisters, registersToClear, std::move(registersToKeep)),
      _vpackOptions(options),
      _outReg(outReg),
      _inReg(inReg) {}

SubqueryEndExecutorInfos::~SubqueryEndExecutorInfos() = default;

bool SubqueryEndExecutorInfos::usesInputRegister() const noexcept {
  return _inReg != RegisterPlan::MaxRegisterId;
}

velocypack::Options const* SubqueryEndExecutorInfos::vpackOptions() const noexcept {
  return _vpackOptions;
}

RegisterId SubqueryEndExecutorInfos::getOutputRegister() const noexcept {
  return _outReg;
}

RegisterId SubqueryEndExecutorInfos::getInputRegister() const noexcept { return _inReg; }

SubqueryEndExecutor::SubqueryEndExecutor(Fetcher& fetcher, SubqueryEndExecutorInfos& infos)
    : _fetcher(fetcher), _infos(infos), _accumulator(nullptr), _state(ACCUMULATE) {
  resetAccumulator();
}

SubqueryEndExecutor::~SubqueryEndExecutor() = default;

std::pair<ExecutionState, NoStats> SubqueryEndExecutor::produceRows(OutputAqlItemRow& output) {
  ExecutionState state;

  InputAqlItemRow inputRow = InputAqlItemRow{CreateInvalidInputRowHint()};
  ShadowAqlItemRow shadowRow = ShadowAqlItemRow{CreateInvalidShadowRowHint{}};

  while (!output.isFull()) {
    switch (_state) {
      case ACCUMULATE: {
        std::tie(state, inputRow) = _fetcher.fetchRow();

        if (state == ExecutionState::WAITING) {
          TRI_ASSERT(!inputRow.isInitialized());
          return {state, NoStats{}};
        }

        // We got a data row, put it into the accumulator
        if (inputRow.isInitialized() && _infos.usesInputRegister()) {
          TRI_ASSERT(_accumulator->isOpenArray());
          AqlValue value = inputRow.getValue(_infos.getInputRegister());
          value.toVelocyPack(_infos.vpackOptions(), *_accumulator, false);
        }

        // We have received DONE on data rows, so now
        // we have to read a relevant shadow row
        if (state == ExecutionState::DONE) {
          _accumulator->close();
          TRI_ASSERT(_accumulator->isClosed());
          _state = RELEVANT_SHADOW_ROW_PENDING;
        }
        break;
      }
      case RELEVANT_SHADOW_ROW_PENDING: {
        std::tie(state, shadowRow) = _fetcher.fetchShadowRow();
        if (state == ExecutionState::WAITING) {
          TRI_ASSERT(!shadowRow.isInitialized());
          return {ExecutionState::WAITING, NoStats{}};
        }
        TRI_ASSERT(state == ExecutionState::DONE || state == ExecutionState::HASMORE);

        // This case happens when there are no inputs from the enclosing query,
        // hence our companion SubqueryStartExecutor has not produced any rows
        // (in particular not a ShadowRow), so we are done.
        if (state == ExecutionState::DONE && !shadowRow.isInitialized()) {
          /* We had better not accumulated any results if we get here */
          TRI_ASSERT(_accumulator->slice().length() == 0);
          resetAccumulator();
          _state = RELEVANT_SHADOW_ROW_PENDING;
          return {ExecutionState::DONE, NoStats{}};
        }

        // Here we have all data *and* the relevant shadow row,
        // so we can now submit
        TRI_ASSERT(shadowRow.isInitialized());
        TRI_ASSERT(shadowRow.isRelevant());

        bool shouldDelete = true;
        AqlValue resultDocVec{_buffer.get(), shouldDelete};
        if (shouldDelete) {
          // resultDocVec told us to delete our data
          _buffer->clear();
        } else {
          // relinquish ownership of _buffer, as it now belongs to
          // resultDocVec
          _buffer.release();
        }
        AqlValueGuard guard{resultDocVec, true};

        // Responsibility is handed over to output
        output.consumeShadowRow(_infos.getOutputRegister(), shadowRow, guard);
        TRI_ASSERT(output.produced());
        output.advanceRow();

        // Reset the accumulator in case we read more data
        resetAccumulator();

        if (state == ExecutionState::DONE) {
          return {ExecutionState::DONE, NoStats{}};
        } else {
          _state = FORWARD_IRRELEVANT_SHADOW_ROWS;
        }
        break;
      }
      case FORWARD_IRRELEVANT_SHADOW_ROWS: {
        // Forward irrelevant shadow rows (and only those)
        std::tie(state, shadowRow) = _fetcher.fetchShadowRow();

        if (state == ExecutionState::WAITING) {
          return {ExecutionState::WAITING, NoStats{}};
        }

        if (shadowRow.isInitialized()) {
          // We got a shadow row, it must be irrelevant,
          // because to get another relevant shadowRow we must
          // first call fetchRow again
          TRI_ASSERT(shadowRow.isRelevant() == false);
          output.decreaseShadowRowDepth(shadowRow);
          output.advanceRow();
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
          return {ExecutionState::DONE, NoStats{}};
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
  _buffer.reset(new arangodb::velocypack::Buffer<uint8_t>);
  _accumulator.reset(new VPackBuilder(*_buffer));
  TRI_ASSERT(_accumulator != nullptr);
  _accumulator->openArray();
}

// We write at most the number of rows that we get from above (potentially much less if we accumulate a lot)
std::pair<ExecutionState, size_t> SubqueryEndExecutor::expectedNumberOfRows(size_t atMost) const {
  ExecutionState state{ExecutionState::HASMORE};
  size_t expected = 0;
  std::tie(state, expected) = _fetcher.preFetchNumberOfRows(atMost);
  return {state, expected};
}
