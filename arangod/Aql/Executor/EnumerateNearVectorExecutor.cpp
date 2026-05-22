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
#include "Aql/Variable.h"
#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "Indexes/IndexIterator.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
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

void EnumerateNearVectorsExecutor::writeProjectionsFromDocument(
    velocypack::Slice docSlice, OutputAqlItemRow& output) {
  if (_infos.projections.empty() || _infos.projectionVarsToRegs.empty()) {
    return;
  }
  _infos.projections.produceFromDocument(
      _projectionsBuilder, docSlice, &_trx,
      [&](Variable const* variable, velocypack::Slice slice) {
        if (slice.isNone()) {
          slice = VPackSlice::nullSlice();
        }
        auto it = _infos.projectionVarsToRegs.find(variable->id);
        TRI_ASSERT(it != _infos.projectionVarsToRegs.end());
        output.moveValueInto(it->second, _inputRow, slice);
      });
}

void EnumerateNearVectorsExecutor::writeProjectionsFromStoredValues(
    velocypack::Slice storedValuesSlice, OutputAqlItemRow& output) {
  if (_infos.projections.empty() || _infos.projectionVarsToRegs.empty()) {
    return;
  }
  IndexIterator::SliceCoveringData covering{storedValuesSlice};
  _infos.projections.produceFromIndex(
      _projectionsBuilder, covering, &_trx,
      [&](Variable const* variable, velocypack::Slice slice) {
        if (slice.isNone()) {
          slice = VPackSlice::nullSlice();
        }
        auto it = _infos.projectionVarsToRegs.find(variable->id);
        TRI_ASSERT(it != _infos.projectionVarsToRegs.end());
        output.moveValueInto(it->second, _inputRow, slice);
      });
}

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

  // Must clear the input so the readBatch in RocksDBVectorIndex has the correct
  // size
  _inputRowConverted.clear();

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
        std::format("a vector must be of dimension {}, but is {}", dimension,
                    vectorComponentsCount));
  }
  ++_processedInputs;
  _reportedCurrentRowForFullCount = false;

  searchResults();
}

void EnumerateNearVectorsExecutor::searchResults() {
  auto* vectorIndex = dynamic_cast<RocksDBVectorIndex*>(_infos.index.get());
  TRI_ASSERT(vectorIndex != nullptr);

  vector::VectorSearchContext ctx{
      .inputs = &_inputRowConverted,
      .inputRow = &_inputRow,
      .trx = &_trx,
      .queryContext = &_infos.queryContext,
  };
  auto result = vectorIndex->readBatch(_infos.searchConfig, ctx);
  _labels = std::move(result.labels);
  _distances = std::move(result.distances);
  _documents = std::move(result.capturedDocuments);
  _currentProcessedResultCount = 0;

  TRI_ASSERT(hasResults());

  auto validCount = std::count_if(_labels.begin(), _labels.end(),
                                  [](auto const& l) { return l != -1; });
  LOG_TOPIC("f1a2b", DEBUG, Logger::ENGINES) << std::format(
      "EnumerateNearVectors::searchResults: requested={}, returned={}, "
      "validLabels={}, collectionCount={}",
      _infos.searchConfig.topK, _labels.size(), validCount, _collectionCount);

  LOG_INTERNAL << "Results: " << _labels << " and distances: " << _distances;
}

