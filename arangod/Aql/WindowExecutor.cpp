////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "WindowExecutor.h"

#include "Aql/Aggregator.h"
#include "Aql/AqlCall.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutionNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
static const AqlValue EmptyValue;
}

WindowExecutorInfos::WindowExecutorInfos(WindowBounds const& bounds, RegisterId rangeRegister,
                                         std::vector<std::string>&& aggregateTypes,
                                         std::vector<std::pair<RegisterId, RegisterId>>&& aggregateRegisters,
                                         QueryWarnings& w, velocypack::Options const* opts)
    : _bounds(bounds),
      _rangeRegister(rangeRegister),
      _aggregateTypes(std::move(aggregateTypes)),
      _aggregateRegisters(std::move(aggregateRegisters)),
      _warnings(w),
      _vpackOptions(opts) {
  TRI_ASSERT(!_aggregateRegisters.empty());
}

WindowBounds const& WindowExecutorInfos::bounds() const { return _bounds; }

RegisterId WindowExecutorInfos::rangeRegister() const { return _rangeRegister; }

std::vector<std::pair<RegisterId, RegisterId>> WindowExecutorInfos::getAggregatedRegisters() const {
  return _aggregateRegisters;
}

std::vector<std::string> WindowExecutorInfos::getAggregateTypes() const {
  return _aggregateTypes;
}

QueryWarnings& WindowExecutorInfos::warnings() const { return _warnings; }

velocypack::Options const* WindowExecutorInfos::getVPackOptions() const {
  return _vpackOptions;
}

BaseWindowExecutor::AggregatorList BaseWindowExecutor::createAggregators(
    WindowExecutor::Infos const& infos) {
  AggregatorList aggregators;

  TRI_ASSERT(!infos.getAggregateTypes().empty());
  if (infos.getAggregateTypes().empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "no aggregators found in WindowExecutor");
  }
  // we do have aggregate registers. create them as empty AqlValues
  aggregators.reserve(infos.getAggregatedRegisters().size());

  // initialize aggregators
  for (auto const& r : infos.getAggregateTypes()) {
    auto factory = Aggregator::factoryFromTypeString(r);
    aggregators.emplace_back((*factory)(infos.getVPackOptions()));
  }

  return aggregators;
}

BaseWindowExecutor::BaseWindowExecutor(Infos& infos)
    : _infos(infos), _aggregators(createAggregators(infos)) {}

BaseWindowExecutor::~BaseWindowExecutor() = default;

const BaseWindowExecutor::Infos& BaseWindowExecutor::infos() const noexcept {
  return _infos;
}

void BaseWindowExecutor::applyAggregators(InputAqlItemRow& input) {
  TRI_ASSERT(_aggregators.size() == _infos.getAggregatedRegisters().size());
  size_t j = 0;
  for (auto const& r : _infos.getAggregatedRegisters()) {
    if (r.second == RegisterPlan::MaxRegisterId) {  // e.g. LENGTH / COUNT
      _aggregators[j]->reduce(::EmptyValue);
    } else {
      _aggregators[j]->reduce(input.getValue(/*inRegister*/ r.second));
    }
    ++j;
  }
}

void BaseWindowExecutor::resetAggregators() {
  for (auto& agg : _aggregators) {
    agg->reset();
  }
}

// produce output row, reset aggregator
void BaseWindowExecutor::produceOutputRow(InputAqlItemRow& input,
                                          OutputAqlItemRow& output, bool reset) {
  size_t j = 0;
  auto const& registers = _infos.getAggregatedRegisters();
  for (std::unique_ptr<Aggregator> const& agg : _aggregators) {
    AqlValue r = agg->get();
    AqlValueGuard guard{r, /*destroy*/ true};
    output.moveValueInto(/*outRegister*/ registers[j++].first, input, guard);
    if (reset) {
      agg->reset();
    }
  }
  output.advanceRow();
}

void BaseWindowExecutor::produceInvalidOutputRow(InputAqlItemRow& input, OutputAqlItemRow& output) {
  VPackSlice const nullSlice = VPackSlice::nullSlice();
  for (auto const& regId : _infos.getAggregatedRegisters()) {
    output.moveValueInto(/*outRegister*/ regId.first, input, nullSlice);
  }
}

// -------------- AccuWindowExecutor --------------

AccuWindowExecutor::AccuWindowExecutor(Fetcher& fetcher, Infos& infos)
    : BaseWindowExecutor(infos) {
  TRI_ASSERT(_infos.bounds().unboundedPreceding());
}

AccuWindowExecutor::~AccuWindowExecutor() = default;

void AccuWindowExecutor::initializeCursor() {
  resetAggregators();
}

