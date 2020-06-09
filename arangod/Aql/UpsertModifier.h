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

#ifndef ARANGOD_AQL_UPSERT_MODIFIER_H
#define ARANGOD_AQL_UPSERT_MODIFIER_H

#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutorAccumulator.h"
#include "Aql/ModificationExecutorInfos.h"

#include "Aql/InsertModifier.h"
#include "Aql/UpdateReplaceModifier.h"

#include <velocypack/Builder.h>

namespace arangodb {
namespace aql {

struct ModificationExecutorInfos;

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
    OutputIterator begin() const;
    OutputIterator end() const;

   private:
    OutputIterator& next();

    UpsertModifier const& _modifier;
    std::vector<ModOp>::const_iterator _operationsIterator;
    VPackArrayIterator _insertResultsIterator;
    VPackArrayIterator _updateResultsIterator;
  };

 public:
  explicit UpsertModifier(ModificationExecutorInfos& infos)
      : _infos(infos),

        // Batch size has to be 1 so that the upsert modifier sees its own
        // writes.
        // This behaviour could be improved, if we can prove that an UPSERT
        // does not need to see its own writes
        _batchSize(1) {}

  ~UpsertModifier() = default;

  void reset();

  Result accumulate(InputAqlItemRow& row);
  Result transact(transaction::Methods& trx);

  size_t nrOfOperations() const;
  size_t nrOfDocuments() const;
  size_t nrOfResults() const;
  size_t nrOfErrors() const;
  size_t nrOfWritesExecuted() const;
  size_t nrOfWritesIgnored() const;

  size_t getBatchSize() const;

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
};

}  // namespace aql
}  // namespace arangodb
#endif
