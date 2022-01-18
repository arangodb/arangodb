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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

// otherwise define conflict between 3rdParty\date\include\date\date.h and
// 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
#include "date/date.h"
#endif

#include "IResearchAqlAnalyzer.h"

#include "utils/hash_utils.hpp"
#include "utils/object_pool.hpp"

#include "Aql/AqlCallList.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Expression.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Aql/Parser.h"
#include "Aql/QueryString.h"
#include "Aql/StandaloneCalculation.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/FunctionUtils.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/SmartContext.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/vocbase.h"

#include <Containers/HashSet.h>
#include "VPackDeserializer/deserializer.h"

namespace {
using namespace arangodb::velocypack::deserializer;
using namespace arangodb::aql;

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

using Options = arangodb::iresearch::AqlAnalyzer::Options;

struct OptionsValidator {
  std::optional<deserialize_error> operator()(Options const& opts) const {
    if (opts.queryString.empty()) {
      return deserialize_error{std::string("Value of '")
                                   .append(QUERY_STRING_PARAM_NAME)
                                   .append("' should be non empty string")};
    }
    if (opts.batchSize == 0) {
      return deserialize_error{std::string("Value of '")
                                   .append(BATCH_SIZE_PARAM_NAME)
                                   .append("' should be greater than 0")};
    }
    if (opts.batchSize > MAX_BATCH_SIZE) {
      return deserialize_error{std::string("Value of '")
                                   .append(BATCH_SIZE_PARAM_NAME)
                                   .append("' should be less or equal to ")
                                   .append(std::to_string(MAX_BATCH_SIZE))};
    }
    if (opts.memoryLimit == 0) {
      return deserialize_error{std::string("Value of '")
                                   .append(MEMORY_LIMIT_PARAM_NAME)
                                   .append("' should be greater than 0")};
    }
    if (opts.memoryLimit > MAX_MEMORY_LIMIT) {
      return deserialize_error{std::string("Value of '")
                                   .append(MEMORY_LIMIT_PARAM_NAME)
                                   .append("' should be less or equal to ")
                                   .append(std::to_string(MAX_MEMORY_LIMIT))};
    }
    if (opts.returnType != arangodb::iresearch::AnalyzerValueType::String &&
        opts.returnType != arangodb::iresearch::AnalyzerValueType::Number &&
        opts.returnType != arangodb::iresearch::AnalyzerValueType::Bool) {
      return deserialize_error{
          std::string("Value of '")
              .append(RETURN_TYPE_PARAM_NAME)
              .append("' should be ")
              .append(arangodb::iresearch::ANALYZER_VALUE_TYPE_STRING)
              .append(" or ")
              .append(arangodb::iresearch::ANALYZER_VALUE_TYPE_NUMBER)
              .append(" or ")
              .append(arangodb::iresearch::ANALYZER_VALUE_TYPE_BOOL)};
    }
    return {};
  }
};

using OptionsDeserializer = utilities::constructing_deserializer<
    Options,
    parameter_list<
        factory_deserialized_parameter<QUERY_STRING_PARAM_NAME,
                                       values::value_deserializer<std::string>,
                                       true>,
        factory_simple_parameter<COLLAPSE_ARRAY_POSITIONS_PARAM_NAME, bool,
                                 false, values::numeric_value<bool, false>>,
        factory_simple_parameter<KEEP_NULL_PARAM_NAME, bool, false,
                                 values::numeric_value<bool, true>>,
        factory_simple_parameter<BATCH_SIZE_PARAM_NAME, uint32_t, false,
                                 values::numeric_value<uint32_t, 10>>,
        factory_simple_parameter<MEMORY_LIMIT_PARAM_NAME, uint32_t, false,
                                 values::numeric_value<uint32_t, 1048576U>>,
        factory_deserialized_default<
            RETURN_TYPE_PARAM_NAME,
            arangodb::iresearch::AnalyzerValueTypeEnumDeserializer,
            values::numeric_value<
                arangodb::iresearch::AnalyzerValueType,
                static_cast<std::underlying_type_t<
                    arangodb::iresearch::AnalyzerValueType>>(
                    arangodb::iresearch::AnalyzerValueType::String)>>>>;

using ValidatingOptionsDeserializer =
    validate<OptionsDeserializer, OptionsValidator>;

bool parse_options_slice(VPackSlice const& slice,
                         arangodb::iresearch::AqlAnalyzer::Options& options) {
  auto const res = deserialize<ValidatingOptionsDeserializer,
                               hints::hint_list<hints::ignore_unknown>>(slice);
  if (!res.ok()) {
    LOG_TOPIC("d88b8", WARN, arangodb::iresearch::TOPIC)
        << "Failed to deserialize options from JSON while constructing '"
        << arangodb::iresearch::AqlAnalyzer::type_name()
        << "' analyzer, error: '" << res.error().message << "'";
    return false;
  }
  options = res.get();
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

irs::analysis::analyzer::ptr make_slice(VPackSlice const& slice) {
  arangodb::iresearch::AqlAnalyzer::Options options;
  if (parse_options_slice(slice, options)) {
    auto validationRes = arangodb::aql::StandaloneCalculation::validateQuery(
        arangodb::DatabaseFeature::getCalculationVocbase(), options.queryString,
        CALCULATION_PARAMETER_NAME, " in aql analyzer");
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

/*static*/ bool AqlAnalyzer::normalize_vpack(const irs::string_ref& args,
                                             std::string& out) {
  auto const slice = arangodb::iresearch::slice(args);
  VPackBuilder builder;
  if (normalize_slice(slice, builder)) {
    out.resize(builder.slice().byteSize());
    std::memcpy(&out[0], builder.slice().begin(), out.size());
    return true;
  }
  return false;
}

/*static*/ bool AqlAnalyzer::normalize_json(const irs::string_ref& args,
                                            std::string& out) {
  auto src = VPackParser::fromJson(args.c_str(), args.size());
  VPackBuilder builder;
  if (normalize_slice(src->slice(), builder)) {
    out = builder.toString();
    return true;
  }
  return false;
}

/*static*/ irs::analysis::analyzer::ptr AqlAnalyzer::make_vpack(
    irs::string_ref const& args) {
  auto const slice = arangodb::iresearch::slice(args);
  return make_slice(slice);
}

/*static*/ irs::analysis::analyzer::ptr AqlAnalyzer::make_json(
    irs::string_ref const& args) {
  auto builder = VPackParser::fromJson(args.c_str(), args.size());
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
  SingleVarExpressionContext ctx(trx, query,
                                 analyzer->_aqlFunctionsInternalCache);

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
    : irs::analysis::analyzer(irs::type<AqlAnalyzer>::get()),
      _options(options),
      _query(arangodb::aql::StandaloneCalculation::buildQueryContext(
          arangodb::DatabaseFeature::getCalculationVocbase())),
      _itemBlockManager(_query->resourceMonitor(),
                        SerializationFormat::SHADOWROWS),
      _engine(0, *_query, _itemBlockManager, SerializationFormat::SHADOWROWS,
              nullptr),
      _resetImpl(&resetFromQuery) {
  _query->resourceMonitor().memoryLimit(_options.memoryLimit);
  std::get<AnalyzerValueTypeAttribute>(_attrs).value = _options.returnType;
  TRI_ASSERT(arangodb::aql::StandaloneCalculation::validateQuery(
                 arangodb::DatabaseFeature::getCalculationVocbase(),
                 _options.queryString, CALCULATION_PARAMETER_NAME,
                 " in aql analyzer")
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
                VPackFunctionParameters params{_params_arena};
                params.push_back(value);
                aql::SingleVarExpressionContext ctx(
                    _query->trxForOptimization(), *_query,
                    _aqlFunctionsInternalCache);
                _valueBuffer = aql::Functions::ToString(
                    &ctx, *_query->ast()->root(), params);
                TRI_ASSERT(_valueBuffer.isString());
                std::get<2>(_attrs).value = irs::ref_cast<irs::byte_type>(
                    _valueBuffer.slice().stringView());
              }
              break;
            case AnalyzerValueType::Number:
              if (value.isNumber()) {
                std::get<3>(_attrs).value = value.slice();
              } else {
                VPackFunctionParameters params{_params_arena};
                params.push_back(value);
                aql::SingleVarExpressionContext ctx(
                    _query->trxForOptimization(), *_query,
                    _aqlFunctionsInternalCache);
                _valueBuffer = aql::Functions::ToNumber(
                    &ctx, *_query->ast()->root(), params);
                TRI_ASSERT(_valueBuffer.isNumber());
                std::get<3>(_attrs).value = _valueBuffer.slice();
              }
              break;
            case AnalyzerValueType::Bool:
              if (value.isBoolean()) {
                std::get<3>(_attrs).value = value.slice();
              } else {
                VPackFunctionParameters params{_params_arena};
                params.push_back(value);
                aql::SingleVarExpressionContext ctx(
                    _query->trxForOptimization(), *_query,
                    _aqlFunctionsInternalCache);
                _valueBuffer = aql::Functions::ToBool(
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
              std::get<2>(_attrs).value = irs::bytes_ref::EMPTY;
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
        AqlCallStack aqlStack{
            AqlCallList{AqlCall::SimulateGetSome(_options.batchSize)}};
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

bool AqlAnalyzer::reset(irs::string_ref const& field) noexcept {
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
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
              TRI_ASSERT(node->getStringView() == CALCULATION_PARAMETER_NAME);
#endif
              // FIXME: move to computed value once here could be not only
              // strings
              auto newNode = ast->createNodeValueMutableString(field.c_str(),
                                                               field.size());
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
      ast->validateAndOptimize(_query->trxForOptimization());

      std::unique_ptr<ExecutionPlan> plan =
          ExecutionPlan::instantiateFromAst(ast, true);

      // run the plan through the optimizer, executing only the absolutely
      // necessary optimizer rules (we skip all other rules to save time). we
      // have to execute the "splice-subqueries" rule here so we replace all
      // SubqueryNodes with SubqueryStartNodes and SubqueryEndNodes.
      Optimizer optimizer(1);
      // disable all rules which are not necessary
      optimizer.disableRules(plan.get(), [](OptimizerRule const& rule) -> bool {
        return rule.canBeDisabled() || rule.isClusterOnly();
      });
      optimizer.createPlans(std::move(plan), _query->queryOptions(), false);

      _plan = optimizer.stealBest();

      // try to optimize
      if (tryOptimize(this)) {
        _resetImpl = &resetFromExpression;
      }
    }

    for (auto node : _bindedNodes) {
      node->setStringValue(field.c_str(), field.size());
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
