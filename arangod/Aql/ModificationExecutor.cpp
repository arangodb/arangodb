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

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/QueryContext.h"
#include "Aql/SingleRowFetcher.h"
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
  if constexpr (std::is_same_v<ModifierType, UpsertModifier> &&
                !std::is_same_v<FetcherType, AllRowsFetcher>) {
    upstreamCall.softLimit = _modifier->getBatchSize();
  }

  auto const hasInputOrPreviousResult = [&] {
    return input.hasDataRow() || _modifier->operationPending();
  };

  // Make AqlItemBlockInputMatrix happy, which dislikes being asked for its
  // input range when empty.
  if (!hasInputOrPreviousResult()) {
    // Input is empty
    return {translateReturnType(input.upstreamState()), ModificationStats{}, upstreamCall};
  }

  // MSVC doesn't see constexpr variables inside a lambda as constexpr, thus
  // we can't just assign is_same_t to a variable.
  using inputIsMatrix =
      std::is_same<typename FetcherType::DataRange, AqlItemBlockInputMatrix>;

  auto upstreamState = input.upstreamState();
  auto stats = ModificationStats{};

  // TODO Change the following logic as follows.
  // First, read input and fill the modifier as much as possible. Only if either
  // the output is satisfied (offset/output block aka maxOutputRows()), or the
  // input is completely consumed (i.e. upstream DONE, not just the current
  // range empty).
  // Only after that, call transact(), i.e. execute the
  // `if (_modifier->nrOfOperations() > 0) {` - block.

  while (hasInputOrPreviousResult() && produceOrSkipData.needMoreOutput()) {
    auto& range = std::invoke([&]() -> AqlItemBlockInputRange& {
      if constexpr (inputIsMatrix::value) {
        return input.getInputRange();
      } else {
        return input;
      }
    });
    auto const maxRows = produceOrSkipData.maxOutputRows();
    while (_modifier->nrOfOperations() < maxRows && range.hasDataRow()) {
      auto [state, row] = range.nextDataRow(AqlItemBlockInputRange::HasDataRow{});

      TRI_ASSERT(row.isInitialized());
      _modifier->accumulate(row);
    }

    upstreamState = range.upstreamState();

    if (_modifier->nrOfOperations() > 0) {
      ExecutionState modifierState = _modifier->transact(_trx);

      if (modifierState == ExecutionState::WAITING) {
        TRI_ASSERT(!ServerState::instance()->isSingleServer());
        return {ExecutionState::WAITING, stats, upstreamCall};
      }

      TRI_ASSERT(_modifier->resultState() == ModificationExecutorResultState::HaveResult);

      _modifier->checkException();
      if (_infos._doCount) {
        stats.addWritesExecuted(_modifier->nrOfWritesExecuted());
        stats.addWritesIgnored(_modifier->nrOfWritesIgnored());
      }

      produceOrSkipData.doOutput();
      _modifier->reset();
    }
  }

  if constexpr (inputIsMatrix::value) {
    if (upstreamState == ExecutorState::DONE) {
      // We are done with this input.
      // We need to forward it to the last ShadowRow.
      input.skipAllRemainingDataRows();
    }
  }

  return {translateReturnType(upstreamState), stats, upstreamCall};
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
    auto maxOutputRows() -> std::size_t final { return _output.numRowsLeft(); }
    auto needMoreOutput() -> bool final { return !_output.isFull(); }
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
    auto maxOutputRows() -> std::size_t final {
      if (_call.getLimit() == 0 && _call.hasHardLimit()) {
        // We need to produce all modification operations.
        // If we are bound by limits or not!
        return ExecutionBlock::SkipAllSize();
      } else {
        return _call.getOffset();
      }
    }
    auto needMoreOutput() -> bool final { return _call.needSkipMore(); }
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

using NoPassthroughSingleRowFetcher = SingleRowFetcher<BlockPassthrough::Disable>;

template class ::arangodb::aql::ModificationExecutor<NoPassthroughSingleRowFetcher, InsertModifier>;
template class ::arangodb::aql::ModificationExecutor<AllRowsFetcher, InsertModifier>;
template class ::arangodb::aql::ModificationExecutor<NoPassthroughSingleRowFetcher, RemoveModifier>;
template class ::arangodb::aql::ModificationExecutor<AllRowsFetcher, RemoveModifier>;
template class ::arangodb::aql::ModificationExecutor<NoPassthroughSingleRowFetcher, UpdateReplaceModifier>;
template class ::arangodb::aql::ModificationExecutor<AllRowsFetcher, UpdateReplaceModifier>;
template class ::arangodb::aql::ModificationExecutor<NoPassthroughSingleRowFetcher, UpsertModifier>;
template class ::arangodb::aql::ModificationExecutor<AllRowsFetcher, UpsertModifier>;

}  // namespace arangodb::aql
