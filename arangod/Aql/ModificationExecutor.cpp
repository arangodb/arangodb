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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "ModificationExecutor.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"
#include "Basics/VelocyPackHelper.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
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
    : _inputRow(std::move(inputRow)), _type(type), _oldValue(oldValue), _newValue(newValue) {}
ModifierOutput::ModifierOutput(InputAqlItemRow&& inputRow, Type type,
                               AqlValue const& oldValue, AqlValue const& newValue)
    : _inputRow(std::move(inputRow)), _type(type), _oldValue(oldValue), _newValue(newValue) {}

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
    : _lastState(ExecutionState::HASMORE), _infos(infos), _fetcher(fetcher), _modifier(infos) {
  // In MMFiles we need to make sure that the data is not moved in memory or collected
  // for this collection as soon as we start writing to it.
  // This pin makes sure that no memory is moved pointers we get from a collection stay
  // correct until we release this pin
  _infos._trx->pinData(this->_infos._aqlCollection->id());
}

// Fetches as many rows as possible from upstream using the fetcher's fetchRow
// method and accumulates results through the modifier
template <typename FetcherType, typename ModifierType>
auto ModificationExecutor<FetcherType, ModifierType>::doCollect(AqlItemBlockInputRange& input,
                                                                size_t maxOutputs)
    -> void {
  // for fetchRow
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  ExecutionState state = ExecutionState::HASMORE;

  // Maximum number of rows we can put into output
  // So we only ever produce this many here
  while (_modifier.nrOfOperations() < maxOutputs && input.hasDataRow()) {
    auto [state, row] = input.nextDataRow();

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
std::pair<ExecutionState, typename ModificationExecutor<FetcherType, ModifierType>::Stats>
ModificationExecutor<FetcherType, ModifierType>::produceRows(OutputAqlItemRow& output) {
  TRI_ASSERT(false);

  return {ExecutionState::DONE, ModificationStats{}};
}

template <typename FetcherType, typename ModifierType>
[[nodiscard]] auto ModificationExecutor<FetcherType, ModifierType>::produceRows(
    AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, ModificationStats, AqlCall> {
  TRI_ASSERT(_infos._trx);

  auto stats = ModificationStats{};

  _modifier.reset();

  // only produce at most output.numRowsLeft() many results
  doCollect(input, output.numRowsLeft());

  if (_modifier.nrOfOperations() > 0) {
    _modifier.transact();

    if (_infos._doCount) {
      stats.addWritesExecuted(_modifier.nrOfWritesExecuted());
      stats.addWritesIgnored(_modifier.nrOfWritesIgnored());
    }

    doOutput(output, stats);
  }

  return {input.upstreamState(), stats, AqlCall{}};
}

template <typename FetcherType, typename ModifierType>
[[nodiscard]] auto ModificationExecutor<FetcherType, ModifierType>::skipRowsRange(
    AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  auto stats = ModificationStats{};
  _modifier.reset();

  doCollect(input, call.getOffset());

  if (_modifier.nrOfOperations() > 0) {
    _modifier.transact();

    if (_infos._doCount) {
      stats.addWritesExecuted(_modifier.nrOfWritesExecuted());
      stats.addWritesIgnored(_modifier.nrOfWritesIgnored());
    }

    call.didSkip(_modifier.nrOfOperations());
  }

  return {input.upstreamState(), stats, _modifier.nrOfOperations(), AqlCall{}};
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
