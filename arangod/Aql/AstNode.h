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
#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/JsonHelper.h"

#include <functional>

namespace triagens {
  namespace basics {
    class StringBuffer;
  }

  namespace aql {
    class Ast;
    struct Variable;

////////////////////////////////////////////////////////////////////////////////
/// @brief type for node flags
////////////////////////////////////////////////////////////////////////////////

    typedef uint32_t AstNodeFlagsType;

////////////////////////////////////////////////////////////////////////////////
/// @brief different flags for nodes
/// the flags are used to prevent repeated calculations of node properties
/// (e.g. is the node value constant, sorted etc.)
////////////////////////////////////////////////////////////////////////////////

    enum AstNodeFlagType : AstNodeFlagsType {
      DETERMINED_SORTED             = 1,       // node is a list and its members are sorted asc.
      DETERMINED_CONSTANT           = 2,       // node value is constant (i.e. not dynamic)
      DETERMINED_SIMPLE             = 4,       // node value is simple (i.e. for use in a simple expression)
      DETERMINED_THROWS             = 8,       // node can throw an exception
      DETERMINED_NONDETERMINISTIC   = 16,      // node produces non-deterministic result (e.g. function call nodes)
      DETERMINED_RUNONDBSERVER      = 32,      // node can run on the DB server in a cluster setup
      DETERMINED_USERDEFINED        = 64,      // node contains user-defined function calls

      VALUE_SORTED                  = 128,     // node is a list and its members are sorted asc.
      VALUE_CONSTANT                = 256,     // node value is constant (i.e. not dynamic)
      VALUE_SIMPLE                  = 512,     // node value is simple (i.e. for use in a simple expression)
      VALUE_THROWS                  = 1024,    // node can throw an exception
      VALUE_NONDETERMINISTIC        = 2048,    // node produces non-deterministic result (e.g. function call nodes)
      VALUE_RUNONDBSERVER           = 4096,    // node can run on the DB server in a cluster setup
      VALUE_USERDEFINED             = 8192,    // node can run on the DB server in a cluster setup

      FLAG_KEEP_VARIABLENAME        = 16384,   // node is a reference to a variable name, not the variable value (used in KEEP nodes)
      FLAG_BIND_PARAMETER           = 32768    // node was created from a JSON bind parameter
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
      uint32_t         length; // only used for string values
      AstNodeValueType type;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief enumeration of AST node types
////////////////////////////////////////////////////////////////////////////////

    enum AstNodeType : uint32_t {
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
      NODE_TYPE_EXPANSION                     = 38,
      NODE_TYPE_ITERATOR                      = 39,
      NODE_TYPE_VALUE                         = 40,
      NODE_TYPE_ARRAY                         = 41,
      NODE_TYPE_OBJECT                        = 42,
      NODE_TYPE_OBJECT_ELEMENT                = 43,
      NODE_TYPE_COLLECTION                    = 44,
      NODE_TYPE_REFERENCE                     = 45,
      NODE_TYPE_PARAMETER                     = 46,
      NODE_TYPE_FCALL                         = 47,
      NODE_TYPE_FCALL_USER                    = 48,
      NODE_TYPE_RANGE                         = 49,
      NODE_TYPE_NOP                           = 50,
      NODE_TYPE_COLLECT_COUNT                 = 51,
      NODE_TYPE_COLLECT_EXPRESSION            = 52,
      NODE_TYPE_CALCULATED_OBJECT_ELEMENT     = 53,
      NODE_TYPE_UPSERT                        = 54,
      NODE_TYPE_EXAMPLE                       = 55,
      NODE_TYPE_PASSTHRU                      = 56,
      NODE_TYPE_ARRAY_LIMIT                   = 57,
      NODE_TYPE_DISTINCT                      = 58,
      NODE_TYPE_TRAVERSAL                     = 59,
      NODE_TYPE_COLLECTION_LIST               = 60,
      NODE_TYPE_DIRECTION                     = 61,
      NODE_TYPE_OPERATOR_NARY_AND             = 62,
      NODE_TYPE_OPERATOR_NARY_OR              = 63
    };

    static_assert(NODE_TYPE_VALUE < NODE_TYPE_ARRAY,  "incorrect node types order");
    static_assert(NODE_TYPE_ARRAY < NODE_TYPE_OBJECT, "incorrect node types order");

// -----------------------------------------------------------------------------
// --SECTION--                                                    struct AstNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the node
////////////////////////////////////////////////////////////////////////////////

