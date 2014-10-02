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
#include "Basics/json.h"
#include "Basics/vector.h"
#include "Basics/JsonHelper.h"
#include "Utils/Exception.h"
#include "Aql/Query.h"

namespace triagens {
  namespace basics {
    class StringBuffer;
  }

  namespace aql {

    class Ast;

////////////////////////////////////////////////////////////////////////////////
/// @brief enumeration of AST node value types
////////////////////////////////////////////////////////////////////////////////

    enum AstNodeValueType {
      VALUE_TYPE_FAIL   = 0,
      VALUE_TYPE_NULL   = 1,
      VALUE_TYPE_BOOL   = 2,
      VALUE_TYPE_INT    = 3,
      VALUE_TYPE_DOUBLE = 4,
      VALUE_TYPE_STRING = 5
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
        void*       _data;
      } value;
      AstNodeValueType type;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief enumeration of AST node types
////////////////////////////////////////////////////////////////////////////////

    enum AstNodeType {
      NODE_TYPE_ROOT                          =  0,
      NODE_TYPE_FOR                           =  1,
      NODE_TYPE_LET                           =  2,
      NODE_TYPE_FILTER                        =  3,
      NODE_TYPE_RETURN                        =  4,
      NODE_TYPE_REMOVE                        =  5,
      NODE_TYPE_INSERT                        =  6,
      NODE_TYPE_UPDATE                        =  7,
      NODE_TYPE_REPLACE                       =  8,
      NODE_TYPE_COLLECT                       =  9,
      NODE_TYPE_SORT                          = 10,
      NODE_TYPE_SORT_ELEMENT                  = 11,
      NODE_TYPE_LIMIT                         = 12,
      NODE_TYPE_VARIABLE                      = 13,
      NODE_TYPE_ASSIGN                        = 14,
      NODE_TYPE_OPERATOR_UNARY_PLUS           = 15,
      NODE_TYPE_OPERATOR_UNARY_MINUS          = 16,
      NODE_TYPE_OPERATOR_UNARY_NOT            = 17,
      NODE_TYPE_OPERATOR_BINARY_AND           = 18,
      NODE_TYPE_OPERATOR_BINARY_OR            = 19,
      NODE_TYPE_OPERATOR_BINARY_PLUS          = 20,
      NODE_TYPE_OPERATOR_BINARY_MINUS         = 21,
      NODE_TYPE_OPERATOR_BINARY_TIMES         = 22,
      NODE_TYPE_OPERATOR_BINARY_DIV           = 23,
      NODE_TYPE_OPERATOR_BINARY_MOD           = 24,
      NODE_TYPE_OPERATOR_BINARY_EQ            = 25,
      NODE_TYPE_OPERATOR_BINARY_NE            = 26,
      NODE_TYPE_OPERATOR_BINARY_LT            = 27,
      NODE_TYPE_OPERATOR_BINARY_LE            = 28,
      NODE_TYPE_OPERATOR_BINARY_GT            = 29,
      NODE_TYPE_OPERATOR_BINARY_GE            = 30,
      NODE_TYPE_OPERATOR_BINARY_IN            = 31,
      NODE_TYPE_OPERATOR_BINARY_NIN           = 32,
      NODE_TYPE_OPERATOR_TERNARY              = 33,
      NODE_TYPE_SUBQUERY                      = 34,
      NODE_TYPE_ATTRIBUTE_ACCESS              = 35,
      NODE_TYPE_BOUND_ATTRIBUTE_ACCESS        = 36,
      NODE_TYPE_INDEXED_ACCESS                = 37,
      NODE_TYPE_EXPAND                        = 38,
      NODE_TYPE_ITERATOR                      = 39,
      NODE_TYPE_VALUE                         = 40,
      NODE_TYPE_LIST                          = 41,
      NODE_TYPE_ARRAY                         = 42,
      NODE_TYPE_ARRAY_ELEMENT                 = 43,
      NODE_TYPE_COLLECTION                    = 44,
      NODE_TYPE_REFERENCE                     = 45,
      NODE_TYPE_PARAMETER                     = 46,
      NODE_TYPE_FCALL                         = 47,
      NODE_TYPE_FCALL_USER                    = 48,
      NODE_TYPE_RANGE                         = 49,
      NODE_TYPE_NOP                           = 50
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    struct AstNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the node
////////////////////////////////////////////////////////////////////////////////

