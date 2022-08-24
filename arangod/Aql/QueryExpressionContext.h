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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ExpressionContext.h"
#include "Aql/AqlValue.h"
#include "Basics/ErrorCode.h"
#include "Containers/FlatHashMap.h"

#include <velocypack/Slice.h>

namespace arangodb {
struct ValidatorBase;
namespace aql {
class QueryContext;
class AqlFunctionsInternalCache;

class QueryExpressionContext : public aql::ExpressionContext {
 public:
  QueryExpressionContext(QueryExpressionContext const&) = delete;
  QueryExpressionContext& operator=(QueryExpressionContext const&) = delete;

  explicit QueryExpressionContext(transaction::Methods& trx,
                                  QueryContext& query,
                                  AqlFunctionsInternalCache& cache) noexcept
      : ExpressionContext(),
        _trx(trx),
        _query(query),
        _aqlFunctionsInternalCache(cache) {}

  void registerWarning(ErrorCode errorCode, char const* msg) override final;
  void registerError(ErrorCode errorCode, char const* msg) override final;

  icu::RegexMatcher* buildRegexMatcher(char const* ptr, size_t length,
                                       bool caseInsensitive) override final;
  icu::RegexMatcher* buildLikeMatcher(char const* ptr, size_t length,
                                      bool caseInsensitive) override final;
  icu::RegexMatcher* buildSplitMatcher(AqlValue splitExpression,
                                       velocypack::Options const* opts,
                                       bool& isEmptyExpression) override final;

  arangodb::ValidatorBase* buildValidator(
      arangodb::velocypack::Slice const&) override final;

  TRI_vocbase_t& vocbase() const override final;
  // may be inaccessible on some platforms
  transaction::Methods& trx() const override final;
  bool killed() const override final;
  QueryContext& query();

  // register a temporary variable in the ExpressionContext. the
  // slice used here is not owned by the QueryExpressionContext!
  // the caller has to make sure the data behind the slice remains
  // valid until clearVariable() is called or the context is discarded.
  void setVariable(Variable const* variable,
                   arangodb::velocypack::Slice value) override;

  // unregister a temporary variable from the ExpressionContext.
  void clearVariable(Variable const* variable) noexcept override;

 protected:
  // return temporary variable if set, otherwise call lambda for
  // retrieving variable value
  template<typename F>
  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy, F&& fn) const {
    if (!_variables.empty()) {
      auto it = _variables.find(variable);

      if (it != _variables.end()) {
        // copy the slice we found
        mustDestroy = true;
        return AqlValue((*it).second);
      }
    }
    return std::invoke(std::forward<F>(fn), variable, doCopy, mustDestroy);
  }

 private:
  transaction::Methods& _trx;
  QueryContext& _query;
  AqlFunctionsInternalCache& _aqlFunctionsInternalCache;

  // variables only temporarily valid during execution. Slices stored
  // here are not owned by the QueryExpressionContext!
  containers::FlatHashMap<Variable const*, arangodb::velocypack::Slice>
      _variables;
};
}  // namespace aql
}  // namespace arangodb
