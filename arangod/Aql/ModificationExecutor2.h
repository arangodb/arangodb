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

namespace ModificationExecutorHelpers {
Result getKeyAndRevision(CollectionNameResolver const& resolver,
                         AqlValue const& value, std::string& key,
                         std::string& rev, bool ignoreRevision = false);
Result buildKeyDocument(VPackBuilder& builder, std::string const& key,
                        std::string const& rev, bool ignoreRevision = false);
};  // namespace ModificationExecutorHelpers

//
// ModificationExecutor2 is the base class for the Insert, Remove, Update,
// Replace and Upsert executors.
//
// The modification-specific code is spliced in via a template for performance
// reasons (TODO: verify that this is true)
//

using ModifierOutput = std::tuple<ModOperationType, InputAqlItemRow, VPackSlice>;

template <typename FetcherType, typename ModifierType>
class ModificationExecutor2 {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr bool allowsBlockPassthrough = false;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Infos = ModificationExecutorInfos;
  using Fetcher = FetcherType;
  using Stats = ModificationStats;

  ModificationExecutor2(Fetcher&, Infos&);
  ~ModificationExecutor2();

  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

 protected:
  std::pair<ExecutionState, Stats> doCollect(size_t const maxOutputs);
  void doOutput(OutputAqlItemRow& output);

  ModificationExecutorInfos& _infos;
  Fetcher& _fetcher;
  ModifierType _modifier;
};

}  // namespace aql
}  // namespace arangodb
#endif
