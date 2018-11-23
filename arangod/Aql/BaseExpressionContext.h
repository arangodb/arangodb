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

#ifndef ARANGOD_AQL_BASE_EXPRESSION_CONTEXT_H
#define ARANGOD_AQL_BASE_EXPRESSION_CONTEXT_H 1

#include "QueryExpressionContext.h"

namespace arangodb {
namespace aql {
class AqlItemBlock;
class Query;

class BaseExpressionContext final : public QueryExpressionContext {
 public:
  BaseExpressionContext(Query* query,
                        size_t startPos, AqlItemBlock const* argv,
                        std::vector<Variable const*> const& vars,
                        std::vector<RegisterId> const& regs)
      : QueryExpressionContext(query),
        _startPos(startPos),
        _argv(argv),
        _vars(&vars),
        _regs(&regs) {}

  ~BaseExpressionContext() {}

  size_t numRegisters() const override {
    return _regs->size();
  }

  AqlValue const& getRegisterValue(size_t i) const override;

  Variable const* getVariable(size_t i) const override;

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override;
  
 private:
  /// @brief temporary storage for expression data context
  size_t _startPos;
  AqlItemBlock const* _argv;
  std::vector<Variable const*> const* _vars;
  std::vector<RegisterId> const* _regs;
};
}
}
#endif
