////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

SubqueryEndExecutorInfos::SubqueryEndExecutorInfos(velocypack::Options const* const options,
                                                   RegisterId inReg, RegisterId outReg)
    : _vpackOptions(options),
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

SubqueryEndExecutor::SubqueryEndExecutor(Fetcher&, SubqueryEndExecutorInfos& infos)
    : _infos(infos), _accumulator(_infos.vpackOptions()) {}

SubqueryEndExecutor::~SubqueryEndExecutor() = default;

void SubqueryEndExecutor::initializeCursor() {
  _accumulator.reset();
}

auto SubqueryEndExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  // We can not account for skipped rows here.
  // If we get this we have invalid logic either in the upstream
  // produced by this Executor
  // or in the reporting by the Executor data is requested from
  TRI_ASSERT(input.skippedInFlight() == 0);
  ExecutorState state{ExecutorState::HASMORE};
  InputAqlItemRow inputRow{CreateInvalidInputRowHint()};

  while (input.hasDataRow()) {
    std::tie(state, inputRow) = input.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(inputRow.isInitialized());

    // We got a data row, put it into the accumulator,
    // if we're getting data through an input register.
    // If not, we just "accumulate" an empty output.
    if (_infos.usesInputRegister()) {
      _accumulator.addValue(inputRow.getValue(_infos.getInputRegister()));
    }
  }
  return {input.upstreamState(), NoStats{}, AqlCall{}};
}

auto SubqueryEndExecutor::skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  // We can not account for skipped rows here.
  // If we get this we have invalid logic either in the upstream
  // produced by this Executor
  // or in the reporting by the Executor data is requested from
  TRI_ASSERT(input.skippedInFlight() == 0);
  ExecutorState state;
  InputAqlItemRow inputRow{CreateInvalidInputRowHint()};

  while (input.hasDataRow()) {
    std::tie(state, inputRow) = input.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(inputRow.isInitialized());
  }
  // This is correct since the SubqueryEndExecutor produces one output out
  // of the accumulation of all the (relevant) inputs
  call.didSkip(1);
  return {input.upstreamState(), NoStats{}, 1, AqlCall{}};
}

auto SubqueryEndExecutor::consumeShadowRow(ShadowAqlItemRow shadowRow,
                                           OutputAqlItemRow& output) -> void {
  AqlValue value;
  AqlValueGuard guard = _accumulator.stealValue(value);
  output.consumeShadowRow(_infos.getOutputRegister(), shadowRow, guard);
}

void SubqueryEndExecutor::Accumulator::reset() {
  // Buffer present. we can get away with reusing and clearing
  // the existing Builder, which points to the Buffer
  _builder.clear();
  _builder.openArray();
  _numValues = 0;
}

void SubqueryEndExecutor::Accumulator::addValue(AqlValue const& value) {
  TRI_ASSERT(_builder.isOpenArray());
  value.toVelocyPack(_options, _builder,
                     /*resolveExternals*/false,
                     /*allowUnindexed*/false);
  ++_numValues;
}

SubqueryEndExecutor::Accumulator::Accumulator(VPackOptions const* const options)
    : _options(options), _builder(_buffer) {
  reset();
}

AqlValueGuard SubqueryEndExecutor::Accumulator::stealValue(AqlValue& result) {
  // Note that an AqlValueGuard holds an AqlValue&, so we cannot create it
  // from a local AqlValue and return the Guard!

  TRI_ASSERT(_builder.isOpenArray());
  _builder.close();
  TRI_ASSERT(_builder.isClosed());

  // Here we have all data *and* the relevant shadow row,
  // so we can now submit
  result = AqlValue{std::move(_buffer)};
  TRI_ASSERT(_buffer.size() == 0);
  _builder.clear();

  // Call reset *after* AqlValueGuard is constructed, so when an exception is
  // thrown, the ValueGuard can free the AqlValue.
  TRI_DEFER(reset());

  return AqlValueGuard{result, true};
}

size_t SubqueryEndExecutor::Accumulator::numValues() const noexcept {
  return _numValues;
}

// We do not write any output for inbound dataRows
// We will only write output for shadowRows. This is accounted for in ExecutionBlockImpl
[[nodiscard]] auto SubqueryEndExecutor::expectedNumberOfRowsNew(AqlItemBlockInputRange const&,
                                                                AqlCall const&) const
    noexcept -> size_t {
  return 0;
}
