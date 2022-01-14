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

#pragma once

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutionEngine.h"

#include "Aql/ModificationExecutorAccumulator.h"
#include "Aql/ModificationExecutorInfos.h"

#include "Aql/InsertModifier.h"
#include "Aql/RemoveModifier.h"
#include "Aql/UpdateReplaceModifier.h"

#include <mutex>
#include <type_traits>

namespace arangodb::aql {

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
template<typename ModifierCompletion, typename _ = void>
struct is_modifier_completion_trait : std::false_type {};

template<>
struct is_modifier_completion_trait<InsertModifierCompletion> : std::true_type {
};

template<>
struct is_modifier_completion_trait<RemoveModifierCompletion> : std::true_type {
};

template<>
struct is_modifier_completion_trait<UpdateReplaceModifierCompletion>
    : std::true_type {};

template<typename ModifierCompletion,
         typename Enable = typename std::enable_if_t<
             is_modifier_completion_trait<ModifierCompletion>::value>>
class SimpleModifier : public std::enable_shared_from_this<
                           SimpleModifier<ModifierCompletion, Enable>> {
  friend class InsertModifierCompletion;
  friend class RemoveModifierCompletion;
  friend class UpdateReplaceModifierCompletion;

  struct NoResult {};
  struct Waiting {};
  using ResultType =
      std::variant<NoResult, Waiting, OperationResult, std::exception_ptr>;

 public:
  using ModOp = std::pair<ModifierOperationType, InputAqlItemRow>;

  class OutputIterator {
   public:
    OutputIterator() = delete;

    explicit OutputIterator(
        SimpleModifier<ModifierCompletion, Enable> const& modifier);

    OutputIterator& operator++();
    bool operator!=(OutputIterator const& other) const noexcept;
    [[nodiscard]] ModifierOutput operator*() const;
    [[nodiscard]] OutputIterator begin() const;
    [[nodiscard]] OutputIterator end() const;

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
        _batchSize(ExecutionBlock::DefaultBatchSize),
        _results(NoResult{}) {
    TRI_ASSERT(_infos.engine() != nullptr);
  }

  virtual ~SimpleModifier() = default;

  void reset();

  void accumulate(InputAqlItemRow& row);
  [[nodiscard]] ExecutionState transact(transaction::Methods& trx);

  void checkException() const;
  void resetResult() noexcept;

  // The two numbers below may not be the same: Operations
  // can skip or ignore documents.

  // The number of operations
  [[nodiscard]] size_t nrOfOperations() const;
  // The number of documents in the accumulator
  [[nodiscard]] size_t nrOfDocuments() const;
  // The number of entries in the results slice
  [[nodiscard]] [[maybe_unused]] size_t nrOfResults() const;

  [[nodiscard]] size_t nrOfErrors() const;

  [[nodiscard]] size_t nrOfWritesExecuted() const;
  [[nodiscard]] size_t nrOfWritesIgnored() const;

  [[nodiscard]] ModificationExecutorInfos& getInfos() const noexcept;
  [[nodiscard]] size_t getBatchSize() const noexcept;

  bool hasResultOrException() const noexcept;
  bool hasNeitherResultNorOperationPending() const noexcept;

 private:
  [[nodiscard]] bool resultAvailable() const;
  [[nodiscard]] VPackArrayIterator getResultsIterator() const;

  ModificationExecutorInfos& _infos;
  ModifierCompletion _completion;

  std::vector<ModOp> _operations;
  ModificationExecutorAccumulator _accumulator;

  size_t const _batchSize;

  /// @brief mutex that protects _results
  mutable std::mutex _resultMutex;
  ResultType _results;
};

using InsertModifier = SimpleModifier<InsertModifierCompletion>;
using RemoveModifier = SimpleModifier<RemoveModifierCompletion>;
using UpdateReplaceModifier = SimpleModifier<UpdateReplaceModifierCompletion>;

}  // namespace arangodb::aql
