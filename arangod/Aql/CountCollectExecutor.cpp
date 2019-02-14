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

#include "CountCollectExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"

#include <lib/Logger/LogMacros.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

CountCollectExecutorInfos::CountCollectExecutorInfos(RegisterId collectRegister,
                                                     RegisterId nrInputRegisters,
                                                     RegisterId nrOutputRegisters,
                                                     std::unordered_set<RegisterId> registersToClear)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(),
                    make_shared_unordered_set({collectRegister}), nrInputRegisters,
                    nrOutputRegisters, std::move(registersToClear)),
      _collectRegister(collectRegister) {}

CountCollectExecutor::CountCollectExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher), _count(0){};

CountCollectExecutor::~CountCollectExecutor() = default;

std::pair<ExecutionState, NoStats> CountCollectExecutor::produceRow(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("CountCollectExecutor::produceRow") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  NoStats stats{};
  InputAqlItemRow input{CreateInvalidInputRowHint{}};
  ExecutionState state;

  while (true) {
    std::tie(state, input) = _fetcher.fetchRow();

    if (state == ExecutionState::WAITING) {
      return {state, stats};
    }

    if (!input) {
      TRI_ASSERT(state == ExecutionState::DONE);
      output.cloneValueInto(_infos.getOutputRegisterId(), input,
                            AqlValue(AqlValueHintUInt(static_cast<uint64_t>(getCount()))));
      return {state, stats};
    }

    TRI_ASSERT(input.isInitialized());
    incrCount();

    // Abort if upstream is done
    if (state == ExecutionState::DONE) {
      output.cloneValueInto(_infos.getOutputRegisterId(), input,
                            AqlValue(AqlValueHintUInt(static_cast<uint64_t>(getCount()))));
      return {state, stats};
    }
  }
}
