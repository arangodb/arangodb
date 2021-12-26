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

#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutorAccumulator.h"
#include "Aql/ModificationExecutorInfos.h"

#include "Aql/InsertModifier.h"
#include "Aql/UpdateReplaceModifier.h"

#include <mutex>

#include <velocypack/Builder.h>

namespace arangodb::aql {

struct ModificationExecutorInfos;

// TODO Remove this state, and use a variant as in SimpleModifier.
//      It makes most sense to do this when implementing async upsert
//      operations, so I'm leaving it for now.
enum class ModificationExecutorResultState {
  // State that is used when the Executor's modifier has not been
  // asked to produce a result.
  // this is also the initial state.
  NoResult,
  // State that is used when the Executor's modifier has been asked
  // to produce a result, but it returned a WAITING status, i.e. the
  // result is not yet ready to consume.
  // This state cannot happen in single servers!
  WaitingForResult,
  // State that is used when the Executor's modifier has produced
  // a result that is ready to consume.
  HaveResult,
};

class UpsertModifier {
 public:
  enum class OperationType {
    // Return the OLD and/or NEW value, if requested, otherwise CopyRow
    InsertReturnIfAvailable,
    UpdateReturnIfAvailable,
    // Just copy the InputAqlItemRow to the OutputAqlItemRow
    CopyRow,
    // Do not produce any output
    SkipRow
  };
  using ModOp = std::pair<UpsertModifier::OperationType, InputAqlItemRow>;

  class OutputIterator {
   public:
    OutputIterator() = delete;

    explicit OutputIterator(UpsertModifier const& modifier);

    OutputIterator& operator++();
    bool operator!=(OutputIterator const& other) const noexcept;
    ModifierOutput operator*() const;
    [[nodiscard]] OutputIterator begin() const;
    [[nodiscard]] OutputIterator end() const;

   private:
    [[nodiscard]] OutputIterator& next();

    UpsertModifier const& _modifier;
    std::vector<ModOp>::const_iterator _operationsIterator;
    VPackArrayIterator _insertResultsIterator;
    VPackArrayIterator _updateResultsIterator;
  };

 public:
  explicit UpsertModifier(ModificationExecutorInfos& infos)
      : _infos(infos),
        _updateResults(Result(), infos._options),
        _insertResults(Result(), infos._options),

        // Batch size has to be 1 so that the upsert modifier sees its own
        // writes.
        // This behaviour could be improved, if we can prove that an UPSERT
        // does not need to see its own writes
        _batchSize(1),
        _resultState(ModificationExecutorResultState::NoResult) {}

  ~UpsertModifier() = default;

  ModificationExecutorResultState resultState() const noexcept;

  void checkException() const {}
  void resetResult() noexcept;
  void reset();

  void accumulate(InputAqlItemRow& row);
  ExecutionState transact(transaction::Methods& trx);

  size_t nrOfOperations() const;
  size_t nrOfDocuments() const;
  [[maybe_unused]] size_t nrOfResults() const;
  size_t nrOfErrors() const;
  size_t nrOfWritesExecuted() const;
  size_t nrOfWritesIgnored() const;

  size_t getBatchSize() const;

  bool hasResultOrException() const noexcept;
  bool hasNeitherResultNorOperationPending() const noexcept;

 private:
  bool resultAvailable() const;
  VPackArrayIterator getUpdateResultsIterator() const;
  VPackArrayIterator getInsertResultsIterator() const;

  OperationType updateReplaceCase(ModificationExecutorAccumulator& accu,
                                  AqlValue const& inDoc, AqlValue const& updateDoc);
  OperationType insertCase(ModificationExecutorAccumulator& accu, AqlValue const& insertDoc);

  ModificationExecutorInfos& _infos;
  std::vector<ModOp> _operations;
  ModificationExecutorAccumulator _insertAccumulator;
  ModificationExecutorAccumulator _updateAccumulator;

  OperationResult _updateResults;
  OperationResult _insertResults;

  arangodb::velocypack::Builder _keyDocBuilder;

  size_t const _batchSize;

  mutable std::mutex _resultStateMutex;
  ModificationExecutorResultState _resultState;
};

}  // namespace arangodb::aql
