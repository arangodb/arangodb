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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/QueryExpressionContext.h"

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {
class InputAqlItemRow;
class QueryContext;

// shared expression context base class for DocumentExpressionContext and
// LateMaterializedExpressionContext. it captures the variables that are used in
// both subclasses, but doesn't have any functionality of its own.
class DocumentProducingExpressionContext : public QueryExpressionContext {
 public:
  DocumentProducingExpressionContext(
      transaction::Methods& trx, QueryContext& query,
      aql::AqlFunctionsInternalCache& cache,
      std::vector<std::pair<VariableId, RegisterId>> const&
          filterVarsToRegister,
      InputAqlItemRow const& inputRow) noexcept
      : QueryExpressionContext(trx, query, cache),
        _filterVarsToRegister(filterVarsToRegister),
        _inputRow(inputRow) {}

  ~DocumentProducingExpressionContext() = default;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // only used for assertions
  virtual bool isLateMaterialized() const noexcept = 0;
#endif

 protected:
  std::vector<std::pair<VariableId, RegisterId>> const& _filterVarsToRegister;

  InputAqlItemRow const& _inputRow;
};
}  // namespace aql
}  // namespace arangodb
