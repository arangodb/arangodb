////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EXECUTOR_EXPRESSION_CONTEXT_H
#define ARANGOD_AQL_EXECUTOR_EXPRESSION_CONTEXT_H 1

#include "Aql/QueryExpressionContext.h"
#include "Aql/Variable.h"
#include "Aql/types.h"

#include <vector>

namespace arangodb {
namespace aql {
class AqlItemBlock;
class Query;
class InputAqlItemRow;

class ExecutorExpressionContext final : public QueryExpressionContext {
 public:
  ExecutorExpressionContext(transaction::Methods& trx,
                            QueryContext& context,
                            AqlFunctionsInternalCache& cache, InputAqlItemRow const& inputRow,
                            std::vector<Variable const*> const& vars,
                            std::vector<RegisterId> const& regs);

  ~ExecutorExpressionContext() override = default;

  bool isDataFromCollection(Variable const* variable) const override {
    return variable->isDataFromCollection;
  }

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override;

 private:
  /// @brief temporary storage for expression data context
  InputAqlItemRow const& _inputRow;
  std::vector<Variable const*> const& _vars;
  std::vector<RegisterId> const& _regs;
};
}  // namespace aql
}  // namespace arangodb
#endif
