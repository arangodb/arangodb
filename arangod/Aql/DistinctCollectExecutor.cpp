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

#include "DistinctCollectExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"

#include <lib/Logger/LogMacros.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

DistinctCollectExecutorInfos::DistinctCollectExecutorInfos(
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep,
    std::unordered_set<RegisterId>&& readableInputRegisters,
    std::unordered_set<RegisterId>&& writeableInputRegisters,
    std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters,
    transaction::Methods* trxPtr)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(readableInputRegisters),
                    std::make_shared<std::unordered_set<RegisterId>>(writeableInputRegisters),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _groupRegisters(groupRegisters),
      _trxPtr(trxPtr) {
  TRI_ASSERT(!_groupRegisters.empty());
}

DistinctCollectExecutor::DistinctCollectExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _seen(1024,
            AqlValueGroupHash(_infos.getTransaction(),
                              _infos.getGroupRegisters().size()),
            AqlValueGroupEqual(_infos.getTransaction())) {}

DistinctCollectExecutor::~DistinctCollectExecutor() {
  destroyValues();
}

void DistinctCollectExecutor::initializeCursor() {
  destroyValues();
}

std::pair<ExecutionState, NoStats> DistinctCollectExecutor::produceRows(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("DistinctCollectExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  NoStats stats{};
  InputAqlItemRow input{CreateInvalidInputRowHint{}};
  ExecutionState state;

  std::vector<AqlValue> groupValues;
  groupValues.reserve(_infos.getGroupRegisters().size());

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

    groupValues.clear();
    // for hashing simply re-use the aggregate registers, without cloning
    // their contents
    for (auto& it : _infos.getGroupRegisters()) {
      groupValues.emplace_back(input.getValue(it.second));
    }

    // now check if we already know this group
    auto foundIt = _seen.find(groupValues);

    bool newGroup = foundIt == _seen.end();
    if (newGroup) {
      size_t i = 0;

      for (auto& it : _infos.getGroupRegisters()) {
        output.cloneValueInto(it.first, input, groupValues[i]);
        ++i;
      }

      // transfer ownership
      std::vector<AqlValue> copy;
      copy.reserve(groupValues.size());
      for (auto const& it : groupValues) {
        copy.emplace_back(it.clone());
      }
      _seen.emplace(std::move(copy));
    }

    // Abort if upstream is done
    if (state == ExecutionState::DONE) {
      return {state, stats};
    }

    return {ExecutionState::HASMORE, stats};
  }
}

std::pair<ExecutionState, size_t> DistinctCollectExecutor::expectedNumberOfRows(size_t atMost) const {
  // This block cannot know how many elements will be returned exactly.
  // but it is upper bounded by the input.
  return _fetcher.preFetchNumberOfRows(atMost);
}

void DistinctCollectExecutor::destroyValues() {
  // destroy all AqlValues captured
  for (auto& it : _seen) {
    for (auto& it2 : it) {
      const_cast<AqlValue*>(&it2)->destroy();
    }
  }
  _seen.clear();
}
