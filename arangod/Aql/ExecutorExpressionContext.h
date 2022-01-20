////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Aql/InputAqlItemRow.h"
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
  ExecutorExpressionContext(
      transaction::Methods& trx, QueryContext& context,
      AqlFunctionsInternalCache& cache, InputAqlItemRow const& inputRow,
      std::vector<std::pair<VariableId, RegisterId>> const& varsToRegister);

  ~ExecutorExpressionContext() override = default;

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override;

 private:
  /// @brief temporary storage for expression data context
  InputAqlItemRow const& _inputRow;
  std::vector<std::pair<VariableId, RegisterId>> const& _varsToRegister;
};

}  // namespace aql
}  // namespace arangodb
