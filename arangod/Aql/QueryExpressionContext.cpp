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

#include "QueryExpressionContext.h"

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/AqlValue.h"
#include "Aql/QueryContext.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;

void QueryExpressionContext::registerWarning(ErrorCode errorCode,
                                             std::string_view msg) {
  _query.warnings().registerWarning(errorCode, msg);
}

void QueryExpressionContext::registerError(ErrorCode errorCode,
                                           std::string_view msg) {
  _query.warnings().registerError(errorCode, msg);
}

icu::RegexMatcher* QueryExpressionContext::buildRegexMatcher(
    std::string_view expr, bool caseInsensitive) {
  return _aqlFunctionsInternalCache.buildRegexMatcher(expr, caseInsensitive);
}

icu::RegexMatcher* QueryExpressionContext::buildLikeMatcher(
    std::string_view expr, bool caseInsensitive) {
  return _aqlFunctionsInternalCache.buildLikeMatcher(expr, caseInsensitive);
}

icu::RegexMatcher* QueryExpressionContext::buildSplitMatcher(
    AqlValue splitExpression, velocypack::Options const* opts,
    bool& isEmptyExpression) {
  return _aqlFunctionsInternalCache.buildSplitMatcher(splitExpression, opts,
                                                      isEmptyExpression);
}

arangodb::ValidatorBase* QueryExpressionContext::buildValidator(
    arangodb::velocypack::Slice params) {
  return _aqlFunctionsInternalCache.buildValidator(params);
}

TRI_vocbase_t& QueryExpressionContext::vocbase() const {
  return _trx.vocbase();
}

transaction::Methods& QueryExpressionContext::trx() const { return _trx; }

bool QueryExpressionContext::killed() const { return _query.killed(); }

QueryContext& QueryExpressionContext::query() { return _query; }

void QueryExpressionContext::setVariable(Variable const* variable,
                                         arangodb::velocypack::Slice value) {
  _variables.emplace(variable, value);
}

void QueryExpressionContext::clearVariable(Variable const* variable) noexcept {
  _variables.erase(variable);
}
