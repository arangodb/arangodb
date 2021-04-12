////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/Ast.h"
#include "Aql/AqlCallList.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AqlTransaction.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Aql/Parser.h"
#include "Aql/QueryString.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/FunctionUtils.h"
#include "IResearchCommon.h"
#include "Logger/LogMacros.h"
#include "VelocyPackHelper.h"
#include "VocBase/vocbase.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/SmartContext.h"
#include "Utils/CollectionNameResolver.h"

#include <Containers/HashSet.h>
#include "VPackDeserializer/deserializer.h"
#include <frozen/set.h>

namespace {
using namespace arangodb::velocypack::deserializer;
using namespace arangodb::aql;

constexpr const char QUERY_STRING_PARAM_NAME[] = "queryString";
constexpr const char COLLAPSE_ARRAY_POSITIONS_PARAM_NAME[] = "collapsePositions";
constexpr const char KEEP_NULL_PARAM_NAME[] = "keepNull";
constexpr const char BATCH_SIZE_PARAM_NAME[] = "batchSize";
constexpr const char MEMORY_LIMIT_PARAM_NAME[] = "memoryLimit";
constexpr const char CALCULATION_PARAMETER_NAME[] = "param";
constexpr const char RETURN_TYPE_PARAM_NAME[] = "returnType";

constexpr const uint32_t MAX_BATCH_SIZE{1000};
constexpr const uint32_t MAX_MEMORY_LIMIT{33554432U}; // 32Mb

using Options = arangodb::iresearch::AqlAnalyzer::Options;

struct OptionsValidator {
  std::optional<deserialize_error> operator()(Options const& opts) const {
    if (opts.queryString.empty()) {
      return deserialize_error{std::string("Value of '").append(QUERY_STRING_PARAM_NAME).append("' should be non empty string")};
    }
    if (opts.batchSize == 0) {
      return deserialize_error{
          std::string("Value of '").append(BATCH_SIZE_PARAM_NAME).append("' should be greater than 0")};
    }
    if (opts.batchSize > MAX_BATCH_SIZE) {
      return deserialize_error{
          std::string("Value of '")
              .append(BATCH_SIZE_PARAM_NAME)
              .append("' should be less or equal to ")
              .append(std::to_string(MAX_BATCH_SIZE))};
    }
    if (opts.memoryLimit == 0) {
      return deserialize_error{
          std::string("Value of '").append(MEMORY_LIMIT_PARAM_NAME).append("' should be greater than 0")};
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
           std::string("Value of '").append(RETURN_TYPE_PARAM_NAME)
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

using OptionsDeserializer = utilities::constructing_deserializer<Options, parameter_list<
  factory_deserialized_parameter<QUERY_STRING_PARAM_NAME, values::value_deserializer<std::string>, true>,
  factory_simple_parameter<COLLAPSE_ARRAY_POSITIONS_PARAM_NAME, bool, false, values::numeric_value<bool, false>>,
  factory_simple_parameter<KEEP_NULL_PARAM_NAME, bool, false, values::numeric_value<bool, true>>,
  factory_simple_parameter<BATCH_SIZE_PARAM_NAME, uint32_t, false, values::numeric_value<uint32_t, 10>>,
  factory_simple_parameter<MEMORY_LIMIT_PARAM_NAME, uint32_t, false, values::numeric_value<uint32_t, 1048576U>>,
  factory_deserialized_default<RETURN_TYPE_PARAM_NAME,
                               arangodb::iresearch::AnalyzerValueTypeEnumDeserializer,
                               values::numeric_value<arangodb::iresearch::AnalyzerValueType,
                                   static_cast<std::underlying_type_t<arangodb::iresearch::AnalyzerValueType>>(
                                       arangodb::iresearch::AnalyzerValueType::String)>>
  >>;

using ValidatingOptionsDeserializer = validate<OptionsDeserializer, OptionsValidator>;

bool parse_options_slice(VPackSlice const& slice,
                         arangodb::iresearch::AqlAnalyzer::Options& options) {
  auto const res = deserialize<ValidatingOptionsDeserializer, hints::hint_list<hints::ignore_unknown>>(slice);
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
        builder.add(RETURN_TYPE_PARAM_NAME,
          VPackValue(arangodb::iresearch::ANALYZER_VALUE_TYPE_STRING));
        break;
      case arangodb::iresearch::AnalyzerValueType::Number:
        builder.add(RETURN_TYPE_PARAM_NAME,
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

/// @brief Dummy transaction state which does nothing but provides valid
/// statuses to keep ASSERT happy
class CalculationTransactionState final : public arangodb::TransactionState {
 public:
  explicit CalculationTransactionState(TRI_vocbase_t& vocbase)
      : TransactionState(vocbase, arangodb::TransactionId(0), arangodb::transaction::Options()) {
    updateStatus(arangodb::transaction::Status::RUNNING);  // always running to make ASSERTS happy
  }

  ~CalculationTransactionState() {
    if (status() == arangodb::transaction::Status::RUNNING) {
      updateStatus(arangodb::transaction::Status::ABORTED);  // simulate state changes to make ASSERTS happy
    }
  }
  /// @brief begin a transaction
  arangodb::Result beginTransaction(arangodb::transaction::Hints) override {
    return {};
  }

  /// @brief commit a transaction
  arangodb::Result commitTransaction(arangodb::transaction::Methods*) override {
    updateStatus(arangodb::transaction::Status::COMMITTED);  // simulate state changes to make ASSERTS happy
    return {};
  }

  /// @brief abort a transaction
  arangodb::Result abortTransaction(arangodb::transaction::Methods*) override {
    updateStatus(arangodb::transaction::Status::ABORTED);  // simulate state changes to make ASSERTS happy
    return {};
  }

  bool hasFailedOperations() const override { return false; }

  /// @brief number of commits, including intermediate commits
  uint64_t numCommits() const override { return 0; }
};

/// @brief Dummy transaction context which just gives dummy state
struct CalculationTransactionContext final : public arangodb::transaction::SmartContext {
  explicit CalculationTransactionContext(TRI_vocbase_t& vocbase)
      : SmartContext(vocbase, arangodb::transaction::Context::makeTransactionId(), nullptr),
        _state(vocbase) {}

  /// @brief get transaction state, determine commit responsiblity
  std::shared_ptr<arangodb::TransactionState> acquireState(arangodb::transaction::Options const& options,
                                                 bool& responsibleForCommit) override {
    return std::shared_ptr<arangodb::TransactionState>(std::shared_ptr<arangodb::TransactionState>(),
                                                       &_state);
  }

  /// @brief unregister the transaction
  void unregisterTransaction() noexcept override {}

  std::shared_ptr<Context> clone() const override {
    TRI_ASSERT(FALSE);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_NOT_IMPLEMENTED,
        "CalculationTransactionContext cloning is not implemented");
  }

 private:
  CalculationTransactionState _state;
};

class CalculationQueryContext final : public arangodb::aql::QueryContext {
 public:
  explicit CalculationQueryContext(TRI_vocbase_t& vocbase)
      : QueryContext(vocbase),
        _resolver(vocbase),
        _transactionContext(vocbase) {
    _ast = std::make_unique<Ast>(*this, NON_CONST_PARAMETERS);
    _trx = AqlTransaction::create(newTrxContext(), _collections, _queryOptions.transactionOptions,
                                  std::unordered_set<std::string>{});
    _trx->addHint(arangodb::transaction::Hints::Hint::FROM_TOPLEVEL_AQL);
    _trx->addHint(arangodb::transaction::Hints::Hint::SINGLE_OPERATION);  // to avoid taking db snapshot
    _trx->begin();
  }

  virtual arangodb::aql::QueryOptions const& queryOptions() const override {
    return _queryOptions;
  }

  double getLockTimeout() const noexcept override {
    return _queryOptions.transactionOptions.lockTimeout;
  }

  void setLockTimeout(double timeout) noexcept override {
    _queryOptions.transactionOptions.lockTimeout = timeout;
  }

  /// @brief pass-thru a resolver object from the transaction context
  virtual arangodb::CollectionNameResolver const& resolver() const override {
    return _resolver;
  }

  virtual arangodb::velocypack::Options const& vpackOptions() const override {
    return arangodb::velocypack::Options::Defaults;
  }

  /// @brief create a transaction::Context
  virtual std::shared_ptr<arangodb::transaction::Context> newTrxContext() const override {
    return std::shared_ptr<arangodb::transaction::Context>(
        std::shared_ptr<arangodb::transaction::Context>(), &_transactionContext);
  }

  virtual arangodb::transaction::Methods& trxForOptimization() override {
    return *_trx;
  }

  virtual bool killed() const override { return false; }

  /// @brief whether or not a query is a modification query
  virtual bool isModificationQuery() const noexcept override { return false; }

  virtual bool isAsyncQuery() const noexcept override { return false; }

  virtual void enterV8Context() override {
    TRI_ASSERT(FALSE);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_NOT_IMPLEMENTED,
        "CalculationQueryContext entering V8 context is not implemented");
  }

 private:
  arangodb::aql::QueryOptions _queryOptions;
  arangodb::CollectionNameResolver _resolver;
  mutable CalculationTransactionContext _transactionContext;
  std::unique_ptr<arangodb::transaction::Methods> _trx;
};

frozen::set<irs::string_ref, 4> forbiddenFunctions{"TOKENS", "NGRAM_MATCH",
                                                   "PHRASE", "ANALYZER"};

arangodb::Result validateQuery(std::string const& queryStringRaw, TRI_vocbase_t& vocbase) {
  try {
    CalculationQueryContext queryContext(vocbase);
    auto queryString = arangodb::aql::QueryString(queryStringRaw);
    auto ast = queryContext.ast();
    TRI_ASSERT(ast);
    Parser parser(queryContext, *ast, queryString);
    parser.parse();
    ast->validateAndOptimize(queryContext.trxForOptimization());
    AstNode* astRoot = const_cast<AstNode*>(ast->root());
    TRI_ASSERT(astRoot);
    // Forbid all V8 related stuff as it is not available on DBServers where analyzers run.
    if (ast->willUseV8()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "V8 usage is forbidden for aql analyzer"};
    }

    // no modification (as data access is forbidden) but to give more clear error message
    if (ast->containsModificationNode()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "DML is forbidden for aql analyzer"};
    }

    // no traversal (also data access is forbidden) but to give more clear error message
    if (ast->containsTraversal()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Traversal usage is forbidden for aql analyzer"};
    }

    std::string errorMessage;
    // Forbid to use functions that reference analyzers -> problems on recovery as analyzers are not available for querying.
    // Forbid all non-Dbserver runnable functions as it is not available on DBServers where analyzers run.
    arangodb::aql::Ast::traverseReadOnly(
        ast->root(), [&errorMessage](arangodb::aql::AstNode const* node) -> bool {
          TRI_ASSERT(node);
          switch (node->type) {
            // these nodes are ok unconditionally
            case arangodb::aql::NODE_TYPE_ROOT:
            case arangodb::aql::NODE_TYPE_FOR:
            case arangodb::aql::NODE_TYPE_LET:
            case arangodb::aql::NODE_TYPE_FILTER:
            case arangodb::aql::NODE_TYPE_ARRAY:
            case arangodb::aql::NODE_TYPE_RETURN:
            case arangodb::aql::NODE_TYPE_SORT:
            case arangodb::aql::NODE_TYPE_SORT_ELEMENT:
            case arangodb::aql::NODE_TYPE_LIMIT:
            case arangodb::aql::NODE_TYPE_VARIABLE:
            case arangodb::aql::NODE_TYPE_ASSIGN:
            case arangodb::aql::NODE_TYPE_OPERATOR_UNARY_PLUS:
            case arangodb::aql::NODE_TYPE_OPERATOR_UNARY_MINUS:
            case arangodb::aql::NODE_TYPE_OPERATOR_UNARY_NOT:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_OR:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_PLUS:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_MINUS:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_TIMES:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_DIV:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_MOD:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN:
            case arangodb::aql::NODE_TYPE_OPERATOR_TERNARY:
            case arangodb::aql::NODE_TYPE_SUBQUERY:
            case arangodb::aql::NODE_TYPE_EXPANSION:
            case arangodb::aql::NODE_TYPE_ITERATOR:
            case arangodb::aql::NODE_TYPE_VALUE:
            case arangodb::aql::NODE_TYPE_OBJECT:
            case arangodb::aql::NODE_TYPE_OBJECT_ELEMENT:
            case arangodb::aql::NODE_TYPE_REFERENCE:
            case arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS:
            case arangodb::aql::NODE_TYPE_BOUND_ATTRIBUTE_ACCESS:
            case arangodb::aql::NODE_TYPE_RANGE:
            case arangodb::aql::NODE_TYPE_NOP:
            case arangodb::aql::NODE_TYPE_CALCULATED_OBJECT_ELEMENT:
            case arangodb::aql::NODE_TYPE_PASSTHRU:
            case arangodb::aql::NODE_TYPE_ARRAY_LIMIT:
            case arangodb::aql::NODE_TYPE_DISTINCT:
            case arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND:
            case arangodb::aql::NODE_TYPE_OPERATOR_NARY_OR:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
            case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
            case arangodb::aql::NODE_TYPE_QUANTIFIER:
              break;
            // some nodes are ok with restrictions
            case arangodb::aql::NODE_TYPE_FCALL: {
              auto func = static_cast<arangodb::aql::Function*>(node->getData());
              if (!func->hasFlag(arangodb::aql::Function::Flags::CanRunOnDBServerCluster) ||
                  !func->hasFlag(arangodb::aql::Function::Flags::CanRunOnDBServerOneShard) ||
                  func->hasFlag(arangodb::aql::Function::Flags::CanReadDocuments) ||
                  forbiddenFunctions.find(func->name) != forbiddenFunctions.end()) {
                errorMessage = "Function '";
                errorMessage.append(func->name)
                    .append("' is forbidden for aql analyzer");
                return false;
              }
            } break;
            case arangodb::aql::NODE_TYPE_PARAMETER: {
              irs::string_ref parameterName(node->getStringValue(), node->getStringLength());
              if (parameterName != CALCULATION_PARAMETER_NAME) {
                errorMessage = "Invalid bind parameter found '";
                errorMessage.append(parameterName).append("'");
                return false;
              }
            } break;
            // by default all is forbidden
            default:
              errorMessage = "Node type '";
              errorMessage.append(node->getTypeString()).append("' is forbidden for aql analyzer");
              return false;
          }
          return true;
        });
    if (!errorMessage.empty()) {
      return {TRI_ERROR_BAD_PARAMETER, errorMessage};
    }
  } catch (arangodb::basics::Exception const& e) {
    return {TRI_ERROR_QUERY_PARSE, e.message()};
  } catch (std::exception const& e) {
    return {TRI_ERROR_QUERY_PARSE, e.what()};
  } catch (...) {
    TRI_ASSERT(FALSE);
    return {TRI_ERROR_QUERY_PARSE, "Unexpected"};
  }
  return {};
}

irs::analysis::analyzer::ptr make_slice(VPackSlice const& slice) {
  arangodb::iresearch::AqlAnalyzer::Options options;
  if (parse_options_slice(slice, options)) {
    auto validationRes =
        validateQuery(options.queryString,
                      arangodb::DatabaseFeature::getCalculationVocbase());
    if (validationRes.ok()) {
      return std::make_shared<arangodb::iresearch::AqlAnalyzer>(options);
    } else {
      LOG_TOPIC("f775e", WARN, arangodb::iresearch::TOPIC)
          << "error validating calculation query: " << validationRes.errorMessage();
    }
  }
  return nullptr;
}
}  // namespace

namespace arangodb {
namespace iresearch {

/*static*/ bool AqlAnalyzer::normalize_vpack(const irs::string_ref& args, std::string& out) {
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

/*static*/ irs::analysis::analyzer::ptr AqlAnalyzer::make_vpack(irs::string_ref const& args) {
  auto const slice = arangodb::iresearch::slice(args);
  return make_slice(slice);
}


/*static*/ irs::analysis::analyzer::ptr AqlAnalyzer::make_json(irs::string_ref const& args) {
  auto builder = VPackParser::fromJson(args.c_str(), args.size());
  return make_slice(builder->slice());
}

AqlAnalyzer::AqlAnalyzer(Options const& options)
  : irs::analysis::analyzer(irs::type<AqlAnalyzer>::get()),
    _options(options),
    _query(new CalculationQueryContext(arangodb::DatabaseFeature::getCalculationVocbase())),
    _itemBlockManager(_query->resourceMonitor(), SerializationFormat::SHADOWROWS),
    _engine(0, *_query, _itemBlockManager,
            SerializationFormat::SHADOWROWS, nullptr) {
  _query->resourceMonitor().memoryLimit(_options.memoryLimit);
  std::get<AnalyzerValueTypeAttribute>(_attrs).value = _options.returnType;
  TRI_ASSERT(validateQuery(_options.queryString,
                           arangodb::DatabaseFeature::getCalculationVocbase())
                 .ok());
}

bool AqlAnalyzer::next() {
  do {
    if (_queryResults != nullptr) {
      while (_queryResults->numRows() > _resultRowIdx) {
        AqlValue const& value =
            _queryResults->getValueReference(_resultRowIdx++, _engine.resultRegister());
        if (_options.keepNull || !value.isNull(true)) {
          switch (_options.returnType) {
            case AnalyzerValueType::String:
              if (value.isString()) {
                std::get<2>(_attrs).value = arangodb::iresearch::getBytesRef(value.slice());
              } else {
                VPackFunctionParameters params{_params_arena};
                params.push_back(value);
                aql::FixedVarExpressionContext ctx(_query->trxForOptimization(), *_query, _aqlFunctionsInternalCache);
                _valueBuffer = aql::Functions::ToString(&ctx, *_query->ast()->root(), params);
                TRI_ASSERT(_valueBuffer.isString());
                std::get<2>(_attrs).value = irs::ref_cast<irs::byte_type>(_valueBuffer.slice().stringView());
              }
              break;
            case AnalyzerValueType::Number:
              if (value.isNumber()) {
                std::get<3>(_attrs).value = value.slice();
              } else {
                VPackFunctionParameters params{_params_arena};
                params.push_back(value);
                aql::FixedVarExpressionContext ctx(_query->trxForOptimization(), *_query, _aqlFunctionsInternalCache);
                _valueBuffer = aql::Functions::ToNumber(&ctx, *_query->ast()->root(), params);
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
                aql::FixedVarExpressionContext ctx(_query->trxForOptimization(), *_query, _aqlFunctionsInternalCache);
                _valueBuffer = aql::Functions::ToBool(&ctx, *_query->ast()->root(), params);
                TRI_ASSERT(_valueBuffer.isBoolean());
                std::get<3>(_attrs).value = _valueBuffer.slice();
              }
              break;
            default:
              // new return type added?
              TRI_ASSERT(false);
              LOG_TOPIC("a9ba5", WARN, iresearch::TOPIC) << "Unexpected AqlAnalyzer return type " <<
                static_cast<std::underlying_type_t<AnalyzerValueType>>(_options.returnType);
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
      _executionState = ExecutionState::DONE;  // set to done to terminate in case of exception
      _resultRowIdx = 0;
      _queryResults = nullptr;
      try {
        AqlCallStack aqlStack{AqlCallList{AqlCall::SimulateGetSome(_options.batchSize)}};
        SkipResult skip;
        std::tie(_executionState, skip, _queryResults) = _engine.execute(aqlStack);
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
  } while (_executionState != ExecutionState::DONE || (_queryResults != nullptr &&
                                                       _queryResults->numRows() > _resultRowIdx));
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
      Ast::traverseAndModify(astRoot, [this, field, ast](AstNode* node) -> AstNode* {
        if (node->type == NODE_TYPE_PARAMETER) {
          // should be only our parameter name. see validation method!
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          TRI_ASSERT(irs::string_ref(node->getStringValue(), node->getStringLength()) ==
                     CALCULATION_PARAMETER_NAME);
#endif
          // FIXME: move to computed value once here could be not only strings
          auto newNode = ast->createNodeValueMutableString(field.c_str(), field.size());
          // finally note that the node was created from a bind parameter
          newNode->setFlag(FLAG_BIND_PARAMETER);
          newNode->setFlag(DETERMINED_CONSTANT);  // keep value as non-constant to prevent optimizations
          newNode->setFlag(DETERMINED_NONDETERMINISTIC, VALUE_NONDETERMINISTIC);
          _bindedNodes.push_back(newNode);
          return newNode;
        } else {
          return node;
        }
      });
      ast->validateAndOptimize(_query->trxForOptimization());

      std::unique_ptr<ExecutionPlan> plan = ExecutionPlan::instantiateFromAst(ast, true);

      // run the plan through the optimizer, executing only the absolutely necessary
      // optimizer rules (we skip all other rules to save time). we have to execute
      // the "splice-subqueries" rule here so we replace all SubqueryNodes with
      // SubqueryStartNodes and SubqueryEndNodes.
      Optimizer optimizer(1);
      // disable all rules which are not necessary
      optimizer.disableRules(plan.get(), [](OptimizerRule const& rule) -> bool {
        return rule.canBeDisabled() || rule.isClusterOnly();
      });
      optimizer.createPlans(std::move(plan), _query->queryOptions(), false);

      _plan = optimizer.stealBest();
    } else {
      for (auto node : _bindedNodes) {
        node->setStringValue(field.c_str(), field.size());
      }
      _engine.reset();
    }
    _queryResults = nullptr;
    _plan->clearVarUsageComputed();
    _aqlFunctionsInternalCache.clear();
    _engine.initFromPlanForCalculation(*_plan);
    _executionState = ExecutionState::HASMORE;
    _resultRowIdx = 0;
    _nextIncVal = 1;  // first increment always 1 to move from -1 to 0
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
} // namespace arangodb
