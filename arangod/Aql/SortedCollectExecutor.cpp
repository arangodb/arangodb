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
/// @author Lars Maier
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "SortedCollectExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/ConditionalDeleter.h"

#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

#include <Logger/LogMacros.h>
#include <utility>

// Set this to true to activate devel logging
#define LOG_DEVEL_SORTED_COLLECT_ENABLED false
#define LOG_DEVEL_SC LOG_DEVEL_IF(LOG_DEVEL_SORTED_COLLECT_ENABLED)

using namespace arangodb;
using namespace arangodb::aql;

static const AqlValue EmptyValue;

SortedCollectExecutor::CollectGroup::CollectGroup(bool count, Infos& infos)
    : groupLength(0),
      count(count),
      infos(infos),
      _lastInputRow(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _shouldDeleteBuilderBuffer(true) {
  for (auto const& aggName : infos.getAggregateTypes()) {
    aggregators.emplace_back(Aggregator::fromTypeString(infos.getTransaction(), aggName));
  }
  TRI_ASSERT(infos.getAggregatedRegisters().size() == aggregators.size());
};

SortedCollectExecutor::CollectGroup::~CollectGroup() {
  for (auto& it : groupValues) {
    it.destroy();
  }
}

void SortedCollectExecutor::CollectGroup::initialize(size_t capacity) {
  groupValues.clear();

  if (capacity > 0) {
    groupValues.reserve(capacity);

    for (size_t i = 0; i < capacity; ++i) {
      groupValues.emplace_back();
    }
  }

  groupLength = 0;

  // reset aggregators
  for (auto& it : aggregators) {
    it->reset();
  }
}

void SortedCollectExecutor::CollectGroup::reset(InputAqlItemRow const& input) {
  _shouldDeleteBuilderBuffer = true;
  ConditionalDeleter<VPackBuffer<uint8_t>> deleter(_shouldDeleteBuilderBuffer);
  std::shared_ptr<VPackBuffer<uint8_t>> buffer(new VPackBuffer<uint8_t>, deleter);
  _builder = VPackBuilder(buffer);

  if (!groupValues.empty()) {
    for (auto& it : groupValues) {
      it.destroy();
    }
    groupValues[0].erase();  // only need to erase [0], because we have
    // only copies of references anyway
  }

  groupLength = 0;
  _lastInputRow = input;

  // reset all aggregators
  for (auto& it : aggregators) {
    it->reset();
  }

  if (input.isInitialized()) {
    // construct the new group
    size_t i = 0;
    _builder.openArray();
    for (auto& it : infos.getGroupRegisters()) {
      this->groupValues[i] = input.getValue(it.second).clone();
      ++i;
    }

    addLine(input);
  } else {
    // We still need an open array...
    _builder.openArray();
  }
}

SortedCollectExecutorInfos::SortedCollectExecutorInfos(
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep,
    std::unordered_set<RegisterId>&& readableInputRegisters,
    std::unordered_set<RegisterId>&& writeableOutputRegisters,
    std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters,
    RegisterId collectRegister, RegisterId expressionRegister,
    Variable const* expressionVariable, std::vector<std::string>&& aggregateTypes,
    std::vector<std::pair<std::string, RegisterId>>&& variables,
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
      _expressionRegister(expressionRegister),
      _variables(variables),
      _expressionVariable(expressionVariable),
      _count(count),
      _trxPtr(trxPtr) {}

SortedCollectExecutor::SortedCollectExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _currentGroup(infos.getCount(), infos),
      _fetcherDone(false) {
  // reserve space for the current row
  _currentGroup.initialize(_infos.getGroupRegisters().size());
  // reset and recreate new group
  // Initialize group with invalid input
  InputAqlItemRow emptyInput{CreateInvalidInputRowHint{}};
  _currentGroup.reset(emptyInput);
};

void SortedCollectExecutor::CollectGroup::addLine(InputAqlItemRow const& input) {
  // remember the last valid row we had
  _lastInputRow = input;

  // calculate aggregate functions
  size_t j = 0;
  for (auto& it : this->aggregators) {
    TRI_ASSERT(!this->aggregators.empty());
    TRI_ASSERT(infos.getAggregatedRegisters().size() > j);
    RegisterId const reg = infos.getAggregatedRegisters()[j].second;
    if (reg != RegisterPlan::MaxRegisterId) {
      it->reduce(input.getValue(reg));
    } else {
      it->reduce(EmptyValue);
    }
    ++j;
  }
  TRI_IF_FAILURE("SortedCollectBlock::getOrSkipSome") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  if (infos.getCollectRegister() != RegisterPlan::MaxRegisterId) {
    if (count) {
      // increase the count
      groupLength++;
    } else if (infos.getExpressionVariable() != nullptr) {
      // compute the expression
      input.getValue(infos.getExpressionRegister()).toVelocyPack(infos.getTransaction(), _builder, false);
    } else {
      // copy variables / keep variables into result register

      _builder.openObject();
      for (auto const& pair : infos.getVariables()) {
        // Only collect input variables, the variable names DO! contain more.
        // e.g. the group variable name
        if (pair.second < infos.numberOfInputRegisters()) {
          _builder.add(VPackValue(pair.first));
          input.getValue(pair.second).toVelocyPack(infos.getTransaction(), _builder, false);
        }
      }
      _builder.close();
    }
  }
  TRI_IF_FAILURE("CollectGroup::addValues") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

bool SortedCollectExecutor::CollectGroup::isSameGroup(InputAqlItemRow const& input) const {
  // if we do not have valid input, return false
  if (!input.isInitialized()) {
    return false;
  } else {
    // check if groups are equal

    size_t i = 0;

    for (auto& it : infos.getGroupRegisters()) {
      // we already had a group, check if the group has changed
      // compare values 1 1 by one
      int cmp = AqlValue::Compare(infos.getTransaction(), this->groupValues[i],
                                  input.getValue(it.second), false);

      if (cmp != 0) {
        // This part pf the groupcheck differs
        return false;
      }
      ++i;
    }
    // Every part matched
    return true;
  }
}

void SortedCollectExecutor::CollectGroup::groupValuesToArray(VPackBuilder& builder) {
  builder.openArray();
  for (auto const& value : groupValues) {
    value.toVelocyPack(infos.getTransaction(), builder, false);
  }

  builder.close();
}

void SortedCollectExecutor::CollectGroup::writeToOutput(OutputAqlItemRow& output,
                                                        InputAqlItemRow const& input) {
  // Thanks to the edge case that we have to emit a row even if we have no
  // input We cannot assert here that the input row is valid ;(

  if (!input.isInitialized()) {
    output.setAllowSourceRowUninitialized();
  }

  size_t i = 0;
  for (auto& it : infos.getGroupRegisters()) {
    AqlValue val = this->groupValues[i];
    AqlValueGuard guard{val, true};

    output.moveValueInto(it.first, _lastInputRow, guard);
    // ownership of value is transferred into res
    this->groupValues[i].erase();
    ++i;
  }

  // handle aggregators
  size_t j = 0;
  for (auto& it : this->aggregators) {
    AqlValue val = it->stealValue();
    AqlValueGuard guard{val, true};
    output.moveValueInto(infos.getAggregatedRegisters()[j].first, _lastInputRow, guard);
    ++j;
  }

  // set the group values
  if (infos.getCollectRegister() != RegisterPlan::MaxRegisterId) {
    if (infos.getCount()) {
      // only set group count in result register
      output.cloneValueInto(infos.getCollectRegister(), _lastInputRow,
                            AqlValue(AqlValueHintUInt(static_cast<uint64_t>(this->groupLength))));
    } else {
      TRI_ASSERT(_builder.isOpenArray());
      _builder.close();

      auto buffer = _builder.steal();
      AqlValue val(buffer.get(), _shouldDeleteBuilderBuffer);
      AqlValueGuard guard{val, true};

      output.moveValueInto(infos.getCollectRegister(), _lastInputRow, guard);
    }
  }

  output.advanceRow();
}

std::pair<ExecutionState, NoStats> SortedCollectExecutor::produceRows(OutputAqlItemRow& output) {
  THROW_ARANGODB_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  TRI_IF_FAILURE("SortedCollectExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  ExecutionState state;
  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  while (true) {
    if (_fetcherDone) {
      if (_currentGroup.isValid()) {
        _currentGroup.writeToOutput(output, input);
        InputAqlItemRow inputDummy{CreateInvalidInputRowHint{}};
        _currentGroup.reset(inputDummy);
        TRI_ASSERT(!_currentGroup.isValid());
        return {ExecutionState::DONE, {}};
      }
      return {ExecutionState::DONE, {}};
    }
    TRI_IF_FAILURE("SortedCollectBlock::getOrSkipSomeOuter") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_IF_FAILURE("SortedCollectBlock::hasMore") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    std::tie(state, input) = _fetcher.fetchRow();

    if (state == ExecutionState::WAITING) {
      return {state, {}};
    }

    if (state == ExecutionState::DONE) {
      _fetcherDone = true;
    }

    // if we are in the same group, we need to add lines to the current group
    if (_currentGroup.isSameGroup(input)) {
      _currentGroup.addLine(input);

      if (state == ExecutionState::DONE) {
        TRI_ASSERT(!output.produced());
        _currentGroup.writeToOutput(output, input);
        // Invalidate group
        input = InputAqlItemRow{CreateInvalidInputRowHint{}};
        _currentGroup.reset(input);
        return {ExecutionState::DONE, {}};
      }
    } else if (_currentGroup.isValid()) {
      // Write the current group.
      // Start a new group from input
      _currentGroup.writeToOutput(output, input);
      TRI_ASSERT(output.produced());
      _currentGroup.reset(input);  // reset and recreate new group
      if (input.isInitialized()) {
        return {ExecutionState::HASMORE, {}};
      }
      TRI_ASSERT(state == ExecutionState::DONE);
      return {ExecutionState::DONE, {}};
    } else {
      if (!input.isInitialized()) {
        if (_infos.getGroupRegisters().empty()) {
          // we got exactly 0 rows as input.
          // by definition we need to emit one collect row
          _currentGroup.writeToOutput(output, input);
          TRI_ASSERT(output.produced());
        }
        TRI_ASSERT(state == ExecutionState::DONE);
        return {ExecutionState::DONE, {}};
      }
      // old group was not valid, do not write it
      _currentGroup.reset(input);  // reset and recreate new group
    }
  }
}

std::pair<ExecutionState, size_t> SortedCollectExecutor::expectedNumberOfRows(size_t atMost) const {
  THROW_ARANGODB_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  if (!_fetcherDone) {
    ExecutionState state;
    size_t expectedRows;
    std::tie(state, expectedRows) = _fetcher.preFetchNumberOfRows(atMost);
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(expectedRows == 0);
      return {state, 0};
    }
    return {ExecutionState::HASMORE, expectedRows + 1};
  }
  // The fetcher will NOT send anything any more
  // We will at most return the current open group
  if (_currentGroup.isValid()) {
    return {ExecutionState::HASMORE, 1};
  }
  return {ExecutionState::DONE, 0};
}

auto SortedCollectExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                        OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TRI_IF_FAILURE("SortedCollectExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  AqlCall clientCall = output.getClientCall();
  TRI_ASSERT(clientCall.offset == 0);

  size_t rowsProduces = 0;
  bool pendingGroup = false;

  while (!output.isFull()) {
    auto [state, input] = inputRange.peekDataRow();

    LOG_DEVEL_SC << "SortedCollectExecutor::produceRows " << state << " "
                 << input.isInitialized();

    if (state == ExecutorState::DONE && !(_haveSeenData || input.isInitialized())) {
      // we have never been called with data
      LOG_DEVEL_SC << "never called with data";
      if (_infos.getGroupRegisters().empty()) {
        // by definition we need to emit one collect row
        _currentGroup.writeToOutput(output, InputAqlItemRow{CreateInvalidInputRowHint{}});
        rowsProduces += 1;
      }
      break;
    }

    // either state != DONE or we have an input row
    TRI_ASSERT(state == ExecutorState::HASMORE || state == ExecutorState::DONE);
    if (!input.isInitialized() && state != ExecutorState::DONE) {
      LOG_DEVEL_SC << "need more input rows";
      break;
    }

    TRI_IF_FAILURE("SortedCollectBlock::getOrSkipSomeOuter") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_IF_FAILURE("SortedCollectBlock::hasMore") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    if (input.isInitialized()) {
      _haveSeenData = true;

      // if we are in the same group, we need to add lines to the current group
      if (_currentGroup.isSameGroup(input)) {
        LOG_DEVEL_SC << "input is same group";
        _currentGroup.addLine(input);

      } else if (_currentGroup.isValid()) {
        LOG_DEVEL_SC << "input is new group, writing old group";
        // Write the current group.
        // Start a new group from input
        rowsProduces += 1;
        _currentGroup.writeToOutput(output, input);

        if (output.isFull()) {
          LOG_DEVEL_SC << "now output is full, exit early";
          pendingGroup = true;
          _currentGroup.reset(InputAqlItemRow{CreateInvalidInputRowHint{}});
          break;
        }
        _currentGroup.reset(input);  // reset and recreate new group

      } else {
        LOG_DEVEL_SC << "generating new group";
        // old group was not valid, do not write it
        _currentGroup.reset(input);  // reset and recreate new group
      }
    }

    inputRange.nextDataRow();

    bool produceMore = !output.isFull();
    if (!produceMore) {
      pendingGroup = true;
      break;
    }

    if (state == ExecutorState::DONE) {
      rowsProduces += 1;
      _currentGroup.writeToOutput(output, input);
      _currentGroup.reset(InputAqlItemRow{CreateInvalidInputRowHint{}});
      break;
    }
  }

  auto newState = pendingGroup ? ExecutorState::HASMORE : inputRange.upstreamState();

  LOG_DEVEL_SC << "reporting state: " << newState;
  return {newState, Stats{}, AqlCall{}};
}

auto SortedCollectExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& clientCall)
    -> std::tuple<ExecutorState, size_t, AqlCall> {
  TRI_IF_FAILURE("SortedCollectExecutor::skipRowsRange") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  TRI_ASSERT(clientCall.needSkipMore());
  while (clientCall.needSkipMore()) {
    LOG_DEVEL_SC << "clientCall.getSkipCount() == " << clientCall.getSkipCount();
    LOG_DEVEL_SC << "clientCall.needSkipMore() == " << clientCall.needSkipMore();

    {
      auto [state, input] = inputRange.peekDataRow();

      LOG_DEVEL_SC << "SortedCollectExecutor::skipRowsRange " << state << " "
                   << std::boolalpha << input.isInitialized();

      if (input.isInitialized()) {
        // we received data
        _haveSeenData = true;

        // if we are in the same group, we can skip this line
        if (_currentGroup.isSameGroup(input)) {
          LOG_DEVEL_SC << "input is same group";
          std::ignore = inputRange.nextDataRow();
          /* do nothing */
        } else {
          if (_currentGroup.isValid()) {
            LOG_DEVEL_SC << "input is new group, skipping current group";
            // The current group is completed, skip it and create a new one
            clientCall.didSkip(1);
            _currentGroup.reset(InputAqlItemRow{CreateInvalidInputRowHint{}});
            continue;
          }

          LOG_DEVEL_SC << "group is invalid, creating new group";
          _currentGroup.reset(input);
          std::ignore = inputRange.nextDataRow();
        }
      }

      if (!clientCall.needSkipMore()) {
        LOG_DEVEL_SC << "stop skipping early, there could be a pending group";
        break;
      }

      if (state == ExecutorState::DONE) {
        if (!_haveSeenData) {
          // we have never been called with data
          LOG_DEVEL_SC << "never called with data";
          if (_infos.getGroupRegisters().empty()) {
            // by definition we need to emit one collect row
            clientCall.didSkip(1);
          }
        } else {
          LOG_DEVEL_SC << "skipping final group";
          clientCall.didSkip(1);
          _currentGroup.reset(InputAqlItemRow{CreateInvalidInputRowHint{}});
        }
        break;
      } else if (!input.isInitialized()) {
        TRI_ASSERT(state == ExecutorState::HASMORE);
        LOG_DEVEL_SC << "waiting for more data to skip";
        break;
      }
    }
  }

  LOG_DEVEL_SC << " skipped rows: " << clientCall.getSkipCount();
  LOG_DEVEL_SC << "reporting state: " << inputRange.upstreamState();

  return {inputRange.upstreamState(), clientCall.getSkipCount(), AqlCall{}};
}
