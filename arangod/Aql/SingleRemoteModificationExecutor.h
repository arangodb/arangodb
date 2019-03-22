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

struct SingleRemoteModificationInfos : ModificationExecutorInfos {
  SingleRemoteModificationInfos(
      boost::optional<RegisterId> input1RegisterId, boost::optional<RegisterId> input2RegisterId,
      boost::optional<RegisterId> input3RegisterId, boost::optional<RegisterId> outputNewRegisterId,
      boost::optional<RegisterId> outputOldRegisterId,
      boost::optional<RegisterId> outputRegisterId, RegisterId nrInputRegisters,
      RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
      std::unordered_set<RegisterId> registersToKeep, transaction::Methods* trx,
      OperationOptions options, aql::Collection const* aqlCollection,
      ProducesResults producesResults, ConsultAqlWriteFilter consultAqlWriteFilter,
      IgnoreErrors ignoreErrors, DoCount doCount, IsReplace isReplace,
      IgnoreDocumentNotFound ignoreDocumentNotFound,  // end of base class params
      std::string key,
      bool hasParent, bool replaceIndex)
      : ModificationExecutorInfos(std::move(input1RegisterId), std::move(input2RegisterId), std::move(input3RegisterId),
                                  std::move(outputNewRegisterId), std::move(outputOldRegisterId), std::move(outputRegisterId),
                                  nrInputRegisters, std::move(nrOutputRegisters), std::move(registersToClear),
                                  std::move(registersToKeep), trx, std::move(options), aqlCollection,
                                  producesResults, consultAqlWriteFilter, ignoreErrors,
                                  doCount, isReplace, ignoreDocumentNotFound),
        _key(std::move(key)), _hasParent(hasParent), _replaceIndex(replaceIndex) {


        }
  std::string _key;
  bool _hasParent;  //node->hasParent();
  bool _replaceIndex;
  constexpr static double const defaultTimeOut = 3600.0;
};

struct IndexTag {};

template <typename Modifier>
struct SingleRemoteModificationExecutor {
  struct Properties {
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = false;
  };
  using Infos = SingleRemoteModificationInfos;
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Stats = SingleRemoteModificationStats;
  using Modification = Modifier;

  SingleRemoteModificationExecutor(Fetcher&, Infos&);
  ~SingleRemoteModificationExecutor() = default;

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

 protected:
  bool doSingleRemoteModificationOperation(InputAqlItemRow&, OutputAqlItemRow&, Stats&);

  Infos& _info;
  Fetcher& _fetcher;
  ExecutionState _upstreamState;
};

}  // namespace aql
}  // namespace arangodb

#endif
