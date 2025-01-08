////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Aql/IndexDistrinctScan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Aql/QueryContext.h"
#include "Transaction/Methods.h"

namespace arangodb {

namespace aql {
struct AqlCall;
class OutputAqlItemRow;

struct IndexDistinctScanInfos {
  // Associated document collection for this index
  Collection const* collection;
  // Index handle
  transaction::Methods::IndexHandle index;

  // Groups used for distinct scan
  std::vector<RegisterId> groupRegisters;

  IndexDistinctScanOptions scanOptions;

  QueryContext* query;
};

struct IndexDistinctScanStats {
  uint64_t seeks = 0;

  void operator+=(IndexDistinctScanStats const& stats) noexcept {
    seeks += stats.seeks;
  }
};

inline ExecutionStats& operator+=(
    ExecutionStats& executionStats,
    IndexDistinctScanStats const& stats) noexcept {
  executionStats.seeks += stats.seeks;
  return executionStats;
}

struct IndexDistinctScanExecutor {
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Stats = IndexDistinctScanStats;
  using Infos = IndexDistinctScanInfos;

  IndexDistinctScanExecutor() = delete;
  IndexDistinctScanExecutor(IndexDistinctScanExecutor&&) = delete;
  IndexDistinctScanExecutor(IndexDistinctScanExecutor const&) = delete;
  IndexDistinctScanExecutor(Fetcher& fetcher, Infos&);

  auto produceRows(AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  auto skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& clientCall)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

 private:
  void constructIterator();

  Fetcher& _fetcher;
  Infos& _infos;
  transaction::Methods _trx;
  std::unique_ptr<AqlIndexDistinctScanIterator> _iterator;
  std::vector<velocypack::Slice> _groupValues;
};

}  // namespace aql
}  // namespace arangodb
