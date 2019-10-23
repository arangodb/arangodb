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

#include "SimpleModifier.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "ModificationExecutor.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/LogMacros.h"

class CollectionNameResolver;

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::aql::ModificationExecutorHelpers;

template <class ModifierCompletion, typename Enable>
SimpleModifier<ModifierCompletion, Enable>::SimpleModifier(ModificationExecutorInfos& infos)
    : _infos(infos),
      _completion(infos),
      _resultsIterator(VPackSlice::emptyArraySlice()),
      _batchSize(ExecutionBlock::DefaultBatchSize()) {}

template <typename ModifierCompletion, typename Enable>
SimpleModifier<ModifierCompletion, Enable>::~SimpleModifier() = default;

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::reset() {
  _accumulator.clear();
  _operations.clear();
  _results = OperationResult{};
  _accumulator.openArray();
}

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::close() {
  _accumulator.close();
  TRI_ASSERT(_accumulator.isClosed());
}

template <typename ModifierCompletion, typename Enable>
Result SimpleModifier<ModifierCompletion, Enable>::accumulate(InputAqlItemRow& row) {
  auto result = _completion.accumulate(_accumulator, row);
  TRI_ASSERT(!_accumulator.isClosed());
  _operations.push_back({result, row});
  return Result{};
}

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::transact() {
  TRI_ASSERT(_accumulator.isClosed());
  TRI_ASSERT(_accumulator.slice().isArray());
  _results = _completion.transact(_accumulator.slice());

  throwOperationResultException(_infos, _results);
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfOperations() const {
  return _operations.size();
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfDocuments() const {
  TRI_ASSERT(_accumulator.slice().isArray());
  return _accumulator.slice().length();
}

template <typename ModifierCompletion, typename Enable>
Result SimpleModifier<ModifierCompletion, Enable>::setupIterator(ModifierIteratorMode const mode) {
  _iteratorMode = mode;
  _operationsIterator = _operations.begin();
  if (mode == ModifierIteratorMode::Full) {
    TRI_ASSERT(_results.slice().isArray());
    _resultsIterator = VPackArrayIterator{_results.slice()};
  }
  return Result{};
}

template <typename ModifierCompletion, typename Enable>
bool SimpleModifier<ModifierCompletion, Enable>::isFinishedIterator() {
  // TODO: Spray in some assserts
  return _operationsIterator == _operations.end();
}

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::advanceIterator() {
  _operationsIterator++;
  _resultsIterator++;
}

template <typename ModifierCompletion, typename Enable>
ModificationExecutorInfos& SimpleModifier<ModifierCompletion, Enable>::getInfos() const {
  return _infos;
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::getBatchSize() const {
  return _batchSize;
}

// TODO: This is a bit ugly, explain at least what's going on
//       can we use local variables and just rely on the compiler not doing
//       anything silly?
// The ModifierOuptut is a triple consisting of the ModOperationType, the InputRow,
// and the result returned by the transaction method called in transact.
template <typename ModifierCompletion, typename Enable>
ModifierOutput SimpleModifier<ModifierCompletion, Enable>::getOutput() {
  switch (_iteratorMode) {
    case ModifierIteratorMode::Full: {
      return ModifierOutput{_operationsIterator->first,
                            _operationsIterator->second, *_resultsIterator};
    }
    case ModifierIteratorMode::OperationsOnly: {
      return ModifierOutput{_operationsIterator->first,
                            _operationsIterator->second, VPackSlice::noneSlice()};
    }
    default: {
      TRI_ASSERT(false);
    }
  }
}

template class ::arangodb::aql::SimpleModifier<InsertModifierCompletion>;
template class ::arangodb::aql::SimpleModifier<RemoveModifierCompletion>;
template class ::arangodb::aql::SimpleModifier<UpdateReplaceModifierCompletion>;
