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

#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"

#include <lib/Logger/LogMacros.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

static const AqlValue EmptyValue;

HashedCollectExecutorInfos::HashedCollectExecutorInfos(
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep,
    std::unordered_set<RegisterId>&& readableInputRegisters,
    std::unordered_set<RegisterId>&& writeableOutputRegisters,
    std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters,
    RegisterId collectRegister, std::vector<std::string>&& aggregateTypes,
    std::vector<std::pair<RegisterId, RegisterId>>&& aggregateRegisters,
    transaction::Methods* trxPtr, bool count)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(readableInputRegisters),
                    std::make_shared<std::unordered_set<RegisterId>>(writeableOutputRegisters),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _aggregateTypes(aggregateTypes),
      _aggregateRegisters(aggregateRegisters),
      _groupRegisters(groupRegisters),
      _collectRegister(collectRegister),
      _count(count),
      _trxPtr(trxPtr) {
  TRI_ASSERT(!_groupRegisters.empty());
}

std::vector<std::function<std::unique_ptr<Aggregator>(transaction::Methods*)> const*>
HashedCollectExecutor::createAggregatorFactories(HashedCollectExecutor::Infos const& infos) {
  std::vector<std::function<std::unique_ptr<Aggregator>(transaction::Methods*)> const*> aggregatorFactories;

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
      _fetcher(fetcher),
      _upstreamState(ExecutionState::HASMORE),
      _lastInitializedInputRow(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _allGroups(1024,
                 AqlValueGroupHash(_infos.getTransaction(),
                                   _infos.getGroupRegisters().size()),
                 AqlValueGroupEqual(_infos.getTransaction())),
      _isInitialized(false),
      _aggregatorFactories(),
      _returnedGroups(0) {
  _aggregatorFactories = createAggregatorFactories(_infos);
};

HashedCollectExecutor::~HashedCollectExecutor() {
  // Generally, _allGroups should be empty when the block is destroyed - except
  // when an exception is thrown during getOrSkipSome, in which case the
  // AqlValue ownership hasn't been transferred.
  destroyAllGroupsAqlValues();
}

void HashedCollectExecutor::destroyAllGroupsAqlValues() {
  for (auto& it : _allGroups) {
    for (auto& it2 : it.first) {
      const_cast<AqlValue*>(&it2)->destroy();
    }
  }
}

void HashedCollectExecutor::consumeInputRow(InputAqlItemRow& input) {
  TRI_ASSERT(input.isInitialized());

  decltype(_allGroups)::iterator currentGroupIt;
  currentGroupIt = findOrEmplaceGroup(input);

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
      if (r.second == ExecutionNode::MaxRegisterId) {
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
  TRI_ASSERT(!_infos.getCount() || _infos.getCollectRegister() != ExecutionNode::MaxRegisterId);

  auto& keys = _currentGroup->first;
  TRI_ASSERT(_currentGroup->second != nullptr);

  TRI_ASSERT(keys.size() == _infos.getGroupRegisters().size());
  size_t i = 0;
  for (auto& it : keys) {
    AqlValue& key = *const_cast<AqlValue*>(&it);
    AqlValueGuard guard{key, true};
    output.moveValueInto(_infos.getGroupRegisters()[i++].first,
                         _lastInitializedInputRow, guard);
    key.erase();  // to prevent double-freeing later
  }

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

ExecutionState HashedCollectExecutor::init() {
  TRI_ASSERT(!_isInitialized);

  // fetch & consume all input
  while (_upstreamState != ExecutionState::DONE) {
    InputAqlItemRow input = InputAqlItemRow{CreateInvalidInputRowHint{}};
    std::tie(_upstreamState, input) = _fetcher.fetchRow();

    if (_upstreamState == ExecutionState::WAITING) {
      TRI_ASSERT(!input.isInitialized());
      return ExecutionState::WAITING;
    }

    // !input.isInitialized() => _upstreamState == ExecutionState::DONE
    TRI_ASSERT(input.isInitialized() || _upstreamState == ExecutionState::DONE);

    // needed to remember the last valid input aql item row
    // NOTE: this might impact the performance
    if (input.isInitialized()) {
      _lastInitializedInputRow = input;

      consumeInputRow(input);
    }
  }

  // initialize group iterator for output
  _currentGroup = _allGroups.begin();

  return ExecutionState::DONE;
}

std::pair<ExecutionState, NoStats> HashedCollectExecutor::produceRow(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("HashedCollectExecutor::produceRow") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (!_isInitialized) {
    // fetch & consume all input and initialize output cursor
    ExecutionState state = init();
    if (state == ExecutionState::WAITING) {
      return {state, NoStats{}};
    }
    TRI_ASSERT(state == ExecutionState::DONE);
    _isInitialized = true;
  }

  // produce output
  if (_currentGroup != _allGroups.end()) {
    writeCurrentGroupToOutput(output);
    _currentGroup++;
    _returnedGroups++;
    TRI_ASSERT(_returnedGroups <= _allGroups.size());
  }

  ExecutionState state = _currentGroup != _allGroups.end() ? ExecutionState::HASMORE
                                                           : ExecutionState::DONE;

  return {state, NoStats{}};
}

// if no group exists for the current row yet, this builds a new group.
std::pair<std::unique_ptr<HashedCollectExecutor::AggregateValuesType>, std::vector<AqlValue>>
HashedCollectExecutor::buildNewGroup(InputAqlItemRow& input, size_t n) {
  GroupKeyType group;
  group.reserve(n);

  // copy the group values before they get invalidated
  for (size_t i = 0; i < n; ++i) {
    group.emplace_back(input.stealValue(_infos.getGroupRegisters()[i].second));
  }

  auto aggregateValues = std::make_unique<AggregateValuesType>();
  aggregateValues->reserve(_aggregatorFactories.size());

  for (auto const& it : _aggregatorFactories) {
    aggregateValues->emplace_back((*it)(_infos.getTransaction()));
  }
  return std::make_pair(std::move(aggregateValues), group);
}

// finds the group matching the current row, or emplaces it. in either case,
// it returns an iterator to the group matching the current row in
// _allGroups. additionally, .second is true iff a new group was emplaced.
decltype(HashedCollectExecutor::_allGroups)::iterator HashedCollectExecutor::findOrEmplaceGroup(
    InputAqlItemRow& input) {
  GroupKeyType groupValues;  // TODO store groupValues locally
  size_t const n = _infos.getGroupRegisters().size();
  groupValues.reserve(n);

  // for hashing simply re-use the aggregate registers, without cloning
  // their contents
  for (size_t i = 0; i < n; ++i) {
    groupValues.emplace_back(input.getValue(_infos.getGroupRegisters()[i].second));
  }

  auto it = _allGroups.find(groupValues);

  if (it != _allGroups.end()) {
    // group already exists
    return it;
  }

  // must create new group
  GroupValueType aggregateValues;
  GroupKeyType group;
  std::tie(aggregateValues, group) = buildNewGroup(input, n);

  // note: aggregateValues may be a nullptr!
  auto emplaceResult = _allGroups.emplace(group, std::move(aggregateValues));
  // emplace must not fail
  TRI_ASSERT(emplaceResult.second);

  return emplaceResult.first;
};

std::pair<ExecutionState, size_t> HashedCollectExecutor::expectedNumberOfRows(size_t atMost) const {
  size_t rowsLeft = 0;
  if (!_isInitialized) {
    ExecutionState state;
    std::tie(state, rowsLeft) = _fetcher.preFetchNumberOfRows(atMost);
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(rowsLeft == 0);
      return {state, 0};
    }
    // Overestimate, we have not grouped!
  } else {
    // This fetcher nows how exactly many rows are left
    // as it knows how many  groups is has created and not returned.
    rowsLeft = _allGroups.size() - _returnedGroups;
  }
  if (rowsLeft > 0) {
    return {ExecutionState::HASMORE, rowsLeft};
  }
  return {ExecutionState::DONE, rowsLeft};
}