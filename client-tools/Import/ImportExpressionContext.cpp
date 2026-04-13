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

#include "ImportExpressionContext.h"

#include "Aql/AqlValue.h"
#include "Aql/QueryContext.h"
#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::import;

ImportExpressionContext::ImportExpressionContext(transaction::Methods& trx,
                                                 aql::QueryContext& query)
    : _trx(trx), _query(query) {}

void ImportExpressionContext::registerWarning(ErrorCode errorCode,
                                               std::string_view msg) {
  // Log warnings but don't abort — consistent with the design doc's
  // requirement that AQL warnings are logged but don't count as errors.
  LOG_TOPIC("d4c1a", WARN, Logger::FIXME)
      << "custom query transformation warning: " << msg;
}

void ImportExpressionContext::registerError(ErrorCode errorCode,
                                             std::string_view msg) {
  LOG_TOPIC("e5b2c", WARN, Logger::FIXME)
      << "custom query transformation error: " << msg;
  THROW_ARANGO_EXCEPTION_MESSAGE(errorCode, msg);
}

icu_64_64::RegexMatcher* ImportExpressionContext::buildRegexMatcher(
    std::string_view expr, bool caseInsensitive) {
  return _aqlFunctionsInternalCache.buildRegexMatcher(expr, caseInsensitive);
}

icu_64_64::RegexMatcher* ImportExpressionContext::buildLikeMatcher(
    std::string_view expr, bool caseInsensitive) {
  return _aqlFunctionsInternalCache.buildLikeMatcher(expr, caseInsensitive);
}

icu_64_64::RegexMatcher* ImportExpressionContext::buildSplitMatcher(
    aql::AqlValue splitExpression, velocypack::Options const* opts,
    bool& isEmptyExpression) {
  return _aqlFunctionsInternalCache.buildSplitMatcher(splitExpression, opts,
                                                       isEmptyExpression);
}

ValidatorBase* ImportExpressionContext::buildValidator(
    velocypack::Slice params) {
  return _aqlFunctionsInternalCache.buildValidator(params);
}

TRI_vocbase_t& ImportExpressionContext::vocbase() const {
  return _trx.vocbase();
}

transaction::Methods& ImportExpressionContext::trx() const { return _trx; }

aql::AqlValue ImportExpressionContext::getVariableValue(
    aql::Variable const* variable, bool doCopy, bool& mustDestroy) const {
  auto it = _variables.find(variable);
  if (it == _variables.end()) {
    mustDestroy = true;
    return aql::AqlValue(aql::AqlValueHintNull());
  }
  if (doCopy) {
    mustDestroy = true;
    return aql::AqlValue(aql::AqlValueHintSliceCopy(it->second));
  }
  mustDestroy = false;
  return aql::AqlValue(aql::AqlValueHintSliceNoCopy(it->second));
}

void ImportExpressionContext::setVariable(aql::Variable const* variable,
                                           velocypack::Slice value) {
  TRI_ASSERT(variable != nullptr);
  _variables[variable] = value;
}

void ImportExpressionContext::clearVariable(
    aql::Variable const* variable) noexcept {
  TRI_ASSERT(variable != nullptr);
  _variables.erase(variable);
}

void ImportExpressionContext::clearAllVariables() noexcept {
  _variables.clear();
}
