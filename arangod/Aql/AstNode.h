////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AST_NODE_H
#define ARANGOD_AQL_AST_NODE_H 1

#include "Basics/Common.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"

#include <velocypack/Slice.h>

#include <iosfwd>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}
namespace basics {
class StringBuffer;
}

namespace aql {
class Ast;
struct Variable;

/// @brief type for node flags
typedef uint32_t AstNodeFlagsType;

/// @brief different flags for nodes
/// the flags are used to prevent repeated calculations of node properties
/// (e.g. is the node value constant, sorted etc.)
enum AstNodeFlagType : AstNodeFlagsType {
  DETERMINED_SORTED = 1,    // node is a list and its members are sorted asc.
  DETERMINED_CONSTANT = 2,  // node value is constant (i.e. not dynamic)
  DETERMINED_SIMPLE =
      4,  // node value is simple (i.e. for use in a simple expression)
  DETERMINED_THROWS = 8,  // node can throw an exception
  DETERMINED_NONDETERMINISTIC =
      16,  // node produces non-deterministic result (e.g. function call nodes)
  DETERMINED_RUNONDBSERVER =
      32,  // node can run on the DB server in a cluster setup
  DETERMINED_CHECKUNIQUENESS = 64,  // object's keys must be checked for uniqueness

  VALUE_SORTED = 128,    // node is a list and its members are sorted asc.
  VALUE_CONSTANT = 256,  // node value is constant (i.e. not dynamic)
  VALUE_SIMPLE =
      512,  // node value is simple (i.e. for use in a simple expression)
  VALUE_THROWS = 1024,            // node can throw an exception
  VALUE_NONDETERMINISTIC = 2048,  // node produces non-deterministic result
                                  // (e.g. function call nodes)
  VALUE_RUNONDBSERVER =
      4096,                  // node can run on the DB server in a cluster setup
  VALUE_CHECKUNIQUENESS = 8192,  // object's keys must be checked for uniqueness

