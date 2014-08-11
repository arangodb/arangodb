////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, expression
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

#include "Aql/Expression.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/Types.h"
#include "Aql/V8Executor.h"
#include "Aql/V8Expression.h"
#include "Aql/Variable.h"
#include "Basics/JsonHelper.h"
#include "BasicsC/json.h"
#include "Utils/Exception.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the expression
////////////////////////////////////////////////////////////////////////////////

Expression::Expression (V8Executor* executor,
                        AstNode const* node)
  : _executor(executor),
    _node(node),
    _type(UNPROCESSED) {

  TRI_ASSERT(_executor != nullptr);
  TRI_ASSERT(_node != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the expression
////////////////////////////////////////////////////////////////////////////////

Expression::~Expression () {
  if (_type == V8) {
    delete _func;
  }
  else if (_type == JSON) {
    // _json is freed automatically by AqlItemBlock
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _data);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return all variables used in the expression
////////////////////////////////////////////////////////////////////////////////

std::unordered_set<Variable*> Expression::variables () const {
  return Ast::getReferencedVariables(_node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::execute (AQL_TRANSACTION_V8* trx,
                              std::vector<TRI_document_collection_t const*>& docColls,
                              std::vector<AqlValue>& argv,
                              size_t startPos,
                              std::vector<Variable*> const& vars,
                              std::vector<RegisterId> const& regs) {
  if (_type == UNPROCESSED) {
    if (_node->isConstant()) {
      // generate a constant value
      _data = _node->toJson(TRI_UNKNOWN_MEM_ZONE);
      _type = JSON;
    }
    else {
      // generate a V8 expression
      _func = _executor->generateExpression(_node);
      _type = V8;
    }
  }

  // and execute
  if (_type == JSON) {
    TRI_ASSERT(_data != nullptr);
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, _data, Json::NOFREE));
  }

  if (_type == V8) {
    return _func->execute(trx, docColls, argv, startPos, vars, regs);
  }
  
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
