////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_DOCUMENT_INDEX_EXPRESSION_CONTEXT_H
#define ARANGOD_AQL_DOCUMENT_INDEX_EXPRESSION_CONTEXT_H 1

#include "Aql/QueryExpressionContext.h"

namespace arangodb {
namespace aql {
class QueryContext;

class DocumentIndexExpressionContext final : public QueryExpressionContext {
 public:
  DocumentIndexExpressionContext(transaction::Methods& trx,
                                 QueryContext& query,
                                 AqlFunctionsInternalCache& cache,
                                 AqlValue (*getValue)(void const* ctx, Variable const* var, bool doCopy),
                                 void const* ctx);

  ~DocumentIndexExpressionContext() = default;

  /// true if the variable we are referring to is set by
  /// a collection enumeration/index enumeration
  virtual bool isDataFromCollection(Variable const* variable) const override {
    return false;
  }

  virtual AqlValue getVariableValue(Variable const* variable, bool doCopy,
                                    bool& mustDestroy) const override;

 private:
  /// @brief returns var's aql value
  AqlValue (*_getValue)(void const* ctx, Variable const* var, bool doCopy);

  /// @brief context for returning var's aql value
  void const* _ctx;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_DOCUMENT_INDEX_EXPRESSION_CONTEXT_H
