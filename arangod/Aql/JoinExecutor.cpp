////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Aql/JoinExecutor.h"
#include "Aql/IndexMerger.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Collection.h"

using namespace arangodb::aql;

JoinExecutor::~JoinExecutor() = default;

JoinExecutor::JoinExecutor(Fetcher& fetcher, Infos& infos)
    : _fetcher(fetcher), _infos(infos) {
  _merger = std::make_unique<AqlIndexMerger>(std::move(infos.indexes), 1);
}

auto JoinExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                               OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TRI_ASSERT(!output.isFull());

  _merger->next([&](std::span<LocalDocumentId> docIds,
                    std::span<VPackSlice> projections) -> bool {
    for (std::size_t k = 0; k < docIds.size(); k++) {
      auto reg = _infos.registers[k];
    }

    return !output.isFull();
  });

  return {ExecutorState::DONE, Stats{}, AqlCall{}};
}

auto JoinExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                 AqlCall& clientCall)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  (void)_fetcher;
  (void)_infos;
  return {ExecutorState::DONE, Stats{}, 0, AqlCall{}};
}
