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
std::pair<ExecutionState, typename ModificationExecutor<FetcherType, ModifierType>::Stats>
ModificationExecutor<FetcherType, ModifierType>::doCollect(size_t maxOutputs) {
  // for fetchRow
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  ExecutionState state = ExecutionState::HASMORE;

  // Maximum number of rows we can put into output
  // So we only ever produce this many here
  // TODO: If we SKIP_IGNORE, then we'd be able to output more;
  //       this would require some counting to happen in the modifier
  while (_modifier.nrOfOperations() < maxOutputs && state != ExecutionState::DONE) {
    std::tie(state, row) = _fetcher.fetchRow(maxOutputs);
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, ModificationStats{}};
    }
    if (row.isInitialized()) {
      // Make sure we have a valid row
      TRI_ASSERT(row.isInitialized());
      _modifier.accumulate(row);
    }
  }
  TRI_ASSERT(state == ExecutionState::DONE || state == ExecutionState::HASMORE);
  return {state, ModificationStats{}};
}

// Outputs accumulated results, and counts the statistics
template <typename FetcherType, typename ModifierType>
void ModificationExecutor<FetcherType, ModifierType>::doOutput(OutputAqlItemRow& output,
                                                               Stats& stats) {
  typename ModifierType::OutputIterator modifierOutputIterator(_modifier);
  for (auto const& modifierOutput : modifierOutputIterator) {
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

  if (_infos._doCount) {
    stats.addWritesExecuted(_modifier.nrOfWritesExecuted());
    stats.addWritesIgnored(_modifier.nrOfWritesIgnored());
  }
}

template <typename FetcherType, typename ModifierType>
std::pair<ExecutionState, typename ModificationExecutor<FetcherType, ModifierType>::Stats>
ModificationExecutor<FetcherType, ModifierType>::produceRows(OutputAqlItemRow& output) {
  TRI_ASSERT(_infos._trx);

  ModificationExecutor::Stats stats;

  const size_t maxOutputs = std::min(output.numRowsLeft(), _modifier.getBatchSize());

  // if we returned "WAITING" the last time we still have
  // documents in the accumulator that we have not submitted
  // yet
  if (_lastState != ExecutionState::WAITING) {
    _modifier.reset();
  }

  TRI_IF_FAILURE("ModificationBlock::getSome") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  std::tie(_lastState, stats) = doCollect(maxOutputs);

  if (_lastState == ExecutionState::WAITING) {
    return {ExecutionState::WAITING, std::move(stats)};
  }

  TRI_ASSERT(_lastState == ExecutionState::DONE || _lastState == ExecutionState::HASMORE);

  _modifier.transact();

  // If the query is silent, there is no way to relate
  // the results slice contents and the submitted documents
  // If the query is *not* silent, we should get one result
  // for every document.
  // Yes. Really.
  TRI_ASSERT(_infos._options.silent || _modifier.nrOfDocuments() == _modifier.nrOfResults());

  doOutput(output, stats);

  return {_lastState, std::move(stats)};
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
