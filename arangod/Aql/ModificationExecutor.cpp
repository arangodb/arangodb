////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "ModificationExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/QueryContext.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/application-exit.h"
#include "StorageEngine/TransactionState.h"

#include "Aql/SimpleModifier.h"
#include "Aql/UpsertModifier.h"

#include <velocypack/velocypack-aliases.h>
#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace arangodb::aql {

namespace {
auto translateReturnType(ExecutorState state) noexcept -> ExecutionState {
  if (state == ExecutorState::DONE) {
    return ExecutionState::DONE;
  }
  return ExecutionState::HASMORE;
}
}  // namespace

ModifierOutput::ModifierOutput(InputAqlItemRow const& inputRow, Type type)
    : _inputRow(inputRow), _type(type), _oldValue(), _newValue() {}
ModifierOutput::ModifierOutput(InputAqlItemRow&& inputRow, Type type)
    : _inputRow(std::move(inputRow)), _type(type), _oldValue(), _newValue() {}

ModifierOutput::ModifierOutput(InputAqlItemRow const& inputRow, Type type,
                               AqlValue const& oldValue, AqlValue const& newValue)
    : _inputRow(inputRow),
      _type(type),
      _oldValue(oldValue),
      _oldValueGuard(std::in_place, _oldValue.value(), true),
      _newValue(newValue),
      _newValueGuard(std::in_place, _newValue.value(), true) {}

ModifierOutput::ModifierOutput(InputAqlItemRow&& inputRow, Type type,
                               AqlValue const& oldValue, AqlValue const& newValue)
    : _inputRow(std::move(inputRow)),
      _type(type),
      _oldValue(oldValue),
      _oldValueGuard(std::in_place, _oldValue.value(), true),
      _newValue(newValue),
      _newValueGuard(std::in_place, _newValue.value(), true) {}

InputAqlItemRow ModifierOutput::getInputRow() const { return _inputRow; }
ModifierOutput::Type ModifierOutput::getType() const { return _type; }
bool ModifierOutput::hasOldValue() const { return _oldValue.has_value(); }
AqlValue const& ModifierOutput::getOldValue() const {
  TRI_ASSERT(_oldValue.has_value());
  return _oldValue.value();
}
bool ModifierOutput::hasNewValue() const { return _newValue.has_value(); }
AqlValue const& ModifierOutput::getNewValue() const {
  TRI_ASSERT(_newValue.has_value());
  return _newValue.value();
}

template <typename FetcherType, typename ModifierType>
ModificationExecutor<FetcherType, ModifierType>::ModificationExecutor(Fetcher& fetcher,
                                                                      Infos& infos)
    : _trx(infos._query.newTrxContext()),
      _infos(infos),
      _modifier(std::make_shared<ModifierType>(infos)) {}

template <typename FetcherType, typename ModifierType>
template <typename ProduceOrSkipData>
[[nodiscard]] auto ModificationExecutor<FetcherType, ModifierType>::produceOrSkip(
    typename FetcherType::DataRange& input, ProduceOrSkipData& produceOrSkipData)
    -> std::tuple<ExecutionState, Stats, AqlCall> {
  TRI_IF_FAILURE("ModificationBlock::getSome") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  AqlCall upstreamCall{};
  if constexpr (std::is_same_v<ModifierType, UpsertModifier>) {
    upstreamCall.softLimit = _modifier->getBatchSize();
  }

  // Try to initialize the RangeHandler
  if (!_rangeHandler.init(input)) {
    return {translateReturnType(_rangeHandler.upstreamState(input)),
            ModificationStats{}, upstreamCall};
  }

  auto stats = ModificationStats{};

  auto const maxRows = std::invoke([&] {
    if constexpr (std::is_same_v<ModifierType, UpsertModifier>) {
      return std::min(produceOrSkipData.maxOutputRows(), _modifier->getBatchSize());
    } else {
      return produceOrSkipData.maxOutputRows();
    }
  });

  // Read as much input as possible
  while (_rangeHandler.hasDataRow(input) && _modifier->nrOfOperations() < maxRows) {
    auto row = _rangeHandler.nextDataRow(input);

    TRI_ASSERT(row.isInitialized());
    _modifier->accumulate(row);
  }

  bool const inputDone = _rangeHandler.upstreamState(input) == ExecutorState::DONE;
  bool const enoughOutputAvailable = maxRows == _modifier->nrOfOperations();
  bool const hasAtLeastOneModification = _modifier->nrOfOperations() > 0;

  // outputFull => enoughOutputAvailable
  TRI_ASSERT(produceOrSkipData.needMoreOutput() || enoughOutputAvailable);

  if (!inputDone && !enoughOutputAvailable) {
    // This case handles when there's still work to do, but no input in the
    // current range.
    TRI_ASSERT(!_rangeHandler.hasDataRow(input));

    return {ExecutionState::HASMORE, stats, upstreamCall};
  }

  if (hasAtLeastOneModification) {
    TRI_ASSERT(inputDone || enoughOutputAvailable);
    ExecutionState modifierState = _modifier->transact(_trx);

    if (modifierState == ExecutionState::WAITING) {
      TRI_ASSERT(!ServerState::instance()->isSingleServer());
      return {ExecutionState::WAITING, stats, upstreamCall};
    }

    TRI_ASSERT(_modifier->hasResultOrException());

    _modifier->checkException();
    if (_infos._doCount) {
      stats.addWritesExecuted(_modifier->nrOfWritesExecuted());
      stats.addWritesIgnored(_modifier->nrOfWritesIgnored());
    }

    produceOrSkipData.doOutput();
    _modifier->reset();
  }

  TRI_ASSERT(_modifier->hasNeitherResultNorOperationPending());

  return {translateReturnType(_rangeHandler.upstreamState(input)), stats, upstreamCall};
}

