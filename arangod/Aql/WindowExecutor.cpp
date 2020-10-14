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

static const AqlValue EmptyValue;

WindowExecutorInfos::WindowExecutorInfos(WindowRange const& range, RegisterId rangeRegister,
                                         std::vector<std::string>&& aggregateTypes,
                                         std::vector<std::pair<RegisterId, RegisterId>>&& aggregateRegisters,
                                         velocypack::Options const* opts)
    : _range(range),
      _aggregateTypes(aggregateTypes),
      _aggregateRegisters(aggregateRegisters),
      _vpackOptions(opts),
      _rangeRegister(rangeRegister) {
  TRI_ASSERT(!aggregateRegisters.empty());
}

WindowRange const& WindowExecutorInfos::range() const { return _range; }

RegisterId WindowExecutorInfos::rangeRegister() const { return _rangeRegister; }

std::vector<std::pair<RegisterId, RegisterId>> WindowExecutorInfos::getAggregatedRegisters() const {
  return _aggregateRegisters;
}

std::vector<std::string> WindowExecutorInfos::getAggregateTypes() const {
  return _aggregateTypes;
}

velocypack::Options const* WindowExecutorInfos::getVPackOptions() const {
  return _vpackOptions;
}

BaseWindowExecutor::AggregatorList BaseWindowExecutor::createAggregators(
    WindowExecutor::Infos const& infos) {
  AggregatorList aggregators;

  TRI_ASSERT(!infos.getAggregateTypes().empty());
  if (infos.getAggregateTypes().empty()) {
    // no aggregate registers. this means we'll only count the number of items
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
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
    : _infos(infos), _aggregators(createAggregators(infos)){};

BaseWindowExecutor::~BaseWindowExecutor() {}

[[nodiscard]] auto BaseWindowExecutor::expectedNumberOfRowsNew(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept -> size_t {
  if (input.finalState() == ExecutorState::DONE) {
    return std::min(call.getLimit(), input.countDataRows());
  }
  // We do not know how many more rows will be returned from upstream.
  // So we can only overestimate
  return call.getLimit();
}

const BaseWindowExecutor::Infos& BaseWindowExecutor::infos() const noexcept {
  return _infos;
}

void BaseWindowExecutor::applyAggregators(InputAqlItemRow& input) {
  TRI_ASSERT(_aggregators.size() == _infos.getAggregatedRegisters().size());
  size_t j = 0;
  for (auto const& r : _infos.getAggregatedRegisters()) {
    if (r.second == RegisterPlan::MaxRegisterId) {  // e.g. LENGTH / COUNT
      _aggregators[j]->reduce(EmptyValue);
    } else {
      _aggregators[j]->reduce(input.getValue(/*inRegister*/ r.second));
    }
    ++j;
  }
}

// produce output row, reset aggregator
void BaseWindowExecutor::produceOutputRow(InputAqlItemRow& input, OutputAqlItemRow& output, bool reset) {
  size_t j = 0;
  auto const& registers = _infos.getAggregatedRegisters();
  for (std::unique_ptr<Aggregator> const& agg : _aggregators) {
    AqlValue r = agg->get();
    AqlValueGuard guard{r, true};
    output.moveValueInto(/*outRegister*/ registers[j++].first, input, guard);
    if (reset) {
      agg->reset();
    }
  }
  output.advanceRow();
}

// -------------- AccuWindowExecutor --------------

AccuWindowExecutor::AccuWindowExecutor(Fetcher& fetcher, Infos& infos)
    : BaseWindowExecutor(infos) {
  if (infos.rangeRegister() != RegisterPlan::MaxRegisterId) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
};

AccuWindowExecutor::~AccuWindowExecutor() {}

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
    produceOutputRow(input, output, /*reset*/false);
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

// -------------- WindowExecutor --------------

WindowExecutor::WindowExecutor(Fetcher& fetcher, Infos& infos)
    : BaseWindowExecutor(infos),
      _numPrecedingRows(infos.rangeRegister() == RegisterPlan::MaxRegisterId
                            ? infos.range().preceding.toInt64()
                            : 0),
      _numFollowingRows(infos.rangeRegister() == RegisterPlan::MaxRegisterId
                            ? infos.range().following.toInt64()
                            : 0) {
  TRI_ASSERT(_numPrecedingRows >= 0);
  TRI_ASSERT(_numFollowingRows >= 0);
  if (infos.rangeRegister() != RegisterPlan::MaxRegisterId) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
};

WindowExecutor::~WindowExecutor() {}

ExecutorState WindowExecutor::consumeInputRange(AqlItemBlockInputRange& inputRange) {
  ExecutorState state = ExecutorState::DONE;
  while (inputRange.hasDataRow() /* && !output.isFull()*/) {
    auto [state, input] = inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input.isInitialized());
    _rows.push_back(input);
    if (_currentIdx < 0) {
      _currentIdx = 0;
    }
  }
  return state;
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

  if (_infos.rangeRegister() == RegisterPlan::MaxRegisterId) {
    if (!_rows.empty()) {
      TRI_ASSERT(_currentIdx >= 0);
      TRI_ASSERT(_numPrecedingRows >= 0);
      TRI_ASSERT(_numFollowingRows >= 0);

      auto haveRows = [&]() -> bool {
        return state == ExecutorState::DONE ||
               (_numPrecedingRows <= _currentIdx &&
                int64_t(_rows.size()) - _currentIdx < _numFollowingRows);
      };

      while (!output.isFull() && haveRows()) {
        int64_t start = std::max(int64_t(0), _currentIdx - _numPrecedingRows);
        int64_t end =
            std::min(int64_t(_rows.size()), _currentIdx + _numFollowingRows + 1);

        while (start != end) {
          applyAggregators(_rows[size_t(start)]);
          start++;
        }
        produceOutputRow(_rows[size_t(_currentIdx)], output, /*reset*/true);
        if (size_t(++_currentIdx) == _rows.size()) {
          break;
        }
      }

      // trim out of bound rows
      while (_currentIdx > _numPrecedingRows) {
        _rows.pop_front();
        _currentIdx--;
      }
    }

    TRI_ASSERT(_currentIdx >= 0 || _rows.empty());
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
auto WindowExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
    -> std::tuple<ExecutorState, NoStats, size_t, AqlCall> {
  TRI_ASSERT(_currentIdx >= 0 && size_t(_currentIdx) < _rows.size());

  std::ignore = consumeInputRange(inputRange);

  // FIXME probably does not really need to be a loop
  while (call.needSkipMore() && !_rows.empty()) {
    if (_currentIdx > _numPrecedingRows) {
      _rows.pop_front();
      _currentIdx--;
    }
    _currentIdx++;
    call.didSkip(1);
  }
  if (_rows.empty()) {
    _currentIdx = -1;
  }

  AqlCall upstreamCall{};
  return {inputRange.upstreamState(), NoStats{}, call.getSkipCount(), upstreamCall};
}
