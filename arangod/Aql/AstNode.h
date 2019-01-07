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

#include "Basics/AttributeNameParser.h"
#include "Basics/Common.h"
#include "Basics/Exceptions.h"

#include <velocypack/Slice.h>

#include <iosfwd>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
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
  DETERMINED_SORTED = 0x0000001,  // node is a list and its members are sorted asc.
  DETERMINED_CONSTANT = 0x0000002,  // node value is constant (i.e. not dynamic)
  DETERMINED_SIMPLE = 0x0000004,  // node value is simple (i.e. for use in a simple expression)
  DETERMINED_NONDETERMINISTIC = 0x0000010,  // node produces non-deterministic
                                            // result (e.g. function call nodes)
  DETERMINED_RUNONDBSERVER = 0x0000020,  // node can run on the DB server in a cluster setup
  DETERMINED_CHECKUNIQUENESS = 0x0000040,  // object's keys must be checked for uniqueness
  DETERMINED_V8 = 0x0000080,               // node will use V8 internally

  VALUE_SORTED = 0x0000100,    // node is a list and its members are sorted asc.
  VALUE_CONSTANT = 0x0000200,  // node value is constant (i.e. not dynamic)
  VALUE_SIMPLE = 0x0000400,  // node value is simple (i.e. for use in a simple expression)
  VALUE_NONDETERMINISTIC = 0x0001000,  // node produces non-deterministic result
                                       // (e.g. function call nodes)
  VALUE_RUNONDBSERVER = 0x0002000,  // node can run on the DB server in a cluster setup
  VALUE_CHECKUNIQUENESS = 0x0004000,  // object's keys must be checked for uniqueness
  VALUE_V8 = 0x0008000,               // node will use V8 internally
  FLAG_KEEP_VARIABLENAME = 0x0010000,  // node is a reference to a variable name, not the variable
                                       // value (used in KEEP nodes)
  FLAG_BIND_PARAMETER = 0x0020000,  // node was created from a bind parameter
  FLAG_FINALIZED = 0x0040000,  // node has been finalized and should not be modified; only
                               // set and checked in maintainer mode
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
  union Value {
    int64_t _int;
    double _double;
    bool _bool;
    char const* _string;
    void* _data;

    // constructors for union values
    explicit Value(int64_t value) : _int(value) {}
    explicit Value(double value) : _double(value) {}
    explicit Value(bool value) : _bool(value) {}
    explicit Value(char const* value) : _string(value) {}
  };

  Value value;
  uint32_t length;  // only used for string values
  AstNodeValueType type;

  AstNodeValue() : value(int64_t(0)), length(0), type(VALUE_TYPE_NULL) {}
  explicit AstNodeValue(int64_t value)
      : value(value), length(0), type(VALUE_TYPE_INT) {}
  explicit AstNodeValue(double value)
      : value(value), length(0), type(VALUE_TYPE_DOUBLE) {}
  explicit AstNodeValue(bool value)
      : value(value), length(0), type(VALUE_TYPE_BOOL) {}
  explicit AstNodeValue(char const* value, uint32_t length)
      : value(value), length(length), type(VALUE_TYPE_STRING) {}
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
  NODE_TYPE_VIEW = 76,
  NODE_TYPE_PARAMETER_DATASOURCE = 77,
  NODE_TYPE_FOR_VIEW = 78,
};

static_assert(NODE_TYPE_VALUE < NODE_TYPE_ARRAY, "incorrect node types order");
static_assert(NODE_TYPE_ARRAY < NODE_TYPE_OBJECT, "incorrect node types order");

/// @brief the node
struct AstNode {
  friend class Ast;

  static std::unordered_map<int, std::string const> const Operators;
  static std::unordered_map<int, std::string const> const TypeNames;
  static std::unordered_map<int, std::string const> const ValueTypeNames;

  /// @brief helper for building flags
  template <typename... Args>
  static inline std::underlying_type<AstNodeFlagType>::type makeFlags(AstNodeFlagType flag,
                                                                      Args... args) {
    return static_cast<std::underlying_type<AstNodeFlagType>::type>(flag) +
           makeFlags(args...);
  }

  static inline std::underlying_type<AstNodeFlagType>::type makeFlags() {
    return static_cast<std::underlying_type<AstNodeFlagType>::type>(0);
  }

  /// @brief create the node
  explicit AstNode(AstNodeType);

  /// @brief create a node, with defining a value
  explicit AstNode(AstNodeValue value);

  /// @brief create the node from VPack
  explicit AstNode(Ast*, arangodb::velocypack::Slice const& slice);

