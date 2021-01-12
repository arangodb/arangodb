////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutorHelpers.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/StaticStrings.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::aql::ModificationExecutorHelpers;
using namespace arangodb::basics;

template <class ModifierCompletion, typename Enable>
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::OutputIterator(
    SimpleModifier<ModifierCompletion, Enable> const& modifier)
    : _modifier(modifier),
      _operationsIterator(modifier._operations.begin()),
      _resultsIterator(modifier.getResultsIterator()) {}

template <typename ModifierCompletion, typename Enable>
typename SimpleModifier<ModifierCompletion, Enable>::OutputIterator&
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::next() {
  // Only move results on if there has been a document
  // submitted to the transaction
  if (_operationsIterator->first == ModifierOperationType::ReturnIfAvailable) {
    ++_resultsIterator;
  }
  ++_operationsIterator;
  return *this;
}

template <class ModifierCompletion, typename Enable>
typename SimpleModifier<ModifierCompletion, Enable>::OutputIterator&
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::operator++() {
  return next();
}

template <class ModifierCompletion, typename Enable>
bool SimpleModifier<ModifierCompletion, Enable>::OutputIterator::operator!=(
    SimpleModifier<ModifierCompletion, Enable>::OutputIterator const& other) const
    noexcept {
  return _operationsIterator != other._operationsIterator;
}

template <class ModifierCompletion, typename Enable>
ModifierOutput SimpleModifier<ModifierCompletion, Enable>::OutputIterator::operator*() const {
  switch (_operationsIterator->first) {
    case ModifierOperationType::ReturnIfAvailable: {
      // This means the results slice is relevant
      if (_modifier.resultAvailable()) {
        VPackSlice elm = *_resultsIterator;
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
        return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::CopyRow};
      }
      case ModifierOperationType::CopyRow:
        return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::CopyRow};
      case ModifierOperationType::SkipRow:
        return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::SkipRow};
    }
  }

  // Shut up compiler
  TRI_ASSERT(false);
  return ModifierOutput{_operationsIterator->second, ModifierOutput::Type::SkipRow};
}

template <class ModifierCompletion, typename Enable>
typename SimpleModifier<ModifierCompletion, Enable>::OutputIterator
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::begin() const {
  return SimpleModifier<ModifierCompletion, Enable>::OutputIterator(this->_modifier);
}

template <class ModifierCompletion, typename Enable>
typename SimpleModifier<ModifierCompletion, Enable>::OutputIterator
SimpleModifier<ModifierCompletion, Enable>::OutputIterator::end() const {
  auto it = SimpleModifier<ModifierCompletion, Enable>::OutputIterator(this->_modifier);
  it._operationsIterator = _modifier._operations.end();

  return it;
}

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::reset() {
  _accumulator.reset();
  _operations.clear();
  _results.reset();
}

template <typename ModifierCompletion, typename Enable>
Result SimpleModifier<ModifierCompletion, Enable>::accumulate(InputAqlItemRow& row) {
  auto result = _completion.accumulate(_accumulator, row);
  _operations.push_back({result, row});
  return Result{};
}

template <typename ModifierCompletion, typename Enable>
void SimpleModifier<ModifierCompletion, Enable>::transact(transaction::Methods& trx) {
  _results = _completion.transact(trx, _accumulator.closeAndGetContents());

  throwOperationResultException(_infos, _results);
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfOperations() const {
  return _operations.size();
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfDocuments() const {
  return _accumulator.nrOfDocuments();
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfResults() const {
  if (_results.hasSlice() && _results.slice().isArray()) {
    return _results.slice().length();
  }
  return 0;
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfErrors() const {
  size_t nrOfErrors{0};

  for (auto const& pair : _results.countErrorCodes) {
    nrOfErrors += pair.second;
  }
  return nrOfErrors;
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfWritesExecuted() const {
  return nrOfDocuments() - nrOfErrors();
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::nrOfWritesIgnored() const {
  return nrOfErrors();
}

template <typename ModifierCompletion, typename Enable>
ModificationExecutorInfos& SimpleModifier<ModifierCompletion, Enable>::getInfos() const
    noexcept {
  return _infos;
}

template <typename ModifierCompletion, typename Enable>
size_t SimpleModifier<ModifierCompletion, Enable>::getBatchSize() const noexcept {
  return _batchSize;
}

template <typename ModifierCompletion, typename Enable>
bool SimpleModifier<ModifierCompletion, Enable>::resultAvailable() const {
  return (nrOfDocuments() > 0 && !_infos._options.silent);
}

template <typename ModifierCompletion, typename Enable>
VPackArrayIterator SimpleModifier<ModifierCompletion, Enable>::getResultsIterator() const {
  if (resultAvailable()) {
    TRI_ASSERT(_results.hasSlice() && _results.slice().isArray());
    return VPackArrayIterator{_results.slice()};
  }
  return VPackArrayIterator(VPackArrayIterator::Empty{});
}

template class ::arangodb::aql::SimpleModifier<InsertModifierCompletion>;
template class ::arangodb::aql::SimpleModifier<RemoveModifierCompletion>;
template class ::arangodb::aql::SimpleModifier<UpdateReplaceModifierCompletion>;