std::tuple<ExecutorState, NoStats, AqlCall> AccuWindowExecutor::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  // This block is passhthrough.
  static_assert(Properties::allowsBlockPassthrough == BlockPassthrough::Enable,
                "For WINDOW with passthrough to work, there must be "
                "exactly enough space for all input in the output.");

  // simple optimization for culmulative SUM and the likes
  while (inputRange.hasDataRow()) {
    // So there will always be enough place for all inputRows within
    // the output.
    TRI_ASSERT(!output.isFull());
    auto [state, input] = inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input.isInitialized());
    applyAggregators(input);
    produceOutputRow(input, output, /*reset*/ false);
  }

  // Just fetch everything from above, allow overfetching
  AqlCall upstreamCall{};
  return {inputRange.upstreamState(), NoStats{}, upstreamCall};
}

/**
 * @brief Skip Rows
 *   We need to consume all rows from the inputRange
 *
 * @param inputRange  Data from input
 * @param call Call from client
 * @return std::tuple<ExecutorState, NoStats, size_t, AqlCall>
 */
auto AccuWindowExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
    -> std::tuple<ExecutorState, NoStats, size_t, AqlCall> {
  // we do not keep any state
  AqlCall upstreamCall{};
  return {inputRange.upstreamState(), NoStats{}, call.getSkipCount(), upstreamCall};
}

[[nodiscard]] auto AccuWindowExecutor::expectedNumberOfRowsNew(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept -> size_t {
  if (input.finalState() == ExecutorState::DONE) {
    // For every input row we produce a new row.
    auto estOnInput = input.countDataRows();
    return std::min(call.getLimit(), estOnInput);
  }
  // We do not know how many more rows will be returned from upstream.
  // So we can only overestimate
  return call.getLimit();
}

// -------------- WindowExecutor --------------

WindowExecutor::WindowExecutor(Fetcher& fetcher, Infos& infos)
    : BaseWindowExecutor(infos) {}

WindowExecutor::~WindowExecutor() = default;

ExecutorState WindowExecutor::consumeInputRange(AqlItemBlockInputRange& inputRange) {
  const RegisterId rangeRegister = _infos.rangeRegister();
  QueryWarnings& qc = _infos.warnings();
  WindowBounds const& b = _infos.bounds();

  while (inputRange.hasDataRow()) {
    auto [state, input] = inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input.isInitialized());
    
    if (rangeRegister != RegisterPlan::MaxRegisterId) {
      AqlValue val = input.getValue(rangeRegister);
      _windowRows.emplace_back(b.calcRow(val, qc));
    }
    _rows.emplace_back(std::move(input));
    if (state == ExecutorState::DONE) {
      return state;
    }
  }
  return inputRange.finalState();
}

void WindowExecutor::trimBounds() {
  TRI_ASSERT(!_rows.empty());

  if (_infos.rangeRegister() == RegisterPlan::MaxRegisterId) {
    const size_t numPreceding = size_t(_infos.bounds().numPrecedingRows());
    // trim out of bound rows, _currentIdx may eq _rows.size()
    if (_currentIdx > numPreceding) {
      auto toRemove = _currentIdx - numPreceding;
      // remove elements [0, numPreceding), excluding elem at idx numPreceding
      _rows.erase(_rows.begin(), _rows.begin() + decltype(_rows)::difference_type(toRemove));
      _currentIdx -= toRemove;
    }
    TRI_ASSERT(_currentIdx <= numPreceding || _rows.empty());
    return;
  }
  
  TRI_ASSERT(_rows.size() == _windowRows.size());

  // trim out of bound rows
  while (_currentIdx < _rows.size() &&
         !_windowRows[_currentIdx].valid) {
    _currentIdx++;
  }
  
  if (_currentIdx >= _rows.size() &&
      _windowRows.back().lowBound == _windowRows.back().value) {
    // processed all rows, do not need preceding values
    _rows.clear();
    _windowRows.clear();
    _currentIdx = 0;
    return;
  }
  
  if (_currentIdx == 0) { // nothing lower to remove
    return;
  }
  
  size_t i = std::min(_currentIdx, _rows.size() - 1);
  WindowBounds::Row row = _windowRows[i];
  bool foundLimit = false;
  while (i-- > 0) { // i might underflow, but thats ok
    if (_windowRows[i].value < row.lowBound && _windowRows[i].valid) {
      TRI_ASSERT(_windowRows[i].value < row.highBound);
      foundLimit = true;
      break;
    }
  }
  
  if (foundLimit) {
    TRI_ASSERT(i < _currentIdx);
    _rows.erase(_rows.begin(), _rows.begin() + decltype(_rows)::difference_type(i + 1));
    _windowRows.erase(_windowRows.begin(), _windowRows.begin() + decltype(_windowRows)::difference_type(i + 1));
    _currentIdx -= (i + 1);
  }
}

