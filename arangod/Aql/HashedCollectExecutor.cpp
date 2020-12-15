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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "HashedCollectExecutor.h"

#include "Aql/Aggregator.h"
#include "Aql/AqlCall.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutionNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/ResourceUsage.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

static const AqlValue EmptyValue;

HashedCollectExecutorInfos::HashedCollectExecutorInfos(
    std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters,
    RegisterId collectRegister, std::vector<std::string>&& aggregateTypes,
    std::vector<std::pair<RegisterId, RegisterId>>&& aggregateRegisters,
    velocypack::Options const* opts, 
    arangodb::ResourceMonitor& resourceMonitor,
    bool count)
    : _aggregateTypes(aggregateTypes),
      _aggregateRegisters(aggregateRegisters),
      _groupRegisters(std::move(groupRegisters)),
      _collectRegister(collectRegister),
      _vpackOptions(opts),
      _resourceMonitor(resourceMonitor),
      _count(count) {
  TRI_ASSERT(!_groupRegisters.empty());
}

std::vector<std::pair<RegisterId, RegisterId>> HashedCollectExecutorInfos::getGroupRegisters() const {
  return _groupRegisters;
}

std::vector<std::pair<RegisterId, RegisterId>> HashedCollectExecutorInfos::getAggregatedRegisters() const {
  return _aggregateRegisters;
}

std::vector<std::string> HashedCollectExecutorInfos::getAggregateTypes() const {
  return _aggregateTypes;
}

velocypack::Options const* HashedCollectExecutorInfos::getVPackOptions() const {
  return _vpackOptions;
}

bool HashedCollectExecutorInfos::getCount() const noexcept { return _count; }

RegisterId HashedCollectExecutorInfos::getCollectRegister() const noexcept {
  return _collectRegister;
}

arangodb::ResourceMonitor& HashedCollectExecutorInfos::getResourceMonitor() const {
  return _resourceMonitor;
}

std::vector<Aggregator::Factory>
HashedCollectExecutor::createAggregatorFactories(HashedCollectExecutor::Infos const& infos) {
  std::vector<Aggregator::Factory> aggregatorFactories;

  if (infos.getAggregateTypes().empty()) {
    // no aggregate registers. this means we'll only count the number of items
    if (infos.getCount()) {
      aggregatorFactories.emplace_back(
          Aggregator::factoryFromTypeString("LENGTH"));
    }
  } else {
    // we do have aggregate registers. create them as empty AqlValues
    aggregatorFactories.reserve(infos.getAggregatedRegisters().size());

    // initialize aggregators
    for (auto const& r : infos.getAggregateTypes()) {
      aggregatorFactories.emplace_back(Aggregator::factoryFromTypeString(r));
    }
  }

  return aggregatorFactories;
}

