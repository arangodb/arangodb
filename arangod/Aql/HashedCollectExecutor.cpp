////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
    arangodb::ResourceMonitor& resourceMonitor)
    : _aggregateTypes(aggregateTypes),
      _aggregateRegisters(aggregateRegisters),
      _groupRegisters(std::move(groupRegisters)),
      _collectRegister(collectRegister),
      _vpackOptions(opts),
      _resourceMonitor(resourceMonitor) {
  TRI_ASSERT(!_groupRegisters.empty());
}

std::vector<std::pair<RegisterId, RegisterId>> const& HashedCollectExecutorInfos::getGroupRegisters() const {
  return _groupRegisters;
}

std::vector<std::pair<RegisterId, RegisterId>> const& HashedCollectExecutorInfos::getAggregatedRegisters() const {
  return _aggregateRegisters;
}

std::vector<std::string> const& HashedCollectExecutorInfos::getAggregateTypes() const {
  return _aggregateTypes;
}

velocypack::Options const* HashedCollectExecutorInfos::getVPackOptions() const {
  return _vpackOptions;
}

RegisterId HashedCollectExecutorInfos::getCollectRegister() const noexcept {
  return _collectRegister;
}

arangodb::ResourceMonitor& HashedCollectExecutorInfos::getResourceMonitor() const {
  return _resourceMonitor;
}

std::vector<Aggregator::Factory const*>
HashedCollectExecutor::createAggregatorFactories(HashedCollectExecutor::Infos const& infos) {
  std::vector<Aggregator::Factory const*> aggregatorFactories;

  if (!infos.getAggregateTypes().empty()) {
    // we do have aggregate registers. create them as empty AqlValues
    aggregatorFactories.reserve(infos.getAggregatedRegisters().size());

    // initialize aggregators
    for (auto const& r : infos.getAggregateTypes()) {
      aggregatorFactories.emplace_back(&Aggregator::factoryFromTypeString(r));
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
  _nextGroup.values.reserve(_infos.getGroupRegisters().size());
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
    for (auto& it2 : it.first.values) {
      const_cast<AqlValue*>(&it2)->destroy();
    }
  }
  _infos.getResourceMonitor().decreaseMemoryUsage(memoryUsage);
}

void HashedCollectExecutor::consumeInputRow(InputAqlItemRow& input) {
  TRI_ASSERT(input.isInitialized());
  
  decltype(_allGroups)::iterator currentGroupIt = findOrEmplaceGroup(input);

  if (!_infos.getAggregateTypes().empty()) {
    // reduce the aggregates
    ValueAggregators* aggregateValues = currentGroupIt->second.get();

    // apply the aggregators for the group
    TRI_ASSERT(aggregateValues != nullptr && aggregateValues->size() == _infos.getAggregatedRegisters().size());
    size_t j = 0;
    for (auto const& r : _infos.getAggregatedRegisters()) {
      if (r.second.value() == RegisterId::maxRegisterId) {
        (*aggregateValues)[j].reduce(EmptyValue);
      } else {
        (*aggregateValues)[j].reduce(input.getValue(r.second));
      }
      ++j;
    }
  }
}

void HashedCollectExecutor::writeCurrentGroupToOutput(OutputAqlItemRow& output) {
  // build the result
  size_t memoryUsage = memoryUsageForGroup(_currentGroup->first, false);
  auto& keys = _currentGroup->first.values;

  TRI_ASSERT(keys.size() == _infos.getGroupRegisters().size());
  size_t i = 0;
  for (auto& it : keys) {
    AqlValue& key = *const_cast<AqlValue*>(&it);
    AqlValueGuard guard{key, true};
    output.moveValueInto(_infos.getGroupRegisters()[i++].first,
                         _lastInitializedInputRow, guard);
    key.erase();  // to prevent double-freeing later
  }


  _infos.getResourceMonitor().decreaseMemoryUsage(memoryUsage);

  if (!_infos.getAggregatedRegisters().empty()) {
    TRI_ASSERT(_currentGroup->second != nullptr);
    auto& aggregators = *_currentGroup->second;
    TRI_ASSERT(aggregators.size() == _infos.getAggregatedRegisters().size());
    size_t j = 0;
    for (std::size_t aggregatorIdx = 0; aggregatorIdx < aggregators.size(); ++aggregatorIdx) {
      AqlValue r = aggregators[aggregatorIdx].stealValue();
      AqlValueGuard guard{r, true};
      output.moveValueInto(_infos.getAggregatedRegisters()[j++].first,
                          _lastInitializedInputRow, guard);
    }
  }
}

auto HashedCollectExecutor::consumeInputRange(AqlItemBlockInputRange& inputRange) -> bool {
  TRI_ASSERT(!_isInitialized);
  do {
    auto [state, input] = inputRange.nextDataRow();
    if (input) {
      consumeInputRow(input);
      // We need to retain this
      _lastInitializedInputRow = std::move(input);
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
  _nextGroup.values.clear();
  TRI_ASSERT(_nextGroup.values.capacity() == _infos.getGroupRegisters().size());

  // for hashing simply re-use the aggregate registers, without cloning
  // their contents
  for (auto const& reg : _infos.getGroupRegisters()) {
    _nextGroup.values.emplace_back(input.getValue(reg.second));
  }

  AqlValueGroupHash hasher(_nextGroup.values.size());
  _nextGroup.hash = hasher(_nextGroup.values);

  auto it = _allGroups.find(_nextGroup);
  if (it != _allGroups.end()) {
    // group already exists
    return it;
  }

  _nextGroup.values.clear();

  if (_infos.getGroupRegisters().size() == 1) {
    auto const& reg = _infos.getGroupRegisters().back();
    // On a single register there can be no duplicate value
    // inside the groupValues, so we cannot get into a situation
    // where it is unclear who is responsible for the data
    AqlValue a = input.stealValue(reg.second);
    AqlValueGuard guard{a, true};
    _nextGroup.values.emplace_back(a);
    guard.steal();
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
      _nextGroup.values.emplace_back(a);
      guard.steal();
    }
  }
  TRI_ASSERT(_nextGroup.hash == hasher(_nextGroup.values));

  // this builds a new group with aggregate functions being prepared.
  auto aggregateValues = makeAggregateValues();

  ResourceUsageScope guard(_infos.getResourceMonitor(), memoryUsageForGroup(_nextGroup, true));

  // note: aggregateValues may be a nullptr!
  auto [result, emplaced] =
      _allGroups.try_emplace(std::move(_nextGroup), std::move(aggregateValues));
  // emplace must not fail
  TRI_ASSERT(emplaced);

  guard.steal();

  // Moving _nextGroup left us with an empty vector of minimum capacity.
  // So in order to have correct capacity reserve again.
  _nextGroup.values.reserve(_infos.getGroupRegisters().size());

  return result;
}

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
                   group.values.size() * sizeof(AqlValue) + 
                   _aggregatorFactories.size() * sizeof(void*);
  }

  for (auto const& it : group.values) {
    if (it.requiresDestruction()) {
      memoryUsage += it.memoryUsage();
    }
  }
  return memoryUsage;
}

