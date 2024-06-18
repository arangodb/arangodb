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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchAqlAnalyzer.h"

#include "Aql/AqlCallList.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Aql/Parser.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryString.h"
#include "Aql/SharedQueryState.h"
#include "Aql/StandaloneCalculation.h"
#include "Basics/FunctionUtils.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Containers/HashSet.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/Hints.h"
#include "Transaction/SmartContext.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/vocbase.h"

#include "utils/hash_utils.hpp"
#include "utils/object_pool.hpp"

#include <absl/strings/str_cat.h>

namespace {
constexpr const char QUERY_STRING_PARAM_NAME[] = "queryString";
constexpr const char COLLAPSE_ARRAY_POSITIONS_PARAM_NAME[] =
    "collapsePositions";
constexpr const char KEEP_NULL_PARAM_NAME[] = "keepNull";
constexpr const char BATCH_SIZE_PARAM_NAME[] = "batchSize";
constexpr const char MEMORY_LIMIT_PARAM_NAME[] = "memoryLimit";
constexpr const char CALCULATION_PARAMETER_NAME[] = "param";
constexpr const char RETURN_TYPE_PARAM_NAME[] = "returnType";

constexpr const uint32_t MAX_BATCH_SIZE{1000};
constexpr const uint32_t MAX_MEMORY_LIMIT{33554432U};  // 32Mb
}  // namespace

namespace arangodb::iresearch {
template<class Inspector>
inline auto inspect(Inspector& f, AnalyzerValueType& x) {
  return f.enumeration(x).values(
      AnalyzerValueType::String, ANALYZER_VALUE_TYPE_STRING,
      AnalyzerValueType::Number, ANALYZER_VALUE_TYPE_NUMBER,
      AnalyzerValueType::Bool, ANALYZER_VALUE_TYPE_BOOL,
      AnalyzerValueType::Null, ANALYZER_VALUE_TYPE_NULL,
      AnalyzerValueType::Array, ANALYZER_VALUE_TYPE_ARRAY,
      AnalyzerValueType::Object, ANALYZER_VALUE_TYPE_OBJECT);
}

template<class Inspector>
inline auto inspect(Inspector& f,
                    arangodb::iresearch::AqlAnalyzer::Options& o) {
  using namespace arangodb::iresearch;
  return f.object(o).fields(
      f.field(QUERY_STRING_PARAM_NAME, o.queryString)
          .invariant([](std::string const& v) -> arangodb::inspection::Status {
            if (v.empty()) {
              return {absl::StrCat("Value of '", QUERY_STRING_PARAM_NAME,
                                   "' should be non empty string")};
            }
            return {};
          }),
      f.field(COLLAPSE_ARRAY_POSITIONS_PARAM_NAME, o.collapsePositions)
          .fallback(false),
      f.field(KEEP_NULL_PARAM_NAME, o.keepNull).fallback(true),
      f.field(BATCH_SIZE_PARAM_NAME, o.batchSize)
          .fallback(10u)
          .invariant([](uint32_t v) -> arangodb::inspection::Status {
            if (v == 0) {
              return {absl::StrCat("Value of '", BATCH_SIZE_PARAM_NAME,
                                   "' should be greater than 0")};
            }
            if (v > MAX_BATCH_SIZE) {
              return {absl::StrCat("Value of '", BATCH_SIZE_PARAM_NAME,
                                   "' should be less or equal to ",
                                   MAX_BATCH_SIZE)};
            }
            return {};
          }),
      f.field(MEMORY_LIMIT_PARAM_NAME, o.memoryLimit)
          .fallback(1048576U)
          .invariant([](uint32_t v) -> arangodb::inspection::Status {
            if (v == 0) {
              return {absl::StrCat("Value of '", MEMORY_LIMIT_PARAM_NAME,
                                   "' should be greater than 0")};
            }
            if (v > MAX_MEMORY_LIMIT) {
              return {absl::StrCat("Value of '", MEMORY_LIMIT_PARAM_NAME,
                                   "' should be less or equal to ",
                                   MAX_MEMORY_LIMIT)};
            }
            return {};
          }),
      f.field(RETURN_TYPE_PARAM_NAME, o.returnType)
          .fallback(AnalyzerValueType::String)
          .invariant([](AnalyzerValueType v) -> arangodb::inspection::Status {
            if (v != AnalyzerValueType::String &&
                v != AnalyzerValueType::Number &&
                v != AnalyzerValueType::Bool) {
              return {absl::StrCat("Value of '", RETURN_TYPE_PARAM_NAME,
                                   "' should be ", ANALYZER_VALUE_TYPE_STRING,
                                   " or ", ANALYZER_VALUE_TYPE_NUMBER, " or ",
                                   ANALYZER_VALUE_TYPE_BOOL)};
            }
            return {};
          }));
}
}  // namespace arangodb::iresearch

