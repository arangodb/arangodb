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
#include "Aql/Parser.h"
#include "Aql/V8Executor.h"
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
              TRI_json_t* bindParameters)
  : _vocbase(vocbase),
    _executor(nullptr),
    _queryString(queryString),
    _queryLength(queryLength),
    _type(AQL_QUERY_READ),
    _bindParameters(bindParameters),
    _collections(vocbase),
    _strings() {

  TRI_ASSERT(_vocbase != nullptr);
  
  _strings.reserve(32);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a query
////////////////////////////////////////////////////////////////////////////////

Query::~Query () {
  if (_executor != nullptr) {
    delete _executor;
    _executor = nullptr;
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
    Parser parser(this);
    parser.parse();

    // put in bind parameters
    parser.ast()->injectBindParameters(_bindParameters);
    // optimize the ast
    parser.ast()->optimize();
    // std::cout << "AST: " << triagens::basics::JsonHelper::toString(parser.ast()->toJson(TRI_UNKNOWN_MEM_ZONE)) << "\n";

    triagens::arango::AqlTransaction<triagens::arango::V8TransactionContext<true>> trx(_vocbase, _collections.collections());

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      return QueryResult(res, TRI_errno_string(res));
    }

    auto plan = ExecutionPlan::instanciateFromAst(parser.ast());

    // Run the query optimiser:
    triagens::aql::Optimizer opt;
    opt.createPlans(plan);  // Now plan and all derived plans belong to the
                            // optimizer
    plan = opt.stealBest(); // Now we own the best one again

    triagens::basics::Json json(triagens::basics::Json::List);

    try { 
      auto engine = ExecutionEngine::instanciateFromPlan(&trx, plan);

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
    result.json = json.steal(); 
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
/// @brief explain an AQL query - TODO: implement and determine return type
////////////////////////////////////////////////////////////////////////////////

void Query::explain () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get v8 executor
////////////////////////////////////////////////////////////////////////////////

V8Executor* Query::executor () {
  if (_executor == nullptr) {
    // the executor is a singleton per query
    _executor = new V8Executor;
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

char* Query::registerString (std::string const& p,
                             bool mustUnescape) {

  return registerString (p.c_str(),p.length(), mustUnescape);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
