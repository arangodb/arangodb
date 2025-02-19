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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ComputedValues.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Expression.h"
#include "Aql/LazyConditions.h"
#include "Aql/Parser.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryString.h"
#include "Aql/QueryWarnings.h"
#include "Aql/StandaloneCalculation.h"
#include "Aql/Variable.h"
#include "Basics/DownCast.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <absl/strings/str_cat.h>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <span>
#include <string_view>
#include <type_traits>

namespace {
// name of bind parameter variable that contains the current document
constexpr std::string_view docParameter("doc");

// helper function to turn an enum class value into its underlying type
std::underlying_type_t<arangodb::ComputeValuesOn> mustComputeOnValue(
    arangodb::ComputeValuesOn mustComputeOn) {
  return static_cast<std::underlying_type_t<arangodb::ComputeValuesOn>>(
      mustComputeOn);
}

}  // namespace

namespace arangodb {

// expression context used for calculating computed values inside
ComputedValuesExpressionContext::ComputedValuesExpressionContext(
    transaction::Methods& trx, LogicalCollection& collection)
    : aql::ExpressionContext(),
      _trx(trx),
      _collection(collection),
      _failOnWarning(false) {}

TRI_vocbase_t& ComputedValuesExpressionContext::vocbase() const {
  return _trx.vocbase();
}

transaction::Methods& ComputedValuesExpressionContext::trx() const {
  return _trx;
}

void ComputedValuesExpressionContext::registerWarning(ErrorCode errorCode,
                                                      std::string_view msg) {
  if (_failOnWarning) {
    // treat as an error if we are supposed to treat warnings as errors
    registerError(errorCode, msg);
  } else {
    std::string error = buildLogMessage("warning", msg);
    LOG_TOPIC("6a31d", WARN, Logger::TRANSACTIONS) << error;
  }
}

void ComputedValuesExpressionContext::registerError(ErrorCode errorCode,
                                                    std::string_view msg) {
  TRI_ASSERT(errorCode != TRI_ERROR_NO_ERROR);

  std::string error = buildLogMessage("error", msg);
  LOG_TOPIC("2a37f", WARN, Logger::TRANSACTIONS) << error;
  THROW_ARANGO_EXCEPTION_MESSAGE(errorCode, error);
}

std::string ComputedValuesExpressionContext::buildLogMessage(
    std::string_view type, std::string_view msg) const {
  // note: on DB servers, the error message will contain the shard name
  // rather than the collection name.
  std::string error = absl::StrCat(
      "computed values expression evaluation produced a runtime ", type,
      " for attribute '", _name, "' of collection '",
      _collection.vocbase().name(), "/", _collection.name(), "': ", msg);

  return error;
}

icu_64_64::RegexMatcher* ComputedValuesExpressionContext::buildRegexMatcher(
    std::string_view expr, bool caseInsensitive) {
  return _aqlFunctionsInternalCache.buildRegexMatcher(expr, caseInsensitive);
}

icu_64_64::RegexMatcher* ComputedValuesExpressionContext::buildLikeMatcher(
    std::string_view expr, bool caseInsensitive) {
  return _aqlFunctionsInternalCache.buildLikeMatcher(expr, caseInsensitive);
}

icu_64_64::RegexMatcher* ComputedValuesExpressionContext::buildSplitMatcher(
    aql::AqlValue splitExpression, velocypack::Options const* opts,
    bool& isEmptyExpression) {
  return _aqlFunctionsInternalCache.buildSplitMatcher(splitExpression, opts,
                                                      isEmptyExpression);
}

ValidatorBase* ComputedValuesExpressionContext::buildValidator(
    velocypack::Slice params) {
  return _aqlFunctionsInternalCache.buildValidator(params);
}

aql::AqlValue ComputedValuesExpressionContext::getVariableValue(
    aql::Variable const* variable, bool doCopy, bool& mustDestroy) const {
  auto it = _variables.find(variable);
  if (it == _variables.end()) {
    return aql::AqlValue(aql::AqlValueHintNull());
  }
  if (doCopy) {
    return aql::AqlValue(aql::AqlValueHintSliceCopy(it->second));
  }
  return aql::AqlValue(aql::AqlValueHintSliceNoCopy(it->second));
}

void ComputedValuesExpressionContext::setVariable(aql::Variable const* variable,
                                                  velocypack::Slice value) {
  TRI_ASSERT(variable != nullptr);
  _variables[variable] = value;
}

// unregister a temporary variable from the ExpressionContext.
void ComputedValuesExpressionContext::clearVariable(
    aql::Variable const* variable) noexcept {
  TRI_ASSERT(variable != nullptr);
  _variables.erase(variable);
}

ComputedValues::ComputedValue::ComputedValue(
    TRI_vocbase_t& vocbase, std::string_view name,
    std::string_view expressionString,
    transaction::OperationOrigin operationOrigin, ComputeValuesOn mustComputeOn,
    bool overwrite, bool failOnWarning, bool keepNull)
    : _vocbase(vocbase),
      _name(name),
      _expressionString(expressionString),
      _mustComputeOn(mustComputeOn),
      _overwrite(overwrite),
      _failOnWarning(failOnWarning),
      _keepNull(keepNull),
      _queryContext(aql::StandaloneCalculation::buildQueryContext(
          _vocbase, operationOrigin)),
      _rootNode(nullptr) {
  aql::Ast* ast = _queryContext->ast();

  auto qs = aql::QueryString(expressionString);
  aql::Parser parser(*_queryContext, *ast, qs);
  // force the condition of the ternary operator (condition ? truePart :
  // falsePart) to be always inlined and not be extracted into its own LET node.
  // if we don't set this boolean flag here, then a ternary operator could
  // create additional LET nodes, which is not supported inside computed values.
  parser.lazyConditions().pushForceInline();
  // will throw if there is any error, but the expression should have been
  // validated before
  parser.parse();

  // we have to set "optimizeNonCacheable" to false here, so that the
  // queryString expression gets re-evaluated every time, and does not
  // store the computed results once (e.g. when using a queryString such
  // as "RETURN DATE_NOW()" you always want the current date to be
  // returned, and not a date once stored)
  ast->validateAndOptimize(
      _queryContext->trxForOptimization(),
      {.optimizeNonCacheable = false, .optimizeFunctionCalls = false});

  if (_failOnWarning) {
    // rethrow any warnings during query inspection
    auto const& warnings = _queryContext->warnings();
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
  ast->scopes()->start(aql::AQL_SCOPE_MAIN);
  _tempVariable = ast->variables()->createTemporaryVariable();

  _rootNode = ast->traverseAndModify(
      const_cast<aql::AstNode*>(ast->root()), [&](aql::AstNode* node) {
        if (node->type == aql::NODE_TYPE_PARAMETER) {
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
  TRI_ASSERT(_rootNode->type == aql::NODE_TYPE_ROOT);
  TRI_ASSERT(_rootNode->numMembers() == 1);
  TRI_ASSERT(_rootNode->getMember(0)->type == aql::NODE_TYPE_RETURN);
  TRI_ASSERT(_rootNode->getMember(0)->numMembers() == 1);
  _rootNode = _rootNode->getMember(0)->getMember(0);

  // build Expression object from Ast
  _expression = std::make_unique<aql::Expression>(ast, _rootNode);
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
  result.add("computeOn", VPackValue(VPackValueType::Array));
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
  result.close();  // computeOn
  result.add("overwrite", VPackValue(_overwrite));
  result.add("failOnWarning", VPackValue(_failOnWarning));
  result.add("keepNull", VPackValue(_keepNull));
  result.close();
}

std::string_view ComputedValues::ComputedValue::name() const noexcept {
  return _name;
}

bool ComputedValues::ComputedValue::overwrite() const noexcept {
  return _overwrite;
}

bool ComputedValues::ComputedValue::failOnWarning() const noexcept {
  return _failOnWarning;
}

bool ComputedValues::ComputedValue::keepNull() const noexcept {
  return _keepNull;
}

aql::Variable const* ComputedValues::ComputedValue::tempVariable()
    const noexcept {
  return _tempVariable;
}

void ComputedValues::ComputedValue::computeAttribute(
    aql::ExpressionContext& ctx, velocypack::Slice input,
    velocypack::Builder& output) const {
  TRI_ASSERT(_expression != nullptr);

  bool mustDestroy;
  aql::AqlValue result = _expression->execute(&ctx, mustDestroy);
  aql::AqlValueGuard guard(result, mustDestroy);

  if (!_keepNull && result.isNull(true)) {
    // the expression produced a value of null, but we don't want
    // to keep null values
    return;
  }

  auto const& vopts = ctx.trx().vpackOptions();
  aql::AqlValueMaterializer materializer(&vopts);
  output.add(_name, materializer.slice(result));
}

ComputedValues::ComputedValues(TRI_vocbase_t& vocbase,
                               std::span<std::string const> shardKeys,
                               velocypack::Slice params,
                               transaction::OperationOrigin operationOrigin) {
  Result res = buildDefinitions(vocbase, shardKeys, params, operationOrigin);
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
    aql::ExpressionContext& ctx, transaction::Methods& trx,
    velocypack::Slice input,
    containers::FlatHashSet<std::string_view> const& keysWritten,
    ComputeValuesOn mustComputeOn, velocypack::Builder& output) const {
  if (mustComputeOn == ComputeValuesOn::kInsert) {
    mergeComputedAttributes(ctx, _attributesForInsert, trx, input, keysWritten,
                            output);
  } else if (mustComputeOn == ComputeValuesOn::kUpdate) {
    mergeComputedAttributes(ctx, _attributesForUpdate, trx, input, keysWritten,
                            output);
  } else if (mustComputeOn == ComputeValuesOn::kReplace) {
    mergeComputedAttributes(ctx, _attributesForReplace, trx, input, keysWritten,
                            output);
  } else {
    TRI_ASSERT(false);
  }
}

void ComputedValues::mergeComputedAttributes(
    aql::ExpressionContext& ctx,
    containers::FlatHashMap<std::string, std::size_t> const& attributes,
    transaction::Methods& trx, velocypack::Slice input,
    containers::FlatHashSet<std::string_view> const& keysWritten,
    velocypack::Builder& output) const {
  output.openObject();

  {
    // copy over document attributes, one by one, in the same
    // order (the order is important, because we expect _key, _id and _rev
    // to be at the front)
    VPackObjectIterator it(input, true);

    while (it.valid()) {
      // note: key slices can be strings or numbers. they are numbers
      // for the internal attributes _id, _key, _rev, _from, _to
      VPackSlice key = it.key(/*translate*/ false);
      if (key.isNumber()) {
        // _id, _key, _rev, _from, _to
        output.addUnchecked(key, it.value());
      } else {
        auto itCompute = attributes.find(key.stringView());
        if (itCompute == attributes.end() ||
            !_values[itCompute->second].overwrite()) {
          // only add these attributes from the original document
          // that we are not going to overwrite
          output.addUnchecked(key, it.value());
        }
      }
      it.next();
    }
  }

  // now add all the computed attributes
  auto& cvec = basics::downCast<ComputedValuesExpressionContext>(ctx);

  for (auto const& it : attributes) {
    auto const& cv = _values[it.second];
    if (cv.overwrite() || !keysWritten.contains(cv.name())) {
      // update "failOnWarning" flag for each computation
      cvec.failOnWarning(cv.failOnWarning());
      // update "name" vlaue for each computation (for errors/warnings)
      cvec.setName(cv.name());
      // inject document into temporary variable (@doc)
      cvec.setVariable(cv.tempVariable(), input);
      // if "computeAttribute" throws, then the operation is
      // intentionally aborted here. caller has to catch the
      // exception
      cv.computeAttribute(cvec, input, output);
      cvec.clearVariable(cv.tempVariable());
    }
  }

  output.close();
}

Result ComputedValues::buildDefinitions(
    TRI_vocbase_t& vocbase, std::span<std::string const> shardKeys,
    velocypack::Slice params, transaction::OperationOrigin operationOrigin) {
  if (params.isNone() || params.isNull()) {
    return {};
  }

  if (!params.isArray()) {
    return {TRI_ERROR_BAD_PARAMETER, "'computedValues' must be an array"};
  }

  containers::FlatHashSet<std::string_view> names;

  for (auto it : VPackArrayIterator(params)) {
    VPackSlice name = it.get("name");
    if (!name.isString() || name.getStringLength() == 0) {
      return {TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: invalid attribute name"};
    }

    auto n = name.stringView();

    if (n == StaticStrings::IdString || n == StaticStrings::RevString ||
        n == StaticStrings::KeyString || n == StaticStrings::FromString ||
        n == StaticStrings::ToString) {
      return {
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat(
              "invalid 'computedValues' entry: '", name.stringView(),
              "' attribute must not be computed via computation expression")};
    }

    // forbid computedValues on shardKeys!
    for (auto const& key : shardKeys) {
      if (key == n) {
        return {TRI_ERROR_BAD_PARAMETER,
                "invalid 'computedValues' entry: cannot compute "
                "values for shard key attributes"};
      }
    }

    VPackSlice overwrite = it.get("overwrite");
    if (!overwrite.isBoolean()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: 'overwrite' must be a boolean"};
    }

    ComputeValuesOn mustComputeOn = ComputeValuesOn::kNever;

    VPackSlice on = it.get("computeOn");
    if (on.isArray()) {
      for (auto const& onValue : VPackArrayIterator(on)) {
        if (!onValue.isString()) {
          return {TRI_ERROR_BAD_PARAMETER,
                  "invalid 'computedValues' entry: invalid 'computeOn' value"};
        }

        auto ov = onValue.stringView();
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
          return {TRI_ERROR_BAD_PARAMETER,
                  absl::StrCat("invalid 'computedValues' entry: invalid "
                               "'computeOn' value: '",
                               ov, "'")};
        }
      }

      if (mustComputeOn == ComputeValuesOn::kNever) {
        return {TRI_ERROR_BAD_PARAMETER,
                "invalid 'computedValues' entry: empty 'computeOn' value"};
      }
    } else if (on.isNone()) {
      // default for "computeOn" is ["insert", "update", "replace"]
      mustComputeOn = static_cast<ComputeValuesOn>(
          ::mustComputeOnValue(ComputeValuesOn::kInsert) |
          ::mustComputeOnValue(ComputeValuesOn::kUpdate) |
          ::mustComputeOnValue(ComputeValuesOn::kReplace));
      _attributesForInsert.emplace(n, _values.size());
      _attributesForUpdate.emplace(n, _values.size());
      _attributesForReplace.emplace(n, _values.size());
    } else {
      return {TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: invalid 'computeOn' value"};
    }

    VPackSlice expression = it.get("expression");
    if (!expression.isString()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: invalid 'expression' value"};
    }

    // validate the actual expression
    Result res = aql::StandaloneCalculation::validateQuery(
        vocbase, expression.stringView(), ::docParameter,
        " in computation expression", operationOrigin,
        /*isComputedValue*/ true);
    if (res.fail()) {
      return {
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("invalid 'computedValues' entry: ", res.errorMessage())};
    }

    bool failOnWarning = false;
    if (VPackSlice fow = it.get("failOnWarning"); fow.isBoolean()) {
      failOnWarning = fow.getBoolean();
    }

    bool keepNull = true;
    if (VPackSlice kn = it.get("keepNull"); kn.isBoolean()) {
      keepNull = kn.getBoolean();
    }

    // check for duplicate names in the array
    if (!names.emplace(name.stringView()).second) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat(
                  "invalid 'computedValues' entry: duplicate attribute name '",
                  name.stringView(), "'")};
    }

    try {
      _values.emplace_back(vocbase, name.stringView(), expression.stringView(),
                           operationOrigin, mustComputeOn,
                           overwrite.getBoolean(), failOnWarning, keepNull);
    } catch (std::exception const& ex) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("invalid 'computedValues' entry: ", ex.what())};
    }
  }

  return {};
}

ResultT<std::shared_ptr<ComputedValues>> ComputedValues::buildInstance(
    TRI_vocbase_t& vocbase, std::vector<std::string> const& shardKeys,
    velocypack::Slice computedValues,
    transaction::OperationOrigin operationOrigin) {
  if (!computedValues.isNone()) {
    if (computedValues.isNull()) {
      computedValues = VPackSlice::emptyArraySlice();
    }
    if (!computedValues.isArray()) {
      return Result{TRI_ERROR_BAD_PARAMETER,
                    "Computed values description is not an array."};
    }

    TRI_ASSERT(computedValues.isArray());

    std::shared_ptr<ComputedValues> newValue;

    // computed values will be removed if empty array is given
    if (!computedValues.isEmptyArray()) {
      try {
        return std::make_shared<ComputedValues>(
            vocbase.server()
                .getFeature<DatabaseFeature>()
                .getCalculationVocbase(),
            std::span(shardKeys), computedValues, operationOrigin);
      } catch (std::exception const& ex) {
        return Result{
            TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Error when validating computedValues: ", ex.what())};
      }
    }
  }

  return std::shared_ptr<ComputedValues>();
}

void ComputedValues::toVelocyPack(velocypack::Builder& result) const {
  if (_values.empty()) {
    result.add(VPackSlice::emptyArraySlice());
    return;
  }

  result.openArray();
  for (auto const& it : _values) {
    it.toVelocyPack(result);
  }
  result.close();
}

}  // namespace arangodb
