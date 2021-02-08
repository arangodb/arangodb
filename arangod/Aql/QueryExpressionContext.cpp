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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "QueryExpressionContext.h"

#include "Aql/AqlValue.h"
#include "Aql/QueryContext.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;

void QueryExpressionContext::registerWarning(ErrorCode errorCode, char const* msg) {
  _query.warnings().registerWarning(errorCode, msg);
}

void QueryExpressionContext::registerError(ErrorCode errorCode, char const* msg) {
  _query.warnings().registerError(errorCode, msg);
}

icu::RegexMatcher* QueryExpressionContext::buildRegexMatcher(char const* ptr, size_t length,
                                                             bool caseInsensitive) {
  return _aqlFunctionsInternalCache.buildRegexMatcher(ptr, length, caseInsensitive);
}

icu::RegexMatcher* QueryExpressionContext::buildLikeMatcher(char const* ptr, size_t length,
                                                            bool caseInsensitive) {
  return _aqlFunctionsInternalCache.buildLikeMatcher(ptr, length, caseInsensitive);
}

icu::RegexMatcher* QueryExpressionContext::buildSplitMatcher(AqlValue splitExpression,
                                                             velocypack::Options const* opts,
                                                             bool& isEmptyExpression) {
  return _aqlFunctionsInternalCache.buildSplitMatcher(splitExpression, opts, isEmptyExpression);
}

arangodb::ValidatorBase* QueryExpressionContext::buildValidator(arangodb::velocypack::Slice const& params) {
  return _aqlFunctionsInternalCache.buildValidator(params);
}

TRI_vocbase_t& QueryExpressionContext::vocbase() const {
  return _trx.vocbase();
}

transaction::Methods& QueryExpressionContext::trx() const {
  return _trx;
}

bool QueryExpressionContext::killed() const  {
  return _query.killed();
}
