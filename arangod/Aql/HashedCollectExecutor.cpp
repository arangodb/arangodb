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

static AqlValue EmptyValue;

HashedCollectExecutorInfos::HashedCollectExecutorInfos(
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep,
    std::unordered_set<RegisterId>&& readableInputRegisters,
    std::unordered_set<RegisterId>&& writeableInputRegisters,
    std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters, RegisterId collectRegister,
    std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> const& aggregateVariables,
    transaction::Methods* trxPtr, bool count)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(readableInputRegisters),
                    std::make_shared<std::unordered_set<RegisterId>>(writeableInputRegisters),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _aggregateVariables(aggregateVariables),
      _groupRegisters(groupRegisters),
      _collectRegister(collectRegister),
      _count(count),
      _trxPtr(trxPtr) {
  TRI_ASSERT(!_groupRegisters.empty());
}

HashedCollectExecutor::HashedCollectExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _allGroups(1024,
                 AqlValueGroupHash(_infos.getTransaction(),
                                   _infos.getGroupVariables().size()),
                 AqlValueGroupEqual(_infos.getTransaction())){};

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
  InputAqlItemRow input{CreateInvalidInputRowHint{}};
  ExecutionState state;

  std::vector<AqlValue> groupValues;
  groupValues.reserve(_infos.getGroupRegisters().size());  // TODO CHECK

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
      group.emplace_back(input.getValue(_infos.getGroupRegisters()[i].second));
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
    std::vector<AqlValue> groupValues;
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

  auto buildResult = [this](InputAqlItemRow& input, OutputAqlItemRow& output) {
      TRI_ASSERT(!_infos.getCount() || _infos.getCollectRegister() != ExecutionNode::MaxRegisterId);

      // TODO CHECK -- size_t row = 0;
      for (auto& it : _allGroups) {
        auto& keys = it.first;
        TRI_ASSERT(it.second != nullptr);

        TRI_ASSERT(keys.size() == _infos.getGroupRegisters().size());
        size_t i = 0;
        for (auto& key : keys) {
          output.cloneValueInto(_infos.getGroupRegisters()[i++].first, input, key);
          const_cast<AqlValue*>(&key)->erase();  // to prevent double-freeing later
        }

        if (!_infos.getCount()) {
          TRI_ASSERT(it.second->size() == _infos.getAggregatedRegisters().size());
          size_t j = 0;
          for (auto const& r : *(it.second)) {
            output.cloneValueInto(_infos.getAggregatedRegisters()[j++].first, input, r->stealValue());
          }
        } else {
          // set group count in result register
          TRI_ASSERT(!it.second->empty());
          output.cloneValueInto(_infos.getCollectRegister(), input, it.second->back()->stealValue());
        }

        /* // TODO
        if (row > 0) {
          // re-use already copied AQLValues for remaining registers
          result->copyValuesFromFirstRow(row, nrInRegs);
        }

        ++row;*/
      }
  };

  // "adds" the current row to its group's aggregates.
  auto reduceAggregates = [this, input](decltype(_allGroups)::iterator groupIt) {
    AggregateValuesType* aggregateValues = groupIt->second.get();

    if (_infos.getAggregateVariables().empty()) {
      // no aggregate registers. simply increase the counter
      if (_infos.getCount()) {
        // TODO get rid of this special case if possible
        TRI_ASSERT(!aggregateValues->empty());
        aggregateValues->back()->reduce(AqlValue());
      }
    } else {
      // apply the aggregators for the group
      TRI_ASSERT(aggregateValues->size() == _infos.getAggregatedRegisters().size());
      size_t j = 0;
      for (auto const& r : _infos.getAggregatedRegisters()) {
        if (r.second == ExecutionNode::MaxRegisterId) {
          (*aggregateValues)[j]->reduce(EmptyValue); // TODO difference to AqlValue() ?
        } else {
          (*aggregateValues)[j]->reduce(input.getValue(r.second));
          ++j;
        }
      }
    }
  };

  while (true) {
    std::tie(state, input) = _fetcher.fetchRow();

    if (state == ExecutionState::WAITING) {
      return {state, stats};
    }

    if (!input) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, stats};
    }
    TRI_ASSERT(input.isInitialized());

    // TODO LOGIC HERE
    // NOLINTNEXTLINE(hicpp-use-auto,modernize-use-auto)
    decltype(_allGroups)::iterator currentGroupIt;
    bool newGroup;
    std::tie(currentGroupIt, newGroup) = findOrEmplaceGroup(input);

    /*
    if (newGroup) {
      ++_skipped;
    }*/

    reduceAggregates(currentGroupIt);

    if (state == ExecutionState::DONE) {
      // TODO check order of calls here
      buildResult(input, output);
    }
  }
}
