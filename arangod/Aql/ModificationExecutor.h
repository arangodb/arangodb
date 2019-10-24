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

#ifndef ARANGOD_AQL_MODIFICATION_EXECUTOR_H
#define ARANGOD_AQL_MODIFICATION_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ModificationExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Utils/OperationResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace aql {
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
// The for completions for SimpleModifiers are defined in InsertModifier.h,
// RemoveModifier.h, and UpdateReplaceModifier.h
//
// The FetcherType has to provide the function fetchRow with the parameter atMost
//
// The two types of modifiers (Simple and Upsert) follow a similar design. The main
// data is held in
//   * an operations vector which stores the type of operation
//     (APPLY_RETURN, IGNORE_RETURN, IGNORE_SKIP) that was performed by the
//     modifier. The Upsert modifier internally uses APPLY_UPDATE and APPLY_INSERT
//     to distinguish between the insert and UpdateReplace branches.
//   * an accumulator that stores the documents that are committed in the
//     transaction.
//
enum class ModOperationType : uint8_t {
  // do not apply, do not produce a result - used for skipping over suppressed
  // errors
  IGNORE_SKIP = 0,
  // do not apply, but pass the row to the next block - used for smart graphs
  // and such
  IGNORE_RETURN = 1,
  // apply it and return the result
  APPLY_RETURN = 2,
  // TODO: These next two should be moved into the upsert modifier, as they
  //       are internal to it
  // apply it and return the result, used only used *internally* in the UPSERT
  // modifier
  APPLY_UPDATE = 3,
  // apply it and return the result, used only used *internally* in the UPSERT
  // modifier
  APPLY_INSERT = 4,
};

// The tuple ModifierOutput is what is returned by the result iterator of a
// modifier, and represents the results of the modification operation on the
// input row.
// The first component has to be the operation type, the second an
// InputAqlItemRow, and the third is a VPackSlice containing the result of the
// transaction for this row.
// using ModifierOutput = std::tuple<ModOperationType, InputAqlItemRow, VPackSlice>;

class ModifierOutput {
 public:
  ModifierOutput() = delete;
  ModifierOutput(InputAqlItemRow const inputRow, bool const error);
  ModifierOutput(InputAqlItemRow const inputRow, bool const error,
                 std::unique_ptr<AqlValue>&& oldValue, std::unique_ptr<AqlValue>&& newValue);

  ModifierOutput(ModifierOutput&& o);
  ModifierOutput& operator=(ModifierOutput&& o);

  InputAqlItemRow getInputRow() const;
  bool isError() const;
  bool hasOldValue() const;
  AqlValue&& getOldValue() const;
  bool hasNewValue() const;
  AqlValue&& getNewValue() const;

 private:
  // No copying or copy assignment allowed of this class or any derived class
  ModifierOutput(ModifierOutput const&);
  ModifierOutput& operator=(ModifierOutput const&);

  InputAqlItemRow const _inputRow;
  bool const _error;
  std::unique_ptr<AqlValue> _oldValue;
  std::unique_ptr<AqlValue> _newValue;
};

template <typename FetcherType, typename ModifierType>
class ModificationExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = FetcherType;
  using Infos = ModificationExecutorInfos;
  using Stats = ModificationStats;

  ModificationExecutor(FetcherType&, Infos&);
  ~ModificationExecutor();

  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

 protected:
  std::pair<ExecutionState, Stats> doCollect(size_t const maxOutputs);
  void doOutput(OutputAqlItemRow& output, Stats& stats);

  // The state that was returned on the last call to produceRows. For us
  // this is relevant because we might have collected some documents in the
  // modifier's accumulator, but not written them yet, because we ran into
  // WAITING
  ExecutionState _lastState;
  ModificationExecutorInfos& _infos;
  FetcherType& _fetcher;
  ModifierType _modifier;
};

}  // namespace aql
}  // namespace arangodb
#endif
