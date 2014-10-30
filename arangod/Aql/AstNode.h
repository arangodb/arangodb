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
/// @brief type for node flags
////////////////////////////////////////////////////////////////////////////////

    typedef uint8_t AstNodeFlagsType;

////////////////////////////////////////////////////////////////////////////////
/// @brief different flags for nodes
/// the flags are used to prevent repeated calculations of node properties
/// (e.g. is the node value constant, sorted etc.)
////////////////////////////////////////////////////////////////////////////////

    enum AstNodeFlagType : uint8_t {
      FLAG_SORTED   = 1,   // node is a list and its members are sorted asc.
      FLAG_CONSTANT = 2,   // node value is constant (i.e. not dynamic)
      FLAG_DYNAMIC  = 4,   // node value is dynamic (i.e. not constant)
      FLAG_SIMPLE   = 8    // node value is simple (i.e. for use in a simple expression)

    };

////////////////////////////////////////////////////////////////////////////////
/// @brief enumeration of AST node value types
/// note: these types must be declared in asc. sort order
////////////////////////////////////////////////////////////////////////////////

    enum AstNodeValueType : uint8_t {
      VALUE_TYPE_NULL   = 0,
      VALUE_TYPE_BOOL   = 1,
      VALUE_TYPE_INT    = 2,
      VALUE_TYPE_DOUBLE = 3,
      VALUE_TYPE_STRING = 4
    };

    static_assert(VALUE_TYPE_NULL < VALUE_TYPE_BOOL, "incorrect ast node value types");
    static_assert(VALUE_TYPE_BOOL < VALUE_TYPE_INT, "incorrect ast node value types");
    static_assert(VALUE_TYPE_INT < VALUE_TYPE_DOUBLE, "incorrect ast node value types");
    static_assert(VALUE_TYPE_DOUBLE < VALUE_TYPE_STRING, "incorrect ast node value types");

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
      } 
      value;
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

    static_assert(NODE_TYPE_VALUE < NODE_TYPE_LIST, "incorrect node types");
    static_assert(NODE_TYPE_LIST < NODE_TYPE_ARRAY, "incorrect node types");

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

      explicit AstNode (AstNodeType); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create a node, with defining a value type
////////////////////////////////////////////////////////////////////////////////

      explicit AstNode (AstNodeType, AstNodeValueType); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create a boolean, with defining a value type
////////////////////////////////////////////////////////////////////////////////

      explicit AstNode (bool); 

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
/// @brief compute the JSON for a constant value node
/// the JSON is owned by the node and must not be freed by the caller
/// note that the return value might be NULL in case of OOM
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* computeJson () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief sort the members of a (list) node
/// this will also set the FLAG_SORTED flag for the node
////////////////////////////////////////////////////////////////////////////////
  
        void sort ();

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

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a JSON representation of the node to the JSON list specified
/// in the first argument
////////////////////////////////////////////////////////////////////////////////

        void toJson (TRI_json_t*,
                     TRI_memory_zone_t*,
                     bool) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the node's value to a boolean value
/// this may create a new node or return the node itself if it is already a
/// boolean value node
////////////////////////////////////////////////////////////////////////////////

        AstNode* castToBool (Ast*);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the node's value to a number value
/// this may create a new node or return the node itself if it is already a
/// numeric value node
////////////////////////////////////////////////////////////////////////////////

        AstNode* castToNumber (Ast*);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the node's value to a string value
/// this may create a new node or return the node itself if it is already a
/// string value node
////////////////////////////////////////////////////////////////////////////////

        AstNode* castToString (Ast*);

////////////////////////////////////////////////////////////////////////////////
/// @brief check a flag for the node
////////////////////////////////////////////////////////////////////////////////
  
        inline bool hasFlag (AstNodeFlagType flag) const {
          return ((flags & static_cast<decltype(flags)>(flag)) != 0); 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set a flag for the node
////////////////////////////////////////////////////////////////////////////////
  
        inline void setFlag (AstNodeFlagType flag) const {
          // ensure that CONSTANT and DYNAMIC flag are mutually exclusive
          TRI_ASSERT(flag != FLAG_DYNAMIC || (flags & static_cast<decltype(flags)>(FLAG_CONSTANT)) == 0);
          TRI_ASSERT(flag != FLAG_CONSTANT || (flags & static_cast<decltype(flags)>(FLAG_DYNAMIC)) == 0);

          flags |= static_cast<decltype(flags)>(flag);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the node value is trueish
////////////////////////////////////////////////////////////////////////////////

        bool isTrue () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the node value is falsey
////////////////////////////////////////////////////////////////////////////////

        bool isFalse () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the members of a list node are sorted
////////////////////////////////////////////////////////////////////////////////

        inline bool isSorted () const {
          return ((flags & static_cast<decltype(flags)>(FLAG_SORTED)) != 0);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a value node is NULL
////////////////////////////////////////////////////////////////////////////////

        inline bool isNullValue () const {
          return (type == NODE_TYPE_VALUE && value.type == VALUE_TYPE_NULL);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a value node is an integer
////////////////////////////////////////////////////////////////////////////////

        inline bool isIntValue () const {
          return (type == NODE_TYPE_VALUE && value.type == VALUE_TYPE_INT);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a value node is a dobule
////////////////////////////////////////////////////////////////////////////////

        inline bool isDoubleValue () const {
          return (type == NODE_TYPE_VALUE && value.type == VALUE_TYPE_DOUBLE);
        }

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
/// @brief whether or not a value node is of list type
////////////////////////////////////////////////////////////////////////////////

        inline bool isList () const {
          return (type == NODE_TYPE_LIST);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a value node is of array type
////////////////////////////////////////////////////////////////////////////////

        inline bool isArray () const {
          return (type == NODE_TYPE_ARRAY);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node is simple enough to be used in a simple
/// expression
/// this may also set the FLAG_SIMPLE flag for the node
////////////////////////////////////////////////////////////////////////////////

        bool isSimple () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node has a constant value
/// this may also set the FLAG_CONSTANT or the FLAG_DYNAMIC flags for the node
////////////////////////////////////////////////////////////////////////////////

        bool isConstant () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node is a comparison operator
////////////////////////////////////////////////////////////////////////////////

        bool isComparisonOperator () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node (and its subnodes) may throw a runtime 
/// exception
////////////////////////////////////////////////////////////////////////////////

        bool canThrow () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node (and its subnodes) can safely be executed on
/// a DB server
////////////////////////////////////////////////////////////////////////////////

        bool canRunOnDBServer () const;

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
/// @brief return the int value of a node
////////////////////////////////////////////////////////////////////////////////

        int64_t getIntValue () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief set the int value of a node
////////////////////////////////////////////////////////////////////////////////

        inline void setIntValue (int64_t v) {
          value.value._int = v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the double value of a node
////////////////////////////////////////////////////////////////////////////////

        double getDoubleValue () const;

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

        void append (triagens::basics::StringBuffer*,
                     bool) const;

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
  
        AstNodeType const         type;

////////////////////////////////////////////////////////////////////////////////
/// @brief flags for the node
////////////////////////////////////////////////////////////////////////////////

        AstNodeFlagsType mutable  flags;

////////////////////////////////////////////////////////////////////////////////
/// @brief the node value
////////////////////////////////////////////////////////////////////////////////

        AstNodeValue              value;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
        
      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief precomputed JSON value (used when executing expressions)
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t mutable*       computedJson;

////////////////////////////////////////////////////////////////////////////////
/// @brief the node's sub nodes
////////////////////////////////////////////////////////////////////////////////
      
        TRI_vector_pointer_t      members;

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
