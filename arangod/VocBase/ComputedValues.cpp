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

#include "ComputedValues.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Expression.h"
#include "Aql/ExpressionContext.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Aql/Parser.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryString.h"
#include "Aql/QueryWarnings.h"
#include "Aql/StandaloneCalculation.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Basics/debugging.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <string_view>
#include <type_traits>
#include <unordered_set>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
// name of bind parameter variable that contains the current document
constexpr std::string_view docParameter("doc");

// helper function to turn an enum class value into its underlying type
std::underlying_type<arangodb::ComputeValuesOn>::type mustComputeOnValue(
    arangodb::ComputeValuesOn mustComputeOn) {
  return static_cast<std::underlying_type<arangodb::ComputeValuesOn>::type>(
      mustComputeOn);
}

// expression context used for calculating computed values inside
class ComputedValuesExpressionContext final : public aql::ExpressionContext {
 public:
  explicit ComputedValuesExpressionContext(transaction::Methods& trx)
      : ExpressionContext(), _trx(trx), _failOnWarning(false) {}

  void registerWarning(ErrorCode errorCode, char const* msg) override {
    if (_failOnWarning) {
      // treat as an error if we are supposed to treat warnings as errors
      registerError(errorCode, msg);
    }
  }

  void registerError(ErrorCode errorCode, char const* msg) override {
    TRI_ASSERT(errorCode != TRI_ERROR_NO_ERROR);

    if (msg == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(errorCode,
                                     "computed values runtime error");
    }

    std::string error("computed values runtime error: ");
    error.append(msg);
    THROW_ARANGO_EXCEPTION_MESSAGE(errorCode, error);
  }

  void failOnWarning(bool value) { _failOnWarning = value; }

  icu::RegexMatcher* buildRegexMatcher(char const* ptr, size_t length,
                                       bool caseInsensitive) override {
    return _aqlFunctionsInternalCache.buildRegexMatcher(ptr, length,
                                                        caseInsensitive);
  }

  icu::RegexMatcher* buildLikeMatcher(char const* ptr, size_t length,
                                      bool caseInsensitive) override {
    return _aqlFunctionsInternalCache.buildLikeMatcher(ptr, length,
                                                       caseInsensitive);
  }

  icu::RegexMatcher* buildSplitMatcher(AqlValue splitExpression,
                                       velocypack::Options const* opts,
                                       bool& isEmptyExpression) override {
    return _aqlFunctionsInternalCache.buildSplitMatcher(splitExpression, opts,
                                                        isEmptyExpression);
  }

  arangodb::ValidatorBase* buildValidator(
      arangodb::velocypack::Slice const& params) override {
    return _aqlFunctionsInternalCache.buildValidator(params);
  }

  TRI_vocbase_t& vocbase() const override { return _trx.vocbase(); }

  // may be inaccessible on some platforms
  transaction::Methods& trx() const override { return _trx; }
  bool killed() const override { return false; }

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override {
    auto it = _variables.find(variable);
    if (it == _variables.end()) {
      return AqlValue(AqlValueHintNull());
    }
    if (doCopy) {
      return AqlValue(AqlValueHintSliceCopy(it->second));
    }
    return AqlValue(AqlValueHintSliceNoCopy(it->second));
  }

  void setVariable(Variable const* variable,
                   arangodb::velocypack::Slice value) override {
    TRI_ASSERT(variable != nullptr);
    _variables[variable] = value;
  }

  // unregister a temporary variable from the ExpressionContext.
  void clearVariable(Variable const* variable) noexcept override {
    TRI_ASSERT(variable != nullptr);
    _variables.erase(variable);
  }

 private:
  transaction::Methods& _trx;
  AqlFunctionsInternalCache _aqlFunctionsInternalCache;
  bool _failOnWarning;

  containers::FlatHashMap<Variable const*, arangodb::velocypack::Slice>
      _variables;
};

}  // namespace

