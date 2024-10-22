////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "EnumerateNearVectorExecutor.h"

#include "Aql/AqlItemBlockInputRange.h"
#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "Aql/ExecutionBlockImpl.tpp"

#include <cmath>

namespace arangodb::aql {

struct AqlCall;
class AqlItemBlockInputRange;
class InputAqlItemRow;
class RegisterInfos;
struct Collection;

using Stats = NoStats;

EnumerateNearVectorsExecutor::EnumerateNearVectorsExecutor(Fetcher& /*unused*/,
                                                           Infos& infos)
    : _infos(infos),
      _trx(_infos.queryContext.newTrxContext()),
      _collection(_infos.collection) {}

void EnumerateNearVectorsExecutor::fillInput(
    AqlItemBlockInputRange& inputRange, std::vector<float>& inputRowsJoined) {
  auto docRegId = _infos.inputReg;

  while (inputRange.hasDataRow()) {
    auto [state, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});

    AqlValue value = input.getValue(docRegId);
    std::vector<float> vectorInput;
    vectorInput.reserve(_infos.index->getVectorIndexDefinition().dimensions);

    // TODO currently we do not accept anything else then array
    if (!value.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
          "query point must be a vector");
    }

    // TODO optimize double allocation with vectorInput and inputRows2
    if (auto res =
            velocypack::deserializeWithStatus(value.slice(), vectorInput);
        !res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
          "could not read input query point");
    }
    if (vectorInput.size() !=
        _infos.index->getVectorIndexDefinition().dimensions) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
          fmt::format("a vector must be of dimension {}",
                      _infos.index->getVectorIndexDefinition().dimensions));
    }

    _inputRows.emplace_back(input);
    inputRowsJoined.insert(inputRowsJoined.end(), vectorInput.begin(),
                           vectorInput.end());
  }
}

void EnumerateNearVectorsExecutor::searchResults(
    std::vector<float>& inputRowsJoined) {
  auto* vectorIndex = dynamic_cast<RocksDBVectorIndex*>(_infos.index.get());
  TRI_ASSERT(vectorIndex != nullptr);

  std::vector<float> convertedInputRows(_inputRows.size());
  RocksDBMethods* mthds =
      RocksDBTransactionState::toMethods(&_trx, _collection->id());

  std::tie(_labels, _distances) = vectorIndex->readBatch(
      inputRowsJoined, mthds, &_trx, _collection->getCollection(),
      _inputRows.size(), _infos.topK);

  _initialized = true;
  _currentProcessedResultCount = 0;
}

void EnumerateNearVectorsExecutor::fillOutput(OutputAqlItemRow& output) {
  auto const docOutId = _infos.outDocumentIdReg;
  auto const distOutId = _infos.outDistancesReg;

  auto inputRowIterator = _inputRows.begin();
  while (!output.isFull() &&
         _currentProcessedResultCount < _inputRows.size() * _infos.topK) {
    // there are no results anymore for this input, so we can skip to next input
    // row
    if (_labels[_currentProcessedResultCount] == -1) {
      _currentProcessedResultCount =
          (_currentProcessedResultCount / _infos.topK + 1) * _infos.topK;
      ++inputRowIterator;
      continue;
    }
    output.moveValueInto(
        docOutId, *inputRowIterator,
        AqlValueHintUInt(_labels[_currentProcessedResultCount]));
    output.moveValueInto(
        distOutId, *inputRowIterator,
        AqlValueHintDouble(_distances[_currentProcessedResultCount]));
    output.advanceRow();

    ++_currentProcessedResultCount;
    if (_currentProcessedResultCount % _infos.topK == 0) {
      ++inputRowIterator;
    }
  }
}

std::tuple<ExecutorState, NoStats, AqlCall>
EnumerateNearVectorsExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                          OutputAqlItemRow& output) {
  NoStats stats;

  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;

  std::vector<float> inputRowsJoined;
  while (inputRange.hasDataRow() && !output.isFull() &&
         _currentProcessedResultCount == _inputRows.size() * _infos.topK) {
    _initialized = false;
    fillInput(inputRange, inputRowsJoined);
  }
  if (_inputRows.empty()) {
    return {inputRange.upstreamState(), stats, output.getClientCall()};
  }

  if (!_initialized && !_inputRows.empty()) {
    searchResults(inputRowsJoined);
  }

  if (_inputRows.size() != 0) {
    fillOutput(output);
  }

  return {inputRange.upstreamState(), stats, output.getClientCall()};
}

[[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall>
EnumerateNearVectorsExecutor::skipRowsRange(AqlItemBlockInputRange&  /*inputRange*/,
                                            AqlCall&  /*call*/) {
  return {};
}

template class ExecutionBlockImpl<EnumerateNearVectorsExecutor>;

}  // namespace arangodb::aql