  /// @brief create the node from VPack
  explicit AstNode(std::function<void(AstNode*)> const& registerNode,
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
  uint64_t hashValue(uint64_t) const noexcept;

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
  inline void setFlag(AstNodeFlagType flag) const { flags |= flag; }

  /// @brief set two flags for the node
  inline void setFlag(AstNodeFlagType typeFlag, AstNodeFlagType valueFlag) const {
    flags |= (typeFlag | valueFlag);
  }

  /// @brief remove a flag for the node
  inline void removeFlag(AstNodeFlagType flag) const { flags &= ~flag; }

  /// @brief whether or not the node value is trueish
  bool isTrue() const;

  /// @brief whether or not the node value is falsey
  bool isFalse() const;

  /// @brief whether or not the members of a list node are sorted
  inline bool isSorted() const {
    return ((flags & (DETERMINED_SORTED | VALUE_SORTED)) == (DETERMINED_SORTED | VALUE_SORTED));
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
    if (type != NODE_TYPE_ATTRIBUTE_ACCESS && type != NODE_TYPE_EXPANSION &&
        !(allowIndexedAccess && type == NODE_TYPE_INDEXED_ACCESS)) {
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
      bool allowIndexedAccess = false) const;

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

  /// @brief whether or not a node will use V8 internally
  /// this may also set the FLAG_V8 flag for the node
  bool willUseV8() const;

  /// @brief whether or not a node is a simple comparison operator
  bool isSimpleComparisonOperator() const;

  /// @brief whether or not a node is a comparison operator
  bool isComparisonOperator() const;

  /// @brief whether or not a node is an array comparison operator
  bool isArrayComparisonOperator() const;

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
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    members.reserve(n);
  }

  /// @brief add a member to the node
  void addMember(AstNode* node) {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
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
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    if (i >= members.size()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "member out of range");
    }
    members[i] = node;
  }

  /// @brief remove a member from the node
  inline void removeMemberUnchecked(size_t i) {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    members.erase(members.begin() + i);
  }

  /// @brief remove all members from the node at once
  void removeMembers() {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    members.clear();
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
  void sortMembers(std::function<bool(AstNode const*, AstNode const*)> const& func) {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    std::sort(members.begin(), members.end(), func);
  }

  /// @brief reduces the number of members of the node
  void reduceMembers(size_t i) {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    if (i > members.size()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "member out of range");
    }
    members.erase(members.begin() + i, members.end());
  }

  inline void clearMembers() {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    members.clear();
  }

  /// @brief mark a < or <= operator to definitely exclude the "null" value
  /// this is only for optimization purposes
  inline void setExcludesNull(bool v = true) {
    TRI_ASSERT(type == NODE_TYPE_OPERATOR_BINARY_LT || type == NODE_TYPE_OPERATOR_BINARY_LE ||
               type == NODE_TYPE_OPERATOR_BINARY_EQ);
    value.value._bool = v;
  }

  /// @brief check if an < or <= operator definitely excludes the "null" value
  /// this is only for optimization purposes
  inline bool getExcludesNull() const noexcept {
    TRI_ASSERT(type == NODE_TYPE_OPERATOR_BINARY_LT || type == NODE_TYPE_OPERATOR_BINARY_LE ||
               type == NODE_TYPE_OPERATOR_BINARY_EQ);
    return value.value._bool;
  }

  /// @brief set the node's value type
  inline void setValueType(AstNodeValueType type) {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    value.type = type;
  }

  /// @brief check whether this node value is of expectedType
  inline bool isValueType(AstNodeValueType expectedType) const noexcept {
    return value.type == expectedType;
  }

  /// @brief return the bool value of a node
  inline bool getBoolValue() const noexcept { return value.value._bool; }

  /// @brief set the bool value of a node
  inline void setBoolValue(bool v) {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    value.value._bool = v;
  }

  /// @brief return the int value of a node
  /// this will return 0 for all non-value nodes and for all non-int value
  /// nodes!!
  int64_t getIntValue() const;

  /// @brief return the int value stored for a node, regardless of the node type
  inline int64_t getIntValue(bool) const noexcept { return value.value._int; }

  /// @brief set the int value of a node
  inline void setIntValue(int64_t v) {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    value.value._int = v;
  }

  /// @brief return the double value of a node
  double getDoubleValue() const;

  /// @brief set the string value of a node
  inline void setDoubleValue(double v) {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    value.value._double = v;
  }

  /// @brief return the string value of a node
  inline char const* getStringValue() const { return value.value._string; }

  /// @brief return the string value of a node
  inline size_t getStringLength() const {
    return static_cast<size_t>(value.length);
  }

  /// @brief set the string value of a node
  inline void setStringValue(char const* v, size_t length) {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
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
    return (other.size() == getStringLength() &&
            memcmp(other.c_str(), getStringValue(), getStringLength()) == 0);
  }

