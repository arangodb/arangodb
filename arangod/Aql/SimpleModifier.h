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

template <typename ModifierCompletion>
class SimpleModifier {
  friend class InsertModifierCompletion;
  friend class UpdateModifierCompletion;
  friend class ReplaceModifierCompletion;
  friend class RemoveModifierCompletion;

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

  // TODO: Make this a real iterator
  Result setupIterator();
  bool isFinishedIterator();
  ModifierOutput getOutput();
  void advanceIterator();

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
