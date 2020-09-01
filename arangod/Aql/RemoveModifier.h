////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_REMOVE_MODIFIER_H
#define ARANGOD_AQL_REMOVE_MODIFIER_H

#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutorAccumulator.h"
#include "Aql/ModificationExecutorInfos.h"

#include <velocypack/Builder.h>

namespace arangodb {
namespace aql {

struct ModificationExecutorInfos;

class RemoveModifierCompletion {
 public:
  explicit RemoveModifierCompletion(ModificationExecutorInfos& infos) : _infos(infos) {}
  ~RemoveModifierCompletion() = default;

  ModifierOperationType accumulate(ModificationExecutorAccumulator& accu,
                                   InputAqlItemRow& row);
  OperationResult transact(transaction::Methods& trx, VPackSlice const& data);

 private:
  ModificationExecutorInfos& _infos;
  arangodb::velocypack::Builder _keyDocBuilder;
};

}  // namespace aql
}  // namespace arangodb
#endif
