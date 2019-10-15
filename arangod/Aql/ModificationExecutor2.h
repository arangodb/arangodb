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

#ifndef ARANGOD_AQL_MODIFICATION_EXECUTOR2_H
#define ARANGOD_AQL_MODIFICATION_EXECUTOR2_H

#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutorTraits.h"

namespace arangodb {
namespace aql {

struct ModificationExecutorInfos;

// TODO: Find a better home for these
namespace ModificationExecutorHelpers {
Result getKeyAndRevision(CollectionNameResolver const& resolver,
                         AqlValue const& value, std::string& key,
                         std::string& rev, bool ignoreRevision = false);
Result buildKeyDocument(VPackBuilder& builder, std::string const& key,
                        std::string const& rev, bool ignoreRevision = false);
bool writeRequired(ModificationExecutorInfos& infos, VPackSlice const& doc,
                   std::string const& key);
};  // namespace ModificationExecutorHelpers

//
// ModificationExecutor2 is the "base" class for the Insert, Remove, Update,
// Replace and Upsert executors.
//
// The fetcher and modification-specific code is spliced in via a template for
// performance reasons (TODO: verify that this is true).
//
// A ModifierType has to provide the function accumulate which batches updates
// to be submitted to the transaction, a function transact which submits the
// currently accumulated batch of updates to the transaction, and an iterator to
// retrieve the results of the transaction.
//
// The currently 5 modifier types are divided into the *simple* modifiers
// Insert, Remove, Update, and Replace, and the Upsert modifier, which is a mix
// of Insert and Update/Replace, and hence more complicated. (TODO: Is there any
// way in which we can redesign the execution of AQL queries that we can just
// transform an upsert into an update and an insert and get rid of the upsert
// executor?)
//
// The simple modifiers are created by instantiating the SimpleModifier template
// class (defined in SimpleModifier.h) with a *completion*.
// The for completions for SimpleModifiers are defined in InsertModifier.h,
// RemoveModifier.h, UpdateModifier.h, and ReplaceModifier.h
//
// The only difference between the Update and the Replace modifier is the
// transaction method called.
//
// The FetcherType has to provide the function fetchRow
//
using ModifierOutput = std::tuple<ModOperationType, InputAqlItemRow, VPackSlice>;

enum class ModifierIteratorMode { OperationsOnly, Full };
std::ostream& operator<<(std::ostream& ostream, ModifierIteratorMode mode);

template <typename FetcherType, typename ModifierType>
class ModificationExecutor2 {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = FetcherType;
  using Infos = ModificationExecutorInfos;
  using Stats = ModificationStats;

  ModificationExecutor2(FetcherType&, Infos&);
  ~ModificationExecutor2();

  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

 protected:
  std::pair<ExecutionState, Stats> doCollect(size_t const maxOutputs);
  void doOutput(OutputAqlItemRow& output, Stats& stats);

  ModificationExecutorInfos& _infos;
  FetcherType& _fetcher;
  ModifierType _modifier;
};

}  // namespace aql
}  // namespace arangodb
#endif
