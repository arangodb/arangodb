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

#include "UpsertModifier.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "ModificationExecutor2.h"
#include "ModificationExecutorTraits.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/LogMacros.h"

class CollectionNameResolver;

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::aql::ModificationExecutorHelpers;

UpsertModifier::UpsertModifier(ModificationExecutorInfos& infos)
    : _infos(infos),
      _updateResultsIterator(VPackSlice::emptyArraySlice()),
      _insertResultsIterator(VPackSlice::emptyArraySlice()),

      // Batch size has to be 1 so that the upsert modifier sees its own
      // writes.
      // This behaviour could be improved, if we can prove that an UPSERT
      // does not need to see its own writes
      _batchSize(1) {}

UpsertModifier::~UpsertModifier() = default;

void UpsertModifier::reset() {
  _insertAccumulator.clear();
  _insertAccumulator.openArray();
  _insertResults = OperationResult{};

  _updateAccumulator.clear();
  _updateAccumulator.openArray();
  _updateResults = OperationResult{};

  _operations.clear();
}

void UpsertModifier::close() {
  _insertAccumulator.close();
  _updateAccumulator.close();
  TRI_ASSERT(_insertAccumulator.isClosed());
  TRI_ASSERT(_updateAccumulator.isClosed());
}

// TODO: In principle the following two functions should be the same as in the Update/Replace modifier
// and the insert modifier, respectively; There is probably a sensible way of factoring them out
Result UpsertModifier::updateCase(AqlValue const& inDoc, AqlValue const& updateDoc,
                                  InputAqlItemRow const& row) {
  std::string key, rev;
  Result result;

  if (writeRequired(_infos, inDoc.slice(), StaticStrings::Empty)) {
    TRI_ASSERT(_infos._trx->resolver() != nullptr);
    CollectionNameResolver const& collectionNameResolver{*_infos._trx->resolver()};

    result = getKeyAndRevision(collectionNameResolver, inDoc, key, rev, Revision::Exclude);

    if (result.ok()) {
      if (updateDoc.isObject()) {
        VPackSlice toUpdate = updateDoc.slice();
        VPackBuilder keyDocBuilder;

        buildKeyDocument(keyDocBuilder, key, rev);

        VPackCollection::merge(_updateAccumulator, toUpdate,
                               keyDocBuilder.slice(), false, false);

        _operations.push_back({ModOperationType::APPLY_UPDATE, row});
      } else {
        _operations.push_back({ModOperationType::IGNORE_SKIP, row});
        return Result{TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                      std::string("expecting 'Object', got: ") +
                          updateDoc.slice().typeName() +
                          std::string(" while handling: UPSERT")};
      }
    } else {
      _operations.push_back({ModOperationType::IGNORE_SKIP, row});
      return result;
    }
  } else {
    _operations.push_back({ModOperationType::IGNORE_RETURN, row});
  }
  return Result{};
}

Result UpsertModifier::insertCase(AqlValue const& insertDoc, InputAqlItemRow const& row) {
  auto const& toInsert = insertDoc.slice();
  if (insertDoc.isObject()) {
    if (writeRequired(_infos, toInsert, StaticStrings::Empty)) {
      _insertAccumulator.add(toInsert);
      _operations.push_back({ModOperationType::APPLY_INSERT, row});
    } else {
      _operations.push_back({ModOperationType::IGNORE_RETURN, row});
    }
  } else {
    _operations.push_back({ModOperationType::IGNORE_SKIP, row});
    return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                  std::string("expecting 'Object', got: ") +
                      insertDoc.slice().typeName() + " while handling: UPSERT");
  }
  return Result{};
}

Result UpsertModifier::accumulate(InputAqlItemRow& row) {
  RegisterId const inDocReg = _infos._input1RegisterId;
  RegisterId const insertReg = _infos._input2RegisterId;
  RegisterId const updateReg = _infos._input3RegisterId;

  Result result;

  // The document to be UPSERTed
  AqlValue const& inDoc = row.getValue(inDocReg);

  // if there is a document in the input register, we
  // update that document, otherwise, we insert
  // TODO: What if inDoc is a string? Should that
  // be an error?
  if (inDoc.isObject()) {
    auto const& updateDoc = row.getValue(updateReg);
    result = updateCase(inDoc, updateDoc, row);
  } else {
    auto const& insertDoc = row.getValue(insertReg);
    result = insertCase(insertDoc, row);
  }
  if (!result.ok() && !_infos._ignoreErrors) {
    THROW_ARANGO_EXCEPTION_MESSAGE(result.errorNumber(), result.errorMessage());
  }
  return Result{};
}