std::unique_ptr<HashedCollectExecutor::ValueAggregators> HashedCollectExecutor::makeAggregateValues() const {
  if (_aggregatorFactories.empty()) {
    return {};
  }
  std::size_t size = sizeof(ValueAggregators) + sizeof(Aggregator*) * _aggregatorFactories.size();
  for (auto factory : _aggregatorFactories) {
    size += factory->getAggregatorSize();
  }
  void* p = ::operator new(size);
  new (p) ValueAggregators(_aggregatorFactories, _infos.getVPackOptions());
  return std::unique_ptr<ValueAggregators>(static_cast<ValueAggregators*>(p));
}

HashedCollectExecutor::ValueAggregators::ValueAggregators(std::vector<Aggregator::Factory const*> factories, velocypack::Options const* opts) 
    : _size(factories.size()) {
  TRI_ASSERT(!factories.empty());
  auto* aggregatorPointers = reinterpret_cast<Aggregator**>(this + 1);
  void* aggregators = aggregatorPointers + _size;
  for (auto factory : factories) {
    factory->createInPlace(aggregators, opts);
    *aggregatorPointers = static_cast<Aggregator*>(aggregators);
    ++aggregatorPointers;
    aggregators = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(aggregators) + factory->getAggregatorSize());
  }
}

HashedCollectExecutor::ValueAggregators::~ValueAggregators() {
  for (std::size_t i = 0; i < _size; ++i) {
    (*this)[i].~Aggregator();
  }
}

std::size_t HashedCollectExecutor::ValueAggregators::size() const {
  return _size;
}

Aggregator& HashedCollectExecutor::ValueAggregators::operator[](std::size_t index) {
  TRI_ASSERT(index < _size);
  return *(reinterpret_cast<Aggregator**>(this + 1)[index]);
}

void HashedCollectExecutor::ValueAggregators::operator delete(void* ptr) {
  ::operator delete(ptr);
}
