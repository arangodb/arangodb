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

#include "SortedCollectExecutor.h"

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

SortedCollectExecutor::CollectGroup::CollectGroup(bool count, Infos& infos)
    : groupLength(0), count(count), infos(infos) {}

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

void SortedCollectExecutor::CollectGroup::reset(InputAqlItemRow& input) {
  if (!groupValues.empty()) {
    for (auto& it : groupValues) {
      it.destroy();
    }
    groupValues[0].erase();  // only need to erase [0], because we have
    // only copies of references anyway
  }

  // TODO: GROUP LENGTH 1 only if initliza
  groupLength = 1;  // TODO fix groupLength set

  // reset all aggregators
  for (auto& it : aggregators) {
    it->reset();
  }

  if (input.isInitialized()) {
    // construct the new group
    size_t i = 0;
    for (auto& it : infos.getGroupRegisters()) {
      this->groupValues[i] = input.getValue(it.second).clone();
      ++i;
    }
  }
}

void SortedCollectExecutor::CollectGroup::addValues(InputAqlItemRow& input,
                                                    RegisterId groupRegister) {
  if (groupRegister == ExecutionNode::MaxRegisterId) {
    // nothing to do, but still make sure we won't add the same rows again
    return;
  }

  // copy group values
  if (count) {
    groupLength += 1;
  } else {
    try {
      TRI_IF_FAILURE("CollectGroup::addValues") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      // groupValues.emplace_back(input.getValue(groupRegister)); // TODO check register
      for (auto& it : infos.getGroupRegisters()) {  // TODO check if this is really correct!!
        groupValues.emplace_back(input.getValue(it.second));  // ROW 328 of CollectBlock.cpp
      }
      /*
      size_t i = 0;
      for (auto& it : infos.getGroupRegisters()) {
        this->groupValues[i] = input.getValue(it.second).clone();
        ++i;
      }
      */
    } catch (...) {
      throw;
    }
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
    std::vector<std::string>&& variableNames,
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
      _variableNames(variableNames),
      _expressionVariable(expressionVariable),
      _count(count),
      _trxPtr(trxPtr) {}

SortedCollectExecutor::SortedCollectExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher), _currentGroup(infos.getCount(), infos) {
  // reserve space for the current row
  _currentGroup.initialize(_infos.getGroupRegisters().size());
  _fetchedFirstRow = false;
  _returnedDone = false;
};

void SortedCollectExecutor::CollectGroup::addLine(InputAqlItemRow& input) {
  size_t j = 0;
  for (auto& it : this->aggregators) {
    RegisterId const reg = infos.getAggregatedRegisters()[j].second;
    it->reduce(input.getValue(reg));
    ++j;
  }
  // _currentGroup.addValues(block, _collectRegister); TODO: Do we need to add values from row?
  this->addValues(input, infos.getCollectRegister());
}

bool SortedCollectExecutor::CollectGroup::isSameGroup(InputAqlItemRow& input) {
  // if we do not have valid input, return false
  if (!input.isInitialized()) {
    return false;
  } else {
    // check if groups are equal
    bool newGroup = false;

    // we already had a group, check if the group has changed
    size_t i = 0;

    for (auto& it : infos.getGroupRegisters()) {
      int cmp = AqlValue::Compare(infos.getTransaction(), this->groupValues[i],
                                  input.getValue(it.second), false);

      if (cmp != 0) {
        // group change
        newGroup = true;
        break;
      }
      ++i;
    }
    if (newGroup) {
      return false;
    } else {
      return true;
    }
  }
}

void SortedCollectExecutor::CollectGroup::groupValuesToArray(VPackBuilder& builder) {
  builder.openArray();
  for (auto const& value : groupValues) {
    value.toVelocyPack(infos.getTransaction(), builder, false);
  }

  builder.close();
}

void SortedCollectExecutor::CollectGroup::writeToOutput(InputAqlItemRow& input,  // TODO INPUT
                                                        OutputAqlItemRow& output) {
  // if we do not have initialized input, just return and do not write to any register

  InputAqlItemRow testInput = InputAqlItemRow{CreateInvalidInputRowHint{}};

  /*
  if (!input.isInitialized()) {
    return;
  } else {*/
  // write group to the output row
  size_t i = 0;
  for (auto& it : infos.getGroupRegisters()) {
    AqlValue val = this->groupValues[i];
    AqlValueGuard guard{val, true};

    LOG_DEVEL << "Output NR1";
    output.moveValueInto(it.first, testInput, guard);
    // ownership of value is transferred into res
    this->groupValues[i].erase();  // TODO: maybe remove, check with reset function // TODO2: DO NOT DESTORY
    ++i;
  }

  // handle aggregators
  size_t j = 0;
  for (auto& it : this->aggregators) {
    // RegisterId const reg = infos.getAggregatedRegisters()[j].second;
    // it->reduce(input.getValue(reg));

    AqlValue val = it->stealValue();
    AqlValueGuard guard{val, true};
    LOG_DEVEL << "Output NR2";
    output.moveValueInto(infos.getAggregatedRegisters()[j].first, testInput, guard);
    ++j;
  }

  // set the group values
  if (infos.getCollectRegister() != ExecutionNode::MaxRegisterId) {
    this->addValues(testInput, infos.getCollectRegister());

    if (infos.getCount()) {
      // only set group count in result register
      LOG_DEVEL << "Output NR3";
      output.cloneValueInto(infos.getCollectRegister(), testInput,  // TODO check move
                            AqlValue(AqlValueHintUInt(static_cast<uint64_t>(this->groupLength))));
    } else if (infos.getExpressionVariable() != nullptr) {
      // copy expression result into result register
      bool shouldDelete = true;
      ConditionalDeleter<VPackBuffer<uint8_t>> deleter(shouldDelete);
      std::shared_ptr<VPackBuffer<uint8_t>> buffer(new VPackBuffer<uint8_t>, deleter);

      VPackBuilder builder(buffer);
      groupValuesToArray(builder);
      AqlValue val = AqlValue(buffer.get(), shouldDelete);
      AqlValueGuard guard{val, true};

      LOG_DEVEL << "Output NR4";
      output.moveValueInto(infos.getCollectRegister(), testInput, guard);
      // output.cloneValueInto(infos.getCollectRegister(), input,  // TODO check move
      //                    this->currentInput.getValue(infos.getExpressionRegister())); // TODO REMOVE INPUT
    } else {
      LOG_DEVEL << "Output NR5"; // TODO CHECK if builder object is properly and correctly filled!
      bool shouldDelete = true;
      ConditionalDeleter<VPackBuffer<uint8_t>> deleter(shouldDelete);
      std::shared_ptr<VPackBuffer<uint8_t>> buffer(new VPackBuffer<uint8_t>, deleter);

      VPackBuilder builder(buffer);
      builder.openObject(); // TODO build object properly - see original CreateFromBlocks -> AqlValue.cpp:1093
      for (std::string name : infos.getVariableNames()) {
        builder.add(VPackValue(name));
      }
      builder.close();
      // copy variables / keep variables into result register
      /*
      output.cloneValueInto(_infos.getCollectRegister(), input,  // TODO check
      move _currentGroup.currentInput.getValue()
                                AqlValue::CreateFromBlocks(_infos.getTransaction(),
                                                           _currentGroup.groupBlocks,
                                                           _infos.getVariableNames()));*/
    }
  }
  //}
}

std::pair<ExecutionState, NoStats> SortedCollectExecutor::produceRow(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("SortedCollectExecutor::produceRow") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  ExecutionState state;
  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  while (true) {
    if (_returnedDone) {
      LOG_DEVEL << "We're finally done.";
      return {ExecutionState::DONE, {}};
    }

    std::tie(state, input) = _fetcher.fetchRow();
    LOG_DEVEL << "fetched a new row, state is: " << state;

    // set the indicator, that we've fetched all rows
    // only return done in the next loop iteration, after we received DONE from fetcher
    if (state == ExecutionState::DONE) {
      LOG_DEVEL << "We're almost done.";
      _returnedDone = true;
    }

    if (state == ExecutionState::WAITING) {
      LOG_DEVEL << "returned waiting";
      return {state, {}};
    }

    // check if we're first within the loop
    if (!_fetchedFirstRow) {
      _currentGroup.reset(input);  // reset and recreate new group
      _fetchedFirstRow = true;
      break;
    }

    // if we are in the same group, we need to add lines to the current group
    if (_currentGroup.isSameGroup(input) && !_returnedDone) { // << returnedDone to check wheter we hit the last row
      LOG_DEVEL << "we found same group";
      _currentGroup.addLine(input);
    } else {
      LOG_DEVEL << "we found another group, write output now";
      _currentGroup.writeToOutput(input, output);
      _currentGroup.reset(input);  // reset and recreate new group
      break;
    }

    // This step is to check wheter we are in the last available input row to come
    // We could reduce / optimize this later on
    if (_returnedDone) {
      _currentGroup.writeToOutput(input, output);
      _currentGroup.reset(input);  // reset and recreate new group
    }


    // TODO: store last or first input row, (last iteration)
    // TODO !!! use invalid row for writing to the output !!!!

    /* // I think we step out here too early, we need to loop 1x more, then quit with done
    if (state == ExecutionState::DONE) {
      LOG_DEVEL << "returned done";
      return {state, {}};
    }*/

    LOG_DEVEL << "returned hasmore";
    return {ExecutionState::HASMORE, {}};
  }
}

// TODOS:
// 1.) Verify that addValues() is emplacing/inserting the correct values to groupValues!
// 2.) Check if we can use an empty InputAqlItemRow for writing to the output (last row / write will
// not have an initialized rw
// 3.) IMPORTANT: Somewhere right now, we're writing the wrong data to the output rows..


// use ./scripts/unittest shell_server_aql --test aql-optimizer-collect-aggregate.js for quick debuging,
// later on all tests of course