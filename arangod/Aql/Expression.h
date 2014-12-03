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
#include "Aql/Query.h"
#include "Aql/Range.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/JsonHelper.h"
#include "Utils/AqlTransaction.h"

struct TRI_json_t;

namespace triagens {
  namespace basics {
    class Json;
    class StringBuffer;
  }

  namespace aql {

    class AqlItemBlock;
    struct AqlValue;
    class Ast;
    class Executor;
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

        Expression (Ast*,
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

        inline bool canThrow () {
          if (_type == UNPROCESSED) {
            analyzeExpression();
          }
          return _canThrow;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the expression can safely run on a DB server
////////////////////////////////////////////////////////////////////////////////

        inline bool canRunOnDBServer () {
          if (_type == UNPROCESSED) {
            analyzeExpression();
          }
          return _canRunOnDBServer;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the expression is deterministic
////////////////////////////////////////////////////////////////////////////////

        inline bool isDeterministic () {
          if (_type == UNPROCESSED) {
            analyzeExpression();
          }
          return _isDeterministic;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clone the expression, needed to clone execution plans
////////////////////////////////////////////////////////////////////////////////

        Expression* clone () {
          // We do not need to copy the _ast, since it is managed by the
          // query object and the memory management of the ASTs
          return new Expression(_ast, _node);
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

        AqlValue execute (triagens::arango::AqlTransaction* trx,
                          std::vector<TRI_document_collection_t const*>&,
                          std::vector<AqlValue>&, size_t,
                          std::vector<Variable*> const&,
                          std::vector<RegisterId> const&,
                          TRI_document_collection_t const**);

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this is a simple expression
////////////////////////////////////////////////////////////////////////////////

        bool isSimple () {
          if (_type == UNPROCESSED) {
            analyzeExpression();
          }
          return _type == SIMPLE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this is an attribute access of any degree (e.g. a.b, 
/// a.b.c, ...)
////////////////////////////////////////////////////////////////////////////////

        bool isAttributeAccess () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this is only a reference access
////////////////////////////////////////////////////////////////////////////////

        bool isReference () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief this gives you ("variable.access", "Reference")
/// call isSimpleAccessReference in advance to ensure no exceptions.
////////////////////////////////////////////////////////////////////////////////

        std::pair<std::string, std::string> getMultipleAttributes();

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify an expression
/// note that currently stringification is only supported for certain node types
////////////////////////////////////////////////////////////////////////////////

        void stringify (triagens::basics::StringBuffer*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief replace variables in the expression
////////////////////////////////////////////////////////////////////////////////

        void replaceVariables (std::unordered_map<VariableId, Variable const*> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidates an expression
/// this only has an effect for V8-based functions, which need to be created,
/// used and destroyed in the same context. when a V8 function is used across
/// multiple V8 contexts, it must be invalidated in between
////////////////////////////////////////////////////////////////////////////////

        void invalidate ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief find a value in a list
////////////////////////////////////////////////////////////////////////////////

        bool findInList (AqlValue const&, 
                         AqlValue const&, 
                         TRI_document_collection_t const*, 
                         TRI_document_collection_t const*,
                         triagens::arango::AqlTransaction*,
                         AstNode const*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief analyze the expression (determine its type etc.)
////////////////////////////////////////////////////////////////////////////////

        void analyzeExpression ();

////////////////////////////////////////////////////////////////////////////////
/// @brief build the expression (if appropriate, compile it into 
/// executable code)
////////////////////////////////////////////////////////////////////////////////

        void buildExpression ();

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE
////////////////////////////////////////////////////////////////////////////////

        AqlValue executeSimpleExpression (AstNode const*,
                                          TRI_document_collection_t const**,
                                          triagens::arango::AqlTransaction*,
                                          std::vector<TRI_document_collection_t const*>&,
                                          std::vector<AqlValue>&, size_t,
                                          std::vector<Variable*> const&,
                                          std::vector<RegisterId> const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the AST
////////////////////////////////////////////////////////////////////////////////

        Ast*                      _ast;

////////////////////////////////////////////////////////////////////////////////
/// @brief the V8 executor
////////////////////////////////////////////////////////////////////////////////

        Executor*                 _executor;

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

          struct TRI_json_t*      _data;
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief type of expression
////////////////////////////////////////////////////////////////////////////////

        ExpressionType            _type;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the expression may throw a runtime exception
////////////////////////////////////////////////////////////////////////////////

        bool                      _canThrow;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the expression can be run safely on a DB server
////////////////////////////////////////////////////////////////////////////////
        
        bool                      _canRunOnDBServer;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the expression is deterministic
////////////////////////////////////////////////////////////////////////////////

        bool                      _isDeterministic;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the expression has been built/compiled
////////////////////////////////////////////////////////////////////////////////

        bool                      _built;

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

