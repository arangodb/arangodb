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


#ifndef ARANGOD_AQL_SORT_EXECUTOR_H
#define ARANGOD_AQL_SORT_EXECUTOR_H

#include "Aql/ExecutionState.h"

#include "Aql/ExecutorInfos.h"

#include <memory>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

class AllRowsFetcher;
class AqlItemMatrix;
class ExecutorInfos;
class NoStats;
class OutputAqlItemRow;
struct SortRegister;

class SortExecutorInfos : public ExecutorInfos {
 public:
  SortExecutorInfos(std::vector<SortRegister> sortRegisters,
                    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                    std::unordered_set<RegisterId> registersToClear,
                    transaction::Methods* trx, bool stable);

  SortExecutorInfos() = delete;
  SortExecutorInfos(SortExecutorInfos &&) = default;
  SortExecutorInfos(SortExecutorInfos const&) = delete;
  ~SortExecutorInfos() = default;

  arangodb::transaction::Methods* trx() const;

  std::vector<SortRegister> const& sortRegisters() const;

  bool stable() const;

 private:
  arangodb::transaction::Methods* _trx;
  std::vector<SortRegister> _sortRegisters;
  bool _stable;
};

/**
 * @brief Implementation of Sort Node
 */
class SortExecutor {
 public:
  using Fetcher = AllRowsFetcher;
  using Infos = SortExecutorInfos;
  using Stats = NoStats;

  SortExecutor(Fetcher& fetcher, SortExecutorInfos&);
  ~SortExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

 private:
  void doSorting();

 public:
  SortExecutorInfos& _infos;

 private:
  Fetcher& _fetcher;


  AqlItemMatrix const* _input;

  std::vector<size_t> _sortedIndexes;

  size_t _returnNext;
};
}  // namespace aql
}  // namespace arangodb

#endif
