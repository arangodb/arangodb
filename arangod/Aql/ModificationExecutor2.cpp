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

#include "ModificationExecutor2.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "VocBase/LogicalCollection.h"

#include "Aql/InsertModifier.h"
#include "Aql/RemoveModifier.h"
#include "Aql/ReplaceModifier.h"
#include "Aql/UpdateModifier.h"
#include "Aql/UpsertModifier.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

class NoStats;

// Extracts key, and rev in from input AqlValue value.
//
// value can either be an object or a string.
//
// * if value is an object, we extract the entry _key into key if it is a string,
//   or signal an error otherwise.
//   if ignoreRev is false, we also extract the entry _rev and assign its contents
//   to rev if it is a string, or signal an error otherise.
// * if value is a string, this string is assigned to key, and rev is emptied.
// * if value is anything else, we return an error.
Result ModificationExecutorHelpers::getKeyAndRevision(CollectionNameResolver const& resolver,
                                                      AqlValue const& value,
                                                      std::string& key, std::string& rev,
                                                      bool ignoreRevision) {
  TRI_ASSERT(key.empty());
  TRI_ASSERT(rev.empty());
  if (value.isObject()) {
    // // TODO:
    // auto resolver = _infos._trx->resolver();
    // TRI_ASSERT(resolver != nullptr);

    bool mustDestroy;
    AqlValue sub = value.get(resolver, StaticStrings::KeyString, mustDestroy, false);
    AqlValueGuard guard(sub, mustDestroy);

    if (sub.isString()) {
      key.assign(sub.slice().copyString());

      if (!ignoreRevision) {
        bool mustDestroyToo;
        AqlValue subTwo =
            value.get(resolver, StaticStrings::RevString, mustDestroyToo, false);
        AqlValueGuard guard(subTwo, mustDestroyToo);
        if (subTwo.isString()) {
          rev.assign(subTwo.slice().copyString());
        } else {
          return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                        std::string{"Expected _rev as string, but got "} +
                            value.slice().typeName());
        }
      }
    } else {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                    std::string{"Expected _key as string, but got "} +
                        value.slice().typeName());
    }
  } else if (value.isString()) {
    key.assign(value.slice().copyString());
    rev.clear();
  } else {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING,
                  std::string{"Expected object or string, but got "} +
                      value.slice().typeName());
  }
  return Result{};
}

// Builds an object "{ _key: key, _rev: rev }" if rev is nonempty and ignoreRevs is false, and
// "{ _key: key, _rev: null }" otherwise.
Result ModificationExecutorHelpers::buildKeyDocument(VPackBuilder& builder,
                                                     std::string const& key,
                                                     std::string const& rev,
                                                     bool ignoreRevision) {
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(key));
  if (ignoreRevision && !rev.empty()) {
    builder.add(StaticStrings::RevString, VPackValue(rev));
  } else {
    builder.add(StaticStrings::RevString, VPackValue(VPackValueType::Null));
  }
  builder.close();
  return Result{};
}

template <typename FetcherType, typename ModifierType>
ModificationExecutor2<FetcherType, ModifierType>::ModificationExecutor2(Fetcher& fetcher,
                                                                        Infos& infos)
    : _infos(infos), _fetcher(fetcher), _modifier(infos) {
  // important for mmfiles
  // TODO: Why is this important for mmfiles?
  _infos._trx->pinData(this->_infos._aqlCollection->id());
}

template <typename FetcherType, typename ModifierType>
ModificationExecutor2<FetcherType, ModifierType>::~ModificationExecutor2() = default;