  FLAG_KEEP_VARIABLENAME = 16384,  // node is a reference to a variable name,
                                   // not the variable value (used in KEEP
                                   // nodes)
  FLAG_BIND_PARAMETER = 32768  // node was created from a bind parameter
};

/// @brief enumeration of AST node value types
/// note: these types must be declared in asc. sort order
enum AstNodeValueType : uint8_t {
  VALUE_TYPE_NULL = 0,
  VALUE_TYPE_BOOL = 1,
  VALUE_TYPE_INT = 2,
  VALUE_TYPE_DOUBLE = 3,
  VALUE_TYPE_STRING = 4
};

static_assert(VALUE_TYPE_NULL < VALUE_TYPE_BOOL,
              "incorrect ast node value types");
static_assert(VALUE_TYPE_BOOL < VALUE_TYPE_INT,
              "incorrect ast node value types");
static_assert(VALUE_TYPE_INT < VALUE_TYPE_DOUBLE,
              "incorrect ast node value types");
static_assert(VALUE_TYPE_DOUBLE < VALUE_TYPE_STRING,
              "incorrect ast node value types");

/// @brief AST node value
struct AstNodeValue {
  union {
    int64_t _int;
    double _double;
    bool _bool;
    char const* _string;
    void* _data;
  } value;
  uint32_t length;  // only used for string values
  AstNodeValueType type;
};

/// @brief enumeration of AST node types
enum AstNodeType : uint32_t {
  NODE_TYPE_ROOT = 0,
  NODE_TYPE_FOR = 1,
  NODE_TYPE_LET = 2,
  NODE_TYPE_FILTER = 3,
  NODE_TYPE_RETURN = 4,
  NODE_TYPE_REMOVE = 5,
  NODE_TYPE_INSERT = 6,
  NODE_TYPE_UPDATE = 7,
  NODE_TYPE_REPLACE = 8,
  NODE_TYPE_COLLECT = 9,
  NODE_TYPE_SORT = 10,
  NODE_TYPE_SORT_ELEMENT = 11,
  NODE_TYPE_LIMIT = 12,
  NODE_TYPE_VARIABLE = 13,
  NODE_TYPE_ASSIGN = 14,
  NODE_TYPE_OPERATOR_UNARY_PLUS = 15,
  NODE_TYPE_OPERATOR_UNARY_MINUS = 16,
  NODE_TYPE_OPERATOR_UNARY_NOT = 17,
  NODE_TYPE_OPERATOR_BINARY_AND = 18,
  NODE_TYPE_OPERATOR_BINARY_OR = 19,
  NODE_TYPE_OPERATOR_BINARY_PLUS = 20,
  NODE_TYPE_OPERATOR_BINARY_MINUS = 21,
  NODE_TYPE_OPERATOR_BINARY_TIMES = 22,
  NODE_TYPE_OPERATOR_BINARY_DIV = 23,
  NODE_TYPE_OPERATOR_BINARY_MOD = 24,
  NODE_TYPE_OPERATOR_BINARY_EQ = 25,
  NODE_TYPE_OPERATOR_BINARY_NE = 26,
  NODE_TYPE_OPERATOR_BINARY_LT = 27,
  NODE_TYPE_OPERATOR_BINARY_LE = 28,
  NODE_TYPE_OPERATOR_BINARY_GT = 29,
  NODE_TYPE_OPERATOR_BINARY_GE = 30,
  NODE_TYPE_OPERATOR_BINARY_IN = 31,
  NODE_TYPE_OPERATOR_BINARY_NIN = 32,
  NODE_TYPE_OPERATOR_TERNARY = 33,
  NODE_TYPE_SUBQUERY = 34,
  NODE_TYPE_ATTRIBUTE_ACCESS = 35,
  NODE_TYPE_BOUND_ATTRIBUTE_ACCESS = 36,
  NODE_TYPE_INDEXED_ACCESS = 37,
  NODE_TYPE_EXPANSION = 38,
  NODE_TYPE_ITERATOR = 39,
  NODE_TYPE_VALUE = 40,
  NODE_TYPE_ARRAY = 41,
  NODE_TYPE_OBJECT = 42,
  NODE_TYPE_OBJECT_ELEMENT = 43,
  NODE_TYPE_COLLECTION = 44,
  NODE_TYPE_REFERENCE = 45,
  NODE_TYPE_PARAMETER = 46,
  NODE_TYPE_FCALL = 47,
  NODE_TYPE_FCALL_USER = 48,
  NODE_TYPE_RANGE = 49,
  NODE_TYPE_NOP = 50,
  NODE_TYPE_COLLECT_COUNT = 51,
  NODE_TYPE_CALCULATED_OBJECT_ELEMENT = 53,
  NODE_TYPE_UPSERT = 54,
  NODE_TYPE_EXAMPLE = 55,
  NODE_TYPE_PASSTHRU = 56,
  NODE_TYPE_ARRAY_LIMIT = 57,
  NODE_TYPE_DISTINCT = 58,
  NODE_TYPE_TRAVERSAL = 59,
  NODE_TYPE_COLLECTION_LIST = 60,
  NODE_TYPE_DIRECTION = 61,
  NODE_TYPE_OPERATOR_NARY_AND = 62,
  NODE_TYPE_OPERATOR_NARY_OR = 63,
  NODE_TYPE_AGGREGATIONS = 64,
  NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ = 65,
  NODE_TYPE_OPERATOR_BINARY_ARRAY_NE = 66,
  NODE_TYPE_OPERATOR_BINARY_ARRAY_LT = 67,
  NODE_TYPE_OPERATOR_BINARY_ARRAY_LE = 68,
  NODE_TYPE_OPERATOR_BINARY_ARRAY_GT = 69,
  NODE_TYPE_OPERATOR_BINARY_ARRAY_GE = 70,
  NODE_TYPE_OPERATOR_BINARY_ARRAY_IN = 71,
  NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN = 72,
  NODE_TYPE_QUANTIFIER = 73,
  NODE_TYPE_WITH = 74,
  NODE_TYPE_SHORTEST_PATH = 75,
};

static_assert(NODE_TYPE_VALUE < NODE_TYPE_ARRAY, "incorrect node types order");
static_assert(NODE_TYPE_ARRAY < NODE_TYPE_OBJECT, "incorrect node types order");

/// @brief the node
struct AstNode {
  friend class Ast;

