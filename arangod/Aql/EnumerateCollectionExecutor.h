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

#ifndef ARANGOD_AQL_ENUMERATECOLLECTION_EXECUTOR_H
#define ARANGOD_AQL_ENUMERATECOLLECTION_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Aql/types.h"

#include <memory>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class ExecutorInfos;
class SingleRowFetcher;

class EnumerateCollectionExecutorInfos : public ExecutorInfos {
 public:
  EnumerateCollectionExecutorInfos(RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                                   std::unordered_set<RegisterId> registersToClear,
                                   aql::Collection& collection,
                                   ExecutionEngine& engine);

  EnumerateCollectionExecutorInfos() = delete;
  EnumerateCollectionExecutorInfos(EnumerateCollectionExecutorInfos&&) = default;
  EnumerateCollectionExecutorInfos(EnumerateCollectionExecutorInfos const&) = delete;
  ~EnumerateCollectionExecutorInfos() = default;

 private:
  aql::Collection& _collection;
  ExecutionEngine& _engine;
};

/**
 * @brief Implementation of EnumerateCollection Node
 */
class EnumerateCollectionExecutor {
 public:
  using Fetcher = SingleRowFetcher;
  using Infos = EnumerateCollectionExecutorInfos;
  using Stats = EnumerateCollectionStats;

  EnumerateCollectionExecutor() = delete;
  EnumerateCollectionExecutor(EnumerateCollectionExecutor&&) = default;
  EnumerateCollectionExecutor(EnumerateCollectionExecutor const&) = delete;
  EnumerateCollectionExecutor(Fetcher& fetcher, Infos&);
  ~EnumerateCollectionExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

 private:
  bool waitForSatellites() const;
  Infos& _infos;
  Fetcher& _fetcher;
};

}  // namespace aql
}  // namespace arangodb

#endif
