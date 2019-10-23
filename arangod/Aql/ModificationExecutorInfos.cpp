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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "ModificationExecutorInfos.h"

#include "Aql/RegisterPlan.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {
std::shared_ptr<std::unordered_set<RegisterId>> makeSet(std::initializer_list<RegisterId> regList) {
  auto rv = make_shared_unordered_set();
  for (RegisterId regId : regList) {
    if (regId < RegisterPlan::MaxRegisterId) {
      rv->insert(regId);
    }
  }
  return rv;
}

}  // namespace

namespace arangodb {
namespace aql {

ModificationExecutorInfos::ModificationExecutorInfos(
    RegisterId input1RegisterId, RegisterId input2RegisterId, RegisterId input3RegisterId,
    RegisterId outputNewRegisterId, RegisterId outputOldRegisterId,
    RegisterId outputRegisterId, RegisterId nrInputRegisters,
    RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep, transaction::Methods* trx,
    OperationOptions options, aql::Collection const* aqlCollection,
    ProducesResults producesResults, ConsultAqlWriteFilter consultAqlWriteFilter,
    IgnoreErrors ignoreErrors, DoCount doCount, IsReplace isReplace,
    IgnoreDocumentNotFound ignoreDocumentNotFound)
    : ExecutorInfos(makeSet({input1RegisterId, input2RegisterId, input3RegisterId}) /*input registers*/,
                    makeSet({outputOldRegisterId, outputNewRegisterId, outputRegisterId}) /*output registers*/,
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _trx(trx),
      _options(options),
      _aqlCollection(aqlCollection),
      _producesResults(ProducesResults(producesResults._value || !_options.silent)),
      _consultAqlWriteFilter(consultAqlWriteFilter),
      _ignoreErrors(ignoreErrors),
      _doCount(doCount),

      _isReplace(isReplace),
      _ignoreDocumentNotFound(ignoreDocumentNotFound),
      _input1RegisterId(input1RegisterId),
      _input2RegisterId(input2RegisterId),
      _input3RegisterId(input3RegisterId),
      _outputNewRegisterId(outputNewRegisterId),
      _outputOldRegisterId(outputOldRegisterId),
      _outputRegisterId(outputRegisterId) {}

}  // namespace aql
}  // namespace arangodb