  static std::unordered_map<int, std::string const> const Operators;
  static std::unordered_map<int, std::string const> const TypeNames;
  static std::unordered_map<int, std::string const> const ValueTypeNames;

  /// @brief create the node
  explicit AstNode(AstNodeType);

  /// @brief create a node, with defining a value type
  AstNode(AstNodeType, AstNodeValueType);

  /// @brief create a boolean node, with defining a value type
  AstNode(bool, AstNodeValueType);

  /// @brief create a boolean node, with defining a value type
  AstNode(int64_t, AstNodeValueType);

  /// @brief create a string node, with defining a value type
  AstNode(char const*, size_t, AstNodeValueType);

  /// @brief create the node from VPack
  AstNode(Ast*, arangodb::velocypack::Slice const& slice);

  /// @brief create the node from VPack
  AstNode(std::function<void(AstNode*)> registerNode,
          std::function<char const*(std::string const&)> registerString,
          arangodb::velocypack::Slice const& slice);

  /// @brief destroy the node
  ~AstNode();

 public:

  static constexpr size_t SortNumberThreshold = 8;

  /// @brief return the string value of a node, as an std::string
  std::string getString() const;

  /// @brief test if all members of a node are equality comparisons
  bool isOnlyEqualityMatch() const;

  /// @brief computes a hash value for a value node
  uint64_t hashValue(uint64_t) const;

/// @brief dump the node (for debugging purposes)
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  void dump(int indent) const;
#endif

  /// @brief compute the value for a constant value node
  /// the value is owned by the node and must not be freed by the caller
  arangodb::velocypack::Slice computeValue() const;

  /// @brief sort the members of an (array) node
  /// this will also set the FLAG_SORTED flag for the node
  void sort();

  /// @brief return the type name of a node
  std::string const& getTypeString() const;

  /// @brief return the value type name of a node
  std::string const& getValueTypeString() const;

  /// @brief stringify the AstNode
  static std::string toString(AstNode const*);

  /// @brief checks whether we know a type of this kind; throws exception if not
  static void validateType(int type);

  /// @brief checks whether we know a value type of this kind;
  /// throws exception if not
  static void validateValueType(int type);

  /// @brief fetch a node's type from VPack
  static AstNodeType getNodeTypeFromVPack(arangodb::velocypack::Slice const& slice);

  /// @brief return a VelocyPack representation of the node value
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPackValue() const;

  /// @brief build a VelocyPack representation of the node value
  ///        Can throw Out of Memory Error
  void toVelocyPackValue(arangodb::velocypack::Builder&) const;

  /// @brief return a VelocyPack representation of the node
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack(bool) const;

  /// @brief Create a VelocyPack representation of the node
  void toVelocyPack(arangodb::velocypack::Builder&, bool) const;

  /// @brief convert the node's value to a boolean value
  /// this may create a new node or return the node itself if it is already a
  /// boolean value node
  AstNode const* castToBool(Ast*) const;

  /// @brief convert the node's value to a number value
  /// this may create a new node or return the node itself if it is already a
  /// numeric value node
  AstNode const* castToNumber(Ast*) const;

  /// @brief check a flag for the node
  inline bool hasFlag(AstNodeFlagType flag) const {
    return ((flags & static_cast<decltype(flags)>(flag)) != 0);
  }

  /// @brief reset flags in case a node is changed drastically
  inline void clearFlags() { flags = 0; }
  
  /// @brief recursively clear flags
  void clearFlagsRecursive();

  /// @brief set a flag for the node
  inline void setFlag(AstNodeFlagType flag) const {
    flags |= static_cast<decltype(flags)>(flag);
  }

  /// @brief set two flags for the node
  inline void setFlag(AstNodeFlagType typeFlag,
                      AstNodeFlagType valueFlag) const {
    flags |= static_cast<decltype(flags)>(typeFlag | valueFlag);
  }

  /// @brief whether or not the node value is trueish
  bool isTrue() const;

  /// @brief whether or not the node value is falsey
  bool isFalse() const;

