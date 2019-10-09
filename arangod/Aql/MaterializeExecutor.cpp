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

#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/LogicalCollection.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"

using namespace arangodb;
using namespace arangodb::aql;

arangodb::IndexIterator::DocumentCallback MaterializeExecutor::ReadContext::copyDocumentCallback(ReadContext & ctx) {
  auto* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);
  typedef std::function<arangodb::IndexIterator::DocumentCallback(ReadContext&)> CallbackFactory;
  static CallbackFactory const callbackFactories[]{
    [](ReadContext& ctx) {
    // capture only one reference to potentially avoid heap allocation
    return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
      TRI_ASSERT(ctx._outputRow);
      TRI_ASSERT(ctx._inputRow);
      TRI_ASSERT(ctx._inputRow->isInitialized());
      TRI_ASSERT(ctx._infos);
      arangodb::aql::AqlValue a{ arangodb::aql::AqlValueHintCopy(doc.begin()) };
      bool mustDestroy = true;
      arangodb::aql::AqlValueGuard guard{ a, mustDestroy };
      ctx._outputRow->moveValueInto(ctx._infos->outputMaterializedDocumentRegId(), *ctx._inputRow, guard);
    };
  },

    [](ReadContext& ctx) {
    // capture only one reference to potentially avoid heap allocation
    return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
      TRI_ASSERT(ctx._outputRow);
      TRI_ASSERT(ctx._inputRow);
      TRI_ASSERT(ctx._inputRow->isInitialized());
      TRI_ASSERT(ctx._infos);
      arangodb::aql::AqlValue a{ arangodb::aql::AqlValueHintDocumentNoCopy(doc.begin()) };
      bool mustDestroy = true;
      arangodb::aql::AqlValueGuard guard{ a, mustDestroy };
      ctx._outputRow->moveValueInto(ctx._infos->outputMaterializedDocumentRegId(), *ctx._inputRow, guard);
    };
  } };

  return callbackFactories[size_t(engine->useRawDocumentPointers())](ctx);
}

arangodb::aql::MaterializerExecutorInfos::MaterializerExecutorInfos(
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToClear,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToKeep,
    RegisterId inNmColPtr, RegisterId inNmDocId, RegisterId outDocRegId, transaction::Methods* trx )
  : ExecutorInfos(
      make_shared_unordered_set(std::initializer_list<RegisterId>({inNmColPtr, inNmDocId})),
      make_shared_unordered_set(std::initializer_list<RegisterId>({outDocRegId})),
      nrInputRegisters, nrOutputRegisters,
      std::move(registersToClear), std::move(registersToKeep)),
      _inNonMaterializedColRegId(inNmColPtr), _inNonMaterializedDocRegId(inNmDocId),
      _outMaterializedDocumentRegId(outDocRegId), _trx(trx) {
}

std::pair<ExecutionState, NoStats> arangodb::aql::MaterializeExecutor::produceRows(OutputAqlItemRow & output) {
  InputAqlItemRow input{CreateInvalidInputRowHint{}};
  ExecutionState state;
  bool written = false;
  // some micro-optimization
  auto& callback = _readDocumentContext._callback;
  auto docRegId = _readDocumentContext._infos->inputNonMaterializedDocRegId();
  auto colRegId = _readDocumentContext._infos->inputNonMaterializedColRegId();
  auto* trx = _readDocumentContext._infos->trx();
  do {
    std::tie(state, input) = _fetcher.fetchRow();
    if (state == ExecutionState::WAITING) {
      return { state, NoStats{} };
    }

    if (!input) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, NoStats{}};
    }
    auto collection =
      reinterpret_cast<arangodb::LogicalCollection const*>(
        input.getValue(colRegId).slice().getUInt());
    TRI_ASSERT(collection != nullptr);
    _readDocumentContext._inputRow = &input;
    _readDocumentContext._outputRow = &output;
    written = collection->readDocumentWithCallback(trx,
      LocalDocumentId(input.getValue(docRegId).slice().getUInt()),
      callback);
  } while (!written && state != ExecutionState::DONE);
  return {state, NoStats{}};
}

std::tuple<ExecutionState, NoStats, size_t> arangodb::aql::MaterializeExecutor::skipRows(size_t toSkipRequested) {
  ExecutionState state;
  size_t skipped;
  std::tie(state, skipped) = _fetcher.skipRows(toSkipRequested);
  return std::make_tuple(state, NoStats{}, skipped);
}
