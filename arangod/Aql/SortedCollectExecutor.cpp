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
  TRI_ASSERT(false);
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
  TRI_ASSERT(false);
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

  /*
   * While the output is not full, read more input ranges and put them into
   *  the groups.
   *  If a row does not belong to a group generate a output row.
   * If the state is DONE generate the last group.
   * If nothing was generated and we have to write at least one row, write that row.
   */

  size_t rowsProduces = 0;

  while (!output.isFull()) {
    auto [state, input] = inputRange.nextDataRow();

    LOG_DEVEL << "SortedCollectExecutor::produceRows " << state << " "
              << input.isInitialized();

    // TODO store in member if we have not been called with data before
    if (state == ExecutorState::DONE && !(_haveSeenData || input.isInitialized())) {
      // we have never been called with data
      LOG_DEVEL << "never called with data";
      if (_infos.getGroupRegisters().empty()) {
        // by definition we need to emit one collect row
        _currentGroup.writeToOutput(output, InputAqlItemRow{CreateInvalidInputRowHint{}});
        rowsProduces += 1;
      }
      break;
    }

    // either state != DONE or we have an input row
    TRI_ASSERT(state == ExecutorState::HASMORE || state == ExecutorState::DONE);
    if (!input.isInitialized()) {
      LOG_DEVEL << "need more input rows";
      break;
    }

    TRI_IF_FAILURE("SortedCollectBlock::getOrSkipSomeOuter") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_IF_FAILURE("SortedCollectBlock::hasMore") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    _haveSeenData = true;

    // if we are in the same group, we need to add lines to the current group
    if (_currentGroup.isSameGroup(input)) {
      LOG_DEVEL << "input is same group";
      _currentGroup.addLine(input);

    } else if (_currentGroup.isValid()) {
      LOG_DEVEL << "input is new group, writing old group";
      // Write the current group.
      // Start a new group from input
      rowsProduces += 1;
      _currentGroup.writeToOutput(output, input);
      _currentGroup.reset(input);  // reset and recreate new group

    } else {
      LOG_DEVEL << "generating new group";
      // old group was not valid, do not write it
      _currentGroup.reset(input);  // reset and recreate new group
    }

    if (state == ExecutorState::DONE) {
      rowsProduces += 1;
      _currentGroup.writeToOutput(output, input);
      _currentGroup.reset(InputAqlItemRow{CreateInvalidInputRowHint{}});
      break;
    }
  }

  LOG_DEVEL << "reporting state: " << inputRange.upstreamState();
  return {inputRange.upstreamState(), Stats{}, AqlCall{}};
}

auto SortedCollectExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& clientCall)
    -> std::tuple<ExecutorState, size_t, AqlCall> {
  TRI_IF_FAILURE("SortedCollectExecutor::skipRowsRange") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  TRI_ASSERT(clientCall.offset > 0);

  /*
   * While the output is not full, read more input ranges and put them into
   *  the groups.
   *  If a row does not belong to a group generate a output row.
   * If the state is DONE generate the last group.
   * If nothing was generated and we have to write at least one row, write that row.
   */
  size_t rowsSkipped = 0;
  while ((rowsSkipped < clientCall.getOffset()) ||
         (clientCall.getLimit() == 0 && clientCall.needsFullCount())) {
    auto [state, input] = inputRange.nextDataRow();

    LOG_DEVEL << "SortedCollectExecutor::skipRowsRange " << state << " "
              << input.isInitialized();

    // TODO store in member if we have not been called with data before
    if (state == ExecutorState::DONE && !(_haveSeenData || input.isInitialized())) {
      // we have never been called with data
      LOG_DEVEL << "never called with data";
      if (_infos.getGroupRegisters().empty()) {
        // by definition we need to emit one collect row
        rowsSkipped += 1;
      }
      break;
    }

    // either state != DONE or we have an input row
    TRI_ASSERT(state == ExecutorState::HASMORE || state == ExecutorState::DONE);
    if (!input.isInitialized()) {
      LOG_DEVEL << "need more input rows";
      break;
    }

    _haveSeenData = true;

    // if we are in the same group, we need to add lines to the current group
    if (_currentGroup.isSameGroup(input)) {
      LOG_DEVEL << "input is same group";
      /* do nothing */
    } else if (_currentGroup.isValid()) {
      LOG_DEVEL << "input is new group, writing old group";
      // Write the current group.
      // Start a new group from input
      rowsSkipped += 1;
      _currentGroup.reset(input);  // reset and recreate new group
    } else {
      LOG_DEVEL << "generating new group";
      // old group was not valid, do not write it
      _currentGroup.reset(input);  // reset and recreate new group
    }

    if (state == ExecutorState::DONE) {
      rowsSkipped += 1;
      input = InputAqlItemRow{CreateInvalidInputRowHint{}};
      _currentGroup.reset(input);
      break;
    }
  }

  clientCall.didSkip(rowsSkipped);

  AqlCall upstreamCall;
  upstreamCall.fullCount = clientCall.fullCount;

  LOG_DEVEL << "reporting state: " << inputRange.upstreamState()
            << " skipped rows: " << rowsSkipped;
  return {inputRange.upstreamState(), rowsSkipped, upstreamCall};
}