template <typename FetcherType, typename ModifierType>
[[nodiscard]] auto ModificationExecutor<FetcherType, ModifierType>::produceRows(
    typename FetcherType::DataRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutionState, Stats, AqlCall> {
  struct ProduceData final : public IProduceOrSkipData {
    OutputAqlItemRow& _output;
    ModificationExecutorInfos& _infos;
    ModifierType& _modifier;
    bool _first = true;

    ~ProduceData() final = default;
    ProduceData(OutputAqlItemRow& output, ModificationExecutorInfos& infos, ModifierType& modifier)
        : _output(output), _infos(infos), _modifier(modifier) {}

    void doOutput() final {
      typename ModifierType::OutputIterator modifierOutputIterator(_modifier);
      // We only accumulated as many items as we can output, so this
      // should be correct
      for (auto const& modifierOutput : modifierOutputIterator) {
        TRI_ASSERT(!_output.isFull());
        bool written = false;
        switch (modifierOutput.getType()) {
          case ModifierOutput::Type::ReturnIfRequired:
            if (_infos._options.returnOld) {
              _output.cloneValueInto(_infos._outputOldRegisterId,
                                     modifierOutput.getInputRow(),
                                     modifierOutput.getOldValue());
              written = true;
            }
            if (_infos._options.returnNew) {
              _output.cloneValueInto(_infos._outputNewRegisterId,
                                     modifierOutput.getInputRow(),
                                     modifierOutput.getNewValue());
              written = true;
            }
            [[fallthrough]];
          case ModifierOutput::Type::CopyRow:
            if (!written) {
              _output.copyRow(modifierOutput.getInputRow());
            }
            _output.advanceRow();
            break;
          case ModifierOutput::Type::SkipRow:
            // nothing.
            break;
        }
      }
    }
    std::size_t maxOutputRows() override { return _output.numRowsLeft(); }
    bool needMoreOutput() override { return !_output.isFull(); }
  };

  auto produceData = ProduceData(output, _infos, *_modifier);
  auto&& [state, localStats, upstreamCall] = produceOrSkip(input, produceData);

  _stats += localStats;

  if (state == ExecutionState::WAITING) {
    return {state, ModificationStats{}, upstreamCall};
  }

  auto stats = ModificationStats{};
  std::swap(stats, _stats);

  return {state, stats, upstreamCall};
}

template <typename FetcherType, typename ModifierType>
[[nodiscard]] auto ModificationExecutor<FetcherType, ModifierType>::skipRowsRange(
    typename FetcherType::DataRange& input, AqlCall& call)
    -> std::tuple<ExecutionState, Stats, size_t, AqlCall> {
  struct SkipData final : public IProduceOrSkipData {
    AqlCall& _call;
    ModifierType& _modifier;

    SkipData(AqlCall& call, ModifierType& modifier)
        : _call(call), _modifier(modifier) {}
    ~SkipData() final = default;

    void doOutput() final {
      auto const skipped = std::invoke([&] {
        if (_call.needsFullCount()) {
          // If we need to do full count the nr of writes we did
          // in this batch is always correct.
          // If we are in offset phase and need to produce data
          // after the toSkip is limited to offset().
          // otherwise we need to report everything we write
          return _modifier.nrOfWritesExecuted();
        } else {
          // If we do not need to report fullcount.
          // we cannot report more than offset
          // but also not more than the operations we
          // have successfully executed
          return (std::min)(_call.getOffset(), _modifier.nrOfWritesExecuted());
        }
      });
      _call.didSkip(skipped);
    }
    std::size_t maxOutputRows() override {
      if (_call.getLimit() == 0 && _call.hasHardLimit()) {
        // We need to produce all modification operations.
        // If we are bound by limits or not!
        return ExecutionBlock::SkipAllSize();
      } else {
        return _call.getOffset();
      }
    }
    bool needMoreOutput() override { return _call.needSkipMore(); }
  };

  TRI_ASSERT(call.getSkipCount() == 0);
  // "move" the previously saved skip count into the call
  call.didSkip(_skipCount);
  _skipCount = 0;

  auto skipData = SkipData(call, *_modifier);
  auto&& [state, localStats, upstreamCall] = produceOrSkip(input, skipData);

  _stats += localStats;

  if (state == ExecutionState::WAITING) {
    // save the skip count until the next call
    _skipCount = call.getSkipCount();
    return {state, ModificationStats{}, 0, upstreamCall};
  }

  auto stats = ModificationStats{};
  std::swap(stats, _stats);

  return {state, stats, call.getSkipCount(), upstreamCall};
}

template <typename FetcherType, typename ModifierType>
auto ModificationExecutor<FetcherType, ModifierType>::RangeHandler::nextDataRow(
    typename FetcherType::DataRange& input) -> arangodb::aql::InputAqlItemRow {
  if constexpr (inputIsMatrix) {
    // Assume init() was called at least once
    TRI_ASSERT(_iterator.isInitialized());
    // Assume hasDataRow() is true (caller has to make sure)
    TRI_ASSERT(hasDataRow(input));

    auto ret = _iterator.next();

    if (!_iterator) {
      // We are done with this input.
      // We need to forward it to the last ShadowRow.
      // RangeHandler::hasDataRow() will continue to return false, even if
      // the AqlItemBlockInputMatrix is now already in the next range.
      input.skipAllRemainingDataRows();
    }

    return ret;
  } else {
    return input.nextDataRow(AqlItemBlockInputRange::HasDataRow{}).second;
  }
}

template <typename FetcherType, typename ModifierType>
auto ModificationExecutor<FetcherType, ModifierType>::RangeHandler::hasDataRow(
    typename FetcherType::DataRange& input) const noexcept -> bool {
  if constexpr (inputIsMatrix) {
    // Assume init() was called at least once
    TRI_ASSERT(_iterator.isInitialized());

    return _iterator.hasMore();
  } else {
    return input.hasDataRow();
  }
}

template <typename FetcherType, typename ModifierType>
bool ModificationExecutor<FetcherType, ModifierType>::RangeHandler::init(
    typename FetcherType::DataRange& input) {
  if constexpr (inputIsMatrix) {
    if (!_iterator.isInitialized()) {
      if (input.hasValidRow()) {
        auto&& [state, matrix] = input.getMatrix();
        TRI_ASSERT(state == ExecutorState::DONE);
        _iterator = matrix->begin();
        if (!_iterator) {
          input.skipAllRemainingDataRows();
        }
        // initialized successfully
        return true;
      } else {
        // can't initialize yet, no matrix
        return false;
      }
    } else {
      // already initialized
      return true;
    }
  } else {
    // no-op
    return true;
  }
}

template <typename FetcherType, typename ModifierType>
auto ModificationExecutor<FetcherType, ModifierType>::RangeHandler::upstreamState(
    typename FetcherType::DataRange& input) const noexcept -> ExecutorState {
  if constexpr (inputIsMatrix) {
    if (ADB_UNLIKELY(!_iterator.isInitialized())) {
      // As long as the iterator isn't initialized, we need to pass the upstream
      // state. In particular if the input is completely empty, we need to
      // return DONE.
      return input.upstreamState();
    } else if (hasDataRow(input)) {
      return ExecutorState::HASMORE;
    } else {
      return ExecutorState::DONE;
    }
  } else {
    return input.upstreamState();
  }
}

using NoPassthroughSingleRowFetcher = SingleRowFetcher<BlockPassthrough::Disable>;

template class ::arangodb::aql::ModificationExecutor<NoPassthroughSingleRowFetcher, InsertModifier>;
template class ::arangodb::aql::ModificationExecutor<NoPassthroughSingleRowFetcher, RemoveModifier>;
template class ::arangodb::aql::ModificationExecutor<NoPassthroughSingleRowFetcher, UpdateReplaceModifier>;
template class ::arangodb::aql::ModificationExecutor<NoPassthroughSingleRowFetcher, UpsertModifier>;

}  // namespace arangodb::aql
