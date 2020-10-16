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
#include "Utils/CollectionNameResolver.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/FunctionUtils.h"
#include "IResearchCommon.h"
#include "IResearchCalculationAnalyzer.h"
#include "Logger/LogMacros.h"
#include "Transaction/StandaloneContext.h"
#include "VelocyPackHelper.h"
#include "VocBase/vocbase.h"

#include <Containers/HashSet.h>
#include "VPackDeserializer/deserializer.h"
#include <frozen/set.h>

namespace {
using namespace arangodb::velocypack::deserializer;
using namespace arangodb::aql;

constexpr const char QUERY_STRING_PARAM_NAME[] = "queryString";
constexpr const char COLLAPSE_ARRAY_POSITIONS_PARAM_NAME[] = "collapseArrayPos";
constexpr const char KEEP_NULL_PARAM_NAME[] = "keepNull";
constexpr const char CALCULATION_PARAMETER_NAME[] = "field";

/// @brief Artificial vocbase for executing calculation queries
std::unique_ptr<TRI_vocbase_t> _calculationVocbase;

using Options = arangodb::iresearch::CalculationAnalyzer::options_t;

struct OptionsValidator {
  std::optional<deserialize_error> operator()(Options const& opts) const {
    if (opts.queryString.empty()) {
      return deserialize_error{std::string("Value of '").append(QUERY_STRING_PARAM_NAME).append("' should be non empty string")};
    }
    return {};
  }
};

using OptionsDeserializer = utilities::constructing_deserializer<Options, parameter_list<
  factory_deserialized_parameter<QUERY_STRING_PARAM_NAME, values::value_deserializer<std::string>, true>,
  factory_simple_parameter<COLLAPSE_ARRAY_POSITIONS_PARAM_NAME, bool, false, values::numeric_value<bool, false>>,
  factory_simple_parameter<KEEP_NULL_PARAM_NAME, bool, false, values::numeric_value<bool, true>>
  >>;

using ValidatingOptionsDeserializer = validate<OptionsDeserializer, OptionsValidator>;

arangodb::CreateDatabaseInfo createExpressionVocbaseInfo(arangodb::application_features::ApplicationServer& server) {
  arangodb::CreateDatabaseInfo info(server, arangodb::ExecContext::current());
  auto rv = info.load("_expression_vocbase", std::numeric_limits<uint64_t>::max()); // TODO: create constant for AqlFeature::lease kludge
  return info;
}

frozen::set<irs::string_ref, 4> forbiddenFunctions {"TOKENS", "NGRAM_MATCH", "PHRASE", "ANALYZER"};

arangodb::Result validateQuery(std::string const& query, TRI_vocbase_t& vocbase) {
  auto parseQuery = std::make_unique<arangodb::aql::Query>(
    std::make_shared<arangodb::transaction::StandaloneContext>(vocbase),
    arangodb::aql::QueryString(query),
    nullptr, nullptr);

  auto res  = parseQuery->parse();
  if (!res.ok()) {
    return {TRI_ERROR_QUERY_PARSE , res.errorMessage()};
  }
  TRI_ASSERT(parseQuery->ast());

  // Forbid all V8 related stuff as it is not available on DBServers where analyzers run.
  if (parseQuery->ast()->willUseV8()) {
    return { TRI_ERROR_BAD_PARAMETER, "V8 usage is forbidden for calculation analyzer" };
  }

  std::string errorMessage;
  // Forbid to use functions that reference analyzers -> problems on recovery as analyzers are not available for querying.
  // Forbid all non-Dbserver runable functions as it is not available on DBServers where analyzers run.
  arangodb::aql::Ast::traverseReadOnly(parseQuery->ast()->root(), [&errorMessage](arangodb::aql::AstNode const* node) -> bool {
    switch(node->type) {
      case arangodb::aql::NODE_TYPE_FCALL:
        {
          auto func = static_cast<arangodb::aql::Function*>(node->getData());
          if (!func->hasFlag(arangodb::aql::Function::Flags::CanRunOnDBServer) ||
              forbiddenFunctions.find(func->name) != forbiddenFunctions.end()) {
            errorMessage = "Function '";
            errorMessage.append(func->name).append("' is forbidden for calculation analyzer");
            return false;
          }
        }
        break;
      case arangodb::aql::NODE_TYPE_PARAMETER:
        {
          irs::string_ref parameterName(node->getStringValue(), node->getStringLength());
          if (parameterName != CALCULATION_PARAMETER_NAME) {
            errorMessage = "Invalid parameter found '";
            errorMessage.append(parameterName).append("'");
            return false;
          }
        }
        break;
      case arangodb::aql::NODE_TYPE_PARAMETER_DATASOURCE:
        errorMessage = "Datasource acces is forbidden for calculation analyzer";
        return false;
      case arangodb::aql::NODE_TYPE_FCALL_USER:
        errorMessage = "UDF functions is forbidden for calculation analyzer";
        return false;
      case arangodb::aql::NODE_TYPE_VIEW:
      case arangodb::aql::NODE_TYPE_FOR_VIEW:
        errorMessage = "View access is forbidden for calculation analyzer";
        return false;
      case arangodb::aql::NODE_TYPE_COLLECTION:
        errorMessage = "Collection access is forbidden for calculation analyzer";
        return false;
      default:
        break;
    }
    return true;
    });
  if (!errorMessage.empty()) {
    return { TRI_ERROR_BAD_PARAMETER, errorMessage };
  }
  return {};
}
}