Result UpsertModifier::transact() {
  auto toInsert = _insertAccumulator.slice();
  if (toInsert.isArray() && toInsert.length() > 0) {
    _insertResults =
        _infos._trx->insert(_infos._aqlCollection->name(), toInsert, _infos._options);
    if (_insertResults.fail()) {
      throwOperationResultException(_infos, _insertResults);
    }
  }

  auto toUpdate = _updateAccumulator.slice();
  if (toUpdate.isArray() && toUpdate.length() > 0) {
    if (_infos._isReplace) {
      _updateResults = _infos._trx->replace(_infos._aqlCollection->name(),
                                            toUpdate, _infos._options);
    } else {
      _updateResults = _infos._trx->update(_infos._aqlCollection->name(),
                                           toUpdate, _infos._options);
    }
    if (_updateResults.fail()) {
      throwOperationResultException(_infos, _insertResults);
    }
  }

  return Result{};
}

size_t UpsertModifier::size() const {
  return _insertAccumulator.slice().length() + _updateAccumulator.slice().length();
}

size_t UpsertModifier::nrOfOperations() const { return _operations.size(); }

Result UpsertModifier::setupIterator(ModifierIteratorMode const mode) {
  _iteratorMode = mode;
  _operationsIterator = _operations.begin();
  if (mode == ModifierIteratorMode::Full) {
    if (_insertResults.slice().isNone()) {
      _insertResultsIterator = VPackArrayIterator(VPackSlice::emptyArraySlice());
    } else if (_insertResults.slice().isArray()) {
      _insertResultsIterator = VPackArrayIterator(_insertResults.slice());
    } else {
      TRI_ASSERT(false);
    }

    if (_updateResults.slice().isNone()) {
      _updateResultsIterator = VPackArrayIterator(VPackSlice::emptyArraySlice());
    } else if (_updateResults.slice().isArray()) {
      _updateResultsIterator = VPackArrayIterator(_updateResults.slice());
    } else {
      TRI_ASSERT(false);
    }
  }

  return Result{};
}

bool UpsertModifier::isFinishedIterator() {
  // TODO: Spray in some assserts
  return _operationsIterator == _operations.end();
}

void UpsertModifier::advanceIterator() {
  // TODO: wat?
  TRI_ASSERT(_operationsIterator->first == ModOperationType::APPLY_UPDATE ||
             _operationsIterator->first == ModOperationType::APPLY_INSERT);

  if (_iteratorMode == ModifierIteratorMode::Full) {
    if (_operationsIterator->first == ModOperationType::APPLY_UPDATE) {
      _updateResultsIterator++;
    } else if (_operationsIterator->first == ModOperationType::APPLY_INSERT) {
      _insertResultsIterator++;
    }
  }
  _operationsIterator++;
}

// TODO: This is a bit ugly, explain at least what's going on
//       can we use local variables and just rely on the compiler not doing
//       anything silly?
// Super ugly pointer/iterator shenanigans
UpsertModifier::OutputTuple UpsertModifier::getOutput() {
  TRI_ASSERT(_operationsIterator->first == ModOperationType::APPLY_UPDATE ||
             _operationsIterator->first == ModOperationType::APPLY_INSERT);

  switch (_iteratorMode) {
    case ModifierIteratorMode::Full: {
      if (_operationsIterator->first == ModOperationType::APPLY_UPDATE) {
        return OutputTuple{ModOperationType::APPLY_RETURN,
                           _operationsIterator->second, *_updateResultsIterator};
      } else if (_operationsIterator->first == ModOperationType::APPLY_INSERT) {
        return OutputTuple{ModOperationType::APPLY_RETURN,
                           _operationsIterator->second, *_insertResultsIterator};
      } else {
        TRI_ASSERT(false);
        // TODO: Warning about control reaches end of non-void function
        return OutputTuple{ModOperationType::IGNORE_SKIP,
                           InputAqlItemRow{CreateInvalidInputRowHint()},
                           VPackSlice::noneSlice()};
      }
    }
    case ModifierIteratorMode::OperationsOnly: {
      return OutputTuple{_operationsIterator->first,
                         _operationsIterator->second, VPackSlice::noneSlice()};
    }
  }
  // Shut up compiler
  TRI_ASSERT(false);
  return OutputTuple{ModOperationType::IGNORE_SKIP,
                     InputAqlItemRow{CreateInvalidInputRowHint()},
                     VPackSlice::noneSlice()};
}

size_t UpsertModifier::getBatchSize() const { return _batchSize; }
