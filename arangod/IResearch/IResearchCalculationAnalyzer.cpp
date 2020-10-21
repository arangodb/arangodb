////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "analysis/analyzers.hpp"
#include "utils/hash_utils.hpp"
#include "utils/object_pool.hpp"

#include "Aql/Ast.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AqlTransaction.h"
#include "Aql/ExpressionContext.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Parser.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/FunctionUtils.h"
#include "IResearchCommon.h"
#include "IResearchCalculationAnalyzer.h"
#include "Logger/LogMacros.h"
#include "VelocyPackHelper.h"
#include "VocBase/vocbase.h"
#include "RestServer/DatabaseFeature.h"

#include <Containers/HashSet.h>
#include "VPackDeserializer/deserializer.h"
#include <frozen/set.h>

namespace {
using namespace arangodb::velocypack::deserializer;
using namespace arangodb::aql;

constexpr const char QUERY_STRING_PARAM_NAME[] = "queryString";
constexpr const char COLLAPSE_ARRAY_POSITIONS_PARAM_NAME[] = "collapseArrayPos";
constexpr const char KEEP_NULL_PARAM_NAME[] = "keepNull";
constexpr const char BATCH_SIZE_PARAM_NAME[] = "batchSize";
constexpr const char CALCULATION_PARAMETER_NAME[] = "field";

constexpr const uint32_t MAX_BATCH_SIZE{1000};


using Options = arangodb::iresearch::CalculationAnalyzer::options_t;

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
    return {};
  }
};

using OptionsDeserializer = utilities::constructing_deserializer<Options, parameter_list<
  factory_deserialized_parameter<QUERY_STRING_PARAM_NAME, values::value_deserializer<std::string>, true>,
  factory_simple_parameter<COLLAPSE_ARRAY_POSITIONS_PARAM_NAME, bool, false, values::numeric_value<bool, false>>,
  factory_simple_parameter<KEEP_NULL_PARAM_NAME, bool, false, values::numeric_value<bool, true>>,
  factory_simple_parameter<BATCH_SIZE_PARAM_NAME, uint32_t, false, values::numeric_value<uint32_t, 1>>
  >>;

using ValidatingOptionsDeserializer = validate<OptionsDeserializer, OptionsValidator>;
} // namespace

namespace arangodb {
namespace iresearch {

class CalculationQueryContext: public QueryContext {
 public:
  explicit CalculationQueryContext(TRI_vocbase_t& vocbase)
    : QueryContext(vocbase), _resolver(vocbase),
    _transactionContext(vocbase),
    _itemBlockManager(&_resourceMonitor, SerializationFormat::SHADOWROWS) {
    // we need non const parameters as we will inject multiple times
    _ast = std::make_unique<Ast>(*this, NON_CONST_PARAMETERS);
    _trx = AqlTransaction::create(newTrxContext(), _collections,
      _queryOptions.transactionOptions,
      std::unordered_set<std::string>{});
    _trx->addHint(arangodb::transaction::Hints::Hint::FROM_TOPLEVEL_AQL);
    _trx->begin();
  }

  virtual QueryOptions const& queryOptions() const override {
    return _queryOptions;
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
      std::shared_ptr<arangodb::transaction::Context>(),
      &_transactionContext);
  }

  virtual arangodb::transaction::Methods& trxForOptimization() override {
    return *_trx;
  }

  virtual bool killed() const override { return false; }

  /// @brief whether or not a query is a modification query
  virtual bool isModificationQuery() const noexcept override { return false; }

  virtual bool isAsyncQuery() const noexcept override { return false; }

  virtual void enterV8Context() override { TRI_ASSERT(FALSE); }

  AqlItemBlockManager& itemBlockManager() {
    return _itemBlockManager;
  }

