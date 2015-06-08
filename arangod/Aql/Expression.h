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
#include "Basics/StringBuffer.h"
#include "Utils/AqlTransaction.h"

struct TRI_json_t;

namespace triagens {
  namespace basics {
    class Json;
  }

  namespace aql {

    class AqlItemBlock;
    struct AqlValue;
    class Ast;
    class AttributeAccessor;
    class Executor;
    struct V8Expression;

////////////////////////////////////////////////////////////////////////////////
/// @brief AqlExpression, used in execution plans and execution blocks
////////////////////////////////////////////////////////////////////////////////

    class Expression {

      enum ExpressionType : uint32_t {
        UNPROCESSED,
        JSON,
        V8,
        SIMPLE,
        ATTRIBUTE
      };

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
      
      public:

        Expression (Expression const&) = delete;
        Expression& operator= (Expression const&) = delete;
        Expression () = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, using an AST start node
////////////////////////////////////////////////////////////////////////////////

        Expression (Ast*,
                    AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, using JSON
////////////////////////////////////////////////////////////////////////////////

        Expression (Ast*,
                    triagens::basics::Json const&);

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
                          AqlItemBlock const*,
                          size_t,
                          std::vector<Variable*> const&,
                          std::vector<RegisterId> const&,
                          TRI_document_collection_t const**);

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this is a JSON expression
////////////////////////////////////////////////////////////////////////////////

        inline bool isJson () {
          if (_type == UNPROCESSED) {
            analyzeExpression();
          }
          return _type == JSON;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this is a V8 expression
////////////////////////////////////////////////////////////////////////////////

        inline bool isV8 () {
          if (_type == UNPROCESSED) {
            analyzeExpression();
          }
          return _type == V8;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get expression type as string
////////////////////////////////////////////////////////////////////////////////

        std::string typeString () {
          if (_type == UNPROCESSED) {
            analyzeExpression();
          }

          switch (_type) {
            case JSON:
              return "json";
            case SIMPLE:
              return "simple";
            case ATTRIBUTE:
              return "attribute";
            case V8:
              return "v8";
            case UNPROCESSED: {
            }
          }
          TRI_ASSERT(false);
          return "unknown";
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
/// @brief check whether this is a constant node
////////////////////////////////////////////////////////////////////////////////
        
        bool isConstant () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief this gives you ("variable.access", "Reference")
/// call isAttributeAccess in advance to ensure no exceptions.
////////////////////////////////////////////////////////////////////////////////

        std::pair<std::string, std::string> getAttributeAccess();

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify an expression
/// note that currently stringification is only supported for certain node types
////////////////////////////////////////////////////////////////////////////////

        void stringify (triagens::basics::StringBuffer*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify an expression, if it is not too long
/// if the stringified version becomes too long, this method will throw
/// note that currently stringification is only supported for certain node types
////////////////////////////////////////////////////////////////////////////////

        void stringifyIfNotTooLong (triagens::basics::StringBuffer*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief replace variables in the expression with other variables
////////////////////////////////////////////////////////////////////////////////

        void replaceVariables (std::unordered_map<VariableId, Variable const*> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a variable reference in the expression with another
/// expression (e.g. inserting c = `a + b` into expression `c + 1` so the latter 
/// becomes `a + b + 1`
////////////////////////////////////////////////////////////////////////////////

        void replaceVariableReference (Variable const*, AstNode const*);

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
/// @brief find a value in an array
////////////////////////////////////////////////////////////////////////////////

        bool findInArray (AqlValue const&, 
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
                                          AqlItemBlock const*,
                                          size_t,
                                          std::vector<Variable*> const&,
                                          std::vector<RegisterId> const&,
                                          bool);

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

          AttributeAccessor*      _accessor;
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
/// @brief whether or not the top-level attributes of the expression were
/// determined
////////////////////////////////////////////////////////////////////////////////

        bool                      _hasDeterminedAttributes;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the expression has been built/compiled
////////////////////////////////////////////////////////////////////////////////

        bool                      _built;

////////////////////////////////////////////////////////////////////////////////
/// @brief the top-level attributes used in the expression, grouped 
/// by variable name
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<Variable const*, std::unordered_set<std::string>> _attributes;

////////////////////////////////////////////////////////////////////////////////
/// @brief buffer for temporary strings
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::StringBuffer _buffer;

// -----------------------------------------------------------------------------
// --SECTION--                                             public static members
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief "constant" global object for NULL which can be shared by all 
/// expressions but must never be freed
////////////////////////////////////////////////////////////////////////////////

        static TRI_json_t const NullJson; 

////////////////////////////////////////////////////////////////////////////////
/// @brief "constant" global object for TRUE which can be shared by all 
/// expressions but must never be freed
////////////////////////////////////////////////////////////////////////////////

        static TRI_json_t const TrueJson;  

////////////////////////////////////////////////////////////////////////////////
/// @brief "constant" global object for FALSE which can be shared by all 
/// expressions but must never be freed
////////////////////////////////////////////////////////////////////////////////

        static TRI_json_t const FalseJson;  
    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
