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
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Expression.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Aql/Parser.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryString.h"
#include "Aql/StandaloneCalculation.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <string_view>
#include <type_traits>
#include <unordered_set>

using namespace arangodb::aql;

namespace {
constexpr std::string_view docParameter("doc");

std::underlying_type<arangodb::RunOn>::type runOnValue(arangodb::RunOn runOn) {
  return static_cast<std::underlying_type<arangodb::RunOn>::type>(runOn);
}

}  // namespace

namespace arangodb {

ComputedValues::ComputedValue::ComputedValue(TRI_vocbase_t& vocbase,
                                             std::string_view name,
                                             std::string_view expressionString,
                                             RunOn runOn, bool doOverride)
    : _vocbase(vocbase),
      _name(name),
      _expressionString(expressionString),
      _runOn(runOn),
      _override(doOverride),
      _queryContext(StandaloneCalculation::buildQueryContext(_vocbase)),
      _rootNode(nullptr) {
  Ast* ast = _queryContext->ast();

  auto qs = QueryString(expressionString);
  Parser parser(*_queryContext, *ast, qs);
  // will throw if there is any error, but the expression should have been
  // validated before
  parser.parse();
  ast->validateAndOptimize(_queryContext->trxForOptimization());

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
  result.add("runOn", VPackValue(VPackValueType::Array));
  if (::runOnValue(_runOn) & ::runOnValue(RunOn::kInsert)) {
    result.add(VPackValue("insert"));
  }
  if (::runOnValue(_runOn) & ::runOnValue(RunOn::kUpdate)) {
    result.add(VPackValue("update"));
  }
  if (::runOnValue(_runOn) & ::runOnValue(RunOn::kReplace)) {
    result.add(VPackValue("replace"));
  }
  result.close();  // runOn
  result.add("override", VPackValue(_override));
  result.close();
}

bool ComputedValues::ComputedValue::mustRunOn(RunOn runOn) const noexcept {
  return ((::runOnValue(_runOn) & ::runOnValue(runOn)) != 0);
}

void ComputedValues::ComputedValue::computeAttribute(
    transaction::Methods& trx, velocypack::Slice input,
    velocypack::Builder& output) const {
  TRI_ASSERT(_queryContext != nullptr);
  TRI_ASSERT(_expression != nullptr);

  AqlFunctionsInternalCache cache;

  FixedVarExpressionContext ctx(trx, *_queryContext, cache);
  // inject document into temporary variable (@doc)
  ctx.setVariableValue(_tempVariable, AqlValue(AqlValueHintSliceNoCopy(input)));

  bool mustDestroy;
  AqlValue result = _expression->execute(&ctx, mustDestroy);
  AqlValueGuard guard(result, mustDestroy);

  output.add(_name, result.slice());
}

ComputedValues::ComputedValues(TRI_vocbase_t& vocbase,
                               std::vector<std::string> const& shardKeys,
                               velocypack::Slice params)
    : _runOn(RunOn::kNever) {
  Result res = buildDefinitions(vocbase, shardKeys, params);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_ASSERT(_runOn != RunOn::kNever);
}

ComputedValues::~ComputedValues() = default;

bool ComputedValues::mustRunOnInsert() const noexcept {
  return mustRunOn(RunOn::kInsert);
}

bool ComputedValues::mustRunOnUpdate() const noexcept {
  return mustRunOn(RunOn::kUpdate);
}

bool ComputedValues::mustRunOnReplace() const noexcept {
  return mustRunOn(RunOn::kReplace);
}

void ComputedValues::computeAttributes(transaction::Methods& trx,
                                       velocypack::Slice input, RunOn runOn,
                                       velocypack::Builder& output) const {
  TRI_ASSERT(mustRunOn(runOn));

  for (auto const& it : _values) {
    if (!it.mustRunOn(runOn)) {
      continue;
    }

    it.computeAttribute(trx, input, output);
  }
}

bool ComputedValues::mustRunOn(RunOn runOn) const noexcept {
  return ((::runOnValue(_runOn) & ::runOnValue(runOn)) != 0);
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
        n == StaticStrings::KeyString) {
      return res.reset(TRI_ERROR_BAD_PARAMETER,
                       "invalid 'computedValues' entry: '"s +
                           name.copyString() +
                           "' attribute cannot be computed");
    }

    // forbid computedValues on shardKeys!
    for (auto const& key : shardKeys) {
      if (key == n) {
        return res.reset(TRI_ERROR_BAD_PARAMETER,
                         "invalid 'computedValues' entry: cannot compute "
                         "values of shard key attributes");
      }
    }

    RunOn runOn = RunOn::kInsert;

    VPackSlice on = it.get("on");
    if (on.isArray()) {
      runOn = RunOn::kNever;

      for (auto const& onValue : VPackArrayIterator(on)) {
        if (!onValue.isString()) {
          return res.reset(
              TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: invalid on enumeration value");
        }
        std::string_view ov = onValue.stringView();
        if (ov == "insert") {
          runOn = static_cast<RunOn>(::runOnValue(runOn) |
                                     ::runOnValue(RunOn::kInsert));
        } else if (ov == "update") {
          runOn = static_cast<RunOn>(::runOnValue(runOn) |
                                     ::runOnValue(RunOn::kUpdate));
        } else if (ov == "replace") {
          runOn = static_cast<RunOn>(::runOnValue(runOn) |
                                     ::runOnValue(RunOn::kReplace));
        } else {
          return res.reset(
              TRI_ERROR_BAD_PARAMETER,
              "invalid 'computedValues' entry: invalid on value '"s +
                  onValue.copyString() + "'");
        }
      }

      if (runOn == RunOn::kNever) {
        return res.reset(TRI_ERROR_BAD_PARAMETER,
                         "invalid 'computedValues' entry: empty on value");
      }
    }

    _runOn = static_cast<RunOn>(::runOnValue(_runOn) | ::runOnValue(runOn));

    VPackSlice expression = it.get("expression");
    if (!expression.isString()) {
      return res.reset(
          TRI_ERROR_BAD_PARAMETER,
          "invalid 'computedValues' entry: invalid computation expression");
    }

    // validate the actual expression
    res.reset(StandaloneCalculation::validateQuery(
        vocbase, expression.stringView(), ::docParameter, ""));
    if (res.fail()) {
      std::string errorMsg = "invalid 'computedValues' entry: ";
      errorMsg.append(res.errorMessage()).append(" in computation expression");
      res.reset(TRI_ERROR_BAD_PARAMETER, std::move(errorMsg));
      break;
    }

    VPackSlice doOverride = it.get("override");
    if (!doOverride.isBoolean()) {
      return res.reset(
          TRI_ERROR_BAD_PARAMETER,
          "invalid 'computedValues' entry: 'override' must be a boolean");
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
                           runOn, doOverride.getBoolean());
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