/**
 * @brief Produce rows.
 *   We need to consume all rows from the inputRange
 *
 * @param inputRange Data from input
 * @param output Where to write the output
 * @return std::tuple<ExecutorState, NoStats, AqlCall>
 */

std::tuple<ExecutorState, NoStats, AqlCall> WindowExecutor::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  ExecutorState state = consumeInputRange(inputRange);

  if (_rows.empty()) {
    return {state, NoStats{}, AqlCall{}};
  }

  if (_infos.rangeRegister() == RegisterPlan::MaxRegisterId) {
    // row based WINDOW

    const size_t numPreceding = size_t(_infos.bounds().numPrecedingRows());
    const size_t numFollowing = size_t(_infos.bounds().numFollowingRows());

    auto haveRows = [&]() -> bool {
      return (state == ExecutorState::DONE && _currentIdx < _rows.size()) ||
             (numPreceding <= _currentIdx &&
              numFollowing + _currentIdx < _rows.size());
    };

    // simon; Fairly inefficient aggregation loop, would need a better
    // Aggregation API allowing removal of values to avoid re-scanning entire range
    while (!output.isFull() && haveRows()) {
      size_t start = _currentIdx > numPreceding ? _currentIdx - numPreceding : 0;
      size_t end = std::min(_rows.size(), _currentIdx + numFollowing + 1);

      while (start != end) {
        applyAggregators(_rows[start]);
        start++;
      }
      produceOutputRow(_rows[_currentIdx], output, /*reset*/ true);
      _currentIdx++;
    }

    trimBounds();
    
  } else {  // range based WINDOW

    TRI_ASSERT(_rows.size() == _windowRows.size());

    // fairly inefficient loop, see comment above
    size_t offset = 0;
    while (!output.isFull() && _currentIdx < _rows.size()) {
      auto const& row = _windowRows[_currentIdx];
      if (!row.valid) {
        produceInvalidOutputRow(_rows[_currentIdx], output);
        _currentIdx++;
        continue;
      }
      
      size_t i = offset;
      bool foundLimit = false;
      for (; i < _windowRows.size(); i++) {
        if (!row.valid) {
          continue; // skip
        }
        
        if (row.lowBound <= _windowRows[i].value) {
          if (row.highBound < _windowRows[i].value) {
            foundLimit = true;
            break;  // do not consider higher values
          }
          applyAggregators(_rows[size_t(i)]);
        } else {
          // lower index have _windowRows[i].value < row.lowBound
          offset = i + 1;
        }
      }
      
      if (foundLimit || state == ExecutorState::DONE) {
        produceOutputRow(_rows[_currentIdx], output, /*reset*/ true);
        _currentIdx++;
        continue;
      }
      TRI_ASSERT(state == ExecutorState::HASMORE);
      resetAggregators();
      break; // need more data from upstream
    }

    trimBounds();
  }
  
  if (_currentIdx < _rows.size()) {
    state = ExecutorState::HASMORE;
  }

  return {state, NoStats{}, AqlCall{}};
}

/**
 * @brief Skip Rows
 *   We need to consume all rows from the inputRange
 *
 * @param inputRange  Data from input
 * @param call Call from client
 * @return std::tuple<ExecutorState, NoStats, size_t, AqlCall>
 */
std::tuple<ExecutorState, NoStats, size_t, AqlCall> WindowExecutor::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& call) {
  TRI_ASSERT(_currentIdx < _rows.size());

  std::ignore = consumeInputRange(inputRange);

  if (!_rows.empty()) {
    // TODO a bit loopy
    while (call.needSkipMore() && _currentIdx < _windowRows.size()) {
      _currentIdx++;
      call.didSkip(1);
    }

    trimBounds();
  }

  ExecutorState state = inputRange.upstreamState();
  if (_currentIdx < _rows.size()) {
    state = ExecutorState::HASMORE;
  }
  
  // Just fetch everything from above, allow overfetching
  AqlCall upstreamCall{};
  return {state, NoStats{}, call.getSkipCount(), upstreamCall};
}

[[nodiscard]] auto WindowExecutor::expectedNumberOfRowsNew(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept -> size_t {
  if (input.finalState() == ExecutorState::DONE) {
    size_t remain = _currentIdx < _rows.size() ? _rows.size() - _currentIdx : 0;
    remain += input.countDataRows();
    return std::min(call.getLimit(), remain);
  }
  // We do not know how many more rows will be returned from upstream.
  // So we can only overestimate
  return call.getLimit();
}