    struct AstNode {
      friend class Ast;

      static std::unordered_map<int, std::string const> const Operators;
      static std::unordered_map<int, std::string const> const TypeNames;
      static std::unordered_map<int, std::string const> const ValueTypeNames;

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
/// @brief create a boolean node, with defining a value type
////////////////////////////////////////////////////////////////////////////////

      explicit AstNode (bool, AstNodeValueType); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create a boolean node, with defining a value type
////////////////////////////////////////////////////////////////////////////////

      explicit AstNode (int64_t, AstNodeValueType); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create a string node, with defining a value type
////////////////////////////////////////////////////////////////////////////////

      explicit AstNode (char const*, size_t, AstNodeValueType); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create the node from JSON
////////////////////////////////////////////////////////////////////////////////

      AstNode (Ast*,
               triagens::basics::Json const& json); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create the node from JSON
////////////////////////////////////////////////////////////////////////////////

      AstNode (std::function<void (AstNode*)> registerNode,
               std::function<char const* (std::string const&)> registerString,
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
/// @brief dump the node (for debugging purposes)
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
        void dump (int) const; 
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief compute the JSON for a constant value node
/// the JSON is owned by the node and must not be freed by the caller
/// note that the return value might be NULL in case of OOM
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* computeJson () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief sort the members of an (array) node
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
/// @brief stringify the AstNode
////////////////////////////////////////////////////////////////////////////////

        static std::string toString (AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether we know a type of this kind; throws exception if not
////////////////////////////////////////////////////////////////////////////////

        static void validateType (int type);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether we know a value type of this kind; 
/// throws exception if not
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
/// @brief adds a JSON representation of the node to the JSON array specified
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
/// @brief reset flags in case a node is changed drastically
////////////////////////////////////////////////////////////////////////////////

        inline void clearFlags () {
          flags = 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set a flag for the node
////////////////////////////////////////////////////////////////////////////////
  
        inline void setFlag (AstNodeFlagType flag) const {
          flags |= static_cast<decltype(flags)>(flag);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set two flags for the node
////////////////////////////////////////////////////////////////////////////////
  
        inline void setFlag (AstNodeFlagType typeFlag, 
                             AstNodeFlagType valueFlag) const {
          flags |= static_cast<decltype(flags)>(typeFlag | valueFlag);
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
          return ((flags & static_cast<decltype(flags)>(DETERMINED_SORTED | VALUE_SORTED)) == 
                  static_cast<decltype(flags)>(DETERMINED_SORTED | VALUE_SORTED));
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

        inline bool isArray () const {
          return (type == NODE_TYPE_ARRAY);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a value node is of array type
////////////////////////////////////////////////////////////////////////////////

        inline bool isObject () const {
          return (type == NODE_TYPE_OBJECT);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a value node is of type attribute access that
/// refers to a variable reference
////////////////////////////////////////////////////////////////////////////////

        AstNode const* getAttributeAccessForVariable () const {
          if (type != NODE_TYPE_ATTRIBUTE_ACCESS &&
              type != NODE_TYPE_EXPANSION) {
            return nullptr;
          }

          auto node = this;
     
          while (node->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                 node->type == NODE_TYPE_EXPANSION) {
            if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
              node = node->getMember(0);
            }
            else {
              // expansion, i.e. [*]
              TRI_ASSERT(node->type == NODE_TYPE_EXPANSION);
              TRI_ASSERT(node->numMembers() >= 2);

              if (node->getMember(1)->type != NODE_TYPE_REFERENCE) {
                if (node->getMember(1)->getAttributeAccessForVariable() == nullptr) {
                  return nullptr;
                }
              }
              
              TRI_ASSERT(node->getMember(0)->type == NODE_TYPE_ITERATOR);

              node = node->getMember(0)->getMember(1);
            }
          }

          if (node->type == NODE_TYPE_REFERENCE) {
            return node;
          }

          return nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a value node is of type attribute access that
/// refers to any variable reference
////////////////////////////////////////////////////////////////////////////////

        bool isAttributeAccessForVariable () const {
          return (getAttributeAccessForVariable() != nullptr);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a value node is of type attribute access that
/// refers to the specified variable reference
////////////////////////////////////////////////////////////////////////////////

        bool isAttributeAccessForVariable (Variable const* variable) const {
          auto node = getAttributeAccessForVariable();

          if (node == nullptr) {
            return false;
          }

          return (static_cast<Variable const*>(node->getData()) == variable);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a value node is of type attribute access that 
/// refers to any variable reference
/// returns true if yes, and then also returns variable reference and array
/// of attribute names in the parameter passed by reference
////////////////////////////////////////////////////////////////////////////////

        bool isAttributeAccessForVariable (std::pair<Variable const*, std::vector<triagens::basics::AttributeName>>&) const;


////////////////////////////////////////////////////////////////////////////////
/// @brief locate a variable including the direct path vector leading to it.
////////////////////////////////////////////////////////////////////////////////

      void findVariableAccess(std::vector<AstNode const*>& currentPath,
                              std::vector<std::vector<AstNode const*>>& paths,
                              Variable const* findme) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief dig through the tree and return a reference to the astnode referencing 
/// findme, nullptr otherwise.
////////////////////////////////////////////////////////////////////////////////

      AstNode const* findReference(AstNode const* findme) const;

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
/// @brief whether or not a node is a simple comparison operator
////////////////////////////////////////////////////////////////////////////////

        bool isSimpleComparisonOperator () const;

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
/// @brief whether or not a node (and its subnodes) is cacheable
////////////////////////////////////////////////////////////////////////////////

        bool isCacheable () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node (and its subnodes) may contain a call to a
/// user-defined function
////////////////////////////////////////////////////////////////////////////////

        bool callsUserDefinedFunction () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the object node contains dynamically named attributes
/// on its first level
////////////////////////////////////////////////////////////////////////////////

        bool containsDynamicAttributeName () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates whether a node of type "searchType" can be found
////////////////////////////////////////////////////////////////////////////////

        bool containsNodeType (AstNodeType searchType) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of members
////////////////////////////////////////////////////////////////////////////////

        inline size_t numMembers () const noexcept {
          return members.size();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a member to the node
////////////////////////////////////////////////////////////////////////////////

        void addMember (AstNode* node) {
          if (node == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }

          try {
            members.emplace_back(node);
          } catch (...) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
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
          if (i >= members.size()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "member out of range");
          }
          members.at(i) = node;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a member from the node
////////////////////////////////////////////////////////////////////////////////

        inline void removeMemberUnchecked (size_t i) {
          members.erase(members.begin() + i);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a member of the node
////////////////////////////////////////////////////////////////////////////////

        inline AstNode* getMember (size_t i) const {
          if (i >= members.size()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "member out of range");
          }
          return getMemberUnchecked(i);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a member of the node
////////////////////////////////////////////////////////////////////////////////

        inline AstNode* getMemberUnchecked (size_t i) const noexcept {
          return members[i];
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief sort members with a custom comparison function
////////////////////////////////////////////////////////////////////////////////
    
        void sortMembers (std::function<bool(AstNode const*, AstNode const*)> const& func) {
          std::sort(members.begin(), members.end(), func);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief reduces the number of members of the node
////////////////////////////////////////////////////////////////////////////////

        void reduceMembers (size_t i) {
          if (i > members.size()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "member out of range");
          }
          members.erase(members.begin() + i, members.end());
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
/// this will return 0 for all non-value nodes and for all non-int value nodes!!
////////////////////////////////////////////////////////////////////////////////

        int64_t getIntValue () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the int value stored for a node, regardless of the node type
////////////////////////////////////////////////////////////////////////////////

        inline int64_t getIntValue (bool) const {
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
/// @brief return the string value of a node
////////////////////////////////////////////////////////////////////////////////

        inline size_t getStringLength () const {
          return static_cast<size_t>(value.length);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the string value of a node
////////////////////////////////////////////////////////////////////////////////

        inline void setStringValue (char const* v,
                                    size_t length) {
          // note: v may contain the NUL byte
          TRI_ASSERT(v == nullptr || strlen(v) <= length);

          value.value._string = v;
          value.length = static_cast<uint32_t>(length);
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

        void stringify (triagens::basics::StringBuffer*,
                        bool,
                        bool) const;
        
        std::string toString () const;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify the value of a node into a string buffer
/// this method is used when generated JavaScript code for the node!
/// this creates an equivalent to what JSON.stringify() would do
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
      
        std::vector<AstNode*>     members;

    };

    int CompareAstNodes (AstNode const*, AstNode const*, bool);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the AstNode to an output stream
////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<< (std::ostream&, triagens::aql::AstNode const*);
std::ostream& operator<< (std::ostream&, triagens::aql::AstNode const&);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
