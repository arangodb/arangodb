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

#ifndef ARANGOD_AQL_QUERY_EXPRESSION_CONTEXT_H
#define ARANGOD_AQL_QUERY_EXPRESSION_CONTEXT_H 1

#include "ExpressionContext.h"

namespace arangodb {
namespace aql {
class Query;

class QueryExpressionContext : public ExpressionContext {
 public:
  explicit QueryExpressionContext(Query* query)
      : ExpressionContext(),
        _query(query) {}

  void registerWarning(int errorCode, char const* msg) override;
  void registerError(int errorCode, char const* msg) override;
  
  icu::RegexMatcher* buildRegexMatcher(char const* ptr, size_t length, bool caseInsensitive) override;
  icu::RegexMatcher* buildLikeMatcher(char const* ptr, size_t length, bool caseInsensitive) override;
  icu::RegexMatcher* buildSplitMatcher(AqlValue splitExpression, transaction::Methods*, bool& isEmptyExpression) override;

  bool killed() const override final;
  TRI_vocbase_t& vocbase() const override final;
  Query* query() const override final;

 private:
  Query* _query;
};
}
}
#endif
