////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Executor/ModificationExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Transaction/Methods.h"
#include "Utils/OperationResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <memory>
#include <optional>

namespace arangodb::aql {

struct AqlCall;
class AqlItemBlockInputRange;
class InputAqlItemRow;
class OutputAqlItemRow;
class RegisterInfos;
class FilterStats;
template<BlockPassthrough>
class SingleRowFetcher;

//
// ModificationExecutor is the "base" class for the Insert, Remove,
// UpdateReplace and Upsert executors.
//
// The fetcher and modification-specific code is spliced in via a template for
// performance reasons
//
// A ModifierType has to provide the function accumulate, which batches updates
// to be submitted to the transaction, a function transact which submits the
// currently accumulated batch of updates to the transaction, and an iterator to
// retrieve the results of the transaction.
//
// There currently are 4 modifier types: Insert, Remove, UpdateReplace, and
// Upsert. These are divided into three *simple* modifiers, Insert, Remove,
// UpdateReplace, and the special Upsert modifier.
//
// The Upsert modifier is a mix of Insert and UpdateReplace, and hence more
// complicated. In the future it should share as much code as possible with the
// Insert and UpdateReplace modifiers. This might enable the removal of the
// extra SimpleModifier layer described below.
//
// The simple modifiers are created by instantiating the SimpleModifier template
// class (defined in SimpleModifier.h) with a *completion*.
// The four completions for SimpleModifiers are defined in InsertModifier.h,
// RemoveModifier.h, and UpdateReplaceModifier.h
//
// The two types of modifiers (Simple and Upsert) follow a similar design. The
// main data is held in
//   * an operations vector which stores the type of operation
//     (APPLY_RETURN, IGNORE_RETURN, IGNORE_SKIP) that was performed by the
//     modifier. The Upsert modifier internally uses APPLY_UPDATE and
//     APPLY_INSERT to distinguish between the insert and UpdateReplace
//     branches.
//   * an accumulator that stores the documents that are committed in the
//     transaction.
//
// The class ModifierOutput is what is returned by the result iterator of a
// modifier, and represents the results of the modification operation on the
// input row.
// It contains the operation type, an InputAqlItemRow, and sometimes (if there
// have been results specific for the InputAqlItemRow) a VPackSlice containing
// the result of the transaction for this row.

// Note that ModifierOperationType is *subtly* different from
// ModifierOutput::Type.
// ModifierOperationType is used internally in the Modifiers (and the UPSERT
// modifier has its own ModifierOperationType that includes a case for Update
// and a case for Insert).
enum class ModifierOperationType {
  // Return result of operation if available (i.e. the query was not silent),
  // this in particular includes values OLD and NEW or errors specific to the
  // InputAqlItemRow under consideration
  ReturnIfAvailable,
  // Just copy the InputAqlItemRow to the OutputAqlItemRow. We do this if there
  // are no results available specific to this row (for example because the
  // query was
  // silent).
  CopyRow,
  // Skip the InputAqlItemRow entirely. This happens in case an error happens at
  // verification stage, for example when a key document does not contain a key
  // or
  // isn't a document.
  SkipRow
};

class ModifierOutput {
 public:
  enum class Type {
    // Return the row if requested by the query
    ReturnIfRequired,
    // Just copy the InputAqlItemRow to the OutputAqlItemRow
    CopyRow,
    // Skip the InputAqlItemRow entirely
    SkipRow
  };

  ModifierOutput() = delete;
  ModifierOutput(InputAqlItemRow const& inputRow, Type type);
  ModifierOutput(InputAqlItemRow&& inputRow, Type type);
  ModifierOutput(InputAqlItemRow const& inputRow, Type type,
                 AqlValue const& oldValue, AqlValue const& newValue);
  ModifierOutput(InputAqlItemRow&& inputRow, Type type,
                 AqlValue const& oldValue, AqlValue const& newValue);

  // No copying or copy assignment allowed of this class or any derived class
  ModifierOutput(ModifierOutput const&) = delete;
  ModifierOutput(ModifierOutput&& o) = delete;
  ModifierOutput& operator=(ModifierOutput&& o) = delete;

  ModifierOutput& operator=(ModifierOutput const&);

  InputAqlItemRow getInputRow() const;
  // If the output should be returned or skipped
  Type getType() const;
  bool hasOldValue() const;
  AqlValue const& getOldValue() const;
  bool hasNewValue() const;
  AqlValue const& getNewValue() const;

 protected:
  InputAqlItemRow const _inputRow;
  Type const _type;
  std::optional<AqlValue> _oldValue;
  std::optional<AqlValueGuard> _oldValueGuard;
  std::optional<AqlValue> _newValue;
  std::optional<AqlValueGuard> _newValueGuard;
};

template<typename FetcherType, typename ModifierType>
class ModificationExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };
  using Fetcher = FetcherType;
  using Infos = ModificationExecutorInfos;
  using Stats = ModificationStats;

  ModificationExecutor(FetcherType&, Infos&);
  ~ModificationExecutor();

  [[nodiscard]] auto produceRows(typename FetcherType::DataRange& input,
                                 OutputAqlItemRow& output)
      -> std::tuple<ExecutionState, Stats, AqlCall>;

  [[nodiscard]] auto skipRowsRange(typename FetcherType::DataRange& inputRange,
                                   AqlCall& call)
      -> std::tuple<ExecutionState, Stats, size_t, AqlCall>;

 protected:
  // Interface of the data structure that is passed by produceRows() and
  // skipRowsRange() to produceOrSkip(), which does the actual work.
  // The interface isn't technically necessary (as produceOrSkip is a template),
  // but I kept it for the overview it provides.
  struct IProduceOrSkipData {
    virtual ~IProduceOrSkipData() = default;
    virtual void doOutput() = 0;
    virtual auto maxOutputRows() -> std::size_t = 0;
    virtual auto needMoreOutput() -> bool = 0;
  };

  template<typename ProduceOrSkipData>
  std::tuple<ExecutionState, Stats, AqlCall> produceOrSkip(
      typename FetcherType::DataRange& input,
      ProduceOrSkipData& produceOrSkipData);

  transaction::Methods _trx;

  ModificationExecutorInfos& _infos;
  std::shared_ptr<ModifierType> _modifier;

  std::size_t _skipCount{};
  Stats _stats{};
};

template<typename FetcherType, typename ModifierType>
ModificationExecutor<FetcherType, ModifierType>::~ModificationExecutor() {
  // Clear all InputAqlItemRows the modifier still holds, and with it the
  // SharedAqlItemBlockPtrs. This is so the referenced AqlItemBlocks can be
  // returned to the AqlItemBlockManager now, while the latter still exists.
  _modifier->clearRows();
}

}  // namespace arangodb::aql
