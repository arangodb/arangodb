////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, AST node
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

#ifndef ARANGODB_AQL_ASTNODE_H
#define ARANGODB_AQL_ASTNODE_H 1

#include "Basics/Common.h"
#include "BasicsC/vector.h"
#include "Utils/Exception.h"

namespace triagens {
  namespace aql {

////////////////////////////////////////////////////////////////////////////////
/// @brief enumeration of AST node value types
////////////////////////////////////////////////////////////////////////////////

    enum AstNodeValueType {
      VALUE_TYPE_FAIL = 0,
      VALUE_TYPE_NULL,
      VALUE_TYPE_INT,
      VALUE_TYPE_DOUBLE,
      VALUE_TYPE_BOOL,
      VALUE_TYPE_STRING
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief AST node value
////////////////////////////////////////////////////////////////////////////////

    struct AstNodeValue {
      union {
        int64_t     _int;
        double      _double;
        bool        _bool;
        char const* _string;
      } value;
      AstNodeValueType type;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief enumeration of AST node types
////////////////////////////////////////////////////////////////////////////////

    enum AstNodeType {
      NODE_TYPE_ROOT,
      NODE_TYPE_SCOPE_START,
      NODE_TYPE_SCOPE_END,
      NODE_TYPE_FOR,
      NODE_TYPE_LET,
      NODE_TYPE_FILTER,
      NODE_TYPE_RETURN,
      NODE_TYPE_REMOVE,
      NODE_TYPE_INSERT,
      NODE_TYPE_UPDATE,
      NODE_TYPE_REPLACE,
      NODE_TYPE_COLLECT,
      NODE_TYPE_SORT,
      NODE_TYPE_SORT_ELEMENT,
      NODE_TYPE_LIMIT,
      NODE_TYPE_VARIABLE,
      NODE_TYPE_ASSIGN,
      NODE_TYPE_OPERATOR_UNARY_PLUS,
      NODE_TYPE_OPERATOR_UNARY_MINUS,
      NODE_TYPE_OPERATOR_UNARY_NOT,
      NODE_TYPE_OPERATOR_BINARY_AND,
      NODE_TYPE_OPERATOR_BINARY_OR,
      NODE_TYPE_OPERATOR_BINARY_PLUS,
      NODE_TYPE_OPERATOR_BINARY_MINUS,
      NODE_TYPE_OPERATOR_BINARY_TIMES,
      NODE_TYPE_OPERATOR_BINARY_DIV,
      NODE_TYPE_OPERATOR_BINARY_MOD,
      NODE_TYPE_OPERATOR_BINARY_EQ,
      NODE_TYPE_OPERATOR_BINARY_NE,
      NODE_TYPE_OPERATOR_BINARY_LT,
      NODE_TYPE_OPERATOR_BINARY_LE,
      NODE_TYPE_OPERATOR_BINARY_GT,
      NODE_TYPE_OPERATOR_BINARY_GE,
      NODE_TYPE_OPERATOR_BINARY_IN,
      NODE_TYPE_OPERATOR_TERNARY,
      NODE_TYPE_SUBQUERY,
      NODE_TYPE_ATTRIBUTE_ACCESS,
      NODE_TYPE_BOUND_ATTRIBUTE_ACCESS,
      NODE_TYPE_INDEXED_ACCESS,
      NODE_TYPE_EXPAND,
      NODE_TYPE_VALUE,
      NODE_TYPE_LIST,
      NODE_TYPE_ARRAY,
      NODE_TYPE_ARRAY_ELEMENT,
      NODE_TYPE_COLLECTION,
      NODE_TYPE_REFERENCE,
      NODE_TYPE_ATTRIBUTE,
      NODE_TYPE_PARAMETER,
      NODE_TYPE_FCALL,
      NODE_TYPE_FCALL_USER
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    struct AstNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the node
////////////////////////////////////////////////////////////////////////////////

    struct AstNode {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a node
////////////////////////////////////////////////////////////////////////////////

      AstNode (AstNodeType type) 
        : type(type) {
  
        TRI_InitVectorPointer(&members, TRI_UNKNOWN_MEM_ZONE);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the node
////////////////////////////////////////////////////////////////////////////////

      ~AstNode () {
        TRI_DestroyVectorPointer(&members);
      }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief add a member to the node
////////////////////////////////////////////////////////////////////////////////

        void addMember (AstNode* node) {
          TRI_ASSERT(node != nullptr);

          int res = TRI_PushBackVectorPointer(&members, static_cast<void*>(node));

          if (res != TRI_ERROR_NO_ERROR) {
            THROW_ARANGO_EXCEPTION(res);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a member to the node
////////////////////////////////////////////////////////////////////////////////

        void addMember (AstNode const* node) {
          addMember(const_cast<AstNode*>(node));
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a member of the node
////////////////////////////////////////////////////////////////////////////////

        AstNode* getMember (size_t i) {
          if (i >= members._length) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
          }
          return static_cast<AstNode*>(members._buffer[i]);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the node's value type
////////////////////////////////////////////////////////////////////////////////

        void setValueType (AstNodeValueType type) {
          value.type = type;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the bool value of a node
////////////////////////////////////////////////////////////////////////////////

        int64_t getBoolValue () const {
          return value.value._bool;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the bool value of a node
////////////////////////////////////////////////////////////////////////////////

        void setBoolValue (bool v) {
          value.value._bool = v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the int value of a node
////////////////////////////////////////////////////////////////////////////////

        int64_t getIntValue () const {
          return value.value._int;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the int value of a node
////////////////////////////////////////////////////////////////////////////////

        void setIntValue (int64_t v) {
          value.value._int = v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the double value of a node
////////////////////////////////////////////////////////////////////////////////

        double getDoubleValue () const {
          return value.value._double;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the string value of a node
////////////////////////////////////////////////////////////////////////////////

        void setDoubleValue (double v) {
          value.value._double = v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the string value of a node
////////////////////////////////////////////////////////////////////////////////

        char const* getStringValue () const {
          return value.value._string;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the string value of a node
////////////////////////////////////////////////////////////////////////////////

        void setStringValue (char const* v) {
          value.value._string = v;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      public:
  
        TRI_vector_pointer_t  members;
        AstNodeType const     type;
        AstNodeValue          value;

    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