namespace {
using namespace arangodb::aql;

bool parse_options_slice(VPackSlice const& slice,
                         arangodb::iresearch::AqlAnalyzer::Options& options) {
  auto const res = arangodb::velocypack::deserializeWithStatus(
      slice, options, {.ignoreUnknownFields = true});

  if (!res.ok()) {
    LOG_TOPIC("d88b8", WARN, arangodb::iresearch::TOPIC)
        << "Failed to deserialize options from JSON while constructing '"
        << arangodb::iresearch::AqlAnalyzer::type_name()
        << "' analyzer, error: '" << res.error() << "' path: " << res.path();
    return false;
  }
  return true;
}

bool normalize_slice(VPackSlice const& slice, VPackBuilder& builder) {
  arangodb::iresearch::AqlAnalyzer::Options options;
  if (parse_options_slice(slice, options)) {
    VPackObjectBuilder root(&builder);
    builder.add(QUERY_STRING_PARAM_NAME, VPackValue(options.queryString));
    builder.add(COLLAPSE_ARRAY_POSITIONS_PARAM_NAME,
                VPackValue(options.collapsePositions));
    builder.add(KEEP_NULL_PARAM_NAME, VPackValue(options.keepNull));
    builder.add(BATCH_SIZE_PARAM_NAME, VPackValue(options.batchSize));
    builder.add(MEMORY_LIMIT_PARAM_NAME, VPackValue(options.memoryLimit));
    switch (options.returnType) {
      case arangodb::iresearch::AnalyzerValueType::String:
        builder.add(
            RETURN_TYPE_PARAM_NAME,
            VPackValue(arangodb::iresearch::ANALYZER_VALUE_TYPE_STRING));
        break;
      case arangodb::iresearch::AnalyzerValueType::Number:
        builder.add(
            RETURN_TYPE_PARAM_NAME,
            VPackValue(arangodb::iresearch::ANALYZER_VALUE_TYPE_NUMBER));
        break;
      case arangodb::iresearch::AnalyzerValueType::Bool:
        builder.add(RETURN_TYPE_PARAM_NAME,
                    VPackValue(arangodb::iresearch::ANALYZER_VALUE_TYPE_BOOL));
        break;
      default:
        TRI_ASSERT(false);
    }
    return true;
  }
  return false;
}

irs::analysis::analyzer::ptr make_slice(VPackSlice slice) {
  arangodb::iresearch::AqlAnalyzer::Options options;
  if (parse_options_slice(slice, options)) {
    auto validationRes = arangodb::aql::StandaloneCalculation::validateQuery(
        arangodb::DatabaseFeature::getCalculationVocbase(), options.queryString,
        CALCULATION_PARAMETER_NAME, " in aql analyzer",
        arangodb::transaction::OperationOriginInternal{
            "validating AQL analyzer"},
        /*isComputedValue*/ false);
    if (validationRes.ok()) {
      return std::make_unique<arangodb::iresearch::AqlAnalyzer>(options);
    } else {
      LOG_TOPIC("f775e", WARN, arangodb::iresearch::TOPIC)
          << "error validating calculation query: "
          << validationRes.errorMessage();
    }
  }
  return nullptr;
}

ExecutionNode* getCalcNode(ExecutionNode* node) {
  // get calculation node which satisfy the requirements
  if (node == nullptr || node->getType() != ExecutionNode::RETURN) {
    return nullptr;
  }

  auto& deps = node->getDependencies();
  if (deps.size() == 1 &&
      deps[0]->getType() == ExecutionNode::NodeType::CALCULATION) {
    auto calcNode = deps[0];

    auto& deps2 = calcNode->getDependencies();
    if (deps2.size() == 1 &&
        deps2[0]->getType() == ExecutionNode::NodeType::SINGLETON) {
      return calcNode;
    }
  }

  return nullptr;
}
}  // namespace

