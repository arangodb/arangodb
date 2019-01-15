////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AqlValue.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "VelocyPackHelper.h"

#include "search/sort.hpp"
#include "utils/noncopyable.hpp"
#include "utils/string.hpp"

#ifndef ARANGOD_IRESEARCH__AQL_HELPER_H
#define ARANGOD_IRESEARCH__AQL_HELPER_H 1

#if defined(__GNUC__)
#pragma GCC diagnostic push
#if (__GNUC__ >= 7)
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough=0"
#endif
#endif

namespace arangodb {
namespace aql {

struct Variable;          // forward declaration
class ExecutionPlan;      // forward declaration
class ExpressionContext;  // forward declaration
class Ast;                // forward declaration

}  // namespace aql

namespace transaction {

class Methods;  // forward declaration

}  // namespace transaction

namespace iresearch {

//////////////////////////////////////////////////////////////////////////////
/// @brief extracts string_ref from an AstNode, note that provided 'node'
///        must be an arangodb::aql::VALUE_TYPE_STRING
/// @return extracted string_ref
//////////////////////////////////////////////////////////////////////////////
inline irs::string_ref getStringRef(aql::AstNode const& node) {
  TRI_ASSERT(aql::VALUE_TYPE_STRING == node.value.type);

  return irs::string_ref(node.getStringValue(), node.getStringLength());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to extract 'size_t' value from the specified AstNode 'node'
/// @returns true on success, false otherwise
////////////////////////////////////////////////////////////////////////////////
inline bool parseValue(size_t& value, aql::AstNode const& node) {
  switch (node.value.type) {
    case aql::VALUE_TYPE_NULL:
    case aql::VALUE_TYPE_BOOL:
      return false;
    case aql::VALUE_TYPE_INT:
    case aql::VALUE_TYPE_DOUBLE:
      value = node.getIntValue();
      return true;
    case aql::VALUE_TYPE_STRING:
      return false;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to extract 'irs::basic_string_ref<Char>' value from the
///         specified AstNode 'node'
/// @returns true on success, false otherwise
////////////////////////////////////////////////////////////////////////////////
template <typename String>
inline bool parseValue(String& value, aql::AstNode const& node) {
  typedef typename String::traits_type traits_t;

  switch (node.value.type) {
    case aql::VALUE_TYPE_NULL:
    case aql::VALUE_TYPE_BOOL:
    case aql::VALUE_TYPE_INT:
    case aql::VALUE_TYPE_DOUBLE:
      return false;
    case aql::VALUE_TYPE_STRING:
      value = String(reinterpret_cast<typename traits_t::char_type const*>(node.getStringValue()),
                     node.getStringLength());
      return true;
  }

  return false;
}

template <typename Visitor>
bool visit(aql::SortCondition const& sort, Visitor const& visitor) {
  for (size_t i = 0, size = sort.numAttributes(); i < size; ++i) {
    auto entry = sort.field(i);

    if (!visitor(std::get<0>(entry), std::get<1>(entry), std::get<2>(entry))) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visits the specified node using the provided 'visitor' according
///        to the specified visiting strategy (preorder/postorder)
////////////////////////////////////////////////////////////////////////////////
template <bool Preorder, typename Visitor>
bool visit(aql::AstNode const& root, Visitor visitor) {
  if (Preorder && !visitor(root)) {
    return false;
  }

  size_t const n = root.numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto const* member = root.getMemberUnchecked(i);
    TRI_ASSERT(member);

    if (!visit<Preorder>(*member, visitor)) {
      return false;
    }
  }

  if (!Preorder && !visitor(root)) {
    return false;
  }

  return true;
}

enum ScopedValueType {
  SCOPED_VALUE_TYPE_INVALID = 0,
  SCOPED_VALUE_TYPE_NULL,
  SCOPED_VALUE_TYPE_BOOL,
  SCOPED_VALUE_TYPE_DOUBLE,
  SCOPED_VALUE_TYPE_STRING,
  SCOPED_VALUE_TYPE_ARRAY,
  SCOPED_VALUE_TYPE_RANGE
};

////////////////////////////////////////////////////////////////////////////////
/// @struct AqlValueTraits
////////////////////////////////////////////////////////////////////////////////
struct AqlValueTraits {
  static ScopedValueType type(aql::AqlValue const& value) noexcept {
    typedef typename std::underlying_type<ScopedValueType>::type underlying_t;

    underlying_t const typeIndex =
        value.isNull(false) + 2 * value.isBoolean() + 3 * value.isNumber() +
        4 * value.isString() + 5 * value.isArray() +
        value.isRange();  // isArray() returns `true` in case of range too

    return static_cast<ScopedValueType>(typeIndex);
  }

  static ScopedValueType type(aql::AstNode const& node) noexcept {
    switch (node.type) {
      case aql::NODE_TYPE_VALUE:
        switch (node.value.type) {
          case aql::VALUE_TYPE_NULL:
            return SCOPED_VALUE_TYPE_NULL;
          case aql::VALUE_TYPE_BOOL:
            return SCOPED_VALUE_TYPE_BOOL;
          case aql::VALUE_TYPE_INT:  // all numerics are doubles here
          case aql::VALUE_TYPE_DOUBLE:
            return SCOPED_VALUE_TYPE_DOUBLE;
          case aql::VALUE_TYPE_STRING:
            return SCOPED_VALUE_TYPE_STRING;
          default:
            return SCOPED_VALUE_TYPE_INVALID;
        }
      case aql::NODE_TYPE_ARRAY:
        return SCOPED_VALUE_TYPE_ARRAY;
      case aql::NODE_TYPE_RANGE:
        return SCOPED_VALUE_TYPE_RANGE;
      default:
        return SCOPED_VALUE_TYPE_INVALID;
    }
  }
};  // AqlValueTraits

////////////////////////////////////////////////////////////////////////////////
/// @struct QueryContext
////////////////////////////////////////////////////////////////////////////////
struct QueryContext {
  transaction::Methods* trx;
  aql::ExecutionPlan* plan;
  aql::Ast* ast;
  aql::ExpressionContext* ctx;
  aql::Variable const* ref;
};  // QueryContext

////////////////////////////////////////////////////////////////////////////////
/// @class ScopedAqlValue
/// @brief convenient wrapper around `AqlValue` and `AstNode`
////////////////////////////////////////////////////////////////////////////////
class ScopedAqlValue : private irs::util::noncopyable {
 public:
  static aql::AstNode const INVALID_NODE;

  explicit ScopedAqlValue(aql::AstNode const& node = INVALID_NODE) noexcept {
    reset(node);
  }

  ScopedAqlValue(ScopedAqlValue&& rhs) noexcept
      : _value(rhs._value), _node(rhs._node), _type(rhs._type) {
    rhs._node = &INVALID_NODE;
    rhs._type = SCOPED_VALUE_TYPE_INVALID;
    rhs._destroy = false;
    rhs._executed = true;
  }

  void reset(aql::AstNode const& node = INVALID_NODE) noexcept {
    _node = &node;
    _type = AqlValueTraits::type(node);
    _executed = node.isConstant();
  }

  ~ScopedAqlValue() noexcept { destroy(); }

  bool isConstant() const noexcept { return _node->isConstant(); }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief executes expression specified in the given `node`
  /// @returns true if expression has been executed, false otherwise
  ////////////////////////////////////////////////////////////////////////////////
  bool execute(QueryContext const& ctx);

  ScopedAqlValue at(size_t i) const {
    TRI_ASSERT(!_node->isConstant() || _node->getMemberUnchecked(i));

    return _node->isConstant() ? ScopedAqlValue(*_node->getMemberUnchecked(i))
                               : ScopedAqlValue(_value, i, false);
  }

  ScopedValueType type() const noexcept { return _type; }

  bool getBoolean() const {
    return _node->isConstant() ? _node->getBoolValue() : _value.toBoolean();
  }

  bool getDouble(double_t& value) const {
    bool failed = false;
    value = _node->isConstant() ? _node->getDoubleValue()
                                : _value.toDouble(nullptr, failed);
    return !failed;
  }

  int64_t getInt64() const {
    return _node->isConstant() ? _node->getIntValue() : _value.toInt64(nullptr);
  }

  bool getString(irs::string_ref& value) const {
    if (_node->isConstant()) {
      return parseValue(value, *_node);
    } else {
      auto const valueSlice = _value.slice();

      if (VPackValueType::String != valueSlice.type()) {
        return false;
      }

      value = getStringRef(valueSlice);
    }

    return true;
  }

  aql::Range const* getRange() const {
    return _node->isConstant() ? nullptr : _value.range();
  }

  size_t size() const {
    return _node->isConstant() ? _node->numMembers() : _value.length();
  }

  void toVelocyPack(velocypack::Builder& builder) const {
    _node->isConstant() ? _node->toVelocyPackValue(builder)
                        : _value.toVelocyPack(nullptr, builder, false);
  }

 private:
  ScopedAqlValue(aql::AqlValue const& src, size_t i, bool doCopy) {
    _value = src.at(nullptr, i, _destroy, doCopy);
    _node = &INVALID_NODE;
    _executed = true;
    _type = AqlValueTraits::type(_value);
  }

  FORCE_INLINE void destroy() noexcept {
    if (_destroy) {
      _value.destroy();
    }
  }

  aql::AqlValue _value;
  aql::AstNode const* _node;
  ScopedValueType _type;
  bool _destroy{false};
  bool _executed;
};  // ScopedAqlValue

////////////////////////////////////////////////////////////////////////////////
/// @brief interprets the specified node as an attribute path description and
///        visits the members in attribute path order calling the provided
///        'visitor' on each path sub-index, expecting the following signatures:
///          bool attributeAccess(AstNode const&) - attribute access
///          bool indexAccess(AstNode const&)     - index access
///          bool expansion(AstNode const&)       - expansion
/// @return success and set head the the starting node of path (reference)
////////////////////////////////////////////////////////////////////////////////
template <typename T>
bool visitAttributeAccess(aql::AstNode const*& head, aql::AstNode const* node, T& visitor) {
  if (!node) {
    return false;
  }

  switch (node->type) {
    case aql::NODE_TYPE_ATTRIBUTE_ACCESS:  // .
      return node->numMembers() >= 1 &&
             visitAttributeAccess(head, node->getMemberUnchecked(0), visitor) &&
             visitor.attributeAccess(*node);
    case aql::NODE_TYPE_INDEXED_ACCESS: {  // [<something>]
      if (node->numMembers() < 2) {
        // malformed node
        return false;
      }

      auto* offset = node->getMemberUnchecked(1);

      return offset && visitAttributeAccess(head, node->getMemberUnchecked(0), visitor) &&
             visitor.indexAccess(*offset);
    }
    case aql::NODE_TYPE_EXPANSION: {  // [*]
      if (node->numMembers() < 2) {
        // malformed node
        return false;
      }

      auto* itr = node->getMemberUnchecked(0);
      auto* ref = node->getMemberUnchecked(1);

      if (itr && itr->numMembers() >= 2) {
        auto* root = itr->getMemberUnchecked(1);
        auto* var = itr->getMemberUnchecked(0);

        return ref && aql::NODE_TYPE_ITERATOR == itr->type &&
               aql::NODE_TYPE_REFERENCE == ref->type && var &&
               aql::NODE_TYPE_VARIABLE == var->type &&
               visitAttributeAccess(head, root, visitor)  // 1st visit root
               && visitor.expansion(*node);  // 2nd visit current node
      }
    }
    case aql::NODE_TYPE_REFERENCE: {
      head = node;
      return true;
    }
    default:
      return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief interprets the specified node as an attribute path description and
///        visits the members in attribute path order calling the provided
///        'visitor' on each path sub-index, expecting the following signatures:
///          bool operator()(irs::string_ref) - string keys
///          bool operator()(int64_t)         - array offsets
///          bool operator()()                - any string key or numeric offset
/// @return success and set head the the starting node of path (reference/value)
////////////////////////////////////////////////////////////////////////////////
template <typename T>
bool visitAttributePath(aql::AstNode const*& head, aql::AstNode const& node, T& visitor) {
  if (node.numMembers() >= 2 && aql::NODE_TYPE_EXPANSION == node.type) {  // [*]
    auto* itr = node.getMemberUnchecked(0);
    auto* ref = node.getMemberUnchecked(1);

    if (itr && itr->numMembers() == 2) {
      auto* root = itr->getMemberUnchecked(1);
      auto* var = itr->getMemberUnchecked(0);

      return ref && aql::NODE_TYPE_ITERATOR == itr->type &&
             aql::NODE_TYPE_REFERENCE == ref->type && root && var &&
             aql::NODE_TYPE_VARIABLE == var->type &&
             visitAttributePath(head, *root, visitor)  // 1st visit root
             && visitor();                             // 2nd visit current node
    }
  } else if (node.numMembers() == 2 && aql::NODE_TYPE_INDEXED_ACCESS == node.type) {  // [<something>]
    auto* root = node.getMemberUnchecked(0);
    auto* offset = node.getMemberUnchecked(1);

    if (offset && offset->isIntValue()) {
      return root && offset->getIntValue() >= 0 &&
             visitAttributePath(head, *root, visitor)  // 1st visit root
             && visitor(offset->getIntValue());        // 2nd visit current node
    }

    return root && offset && offset->isStringValue() &&
           visitAttributePath(head, *root, visitor)       // 1st visit root
           && visitor(iresearch::getStringRef(*offset));  // 2nd visit current node
  } else if (node.numMembers() == 1 && aql::NODE_TYPE_ATTRIBUTE_ACCESS == node.type) {
    auto* root = node.getMemberUnchecked(0);

    return root && aql::VALUE_TYPE_STRING == node.value.type &&
           visitAttributePath(head, *root, visitor)    // 1st visit root
           && visitor(iresearch::getStringRef(node));  // 2nd visit current node
  } else if (!node.numMembers()) {  // end of attribute path (base case)
    head = &node;

    return aql::NODE_TYPE_REFERENCE == node.type ||
           (aql::NODE_TYPE_VALUE == node.type &&
            aql::VALUE_TYPE_STRING == node.value.type &&
            visitor(iresearch::getStringRef(node)));
  }

  return false;
}

struct NormalizedCmpNode {
  aql::AstNode const* attribute;
  aql::AstNode const* value;
  aql::AstNodeType cmp;
};

////////////////////////////////////////////////////////////////////////////////
/// @returns pointer to type name for the specified value if it's present in
///          TypeMap, nullptr otherwise
////////////////////////////////////////////////////////////////////////////////
inline std::string const* getNodeTypeName(aql::AstNodeType type) noexcept {
  auto const it = aql::AstNode::TypeNames.find(type);

  if (aql::AstNode::TypeNames.end() == it) {
    return nullptr;
  }

  return &it->second;
}

////////////////////////////////////////////////////////////////////////////////
/// @returns pointer to 'idx'th member of type 'expectedType', or nullptr
////////////////////////////////////////////////////////////////////////////////
inline aql::AstNode const* getNode(aql::AstNode const& node, size_t idx,
                                   aql::AstNodeType expectedType) {
  TRI_ASSERT(idx < node.numMembers());

  auto const* subNode = node.getMemberUnchecked(idx);
  TRI_ASSERT(subNode);

  return subNode->type != expectedType ? nullptr : subNode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether the specified node contains the specified variable
///        at any level of the hierarchy
/// @returns true if the specified node contains variable, false otherwise
////////////////////////////////////////////////////////////////////////////////
inline bool findReference(aql::AstNode const& root, aql::Variable const& ref) noexcept {
  auto visitor = [&ref](aql::AstNode const& node) noexcept {
    return aql::NODE_TYPE_REFERENCE != node.type ||
           reinterpret_cast<void const*>(&ref) != node.getData();
  };

  return !visit<true>(root, visitor);  // preorder walk
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes input binary comparison node (==, !=, <, <=, >, >=) and
///        fills the specified struct
/// @returns true if the specified 'in' nodes has been successfully normalized,
///          false otherwise
////////////////////////////////////////////////////////////////////////////////
bool normalizeCmpNode(aql::AstNode const& in, aql::Variable const& ref,
                      NormalizedCmpNode& out);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks 2 nodes of type NODE_TYPE_ATTRIBUTE_ACCESS for equality
/// @returns true if the specified nodes are equal, false otherwise
////////////////////////////////////////////////////////////////////////////////
bool attributeAccessEqual(aql::AstNode const* lhs, aql::AstNode const* rhs,
                          QueryContext const* ctx);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates field name from the specified 'node'
/// @returns true on success, false otherwise
////////////////////////////////////////////////////////////////////////////////
bool nameFromAttributeAccess(std::string& name, aql::AstNode const& node,
                             QueryContext const& ctx);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether the specified node is correct attribute access node,
///        treats node of type NODE_TYPE_REFERENCE as invalid
/// @returns the specified node on success, nullptr otherwise
////////////////////////////////////////////////////////////////////////////////
aql::AstNode const* checkAttributeAccess(aql::AstNode const* node,
                                         aql::Variable const& ref) noexcept;

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__AQL_HELPER_H

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