  /// @brief whether or not the members of a list node are sorted
  inline bool isSorted() const {
    return ((flags &
             static_cast<decltype(flags)>(DETERMINED_SORTED | VALUE_SORTED)) ==
            static_cast<decltype(flags)>(DETERMINED_SORTED | VALUE_SORTED));
  }

  /// @brief whether or not a value node is NULL
  inline bool isNullValue() const {
    return (type == NODE_TYPE_VALUE && value.type == VALUE_TYPE_NULL);
  }

  /// @brief whether or not a value node is an integer
  inline bool isIntValue() const {
    return (type == NODE_TYPE_VALUE && value.type == VALUE_TYPE_INT);
  }

  /// @brief whether or not a value node is a dobule
  inline bool isDoubleValue() const {
    return (type == NODE_TYPE_VALUE && value.type == VALUE_TYPE_DOUBLE);
  }

  /// @brief whether or not a value node is of numeric type
  inline bool isNumericValue() const {
    return (type == NODE_TYPE_VALUE &&
            (value.type == VALUE_TYPE_INT || value.type == VALUE_TYPE_DOUBLE));
  }

  /// @brief whether or not a value node is of bool type
  inline bool isBoolValue() const {
    return (type == NODE_TYPE_VALUE && value.type == VALUE_TYPE_BOOL);
  }

  /// @brief whether or not a value node is of string type
  inline bool isStringValue() const {
    return (type == NODE_TYPE_VALUE && value.type == VALUE_TYPE_STRING);
  }

  /// @brief whether or not a value node is of list type
  inline bool isArray() const { return (type == NODE_TYPE_ARRAY); }

  /// @brief whether or not a value node is of array type
  inline bool isObject() const { return (type == NODE_TYPE_OBJECT); }

