////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/IndexNode.h"
#include "Aql/DocumentProducingExpressionContext.h"
#include "Indexes/IndexIterator.h"

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {
class QueryContext;
class InputAqlItemRow;
struct Variable;

// Expression context used when filtering on index values using late
// materialization
class LateMaterializedExpressionContext final
    : public DocumentProducingExpressionContext {
 public:
  LateMaterializedExpressionContext(
      transaction::Methods& trx, QueryContext& query,
      aql::AqlFunctionsInternalCache& cache,
      std::vector<std::pair<VariableId, RegisterId>> const&
          filterVarsToRegister,
      InputAqlItemRow const& inputRow,
      IndexNode::IndexValuesVars const& outNonMaterializedIndVars) noexcept;

  ~LateMaterializedExpressionContext() = default;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // only used for assertions
  bool isLateMaterialized() const noexcept override { return true; }
#endif

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override;

  void setCurrentCoveringData(
      IndexIteratorCoveringData const* covering) noexcept {
    _covering = covering;
  }

 private:
  IndexNode::IndexValuesVars const& _outNonMaterializedIndVars;

  // pointer to current expression data context (changing for
  // every input row)
  IndexIteratorCoveringData const* _covering;
};

}  // namespace aql
}  // namespace arangodb
