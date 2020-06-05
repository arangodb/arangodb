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

#ifndef ARANGOD_AQL_SIMPLE_MODIFIER_H
#define ARANGOD_AQL_SIMPLE_MODIFIER_H

#include "Aql/ExecutionBlock.h"
#include "Aql/ModificationExecutorAccumulator.h"
#include "Aql/ModificationExecutorInfos.h"

#include "Aql/InsertModifier.h"
#include "Aql/RemoveModifier.h"
#include "Aql/UpdateReplaceModifier.h"

#include <type_traits>

namespace arangodb {
namespace aql {

struct ModificationExecutorInfos;

//
// The SimpleModifier template class is the template for the simple modifiers
// Insert, Remove, Replace, and Update.
//
// It provides the accumulator for building up the VelocyPack that is submitted
// to the transaction, and a facility to iterate over the results of the
// operation.
//
// The only code that the ModifierCompletions have to implement is the
// accumulate and transact functions. The accumulate function collects the
// actual modifications and there is a specific one for Insert, Remove, and
// Update/Replace. The transact function calls the correct method for the
// transaction (insert, remove, update, replace), and the only difference
// between Update and Replace is which transaction method is called.
//

// Only classes that have is_modifier_completion_trait can be used as
// template parameter for SimpleModifier. This is mainly a safety measure
// to not run into ridiculous template errors
template <typename ModifierCompletion, typename _ = void>
struct is_modifier_completion_trait : std::false_type {};

template <>
struct is_modifier_completion_trait<InsertModifierCompletion> : std::true_type {};

template <>
struct is_modifier_completion_trait<RemoveModifierCompletion> : std::true_type {};

template <>
struct is_modifier_completion_trait<UpdateReplaceModifierCompletion> : std::true_type {
};

template <typename ModifierCompletion, typename Enable = typename std::enable_if_t<is_modifier_completion_trait<ModifierCompletion>::value>>
class SimpleModifier {
  friend class InsertModifierCompletion;
  friend class RemoveModifierCompletion;
  friend class UpdateReplaceModifierCompletion;

 public:
  using ModOp = std::pair<ModifierOperationType, InputAqlItemRow>;

  class OutputIterator {
   public:
    OutputIterator() = delete;

    explicit OutputIterator(SimpleModifier<ModifierCompletion, Enable> const& modifier);

    OutputIterator& operator++();
    bool operator!=(OutputIterator const& other) const noexcept;
    ModifierOutput operator*() const;
    OutputIterator begin() const;
    OutputIterator end() const;

   private:
    OutputIterator& next();

    SimpleModifier<ModifierCompletion, Enable> const& _modifier;
    std::vector<ModOp>::const_iterator _operationsIterator;
    VPackArrayIterator _resultsIterator;
  };

 public:
  explicit SimpleModifier(ModificationExecutorInfos& infos)
      : _infos(infos),
        _completion(infos),
        _resultsIterator(VPackArrayIterator::Empty{}),
        _batchSize(ExecutionBlock::DefaultBatchSize) {}
  ~SimpleModifier() = default;

  void reset();

  Result accumulate(InputAqlItemRow& row);
  void transact(transaction::Methods& trx);

  // The two numbers below may not be the same: Operations
  // can skip or ignore documents.

  // The number of operations
  size_t nrOfOperations() const;
  // The number of documents in the accumulator
  size_t nrOfDocuments() const;
  // The number of entries in the results slice
  size_t nrOfResults() const;

  size_t nrOfErrors() const;

  size_t nrOfWritesExecuted() const;
  size_t nrOfWritesIgnored() const;

  ModificationExecutorInfos& getInfos() const noexcept;
  size_t getBatchSize() const noexcept;

 private:
  bool resultAvailable() const;
  VPackArrayIterator getResultsIterator() const;

  ModificationExecutorInfos& _infos;
  ModifierCompletion _completion;

  std::vector<ModOp> _operations;
  ModificationExecutorAccumulator _accumulator;

  OperationResult _results;

  std::vector<ModOp>::const_iterator _operationsIterator;
  VPackArrayIterator _resultsIterator;

  size_t const _batchSize;
};

using InsertModifier = SimpleModifier<InsertModifierCompletion>;
using RemoveModifier = SimpleModifier<RemoveModifierCompletion>;
using UpdateReplaceModifier = SimpleModifier<UpdateReplaceModifierCompletion>;

}  // namespace aql
}  // namespace arangodb

#endif
