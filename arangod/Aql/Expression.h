////////////////////////////////////////////////////////////////////////////////
/// @brief AQL, expression
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_EXPRESSION_H
#define ARANGODB_AQL_EXPRESSION_H 1

#include "Basics/Common.h"
#include "Aql/AstNode.h"
#include "Aql/Types.h"
#include "Aql/Query.h"
#include "Basics/JsonHelper.h"
#include "Utils/AqlTransaction.h"

struct TRI_json_s;

namespace triagens {
  namespace basics {
    class Json;
  }

  namespace aql {

    class AqlItemBlock;
    struct AqlValue;
    class Ast;
    struct Variable;
    class V8Executor;
    struct V8Expression;

////////////////////////////////////////////////////////////////////////////////
/// @brief AqlExpression, used in execution plans and execution blocks
////////////////////////////////////////////////////////////////////////////////

    class Expression {

      enum ExpressionType {
        UNPROCESSED,
        JSON,
        V8,
        SIMPLE
      };

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, using an AST start node
////////////////////////////////////////////////////////////////////////////////

        Expression (V8Executor*,
                    AstNode const*);

        Expression (Ast*,
                    triagens::basics::Json const &json);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~Expression ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the underlying AST node
////////////////////////////////////////////////////////////////////////////////

        inline AstNode const* node () {
          return _node;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the expression can throw an exception
////////////////////////////////////////////////////////////////////////////////

        inline bool canThrow () const {
          return _canThrow;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clone the expression, needed to clone execution plans
////////////////////////////////////////////////////////////////////////////////

        Expression* clone () {
          // We do not need to copy the _ast, since it is managed by the
          // query object and the memory management of the ASTs
          return new Expression(_executor, _node);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return all variables used in the expression
////////////////////////////////////////////////////////////////////////////////

        std::unordered_set<Variable*> variables () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return a Json representation of the expression
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json toJson (TRI_memory_zone_t* zone,
                                       bool verbose) const {
          return triagens::basics::Json(zone, _node->toJson(zone, verbose));
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

        AqlValue execute (AQL_TRANSACTION_V8* trx,
                          std::vector<TRI_document_collection_t const*>&,
                          std::vector<AqlValue>&, size_t,
                          std::vector<Variable*> const&,
                          std::vector<RegisterId> const&);


////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this is a simple expression.
////////////////////////////////////////////////////////////////////////////////

        bool isSimple () const {
          return _type == SIMPLE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this is a simple access to a Reference.
////////////////////////////////////////////////////////////////////////////////

        bool isSimpleAccessReference () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief this gives you ("variable.access", "Reference")
/// call isSimpleAccessReference in advance to enshure no exceptions.
////////////////////////////////////////////////////////////////////////////////

        std::pair<std::string, std::string> getAccessNRef() const;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief analyze the expression (and, if appropriate, compile it into 
/// executable code)
////////////////////////////////////////////////////////////////////////////////

        void analyzeExpression ();

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE
////////////////////////////////////////////////////////////////////////////////

        AqlValue executeSimpleExpression (AstNode const*,
                                          TRI_document_collection_t const**,
                                          AQL_TRANSACTION_V8*,
                                          std::vector<TRI_document_collection_t const*>&,
                                          std::vector<AqlValue>&, size_t,
                                          std::vector<Variable*> const&,
                                          std::vector<RegisterId> const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the V8 executor
////////////////////////////////////////////////////////////////////////////////

        V8Executor*               _executor;

////////////////////////////////////////////////////////////////////////////////
/// @brief the AST node that contains the expression to execute
////////////////////////////////////////////////////////////////////////////////

        AstNode const*            _node;

////////////////////////////////////////////////////////////////////////////////
/// @brief a v8 function that will be executed for the expression
/// if the expression is a constant, it will be stored as plain JSON instead
////////////////////////////////////////////////////////////////////////////////

        union {
          V8Expression*           _func;

          struct TRI_json_s*      _data;
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief type of expression
////////////////////////////////////////////////////////////////////////////////

        ExpressionType            _type;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the expression may throw a runtime exception
////////////////////////////////////////////////////////////////////////////////

        bool                      _canThrow;

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

