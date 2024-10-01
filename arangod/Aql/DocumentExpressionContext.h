////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Aql/DocumentProducingExpressionContext.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {
class InputAqlItemRow;
class QueryContext;
struct Variable;

// Expression context used when filtering on full documents or on index values
class DocumentExpressionContext : public DocumentProducingExpressionContext {
 public:
  DocumentExpressionContext(
      transaction::Methods& trx, QueryContext& query,
      aql::AqlFunctionsInternalCache& cache,
      std::vector<std::pair<VariableId, RegisterId>> const&
          filterVarsToRegister,
      InputAqlItemRow const& inputRow, Variable const* outputVariable) noexcept;

  ~DocumentExpressionContext() = default;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // only used for assertions
  bool isLateMaterialized() const noexcept final { return false; }
#endif

  void setCurrentDocument(arangodb::velocypack::Slice document) noexcept {
    _document = document;
  }

 protected:
  Variable const* _outputVariable;

  // brief temporary storage for expression data context (changing for
  // every input row)
  arangodb::velocypack::Slice _document;
};

// expression context which only has a single variable (the current document or
// index entry!)
class SimpleDocumentExpressionContext final : public DocumentExpressionContext {
 public:
  SimpleDocumentExpressionContext(
      transaction::Methods& trx, QueryContext& query,
      aql::AqlFunctionsInternalCache& cache,
      std::vector<std::pair<VariableId, RegisterId>> const&
          filterVarsToRegister,
      InputAqlItemRow const& inputRow, Variable const* outputVariable) noexcept;

  ~SimpleDocumentExpressionContext() = default;

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override;
};

// expression context which has multiple variables available
class GenericDocumentExpressionContext final
    : public DocumentExpressionContext {
 public:
  GenericDocumentExpressionContext(
      transaction::Methods& trx, QueryContext& query,
      aql::AqlFunctionsInternalCache& cache,
      std::vector<std::pair<VariableId, RegisterId>> const&
          filterVarsToRegister,
      InputAqlItemRow const& inputRow, Variable const* outputVariable) noexcept;

  ~GenericDocumentExpressionContext() = default;

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override;
};

}  // namespace aql
}  // namespace arangodb
