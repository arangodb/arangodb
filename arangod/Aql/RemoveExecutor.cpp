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

#include "RemoveExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "ModificationExecutorTraits.h"
#include "VocBase/LogicalCollection.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

size_t accumulate(transaction::Methods* trx, AqlValue const& inVal,
                  VPackBuilder& accumulator, bool includeRev) {
  std::string key, rev;

  if (inVal.isObject()) {
    bool mustDestroy;
    auto resolver = trx->resolver();
    TRI_ASSERT(resolver != nullptr);
    AqlValue sub = inVal.get(*resolver, StaticStrings::KeyString, mustDestroy, false);
    AqlValueGuard guard(sub, mustDestroy);

    if (sub.isString()) {
      key.assign(sub.slice().copyString());

      if (includeRev) {
        bool mustDestroyToo;
        AqlValue subTwo =
            inVal.get(*resolver, StaticStrings::RevString, mustDestroyToo, false);
        AqlValueGuard guard(subTwo, mustDestroyToo);
        if (subTwo.isString()) {
          rev.assign(subTwo.slice().copyString());
        } else {
          // TODO: Error handling?
        }
      }
    }
  } else if (inVal.isString()) {
    key.assign(inVal.slice().copyString());
  } else {
    // TODO: Error
  }

  accumulator.openObject();
  accumulator.add(StaticStrings::KeyString, VPackValue(key));
  if (includeRev && !rev.empty()) {
    accumulator.add(StaticStrings::RevString, VPackValue(rev));
  }

  accumulator.close();
  return 0;
}

}  // namespace

template <typename FetcherType>
RemoveExecutor<FetcherType>::RemoveExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher) {
  // important for mmfiles
  this->_infos._trx->pinData(this->_infos._aqlCollection->id());
}

template <typename FetcherType>
RemoveExecutor<FetcherType>::~RemoveExecutor() = default;

template <typename FetcherType>
std::pair<ExecutionState, typename RemoveExecutor<FetcherType>::Stats>
RemoveExecutor<FetcherType>::produceRows(OutputAqlItemRow& output) {
  TRI_ASSERT(_infos._trx);

  bool justCopy{false};

  ExecutionState state = ExecutionState::HASMORE;
  RemoveExecutor::Stats stats;

  std::vector<std::pair<ModOperationType, InputAqlItemRow>> outputs;

  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  VPackBuilder accumulator;

  RegisterId const inReg = _infos._input1RegisterId;

  accumulator.openArray();
  // Maximum number of rows we can put into output
  // So we only ever produce this many here
  size_t maxOutputs = std::min(output.numRowsLeft(), ExecutionBlock::DefaultBatchSize());
  while (outputs.size() < maxOutputs && state != ExecutionState::DONE) {
    std::tie(state, row) = _fetcher.fetchRow();
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, stats};
    }
    // Make sure we have a valid row
    TRI_ASSERT(row.isInitialized());

    auto const& inVal = row.getValue(inReg);

    // Make sure we don't close the array
    TRI_ASSERT(accumulator.isOpenArray());

    if (!_infos._consultAqlWriteFilter ||
        !_infos._aqlCollection->getCollection()->skipForAqlWrite(inVal.slice(),
                                                                 StaticStrings::Empty)) {
      size_t error =
          accumulate(_infos._trx, inVal, accumulator, !_infos._options.ignoreRevs);
      // make sure accumulate didn't mess up accumulator too much
      TRI_ASSERT(accumulator.isOpenArray());
      if (error) {
        outputs.emplace_back(ModOperationType::IGNORE_SKIP, row);
      } else {
        outputs.emplace_back(ModOperationType::APPLY_RETURN, row);
      }
    } else {
      // Not relevant for us
      outputs.emplace_back(ModOperationType::IGNORE_RETURN, row);
    }
  }
  accumulator.close();
  auto toRemove = accumulator.slice();
  if (toRemove.length() == 0) {
    justCopy = true;
  }

  // Actually Insert
  auto operationResult =
      _infos._trx->remove(_infos._aqlCollection->name(), toRemove, _infos._options);

  // operation result contains results for all things committed
  if (operationResult.fail()) {
    THROW_ARANGO_EXCEPTION(operationResult.result);
  }

  // TODO: something about stats.

  ModOperationType modOp;

  // We had better gotten as many results as we tried inserting
  // rows.
  TRI_ASSERT(outputs.size() == operationResult.slice().length());

  if (justCopy || _infos._options.silent) {
    for (auto o : outputs) {
      std::tie(std::ignore, row) = o;
      output.copyRow(row);
    }
  } else {
    // TODO handle weird return cases for operationsResult m(
    auto r = VPackArrayIterator(operationResult.slice());
    for (auto o : outputs) {
      std::tie(modOp, row) = o;

      auto elm = r.value();
      bool error =
          arangodb::basics::VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);
      if (!error) {
        switch (modOp) {
          case ModOperationType::APPLY_RETURN: {
            if (_infos._options.returnNew) {
              // TODO: Static strings
              AqlValue value(elm.get("new"));
              AqlValueGuard guard(value, true);
              output.moveValueInto(_infos._outputNewRegisterId, row, guard);
            }
            if (_infos._options.returnOld) {
              // TODO: Static strings
              auto slice = elm.get("old");
              if (slice.isNone()) {
                slice = VPackSlice::nullSlice();
              }
              AqlValue value(slice);
              AqlValueGuard guard(value, true);
              output.moveValueInto(_infos._outputOldRegisterId, row, guard);
            }
            break;
          }
          case ModOperationType::IGNORE_RETURN: {
            output.copyRow(row);
            break;
          }
          case ModOperationType::IGNORE_SKIP: {
            // TODO? Just skipped?
            break;
          }
          default: {
            TRI_ASSERT(false);
            break;
          }
        }
      }
    }
    output.advanceRow();
    r++;
  }
  // TODO:
  // We want to say "has more" if upstream still has rows.
  // sooo... just return the upstream state?
  return {ExecutionState::DONE, std::move(stats)};
}

template class ::arangodb::aql::RemoveExecutor<SingleRowFetcher<false>>;
// TODO: activate
// template class ::arangodb::aql::InsertExecutor<AllRowsFetcher>;
