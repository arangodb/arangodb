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
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "ModificationExecutor2.h"
#include "ModificationExecutorTraits.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

class CollectionNameResolver;

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::aql::ModificationExecutorHelpers;

template <typename ModifierCompletion>
SimpleModifier<ModifierCompletion>::SimpleModifier(ModificationExecutorInfos& infos)
    : _infos(infos),
      _completion(*this),
      _resultsIterator(VPackSlice::emptyArraySlice()) {}

template <typename ModifierCompletion>
SimpleModifier<ModifierCompletion>::~SimpleModifier() = default;

template <typename ModifierCompletion>
void SimpleModifier<ModifierCompletion>::reset() {
  _accumulator.clear();
  _operations.clear();
  _results = OperationResult{};
}

template <typename ModifierCompletion>
Result SimpleModifier<ModifierCompletion>::accumulate(InputAqlItemRow& row) {
  return _completion.accumulate(row);
}

template <typename ModifierCompletion>
Result SimpleModifier<ModifierCompletion>::transact() {
  return _completion.transact();
}

template <typename ModifierCompletion>
size_t SimpleModifier<ModifierCompletion>::size() const {
  // TODO: spray around some asserts
  return _accumulator.slice().length();
}

template <typename ModifierCompletion>
Result SimpleModifier<ModifierCompletion>::setupIterator() {
  _operationsIterator = _operations.begin();
  _resultsIterator = VPackArrayIterator{_results.slice()};

  return Result{};
}

template <typename ModifierCompletion>
bool SimpleModifier<ModifierCompletion>::isFinishedIterator() {
  // TODO: Spray in some assserts
  return _operationsIterator == _operations.end();
}

template <typename ModifierCompletion>
void SimpleModifier<ModifierCompletion>::advanceIterator() {
  _operationsIterator++;
  _resultsIterator++;
}

// TODO: This is a bit ugly, explain at least what's going on
//       can we use local variables and just rely on the compiler not doing
//       anything silly?
// Super ugly pointer/iterator shenanigans
template <typename ModifierCompletion>
ModifierOutput SimpleModifier<ModifierCompletion>::getOutput() {
  return ModifierOutput{_operationsIterator->first, _operationsIterator->second,
                        *_resultsIterator};
}
