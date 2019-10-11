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

#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutor2.h"
#include "Aql/ModificationExecutorTraits.h"

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
template <typename ModifierCompletion>
class SimpleModifier {
  friend class InsertModifierCompletion;
  friend class RemoveModifierCompletion;
  friend class ReplaceModifierCompletion;
  friend class UpdateModifierCompletion;

 public:
  using ModOp = std::pair<ModOperationType, InputAqlItemRow>;

 public:
  SimpleModifier(ModificationExecutorInfos& infos);
  ~SimpleModifier();

  void reset();
  void close();

  Result accumulate(InputAqlItemRow& row);
  Result transact();

  size_t size() const;

  void throwTransactErrors();

  // TODO: Make this a real iterator
  Result setupIterator();
  bool isFinishedIterator();
  ModifierOutput getOutput();
  void advanceIterator();

  // For SmartGraphs in the Enterprise Edition this is used to skip write
  // operations because they would otherwise be executed twice.
  //
  // In the community edition this will always return true
  bool writeRequired(VPackSlice const& doc, std::string const& key) const;

  // We need to have a function that adds a document because returning a
  // (reference to a) slice has scoping problems
  void addDocument(VPackSlice const& doc);

  ModificationExecutorInfos& getInfos();

 private:
  ModificationExecutorInfos& _infos;
  ModifierCompletion const& _completion;

  std::vector<ModOp> _operations;
  VPackBuilder _accumulator;

  OperationResult _results;

  std::vector<ModOp>::const_iterator _operationsIterator;
  VPackArrayIterator _resultsIterator;
};

}  // namespace aql
}  // namespace arangodb
#endif
