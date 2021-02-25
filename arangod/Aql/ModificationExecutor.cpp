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
#include "Basics/Common.h"
#include "Basics/VelocyPackHelper.h"
#include "StorageEngine/TransactionState.h"
#include "VocBase/LogicalCollection.h"

#include "Aql/InsertModifier.h"
#include "Aql/RemoveModifier.h"
#include "Aql/SimpleModifier.h"
#include "Aql/UpdateReplaceModifier.h"
#include "Aql/UpsertModifier.h"

#include "Logger/LogMacros.h"

#include <algorithm>
#include "velocypack/velocypack-aliases.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace arangodb {
namespace aql {

ModifierOutput::ModifierOutput(InputAqlItemRow const& inputRow, Type type)
    : _inputRow(std::move(inputRow)), _type(type), _oldValue(), _newValue() {}
ModifierOutput::ModifierOutput(InputAqlItemRow&& inputRow, Type type)
    : _inputRow(std::move(inputRow)), _type(type), _oldValue(), _newValue() {}

ModifierOutput::ModifierOutput(InputAqlItemRow const& inputRow, Type type,
                               AqlValue const& oldValue, AqlValue const& newValue)
    : _inputRow(std::move(inputRow)),
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
ModificationExecutor<FetcherType, ModifierType>::ModificationExecutor(Fetcher& fetcher, Infos& infos)
    : _trx(infos._query.newTrxContext()), _lastState(ExecutionState::HASMORE), _infos(infos), _modifier(infos) {}

// Fetches as many rows as possible from upstream and accumulates results
// through the modifier
template <typename FetcherType, typename ModifierType>
auto ModificationExecutor<FetcherType, ModifierType>::doCollect(AqlItemBlockInputRange& input,
                                                                size_t maxOutputs)
    -> void {
  ExecutionState state = ExecutionState::HASMORE;

  // Maximum number of rows we can put into output
  // So we only ever produce this many here
  while (_modifier.nrOfOperations() < maxOutputs && input.hasDataRow()) {
    auto [state, row] = input.nextDataRow(AqlItemBlockInputRange::HasDataRow{});

    // Make sure we have a valid row
    TRI_ASSERT(row.isInitialized());
    _modifier.accumulate(row);
  }
  TRI_ASSERT(state == ExecutionState::DONE || state == ExecutionState::HASMORE);
}

// Outputs accumulated results, and counts the statistics
template <typename FetcherType, typename ModifierType>
void ModificationExecutor<FetcherType, ModifierType>::doOutput(OutputAqlItemRow& output,
                                                               Stats& stats) {
  typename ModifierType::OutputIterator modifierOutputIterator(_modifier);
  // We only accumulated as many items as we can output, so this
  // should be correct
  for (auto const& modifierOutput : modifierOutputIterator) {
    TRI_ASSERT(!output.isFull());
    bool written = false;
    switch (modifierOutput.getType()) {
      case ModifierOutput::Type::ReturnIfRequired:
        if (_infos._options.returnOld) {
          output.cloneValueInto(_infos._outputOldRegisterId, modifierOutput.getInputRow(),
                                modifierOutput.getOldValue());
          written = true;
        }
        if (_infos._options.returnNew) {
          output.cloneValueInto(_infos._outputNewRegisterId, modifierOutput.getInputRow(),
                                modifierOutput.getNewValue());
          written = true;
        }
        [[fallthrough]];
      case ModifierOutput::Type::CopyRow:
        if (!written) {
          output.copyRow(modifierOutput.getInputRow());
        }
        output.advanceRow();
        break;
      case ModifierOutput::Type::SkipRow:
        // nothing.
        break;
    }
  }
}

template <typename FetcherType, typename ModifierType>
[[nodiscard]] auto ModificationExecutor<FetcherType, ModifierType>::produceRows(
    typename FetcherType::DataRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, ModificationStats, AqlCall> {
  AqlCall upstreamCall{};
  if constexpr (std::is_same_v<ModifierType, UpsertModifier> &&
                !std::is_same_v<FetcherType, AllRowsFetcher>) {
    upstreamCall.softLimit = _modifier.getBatchSize();
  }
  auto stats = ModificationStats{};

  _modifier.reset();
  if (!input.hasDataRow()) {
    // Input is empty
    return {input.upstreamState(), stats, upstreamCall};
  }

  TRI_IF_FAILURE("ModificationBlock::getSome") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // only produce at most output.numRowsLeft() many results
  ExecutorState upstreamState = ExecutorState::HASMORE;
  if constexpr (std::is_same_v<typename FetcherType::DataRange, AqlItemBlockInputMatrix>) {
    auto& range = input.getInputRange();
    doCollect(range, output.numRowsLeft());
    upstreamState = range.upstreamState();
    if (upstreamState == ExecutorState::DONE) {
      // We are done with this input.
      // We need to forward it to the last ShadowRow.
      input.skipAllRemainingDataRows();
    }
  } else {
    doCollect(input, output.numRowsLeft());
    upstreamState = input.upstreamState();
  }
  if (_modifier.nrOfOperations() > 0) {
    _modifier.transact(_trx);

    if (_infos._doCount) {
      stats.addWritesExecuted(_modifier.nrOfWritesExecuted());
      stats.addWritesIgnored(_modifier.nrOfWritesIgnored());
    }

    doOutput(output, stats);
  }

  return {upstreamState, stats, upstreamCall};
}

template <typename FetcherType, typename ModifierType>
[[nodiscard]] auto ModificationExecutor<FetcherType, ModifierType>::skipRowsRange(
    typename FetcherType::DataRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  AqlCall upstreamCall{};
  if constexpr (std::is_same_v<ModifierType, UpsertModifier> &&
                !std::is_same_v<FetcherType, AllRowsFetcher>) {
    upstreamCall.softLimit = _modifier.getBatchSize();
  }

  auto stats = ModificationStats{};

  TRI_IF_FAILURE("ModificationBlock::getSome") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // only produce at most output.numRowsLeft() many results
  ExecutorState upstreamState = input.upstreamState();
  while (input.hasDataRow() && call.needSkipMore()) {
    _modifier.reset();
    size_t toSkip = call.getOffset();
    if (call.getLimit() == 0 && call.hasHardLimit()) {
      // We need to produce all modification operations.
      // If we are bound by limits or not!
      toSkip = ExecutionBlock::SkipAllSize();
    }
    if constexpr (std::is_same_v<typename FetcherType::DataRange, AqlItemBlockInputMatrix>) {
      auto& range = input.getInputRange();
      if (range.hasDataRow()) {
        doCollect(range, toSkip);
      }
      upstreamState = range.upstreamState();
      if (upstreamState == ExecutorState::DONE) {
        // We are done with this input.
        // We need to forward it to the last ShadowRow.
        input.skipAllRemainingDataRows();
        TRI_ASSERT(input.upstreamState() == ExecutorState::DONE);
      }
    } else {
      doCollect(input, toSkip);
      upstreamState = input.upstreamState();
    }

    if (_modifier.nrOfOperations() > 0) {
      _modifier.transact(_trx);

      if (_infos._doCount) {
        stats.addWritesExecuted(_modifier.nrOfWritesExecuted());
        stats.addWritesIgnored(_modifier.nrOfWritesIgnored());
      }

      if (call.needsFullCount()) {
        // If we need to do full count the nr of writes we did
        // in this batch is always correct.
        // If we are in offset phase and need to produce data
        // after the toSkip is limited to offset().
        // otherwise we need to report everything we write
        call.didSkip(_modifier.nrOfWritesExecuted());
      } else {
        // If we do not need to report fullcount.
        // we cannot report more than offset
        // but also not more than the operations we
        // have successfully executed
        call.didSkip((std::min)(call.getOffset(), _modifier.nrOfWritesExecuted()));
      }
    }
  }

  return {upstreamState, stats, call.getSkipCount(), upstreamCall};
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

}  // namespace aql
}  // namespace arangodb
