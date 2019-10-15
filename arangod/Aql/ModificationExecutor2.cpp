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
                                                      std::string& key,
                                                      std::string& rev, Revision what) {
  TRI_ASSERT(key.empty());
  TRI_ASSERT(rev.empty());
  if (value.isObject()) {
    bool mustDestroy;
    AqlValue sub = value.get(resolver, StaticStrings::KeyString, mustDestroy, false);
    AqlValueGuard guard(sub, mustDestroy);

    if (sub.isString()) {
      key.assign(sub.slice().copyString());

      if (what == Revision::Include) {
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
                                                     std::string const& rev, Revision what) {
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(key));
  if (what == Revision::Exclude && !rev.empty()) {
    builder.add(StaticStrings::RevString, VPackValue(rev));
  } else {
    builder.add(StaticStrings::RevString, VPackValue(VPackValueType::Null));
  }
  builder.close();
  return Result{};
}

bool ModificationExecutorHelpers::writeRequired(ModificationExecutorInfos& infos,
                                                VPackSlice const& doc,
                                                std::string const& key) {
  return (!infos._consultAqlWriteFilter ||
          !infos._aqlCollection->getCollection()->skipForAqlWrite(doc, key));
}

// TODO: This has to be nicer.
void ModificationExecutorHelpers::throwOperationResultException(
    ModificationExecutorInfos& infos, OperationResult const& result) {
  std::string message;
  auto const& errorCounter = result.countErrorCodes;

  TRI_ASSERT(result.slice().isArray());

  // Find the first relevant error for which we want to throw.
  // If _ignoreDocumentNotFound is true, then this is any error other than
  // TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, otherwise it is just any error.
  //
  // Find the first error with a message and throw that
  // This mirrors previous behaviour and might not be entirely ideal.
  for (auto const p : errorCounter) {
    auto const errorCode = p.first;
    if (!(infos._ignoreDocumentNotFound && errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
      // Find the first error and throw with message.
      for (auto doc : VPackArrayIterator(result.slice())) {
        if (doc.isObject() && doc.hasKey(StaticStrings::ErrorNum) &&
            doc.get(StaticStrings::ErrorNum).getInt() == errorCode) {
          VPackSlice s = doc.get(StaticStrings::ErrorMessage);
          if (s.isString()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(errorCode, s.copyString());
          }
        }
      }
      // if we did not find a message, we still throw something, because we know
      // that a relevant error has happened
      THROW_ARANGO_EXCEPTION(errorCode);
    }
  }
}

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
    : _infos(infos), _fetcher(fetcher), _modifier(infos) {
  // important for mmfiles
  // TODO: Why is this important for mmfiles?
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
    std::tie(state, row) = _fetcher.fetchRow();
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, ModificationStats{}};
    }
    // Make sure we have a valid row
    TRI_ASSERT(row.isInitialized());

    _modifier.accumulate(row);
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
  if (_modifier.size() == 0 || _infos._options.silent) {
    _modifier.setupIterator(ModifierIteratorMode::OperationsOnly);
    InputAqlItemRow row{CreateInvalidInputRowHint{}};
    while (!_modifier.isFinishedIterator()) {
      std::tie(std::ignore, row, std::ignore) = _modifier.getOutput();

      output.copyRow(row);

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
      }
    }
    output.advanceRow();
    _modifier.advanceIterator();
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

  size_t maxOutputs = std::min(output.numRowsLeft(), _modifier.getBatchSize());

  std::tie(state, stats) = doCollect(maxOutputs);
  if (state == ExecutionState::WAITING) {
    return {ExecutionState::WAITING, std::move(stats)};
  }
  TRI_ASSERT(state == ExecutionState::DONE || state == ExecutionState::HASMORE);

  // Close the accumulator
  _modifier.close();
  //  auto transactResult =
  _modifier.transact();

  // If the transaction resulted in any errors,
  // they should have been thrown during transact
  // if (!transactResult.ok()) {
  // }

  doOutput(output, stats);

  return {state, std::move(stats)};
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