  /// @brief whether or not a value node is of type attribute access that
  /// refers to a variable reference
  AstNode const* getAttributeAccessForVariable(bool allowIndexedAccess) const {
    if (type != NODE_TYPE_ATTRIBUTE_ACCESS && type != NODE_TYPE_EXPANSION
        && !(allowIndexedAccess && type == NODE_TYPE_INDEXED_ACCESS)) {
      return nullptr;
    }

    auto node = this;

    while (node->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
           (allowIndexedAccess && node->type == NODE_TYPE_INDEXED_ACCESS) ||
           node->type == NODE_TYPE_EXPANSION) {
      if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS || node->type == NODE_TYPE_INDEXED_ACCESS) {
        node = node->getMember(0);
      } else {
        // expansion, i.e. [*]
        TRI_ASSERT(node->type == NODE_TYPE_EXPANSION);
        TRI_ASSERT(node->numMembers() >= 2);

        if (node->getMember(1)->type != NODE_TYPE_REFERENCE) {
          if (node->getMember(1)->getAttributeAccessForVariable(allowIndexedAccess) == nullptr) {
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

  /// @brief whether or not a value node is of type attribute access that
  /// refers to any variable reference
  bool isAttributeAccessForVariable() const {
    return (getAttributeAccessForVariable(false) != nullptr);
  }

  /// @brief whether or not a value node is of type attribute access that
  /// refers to the specified variable reference
  bool isAttributeAccessForVariable(Variable const* variable, bool allowIndexedAccess) const {
    auto node = getAttributeAccessForVariable(allowIndexedAccess);

    if (node == nullptr) {
      return false;
    }

    return (static_cast<Variable const*>(node->getData()) == variable);
  }

  /// @brief whether or not a value node is of type attribute access that
  /// refers to any variable reference
  /// returns true if yes, and then also returns variable reference and array
  /// of attribute names in the parameter passed by reference
  bool isAttributeAccessForVariable(
      std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>&,
                                    bool allowIndexedAccess = false)
      const;

  /// @brief locate a variable including the direct path vector leading to it.
  void findVariableAccess(std::vector<AstNode const*>& currentPath,
                          std::vector<std::vector<AstNode const*>>& paths,
                          Variable const* findme) const;

  /// @brief dig through the tree and return a reference to the astnode
  /// referencing
  /// findme, nullptr otherwise.
  AstNode const* findReference(AstNode const* findme) const;

  /// @brief whether or not a node is simple enough to be used in a simple
  /// expression
  /// this may also set the FLAG_SIMPLE flag for the node
  bool isSimple() const;

  /// @brief whether or not a node has a constant value
  /// this may also set the FLAG_CONSTANT or the FLAG_DYNAMIC flags for the node
  bool isConstant() const;

  /// @brief whether or not a node is a simple comparison operator
  bool isSimpleComparisonOperator() const;

  /// @brief whether or not a node is a comparison operator
  bool isComparisonOperator() const;
  
  /// @brief whether or not a node is an array comparison operator
  bool isArrayComparisonOperator() const;

  /// @brief whether or not a node (and its subnodes) may throw a runtime
  /// exception
  bool canThrow() const;

  /// @brief whether or not a node (and its subnodes) can safely be executed on
  /// a DB server
  bool canRunOnDBServer() const;
  
  /// @brief whether or not an object's keys must be checked for uniqueness
  bool mustCheckUniqueness() const;

  /// @brief whether or not a node (and its subnodes) is deterministic
  bool isDeterministic() const;

  /// @brief whether or not a node (and its subnodes) is cacheable
  bool isCacheable() const;

  /// @brief whether or not a node (and its subnodes) may contain a call to a
  /// user-defined function
  bool callsUserDefinedFunction() const;
  
  /// @brief whether or not a node (and its subnodes) may contain a call to a
  /// a function or a user-defined function
  bool callsFunction() const;

  /// @brief whether or not the object node contains dynamically named
  /// attributes
  /// on its first level
  bool containsDynamicAttributeName() const;

  /// @brief iterates whether a node of type "searchType" can be found
  bool containsNodeType(AstNodeType searchType) const;

  /// @brief return the number of members
  inline size_t numMembers() const noexcept { return members.size(); }

  /// @brief reserve space for members
  void reserve(size_t n) {
    members.reserve(n);
  }

  /// @brief add a member to the node
  void addMember(AstNode* node) {
    if (node == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    try {
      members.emplace_back(node);
    } catch (...) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  /// @brief add a member to the node
  inline void addMember(AstNode const* node) {
    addMember(const_cast<AstNode*>(node));
  }

  /// @brief change a member of the node
  void changeMember(size_t i, AstNode* node) {
    if (i >= members.size()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "member out of range");
    }
    members.at(i) = node;
  }

  /// @brief remove a member from the node
  inline void removeMemberUnchecked(size_t i) {
    members.erase(members.begin() + i);
  }

  /// @brief return a member of the node
  inline AstNode* getMember(size_t i) const {
    if (i >= members.size()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "member out of range");
    }
    return getMemberUnchecked(i);
  }

  /// @brief return a member of the node
  inline AstNode* getMemberUnchecked(size_t i) const noexcept {
    return members[i];
  }

  /// @brief sort members with a custom comparison function
  void sortMembers(
      std::function<bool(AstNode const*, AstNode const*)> const& func) {
    std::sort(members.begin(), members.end(), func);
  }

  /// @brief reduces the number of members of the node
  void reduceMembers(size_t i) {
    if (i > members.size()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "member out of range");
    }
    members.erase(members.begin() + i, members.end());
  }
  
  inline void clearMembers() {
    members.clear();
  }

  /// @brief set the node's value type
  inline void setValueType(AstNodeValueType type) { value.type = type; }

  /// @brief check whether this node value is of expectedType
  inline bool isValueType(AstNodeValueType expectedType) {
    return value.type == expectedType;
  }

  /// @brief return the bool value of a node
  inline bool getBoolValue() const { return value.value._bool; }

  /// @brief set the bool value of a node
  inline void setBoolValue(bool v) { value.value._bool = v; }

  /// @brief return the int value of a node
  /// this will return 0 for all non-value nodes and for all non-int value
  /// nodes!!
  int64_t getIntValue() const;

  /// @brief return the int value stored for a node, regardless of the node type
  inline int64_t getIntValue(bool) const { return value.value._int; }

  /// @brief set the int value of a node
  inline void setIntValue(int64_t v) { value.value._int = v; }

  /// @brief return the double value of a node
  double getDoubleValue() const;

  /// @brief set the string value of a node
  inline void setDoubleValue(double v) { value.value._double = v; }

  /// @brief return the string value of a node
  inline char const* getStringValue() const { return value.value._string; }

  /// @brief return the string value of a node
  inline size_t getStringLength() const {
    return static_cast<size_t>(value.length);
  }

  /// @brief set the string value of a node
  inline void setStringValue(char const* v, size_t length) {
    // note: v may contain the NUL byte and is not necessarily
    // null-terminated itself (if from VPack)
    value.type = VALUE_TYPE_STRING;
    value.value._string = v;
    value.length = static_cast<uint32_t>(length);
  }
  
  /// @brief whether or not a string is equal to another
  inline bool stringEquals(char const* other, bool caseInsensitive) const {
    if (caseInsensitive) {
      return (strncasecmp(getStringValue(), other, getStringLength()) == 0);
    }
    return (strncmp(getStringValue(), other, getStringLength()) == 0);
  }
  
  /// @brief whether or not a string is equal to another
  inline bool stringEquals(std::string const& other) const {
    return (other.size() == getStringLength() && memcmp(other.c_str(), getStringValue(), getStringLength()) == 0);
  }

  /// @brief return the data value of a node
  inline void* getData() const { return value.value._data; }

  /// @brief set the data value of a node
  inline void setData(void* v) { value.value._data = v; }

  /// @brief set the data value of a node
  inline void setData(void const* v) {
    value.value._data = const_cast<void*>(v);
  }

  /// @brief clone a node, recursively
  AstNode* clone(Ast*) const;

  /// @brief append a string representation of the node into a string buffer
  /// the string representation does not need to be JavaScript-compatible
  /// except for node types NODE_TYPE_VALUE, NODE_TYPE_ARRAY and NODE_TYPE_OBJECT
  /// (only for objects that do not contain dynamic attributes)
  /// note that this may throw and that the caller is responsible for
  /// catching the error
  void stringify(arangodb::basics::StringBuffer*, bool, bool) const;

  /// note that this may throw and that the caller is responsible for
  /// catching the error
  std::string toString() const;

  /// @brief stringify the value of a node into a string buffer
  /// this method is used when generated JavaScript code for the node!
  /// this creates an equivalent to what JSON.stringify() would do
  void appendValue(arangodb::basics::StringBuffer*) const;

  /// @brief Steals the computed value and frees it.
  void stealComputedValue();


  /// @brief Removes all members from the current node that are also
  ///        members of the other node (ignoring ording)
  ///        Can only be applied if this and other are of type
  ///        n-ary-and
  void removeMembersInOtherAndNode(AstNode const* other);

 public:
  /// @brief the node type
  AstNodeType const type;

  /// @brief flags for the node
  AstNodeFlagsType mutable flags;

  /// @brief the node value
  AstNodeValue value;

 private:
  /// @brief precomputed VPack value (used when executing expressions)
  uint8_t mutable* computedValue;

  /// @brief the node's sub nodes
  std::vector<AstNode*> members;
};

int CompareAstNodes(AstNode const* lhs, AstNode const* rhs, bool compareUtf8);

/// @brief less comparator for Ast value nodes
template <bool useUtf8>
struct AstNodeValueLess {
  inline bool operator()(AstNode const* lhs, AstNode const* rhs) const {
    return CompareAstNodes(lhs, rhs, useUtf8) < 0;
  }
};

struct AstNodeValueHash {
  inline size_t operator()(AstNode const* value) const {
    return static_cast<size_t>(value->hashValue(0x12345678));
  }
};

struct AstNodeValueEqual {
  inline bool operator()(AstNode const* lhs, AstNode const* rhs) const {
    return CompareAstNodes(lhs, rhs, false) == 0;
  }
};
}
}

/// @brief append the AstNode to an output stream
std::ostream& operator<<(std::ostream&, arangodb::aql::AstNode const*);
std::ostream& operator<<(std::ostream&, arangodb::aql::AstNode const&);

#endif