    struct AstNode {

      static std::unordered_map<int, std::string const> const Operators;
      static std::unordered_map<int, std::string const> const TypeNames;
      static std::unordered_map<int, std::string const> const valueTypeNames;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the node
////////////////////////////////////////////////////////////////////////////////

      AstNode (AstNodeType); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create the node from JSON
////////////////////////////////////////////////////////////////////////////////

      AstNode (Ast*,
               triagens::basics::Json const& json); 

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the node
////////////////////////////////////////////////////////////////////////////////

      ~AstNode ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type name of a node
////////////////////////////////////////////////////////////////////////////////

        std::string const& getTypeString () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the value type name of a node
////////////////////////////////////////////////////////////////////////////////

        std::string const& getValueTypeString () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether we know a type of this kind; throws exception if not.
////////////////////////////////////////////////////////////////////////////////

        static void validateType (int type);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether we know a value type of this kind; 
/// throws exception if not.
////////////////////////////////////////////////////////////////////////////////

        static void validateValueType (int type);

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch a node's type from json
////////////////////////////////////////////////////////////////////////////////

        static AstNodeType getNodeTypeFromJson (triagens::basics::Json const& json);

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the node value
/// the caller is responsible for freeing the JSON later
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* toJsonValue (TRI_memory_zone_t*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the node
/// the caller is responsible for freeing the JSON later
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* toJson (TRI_memory_zone_t*,
                            bool) const;

      std::string toInfoString (TRI_memory_zone_t* zone) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a JSON representation of the node to the JSON list specified
/// in the first argument
////////////////////////////////////////////////////////////////////////////////

        void toJson (TRI_json_t*,
                     TRI_memory_zone_t*,
                     bool) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the node's value to a boolean value
////////////////////////////////////////////////////////////////////////////////

        bool toBoolean () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a value node is of numeric type
////////////////////////////////////////////////////////////////////////////////

        inline bool isNumericValue () const {
          return (type == NODE_TYPE_VALUE &&
                 (value.type == VALUE_TYPE_INT || value.type == VALUE_TYPE_DOUBLE)); 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a value node is of bool type
////////////////////////////////////////////////////////////////////////////////

        inline bool isBoolValue () const {
          return (type == NODE_TYPE_VALUE && value.type == VALUE_TYPE_BOOL);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a value node is of string type
////////////////////////////////////////////////////////////////////////////////

        inline bool isStringValue () const {
          return (type == NODE_TYPE_VALUE && value.type == VALUE_TYPE_STRING);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node is simple enough to be used in a simple
/// expression
////////////////////////////////////////////////////////////////////////////////

        bool isSimple () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node has a constant value
////////////////////////////////////////////////////////////////////////////////

        bool isConstant () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node is a comparison operator
////////////////////////////////////////////////////////////////////////////////

        bool isComparisonOperator () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node always produces a boolean value
////////////////////////////////////////////////////////////////////////////////

        bool alwaysProducesBoolValue () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node (and its subnodes) may throw a runtime 
/// exception
////////////////////////////////////////////////////////////////////////////////

        bool canThrow () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node (and its subnodes) is deterministic
////////////////////////////////////////////////////////////////////////////////
        
        bool isDeterministic () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of members
////////////////////////////////////////////////////////////////////////////////

        inline size_t numMembers () const {
          return members._length;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a member to the node
////////////////////////////////////////////////////////////////////////////////

        void addMember (AstNode* node) {
          if (node == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }

          int res = TRI_PushBackVectorPointer(&members, static_cast<void*>(node));

          if (res != TRI_ERROR_NO_ERROR) {
            THROW_ARANGO_EXCEPTION(res);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a member to the node
////////////////////////////////////////////////////////////////////////////////

        inline void addMember (AstNode const* node) {
          addMember(const_cast<AstNode*>(node));
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief change a member of the node
////////////////////////////////////////////////////////////////////////////////

        void changeMember (size_t i,
                           AstNode* node) {
          if (i >= members._length) {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "member out of range");
          }

          members._buffer[i] = node;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a member of the node
////////////////////////////////////////////////////////////////////////////////

        inline AstNode* getMember (size_t i) const {
          if (i >= members._length) {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "member out of range");
          }
          return static_cast<AstNode*>(members._buffer[i]);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return an optional member of the node
////////////////////////////////////////////////////////////////////////////////

        inline AstNode* getOptionalMember (size_t i) const {
          if (i >= members._length) {
            return nullptr;
          }
          return static_cast<AstNode*>(members._buffer[i]);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the node's value type
////////////////////////////////////////////////////////////////////////////////

        inline void setValueType (AstNodeValueType type) {
          value.type = type;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether this node value is of expectedType
////////////////////////////////////////////////////////////////////////////////

        inline bool isValueType (AstNodeValueType expectedType) {
          return value.type == expectedType;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the bool value of a node
////////////////////////////////////////////////////////////////////////////////

        inline bool getBoolValue () const {
          return value.value._bool;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the bool value of a node
////////////////////////////////////////////////////////////////////////////////

        inline void setBoolValue (bool v) {
          value.value._bool = v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the int value of a node, without asserting the node type
////////////////////////////////////////////////////////////////////////////////

        inline int64_t getIntValue (bool) const {
          return value.value._int;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the int value of a node, with asserting the node type
////////////////////////////////////////////////////////////////////////////////

        inline int64_t getIntValue () const {
          TRI_ASSERT(type == NODE_TYPE_VALUE);
          TRI_ASSERT(value.type == VALUE_TYPE_INT || 
                     value.type == VALUE_TYPE_DOUBLE);

          if (value.type == VALUE_TYPE_DOUBLE) {
            return static_cast<int64_t>(value.value._double);
          }

          return value.value._int;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the int value of a node
////////////////////////////////////////////////////////////////////////////////

        inline void setIntValue (int64_t v) {
          value.value._int = v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the double value of a node
////////////////////////////////////////////////////////////////////////////////

        inline double getDoubleValue () const {
          TRI_ASSERT(type == NODE_TYPE_VALUE);
          TRI_ASSERT(value.type == VALUE_TYPE_INT || 
                     value.type == VALUE_TYPE_DOUBLE);

          if (value.type == VALUE_TYPE_INT) {
            return static_cast<double>(value.value._int);
          }

          return value.value._double;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the string value of a node
////////////////////////////////////////////////////////////////////////////////

        inline void setDoubleValue (double v) {
          value.value._double = v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the string value of a node
////////////////////////////////////////////////////////////////////////////////

        inline char const* getStringValue () const {
          return value.value._string;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the string value of a node
////////////////////////////////////////////////////////////////////////////////

        inline void setStringValue (char const* v) {
          value.value._string = v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the data value of a node
////////////////////////////////////////////////////////////////////////////////

        inline void* getData () const {
          return value.value._data;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the data value of a node
////////////////////////////////////////////////////////////////////////////////

        inline void setData (void* v) {
          value.value._data = v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the data value of a node
////////////////////////////////////////////////////////////////////////////////

        inline void setData (void const* v) {
          value.value._data = const_cast<void*>(v);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clone a node, recursively
////////////////////////////////////////////////////////////////////////////////

        AstNode* clone (Ast*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief append a JavaScript representation of the node into a string buffer
////////////////////////////////////////////////////////////////////////////////

        void append (triagens::basics::StringBuffer*) const;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify the value of a node into a string buffer
////////////////////////////////////////////////////////////////////////////////

        void appendValue (triagens::basics::StringBuffer*) const;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief the node type
////////////////////////////////////////////////////////////////////////////////
  
        AstNodeType const     type;

////////////////////////////////////////////////////////////////////////////////
/// @brief the node value
////////////////////////////////////////////////////////////////////////////////

        AstNodeValue          value;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
        
      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the node's sub nodes
////////////////////////////////////////////////////////////////////////////////
      
        TRI_vector_pointer_t  members;

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
