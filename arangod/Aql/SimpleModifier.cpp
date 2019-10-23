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
#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutorHelpers.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
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
      _accumulator(nullptr),
      _resultsIterator(VPackSlice::emptyArraySlice()),
      _batchSize(ExecutionBlock::DefaultBatchSize()) {}

template <typename ModifierCompletion, typename Enable>
SimpleModifier<ModifierCompletion, Enable>::~SimpleModifier() = default;

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::reset() {
  _accumulator.reset(new ModificationExecutorAccumulator());
  _operations.clear();
  _results = OperationResult{};
}

template <typename ModifierCompletion, typename Enable>
Result SimpleModifier<ModifierCompletion, Enable>::accumulate(InputAqlItemRow& row) {
  TRI_ASSERT(_accumulator != nullptr);
  auto result = _completion.accumulate(*_accumulator.get(), row);
  _operations.push_back({result, row});
  return Result{};
}

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::transact() {
  _results = _completion.transact(_accumulator->closeAndGetContents());

  throwOperationResultException(_infos, _results);
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfOperations() const {
  return _operations.size();
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfDocuments() const {
  return _accumulator->nrOfDocuments();
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
  return _operationsIterator == _operations.end();
}

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::advanceIterator() {
  // Only move results on if there has been a document
  // submitted to the transaction
  if (_operationsIterator->first == ModOperationType::APPLY_RETURN) {
    _resultsIterator++;
  }
  _operationsIterator++;
}

template <typename ModifierCompletion, typename Enable>
ModificationExecutorInfos& SimpleModifier<ModifierCompletion, Enable>::getInfos() const {
  return _infos;
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::getBatchSize() const {
  return _batchSize;
}

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
  }
  TRI_ASSERT(false);
  return ModifierOutput{ModOperationType::IGNORE_SKIP,
                        InputAqlItemRow{CreateInvalidInputRowHint()},
                        VPackSlice::noneSlice()};
}

template class ::arangodb::aql::SimpleModifier<InsertModifierCompletion>;
template class ::arangodb::aql::SimpleModifier<RemoveModifierCompletion>;
template class ::arangodb::aql::SimpleModifier<UpdateReplaceModifierCompletion>;