 private:
  QueryOptions _queryOptions;
  arangodb::CollectionNameResolver _resolver;
  mutable arangodb::transaction::StandaloneContext _transactionContext;
  std::unique_ptr<arangodb::transaction::Methods> _trx;
  ResourceMonitor _resourceMonitor;
  AqlItemBlockManager _itemBlockManager;
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
              "V8 usage is forbidden for calculation analyzer"};
    }

    // no modification (as data access is forbidden) but to give more clear error message
    if (ast->containsModificationNode()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "DML is forbidden for calculation analyzer"};
    }

    // no traversal (also data access is forbidden) but to give mo clear error message
    if (ast->containsTraversal()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Traversal usage is forbidden for calculation analyzer"};
    }

    std::string errorMessage;
    // Forbid to use functions that reference analyzers -> problems on recovery as analyzers are not available for querying.
    // Forbid all non-Dbserver runable functions as it is not available on DBServers where analyzers run.
    arangodb::aql::Ast::traverseReadOnly(
        ast->root(), [&errorMessage](arangodb::aql::AstNode const* node) -> bool {
          switch (node->type) {
            case arangodb::aql::NODE_TYPE_FCALL: {
              auto func = static_cast<arangodb::aql::Function*>(node->getData());
              if (!func->hasFlag(arangodb::aql::Function::Flags::CanRunOnDBServer) ||
                  forbiddenFunctions.find(func->name) != forbiddenFunctions.end()) {
                errorMessage = "Function '";
                errorMessage.append(func->name)
                    .append("' is forbidden for calculation analyzer");
                return false;
              }
            } break;
            case arangodb::aql::NODE_TYPE_COLLECT: // COLLECT nodes requires optimizer rule to work properly
            case arangodb::aql::NODE_TYPE_COLLECT_COUNT:
              errorMessage = "COLLECT is forbidden for calculation analyzer";
              return false;
            case arangodb::aql::NODE_TYPE_PARAMETER: {
              irs::string_ref parameterName(node->getStringValue(), node->getStringLength());
              if (parameterName != CALCULATION_PARAMETER_NAME) {
                errorMessage = "Invalid parameter found '";
                errorMessage.append(parameterName).append("'");
                return false;
              }
            } break;
            case arangodb::aql::NODE_TYPE_PARAMETER_DATASOURCE:
              errorMessage =
                  "Datasource acces is forbidden for calculation analyzer";
              return false;
            case arangodb::aql::NODE_TYPE_FCALL_USER:
              errorMessage =
                  "UDF functions is forbidden for calculation analyzer";
              return false;
            case arangodb::aql::NODE_TYPE_VIEW:
            case arangodb::aql::NODE_TYPE_FOR_VIEW:
              errorMessage =
                  "View access is forbidden for calculation analyzer";
              return false;
            case arangodb::aql::NODE_TYPE_COLLECTION:
              errorMessage =
                  "Collection access is forbidden for calculation analyzer";
              return false;
            default:
              break;
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


bool CalculationAnalyzer::parse_options(const irs::string_ref& args, options_t& options) {
  auto const slice = arangodb::iresearch::slice(args);
  auto const res = deserialize<ValidatingOptionsDeserializer>(slice);
  if (!res.ok()) {
    LOG_TOPIC("4349c", WARN, arangodb::iresearch::TOPIC) <<
      "Failed to deserialize options from JSON while constructing '"
      << type_name() << "' analyzer, error: '"
      << res.error().message << "'";
    return false;
  }
  options = res.get();
  return true;
}

/*static*/ bool CalculationAnalyzer::normalize(const irs::string_ref& args, std::string& out) {
  options_t options;
  if (parse_options(args, options)) {
    VPackBuilder builder;
    {
      VPackObjectBuilder root(&builder);
      builder.add(QUERY_STRING_PARAM_NAME, VPackValue(options.queryString));
      builder.add(COLLAPSE_ARRAY_POSITIONS_PARAM_NAME, VPackValue(options.collapseArrayPositions));
      builder.add(KEEP_NULL_PARAM_NAME, VPackValue(options.keepNull));
      builder.add(BATCH_SIZE_PARAM_NAME, VPackValue(options.batchSize));
    }
    out.resize(builder.slice().byteSize());
    std::memcpy(&out[0], builder.slice().begin(), out.size());
    return true;
  }
  return false;
}

/*static*/ irs::analysis::analyzer::ptr CalculationAnalyzer::make(irs::string_ref const& args) {
  options_t options;
  if (parse_options(args, options)) {
    auto validationRes = validateQuery(options.queryString,
                                       arangodb::DatabaseFeature::getCalculationVocbase());
    if (validationRes.ok()) {
      return std::make_shared<CalculationAnalyzer>(options);
    } else {
      LOG_TOPIC("f775e", WARN, iresearch::TOPIC)
          << "error validating calculation query: " << validationRes.errorMessage();
    }
  }
  return nullptr;
}

CalculationAnalyzer::CalculationAnalyzer(options_t const& options)
  : irs::analysis::analyzer(irs::type<CalculationAnalyzer>::get()),
    _options(options),
    _query(arangodb::DatabaseFeature::getCalculationVocbase()),
    _engine(0, _query, _query.itemBlockManager(),
            SerializationFormat::SHADOWROWS, nullptr) {
  TRI_ASSERT(validateQuery(_options.queryString,
                           arangodb::DatabaseFeature::getCalculationVocbase())
                 .ok());
}

bool CalculationAnalyzer::next() {
  do {
    if (_queryResults.get()) {
      while (_queryResults->numRows() > _resultRowIdx) {
        AqlValue const& value =
            _queryResults->getValueReference(_resultRowIdx++, _engine.resultRegister());
        if (value.isString() || (value.isNull(true) && _options.keepNull)) {
          if (value.isString()) {
            _term.value = arangodb::iresearch::getBytesRef(value.slice());
          } else {
            _term.value = irs::bytes_ref::EMPTY;
          }
          _inc.value = _nextIncVal;
          _nextIncVal = !_options.collapseArrayPositions;
          return true;
        }
      }
    }
    if (_executionState == ExecutionState::HASMORE) {
      try {
        _executionState = ExecutionState::DONE; // set to done to terminate in case of exception
        _resultRowIdx = 0;
        _queryResults = nullptr;
        std::tie(_executionState, _queryResults) = _engine.getSome(_options.batchSize);
        TRI_ASSERT(_executionState != ExecutionState::WAITING);
      } catch (basics::Exception const& e) {
        LOG_TOPIC("b0026", ERR, iresearch::TOPIC)
            << "error executing calculation query: " << e.message();
      } catch (std::exception const& e) {
        LOG_TOPIC("c92eb", ERR, iresearch::TOPIC)
            << "error executing calculation query: " << e.what();
      } catch (...) {
        LOG_TOPIC("bf89b", ERR, iresearch::TOPIC)
            << "error executing calculation query";
      }
    }
  } while (_executionState != ExecutionState::DONE || (_queryResults.get() &&
                                                       _queryResults->numRows() > _resultRowIdx));
  return false;
}

bool CalculationAnalyzer::reset(irs::string_ref const& field) noexcept {
  try {
    if (!_plan) {  // lazy initialization
      // important to hold a copy here as parser accepts reference!
      auto queryString = arangodb::aql::QueryString(_options.queryString);
      auto ast = _query.ast();
      TRI_ASSERT(ast);
      Parser parser(_query, *ast, queryString);
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
      ast->validateAndOptimize(_query.trxForOptimization());
      _plan = ExecutionPlan::instantiateFromAst(ast);
    } else {
      for (auto node : _bindedNodes) {
        node->setStringValue(field.c_str(), field.size());
      }
      _engine.reset();
    }
    _queryResults = nullptr;
    _plan->clearVarUsageComputed();
    _engine.initFromPlanForCalculation(*_plan);
    _executionState = ExecutionState::HASMORE;
    _resultRowIdx = 0;
    _nextIncVal = 1;  // first increment always 1 to move from -1 to 0
    return true;
  } catch (basics::Exception const& e) {
    LOG_TOPIC("8ee1a", ERR, iresearch::TOPIC)
        << "error creating calculation query: " << e.message();
  } catch (std::exception const& e) {
    LOG_TOPIC("d2223", ERR, iresearch::TOPIC)
        << "error creating calculation query: " << e.what();
  } catch (...) {
    LOG_TOPIC("5ad87", ERR, iresearch::TOPIC)
        << "error creating calculation query";
  }
  return false;
}

CalculationAnalyzer::CalculationQueryContext::CalculationQueryContext(TRI_vocbase_t& vocbase)
  : QueryContext(vocbase), _resolver(vocbase),
  _transactionContext(vocbase),
  _itemBlockManager(&_resourceMonitor, SerializationFormat::SHADOWROWS) {
  _ast = std::make_unique<Ast>(*this, NON_CONST_PARAMETERS);
  _trx = AqlTransaction::create(newTrxContext(), _collections,
    _queryOptions.transactionOptions,
    std::unordered_set<std::string>{});
  _trx->addHint(arangodb::transaction::Hints::Hint::FROM_TOPLEVEL_AQL);
  _trx->begin();
}

std::shared_ptr<arangodb::transaction::Context> CalculationAnalyzer::CalculationQueryContext::newTrxContext() const {
  return std::shared_ptr<arangodb::transaction::Context>(
    std::shared_ptr<arangodb::transaction::Context>(),
    &_transactionContext);
}

} // namespace iresearch
} // namespace arangodb
