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

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/AqlValue.h"
#include "Aql/ExpressionContext.h"
#include "Basics/ErrorCode.h"
#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;
struct ValidatorBase;

namespace aql {
struct AstNode;
class Expression;
class ExpressionContext;
class QueryContext;
struct Variable;
}  // namespace aql

namespace transaction {
class Methods;
}

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

enum class ComputeValuesOn : uint8_t {
  kNever = 0,
  kInsert = 1,
  kUpdate = 2,
  kReplace = 4,
};

// expression context used for calculating computed values inside
class ComputedValuesExpressionContext final : public aql::ExpressionContext {
 public:
  explicit ComputedValuesExpressionContext(transaction::Methods& trx,
                                           LogicalCollection& collection);

  void registerWarning(ErrorCode errorCode, std::string_view msg) override;

  void registerError(ErrorCode errorCode, std::string_view msg) override;

  void failOnWarning(bool value) noexcept { _failOnWarning = value; }

  void setName(std::string_view name) noexcept { _name = name; }

  icu::RegexMatcher* buildRegexMatcher(std::string_view expr,
                                       bool caseInsensitive) override;

  icu::RegexMatcher* buildLikeMatcher(std::string_view expr,
                                      bool caseInsensitive) override;

  icu::RegexMatcher* buildSplitMatcher(aql::AqlValue splitExpression,
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

  // unregister a temporary variable from the ExpressionContext.
  void clearVariable(aql::Variable const* variable) noexcept override;

 private:
  std::string buildLogMessage(std::string_view type,
                              std::string_view msg) const;

  transaction::Methods& _trx;
  LogicalCollection& _collection;
  aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;

  // current runtime (execution) state follows...
  // current attribute name
  std::string_view _name;
  // current setting of "failOnWarning"
  bool _failOnWarning;

  containers::FlatHashMap<aql::Variable const*, velocypack::Slice> _variables;
};

class ComputedValues {
  class ComputedValue {
   public:
    ComputedValue(TRI_vocbase_t& vocbase, std::string_view name,
                  std::string_view expressionString,
                  ComputeValuesOn mustComputeOn, bool overwrite,
                  bool failOnWarning, bool keepNull);
    ComputedValue(ComputedValue const&) = delete;
    ComputedValue& operator=(ComputedValue const&) = delete;
    ComputedValue(ComputedValue&&) = default;
    ComputedValue& operator=(ComputedValue&&) = delete;
    ~ComputedValue();

    void toVelocyPack(velocypack::Builder&) const;
    void computeAttribute(aql::ExpressionContext& ctx, velocypack::Slice input,
                          velocypack::Builder& output) const;
    std::string_view name() const noexcept;
    bool overwrite() const noexcept;
    bool failOnWarning() const noexcept;
    bool keepNull() const noexcept;
    aql::Variable const* tempVariable() const noexcept;

   private:
    TRI_vocbase_t& _vocbase;
    std::string _name;
    std::string _expressionString;
    ComputeValuesOn _mustComputeOn;
    bool _overwrite;
    bool _failOnWarning;
    bool _keepNull;
    std::unique_ptr<aql::QueryContext> _queryContext;

    std::unique_ptr<aql::Expression> _expression;
    // temporary variable we will use for injecting the bind
    // parameter's (@doc) value into.
    aql::Variable const* _tempVariable;
    // root node of the expression
    aql::AstNode* _rootNode;
  };

 public:
  explicit ComputedValues(TRI_vocbase_t& vocbase,
                          std::span<std::string const> shardKeys,
                          velocypack::Slice params);
  ComputedValues(ComputedValues const&) = delete;
  ComputedValues& operator=(ComputedValues const&) = delete;
  ~ComputedValues();

  void toVelocyPack(velocypack::Builder&) const;

  bool mustComputeValuesOnInsert() const noexcept;
  bool mustComputeValuesOnUpdate() const noexcept;
  bool mustComputeValuesOnReplace() const noexcept;

  void mergeComputedAttributes(
      aql::ExpressionContext& ctx, transaction::Methods& trx,
      velocypack::Slice input,
      containers::FlatHashSet<std::string_view> const& keysWritten,
      ComputeValuesOn mustComputeOn, velocypack::Builder& output) const;

  static ResultT<std::shared_ptr<ComputedValues>> buildInstance(
      TRI_vocbase_t& vocbase, std::vector<std::string> const& shardKeys,
      velocypack::Slice computedValues);

 private:
  void mergeComputedAttributes(
      aql::ExpressionContext& ctx,
      containers::FlatHashMap<std::string, std::size_t> const& attributes,
      transaction::Methods& trx, velocypack::Slice input,
      containers::FlatHashSet<std::string_view> const& keysWritten,
      velocypack::Builder& output) const;

  Result buildDefinitions(TRI_vocbase_t& vocbase,
                          std::span<std::string const> shardKeys,
                          velocypack::Slice params);

  // individual instructions for computed values
  std::vector<ComputedValue> _values;

  // the size_t value indiciates the position of the computation inside
  // the _values vector
  containers::FlatHashMap<std::string, std::size_t> _attributesForInsert;
  containers::FlatHashMap<std::string, std::size_t> _attributesForUpdate;
  containers::FlatHashMap<std::string, std::size_t> _attributesForReplace;
};

}  // namespace arangodb
