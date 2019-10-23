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
#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutorHelpers.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "Transaction/Methods.h"
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

ModOperationType UpsertModifier::updateReplaceCase(VPackBuilder& accu, AqlValue const& inDoc,
                                                   AqlValue const& updateDoc) {
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

        VPackCollection::merge(accu, toUpdate, keyDocBuilder.slice(), false, false);

        return ModOperationType::APPLY_UPDATE;
      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
            std::string("expecting 'Object', got: ") + updateDoc.slice().typeName() +
                std::string(" while handling: UPSERT"));
        return ModOperationType::IGNORE_SKIP;
      }
    } else {
      if (!_infos._ignoreErrors) {
        THROW_ARANGO_EXCEPTION_MESSAGE(result.errorNumber(), result.errorMessage());
      }
      return ModOperationType::IGNORE_SKIP;
    }
  } else {
    return ModOperationType::IGNORE_RETURN;
  }
}

ModOperationType UpsertModifier::insertCase(VPackBuilder& accu, AqlValue const& insertDoc) {
  if (insertDoc.isObject()) {
    auto const& toInsert = insertDoc.slice();
    if (writeRequired(_infos, toInsert, StaticStrings::Empty)) {
      accu.add(toInsert);
      return ModOperationType::APPLY_INSERT;
    } else {
      return ModOperationType::IGNORE_RETURN;
    }
  } else {
    if (!_infos._ignoreErrors) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                                     std::string("expecting 'Object', got: ") +
                                         insertDoc.slice().typeName() +
                                         " while handling: UPSERT");
    }
    return ModOperationType::IGNORE_SKIP;
  }
}

Result UpsertModifier::accumulate(InputAqlItemRow& row) {
  RegisterId const inDocReg = _infos._input1RegisterId;
  RegisterId const insertReg = _infos._input2RegisterId;
  RegisterId const updateReg = _infos._input3RegisterId;

  ModOperationType result;

  // The document to be UPSERTed
  AqlValue const& inDoc = row.getValue(inDocReg);

  // if there is a document in the input register, we
  // update that document, otherwise, we insert
  if (inDoc.isObject()) {
    auto updateDoc = row.getValue(updateReg);
    result = updateReplaceCase(_updateAccumulator, inDoc, updateDoc);
  } else {
    auto insertDoc = row.getValue(insertReg);
    result = insertCase(_insertAccumulator, insertDoc);
  }
  _operations.push_back({result, row});
  return Result{};
}

Result UpsertModifier::transact() {
  auto toInsert = _insertAccumulator.slice();
  if (toInsert.isArray() && toInsert.length() > 0) {
    _insertResults =
        _infos._trx->insert(_infos._aqlCollection->name(), toInsert, _infos._options);
    throwOperationResultException(_infos, _insertResults);
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
    throwOperationResultException(_infos, _updateResults);
  }

  return Result{};
}

size_t UpsertModifier::nrOfDocuments() const {
  return _insertAccumulator.slice().length() + _updateAccumulator.slice().length();
}

size_t UpsertModifier::nrOfOperations() const { return _operations.size(); }

Result UpsertModifier::setupIterator(ModifierIteratorMode const mode) {
  _iteratorMode = mode;
  _operationsIterator = _operations.begin();
  if (mode == ModifierIteratorMode::Full) {
    if (!_insertResults.hasSlice() || _insertResults.slice().isNone()) {
      _insertResultsIterator = VPackArrayIterator(VPackSlice::emptyArraySlice());
    } else if (_insertResults.slice().isArray()) {
      _insertResultsIterator = VPackArrayIterator(_insertResults.slice());
    } else {
      TRI_ASSERT(false);
    }

    if (!_updateResults.hasSlice() || _updateResults.slice().isNone()) {
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
  return _operationsIterator == _operations.end();
}

void UpsertModifier::advanceIterator() {
  if (_iteratorMode == ModifierIteratorMode::Full) {
    if (_operationsIterator->first == ModOperationType::APPLY_UPDATE) {
      _updateResultsIterator++;
    } else if (_operationsIterator->first == ModOperationType::APPLY_INSERT) {
      _insertResultsIterator++;
    }
  }
  _operationsIterator++;
}

// When we get the output of our iterator, we have to check whether the
// operation in question was APPLY_UPDATE or APPLY_INSERT to determine which
// of the results slices (UpdateReplace or Insert) we have to look in and
// increment.
UpsertModifier::OutputTuple UpsertModifier::getOutput() {
  switch (_iteratorMode) {
    case ModifierIteratorMode::Full: {
      if (_operationsIterator->first == ModOperationType::APPLY_UPDATE) {
        return OutputTuple{ModOperationType::APPLY_RETURN,
                           _operationsIterator->second, *_updateResultsIterator};
      } else if (_operationsIterator->first == ModOperationType::APPLY_INSERT) {
        return OutputTuple{ModOperationType::APPLY_RETURN,
                           _operationsIterator->second, *_insertResultsIterator};
      } else {
        return OutputTuple{_operationsIterator->first,
                           _operationsIterator->second, VPackSlice::noneSlice()};
      }
    }
    case ModifierIteratorMode::OperationsOnly: {
      return OutputTuple{_operationsIterator->first,
                         _operationsIterator->second, VPackSlice::noneSlice()};
    }
  }
  // shut up compiler
  TRI_ASSERT(false);
  return OutputTuple{ModOperationType::IGNORE_SKIP,
                     InputAqlItemRow{CreateInvalidInputRowHint()},
                     VPackSlice::noneSlice()};
}

size_t UpsertModifier::getBatchSize() const { return _batchSize; }
