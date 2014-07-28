////////////////////////////////////////////////////////////////////////////////
/// @brief fundamental types for the optimisation and execution of AQL
///
/// @file arangod/Aql/Types.h
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

#ifndef ARANGODB_AQL_TYPES_H
#define ARANGODB_AQL_TYPES_H 1

#include <Basics/JsonHelper.h>

#include "VocBase/document-collection.h"
#include "Aql/AstNode.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                      AqlValues
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief struct AqlValue, used to pipe documents through executions
/// the execution engine keeps one AqlValue struct for each document
/// that is piped through the engine. Note that the document can exist
/// in one of two formats throughout its lifetime in the engine.
/// When it resides originally in the WAl or a datafile, it stays
/// there unchanged and we only store a (pointer to) a copy of the
/// TRI_doc_mptr_t struct. Sometimes, when it is modified (or created
/// on the fly anyway), we keep the document as a TRI_json_t, wrapped
/// by a Json struct. That is, the following struct has the following
/// invariant:
/// Either the whole struct is empty and thus _json is empty and _mptr
/// is a nullptr. Otherwise, either _json is empty and _mptr is not a
/// nullptr, or _json is non-empty and _mptr is anullptr.
/// Additionally, the struct contains another TRI_json_t holding
/// the current state of the LET variables (and possibly some other
/// computations). This is the _vars attribute.
/// Note that both Json subobjects are constructed as AUTOFREE.
////////////////////////////////////////////////////////////////////////////////

    struct AqlValue {
      triagens::basics::Json      _json;
      TRI_doc_mptr_t*             _mptr;
      triagens::basics::Json      _vars;

////////////////////////////////////////////////////////////////////////////////
/// @brief convenience constructors
////////////////////////////////////////////////////////////////////////////////

      AqlValue ()
        : _json(), _mptr(nullptr), _vars() {
      }

      AqlValue (TRI_doc_mptr_t* mptr)
        : _json(), _mptr(mptr), _vars() {
      }

      AqlValue (triagens::basics::Json json)
        : _json(json), _mptr(nullptr), _vars() {
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

      ~AqlValue () {
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the value
////////////////////////////////////////////////////////////////////////////////

      std::string toString () const {
        std::string out;
        if (! _json.isEmpty()) {
          out += _json.toString();
        }
        else if (_mptr != nullptr) {
          out.append("got a master pointer");
        }

        if (! _vars.isEmpty()) {
          out += _vars.toString();
        }

        return out;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief getValue, get the current value of a variable or attribute
////////////////////////////////////////////////////////////////////////////////

      triagens::basics::Json getValue (std::string name);

////////////////////////////////////////////////////////////////////////////////
/// @brief setValue, set the current value of a variable or attribute
////////////////////////////////////////////////////////////////////////////////

      void setValue (std::string name, triagens::basics::Json json);

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    AqlExpressions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class AqlExpression, used in execution plans and execution blocks
////////////////////////////////////////////////////////////////////////////////

    class AqlExpression {
      
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief default constructor, creating an empty expression
////////////////////////////////////////////////////////////////////////////////

        AqlExpression () : _ast(nullptr) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, using an abstract syntax tree
////////////////////////////////////////////////////////////////////////////////

        AqlExpression (AstNode* ast) : _ast(ast) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~AqlExpression () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getAst, get the underlying abstract syntax tree
////////////////////////////////////////////////////////////////////////////////

        AstNode* getAst () {
          return _ast;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clone the expression, needed to clone execution plans
////////////////////////////////////////////////////////////////////////////////

        AqlExpression* clone ();

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json execute (AqlValue* aqldoc);

////////////////////////////////////////////////////////////////////////////////
/// @brief private members
////////////////////////////////////////////////////////////////////////////////

      private:

        // do we need a (possibly empty) subquery entry here?
        AstNode* _ast;

    };

  }  // namespace triagens::aql
}  // namespace triagens



#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