namespace arangodb {
namespace iresearch {

class CalculationQueryContext: public QueryContext {
 public:
  CalculationQueryContext(TRI_vocbase_t& vocbase)
    : QueryContext(vocbase), _resolver(vocbase),
    _transactionContext(vocbase),
    _itemBlockManager(&_resourceMonitor, SerializationFormat::SHADOWROWS) {
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
    return std::make_shared<CalculationAnalyzer>(options);
  }
  return nullptr;
}

CalculationAnalyzer::CalculationAnalyzer(options_t const& options)
  : irs::analysis::analyzer(irs::type<CalculationAnalyzer>::get()), _options(options) {
  // TODO: move this to normalize?? As here is too late to report
  //TRI_ASSERT(validateQuery(_options.queryString, *_calculationVocbase).ok());
  
  // inject bind parameters into query AST
  //auto func = [&](AstNode* node) -> AstNode* {
  //  if (node->type == NODE_TYPE_PARAMETER || node->type == NODE_TYPE_PARAMETER_DATASOURCE) {
  //    // found a bind parameter in the query string
  //    std::string const param = node->getString();

  //    if (param.empty()) {
  //      // parameter name must not be empty
  //      ::throwFormattedError(_query, TRI_ERROR_QUERY_BIND_PARAMETER_MISSING, param.c_str());
  //    }

  //    auto const& it = p.find(param);

  //    if (it == p.end()) {
  //      // query uses a bind parameter that was not defined by the user
  //      ::throwFormattedError(_query, TRI_ERROR_QUERY_BIND_PARAMETER_MISSING, param.c_str());
  //    }

  //    // mark the bind parameter as being used
  //    (*it).second.second = true;

  //    auto const& value = (*it).second.first;

  //    if (node->type == NODE_TYPE_PARAMETER) {
  //      // bind parameter containing a value literal
  //      node = nodeFromVPack(value, true);

  //      if (node != nullptr) {
  //        // already mark node as constant here
  //        node->setFlag(DETERMINED_CONSTANT, VALUE_CONSTANT);
  //        // mark node as simple
  //        node->setFlag(DETERMINED_SIMPLE, VALUE_SIMPLE);
  //        // mark node as executable on db-server
  //        node->setFlag(DETERMINED_RUNONDBSERVER, VALUE_RUNONDBSERVER);
  //        // mark node as deterministic
  //        node->setFlag(DETERMINED_NONDETERMINISTIC);

  //        // finally note that the node was created from a bind parameter
  //        node->setFlag(FLAG_BIND_PARAMETER);
  //      }
  //    } else {
  //      TRI_ASSERT(node->type == NODE_TYPE_PARAMETER_DATASOURCE);

  //      if (!value.isString()) {
  //        // we can get here in case `WITH @col ...` when the value of @col
  //        // is not a string
  //        ::throwFormattedError(_query, TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, param.c_str());
  //        // query will have been aborted here
  //      }

  //      // bound data source parameter
  //      TRI_ASSERT(value.isString());
  //      VPackValueLength l;
  //      char const* name = value.getString(l);

  //      // check if the collection was used in a data-modification query
  //      bool isWriteCollection = false;

  //      arangodb::velocypack::StringRef paramRef(param);
  //      arangodb::velocypack::StringRef nameRef(name, l);

  //      AstNode* newNode = nullptr;
  //      for (auto const& it : _writeCollections) {
  //        auto const& c = it.first;

  //        if (c->type == NODE_TYPE_PARAMETER_DATASOURCE &&
  //          paramRef == arangodb::velocypack::StringRef(c->getStringValue(),
  //            c->getStringLength())) {
  //          // bind parameter still present in _writeCollections
  //          TRI_ASSERT(newNode == nullptr);
  //          isWriteCollection = true;
  //          break;
  //        } else if (c->type == NODE_TYPE_COLLECTION &&
  //          nameRef == arangodb::velocypack::StringRef(c->getStringValue(),
  //            c->getStringLength())) {
  //          // bind parameter was already replaced with a proper collection node 
  //          // in _writeCollections
  //          TRI_ASSERT(newNode == nullptr);
  //          isWriteCollection = true;
  //          newNode = const_cast<AstNode*>(c);
  //          break;
  //        }
  //      }

  //      TRI_ASSERT(newNode == nullptr || isWriteCollection);

  //      if (newNode == nullptr) {
  //        newNode = createNodeDataSource(resolver, name, l,
  //          isWriteCollection ? AccessMode::Type::WRITE
  //          : AccessMode::Type::READ,
  //          false, true);
  //        TRI_ASSERT(newNode != nullptr);

  //        if (isWriteCollection) {
  //          // must update AST info now for all nodes that contained this
  //          // parameter
  //          for (auto& it : _writeCollections) {
  //            auto& c = it.first;

  //            if (c->type == NODE_TYPE_PARAMETER_DATASOURCE &&
  //              paramRef == arangodb::velocypack::StringRef(c->getStringValue(),
  //                c->getStringLength())) {
  //              c = newNode;
  //              // no break here. replace all occurrences
  //            }
  //          }
  //        }
  //      }
  //      node = newNode;
  //    }
  //  } else if (node->type == NODE_TYPE_BOUND_ATTRIBUTE_ACCESS) {
  //    // look at second sub-node. this is the (replaced) bind parameter
  //    auto name = node->getMember(1);

  //    if (name->type == NODE_TYPE_VALUE) {
  //      if (name->value.type == VALUE_TYPE_STRING && name->value.length != 0) {
  //        // convert into a regular attribute access node to simplify handling
  //        // later
  //        return createNodeAttributeAccess(node->getMember(0), name->getStringValue(),
  //          name->getStringLength());
  //      }
  //    } else if (name->type == NODE_TYPE_ARRAY) {
  //      // bind parameter is an array (e.g. ["a", "b", "c"]. now build the
  //      // attribute accesses for the array members recursively
  //      size_t const n = name->numMembers();

  //      AstNode* result = nullptr;
  //      if (n > 0) {
  //        result = node->getMember(0);
  //      }

  //      for (size_t i = 0; i < n; ++i) {
  //        auto part = name->getMember(i);
  //        if (part->value.type != VALUE_TYPE_STRING || part->value.length == 0) {
  //          // invalid attribute name part
  //          result = nullptr;
  //          break;
  //        }

  //        result = createNodeAttributeAccess(result, part->getStringValue(),
  //          part->getStringLength());
  //      }

  //      if (result != nullptr) {
  //        return result;
  //      }
  //    }
  //    // fallthrough to exception

  //    // if no string value was inserted for the parameter name, this is an
  //    // error
  //    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE,
  //      node->getString().c_str());
  //  } else if (node->type == NODE_TYPE_TRAVERSAL) {
  //    extractCollectionsFromGraph(node->getMember(2));
  //  } else if (node->type == NODE_TYPE_SHORTEST_PATH ||
  //    node->type == NODE_TYPE_K_SHORTEST_PATHS) {
  //    extractCollectionsFromGraph(node->getMember(3));
  //  }

  //  return node;
  //};
  //Ast::traverseAndModify(_ast->root(), func);
}

bool CalculationAnalyzer::next() {
  if (_queryResults.get()) {
    // TODO: call engine refill!
    while (_queryResults->numRows() > _resultRowIdx) {
      AqlValue const& value = _queryResults->getValueReference(_resultRowIdx++, _engine->resultRegister());
      if (value.isString()) {
        _term.value = irs::ref_cast<irs::byte_type>(arangodb::iresearch::getStringRef(value.slice()));
        return true;
      }
    }
  }
  return false;
}

bool CalculationAnalyzer::reset(irs::string_ref const& field) noexcept {
  if (!_query) { // lazy initialization 
    _query = std::make_unique<CalculationQueryContext>(*_calculationVocbase);
    _ast = std::make_unique<Ast>(*_query);
    // important to hold a copy here as parser accepts reference!
    auto queryString = arangodb::aql::QueryString(_options.queryString);
    Parser parser(*_query, *_ast, queryString);
    parser.parse();
    AstNode* astRoot = const_cast<AstNode*>(_ast->root());
    TRI_ASSERT(astRoot);
    Ast::traverseAndModify(astRoot, [this, field](AstNode * node) -> AstNode* {
      if (node->type == NODE_TYPE_PARAMETER) {
        // should be only our parameter name. see validation method!
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        TRI_ASSERT(irs::string_ref(node->getStringValue(), 
                                   node->getStringLength()) == CALCULATION_PARAMETER_NAME);
#endif
        // FIXME: move to computed value once here could be not only strings
        auto newNode = _ast->createNodeValueString(field.c_str(), field.size());
        // finally note that the node was created from a bind parameter
        newNode->setFlag(FLAG_BIND_PARAMETER);
        newNode->setFlag(DETERMINED_NONDETERMINISTIC);
        _bindedNodes.push_back(newNode);
        return newNode;
      } else {
        return node;
      }});
    _plan = ExecutionPlan::instantiateFromAst(_ast.get());
  } else {
    for (auto node : _bindedNodes) {
      node->setStringValue(field.c_str(), field.size());
    }
  }

  _engine = std::make_unique<ExecutionEngine>(0, *_query, _query->itemBlockManager(),
                                              SerializationFormat::SHADOWROWS, nullptr);
  _engine->initFromPlanForCalculation(*_plan);
  ExecutionState state = ExecutionState::HASMORE;
  std::tie(state, _queryResults) = _engine->getSome(1000); // TODO: configure batch size
  TRI_ASSERT(state != ExecutionState::WAITING);
  // TODO: Position calculation as parameter
  // TODO: Filtering results
  _resultRowIdx = 0;
  return true;
}

void CalculationAnalyzer::initCalculationContext(arangodb::application_features::ApplicationServer& server) {
  _calculationVocbase = std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_NORMAL, createExpressionVocbaseInfo(server));
}

void CalculationAnalyzer::shutdownCalculationContext() {
  _calculationVocbase.reset();
}

} // iresearch
} // arangodb
