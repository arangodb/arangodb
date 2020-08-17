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
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/ConditionalDeleter.h"

#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

#include <Logger/LogMacros.h>
#include <utility>

// Set this to true to activate devel logging
#define INTERNAL_LOG_SC LOG_DEVEL_IF(false)

using namespace arangodb;
using namespace arangodb::aql;

static const AqlValue EmptyValue;

SortedCollectExecutor::CollectGroup::CollectGroup(bool count, Infos& infos)
    : groupLength(0),
      count(count),
      _shouldDeleteBuilderBuffer(true),
      infos(infos),
      _lastInputRow(InputAqlItemRow{CreateInvalidInputRowHint{}}) {
  for (auto const& aggName : infos.getAggregateTypes()) {
    aggregators.emplace_back(Aggregator::fromTypeString(infos.getVPackOptions(), aggName));
  }
  TRI_ASSERT(infos.getAggregatedRegisters().size() == aggregators.size());
}

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
    std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters,
    RegisterId collectRegister, RegisterId expressionRegister,
    Variable const* expressionVariable, std::vector<std::string>&& aggregateTypes,
    std::vector<std::pair<std::string, RegisterId>>&& inputVariables,
    std::vector<std::pair<RegisterId, RegisterId>>&& aggregateRegisters,
    velocypack::Options const* opts, bool count)
    : _aggregateTypes(std::move(aggregateTypes)),
      _aggregateRegisters(std::move(aggregateRegisters)),
      _groupRegisters(std::move(groupRegisters)),
      _collectRegister(collectRegister),
      _expressionRegister(expressionRegister),
      _inputVariables(std::move(inputVariables)),
      _expressionVariable(expressionVariable),
      _vpackOptions(opts),
      _count(count) {}

SortedCollectExecutor::SortedCollectExecutor(Fetcher&, Infos& infos)
    : _infos(infos), _currentGroup(infos.getCount(), infos) {
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
      input.getValue(infos.getExpressionRegister()).toVelocyPack(infos.getVPackOptions(), _builder, false);
    } else {
      // copy variables / keep variables into result register

      _builder.openObject();
      for (auto const& pair : infos.getInputVariables()) {
        _builder.add(VPackValue(pair.first));
        input.getValue(pair.second).toVelocyPack(infos.getVPackOptions(), _builder, false);
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
      // Note that `None` and `null` are considered equal by AqlValue::Compare,
      // which is a problem if we encounter `null` values on the very first row,
      // when groupValues is still uninitialized and thus `None`.
      if (this->groupValues[i].isNone()) {
        return false;
      }
      // we already had a group, check if the group has changed
      // compare values 1 1 by one
      int cmp = AqlValue::Compare(infos.getVPackOptions(), this->groupValues[i],
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
    value.toVelocyPack(infos.getVPackOptions(), builder, false);
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

[[nodiscard]] auto SortedCollectExecutor::expectedNumberOfRowsNew(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept -> size_t {
  if (input.finalState() == ExecutorState::DONE) {
    // Worst case assumption:
    // For every input row we have a new group.
    // If we have an open group right now, we need to add 1 to this estimate.
    // We will never produce more then asked for
    auto estOnInput = input.countDataRows();
    if (_currentGroup.isValid()) {
      // Have one group still to write,
      // that is not part of this input.
      estOnInput += 1;
    }
    if (estOnInput == 0 && _infos.getGroupRegisters().empty()) {
      // Special case, on empty input we will produce 1 output
      estOnInput = 1;
    }
    return std::min(call.getLimit(), estOnInput);
  }
  // Otherwise we do not know.
  return call.getLimit();
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

    INTERNAL_LOG_SC << "SortedCollectExecutor::produceRows " << state << " "
                 << input.isInitialized();

    if (state == ExecutorState::DONE && !(_haveSeenData || input.isInitialized())) {
      // we have never been called with data
      INTERNAL_LOG_SC << "never called with data";
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
      INTERNAL_LOG_SC << "need more input rows";
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
        INTERNAL_LOG_SC << "input is same group";
        _currentGroup.addLine(input);

      } else if (_currentGroup.isValid()) {
        INTERNAL_LOG_SC << "input is new group, writing old group";
        // Write the current group.
        // Start a new group from input
        rowsProduces += 1;
        _currentGroup.writeToOutput(output, input);

        if (output.isFull()) {
          INTERNAL_LOG_SC << "now output is full, exit early";
          pendingGroup = true;
          _currentGroup.reset(InputAqlItemRow{CreateInvalidInputRowHint{}});
          break;
        }
        _currentGroup.reset(input);  // reset and recreate new group

      } else {
        INTERNAL_LOG_SC << "generating new group";
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

  INTERNAL_LOG_SC << "reporting state: " << newState;
  return {newState, Stats{}, AqlCall{}};
}

auto SortedCollectExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& clientCall)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  TRI_IF_FAILURE("SortedCollectExecutor::skipRowsRange") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  TRI_ASSERT(clientCall.needSkipMore());
  while (clientCall.needSkipMore()) {
    INTERNAL_LOG_SC << "clientCall.getSkipCount() == " << clientCall.getSkipCount();
    INTERNAL_LOG_SC << "clientCall.needSkipMore() == " << clientCall.needSkipMore();

    {
      auto [state, input] = inputRange.peekDataRow();

      INTERNAL_LOG_SC << "SortedCollectExecutor::skipRowsRange " << state << " "
                   << std::boolalpha << input.isInitialized();

      if (input.isInitialized()) {
        // we received data
        _haveSeenData = true;

        // if we are in the same group, we can skip this line
        if (_currentGroup.isSameGroup(input)) {
          INTERNAL_LOG_SC << "input is same group";
          /* do nothing */
        } else {
          if (_currentGroup.isValid()) {
            INTERNAL_LOG_SC << "input is new group, skipping current group";
            // The current group is completed, skip it and create a new one
            clientCall.didSkip(1);
            _currentGroup.reset(InputAqlItemRow{CreateInvalidInputRowHint{}});
            continue;
          }

          INTERNAL_LOG_SC << "group is invalid, creating new group";
          _currentGroup.reset(input);
        }
        std::ignore = inputRange.nextDataRow();
      }

      if (!clientCall.needSkipMore()) {
        INTERNAL_LOG_SC << "stop skipping early, there could be a pending group";
        break;
      }

      if (state == ExecutorState::DONE) {
        if (!_haveSeenData) {
          // we have never been called with data
          INTERNAL_LOG_SC << "never called with data";
          if (_infos.getGroupRegisters().empty()) {
            // by definition we need to emit one collect row
            clientCall.didSkip(1);
          }
        } else {
          if (_currentGroup.isValid()) {
            INTERNAL_LOG_SC << "skipping final group";
            clientCall.didSkip(1);
            _currentGroup.reset(InputAqlItemRow{CreateInvalidInputRowHint{}});
          }
        }
        break;
      } else if (!input.isInitialized()) {
        TRI_ASSERT(state == ExecutorState::HASMORE);
        INTERNAL_LOG_SC << "waiting for more data to skip";
        break;
      }
    }
  }

  INTERNAL_LOG_SC << " skipped rows: " << clientCall.getSkipCount();
  INTERNAL_LOG_SC << "reporting state: " << inputRange.upstreamState();

  return {inputRange.upstreamState(), NoStats{}, clientCall.getSkipCount(), AqlCall{}};
}
