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

#include "Basics/ErrorCode.h"

#include <string_view>
#include <unicode/regex.h>

struct TRI_vocbase_t;

namespace arangodb {
struct ValidatorBase;
namespace transaction {
class Methods;
}
namespace velocypack {
struct Options;
class Slice;
}  // namespace velocypack

namespace aql {
struct AqlValue;
struct Variable;

class ExpressionContext {
 public:
  ExpressionContext() = default;

  virtual ~ExpressionContext() = default;

  virtual AqlValue getVariableValue(Variable const* variable, bool doCopy,
                                    bool& mustDestroy) const = 0;

  virtual void registerWarning(ErrorCode errorCode, std::string_view msg) = 0;
  virtual void registerError(ErrorCode errorCode, std::string_view msg) = 0;

  virtual icu::RegexMatcher* buildRegexMatcher(std::string_view expr,
                                               bool caseInsensitive) = 0;
  virtual icu::RegexMatcher* buildLikeMatcher(std::string_view expr,
                                              bool caseInsensitive) = 0;
  virtual icu::RegexMatcher* buildSplitMatcher(AqlValue splitExpression,
                                               velocypack::Options const* opts,
                                               bool& isEmptyExpression) = 0;
  virtual arangodb::ValidatorBase* buildValidator(velocypack::Slice) = 0;

  virtual TRI_vocbase_t& vocbase() const = 0;
  virtual transaction::Methods& trx() const = 0;
  virtual bool killed() const = 0;

  // register a temporary variable in the ExpressionContext. the
  // slice used here is not owned by the QueryExpressionContext!
  // the caller has to make sure the data behind the slice remains
  // valid until clearVariable() is called or the context is discarded.
  virtual void setVariable(Variable const* variable,
                           velocypack::Slice value) = 0;

  // unregister a temporary variable from the ExpressionContext.
  virtual void clearVariable(Variable const* variable) noexcept = 0;
};
}  // namespace aql
}  // namespace arangodb