HashedCollectExecutor::HashedCollectExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _lastInitializedInputRow(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _allGroups(1024,
                 AqlValueGroupHash(_infos.getGroupRegisters().size()),
                 AqlValueGroupEqual(_infos.getVPackOptions())),
      _isInitialized(false),
      _aggregatorFactories() {
  _aggregatorFactories = createAggregatorFactories(_infos);
  _nextGroupValues.reserve(_infos.getGroupRegisters().size());
};

HashedCollectExecutor::~HashedCollectExecutor() {
  // Generally, _allGroups should be empty when the block is destroyed - except
  // when an exception is thrown during getOrSkipSome, in which case the
  // AqlValue ownership hasn't been transferred.
  destroyAllGroupsAqlValues();
}

void HashedCollectExecutor::destroyAllGroupsAqlValues() {
  size_t memoryUsage = 0;
  for (auto& it : _allGroups) {
    memoryUsage += memoryUsageForGroup(it.first, true);
    for (auto& it2 : it.first) {
      const_cast<AqlValue*>(&it2)->destroy();
    }
  }
  _infos.getResourceMonitor().decreaseMemoryUsage(memoryUsage);
}

void HashedCollectExecutor::consumeInputRow(InputAqlItemRow& input) {
  TRI_ASSERT(input.isInitialized());

  decltype(_allGroups)::iterator currentGroupIt = findOrEmplaceGroup(input);

  // reduce the aggregates
  AggregateValuesType* aggregateValues = currentGroupIt->second.get();

  if (_infos.getAggregateTypes().empty()) {
    // no aggregate registers. simply increase the counter
    if (_infos.getCount()) {
      // TODO get rid of this special case if possible
      TRI_ASSERT(!aggregateValues->empty());
      aggregateValues->back()->reduce(EmptyValue);
    }
  } else {
    // apply the aggregators for the group
    TRI_ASSERT(aggregateValues->size() == _infos.getAggregatedRegisters().size());
    size_t j = 0;
    for (auto const& r : _infos.getAggregatedRegisters()) {
      if (r.second == RegisterPlan::MaxRegisterId) {
        (*aggregateValues)[j]->reduce(EmptyValue);
      } else {
        (*aggregateValues)[j]->reduce(input.getValue(r.second));
      }
      ++j;
    }
  }
}

void HashedCollectExecutor::writeCurrentGroupToOutput(OutputAqlItemRow& output) {
  // build the result
  TRI_ASSERT(!_infos.getCount() || _infos.getCollectRegister() != RegisterPlan::MaxRegisterId);

  auto& keys = _currentGroup->first;
  TRI_ASSERT(_currentGroup->second != nullptr);

  TRI_ASSERT(keys.size() == _infos.getGroupRegisters().size());
  size_t memoryUsage = memoryUsageForGroup(keys, false);
  size_t i = 0;
  for (auto& it : keys) {
    AqlValue& key = *const_cast<AqlValue*>(&it);
    AqlValueGuard guard{key, true};
    output.moveValueInto(_infos.getGroupRegisters()[i++].first,
                         _lastInitializedInputRow, guard);
    key.erase();  // to prevent double-freeing later
  }

  _infos.getResourceMonitor().decreaseMemoryUsage(memoryUsage);

  if (!_infos.getCount()) {
    TRI_ASSERT(_currentGroup->second->size() == _infos.getAggregatedRegisters().size());
    size_t j = 0;
    for (auto const& it : *(_currentGroup->second)) {
      AqlValue r = it->stealValue();
      AqlValueGuard guard{r, true};
      output.moveValueInto(_infos.getAggregatedRegisters()[j++].first,
                           _lastInitializedInputRow, guard);
    }
  } else {
    // set group count in result register
    TRI_ASSERT(!_currentGroup->second->empty());
    AqlValue r = _currentGroup->second->back()->stealValue();
    AqlValueGuard guard{r, true};
    output.moveValueInto(_infos.getCollectRegister(), _lastInitializedInputRow, guard);
  }
}

auto HashedCollectExecutor::consumeInputRange(AqlItemBlockInputRange& inputRange) -> bool {
  TRI_ASSERT(!_isInitialized);
  do {
    auto [state, input] = inputRange.nextDataRow();
    if (input) {
      consumeInputRow(input);
      // We need to retain this
      _lastInitializedInputRow = input;
    }
    if (state == ExecutorState::DONE) {
      // initialize group iterator for output
      _currentGroup = _allGroups.begin();
      return true;
    }
  } while (inputRange.hasDataRow());

  TRI_ASSERT(inputRange.upstreamState() == ExecutorState::HASMORE);
  return false;
}

auto HashedCollectExecutor::returnState() const -> ExecutorState {
  if (!_isInitialized || _currentGroup != _allGroups.end()) {
    // We have either not started, or not produce all groups.
    return ExecutorState::HASMORE;
  }
  return ExecutorState::DONE;
}

/**
 * @brief Produce rows.
 *   We need to consume all rows from the inputRange, except the
 *   last Row. This is to indicate that this executor is not yet done.
 *   Afterwards we write all groups into the output.
 *   Only if we have written the last group we consume
 *   the remaining inputRow. This is to indicate that
 *   this executor cannot produce anymore.
 *
 * @param inputRange Data from input
 * @param output Where to write the output
 * @return std::tuple<ExecutorState, NoStats, AqlCall>
 */

auto HashedCollectExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                        OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, NoStats, AqlCall> {
  TRI_IF_FAILURE("HashedCollectExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  if (!_isInitialized) {
    // Consume the input range
    _isInitialized = consumeInputRange(inputRange);
  }

  if (_isInitialized) {
    while (_currentGroup != _allGroups.end() && !output.isFull()) {
      writeCurrentGroupToOutput(output);
      ++_currentGroup;
      ++_returnedGroups;
      output.advanceRow();
    }
  }

  AqlCall upstreamCall{};
  // We cannot forward anything, no skip, no limit.
  // Need to request all from upstream.
  return {returnState(), NoStats{}, upstreamCall};
}

/**
 * @brief Skip Rows
 *   We need to consume all rows from the inputRange, except the
 *   last Row. This is to indicate that this executor is not yet done.
 *   Afterwards we skip all groups.
 *   Only if we have skipped the last group we consume
 *   the remaining inputRow. This is to indicate that
 *   this executor cannot produce anymore.
 *
 * @param inputRange  Data from input
 * @param call Call from client
 * @return std::tuple<ExecutorState, NoStats, size_t, AqlCall>
 */
auto HashedCollectExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
    -> std::tuple<ExecutorState, NoStats, size_t, AqlCall> {
  if (!_isInitialized) {
    // Consume the input range
    _isInitialized = consumeInputRange(inputRange);
  }

  if (_isInitialized) {
    while (_currentGroup != _allGroups.end() && call.needSkipMore()) {
      ++_currentGroup;
      call.didSkip(1);
    }
  }

  AqlCall upstreamCall{};
  // We cannot forward anything, no skip, no limit.
  // Need to request all from upstream.
  return {returnState(), NoStats{}, call.getSkipCount(), upstreamCall};
}

// finds the group matching the current row, or emplaces it. in either case,
// it returns an iterator to the group matching the current row in
// _allGroups. additionally, .second is true iff a new group was emplaced.
decltype(HashedCollectExecutor::_allGroups)::iterator HashedCollectExecutor::findOrEmplaceGroup(
    InputAqlItemRow& input) {
  _nextGroupValues.clear();

  // for hashing simply re-use the aggregate registers, without cloning
  // their contents
  for (auto const& reg : _infos.getGroupRegisters()) {
    _nextGroupValues.emplace_back(input.getValue(reg.second));
  }

  auto it = _allGroups.find(_nextGroupValues);

  if (it != _allGroups.end()) {
    // group already exists
    return it;
  }

  _nextGroupValues.clear();

  if (_infos.getGroupRegisters().size() == 1) {
    for (auto const& reg : _infos.getGroupRegisters()) {
      // On a single register there can be no duplicate value
      // inside the groupValues, so we cannot get into a situation
      // where it is unclear who is responsible for the data
      AqlValue a = input.stealValue(reg.second);
      AqlValueGuard guard{a, true};
      _nextGroupValues.emplace_back(a);
      guard.steal();
    }
  } else {
    for (auto const& reg : _infos.getGroupRegisters()) {
      // With more then 1 register we cannot reliably figure out who
      // is responsible for which value.
      // E.g. for 2 registers we have two groups: A , 1 and A , 2
      // Now A can be from different input blocks, and can be also
      // be written to different output blocks, or even not handed over
      // because of a limit. So we simply clone A here on every new group.
      // So this block is responsible for every grouped tuple, until it
      // is handed over to the output block. There is no overlapping
      // of responsibilities of tuples.
      AqlValue a = input.getValue(reg.second).clone();
      AqlValueGuard guard{a, true};
      _nextGroupValues.emplace_back(a);
      guard.steal();
    }
  }

  // this builds a new group with aggregate functions being prepared.
  auto aggregateValues = std::make_unique<AggregateValuesType>();
  aggregateValues->reserve(_aggregatorFactories.size());
  auto* vpackOpts = _infos.getVPackOptions();
  for (auto const& factory : _aggregatorFactories) {
    aggregateValues->emplace_back((*factory)(vpackOpts));
  }

  ResourceUsageScope guard(_infos.getResourceMonitor(), memoryUsageForGroup(_nextGroupValues, true));

  // note: aggregateValues may be a nullptr!
  auto [result, emplaced] =
      _allGroups.try_emplace(std::move(_nextGroupValues), std::move(aggregateValues));
  // emplace must not fail
  TRI_ASSERT(emplaced);

  // memory now owned by _allGroups
  guard.steal();

  // Moving _nextGroupValues left us with an empty vector of minimum capacity.
  // So in order to have correct capacity reserve again.
  _nextGroupValues.clear();
  _nextGroupValues.reserve(_infos.getGroupRegisters().size());

  return result;
};

[[nodiscard]] auto HashedCollectExecutor::expectedNumberOfRowsNew(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept -> size_t {
  if (!_isInitialized) {
    if (input.finalState() == ExecutorState::DONE) {
      // Worst case assumption:
      // For every input row we have a new group.
      // We will never produce more then asked for
      auto estOnInput = input.countDataRows();
      if (estOnInput == 0 && _infos.getGroupRegisters().empty()) {
        // Special case, on empty input we will produce 1 output
        estOnInput = 1;
      }
      return std::min(call.getLimit(), estOnInput);
    }
    // Otherwise we do not know.
    return call.getLimit();
  }
  // We know how many groups we have left
  TRI_ASSERT(_returnedGroups <= _allGroups.size());
  return std::min<size_t>(call.getLimit(), _allGroups.size() - _returnedGroups);
}

HashedCollectExecutor::Infos const& HashedCollectExecutor::infos() const noexcept {
  return _infos;
}

size_t HashedCollectExecutor::memoryUsageForGroup(GroupKeyType const& group, bool withBase) const {
  // track memory usage of unordered_map entry (somewhat)
  size_t memoryUsage = 0;
  if (withBase) {
    memoryUsage += 4 * sizeof(void*) + /* generic overhead */
                   group.size() * sizeof(AqlValue) + 
                   _aggregatorFactories.size() * sizeof(void*);
  }

  for (auto const& it : group) {
    if (it.requiresDestruction()) {
      memoryUsage += it.memoryUsage();
    }
  }
  return memoryUsage;
}
