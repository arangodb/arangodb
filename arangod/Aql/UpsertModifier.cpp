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
#include "Aql/ModificationExecutorAccumulator.h"
#include "Aql/ModificationExecutorHelpers.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/QueryContext.h"
#include "Basics/Common.h"
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/LogMacros.h"

class CollectionNameResolver;

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::aql::ModificationExecutorHelpers;
using namespace arangodb::basics;

UpsertModifier::OutputIterator::OutputIterator(UpsertModifier const& modifier)
    : _modifier(modifier),
      _operationsIterator(modifier._operations.begin()),
      _insertResultsIterator(modifier.getInsertResultsIterator()),
      _updateResultsIterator(modifier.getUpdateResultsIterator()) {}

UpsertModifier::OutputIterator& UpsertModifier::OutputIterator::next() {
  if (_operationsIterator->first == UpsertModifier::OperationType::UpdateReturnIfAvailable) {
    ++_updateResultsIterator;
  } else if (_operationsIterator->first == UpsertModifier::OperationType::InsertReturnIfAvailable) {
    ++_insertResultsIterator;
  }
  ++_operationsIterator;
  return *this;
}

UpsertModifier::OutputIterator& UpsertModifier::OutputIterator::operator++() {
  return next();
}

bool UpsertModifier::OutputIterator::operator!=(UpsertModifier::OutputIterator const& other) const
    noexcept {
  return _operationsIterator != other._operationsIterator;
}

ModifierOutput UpsertModifier::OutputIterator::operator*() const {
  // When we get the output of our iterator, we have to check whether the
  // operation in question was APPLY_UPDATE or APPLY_INSERT to determine which
  // of the results slices (UpdateReplace or Insert) we have to look in and
  // increment.
  if (_modifier.resultAvailable()) {
    VPackSlice elm;

    switch (_operationsIterator->first) {
      case UpsertModifier::OperationType::CopyRow:
        return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::CopyRow};
      case UpsertModifier::OperationType::SkipRow:
        return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::SkipRow};
      case UpsertModifier::OperationType::UpdateReturnIfAvailable:
        elm = *_updateResultsIterator;
        break;
      case UpsertModifier::OperationType::InsertReturnIfAvailable:
        elm = *_insertResultsIterator;
        break;
    }

    bool error = VelocyPackHelper::getBooleanValue(elm, StaticStrings::Error, false);
    if (error) {
      return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::SkipRow};
    } else {
      return ModifierOutput{
          _operationsIterator->second, ModifierOutput::Type::ReturnIfRequired,
          ModificationExecutorHelpers::getDocumentOrNull(elm, StaticStrings::Old),
          ModificationExecutorHelpers::getDocumentOrNull(elm, StaticStrings::New)};
    }
  } else {
    switch (_operationsIterator->first) {
      case UpsertModifier::OperationType::UpdateReturnIfAvailable:
      case UpsertModifier::OperationType::InsertReturnIfAvailable:
      case UpsertModifier::OperationType::CopyRow:
        return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::CopyRow};
      case UpsertModifier::OperationType::SkipRow:
        return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::SkipRow};
    }
  }

  // shut up compiler
  TRI_ASSERT(false);
  return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::SkipRow};
}

typename UpsertModifier::OutputIterator UpsertModifier::OutputIterator::begin() const {
  return UpsertModifier::OutputIterator(this->_modifier);
}

typename UpsertModifier::OutputIterator UpsertModifier::OutputIterator::end() const {
  auto it = UpsertModifier::OutputIterator(this->_modifier);
  it._operationsIterator = _modifier._operations.end();

  return it;
}

void UpsertModifier::reset() {
  _insertAccumulator.reset();
  _insertResults.reset();
  _updateAccumulator.reset();
  _updateResults.reset();

  _operations.clear();
}

UpsertModifier::OperationType UpsertModifier::updateReplaceCase(
    ModificationExecutorAccumulator& accu, AqlValue const& inDoc, AqlValue const& updateDoc) {
  if (writeRequired(_infos, inDoc.slice(), StaticStrings::Empty)) {
    CollectionNameResolver const& collectionNameResolver{_infos._query.resolver()};

    // We are only interested in the key from `inDoc`
    std::string key;
    Result result = getKey(collectionNameResolver, inDoc, key);

    if (!result.ok()) {
      if (!_infos._ignoreErrors) {
        THROW_ARANGO_EXCEPTION(result);
      }
      return UpsertModifier::OperationType::SkipRow;
    }

    if (updateDoc.isObject()) {
      VPackSlice toUpdate = updateDoc.slice();
      _keyDocBuilder.clear();

      buildKeyDocument(_keyDocBuilder, key);
      auto merger = VPackCollection::merge(toUpdate, _keyDocBuilder.slice(), false, false);
      accu.add(merger.slice());

      return UpsertModifier::OperationType::UpdateReturnIfAvailable;
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                                     std::string("expecting 'Object', got: ") +
                                         updateDoc.slice().typeName() + std::string(" while handling: UPSERT"));
      return UpsertModifier::OperationType::SkipRow;
    }
  } else {
    return UpsertModifier::OperationType::CopyRow;
  }
}

