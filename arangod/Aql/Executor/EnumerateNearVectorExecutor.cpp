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
///
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#include "EnumerateNearVectorExecutor.h"

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "Aql/ExecutionBlockImpl.tpp"

#include <cmath>

// Activate logging
#define LOG_ENABLED false
#define LOG_INTERNAL \
  LOG_DEVEL_IF((LOG_ENABLED)) << __FUNCTION__ << ":" << __LINE__ << " "

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
    AqlItemBlockInputRange& inputRange) {
  auto docRegId = _infos.inputReg;

  std::tie(_state, _inputRow) =
      inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});

  AqlValue value = _inputRow.getValue(docRegId);

  // TODO currently we do not accept anything else then array
  if (!value.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
        basics::StringUtils::concatT("query point must be a vector, but is a ",
                                     value.getTypeString()));
  }

  auto const dimension = _infos.index->getVectorIndexDefinition().dimension;
  _inputRowConverted.reserve(dimension);
  std::size_t vectorComponentsCount{0};
  for (arangodb::velocypack::ArrayIterator itr(value.slice()); itr.valid();
       ++itr, ++vectorComponentsCount) {
    _inputRowConverted.push_back(itr.value().getNumericValue<double>());
  }

  if (vectorComponentsCount != dimension) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
        fmt::format("a vector must be of dimension {}, but is {}", dimension,
                    vectorComponentsCount));
  }
  ++_processedInputs;

  searchResults();
}

void EnumerateNearVectorsExecutor::searchResults() {
  auto* vectorIndex = dynamic_cast<RocksDBVectorIndex*>(_infos.index.get());
  TRI_ASSERT(vectorIndex != nullptr);

  RocksDBMethods* mthds =
      RocksDBTransactionState::toMethods(&_trx, _collection->id());

  std::tie(_labels, _distances) = vectorIndex->readBatch(
      _inputRowConverted, mthds, &_trx, _collection->getCollection(), 1,
      _infos.getNumberOfResults());
  _currentProcessedResultCount = 0;
  TRI_ASSERT(hasResults());
  LOG_INTERNAL << "Results: " << _labels << " and distances: " << _distances;
}

void EnumerateNearVectorsExecutor::fillOutput(OutputAqlItemRow& output) {
  auto const docOutId = _infos.outDocumentIdReg;
  auto const distOutId = _infos.outDistancesReg;

  while (!output.isFull() &&
         _currentProcessedResultCount < _infos.getNumberOfResults()) {
    // there are no results anymore for this input, so we can skip to next input
    // row
    if (_labels[_currentProcessedResultCount] == -1) {
      _currentProcessedResultCount = _infos.getNumberOfResults();
      break;
    }
    output.moveValueInto(
        docOutId, _inputRow,
        AqlValueHintUInt(_labels[_currentProcessedResultCount]));
    output.moveValueInto(
        distOutId, _inputRow,
        AqlValueHintDouble(_distances[_currentProcessedResultCount]));
    output.advanceRow();

    ++_currentProcessedResultCount;
  }
  TRI_ASSERT(hasResults() !=
             (_currentProcessedResultCount == _infos.getNumberOfResults()));
  // Must clear the input so the readBatch in RocksDBVectorIndex has the correct
  // size
  _inputRowConverted.clear();
}

std::uint64_t EnumerateNearVectorsExecutor::skipOutput(
    AqlCall::Limit toSkip) noexcept {
  auto skipped = 0;

  while (skipped < toSkip &&
         _currentProcessedResultCount < _infos.getNumberOfResults()) {
    // there are no results anymore for this input, so we can skip to next input
    // row
    if (_labels[_currentProcessedResultCount] == -1) {
      _currentProcessedResultCount = _infos.getNumberOfResults();
      break;
    }
    ++skipped;

    ++_currentProcessedResultCount;
  }
  TRI_ASSERT(hasResults() !=
             (_currentProcessedResultCount == _infos.getNumberOfResults()));

  return skipped;
}

std::tuple<ExecutorState, NoStats, AqlCall>
EnumerateNearVectorsExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                          OutputAqlItemRow& output) {
  LOG_INTERNAL << "hasDataRow: " << inputRange.hasDataRow();
  while (!output.isFull() && (inputRange.hasDataRow() || hasResults())) {
    if (!hasResults()) {
      fillInput(inputRange);
    }
    if (hasResults()) {
      fillOutput(output);
    }
  }

  return {inputRange.upstreamState(), {}, /*output.getClientCall()*/ {}};
}

[[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall>
EnumerateNearVectorsExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                            AqlCall& call) {
  auto state = [&] {
    auto state = inputRange.upstreamState();
    if (hasResults()) {
      state = ExecutorState::HASMORE;
    }
    return state;
  };
  if (call.getOffset() > 0) {
    auto skipped = std::uint64_t{};
    while (call.needSkipMore() && (inputRange.hasDataRow() || hasResults())) {
      if (!hasResults()) {
        fillInput(inputRange);
      }
      if (hasResults()) {
        auto skippedLocal = skipOutput(call.getOffset());
        skipped += skippedLocal;
        call.didSkip(skippedLocal);
      }
    }

    return {state(), {}, skipped, {}};
  } else if (call.needSkipMore()) {
    TRI_ASSERT(call.needsFullCount());
    auto const colCount =
        _collection->count(&_trx, transaction::CountType::kNormal);
    auto skipped = skipOutput(AqlCall::Infinity{});
    skipped += (colCount - _infos.getNumberOfResults());
    TRI_ASSERT(!hasResults());
    auto remainingRows = inputRange.countAndSkipAllRemainingDataRows();
    // remainingRows += inputRange.skipAll();
    // TODO call _collection->count only once
    skipped += remainingRows * colCount;
    call.didSkip(skipped);

    //    LOG_INTERNAL << fmt::format(
    //        "skipped={}, remainingRows={}, currentProcessed={}, nr={},
    //        state={}, " "hasResults={}, call={}", skipped, remainingRows,
    //        _currentProcessedResultCount, _infos.getNumberOfResults(),
    //        state(), hasResults(), to_string(call));

    auto upstreamCall = AqlCall{};
    // upstreamCall.fullCount = true;
    // upstreamCall.hardLimit = std::uint64_t{0};

    return {state(), {}, skipped, upstreamCall};
  }

  return {state(), {}, 0, {}};
}

bool EnumerateNearVectorsExecutor::hasResults() const noexcept {
  return _currentProcessedResultCount < _infos.getNumberOfResults();
}

template class ExecutionBlockImpl<EnumerateNearVectorsExecutor>;

}  // namespace arangodb::aql
