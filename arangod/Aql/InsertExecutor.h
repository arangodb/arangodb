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

#ifndef ARANGOD_AQL_INSERT_EXECUTOR_H
#define ARANGOD_AQL_INSERT_EXECUTOR_H

#include "Aql/ModificationExecutor.h"

namespace arangodb {
namespace aql {

struct ModificationExecutorInfos;

template <typename FetcherType>
class InsertExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr bool allowsBlockPassthrough = false;
    static constexpr bool inputSizeRestrictsOutputSize =
        false;  // Disabled because prefetch does not work in the Cluster
    // Maybe This should ask for a 1:1 relation.
  };
  using Infos = ModificationExecutorInfos;
  using Fetcher = FetcherType;  // SingleBlockFetcher<Properties::allowsBlockPassthrough>;
  using Stats = ModificationStats;

  InsertExecutor(Fetcher&, Infos&);
  ~InsertExecutor();

  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

 private:
  ModificationExecutorInfos& _infos;
  Fetcher& _fetcher;
};

}  // namespace aql
}  // namespace arangodb
#endif