UpsertModifier::OperationType UpsertModifier::insertCase(ModificationExecutorAccumulator& accu,
                                                         AqlValue const& insertDoc) {
  if (insertDoc.isObject()) {
    auto const& toInsert = insertDoc.slice();
    if (writeRequired(_infos, toInsert, StaticStrings::Empty)) {
      accu.add(toInsert);
      return UpsertModifier::OperationType::InsertReturnIfAvailable;
    } else {
      return UpsertModifier::OperationType::CopyRow;
    }
  } else {
    if (!_infos._ignoreErrors) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                                     std::string("expecting 'Object', got: ") +
                                         insertDoc.slice().typeName() +
                                         " while handling: UPSERT");
    }
    return UpsertModifier::OperationType::SkipRow;
  }
}

bool UpsertModifier::resultAvailable() const {
  return (nrOfDocuments() > 0 && !_infos._options.silent);
}

VPackArrayIterator UpsertModifier::getUpdateResultsIterator() const {
  if (_updateResults.hasSlice() && _updateResults.slice().isArray()) {
    return VPackArrayIterator(_updateResults.slice());
  }
  return VPackArrayIterator(VPackArrayIterator::Empty{});
}

VPackArrayIterator UpsertModifier::getInsertResultsIterator() const {
  if (_insertResults.hasSlice() && _insertResults.slice().isArray()) {
    return VPackArrayIterator(_insertResults.slice());
  }
  return VPackArrayIterator(VPackArrayIterator::Empty{});
}

Result UpsertModifier::accumulate(InputAqlItemRow& row) {
  RegisterId const inDocReg = _infos._input1RegisterId;
  RegisterId const insertReg = _infos._input2RegisterId;
  RegisterId const updateReg = _infos._input3RegisterId;

  UpsertModifier::OperationType result;

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

Result UpsertModifier::transact(transaction::Methods& trx) {
  auto toInsert = _insertAccumulator.closeAndGetContents();
  if (toInsert.isArray() && toInsert.length() > 0) {
    _insertResults =
        trx.insert(_infos._aqlCollection->name(), toInsert, _infos._options);
    throwOperationResultException(_infos, _insertResults);
  }

  auto toUpdate = _updateAccumulator.closeAndGetContents();
  if (toUpdate.isArray() && toUpdate.length() > 0) {
    if (_infos._isReplace) {
      _updateResults = trx.replace(_infos._aqlCollection->name(),
                                   toUpdate, _infos._options);
    } else {
      _updateResults = trx.update(_infos._aqlCollection->name(),
                                  toUpdate, _infos._options);
    }
    throwOperationResultException(_infos, _updateResults);
  }

  return Result{};
}

size_t UpsertModifier::nrOfDocuments() const {
  return _insertAccumulator.nrOfDocuments() + _updateAccumulator.nrOfDocuments();
}

size_t UpsertModifier::nrOfOperations() const { return _operations.size(); }

size_t UpsertModifier::nrOfResults() const {
  size_t n{0};

  if (_insertResults.hasSlice() && _insertResults.slice().isArray()) {
    n += _insertResults.slice().length();
  }
  if (_updateResults.hasSlice() && _updateResults.slice().isArray()) {
    n += _updateResults.slice().length();
  }
  return n;
}

size_t UpsertModifier::nrOfErrors() const {
  size_t nrOfErrors{0};

  for (auto const& pair : _insertResults.countErrorCodes) {
    nrOfErrors += pair.second;
  }
  for (auto const& pair : _updateResults.countErrorCodes) {
    nrOfErrors += pair.second;
  }
  return nrOfErrors;
}

size_t UpsertModifier::nrOfWritesExecuted() const {
  return nrOfDocuments() - nrOfErrors();
}

size_t UpsertModifier::nrOfWritesIgnored() const { return nrOfErrors(); }

size_t UpsertModifier::getBatchSize() const { return _batchSize; }
