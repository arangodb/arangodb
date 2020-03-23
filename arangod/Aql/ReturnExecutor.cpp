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

#include "ReturnExecutor.h"
#include "Aql/AqlValue.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

ReturnExecutorInfos::ReturnExecutorInfos(RegisterId inputRegister, RegisterId nrInputRegisters,
                                         RegisterId nrOutputRegisters, bool doCount)
    : ExecutorInfos(make_shared_unordered_set({inputRegister}),
                    make_shared_unordered_set({0}), nrInputRegisters, nrOutputRegisters,
                    std::unordered_set<RegisterId>{} /*to clear*/,
                    std::unordered_set<RegisterId>{} /*to keep*/
                    ),
      _inputRegisterId(inputRegister),
      _doCount(doCount) {
  // For the time beeing return will only write to register 0.
  // It is defined that it can only have exactly 1 output register.
  // We can easily replace this by a different register, if we
  // modify the caller within the ExecutionEngine to ask for the
  // output register from outside.
  TRI_ASSERT(nrOutputRegisters == 1);
}

ReturnExecutor::ReturnExecutor(Fetcher& fetcher, ReturnExecutorInfos& infos)
    : _infos(infos), _fetcher(fetcher) {}

ReturnExecutor::~ReturnExecutor() = default;

// TODO: @deprecated remove
std::pair<ExecutionState, size_t> ReturnExecutor::expectedNumberOfRows(size_t atMost) const {
  return _fetcher.preFetchNumberOfRows(atMost);
}

// TODO: @deprecated remove
auto ReturnExecutor::produceRows(OutputAqlItemRow& output)
    -> std::pair<ExecutionState, Stats> {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto ReturnExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  TRI_IF_FAILURE("ReturnExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  auto stats = Stats{};
  auto skippedUpstream = inputRange.skipAll();
  call.didSkip(skippedUpstream);
  /*
  if (_infos.doCount()) {
    // TODO: do we need to include counted here?
    stats.incrCounted(skippedUpstream);
  }
  */

  while (inputRange.hasDataRow() && call.needSkipMore()) {
    // I do not think that this is actually called.
    // It will be called first to get the upstream-Call
    // but this executor will always delegate the skipping
    // to upstream.
    TRI_ASSERT(false);
    auto [state, input] = inputRange.nextDataRow();
    TRI_ASSERT(input.isInitialized());
    TRI_IF_FAILURE("ReturnBlock::getSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    call.didSkip(1);

    /*
    if (_infos.doCount()) {
      // TODO: do we need to include counted here?
      stats.incrCounted();
    }
    */
  }

  return {inputRange.upstreamState(), stats, call.getSkipCount(), call};
}

auto ReturnExecutor::produceRows(AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TRI_IF_FAILURE("ReturnExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  Stats stats{};

  while (inputRange.hasDataRow() && !output.isFull()) {
    auto [state, input] = inputRange.nextDataRow();
    TRI_ASSERT(input.isInitialized());
    // REMARK: it is called `getInputRegisterId` here but FilterExecutor calls it `getInputRegister`.
    AqlValue val = input.stealValue(_infos.getInputRegisterId());
    AqlValueGuard guard(val, true);
    TRI_IF_FAILURE("ReturnBlock::getSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    output.moveValueInto(_infos.getOutputRegisterId(), input, guard);
    output.advanceRow();
    if (_infos.doCount()) {
      stats.incrCounted();
    }
  }

  return {inputRange.upstreamState(), stats, output.getClientCall()};
}

[[nodiscard]] auto ReturnExecutor::expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                                           AqlCall const& call) const
    noexcept -> size_t {
  if (input.finalState() == ExecutorState::DONE) {
    return input.countDataRows();
  }
  // Otherwise we do not know.
  return call.getLimit();
}
