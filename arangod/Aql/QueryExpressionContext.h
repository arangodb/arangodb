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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_QUERY_EXPRESSION_CONTEXT_H
#define ARANGOD_AQL_QUERY_EXPRESSION_CONTEXT_H 1

#include "ExpressionContext.h"

namespace arangodb {
struct ValidatorBase;
namespace aql {
class QueryContext;
class AqlFunctionsInternalCache;

class QueryExpressionContext : public ExpressionContext {
 public:
  explicit QueryExpressionContext(transaction::Methods& trx,
                                  QueryContext& query,
                                  AqlFunctionsInternalCache& cache) noexcept
      : ExpressionContext(),
        _trx(trx), _query(query), _aqlFunctionsInternalCache(cache) {}

  void registerWarning(int errorCode, char const* msg) override final;
  void registerError(int errorCode, char const* msg) override final;

  icu::RegexMatcher* buildRegexMatcher(char const* ptr, size_t length,
                                       bool caseInsensitive) override final;
  icu::RegexMatcher* buildLikeMatcher(char const* ptr, size_t length,
                                      bool caseInsensitive) override final;
  icu::RegexMatcher* buildSplitMatcher(AqlValue splitExpression, velocypack::Options const* opts,
                                       bool& isEmptyExpression) override final;

  arangodb::ValidatorBase* buildValidator(arangodb::velocypack::Slice const&) override final;

  TRI_vocbase_t& vocbase() const override final;
  /// may be inaccessible on some platforms
  transaction::Methods& trx() const override final;
  bool killed() const override final;

 private:
  transaction::Methods& _trx;
  QueryContext& _query;
  AqlFunctionsInternalCache& _aqlFunctionsInternalCache;
};
}  // namespace aql
}  // namespace arangodb
#endif
