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
// --SECTION--                                                            AqlDoc
// -----------------------------------------------------------------------------


    struct AqlItem;

    struct AqlValue {

      enum AqlValueType {
        JSON,
        DOCVEC,
        RANGE
      };

      struct Range {
        int64_t _low;
        int64_t _high;
        Range(int64_t low, int64_t high) : _low(low), _high(high) {}
      };

      union {
        triagens::basics::Json* _json;
        std::vector<shared_ptr<AqlItem> >*  _vector;
        Range                   _range;
      };
      
      AqlValueType _type;

      AqlValue (triagens::basics::Json* json)
        : _json(json), _type(JSON) {
      }

      AqlValue (std::vector<shared_ptr<AqlItem> >* vector)
        : _vector(vector), _type(DOCVEC) {
      }

      AqlValue (int64_t low, int64_t high) 
        : _range(low, high), _type(RANGE) {
      }

      ~AqlValue () {
        switch (_type) {
          case JSON:
            delete _json;
            break;
          case DOCVEC:
            delete _vector;
            break;
          case RANGE:
            break;
        }
      }

      std::string toString () {
        switch (_type) {
          case JSON:
            return _json->toString();
          case DOCVEC: {
            std::stringstream s;
            s << "I am a DOCVEC of length " << _vector->size() << ".";
            return s.str();
          }
          case RANGE: {
            std::stringstream s;
            s << "I am a range: " << _range._low << " .. " << _range._high;
            return s.str();
          }
          default:
            return std::string("");
        }
      }

    };

    struct AqlItem {
      shared_ptr<AqlItem> _outer;
      AqlValue**          _vars;
      int32_t             _nrvars;

      AqlItem (int nrvars)
        : _outer(nullptr), _nrvars(nrvars) {
        if (nrvars > 0) {
          _vars = new AqlValue* [nrvars];
          for (int i = 0; i < nrvars; i++) {
            _vars[i] = nullptr;
          }
        }
        else {
          _vars = nullptr;
        }
      }

      AqlItem (shared_ptr<AqlItem> outer, int nrvars)
        : _outer(outer), _nrvars(nrvars) {
        if (nrvars > 0) {
          _vars = new AqlValue* [nrvars];
          for (int i = 0; i < nrvars; i++) {
            _vars[i] = nullptr;
          }
        }
        else {
          _vars = nullptr;
        }
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

      ~AqlItem () {
        if (_vars != nullptr) {
          for (int i = 0; i < _nrvars; i++) {
            delete _vars[i];
          }
          delete[] _vars;
        }
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief getValue, get the current value of a variable or attribute
////////////////////////////////////////////////////////////////////////////////

      AqlValue* getValue (int up, int index) {
        AqlItem* p = this;
        for (int i = 0; i < up; i++) {
          p = p->_outer.get();
        }
        return p->_vars[index];
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief setValue, set the current value of a variable or attribute
////////////////////////////////////////////////////////////////////////////////

      void setValue (int up, int index, AqlValue* zeug) {
        AqlItem* p = this;
        for (int i = 0; i < up; i++) {
          p = p->_outer.get();
        }
        p->_vars[index] = zeug;
      }

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

        AqlExpression* clone () {
          // We do not need to copy the _ast, since it is managed by the
          // query object and the memory management of the ASTs
          return new AqlExpression(_ast);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

        AqlValue* execute (AqlItem* aqldoc);

////////////////////////////////////////////////////////////////////////////////
/// @brief return a Json representation of the expression
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json toJson (TRI_memory_zone_t* zone) const {
          return triagens::basics::Json(zone, _ast->toJson(zone));
        }

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