void EnumerateNearVectorsExecutor::fillOutput(OutputAqlItemRow& output) {
  auto const docOutId = _infos.outDocumentIdReg;
  auto const distOutId = _infos.outDistancesReg;

  while (!output.isFull() && _currentProcessedResultCount < _labels.size()) {
    // there are no results anymore for this input, so we can skip to next input
    // row
    if (_labels[_currentProcessedResultCount] == -1) {
      _labels.resize(_currentProcessedResultCount);
      break;
    }

    switch (_infos.projectionMode) {
      case vector::ProjectionMode::kPassThroughId: {
        TRI_ASSERT(docOutId != RegisterId::maxRegisterId);
        output.moveValueInto(
            docOutId, _inputRow,
            AqlValueHintUInt(_labels[_currentProcessedResultCount]));
        break;
      }
      case vector::ProjectionMode::kDocument: {
        LocalDocumentId const id{static_cast<LocalDocumentId::BaseType>(
            _labels[_currentProcessedResultCount])};
        auto it = _documents.find(id);
        TRI_ASSERT(it != _documents.end());
        velocypack::Slice docSlice = it->second.slice();
        if (docOutId != RegisterId::maxRegisterId) {
          output.moveValueInto(docOutId, _inputRow, docSlice);
        }
        writeProjectionsFromDocument(docSlice, output);
        break;
      }
      case vector::ProjectionMode::kCovered: {
        // No doc register for kCovered (createBlock leaves docOutId unset);
        // projections come positionally out of the storedValues array.
        TRI_ASSERT(docOutId == RegisterId::maxRegisterId);
        LocalDocumentId const id{static_cast<LocalDocumentId::BaseType>(
            _labels[_currentProcessedResultCount])};
        auto it = _documents.find(id);
        TRI_ASSERT(it != _documents.end());
        writeProjectionsFromStoredValues(it->second.slice(), output);
        break;
      }
    }

    output.moveValueInto(
        distOutId, _inputRow,
        AqlValueHintDouble(_distances[_currentProcessedResultCount]));
    output.advanceRow();

    ++_currentProcessedResultCount;
  }
}

std::uint64_t EnumerateNearVectorsExecutor::skipOutput(
    AqlCall::Limit toSkip) noexcept {
  std::uint64_t skipped = 0;

  while (skipped < toSkip && _currentProcessedResultCount < _labels.size()) {
    // there are no results anymore for this input, so we can skip to next input
    // row
    if (_labels[_currentProcessedResultCount] == -1) {
      _labels.resize(_currentProcessedResultCount);
      break;
    }

    ++skipped;
    ++_currentProcessedResultCount;
  }

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

  if (inputRange.hasDataRow() || hasResults()) {
    return {ExecutorState::HASMORE, {}, {}};
  }

  return {inputRange.upstreamState(), {}, /*output.getClientCall()*/ {}};
}

// The fullCount will not behave as expected when there is less data produced
// then the limit is set to. E.g. If nLists is set to the number of documents,
// meaning every list in faiss index would have 1 document = centroid, and
// the limit is 3, nProbe is 1, fullCount true, then the maximum number of docs
// produced can be 1, but the fullCount  returned will not be valid since we
// cannot produce more documents then the limit, and we will not enter
// skipRowsRange
[[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall>
EnumerateNearVectorsExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                            AqlCall& call) {
  auto state = [&] {
    if (hasResults()) {
      return ExecutorState::HASMORE;
    }
    return inputRange.upstreamState();
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
    auto skipped = skipOutput(AqlCall::Infinity{});

    TRI_ASSERT(!hasResults());
    auto remainingRows = inputRange.countAndSkipAllRemainingDataRows();
    if (_processedInputs > 0 && !_reportedCurrentRowForFullCount) {
      skipped += _collectionCount - _currentProcessedResultCount;
      _reportedCurrentRowForFullCount = true;
    }
    skipped += remainingRows * _collectionCount;
    call.didSkip(skipped);

    LOG_TOPIC("f1a2c", DEBUG, Logger::ENGINES) << std::format(
        "EnumerateNearVectors::skipRowsRange(fullCount): skipped={}, "
        "remainingRows={}, currentProcessed={}, nr={}, state={}, "
        "hasResults={}, call={}, colCount={}",
        skipped, remainingRows, _currentProcessedResultCount,
        _infos.searchConfig.topK, state(), hasResults(), to_string(call),
        _collectionCount);

    auto upstreamCall = AqlCall{};

    return {state(), {}, skipped, upstreamCall};
  }

  return {state(), {}, 0, {}};
}

bool EnumerateNearVectorsExecutor::hasResults() const noexcept {
  return _currentProcessedResultCount < _labels.size();
}

template class ExecutionBlockImpl<EnumerateNearVectorsExecutor>;

}  // namespace arangodb::aql