// Fetches as many rows as possible from upstream using the fetcher's fetchRow
// method and accumulates results through the modifier
template <typename FetcherType, typename ModifierType>
std::pair<ExecutionState, typename ModificationExecutor2<FetcherType, ModifierType>::Stats>
ModificationExecutor2<FetcherType, ModifierType>::doCollect(size_t const maxOutputs) {
  // for fetchRow
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  ExecutionState state = ExecutionState::HASMORE;

  // Maximum number of rows we can put into output
  // So we only ever produce this many here
  while (_modifier.size() < maxOutputs && state != ExecutionState::DONE) {
    std::tie(state, row) = _fetcher.fetchRow();
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, ModificationStats{}};
    }
    // Make sure we have a valid row
    TRI_ASSERT(row.isInitialized());

    _modifier.accumulate(row);
  }
  _modifier.close();
  return {state, ModificationStats{}};
}

// Outputs accumulated results
template <typename FetcherType, typename ModifierType>
void ModificationExecutor2<FetcherType, ModifierType>::doOutput(OutputAqlItemRow& output) {
  // If we have made no modifications or are silent,
  // we can just copy rows; this is an optimisation for silent
  // queries
  if (_modifier.size() == 0 || _infos._options.silent) {
    _modifier.setupIterator();
    while (!_modifier.isFinishedIterator()) {
      InputAqlItemRow row{CreateInvalidInputRowHint{}};
      std::tie(std::ignore, row, std::ignore) = _modifier.getOutput();
      output.copyRow(row);
      _modifier.advanceIterator();
    }
  } else {
    ModOperationType modOp;
    InputAqlItemRow row{CreateInvalidInputRowHint{}};
    VPackSlice elm;

    // TODO: Make this a proper iterator
    _modifier.setupIterator();
    while (!_modifier.isFinishedIterator()) {
      std::tie(modOp, row, elm) = _modifier.getOutput();

      bool error =
          arangodb::basics::VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);
      if (!error) {
        switch (modOp) {
          case ModOperationType::APPLY_RETURN: {
            if (_infos._options.returnNew) {
              AqlValue value(elm.get(StaticStrings::New));
              AqlValueGuard guard(value, true);
              output.moveValueInto(_infos._outputNewRegisterId, row, guard);
            }
            if (_infos._options.returnOld) {
              auto slice = elm.get(StaticStrings::Old);
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
            output.copyRow(row);
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
      }
    }
    output.advanceRow();
  }
}

template <typename FetcherType, typename ModifierType>
std::pair<ExecutionState, typename ModificationExecutor2<FetcherType, ModifierType>::Stats>
ModificationExecutor2<FetcherType, ModifierType>::produceRows(OutputAqlItemRow& output) {
  TRI_ASSERT(_infos._trx);

  // TODO: isDBServer case
  ExecutionState state;
  ModificationExecutor2::Stats stats;

  _modifier.reset();

  size_t maxOutputs = std::min(output.numRowsLeft(), ExecutionBlock::DefaultBatchSize());
  std::tie(state, stats) = doCollect(maxOutputs);
  if (state == ExecutionState::WAITING) {
    return {ExecutionState::WAITING, std::move(stats)};
  }

  auto operationResult = _modifier.transact();
  // We have no way of handling anything other than .ok() here,
  // and the modifier should have thrown an exception
  // if something went wrong.
  TRI_ASSERT(operationResult.ok());

  // TODO: something about stats.
  doOutput(output);

  // TODO:
  // We want to say "has more" if upstream still has rows.
  // sooo... just return the upstream state?
  return {ExecutionState::DONE, std::move(stats)};
}

template class ::arangodb::aql::ModificationExecutor2<SingleRowFetcher<false>, InsertModifier>;
template class ::arangodb::aql::ModificationExecutor2<SingleRowFetcher<false>, RemoveModifier>;
template class ::arangodb::aql::ModificationExecutor2<SingleRowFetcher<false>, ReplaceModifier>;
template class ::arangodb::aql::ModificationExecutor2<SingleRowFetcher<false>, UpdateModifier>;
template class ::arangodb::aql::ModificationExecutor2<SingleRowFetcher<false>, UpsertModifier>;
// template class ::arangodb::aql::ModificationExecutor2<AllRowsFetcher, UpdateModifier>;
