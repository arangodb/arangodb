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
    std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters, RegisterId collectRegister,
    std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> const& aggregateVariables,
    std::vector<std::pair<RegisterId, RegisterId>>&& aggregateRegisters,
    std::vector<std::pair<Variable const*, Variable const*>> groupVariables,
    transaction::Methods* trxPtr, bool count)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(readableInputRegisters),
                    std::make_shared<std::unordered_set<RegisterId>>(writeableOutputRegisters),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _aggregateVariables(aggregateVariables),
      _aggregateRegisters(aggregateRegisters),
      _groupRegisters(groupRegisters),
      _collectRegister(collectRegister),
      _groupVariables(groupVariables),
      _count(count),
      _trxPtr(trxPtr) {
  TRI_ASSERT(!_groupRegisters.empty());
}

HashedCollectExecutor::HashedCollectExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _state(ExecutionState::HASMORE),
      _input(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _allGroups(1024,
                 AqlValueGroupHash(_infos.getTransaction(),
                                   _infos.getGroupVariables().size()),
                 AqlValueGroupEqual(_infos.getTransaction())),
      _done(false),
      _init(false){};

HashedCollectExecutor::~HashedCollectExecutor() {
  // Generally, _allGroups should be empty when the block is destroyed - except
  // when an exception is thrown during getOrSkipSome, in which case the
  // AqlValue ownership hasn't been transferred.
  _destroyAllGroupsAqlValues();
}

void HashedCollectExecutor::_destroyAllGroupsAqlValues() {
  for (auto& it : _allGroups) {
    for (auto& it2 : it.first) {
      const_cast<AqlValue*>(&it2)->destroy();
    }
  }
}

std::pair<ExecutionState, NoStats> HashedCollectExecutor::produceRow(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("HashedCollectExecutor::produceRow") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  NoStats stats{};

  std::vector<std::function<std::unique_ptr<Aggregator>(transaction::Methods*)> const*> aggregatorFactories;
  if (_infos.getAggregateVariables().empty()) {
    // no aggregate registers. this means we'll only count the number of items
    if (_infos.getCount()) {
      aggregatorFactories.emplace_back(
          Aggregator::factoryFromTypeString("LENGTH"));
    }
  } else {
    // we do have aggregate registers. create them as empty AqlValues
    aggregatorFactories.reserve(_infos.getAggregatedRegisters().size());

    // initialize aggregators
    for (auto const& r : _infos.getAggregateVariables()) {
      aggregatorFactories.emplace_back(
          Aggregator::factoryFromTypeString(r.second.second));
    }
  }

  // if no group exists for the current row yet, this builds a new group.
  auto buildNewGroup = [this, &aggregatorFactories](InputAqlItemRow& input, size_t n)
      -> std::pair<std::unique_ptr<AggregateValuesType>, std::vector<AqlValue>> {
    std::vector<AqlValue> group;
    group.reserve(n);

    // copy the group values before they get invalidated
    for (size_t i = 0; i < n; ++i) {
      group.emplace_back(input.stealValue(_infos.getGroupRegisters()[i].second));
    }

    auto aggregateValues = std::make_unique<AggregateValuesType>();
    aggregateValues->reserve(aggregatorFactories.size());

    for (auto const& it : aggregatorFactories) {
      aggregateValues->emplace_back((*it)(_infos.getTransaction()));
    }
    return std::make_pair(std::move(aggregateValues), group);
  };

  // finds the group matching the current row, or emplaces it. in either case,
  // it returns an iterator to the group matching the current row in
  // _allGroups. additionally, .second is true iff a new group was emplaced.
  auto findOrEmplaceGroup = [this, &buildNewGroup](InputAqlItemRow& input)
      -> std::pair<decltype(_allGroups)::iterator, bool> {
    std::vector<AqlValue> groupValues;  // TODO store groupValues locally
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
      return {it, false};
    }

    // must create new group
    std::unique_ptr<AggregateValuesType> aggregateValues;
    std::vector<AqlValue> group;
    std::tie(aggregateValues, group) = buildNewGroup(input, n);

    // note: aggregateValues may be a nullptr!
    auto emplaceResult = _allGroups.emplace(group, std::move(aggregateValues));
    // emplace must not fail
    TRI_ASSERT(emplaceResult.second);

    return {emplaceResult.first, true};
  };

  if (_done) {
    return {_state, stats};
  }

  while (true) {
    // if initialization is done
    if (_init) {
      if (_currentGroup == _allGroups.end()) {
        _done = true;
        return {ExecutionState::DONE, stats};
      }

      // build the result
      TRI_ASSERT(!_infos.getCount() ||
                 _infos.getCollectRegister() != ExecutionNode::MaxRegisterId);

      auto& keys = _currentGroup->first;
      TRI_ASSERT(_currentGroup->second != nullptr);

      TRI_ASSERT(keys.size() == _infos.getGroupRegisters().size());
      size_t i = 0;
      for (auto& key : keys) {
        output.cloneValueInto(_infos.getGroupRegisters()[i++].first, _input, key);
        const_cast<AqlValue*>(&key)->erase();  // to prevent double-freeing later
      }

      if (!_infos.getCount()) {
        TRI_ASSERT(_currentGroup->second->size() ==
                   _infos.getAggregatedRegisters().size());
        size_t j = 0;
        for (auto const& r : *(_currentGroup->second)) {
          output.cloneValueInto(_infos.getAggregatedRegisters()[j++].first,
                                _input, r->stealValue());
        }
      } else {
        // set group count in result register
        TRI_ASSERT(!_currentGroup->second->empty());
        output.cloneValueInto(_infos.getCollectRegister(), _input,
                              _currentGroup->second->back()->stealValue());
      }
      // incr the group iterator
      _currentGroup++;

      if (_currentGroup == _allGroups.end()) {
        return {ExecutionState::DONE, stats};
      }

      return {ExecutionState::HASMORE, stats};
    }

    InputAqlItemRow input = InputAqlItemRow{CreateInvalidInputRowHint{}};
    if (_state != ExecutionState::DONE) {
      std::tie(_state, input) = _fetcher.fetchRow();

      if (_state == ExecutionState::WAITING) {
        TRI_ASSERT(!input.isInitialized());
        return {_state, stats};
      }

      // !input.isInitialized() => _state == ExecutionState::DONE
      TRI_ASSERT(input.isInitialized() || _state == ExecutionState::DONE);

      // needed to remember the last valid input aql item row
      // NOTE: this might impact the performance
      if (input.isInitialized()) {
        _input = input;
      }
    }

    if (!input && _done) {
      TRI_ASSERT(_state == ExecutionState::DONE);
      return {_state, stats};
    }

    if (input.isInitialized()) {
      decltype(_allGroups)::iterator currentGroupIt;
      bool newGroup;
      TRI_ASSERT(input.isInitialized());
      std::tie(currentGroupIt, newGroup) = findOrEmplaceGroup(input);

      // reduce the aggregates
      AggregateValuesType* aggregateValues = currentGroupIt->second.get();

      if (_infos.getAggregateVariables().empty()) {
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

    if (_state == ExecutionState::DONE) {
      // setup the iterator
      if (!_init) {
        _currentGroup = _allGroups.begin();
        _init = true;
      }
    }
  }
}
