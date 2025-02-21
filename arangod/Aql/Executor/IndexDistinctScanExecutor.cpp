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

#include "IndexDistinctScanExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionBlockImpl.tpp"

namespace arangodb::aql {

auto IndexDistinctScanExecutor::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& clientCall)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  std::size_t skipped = 0;
  uint64_t seeks = 0;
  while (inputRange.hasDataRow() && clientCall.needSkipMore()) {
    seeks += 1;
    bool hasMore = _iterator->next(_groupValues);
    if (not hasMore) {
      inputRange.advanceDataRow();
      continue;
    }

    clientCall.didSkip(1);
    skipped += 1;
  }

  return std::make_tuple(inputRange.upstreamState(),
                         IndexDistinctScanStats{seeks}, skipped, AqlCall{});
}

auto IndexDistinctScanExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                            OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  uint64_t seeks = 0;
  while (inputRange.hasDataRow() && !output.isFull()) {
    seeks += 1;
    bool hasMore = _iterator->next(_groupValues);
    if (not hasMore) {
      inputRange.advanceDataRow();
      continue;
    }

    for (size_t k = 0; k < _infos.groupRegisters.size(); k++) {
      output.moveValueInto(_infos.groupRegisters[k],
                           inputRange.peekDataRow().second, _groupValues[k]);
    }
    output.advanceRow();
  }

  return std::make_tuple(inputRange.upstreamState(),
                         IndexDistinctScanStats{seeks}, AqlCall{});
}

IndexDistinctScanExecutor::IndexDistinctScanExecutor(Fetcher& fetcher,
                                                     Infos& infos)
    : _fetcher(fetcher), _infos(infos), _trx{_infos.query->newTrxContext()} {
  constructIterator();
  _groupValues.resize(_infos.groupRegisters.size());
}

void IndexDistinctScanExecutor::constructIterator() {
  _iterator = _infos.index->distinctScanFor(&_trx, _infos.scanOptions);
  ADB_PROD_ASSERT(_iterator != nullptr);
}

template class ExecutionBlockImpl<IndexDistinctScanExecutor>;

}  // namespace arangodb::aql
