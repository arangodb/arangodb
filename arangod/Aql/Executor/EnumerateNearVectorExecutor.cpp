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

  std::size_t vectorComponentsCount{0};
  for (arangodb::velocypack::ArrayIterator itr(value.slice()); itr.valid();
       ++itr, ++vectorComponentsCount) {
    _inputRowConverted.push_back(itr.value().getDouble());
  }

  if (vectorComponentsCount !=
      _infos.index->getVectorIndexDefinition().dimension) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
        fmt::format("a vector must be of dimension {}, but is {}",
                    _infos.index->getVectorIndexDefinition().dimension,
                    vectorComponentsCount));
  }
}

void EnumerateNearVectorsExecutor::searchResults() {
  auto* vectorIndex = dynamic_cast<RocksDBVectorIndex*>(_infos.index.get());
  TRI_ASSERT(vectorIndex != nullptr);

  RocksDBMethods* mthds =
      RocksDBTransactionState::toMethods(&_trx, _collection->id());

  std::tie(_labels, _distances) = vectorIndex->readBatch(
      _inputRowConverted, mthds, &_trx, _collection->getCollection(), 1,
      _infos.getNubmerOfResults());
  _initialized = true;
}

void EnumerateNearVectorsExecutor::fillOutput(OutputAqlItemRow& output) {
  auto const docOutId = _infos.outDocumentIdReg;
  auto const distOutId = _infos.outDistancesReg;

  while (!output.isFull() &&
         _currentProcessedResultCount < (_infos.topK + _infos.offset)) {
    // there are no results anymore for this input, so we can skip to next input
    // row
    if (_labels[_currentProcessedResultCount] == -1) {
      _currentProcessedResultCount = _infos.getNubmerOfResults();
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
}

void EnumerateNearVectorsExecutor::initializeCursor() {
  _state = ExecutorState::HASMORE;
  _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  _initialized = false;
  _inputRowConverted.clear();
  _currentProcessedResultCount = 0;
}

std::tuple<ExecutorState, NoStats, AqlCall>
EnumerateNearVectorsExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                          OutputAqlItemRow& output) {
  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;

  while (!output.isFull()) {
    if (!_initialized) {
      if (!_inputRow.isInitialized()) {
        fillInput(inputRange);
      }

      searchResults();
    }
    fillOutput(output);
    if (_currentProcessedResultCount == _infos.getNubmerOfResults()) {
      inputRange.advanceDataRow();
      return {ExecutorState::DONE, {}, output.getClientCall()};
    }
  }

  return {inputRange.upstreamState(), {}, output.getClientCall()};
}

[[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall>
EnumerateNearVectorsExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                            AqlCall& call) {
  std::size_t skipped{0};
  while (call.needSkipMore()) {
    // get an input row first, if necessary
    if (!_inputRow.isInitialized() && !_initialized) {
      std::tie(_state, _inputRow) = inputRange.peekDataRow();

      if (_inputRow.isInitialized()) {
        fillInput(inputRange);
      } else {
        break;
      }
    }
    if (!_initialized) {
      searchResults();
    }

    skipped = call.getOffset();
    _currentProcessedResultCount += skipped;
    call.didSkip(skipped);
  }

  AqlCall upstreamCall;

  return {returnState(), {}, skipped, std::move(upstreamCall)};
}

auto EnumerateNearVectorsExecutor::returnState() const noexcept
    -> ExecutorState {
  if (_inputRow.isInitialized()) {
    // We are still working.
    // TODO: Potential optimization: We can ask if the cursor has more, or there
    // are other cursors.
    return ExecutorState::HASMORE;
  }
  return _state;
}

template class ExecutionBlockImpl<EnumerateNearVectorsExecutor>;

}  // namespace arangodb::aql
