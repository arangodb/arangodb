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

std::ostream& operator<<(std::ostream& ostream, ModifierIteratorMode mode) {
  switch (mode) {
    case ModifierIteratorMode::Full:
      ostream << "Full";
      break;
    case ModifierIteratorMode::OperationsOnly:
      ostream << "OperationsOnly";
      break;
  }
  return ostream;
}

template <typename FetcherType, typename ModifierType>
ModificationExecutor2<FetcherType, ModifierType>::ModificationExecutor2(Fetcher& fetcher,
                                                                        Infos& infos)
    : _lastState(ExecutionState::HASMORE), _infos(infos), _fetcher(fetcher), _modifier(infos) {
  // In MMFiles we need to make sure that the data is not moved in memory or collected
  // for this collection as soon as we start writing to it.
  // This pin makes sure that no memory is moved pointers we get from a collection stay
  // correct until we release this pin
  _infos._trx->pinData(this->_infos._aqlCollection->id());

  // TODO: explain this abomination
  auto* trx = _infos._trx;
  TRI_ASSERT(trx != nullptr);
  bool const isDBServer = trx->state()->isDBServer();
  _infos._producesResults = ProducesResults(
      _infos._producesResults || (isDBServer && _infos._ignoreDocumentNotFound));
}

template <typename FetcherType, typename ModifierType>
ModificationExecutor2<FetcherType, ModifierType>::~ModificationExecutor2() = default;

// Fetches as many rows as possible from upstream using the fetcher's fetchRow
// method and accumulates results through the modifier
// TODO: Perhaps we can remove stats here?
template <typename FetcherType, typename ModifierType>
std::pair<ExecutionState, typename ModificationExecutor2<FetcherType, ModifierType>::Stats>
ModificationExecutor2<FetcherType, ModifierType>::doCollect(size_t const maxOutputs) {
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
void ModificationExecutor2<FetcherType, ModifierType>::doOutput(OutputAqlItemRow& output,
                                                                Stats& stats) {
  // If we have made no modifications or are silent,
  // we can just copy rows; this is an optimisation for silent
  // queries
  if (_modifier.nrOfDocuments() == 0 || _infos._options.silent) {
    _modifier.setupIterator(ModifierIteratorMode::OperationsOnly);
    InputAqlItemRow row{CreateInvalidInputRowHint{}};
    while (!_modifier.isFinishedIterator()) {
      std::tie(std::ignore, row, std::ignore) = _modifier.getOutput();

      output.copyRow(row);

      if (_infos._doCount) {
        stats.incrWritesExecuted();
      }

      _modifier.advanceIterator();
      output.advanceRow();
    }
  } else {
    ModOperationType modOp;
    InputAqlItemRow row{CreateInvalidInputRowHint{}};
    VPackSlice elm;

    // TODO: Make this a proper iterator
    _modifier.setupIterator(ModifierIteratorMode::Full);
    while (!_modifier.isFinishedIterator()) {
      std::tie(modOp, row, elm) = _modifier.getOutput();

      bool error = VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);
      if (!error) {
        switch (modOp) {
          case ModOperationType::APPLY_RETURN: {
            if (_infos._options.returnNew) {
              AqlValue value(elm.get(StaticStrings::New));
              AqlValueGuard guard(value, true);
              output.moveValueInto(_infos._outputNewRegisterId, row, guard);
            }
            if (_infos._options.returnOld) {
              // TODO: prettier
              auto slice = elm.get(StaticStrings::Old);
              if (slice.isNone()) {
                slice = VPackSlice::nullSlice();
              }
              AqlValue value(slice);
              AqlValueGuard guard(value, true);
              output.moveValueInto(_infos._outputOldRegisterId, row, guard);
            }
            if (_infos._doCount) {
              stats.incrWritesExecuted();
            }
            // What if we neither return old or new? we can't advance row then...
            break;
          }
          case ModOperationType::IGNORE_RETURN: {
            output.copyRow(row);
            if (_infos._doCount) {
              stats.incrWritesIgnored();
            }
            break;
          }
          case ModOperationType::IGNORE_SKIP: {
            output.copyRow(row);
            if (_infos._doCount) {
              stats.incrWritesIgnored();
            }
            break;
          }
          case ModOperationType::APPLY_UPDATE:
          case ModOperationType::APPLY_INSERT: {
            // These values should not appear here anymore
            // As we handle them in the UPSERT modifier and translate them
            // into APPLY_RETURN
            TRI_ASSERT(false);
          }
          default: {
            TRI_ASSERT(false);
            break;
          }
        }
        // only advance row if we produced something
        output.advanceRow();
      }
      _modifier.advanceIterator();
    }
  }
}

template <typename FetcherType, typename ModifierType>
std::pair<ExecutionState, typename ModificationExecutor2<FetcherType, ModifierType>::Stats>
ModificationExecutor2<FetcherType, ModifierType>::produceRows(OutputAqlItemRow& output) {
  TRI_ASSERT(_infos._trx);

  ModificationExecutor2::Stats stats;

  const size_t maxOutputs = std::min(output.numRowsLeft(), _modifier.getBatchSize());

  // if we returned "WAITING" the last time we still have
  // documents in the accumulator that we have not submitted
  // yet
  if (_lastState != ExecutionState::WAITING) {
    _modifier.reset();
  }

  std::tie(_lastState, stats) = doCollect(maxOutputs);

  if (_lastState == ExecutionState::WAITING) {
    return {ExecutionState::WAITING, std::move(stats)};
  }

  TRI_ASSERT(_lastState == ExecutionState::DONE || _lastState == ExecutionState::HASMORE);

  _modifier.close();
  _modifier.transact();

  doOutput(output, stats);

  return {_lastState, std::move(stats)};
}

using NoPassthroughSingleRowFetcher = SingleRowFetcher<BlockPassthrough::Disable>;

template class ::arangodb::aql::ModificationExecutor2<NoPassthroughSingleRowFetcher, InsertModifier>;
template class ::arangodb::aql::ModificationExecutor2<AllRowsFetcher, InsertModifier>;
template class ::arangodb::aql::ModificationExecutor2<NoPassthroughSingleRowFetcher, RemoveModifier>;
template class ::arangodb::aql::ModificationExecutor2<AllRowsFetcher, RemoveModifier>;
template class ::arangodb::aql::ModificationExecutor2<NoPassthroughSingleRowFetcher, UpdateReplaceModifier>;
template class ::arangodb::aql::ModificationExecutor2<AllRowsFetcher, UpdateReplaceModifier>;
template class ::arangodb::aql::ModificationExecutor2<NoPassthroughSingleRowFetcher, UpsertModifier>;
template class ::arangodb::aql::ModificationExecutor2<AllRowsFetcher, UpsertModifier>;

}  // namespace aql
}  // namespace arangodb