namespace arangodb {

ComputedValues::ComputedValue::ComputedValue(TRI_vocbase_t& vocbase,
                                             std::string_view name,
                                             std::string_view expressionString,
                                             ComputeValuesOn mustComputeOn,
                                             bool doOverride,
                                             bool failOnWarning)
    : _vocbase(vocbase),
      _name(name),
      _expressionString(expressionString),
      _mustComputeOn(mustComputeOn),
      _override(doOverride),
      _failOnWarning(failOnWarning),
      _queryContext(StandaloneCalculation::buildQueryContext(_vocbase)),
      _rootNode(nullptr) {
  Ast* ast = _queryContext->ast();

  auto qs = QueryString(expressionString);
  Parser parser(*_queryContext, *ast, qs);
  // will throw if there is any error, but the expression should have been
  // validated before
  parser.parse();

  ast->validateAndOptimize(_queryContext->trxForOptimization(),
                           /*optimizeNonCacheable*/ false);

  if (_failOnWarning) {
    // rethrow any warnings during query inspection
    QueryWarnings const& warnings = _queryContext->warnings();
    if (!warnings.empty()) {
      auto const& allWarnings = warnings.all();
      TRI_ASSERT(!allWarnings.empty());
      THROW_ARANGO_EXCEPTION_MESSAGE(allWarnings[0].first,
                                     allWarnings[0].second);
    }
  }

  // create a temporary variable name, with which the bind parameter will be
  // replaced with, e.g. @doc -> temp_1. that way we only have to set the value
  // for the temporary variable during expression calculation, so we can use a
  // const Ast.
  ast->scopes()->start(AQL_SCOPE_MAIN);
  _tempVariable = ast->variables()->createTemporaryVariable();

  _rootNode = ast->traverseAndModify(
      const_cast<AstNode*>(ast->root()), [&](AstNode* node) {
        if (node->type == NODE_TYPE_PARAMETER) {
          // already validated before that only @doc is used as bind parameter
          TRI_ASSERT(node->getStringView() == ::docParameter);
          node = ast->createNodeReference(_tempVariable);
        }
        return node;
      });

  ast->scopes()->endCurrent();

  // the AstNode looks like this:
  // - ROOT
  //   - RETURN
  //     - expression
  TRI_ASSERT(_rootNode->type == NODE_TYPE_ROOT);
  TRI_ASSERT(_rootNode->numMembers() == 1);
  TRI_ASSERT(_rootNode->getMember(0)->type == NODE_TYPE_RETURN);
  TRI_ASSERT(_rootNode->getMember(0)->numMembers() == 1);
  _rootNode = _rootNode->getMember(0)->getMember(0);

  // build Expression object from Ast
  _expression = std::make_unique<Expression>(ast, _rootNode);
  _expression->prepareForExecution();
  TRI_ASSERT(!_expression->willUseV8());
  TRI_ASSERT(_expression->canRunOnDBServer(true));
  TRI_ASSERT(_expression->canRunOnDBServer(false));
}

ComputedValues::ComputedValue::~ComputedValue() = default;

void ComputedValues::ComputedValue::toVelocyPack(
    velocypack::Builder& result) const {
  result.openObject();
  result.add("name", VPackValue(_name));
  result.add("expression", VPackValue(_expressionString));
  result.add("mustComputeOn", VPackValue(VPackValueType::Array));
  if (::mustComputeOnValue(_mustComputeOn) &
      ::mustComputeOnValue(ComputeValuesOn::kInsert)) {
    result.add(VPackValue("insert"));
  }
  if (::mustComputeOnValue(_mustComputeOn) &
      ::mustComputeOnValue(ComputeValuesOn::kUpdate)) {
    result.add(VPackValue("update"));
  }
  if (::mustComputeOnValue(_mustComputeOn) &
      ::mustComputeOnValue(ComputeValuesOn::kReplace)) {
    result.add(VPackValue("replace"));
  }
  result.close();  // mustComputeOn
  result.add("override", VPackValue(_override));
  result.add("failOnWarning", VPackValue(_failOnWarning));
  result.close();
}

std::string_view ComputedValues::ComputedValue::name() const noexcept {
  return _name;
}

bool ComputedValues::ComputedValue::doOverride() const noexcept {
  return _override;
}

bool ComputedValues::ComputedValue::failOnWarning() const noexcept {
  return _failOnWarning;
}

aql::Variable const* ComputedValues::ComputedValue::tempVariable()
    const noexcept {
  return _tempVariable;
}

void ComputedValues::ComputedValue::computeAttribute(
    ExpressionContext& ctx, velocypack::Slice input,
    velocypack::Builder& output) const {
  TRI_ASSERT(_expression != nullptr);

  bool mustDestroy;
  AqlValue result = _expression->execute(&ctx, mustDestroy);
  AqlValueGuard guard(result, mustDestroy);

  auto const& vopts = ctx.trx().vpackOptions();
  AqlValueMaterializer materializer(&vopts);
  output.add(_name, materializer.slice(result, false));
}

ComputedValues::ComputedValues(TRI_vocbase_t& vocbase,
                               std::vector<std::string> const& shardKeys,
                               velocypack::Slice params) {
  Result res = buildDefinitions(vocbase, shardKeys, params);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

ComputedValues::~ComputedValues() = default;

bool ComputedValues::mustComputeValuesOnInsert() const noexcept {
  return !_attributesForInsert.empty();
}

bool ComputedValues::mustComputeValuesOnUpdate() const noexcept {
  return !_attributesForUpdate.empty();
}

bool ComputedValues::mustComputeValuesOnReplace() const noexcept {
  return !_attributesForReplace.empty();
}

void ComputedValues::mergeComputedAttributes(
    transaction::Methods& trx, velocypack::Slice input,
    containers::FlatHashSet<std::string_view> const& keysWritten,
    ComputeValuesOn mustComputeOn, velocypack::Builder& output) const {
  if (mustComputeOn == ComputeValuesOn::kInsert) {
    // insert case
    mergeComputedAttributes(_attributesForInsert, trx, input, keysWritten,
                            output);
  } else if (mustComputeOn == ComputeValuesOn::kUpdate) {
    // update case
    mergeComputedAttributes(_attributesForUpdate, trx, input, keysWritten,
                            output);
  } else if (mustComputeOn == ComputeValuesOn::kReplace) {
    // replace case
    mergeComputedAttributes(_attributesForReplace, trx, input, keysWritten,
                            output);
  } else {
    TRI_ASSERT(false);
  }
}

void ComputedValues::mergeComputedAttributes(
    containers::FlatHashMap<std::string, std::size_t> const& attributes,
    transaction::Methods& trx, velocypack::Slice input,
    containers::FlatHashSet<std::string_view> const& keysWritten,
    velocypack::Builder& output) const {
  ComputedValuesExpressionContext ctx(trx);

  output.openObject();

  {
    // copy over document attributes, one by one, in the same
    // order (the order is important, because we expect _key, _id and _rev
    // to be at the front)
    VPackObjectIterator it(input, true);

    while (it.valid()) {
      auto const& key = it.key();
      auto itCompute = attributes.find(key.stringView());
      if (itCompute == attributes.end() ||
          !_values[itCompute->second].doOverride()) {
        // only add these attributes from the original document
        // that we are not going to overwrite
        output.add(key);
        output.add(it.value());
      }
      it.next();
    }
  }

  // now add all the computed attributes
  for (auto const& it : attributes) {
    ComputedValue const& cv = _values[it.second];
    if (cv.doOverride() || !keysWritten.contains(cv.name())) {
      // update "failOnWarning" flag for each computation
      ctx.failOnWarning(cv.failOnWarning());
      // inject document into temporary variable (@doc)
      ctx.setVariable(cv.tempVariable(), input);
      // if "computeAttribute" throws, then the operation is
      // intentionally aborted here. caller has to catch the
      // exception
      cv.computeAttribute(ctx, input, output);
      ctx.clearVariable(cv.tempVariable());
    }
  }

  output.close();
}

Result ComputedValues::buildDefinitions(
    TRI_vocbase_t& vocbase, std::vector<std::string> const& shardKeys,
    velocypack::Slice params) {
  if (params.isNone() || params.isNull()) {
    return {};
  }

  if (!params.isArray()) {
    return {TRI_ERROR_BAD_PARAMETER, "'computedValues' must be an array"};
  }

  using namespace std::literals::string_literals;

  std::unordered_set<std::string_view> names;
  Result res;

  for (auto it : VPackArrayIterator(params)) {
    VPackSlice name = it.get("name");
    if (!name.isString() || name.getStringLength() == 0) {
      return res.reset(
          TRI_ERROR_BAD_PARAMETER,
          "invalid 'computedValues' entry: invalid attribute name");
    }

    std::string_view n = name.stringView();

    if (n == StaticStrings::IdString || n == StaticStrings::RevString ||
        n == StaticStrings::KeyString || n == StaticStrings::FromString ||
        n == StaticStrings::ToString) {
      return res.reset(
          TRI_ERROR_BAD_PARAMETER,
          "invalid 'computedValues' entry: '"s + name.copyString() +
              "' attribute cannot be computed via computation expression");
    }

    // forbid computedValues on shardKeys!
    for (auto const& key : shardKeys) {
      if (key == n) {
        return res.reset(TRI_ERROR_BAD_PARAMETER,
                         "invalid 'computedValues' entry: cannot compute "
                         "values of shard key attributes");
      }
    }

    VPackSlice doOverride = it.get("override");
    if (!doOverride.isBoolean()) {
      return res.reset(
          TRI_ERROR_BAD_PARAMETER,
          "invalid 'computedValues' entry: 'override' must be a boolean");
    }

    ComputeValuesOn mustComputeOn = ComputeValuesOn::kInsert;

    VPackSlice on = it.get("on");
    if (on.isArray()) {
      mustComputeOn = ComputeValuesOn::kNever;

      for (auto const& onValue : VPackArrayIterator(on)) {
        if (!onValue.isString()) {
          return res.reset(
              TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: invalid on enumeration value");
        }
        std::string_view ov = onValue.stringView();
        if (ov == "insert") {
          mustComputeOn = static_cast<ComputeValuesOn>(
              ::mustComputeOnValue(mustComputeOn) |
              ::mustComputeOnValue(ComputeValuesOn::kInsert));
          _attributesForInsert.emplace(n, _values.size());
        } else if (ov == "update") {
          mustComputeOn = static_cast<ComputeValuesOn>(
              ::mustComputeOnValue(mustComputeOn) |
              ::mustComputeOnValue(ComputeValuesOn::kUpdate));
          _attributesForUpdate.emplace(n, _values.size());
        } else if (ov == "replace") {
          mustComputeOn = static_cast<ComputeValuesOn>(
              ::mustComputeOnValue(mustComputeOn) |
              ::mustComputeOnValue(ComputeValuesOn::kReplace));
          _attributesForReplace.emplace(n, _values.size());
        } else {
          return res.reset(
              TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: invalid on value '"s +
                  onValue.copyString() + "'");
        }
      }

      if (mustComputeOn == ComputeValuesOn::kNever) {
        return res.reset(TRI_ERROR_BAD_PARAMETER,
                         "invalid 'computedValues' entry: empty on value");
      }
    } else if (on.isNone()) {
      // default is just "insert"
      mustComputeOn = static_cast<ComputeValuesOn>(
          ::mustComputeOnValue(mustComputeOn) |
          ::mustComputeOnValue(ComputeValuesOn::kInsert));
      _attributesForInsert.emplace(n, _values.size());
    } else {
      return res.reset(TRI_ERROR_BAD_PARAMETER,
                       "invalid 'computedValues' entry: invalid on value");
    }

    VPackSlice expression = it.get("expression");
    if (!expression.isString()) {
      return res.reset(
          TRI_ERROR_BAD_PARAMETER,
          "invalid 'computedValues' entry: invalid computation expression");
    }

    // validate the actual expression
    res.reset(StandaloneCalculation::validateQuery(
        vocbase, expression.stringView(), ::docParameter,
        " in computation expression", /*isComputedValue*/ true));
    if (res.fail()) {
      std::string errorMsg = "invalid 'computedValues' entry: ";
      errorMsg.append(res.errorMessage());
      res.reset(TRI_ERROR_BAD_PARAMETER, std::move(errorMsg));
      break;
    }

    bool failOnWarning = false;
    if (VPackSlice fow = it.get("failOnWarning"); fow.isBoolean()) {
      failOnWarning = fow.getBoolean();
    }

    // check for duplicate names in the array
    if (!names.emplace(name.stringView()).second) {
      using namespace std::literals::string_literals;
      return res.reset(
          TRI_ERROR_BAD_PARAMETER,
          "invalid 'computedValues' entry: duplicate attribute name '"s +
              name.copyString() + "'");
    }

    try {
      _values.emplace_back(vocbase, name.stringView(), expression.stringView(),
                           mustComputeOn, doOverride.getBoolean(),
                           failOnWarning);
    } catch (std::exception const& ex) {
      return res.reset(TRI_ERROR_BAD_PARAMETER,
                       "invalid 'computedValues' entry: "s + ex.what());
    }
  }

  return res;
}

void ComputedValues::toVelocyPack(velocypack::Builder& result) const {
  if (_values.empty()) {
    result.add(VPackSlice::emptyArraySlice());
  } else {
    result.openArray();
    for (auto const& it : _values) {
      it.toVelocyPack(result);
    }
    result.close();
  }
}

}  // namespace arangodb
