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
#include "Aql/QueryContext.h"
#include "Cluster/ServerState.h"

using namespace arangodb;
using namespace arangodb::aql;

ModificationExecutorInfos::ModificationExecutorInfos(
    RegisterId input1RegisterId, RegisterId input2RegisterId, RegisterId input3RegisterId,
    RegisterId outputNewRegisterId, RegisterId outputOldRegisterId,
    RegisterId outputRegisterId, arangodb::aql::QueryContext& query, OperationOptions options,
    aql::Collection const* aqlCollection, ProducesResults producesResults,
    ConsultAqlWriteFilter consultAqlWriteFilter, IgnoreErrors ignoreErrors,
    DoCount doCount, IsReplace isReplace, IgnoreDocumentNotFound ignoreDocumentNotFound)
    : _query(query),
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
      _outputRegisterId(outputRegisterId) {
  // If we're running on a DBServer in a cluster some modification operations legitimately
  // fail due to the affected document not being available (which is reflected in _ignoreDocumentNotFound).
  // This makes sure that results are reported back from a DBServer.
  auto isDBServer = ServerState::instance()->isDBServer();
  _producesResults = ProducesResults(_producesResults || !_options.silent ||
                                     (isDBServer && _ignoreDocumentNotFound));
}
