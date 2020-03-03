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
////////////////////////////////////////////////////////////////////////////////

#include "SubqueryStartExecutor.h"

#include "Aql/ExecutionState.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"

using namespace arangodb;
using namespace arangodb::aql;

SubqueryStartExecutor::SubqueryStartExecutor(Fetcher& fetcher, Infos& infos)
    : _fetcher(fetcher) {}
SubqueryStartExecutor::~SubqueryStartExecutor() = default;

std::pair<ExecutionState, NoStats> SubqueryStartExecutor::produceRows(OutputAqlItemRow& output) {
  while (!output.isFull()) {
    switch (_internalState) {
      case State::READ_DATA_ROW: {
        TRI_ASSERT(!_input.isInitialized());
        if (_upstreamState == ExecutionState::DONE) {
          return {ExecutionState::DONE, NoStats{}};
        }
        // We need to round the number of rows, otherwise this might be called
        // with atMost == 0 Note that we must not set _upstreamState to DONE
        // here, as fetchRow will report DONE when encountering a shadow row.
        auto rowWithStates =
            _fetcher.fetchRowWithGlobalState((output.numRowsLeft() + 1) / 2);
        _input = std::move(rowWithStates.row);
        if (rowWithStates.localState == ExecutionState::WAITING) {
          return {ExecutionState::WAITING, NoStats{}};
        }
        TRI_ASSERT(_upstreamState == ExecutionState::HASMORE);
        TRI_ASSERT(rowWithStates.globalState == ExecutionState::HASMORE ||
                   rowWithStates.globalState == ExecutionState::DONE);
        // This can only switch from HASMORE to DONE
        _upstreamState = rowWithStates.globalState;
        if (!_input.isInitialized()) {
          TRI_ASSERT(rowWithStates.localState == ExecutionState::DONE);
          _internalState = State::PASS_SHADOW_ROW;
        } else {
          _internalState = State::PRODUCE_DATA_ROW;
        }
      } break;
      case State::PRODUCE_DATA_ROW: {
        TRI_ASSERT(!output.isFull());
        TRI_ASSERT(_input.isInitialized());
        output.copyRow(_input);
        output.advanceRow();
        _internalState = State::PRODUCE_SHADOW_ROW;
      } break;
      case State::PRODUCE_SHADOW_ROW: {
        TRI_ASSERT(_input.isInitialized());
        output.createShadowRow(_input);
        output.advanceRow();
        _input = InputAqlItemRow(CreateInvalidInputRowHint{});
        _internalState = State::READ_DATA_ROW;
      } break;
      case State::PASS_SHADOW_ROW: {
        if (_upstreamState == ExecutionState::DONE) {
          return {ExecutionState::DONE, NoStats{}};
        }
        // We need to handle shadowRows now. It is the job of this node to
        // increase the shadow row depth
        auto const [state, shadowRow] = _fetcher.fetchShadowRow();
        if (state == ExecutionState::WAITING) {
          return {ExecutionState::WAITING, NoStats{}};
        }
        TRI_ASSERT(_upstreamState == ExecutionState::HASMORE);
        TRI_ASSERT(state == ExecutionState::HASMORE || state == ExecutionState::DONE);
        // This can only switch from HASMORE to DONE
        _upstreamState = state;
        if (shadowRow.isInitialized()) {
          output.increaseShadowRowDepth(shadowRow);
          output.advanceRow();
          // stay in state PASS_SHADOW_ROW
        } else {
          _internalState = State::READ_DATA_ROW;
        }
      } break;
    }
  }

  if (_internalState == State::READ_DATA_ROW || _internalState == State::PASS_SHADOW_ROW) {
    TRI_ASSERT(!_input.isInitialized());
    return {_upstreamState, NoStats{}};
  }
  // PRODUCE_DATA_ROW should always immediately be processed without leaving the
  // loop
  TRI_ASSERT(_internalState == State::PRODUCE_SHADOW_ROW);
  TRI_ASSERT(_input.isInitialized());
  return {ExecutionState::HASMORE, NoStats{}};
}

std::pair<ExecutionState, size_t> SubqueryStartExecutor::expectedNumberOfRows(size_t atMost) const {
  auto const [state, upstreamRows] = _fetcher.preFetchNumberOfRows(atMost);
  // We will write one shadow row per input data row.
  // We might write less on all shadow rows in input, right now we do not figure this out yes.
  TRI_ASSERT( _input.isInitialized() == (_internalState == State::PRODUCE_SHADOW_ROW));
  TRI_ASSERT(_internalState != State::PRODUCE_DATA_ROW);

  // Return 1 if _input.isInitialized(), and 0 otherwise. Looks more complicated
  // than it is.
  auto const localRows = [this]() {
    if (_input.isInitialized()) {
      switch (_internalState) {
        case State::PRODUCE_SHADOW_ROW:
          return 1;
        case State::PRODUCE_DATA_ROW:
        case State::READ_DATA_ROW:
        case State::PASS_SHADOW_ROW: {
          TRI_ASSERT(false);
          using namespace std::string_literals;
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL_AQL,
              "Unexpected state "s + stateToString(_internalState) +
                  " in SubqueryStartExecutor with local row");
        }
      }
    } else {
      switch (_internalState) {
        case State::READ_DATA_ROW:
        case State::PASS_SHADOW_ROW:
          return 0;
        case State::PRODUCE_DATA_ROW:
        case State::PRODUCE_SHADOW_ROW:
          TRI_ASSERT(false);
          using namespace std::string_literals;
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL_AQL,
              "Unexpected state "s + stateToString(_internalState) +
                  " in SubqueryStartExecutor with no local row");
      }
    }
    TRI_ASSERT(false);
    return 0;
  }();

  return {state, upstreamRows * 2 + localRows};
}

auto SubqueryStartExecutor::stateToString(SubqueryStartExecutor::State state) -> std::string {
  switch (state) {
    case State::READ_DATA_ROW:
      return "READ_DATA_ROW";
    case State::PRODUCE_DATA_ROW:
      return "PRODUCE_DATA_ROW";
    case State::PRODUCE_SHADOW_ROW:
      return "PRODUCE_SHADOW_ROW";
    case State::PASS_SHADOW_ROW:
      return "PASS_SHADOW_ROW";
  }
  return "unhandled state";
}
