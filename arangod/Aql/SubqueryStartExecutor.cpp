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

SubqueryStartExecutor::SubqueryStartExecutor(Fetcher& fetcher, Infos& infos) {}

std::pair<ExecutionState, NoStats> SubqueryStartExecutor::produceRows(OutputAqlItemRow& output) {
  TRI_ASSERT(false);
}

auto SubqueryStartExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  while (!output.isFull()) {
    switch (_internalState) {
      case State::READ_DATA_ROW: {
        TRI_ASSERT(!_inputRow.isInitialized());

        if (input.hasDataRow()) {
          std::tie(_upstreamState, _inputRow) = input.nextDataRow();
          _internalState = State::PRODUCE_DATA_ROW;
        } else {
          // might have shadow rows we need to pass through
          _internalState = State::PASS_SHADOW_ROW;
        }
      } break;
      case State::PRODUCE_DATA_ROW: {
        TRI_ASSERT(!output.isFull());
        TRI_ASSERT(_inputRow.isInitialized());
        output.copyRow(_inputRow);
        output.advanceRow();
        _internalState = State::PRODUCE_SHADOW_ROW;
      } break;
      case State::PRODUCE_SHADOW_ROW: {
        TRI_ASSERT(_inputRow.isInitialized());
        output.createShadowRow(_inputRow);
        output.advanceRow();
        _inputRow = InputAqlItemRow(CreateInvalidInputRowHint{});
        _internalState = State::READ_DATA_ROW;
      } break;
      case State::PASS_SHADOW_ROW: {
        // We need to handle shadowRows now. It is the job of this node to
        // increase the shadow row depth
        if (input.hasShadowRow()) {
          ShadowAqlItemRow shadowRow{CreateInvalidShadowRowHint{}};
          std::tie(_upstreamState, shadowRow) = input.nextShadowRow();
          output.increaseShadowRowDepth(shadowRow);
          output.advanceRow();
        } else {
          if (_upstreamState == ExecutorState::DONE) {
            return {ExecutorState::DONE, NoStats{}, AqlCall{}};
          } else {
            _internalState = State::READ_DATA_ROW;
          }
        }
      } break;
    }
  }

  // When we reach here, then output is full
  if (_internalState == State::READ_DATA_ROW || _internalState == State::PASS_SHADOW_ROW) {
    TRI_ASSERT(!_inputRow.isInitialized());
    return {_upstreamState, NoStats{}, AqlCall{}};
  }
  // PRODUCE_DATA_ROW should always immediately be processed without leaving the
  // loop
  TRI_ASSERT(_internalState == State::PRODUCE_SHADOW_ROW);
  TRI_ASSERT(_inputRow.isInitialized());
  return {ExecutorState::HASMORE, NoStats{}, AqlCall{}};
}

auto SubqueryStartExecutor::skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, size_t, AqlCall> {
  auto skipped = size_t{0};
  while (call.shouldSkip()) {
    switch (_internalState) {
      case State::READ_DATA_ROW: {
        TRI_ASSERT(!_inputRow.isInitialized());

        if (input.hasDataRow()) {
          std::tie(_upstreamState, _inputRow) = input.nextDataRow();
        } else {
          // might have shadow rows we need to pass through
          _internalState = State::PASS_SHADOW_ROW;
        }
      } break;
      case State::PRODUCE_DATA_ROW: {
        call.didSkip(1);
        skipped++;
        _internalState = State::READ_DATA_ROW;
      } break;
      case State::PRODUCE_SHADOW_ROW: {
        TRI_ASSERT(false);
      } break;
      case State::PASS_SHADOW_ROW: {
        TRI_ASSERT(false);
      } break;
    }
  }

  return {_upstreamState, skipped, AqlCall{}};
}

// FIXME: we cannot prefetch anything
//        because we dont' have a fetcher
auto SubqueryStartExecutor::expectedNumberOfRows(size_t atMost) const
    -> std::pair<ExecutionState, size_t> {
  TRI_ASSERT(false);

  auto state = ExecutionState{ExecutionState::HASMORE};
  auto upstreamRows = size_t{2 * atMost};
  //   auto const [state, upstreamRows] = _fetcher.preFetchNumberOfRows(atMost);
  // We will write one shadow row per input data row.
  // We might write less on all shadow rows in input, right now we do not figure this out yes.
  TRI_ASSERT(_inputRow.isInitialized() == (_internalState == State::PRODUCE_SHADOW_ROW));
  TRI_ASSERT(_internalState != State::PRODUCE_DATA_ROW);

  // Return 1 if _input.isInitialized(), and 0 otherwise. Looks more
  // complicated than it is.
  auto const localRows = [this]() {
    if (_inputRow.isInitialized()) {
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
