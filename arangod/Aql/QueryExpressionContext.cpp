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

#include "QueryExpressionContext.h"
#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Aql/RegexCache.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;

void QueryExpressionContext::registerWarning(int errorCode, char const* msg) {
  _query->registerWarning(errorCode, msg);
}

void QueryExpressionContext::registerError(int errorCode, char const* msg) {
  _query->registerError(errorCode, msg);
}
  
icu::RegexMatcher* QueryExpressionContext::buildRegexMatcher(char const* ptr, size_t length, bool caseInsensitive) {
  return _query->regexCache()->buildRegexMatcher(ptr, length, caseInsensitive);
}

icu::RegexMatcher* QueryExpressionContext::buildLikeMatcher(char const* ptr, size_t length, bool caseInsensitive) {
  return _query->regexCache()->buildLikeMatcher(ptr, length, caseInsensitive);
}

icu::RegexMatcher* QueryExpressionContext::buildSplitMatcher(AqlValue splitExpression, transaction::Methods* trx, bool& isEmptyExpression) {
  return _query->regexCache()->buildSplitMatcher(splitExpression, trx, isEmptyExpression);
}

bool QueryExpressionContext::killed() const {
  return _query->killed();
}

TRI_vocbase_t& QueryExpressionContext::vocbase() const {
  return _query->vocbase();
}

Query* QueryExpressionContext::query() const {
  return _query;
}
