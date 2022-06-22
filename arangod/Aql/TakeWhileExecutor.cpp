////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "TakeWhileExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/Stats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Basics/debugging.h"
#include "Cluster/ServerState.h"

namespace arangodb::aql {

TakeWhileExecutorInfos::TakeWhileExecutorInfos(RegisterId inputRegister,
                                               bool emitFirstFalseLine)
    : _inputRegister(inputRegister), _emitFirstFalseLine(emitFirstFalseLine) {}

auto TakeWhileExecutorInfos::getInputRegister() const noexcept -> RegisterId {
  return _inputRegister;
}

auto TakeWhileExecutorInfos::emitFirstFalseLine() const noexcept -> bool {
  return _emitFirstFalseLine;
}

TakeWhileExecutor::TakeWhileExecutor(TakeWhileExecutor::Fetcher&,
                                     TakeWhileExecutor::Infos& infos)
    : _infos(infos) {
  // At least currently, emitFirstFalseLine() should be set exactly when the
  // node is on the DBServer. If this changes in the future e.g. because users
  // get an option to turn this on for themselves, this assertion can be
  // removed.
  TRI_ASSERT(ServerState::instance()->isDBServer() ==
             infos.emitFirstFalseLine());
}

[[nodiscard]] auto TakeWhileExecutor::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TakeWhileStats stats{};

  while (!_stopTaking && inputRange.hasDataRow() && !output.isFull()) {
    auto const& [state, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input);
    TRI_ASSERT(input.isInitialized());
    if (input.getValue(_infos.getInputRegister()).toBoolean()) {
      output.copyRow(input);
      output.advanceRow();
    } else {
      _stopTaking = true;
      if (_infos.emitFirstFalseLine()) {
        output.copyRow(input);
        output.advanceRow();
      }
      // TODO count overfetched rows
      // stats.incrFiltered();
    }
  }

  if (_stopTaking) {
    auto call = AqlCall{};
    call.hardLimit = std::size_t(0);
    return {ExecutorState::DONE, stats, call};
  }
  // Just fetch everything from above, allow overfetching
  return {inputRange.upstreamState(), stats, AqlCall{}};
}

auto TakeWhileExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                      AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  // This must never be true: It is used when TAKE WHILE is pushed (partially)
  // on the DBServers, on these nodes; however, in this situation, skipping
  // may only happen on the coordinator.
  TRI_ASSERT(!_infos.emitFirstFalseLine());
  TakeWhileStats stats{};

  while (inputRange.hasDataRow() && call.needSkipMore()) {
    auto const [_, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input);
    TRI_ASSERT(input.isInitialized());
    if (input.getValue(_infos.getInputRegister()).toBoolean()) {
      call.didSkip(1);
    } else {
      _stopTaking = true;
      // TODO count overfetched rows
      // stats.incrFiltered();
    }
  }

  if (_stopTaking) {
    auto upstreamCall = AqlCall{};
    upstreamCall.hardLimit = std::size_t(0);
    return {ExecutorState::DONE, stats, call.getSkipCount(), upstreamCall};
  }
  // Just fetch everything from above, allow overfetching
  return {inputRange.upstreamState(), stats, call.getSkipCount(), AqlCall{}};
}

auto TakeWhileExecutor::expectedNumberOfRowsNew(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept
    -> size_t {
  TRI_ASSERT(!_stopTaking);
  if (input.finalState() == MainQueryState::DONE) {
    // This could be improved by looking for the first false value (if any) in
    // our input register.
    return std::min(call.getLimit(), input.countDataRows());
  }
  // We do not know how many more rows will be returned from upstream.
  // So we can only overestimate
  return call.getLimit();
}

}  // namespace arangodb::aql
