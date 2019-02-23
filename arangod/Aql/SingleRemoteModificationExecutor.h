////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SINGLE_REMOTE_MODIFICATION_EXECUTOR_H
#define ARANGOD_AQL_SINGLE_REMOTE_MODIFICATION_EXECUTOR_H 1

#include "Aql/ModificationExecutor.h"

namespace arangodb {
namespace aql {

struct Index {};

template <typename Modifier>
struct SingleRemoteModificationExecutor {
  struct Properties {
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = false;
  };
  using Infos = ModificationExecutorInfos;
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Stats = ModificationStats;
  using Modification = Modifier;

  SingleRemoteModificationExecutor(Fetcher&, Infos&);
  ~SingleRemoteModificationExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

 protected:
  bool doSingleRemoteModificationOperation(InputAqlItemRow&, OutputAqlItemRow&, Stats&);

  ModificationExecutorInfos& _info;
  Fetcher& _fetcher;
  ExecutionState _upstreamState;
  std::string _key;
};


}  // namespace aql
}  // namespace arangodb

#endif
