////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "MaterializeExecutor.h"

#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;

template <typename T>
arangodb::IndexIterator::DocumentCallback MaterializeExecutor<T>::ReadContext::copyDocumentCallback(
    ReadContext& ctx) {
  typedef std::function<arangodb::IndexIterator::DocumentCallback(ReadContext&)> CallbackFactory;
  static CallbackFactory const callbackFactory{
      [](ReadContext& ctx) {
        return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
          TRI_ASSERT(ctx._outputRow);
          TRI_ASSERT(ctx._inputRow);
          TRI_ASSERT(ctx._inputRow->isInitialized());
          TRI_ASSERT(ctx._infos);
          arangodb::aql::AqlValue a{arangodb::aql::AqlValueHintCopy(doc.begin())};
          bool mustDestroy = true;
          arangodb::aql::AqlValueGuard guard{a, mustDestroy};
          ctx._outputRow->moveValueInto(ctx._infos->outputMaterializedDocumentRegId(),
                                        *ctx._inputRow, guard);
          return true;
        };
      }
    };

  return callbackFactory(ctx);
}

template <typename T>
arangodb::aql::MaterializerExecutorInfos<T>::MaterializerExecutorInfos(
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToClear,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToKeep, T const collectionSource,
    RegisterId inNmDocId, RegisterId outDocRegId, transaction::Methods* trx)
    : ExecutorInfos(getReadableInputRegisters(collectionSource, inNmDocId),
                    make_shared_unordered_set(std::initializer_list<RegisterId>({outDocRegId})),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _collectionSource(collectionSource),
      _inNonMaterializedDocRegId(inNmDocId),
      _outMaterializedDocumentRegId(outDocRegId),
      _trx(trx) {}

template <typename T>
std::tuple<ExecutorState, NoStats, AqlCall> arangodb::aql::MaterializeExecutor<T>::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;

  while (inputRange.hasDataRow() && !output.isFull()) {
    bool written = false;

    // some micro-optimization
    auto& callback = _readDocumentContext._callback;
    auto docRegId = _readDocumentContext._infos->inputNonMaterializedDocRegId();
    T collectionSource = _readDocumentContext._infos->collectionSource();
    auto* trx = _readDocumentContext._infos->trx();
    auto const [state, input] = inputRange.nextDataRow();

    arangodb::LogicalCollection const* collection = nullptr;
    if constexpr (std::is_same<T, std::string const&>::value) {
      if (_collection == nullptr) {
        _collection = trx->documentCollection(collectionSource);
      }
      collection = _collection;
    } else {
      collection = reinterpret_cast<arangodb::LogicalCollection const*>(
          input.getValue(collectionSource).slice().getUInt());
    }
    TRI_ASSERT(collection != nullptr);
    _readDocumentContext._inputRow = &input;
    _readDocumentContext._outputRow = &output;
    written = collection->readDocumentWithCallback(
        trx, LocalDocumentId(input.getValue(docRegId).slice().getUInt()), callback);
    if (written) {
      output.advanceRow();
    }
  }

  return {inputRange.upstreamState(), NoStats{}, upstreamCall};
}

template <typename T>
std::tuple<ExecutorState, NoStats, size_t, AqlCall> arangodb::aql::MaterializeExecutor<T>::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& call) {
  size_t skipped = 0;

  if (call.getLimit() > 0) {
    // we can only account for offset
    skipped = inputRange.skip(call.getOffset());
  } else {
    skipped = inputRange.skipAll();
  }
  call.didSkip(skipped);

  return {inputRange.upstreamState(), NoStats{}, skipped, call};
}

template class ::arangodb::aql::MaterializeExecutor<RegisterId>;
template class ::arangodb::aql::MaterializeExecutor<std::string const&>;

template class ::arangodb::aql::MaterializerExecutorInfos<RegisterId>;
template class ::arangodb::aql::MaterializerExecutorInfos<std::string const&>;
