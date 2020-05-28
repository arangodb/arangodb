////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchExpressionContext.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/QueryContext.h"
#include "Aql/IResearchViewNode.h"
#include "Basics/StaticStrings.h"

#include <Containers/HashSet.h>

namespace arangodb {
namespace iresearch {

using namespace arangodb::aql;

// -----------------------------------------------------------------------------
// --SECTION--                          ViewExpressionContextBase implementation
// -----------------------------------------------------------------------------

void ViewExpressionContextBase::registerWarning(int errorCode, char const* msg) {
  _query->warnings().registerWarning(errorCode, msg);
}

void ViewExpressionContextBase::registerError(int errorCode, char const* msg) {
  _query->warnings().registerError(errorCode, msg);
}

icu::RegexMatcher* ViewExpressionContextBase::buildRegexMatcher(char const* ptr, size_t length,
                                                             bool caseInsensitive) {
  return _aqlFunctionsInternalCache->buildRegexMatcher(ptr, length, caseInsensitive);
}

icu::RegexMatcher* ViewExpressionContextBase::buildLikeMatcher(char const* ptr, size_t length,
                                                            bool caseInsensitive) {
  return _aqlFunctionsInternalCache->buildLikeMatcher(ptr, length, caseInsensitive);
}

icu::RegexMatcher* ViewExpressionContextBase::buildSplitMatcher(AqlValue splitExpression,
                                                             velocypack::Options const* opts,
                                                             bool& isEmptyExpression) {
  return _aqlFunctionsInternalCache->buildSplitMatcher(splitExpression, opts, isEmptyExpression);
}

arangodb::ValidatorBase* ViewExpressionContextBase::buildValidator(arangodb::velocypack::Slice const& params) {
  return _aqlFunctionsInternalCache->buildValidator(params);
}

TRI_vocbase_t& ViewExpressionContextBase::vocbase() const {
  return _trx->vocbase();
}

transaction::Methods& ViewExpressionContextBase::trx() const {
  return *_trx;
}

bool ViewExpressionContextBase::killed() const  {
  return _query->killed();
}

// -----------------------------------------------------------------------------
// --SECTION--                              ViewExpressionContext implementation
// -----------------------------------------------------------------------------

AqlValue ViewExpressionContext::getVariableValue(Variable const* var, bool doCopy,
                                                 bool& mustDestroy) const {
  TRI_ASSERT(var);

  if (var == &outVariable()) {
    // self-reference
    if (_expr) {
      std::string expr;

      try {
        expr = _expr->toString();
      } catch (...) {
      }

      if (!expr.empty()) {
        THROW_ARANGO_EXCEPTION_FORMAT(
            TRI_ERROR_NOT_IMPLEMENTED,
            "Unable to evaluate loop variable '%s' as a part of ArangoSearch "
            "noncompliant expression '%s'",
            var->name.c_str(), expr.c_str());
      }
    }

    THROW_ARANGO_EXCEPTION_FORMAT(
        TRI_ERROR_NOT_IMPLEMENTED,
        "Unable to evaluate loop variable '%s' as a part of ArangoSearch "
        "noncompliant expression",
        var->name.c_str());
  }

  mustDestroy = false;

  auto const& vars = varInfoMap();
  auto const it = vars.find(var->id);

  if (vars.end() == it) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot find variable");
  }

  auto const& varInfo = it->second;

  if (varInfo.depth > decltype(varInfo.depth)(nodeDepth())) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                  "Variable '%s' is used before being assigned",
                                  var->name.c_str());
  }


  TRI_ASSERT(_inputRow.isInitialized());
  AqlValue const& value = _inputRow.getValue(varInfo.registerId);

  if (doCopy) {
    mustDestroy = true;
    return value.clone();
  }

  return value;
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
