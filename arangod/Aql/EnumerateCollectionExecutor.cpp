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

#include "EnumerateCollectionExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"

#include <lib/Logger/LogMacros.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

EnumerateCollectionExecutor::EnumerateCollectionExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher){};
EnumerateCollectionExecutor::~EnumerateCollectionExecutor() = default;

std::pair<ExecutionState, EnumerateCollectionStats> EnumerateCollectionExecutor::produceRow(
    OutputAqlItemRow& output) {
  TRI_IF_FAILURE("EnumerateCollectionExecutor::produceRow") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  ExecutionState state;
  EnumerateCollectionStats stats{};
  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  /*
  if (!waitForSatellites()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC,
                                   "collection " + _collection->name() +
                                       " did not come into sync in time (" +
                                       std::to_string(maxWait) + ")");
  }*/

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

    if (true) {
      output.copyRow(input);
      return {state, stats};
    } else {
      // stats.incrEnumerateCollectioned();
    }

    if (state == ExecutionState::DONE) {
      return {state, stats};
    }
    TRI_ASSERT(state == ExecutionState::HASMORE);
  }
}

EnumerateCollectionExecutorInfos::EnumerateCollectionExecutorInfos(
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear,
    aql::Collection& collection, ExecutionEngine& engine)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(),
                    std::make_shared<std::unordered_set<RegisterId>>(), nrInputRegisters,
                    nrOutputRegisters, std::move(registersToClear)),
      _collection(collection),
      _engine(engine) {}

#ifndef USE_ENTERPRISE
bool EnumerateCollectionExecutor::waitForSatellites() const { return true; }
#endif
