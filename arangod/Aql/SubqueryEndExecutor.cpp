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
#include "Basics/ScopeGuard.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <memory>
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

RegisterId SubqueryEndExecutorInfos::getInputRegister() const noexcept {
  return _inReg;
}

SubqueryEndExecutor::SubqueryEndExecutor(Fetcher& fetcher, SubqueryEndExecutorInfos& infos)
    : _fetcher(fetcher), _infos(infos), _accumulator(_infos.vpackOptions()) {}

SubqueryEndExecutor::~SubqueryEndExecutor() = default;

std::pair<ExecutionState, NoStats> SubqueryEndExecutor::produceRows(OutputAqlItemRow& output) {
  ExecutionState state;

  InputAqlItemRow inputRow = InputAqlItemRow{CreateInvalidInputRowHint()};
  ShadowAqlItemRow shadowRow = ShadowAqlItemRow{CreateInvalidShadowRowHint{}};

  while (!output.isFull()) {
    switch (_state) {
      case State::ACCUMULATE_DATA_ROWS: {
        std::tie(state, inputRow) = _fetcher.fetchRow();

        if (state == ExecutionState::WAITING) {
          TRI_ASSERT(!inputRow.isInitialized());
          return {state, NoStats{}};
        }

        // We got a data row, put it into the accumulator
        if (inputRow.isInitialized() && _infos.usesInputRegister()) {
          _accumulator.addValue(inputRow.getValue(_infos.getInputRegister()));
        }

        // We have received DONE on data rows, so now
        // we have to read a relevant shadow row
        if (state == ExecutionState::DONE) {
          _state = State::PROCESS_SHADOW_ROWS;
        }
      } break;
      case State::PROCESS_SHADOW_ROWS: {
        std::tie(state, shadowRow) = _fetcher.fetchShadowRow();
        if (state == ExecutionState::WAITING) {
          TRI_ASSERT(!shadowRow.isInitialized());
          return {ExecutionState::WAITING, NoStats{}};
        }
        TRI_ASSERT(state == ExecutionState::DONE || state == ExecutionState::HASMORE);

        if (!shadowRow.isInitialized()) {
          TRI_ASSERT(_accumulator.numValues() == 0);
          if (state == ExecutionState::HASMORE) {
            // We did not get another shadowRow; either we
            // are DONE or we are getting another relevant
            // shadow row, but only after we called fetchRow
            // again
            _state = State::ACCUMULATE_DATA_ROWS;
          } else {
            TRI_ASSERT(state == ExecutionState::DONE);
            return {ExecutionState::DONE, NoStats{}};
          }
        } else {
          if (shadowRow.isRelevant()) {
            AqlValue value;
            AqlValueGuard guard = _accumulator.stealValue(value);

            // Responsibility is handed over to output
            output.consumeShadowRow(_infos.getOutputRegister(), shadowRow, guard);
            TRI_ASSERT(output.produced());
            output.advanceRow();

            if (state == ExecutionState::DONE) {
              return {ExecutionState::DONE, NoStats{}};
            }
          } else {
            TRI_ASSERT(_accumulator.numValues() == 0);
            // We got a shadow row, it must be irrelevant,
            // because to get another relevant shadowRow we must
            // first call fetchRow again
            output.decreaseShadowRowDepth(shadowRow);
            output.advanceRow();
          }
        }
      } break;

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

void SubqueryEndExecutor::Accumulator::reset() {
  _buffer = std::make_unique<arangodb::velocypack::Buffer<uint8_t>>();
  _builder = std::make_unique<VPackBuilder>(*_buffer);
  TRI_ASSERT(_builder != nullptr);
  _builder->openArray();
  _numValues = 0;
}

void SubqueryEndExecutor::Accumulator::addValue(AqlValue const& value) {
  TRI_ASSERT(_builder->isOpenArray());
  value.toVelocyPack(_options, *_builder, false);
  ++_numValues;
}

SubqueryEndExecutor::Accumulator::Accumulator(VPackOptions const* const options)
    : _options(options) {
  reset();
}

AqlValueGuard SubqueryEndExecutor::Accumulator::stealValue(AqlValue& result) {
  // Note that an AqlValueGuard holds an AqlValue&, so we cannot create it from
  // a local AqlValue and return the Guard!

  TRI_ASSERT(_builder->isOpenArray());
  _builder->close();
  TRI_ASSERT(_builder->isClosed());

  // Here we have all data *and* the relevant shadow row,
  // so we can now submit
  bool shouldDelete = true;
  result = AqlValue{_buffer.get(), shouldDelete};
  if (shouldDelete) {
    // resultDocVec told us to delete our data
    _buffer->clear();
  } else {
    // relinquish ownership of _buffer, as it now belongs to
    // resultDocVec
    std::ignore = _buffer.release();
  }

  // Call reset *after* AqlValueGuard is constructed, so when an exception is
  // thrown, the ValueGuard can free the AqlValue.
  TRI_DEFER(reset());

  return AqlValueGuard{result, true};
}

size_t SubqueryEndExecutor::Accumulator::numValues() const noexcept {
  return _numValues;
}

// We write at most the number of rows that we get from above (potentially much less if we accumulate a lot)
std::pair<ExecutionState, size_t> SubqueryEndExecutor::expectedNumberOfRows(size_t atMost) const {
  ExecutionState state{ExecutionState::HASMORE};
  size_t expected = 0;
  std::tie(state, expected) = _fetcher.preFetchNumberOfRows(atMost);
  return {state, expected};
}
