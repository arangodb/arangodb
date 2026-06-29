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
/// @author Simran Spiller
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExpressionContext.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "Containers/FlatHashMap.h"

namespace arangodb {
namespace aql {
class QueryContext;
}
namespace transaction {
class Methods;
}

namespace import {

/// @brief ExpressionContext implementation for arangoimport's --custom-query
/// feature. Modeled after ComputedValuesExpressionContext in
/// arangod/VocBase/ComputedValues.h. Stores variable bindings for @doc and
/// LET variables, delegates function evaluation to the AQL function cache.
class ImportExpressionContext final : public aql::ExpressionContext {
 public:
  explicit ImportExpressionContext(transaction::Methods& trx,
                                   aql::QueryContext& query);

  void registerWarning(ErrorCode errorCode, std::string_view msg) override;
  void registerError(ErrorCode errorCode, std::string_view msg) override;

  icu_64_64::RegexMatcher* buildRegexMatcher(std::string_view expr,
                                             bool caseInsensitive) override;
  icu_64_64::RegexMatcher* buildLikeMatcher(std::string_view expr,
                                            bool caseInsensitive) override;
  icu_64_64::RegexMatcher* buildSplitMatcher(aql::AqlValue splitExpression,
                                             velocypack::Options const* opts,
                                             bool& isEmptyExpression) override;
  ValidatorBase* buildValidator(velocypack::Slice params) override;

  TRI_vocbase_t& vocbase() const override;
  transaction::Methods& trx() const override;
  bool killed() const override { return false; }

  aql::AqlValue getVariableValue(aql::Variable const* variable, bool doCopy,
                                 bool& mustDestroy) const override;

  void setVariable(aql::Variable const* variable,
                   velocypack::Slice value) override;
  void clearVariable(aql::Variable const* variable) noexcept override;

  /// @brief clear all variable bindings (used between document transforms)
  void clearAllVariables() noexcept;

 private:
  transaction::Methods& _trx;
  aql::QueryContext& _query;
  aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;
  containers::FlatHashMap<aql::Variable const*, velocypack::Slice> _variables;
};

}  // namespace import
}  // namespace arangodb