  /// @brief return the data value of a node
  inline void* getData() const { return value.value._data; }

  /// @brief set the data value of a node
  inline void setData(void* v) {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    value.value._data = v;
  }

  /// @brief set the data value of a node
  inline void setData(void const* v) {
    TRI_ASSERT(!hasFlag(AstNodeFlagType::FLAG_FINALIZED));
    value.value._data = const_cast<void*>(v);
  }

  /// @brief clone a node, recursively
  AstNode* clone(Ast*) const;

  /// @brief validate that given node is an object with const-only values
  bool isConstObject() const;

  /// @brief append a string representation of the node into a string buffer
  /// the string representation does not need to be JavaScript-compatible
  /// except for node types NODE_TYPE_VALUE, NODE_TYPE_ARRAY and
  /// NODE_TYPE_OBJECT (only for objects that do not contain dynamic attributes)
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
  ///        members of the other node (ignoring ordering)
  ///        Can only be applied if this and other are of type
  ///        n-ary-and
  void removeMembersInOtherAndNode(AstNode const* other);

  /// @brief If the node has not been marked finalized, mark its subtree so.
  /// If it runs into a finalized node, it assumes the whole subtree beneath
  /// it is marked already and exits early; otherwise it will finalize the node
  /// and recurse on its subtree.
  static void markFinalized(AstNode* subtreeRoot);

 public:
  /// @brief the node type
  AstNodeType type;

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
  inline size_t operator()(AstNode const* value) const noexcept {
    return static_cast<size_t>(value->hashValue(0x12345678));
  }
};

struct AstNodeValueEqual {
  inline bool operator()(AstNode const* lhs, AstNode const* rhs) const {
    return CompareAstNodes(lhs, rhs, false) == 0;
  }
};
}  // namespace aql
}  // namespace arangodb

/// @brief append the AstNode to an output stream
std::ostream& operator<<(std::ostream&, arangodb::aql::AstNode const*);
std::ostream& operator<<(std::ostream&, arangodb::aql::AstNode const&);

/** README
 * Typically, the pattern should be that once an AstNode is created and inserted
 * into the tree, it is immutable. Currently there are some places where this
 * is not the case, so we cannot currently enforce it via typing.
 *
 * Instead, we have a flag which we set and check only in maintainer mode,
 * `FLAG_FINALIZED` to signal that the node should not change state. This allows
 * us to take a more functional style approach to building and modifying the
 * Ast, allowing us to simply point to the same node multiple times instead of
 * creating several copies when we need to e.g. distribute an AND operation.
 *
 * If you need to modify a node, try to replace it with a new one. You can
 * create a copy-for-modify node via `Ast::shallowCopyForModify`. You can then
 * set it to be finalized when it goes out of scope using
 * `TRI_DEFER(FINALIZE_SUBTREE(node))`.
 *
 * If you cannot follow this pattern, for instance if you do not have access
 * to the Ast or to the node's ancestors, then you might be able to get away
 * with temporarily unlocking the node and modifying it in place. This could
 * create correctness issues in our optmization code though, so do this with
 * extreme caution. The correct way to do this is via the
 * `TEMPORARILY_UNLOCK_NODE(node)` macro. It will relock the node when the macro
 * instance goes out of scope.
 */

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#define FINALIZE_SUBTREE(n) \
  arangodb::aql::AstNode::markFinalized(const_cast<arangodb::aql::AstNode*>(n))
#define FINALIZE_SUBTREE_CONDITIONAL(n, b) \
  if (b) {                                 \
    FINALIZE_SUBTREE(n);                   \
  }
#define TEMPORARILY_UNLOCK_NODE(n)                                                         \
  bool wasFinalizedAlready = (n)->hasFlag(arangodb::aql::AstNodeFlagType::FLAG_FINALIZED); \
  if (wasFinalizedAlready) {                                                               \
    (n)->flags = ((n)->flags & ~arangodb::aql::AstNodeFlagType::FLAG_FINALIZED);           \
  }                                                                                        \
  TRI_DEFER(FINALIZE_SUBTREE_CONDITIONAL(n, wasFinalizedAlready));
#else
#define FINALIZE_SUBTREE(n) \
  while (0) {               \
    (void)(n);              \
  }                         \
  do {                      \
  } while (0)
#define FINALIZE_SUBTREE_CONDITIONAL(n, b) \
  while (0) {                              \
    (void)(n);                             \
  }                                        \
  do {                                     \
  } while (0)
#define TEMPORARILY_UNLOCK_NODE(n) \
  while (0) {                      \
    (void)(n);                     \
  }                                \
  do {                             \
  } while (0)
#endif

#endif