namespace arangodb {
namespace iresearch {

bool AqlAnalyzer::normalize_vpack(std::string_view args, std::string& out) {
  auto const slice = arangodb::iresearch::slice(args);
  VPackBuilder builder;
  if (normalize_slice(slice, builder)) {
    out.resize(builder.slice().byteSize());
    std::memcpy(&out[0], builder.slice().begin(), out.size());
    return true;
  }
  return false;
}

bool AqlAnalyzer::normalize_json(std::string_view args, std::string& out) {
  auto src = VPackParser::fromJson(args.data(), args.size());
  VPackBuilder builder;
  if (normalize_slice(src->slice(), builder)) {
    out = builder.toString();
    return true;
  }
  return false;
}

irs::analysis::analyzer::ptr AqlAnalyzer::make_vpack(std::string_view args) {
  auto const slice = arangodb::iresearch::slice(args);
  return make_slice(slice);
}

irs::analysis::analyzer::ptr AqlAnalyzer::make_json(std::string_view args) {
  auto builder = VPackParser::fromJson(args.data(), args.size());
  return make_slice(builder->slice());
}

bool tryOptimize(AqlAnalyzer* analyzer) {
  ExecutionNode* execNode = getCalcNode(analyzer->_plan->root());
  if (execNode != nullptr) {
    TRI_ASSERT(execNode->getType() == ExecutionNode::NodeType::CALCULATION);
    analyzer->_nodeToOptimize = static_cast<CalculationNode*>(execNode);
    // allocate memory for result
    analyzer->_queryResults = analyzer->_itemBlockManager.requestBlock(1, 1);
    return true;
  }

  return false;
}

void resetFromExpression(AqlAnalyzer* analyzer) {
  auto e = analyzer->_nodeToOptimize->expression();

  auto& trx = analyzer->_query->trxForOptimization();

  auto& query = analyzer->_query->ast()->query();

  // create context
  // value is not needed since getting it from _bindedNodes
  NoVarExpressionContext ctx(trx, query, analyzer->_aqlFunctionsInternalCache);

  analyzer->_executionState = ExecutionState::DONE;  // already calculated

  // put calculated value in _queryResults
  analyzer->_queryResults->destroyValue(0, 0);
  bool mustDestroy = true;

  analyzer->_queryResults->setValue(0, 0, e->execute(&ctx, mustDestroy));

  analyzer->_engineResultRegister = 0;
}

void resetFromQuery(AqlAnalyzer* analyzer) {
  analyzer->_queryResults = nullptr;
  analyzer->_plan->clearVarUsageComputed();
  analyzer->_aqlFunctionsInternalCache.clear();
  analyzer->_engine.initFromPlanForCalculation(*analyzer->_plan);
  analyzer->_executionState = ExecutionState::HASMORE;
  analyzer->_engineResultRegister = analyzer->_engine.resultRegister();
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
bool AqlAnalyzer::isOptimized() const {
  return _resetImpl == &resetFromExpression;
}
#endif

AqlAnalyzer::AqlAnalyzer(Options const& options)
    : _options(options),
      _query(arangodb::aql::StandaloneCalculation::buildQueryContext(
          arangodb::DatabaseFeature::getCalculationVocbase(),
          transaction::OperationOriginInternal{"AQL analyzer"})),
      _itemBlockManager(_query->resourceMonitor()),
      _engine(0, *_query, _itemBlockManager,
              std::make_shared<SharedQueryState>(_query->vocbase().server())),
      _resetImpl(&resetFromQuery) {
  _query->resourceMonitor().memoryLimit(_options.memoryLimit);
  std::get<AnalyzerValueTypeAttribute>(_attrs).value = _options.returnType;
  TRI_ASSERT(
      arangodb::aql::StandaloneCalculation::validateQuery(
          arangodb::DatabaseFeature::getCalculationVocbase(),
          _options.queryString, CALCULATION_PARAMETER_NAME, " in aql analyzer",
          transaction::OperationOriginInternal{"validating AQL analyzer"},
          /*isComputedValue*/ false)
          .ok());
}

bool AqlAnalyzer::next() {
  do {
    if (_queryResults != nullptr) {
      while (_queryResults->numRows() > _resultRowIdx) {
        AqlValue const& value = _queryResults->getValueReference(
            _resultRowIdx++, _engineResultRegister);
        if (_options.keepNull || !value.isNull(true)) {
          switch (_options.returnType) {
            case AnalyzerValueType::String:
              if (value.isString()) {
                std::get<2>(_attrs).value =
                    arangodb::iresearch::getBytesRef(value.slice());
              } else {
                functions::VPackFunctionParameters params;
                params.push_back(value);
                aql::NoVarExpressionContext ctx(_query->trxForOptimization(),
                                                *_query,
                                                _aqlFunctionsInternalCache);
                _valueBuffer = aql::functions::ToString(
                    &ctx, *_query->ast()->root(), params);
                TRI_ASSERT(_valueBuffer.isString());
                std::get<2>(_attrs).value = irs::ViewCast<irs::byte_type>(
                    _valueBuffer.slice().stringView());
              }
              break;
            case AnalyzerValueType::Number:
              if (value.isNumber()) {
                std::get<3>(_attrs).value = value.slice();
              } else {
                functions::VPackFunctionParameters params;
                params.push_back(value);
                aql::NoVarExpressionContext ctx(_query->trxForOptimization(),
                                                *_query,
                                                _aqlFunctionsInternalCache);
                _valueBuffer = aql::functions::ToNumber(
                    &ctx, *_query->ast()->root(), params);
                TRI_ASSERT(_valueBuffer.isNumber());
                std::get<3>(_attrs).value = _valueBuffer.slice();
              }
              break;
            case AnalyzerValueType::Bool:
              if (value.isBoolean()) {
                std::get<3>(_attrs).value = value.slice();
              } else {
                functions::VPackFunctionParameters params;
                params.push_back(value);
                aql::NoVarExpressionContext ctx(_query->trxForOptimization(),
                                                *_query,
                                                _aqlFunctionsInternalCache);
                _valueBuffer = aql::functions::ToBool(
                    &ctx, *_query->ast()->root(), params);
                TRI_ASSERT(_valueBuffer.isBoolean());
                std::get<3>(_attrs).value = _valueBuffer.slice();
              }
              break;
            default:
              // new return type added?
              TRI_ASSERT(false);
              LOG_TOPIC("a9ba5", WARN, iresearch::TOPIC)
                  << "Unexpected AqlAnalyzer return type "
                  << static_cast<std::underlying_type_t<AnalyzerValueType>>(
                         _options.returnType);
              std::get<2>(_attrs).value = irs::kEmptyStringView<irs::byte_type>;
              _valueBuffer = AqlValue();
              std::get<3>(_attrs).value = _valueBuffer.slice();
          }
          std::get<0>(_attrs).value = _nextIncVal;
          _nextIncVal = !_options.collapsePositions;
          return true;
        }
      }
    }
    if (_executionState == ExecutionState::HASMORE) {
      _executionState = ExecutionState::DONE;  // set to done to terminate in
                                               // case of exception
      _resultRowIdx = 0;
      _queryResults = nullptr;
      try {
        AqlCallStack aqlStack{AqlCallList{
            AqlCall{/*offset*/ 0, /*softLimit*/ _options.batchSize,
                    /*hardLimit*/ AqlCall::Infinity{}, /*fullCount*/ false}}};
        SkipResult skip;
        std::tie(_executionState, skip, _queryResults) =
            _engine.execute(aqlStack);
        TRI_ASSERT(skip.nothingSkipped());
        TRI_ASSERT(_executionState != ExecutionState::WAITING);
      } catch (std::exception const& e) {
        LOG_TOPIC("c92eb", WARN, iresearch::TOPIC)
            << "error executing calculation query: " << e.what()
            << " AQL query: " << _options.queryString;
      } catch (...) {
        LOG_TOPIC("bf89b", WARN, iresearch::TOPIC)
            << "error executing calculation query"
            << " AQL query: " << _options.queryString;
      }
    }
  } while (
      _executionState != ExecutionState::DONE ||
      (_queryResults != nullptr && _queryResults->numRows() > _resultRowIdx));
  return false;
}

bool AqlAnalyzer::reset(std::string_view field) noexcept {
  try {
    if (!_plan) {  // lazy initialization
      // important to hold a copy here as parser accepts reference!
      auto queryString = arangodb::aql::QueryString(_options.queryString);
      auto ast = _query->ast();
      TRI_ASSERT(ast);
      Parser parser(*_query, *ast, queryString);
      parser.parse();
      AstNode* astRoot = const_cast<AstNode*>(ast->root());
      TRI_ASSERT(astRoot);
      Ast::traverseAndModify(
          astRoot, [this, field, ast](AstNode* node) -> AstNode* {
            if (node->type == NODE_TYPE_PARAMETER) {
              // should be only our parameter name. see validation method!
              TRI_ASSERT(node->getStringView() == CALCULATION_PARAMETER_NAME);
              // FIXME: move to computed value once here could be not only
              // strings
              auto newNode =
                  ast->createNodeValueMutableString(field.data(), field.size());
              // finally note that the node was created from a bind parameter
              newNode->setFlag(FLAG_BIND_PARAMETER);
              newNode->setFlag(
                  DETERMINED_CONSTANT);  // keep value as non-constant to
                                         // prevent optimizations
              newNode->setFlag(DETERMINED_NONDETERMINISTIC,
                               VALUE_NONDETERMINISTIC);
              _bindedNodes.push_back(newNode);
              return newNode;
            } else {
              return node;
            }
          });
      // we have to set "optimizeNonCacheable" to false here, so that the
      // queryString expression gets re-evaluated every time, and does not
      // store the computed results once (e.g. when using a queryString such
      // as "RETURN DATE_NOW()" you always want the current date to be
      // returned, and not a date once stored)
      ast->validateAndOptimize(_query->trxForOptimization(),
                               {.optimizeNonCacheable = false});

      std::unique_ptr<ExecutionPlan> plan =
          ExecutionPlan::instantiateFromAst(ast, true);

      // run the plan through the optimizer, executing only the absolutely
      // necessary optimizer rules (we skip all other rules to save time). we
      // have to execute the "splice-subqueries" rule here so we replace all
      // SubqueryNodes with SubqueryStartNodes and SubqueryEndNodes.
      Optimizer optimizer(_query->resourceMonitor(), 1);
      // disable all rules which are not necessary
      optimizer.initializeRules(plan.get(), _query->queryOptions());
      optimizer.disableRules(plan.get(), [](OptimizerRule const& rule) -> bool {
        return rule.canBeDisabled() || rule.isClusterOnly();
      });
      optimizer.createPlans(std::move(plan), _query->queryOptions(), false);

      _plan = optimizer.stealBest();
      TRI_ASSERT(
          !_plan->hasAppliedRule(OptimizerRule::RuleLevel::asyncPrefetchRule));

      // try to optimize
      if (tryOptimize(this)) {
        _resetImpl = &resetFromExpression;
      }
    }

    for (auto node : _bindedNodes) {
      node->setStringValue(field.data(), field.size());
    }

    _resultRowIdx = 0;
    _nextIncVal = 1;  // first increment always 1 to move from -1 to 0
    _engine.reset();

    _resetImpl(this);
    return true;

  } catch (std::exception const& e) {
    LOG_TOPIC("d2223", WARN, iresearch::TOPIC)
        << "error creating calculation query: " << e.what()
        << " AQL query: " << _options.queryString;
  } catch (...) {
    LOG_TOPIC("5ad87", WARN, iresearch::TOPIC)
        << "error creating calculation query"
        << " AQL query: " << _options.queryString;
  }
  return false;
}
}  // namespace iresearch
}  // namespace arangodb
