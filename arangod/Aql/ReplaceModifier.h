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

#ifndef ARANGOD_AQL_REPLACE_MODIFIER_H
#define ARANGOD_AQL_REPLACE_MODIFIER_H

#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutorTraits.h"

namespace arangodb {
namespace aql {

struct ModificationExecutorInfos;

class ReplaceModifier {
 public:
  using OutputTuple = std::tuple<ModOperationType, InputAqlItemRow, VPackSlice>;
  using ModOp = std::pair<ModOperationType, InputAqlItemRow>;

 public:
  ReplaceModifier(ModificationExecutorInfos& infos);
  ~ReplaceModifier();

  void reset();
  void close();

  Result accumulate(InputAqlItemRow& row);

  OperationResult transact();

  size_t size() const;

  // TODO: Make this a real iterator
  Result setupIterator();
  bool isFinishedIterator();
  OutputTuple getOutput();
  void advanceIterator();

 private:
  ModificationExecutorInfos& _infos;
  std::vector<ModOp> _operations;
  VPackBuilder _accumulator;

  VPackSlice _operationResults;

  std::vector<ModOp>::const_iterator _operationsIterator;
  std::unique_ptr<VPackArrayIterator> _resultsIterator;
};

}  // namespace aql
}  // namespace arangodb
#endif
