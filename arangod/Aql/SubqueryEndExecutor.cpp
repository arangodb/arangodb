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
#include "Basics/ResourceUsage.h"
#include "Basics/ScopeGuard.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <memory>
#include <utility>

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

SubqueryEndExecutorInfos::SubqueryEndExecutorInfos(velocypack::Options const* options,
                                                   arangodb::ResourceMonitor& resourceMonitor,
                                                   RegisterId inReg, RegisterId outReg)
    : _vpackOptions(options),
      _resourceMonitor(resourceMonitor),
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

arangodb::ResourceMonitor& SubqueryEndExecutorInfos::getResourceMonitor() const noexcept {
  return _resourceMonitor;
}

SubqueryEndExecutor::SubqueryEndExecutor(Fetcher&, SubqueryEndExecutorInfos& infos)
    : _infos(infos), 
      _accumulator(_infos.getResourceMonitor(), _infos.vpackOptions()) {}

SubqueryEndExecutor::~SubqueryEndExecutor() = default;

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
  if (_memoryUsage > 0) {
    _resourceMonitor.decreaseMemoryUsage(_memoryUsage);
    _memoryUsage = 0;
  }

  if (_buffer == nullptr) {
    // no Buffer present
    _buffer = std::make_unique<arangodb::velocypack::Buffer<uint8_t>>();
    // we need to recreate the builder even if the old one still exists.
    // this is because the Builder points to the Buffer
    _builder = std::make_unique<VPackBuilder>(*_buffer);
  } else {
    // Buffer still present. we can get away with reusing and clearing 
    // the existing Builder, which points to the Buffer
    TRI_ASSERT(_builder != nullptr);
    _builder->clear();
  }
  TRI_ASSERT(_builder != nullptr);
  _builder->openArray();
  _numValues = 0;
}

void SubqueryEndExecutor::Accumulator::addValue(AqlValue const& value) {
  size_t previousLength = _builder->bufferRef().byteSize();

  TRI_ASSERT(_builder->isOpenArray());
  value.toVelocyPack(_options, *_builder, false);
  ++_numValues;

  size_t currentLength = _builder->bufferRef().byteSize();
  TRI_ASSERT(currentLength >= previousLength);
  
  // per-item overhead (this is because we have to account for the index
  // table entries as well, which are only added later in VelocyPack when
  // the array is closed). this is approximately only, but that should be
  // ok here.
  size_t entryOverhead;
  if (_memoryUsage > 65535) {
    entryOverhead = 4;
  } else if (_memoryUsage > 255) {
    entryOverhead = 2;
  } else {
    entryOverhead = 1;
  }
  size_t diff = currentLength - previousLength + entryOverhead;
  _resourceMonitor.increaseMemoryUsage(diff);
  _memoryUsage += diff;
}

SubqueryEndExecutor::Accumulator::Accumulator(arangodb::ResourceMonitor& resourceMonitor,
                                              VPackOptions const* options) 
    : _resourceMonitor(resourceMonitor), 
      _options(options) {
  reset();
}

SubqueryEndExecutor::Accumulator::~Accumulator() {
  _resourceMonitor.decreaseMemoryUsage(_memoryUsage);
}

AqlValueGuard SubqueryEndExecutor::Accumulator::stealValue(AqlValue& result) {
  // Note that an AqlValueGuard holds an AqlValue&, so we cannot create it
  // from a local AqlValue and return the Guard!

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

// We do not write any output for inbound dataRows
// We will only write output for shadowRows. This is accounted for in ExecutionBlockImpl
[[nodiscard]] auto SubqueryEndExecutor::expectedNumberOfRowsNew(AqlItemBlockInputRange const&,
                                                                AqlCall const&) const
    noexcept -> size_t {
  return 0;
}
