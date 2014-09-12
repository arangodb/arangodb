////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, query context
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Query.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor.h"
#include "Aql/Parser.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Optimizer.h"
#include "Basics/JsonHelper.h"
#include "BasicsC/json.h"
#include "BasicsC/tri-strings.h"
#include "Utils/AqlTransaction.h"
#include "Utils/Exception.h"
#include "Utils/V8TransactionContext.h"
#include "VocBase/vocbase.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query
////////////////////////////////////////////////////////////////////////////////

Query::Query (TRI_vocbase_t* vocbase,
              char const* queryString,
              size_t queryLength,
              TRI_json_t* bindParameters,
              TRI_json_t* options)
  : _vocbase(vocbase),
    _executor(nullptr),
    _queryString(queryString),
    _queryLength(queryLength),
    _queryJson(),
    _type(AQL_QUERY_READ),
    _bindParameters(bindParameters),
    _options(options),
    _collections(vocbase),
    _strings(),
    _ast(nullptr) {

  TRI_ASSERT(_vocbase != nullptr);
  
  _ast = new Ast(this);

  _strings.reserve(32);
}

Query::Query (TRI_vocbase_t* vocbase,
              triagens::basics::Json queryStruct,
              QueryType Type)
  : _vocbase(vocbase),
    _executor(nullptr),
    _queryString(nullptr),
    _queryLength(0),
    _queryJson(queryStruct),
    _type(Type),
    _bindParameters(nullptr),
    _options(nullptr),
    _collections(vocbase),
    _strings(),
    _ast(nullptr) {

  TRI_ASSERT(_vocbase != nullptr);

  _ast = new Ast(this);
  _strings.reserve(32);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a query
////////////////////////////////////////////////////////////////////////////////

Query::~Query () {
  if (_options != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _options);
    _options = nullptr;
  }

  if (_executor != nullptr) {
    delete _executor;
    _executor = nullptr;
  }

  if (_ast != nullptr) {
    delete _ast;
    _ast = nullptr;
  }

  // free strings
  for (auto it = _strings.begin(); it != _strings.end(); ++it) {
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, const_cast<char*>(*it));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a region from the query
////////////////////////////////////////////////////////////////////////////////

std::string Query::extractRegion (int line, 
                                  int column) const {
  // note: line numbers reported by bison/flex start at 1, columns start at 0
  int currentLine   = 1;
  int currentColumn = 0;

  char c;
  char const* p = _queryString;

  while ((c = *p)) {
    if (currentLine > line || 
        (currentLine >= line && currentColumn >= column)) {
      break;
    }

    if (c == '\n') {
      ++p;
      ++currentLine;
      currentColumn = 0;
    }
    else if (c == '\r') {
      ++p;
      ++currentLine;
      currentColumn = 0;

      // eat a following newline
      if (*p == '\n') {
        ++p;
      }
    }
    else {
      ++currentColumn;
      ++p;
    }
  }

  // p is pointing at the position in the query the parse error occurred at
  TRI_ASSERT(p >= _queryString);

  size_t offset = static_cast<size_t>(p - _queryString);

  static int const   SNIPPET_LENGTH = 32;
  static char const* SNIPPET_SUFFIX = "...";

  if (_queryLength < offset + SNIPPET_LENGTH) {
    // return a copy of the region
    return std::string(_queryString + offset, _queryLength - offset);
  }

  // copy query part
  std::string result(_queryString + offset, SNIPPET_LENGTH);
  result.append(SNIPPET_SUFFIX);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error
////////////////////////////////////////////////////////////////////////////////

void Query::registerError (int code,
                           char const* details) {

  TRI_ASSERT(code != TRI_ERROR_NO_ERROR);

  if (details == nullptr) {
    THROW_ARANGO_EXCEPTION(code);
  }
  else {
    THROW_ARANGO_EXCEPTION_PARAMS(code, details);
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief execute an AQL query 
////////////////////////////////////////////////////////////////////////////////

QueryResult Query::execute () {
  try {
    ExecutionPlan* plan;
    Parser parser(this);

    if (_queryString != nullptr) {
      parser.parse();
      // put in bind parameters
      parser.ast()->injectBindParameters(_bindParameters);
      // optimize the ast
      parser.ast()->optimize();
      // std::cout << "AST: " << triagens::basics::JsonHelper::toString(parser.ast()->toJson(TRI_UNKNOWN_MEM_ZONE, false)) << "\n";
    }

    // create the transaction object, but do not start it yet
    AQL_TRANSACTION_V8 trx(_vocbase, _collections.collections());

    if (_queryString != nullptr) {
      // we have an AST
      int res = trx.begin();

      if (res != TRI_ERROR_NO_ERROR) {
        return transactionError(res, trx);
      }

      plan = ExecutionPlan::instanciateFromAst(parser.ast());
      if (plan == nullptr) {
        // oops
        return QueryResult(TRI_ERROR_INTERNAL);
      }
    }
    else {
      ExecutionPlan::getCollectionsFromJson(parser.ast(), _queryJson);

      // creating the plan may have produced some collections
      // we need to add them to the transaction now (otherwise the query will fail)
      int res = trx.addCollectionList(_collections.collections());
      
      if (res == TRI_ERROR_NO_ERROR) {
        res = trx.begin();
      }

      if (res != TRI_ERROR_NO_ERROR) {
        return transactionError(res, trx);
      }

      // we have an execution plan in JSON format
      plan = ExecutionPlan::instanciateFromJson(parser.ast(), _queryJson);
      if (plan == nullptr) {
        // oops
        return QueryResult(TRI_ERROR_INTERNAL);
      }

    }
    
    // get enabled/disabled rules
    std::vector<std::string> rules(getRulesFromOptions());

    // Run the query optimiser:
    triagens::aql::Optimizer opt;
    opt.createPlans(plan, rules);  

    // Now plan and all derived plans belong to the optimizer
    plan = opt.stealBest(); // Now we own the best one again
    TRI_ASSERT(plan != nullptr);
    /* // for debugging of serialisation/deserialisation . . .
    auto JsonPlan = plan->toJson(TRI_UNKNOWN_MEM_ZONE, true);
    auto JsonString = JsonPlan.toString();
    std::cout << "original plan: \n" << JsonString << "\n";

    auto otherPlan = ExecutionPlan::instanciateFromJson (parser.ast(),
                                                         JsonPlan);
    otherPlan->getCost(); 
    auto otherJsonString =
      otherPlan->toJson(TRI_UNKNOWN_MEM_ZONE, true).toString(); 
    std::cout << "deserialised plan: \n" << otherJsonString << "\n";
    TRI_ASSERT(otherJsonString == JsonString);*/

    triagens::basics::Json json(triagens::basics::Json::List);
    triagens::basics::Json stats;

    try { 
      auto engine = ExecutionEngine::instanciateFromPlan(&trx, this, plan);

      try {
        AqlItemBlock* value;
    
        while (nullptr != (value = engine->getSome(1, ExecutionBlock::DefaultBatchSize))) {
          auto doc = value->getDocumentCollection(0);
          size_t const n = value->size();
          for (size_t i = 0; i < n; ++i) {
            AqlValue val = value->getValue(i, 0);

            if (! val.isEmpty()) {
              json.add(val.toJson(&trx, doc)); 
            }
          }
          delete value;
        }

        stats = engine->_stats.toJson();

        delete engine;
      }
      catch (...) {
        delete engine;
        throw;
      }
    }
    catch (...) {
      delete plan;
      throw;
    }

    delete plan;
    trx.commit();
    
    QueryResult result(TRI_ERROR_NO_ERROR);
    result.json  = json.steal();
    result.stats = stats.steal(); 
    return result;
  }
  catch (triagens::arango::Exception const& ex) {
    return QueryResult(ex.code(), ex.message());
  }
  catch (std::bad_alloc const& ex) {
    return QueryResult(TRI_ERROR_OUT_OF_MEMORY, TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));
  }
  catch (std::exception const& ex) {
    return QueryResult(TRI_ERROR_INTERNAL, ex.what());
  }
  catch (...) {
    return QueryResult(TRI_ERROR_INTERNAL, TRI_errno_string(TRI_ERROR_INTERNAL));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an AQL query
////////////////////////////////////////////////////////////////////////////////

QueryResult Query::parse () {
  try {
    Parser parser(this);
    return parser.parse();
  }
  catch (triagens::arango::Exception const& ex) {
    return QueryResult(ex.code(), ex.message());
  }
  catch (...) {
    return QueryResult(TRI_ERROR_OUT_OF_MEMORY, TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explain an AQL query
////////////////////////////////////////////////////////////////////////////////

QueryResult Query::explain () {
  try {
    ExecutionPlan* plan;
    Parser parser(this);

    parser.parse();
    // put in bind parameters
    parser.ast()->injectBindParameters(_bindParameters);
    // optimize the ast
    parser.ast()->optimize();
    // std::cout << "AST: " << triagens::basics::JsonHelper::toString(parser.ast()->toJson(TRI_UNKNOWN_MEM_ZONE)) << "\n";

    // create the transaction object, but do not start it yet
    AQL_TRANSACTION_V8 trx(_vocbase, _collections.collections());

    // we have an AST
    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      return transactionError(res, trx);
    }

    plan = ExecutionPlan::instanciateFromAst(parser.ast());
    if (plan == nullptr) {
      // oops
      return QueryResult(TRI_ERROR_INTERNAL);
    }

    // Run the query optimiser:
    triagens::aql::Optimizer opt;

    // get enabled/disabled rules
    std::vector<std::string> rules(getRulesFromOptions());

    opt.createPlans(plan, rules);
      
    trx.commit();
      
    QueryResult result(TRI_ERROR_NO_ERROR);

    if (allPlans()) {
      triagens::basics::Json out(triagens::basics::Json::List);

      auto plans = opt.getPlans();
      for (auto it : plans) {
        TRI_ASSERT(it != nullptr);

        out.add(it->toJson(parser.ast(), TRI_UNKNOWN_MEM_ZONE, verbosePlans()));
      }
      
      result.json = out.steal();
    }
    else {
      // Now plan and all derived plans belong to the optimizer
      plan = opt.stealBest(); // Now we own the best one again
      TRI_ASSERT(plan != nullptr);

      result.json = plan->toJson(parser.ast(), TRI_UNKNOWN_MEM_ZONE, verbosePlans()).steal(); 

      delete plan;
    }
    
    return result;
  }
  catch (triagens::arango::Exception const& ex) {
    return QueryResult(ex.code(), ex.message());
  }
  catch (std::bad_alloc const& ex) {
    return QueryResult(TRI_ERROR_OUT_OF_MEMORY, TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));
  }
  catch (std::exception const& ex) {
    return QueryResult(TRI_ERROR_INTERNAL, ex.what());
  }
  catch (...) {
    return QueryResult(TRI_ERROR_INTERNAL, TRI_errno_string(TRI_ERROR_INTERNAL));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get v8 executor
////////////////////////////////////////////////////////////////////////////////

Executor* Query::executor () {
  if (_executor == nullptr) {
    // the executor is a singleton per query
    _executor = new Executor;
  }

  TRI_ASSERT(_executor != nullptr);
  return _executor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string
/// the string is freed when the query is destroyed
////////////////////////////////////////////////////////////////////////////////

char* Query::registerString (char const* p, 
                             size_t length,
                             bool mustUnescape) {

  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (length == 0) {
    static char const* empty = "";
    // optimisation for the empty string
    return const_cast<char*>(empty);
  }

  char* copy = nullptr;
  if (mustUnescape) {
    size_t outLength;
    copy = TRI_UnescapeUtf8StringZ(TRI_UNKNOWN_MEM_ZONE, p, length, &outLength);
  }
  else {
    copy = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, p, length);
  }

  if (copy == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  try {
    _strings.push_back(copy);
  }
  catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string
/// the string is freed when the query is destroyed
////////////////////////////////////////////////////////////////////////////////

char* Query::registerString (std::string const& p,
                             bool mustUnescape) {

  return registerString(p.c_str(), p.length(), mustUnescape);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch a boolean value from the options
////////////////////////////////////////////////////////////////////////////////

bool Query::getBooleanOption (char const* option, bool defaultValue) const {  
  if (! TRI_IsArrayJson(_options)) {
    return defaultValue;
  }

  TRI_json_t const* valueJson = TRI_LookupArrayJson(_options, option);
  if (! TRI_IsBooleanJson(valueJson)) {
    return defaultValue;
  }

  return valueJson->_value._boolean;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief neatly format transaction errors to the user.
////////////////////////////////////////////////////////////////////////////////

QueryResult Query::transactionError (int errorCode, AQL_TRANSACTION_V8 const& trx) const
{
  std::string err(TRI_errno_string(errorCode));

  auto detail = trx.getErrorData();
  if (detail.size() > 0) {
    err += std::string(" (") + detail + std::string(")");
  }

  err += std::string("\nwhile executing:\n") + _queryString + std::string("\n");

  return QueryResult(errorCode, err);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read the "optimizer.rules" section from the options
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Query::getRulesFromOptions () const {
  std::vector<std::string> rules;

  if (TRI_IsArrayJson(_options)) {
    TRI_json_t const* optJson = TRI_LookupArrayJson(_options, "optimizer");

    if (TRI_IsArrayJson(optJson)) {
      TRI_json_t const* rulesJson = TRI_LookupArrayJson(optJson, "rules");

      if (TRI_IsListJson(rulesJson)) {
        size_t const n = TRI_LengthListJson(rulesJson);

        for (size_t i = 0; i < n; ++i) {
          TRI_json_t const* rule = static_cast<TRI_json_t const*>(TRI_AtVector(&rulesJson->_value._objects, i));

          if (TRI_IsStringJson(rule)) {
            rules.emplace_back(rule->_value._string.data, rule->_value._string.length - 1);
          }
        }
      }
    }
  }

  return rules;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
