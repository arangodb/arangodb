////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlValue.h"
#include "Aql/QueryExpressionContext.h"

#include <unordered_map>

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace aql {
class AqlItemBlock;

class FixedVarExpressionContext final : public QueryExpressionContext {
 public:
  explicit FixedVarExpressionContext(transaction::Methods& trx, QueryContext& query,
                                     AqlFunctionsInternalCache& cache);

  ~FixedVarExpressionContext() override = default;

  bool isDataFromCollection(Variable const* variable) const override;

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override;

  void clearVariableValues() noexcept;

  // @brief This method will set the given variable to the given AQL value
  // if the variable already holds a value, this method will keep the old value.
  void setVariableValue(Variable const*, AqlValue const&);

  // @brief This method will only clear the given variable, and keep
  // all others intact. If the variable does not exist, this is a noop.
  void clearVariableValue(Variable const*);

  void serializeAllVariables(velocypack::Options const& opts,
                             arangodb::velocypack::Builder&) const;

 private:
  /// @brief temporary storage for expression data context
  std::unordered_map<Variable const*, AqlValue> _vars;
};

class SingleVarExpressionContext final : public QueryExpressionContext {
 public:
  explicit SingleVarExpressionContext(transaction::Methods& trx, QueryContext& query,
                                      AqlFunctionsInternalCache& cache);

  explicit SingleVarExpressionContext(transaction::Methods& trx, QueryContext& query,
                                      AqlFunctionsInternalCache& cache,
                                      Variable* var, AqlValue val);

  ~SingleVarExpressionContext() override;

  bool isDataFromCollection(Variable const* variable) const override;

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override;

  void setVariableValue(Variable* var, AqlValue& val);

 private:
  const Variable* _variable;
  AqlValue _value;
};

}  // namespace aql
}  // namespace arangodb
