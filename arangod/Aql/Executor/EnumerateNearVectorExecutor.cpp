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
    : _inputRow(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _infos(infos),
      _trx(_infos.queryContext.newTrxContext()),
      _collection(_infos.collection) {}

void EnumerateNearVectorsExecutor::fillInput(
    AqlItemBlockInputRange& inputRange) {
  auto docRegId = _infos.inputReg;

  auto [state, _inputRow] =
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
}

void EnumerateNearVectorsExecutor::searchResults() {
  auto* vectorIndex = dynamic_cast<RocksDBVectorIndex*>(_infos.index.get());
  TRI_ASSERT(vectorIndex != nullptr);

  RocksDBMethods* mthds =
      RocksDBTransactionState::toMethods(&_trx, _collection->id());

  std::tie(_labels, _distances) = vectorIndex->readBatch(
      _inputRowConverted, _infos.searchParameters, mthds, &_trx,
      _collection->getCollection(), 1, _infos.getNumberOfResults());
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

  if (_currentProcessedResultCount == _infos.getNumberOfResults()) {
    _resultsAreProcessed = true;
  }
}

std::tuple<ExecutorState, NoStats, AqlCall>
EnumerateNearVectorsExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                          OutputAqlItemRow& output) {
  LOG_INTERNAL << "hasDataRow: " << inputRange.hasDataRow();
  while (!output.isFull()) {
    if (!_initialized) {
      if (!_inputRow.isInitialized()) {
        fillInput(inputRange);
      }

      searchResults();
    }
    fillOutput(output);
    if (_currentProcessedResultCount == _infos.getNumberOfResults()) {
      inputRange.advanceDataRow();
      _initialized = false;
      break;
    }
  }

  return {inputRange.upstreamState(), {}, output.getClientCall()};
}

[[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall>
EnumerateNearVectorsExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                            AqlCall& call) {
  std::size_t skipped{0};
  LOG_INTERNAL << "call: " << call << " fullCount: " << std::boolalpha
               << call.fullCount << " needSkipMore: " << std::boolalpha
               << call.needSkipMore()
               << ", hasInputRange: " << inputRange.hasDataRow()
               << ", resultsAreProcessed: " << _resultsAreProcessed;
  while (call.needSkipMore() &&
         (inputRange.hasDataRow() || !_resultsAreProcessed)) {
    // get an input row first, if necessary
    if (!_inputRow.isInitialized() && !_initialized) {
      std::tie(_state, _inputRow) = inputRange.peekDataRow();

      if (_inputRow.isInitialized()) {
        fillInput(inputRange);
        searchResults();
      } else {
        break;
      }
    }

    LOG_INTERNAL << "offset: " << call.getOffset()
                 << ", _currentProcessedResultCount: "
                 << _currentProcessedResultCount;
    if (call.getOffset() == 0) {
      TRI_ASSERT(call.needsFullCount())
          << "if we need to skip and skip is 0 fullCount must be true";
      LOG_INTERNAL << "needsFullCount is set";

      skipped = ExecutionBlock::SkipAllSize();
      break;
    }

    if (call.getOffset() == 0) {
      TRI_ASSERT(call.needsFullCount());
      // we are done
      skipped = ExecutionBlock::SkipAllSize();
      inputRange.advanceDataRow();
      _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
      _initialized = false;
      continue;
    }

    while (call.getOffset() > 0) {
      if (_labels[_currentProcessedResultCount] == -1) {
        // we are done, since faiss return -1 for nonexistent results
        skipped = ExecutionBlock::SkipAllSize();
        _resultsAreProcessed = true;
        inputRange.advanceDataRow();
        _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
        _initialized = false;
        call.didSkip(skipped);
        continue;
      }
      LOG_INTERNAL << "Skipping one, current: " << _currentProcessedResultCount;
      ++_currentProcessedResultCount;
      skipped += 1;
      call.didSkip(1);
    }

    LOG_INTERNAL << "done with checking searchResults offset: "
                 << call.getOffset();
  }

  if (call.needsFullCount() && _resultsAreProcessed) {
    LOG_INTERNAL << "we have fullCount and all resultsAreProcessed, this is "
                    "possible after produceRows";
    _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
    skipped = ExecutionBlock::SkipAllSize();
    inputRange.advanceDataRow();
    _initialized = false;
    call.didSkip(skipped);
    _state = ExecutorState::DONE;
  }

  auto state = std::invoke([&]() {
    if (_inputRow.isInitialized() && !_resultsAreProcessed) {
      return ExecutorState::HASMORE;
    }
    return _state;
  });

  AqlCall upstreamCall;
  LOG_INTERNAL << "state: " << state << ", skipped: " << skipped
               << ", upstreamCall: " << upstreamCall
               << ", resultsAreProcessed: " << _resultsAreProcessed;
  return {state, {}, skipped, upstreamCall};
}

template class ExecutionBlockImpl<EnumerateNearVectorsExecutor>;

}  // namespace arangodb::aql
