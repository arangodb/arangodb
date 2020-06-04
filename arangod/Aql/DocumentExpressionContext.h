////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_DOCUMENT_EXPRESSION_CONTEXT_H
#define ARANGOD_AQL_DOCUMENT_EXPRESSION_CONTEXT_H 1

#include "Aql/QueryExpressionContext.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace aql {

class DocumentExpressionContext final : public QueryExpressionContext {
 public:
  DocumentExpressionContext(transaction::Methods& trx, QueryContext& query,
                            AqlFunctionsInternalCache& cache, arangodb::velocypack::Slice document);

  ~DocumentExpressionContext() = default;

  bool isDataFromCollection(Variable const* variable) const override {
    return true;
  }

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override;

 private:
  /// @brief temporary storage for expression data context
  arangodb::velocypack::Slice _document;
};
}  // namespace aql
}  // namespace arangodb
#endif
