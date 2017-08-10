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

#include "Aql/AstNode.h"

#include "utils/string.hpp"

#ifndef ARANGOD_IRESEARCH__AST_HELPER_H
#define ARANGOD_IRESEARCH__AST_HELPER_H 1

namespace arangodb {
namespace iresearch {

//////////////////////////////////////////////////////////////////////////////
/// @brief extracts string_ref from an AstNode, note that provided 'node'
///        must be an arangodb::aql::VALUE_TYPE_STRING
/// @return extracted string_ref
//////////////////////////////////////////////////////////////////////////////
inline irs::string_ref getStringRef(arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::VALUE_TYPE_STRING == node.value.type);

  return irs::string_ref(node.getStringValue(), node.getStringLength());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to extract 'size_t' value from the specified AstNode 'node'
/// @returns true on success, false otherwise
////////////////////////////////////////////////////////////////////////////////
inline bool parseValue(size_t& value, arangodb::aql::AstNode const& node) {
  switch (node.value.type) {
    case arangodb::aql::VALUE_TYPE_NULL:
    case arangodb::aql::VALUE_TYPE_BOOL:
      return false;
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE:
      value = node.getIntValue();
      return true;
    case arangodb::aql::VALUE_TYPE_STRING:
      return false;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to extract 'irs::basic_string_ref<Char>' value from the
//         specified AstNode 'node'
/// @returns true on success, false otherwise
////////////////////////////////////////////////////////////////////////////////
template<typename String>
inline bool parseValue(
    String& value,
    arangodb::aql::AstNode const& node
) {
  typedef typename String::traits_type traits_t;

  switch (node.value.type) {
    case arangodb::aql::VALUE_TYPE_NULL:
    case arangodb::aql::VALUE_TYPE_BOOL:
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE:
      return false;
    case arangodb::aql::VALUE_TYPE_STRING:
      value = String(
        reinterpret_cast<typename traits_t::char_type const*>(node.getStringValue()),
        node.getStringLength()
      );
      return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visits the specified node using the provided 'visitor' according
///        to the specified visiting strategy (preorder/postorder)
////////////////////////////////////////////////////////////////////////////////
template<bool Preorder, typename Visitor>
bool visit(arangodb::aql::AstNode const& root, Visitor visitor) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief interprets the specified node as an attribute path description and
///        visits the members in attribute path order calling the provided
///        'visitor' on each path sub-index, expecting the following signatures:
///          bool operator()(irs::string_ref) - string keys
///          bool operator()(int64_t)         - array offsets
///          bool operator()()                - any string key or numeric offset
/// @return success and set head the the starting node of path (reference/value)
////////////////////////////////////////////////////////////////////////////////
template<typename T>
bool visitAttributePath(
  arangodb::aql::AstNode const*& head,
  arangodb::aql::AstNode const& node,
  T& visitor
) {
  if (node.numMembers() >= 2
      && arangodb::aql::NODE_TYPE_EXPANSION == node.type) { // [*]
    auto* itr = node.getMemberUnchecked(0);
    auto* ref = node.getMemberUnchecked(1);

    if (itr && itr->numMembers() == 2) {
      auto* root = itr->getMemberUnchecked(1);
      auto* var = itr->getMemberUnchecked(0);

      return ref
        && arangodb::aql::NODE_TYPE_ITERATOR == itr->type
        && arangodb::aql::NODE_TYPE_REFERENCE == ref->type
        && root && var
        && arangodb::aql::NODE_TYPE_VARIABLE == var->type
        && visitAttributePath(head, *root, visitor) // 1st visit root
        && visitor(); // 2nd visit current node
    }
  } else if (node.numMembers() == 2
             && arangodb::aql::NODE_TYPE_INDEXED_ACCESS == node.type) { // [<something>]
    auto* root = node.getMemberUnchecked(0);
    auto* offset = node.getMemberUnchecked(1);

    if (offset && offset->isIntValue()) {
      return root
        && offset->getIntValue() >= 0
        && visitAttributePath(head, *root, visitor) // 1st visit root
        && visitor(offset->getIntValue()); // 2nd visit current node
    }

    return root && offset && offset->isStringValue()
      && visitAttributePath(head, *root, visitor) // 1st visit root
      && visitor(arangodb::iresearch::getStringRef(*offset)); // 2nd visit current node
  } else if (node.numMembers() == 1
             && arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS == node.type) {
    auto* root = node.getMemberUnchecked(0);

    return root
      && arangodb::aql::VALUE_TYPE_STRING == node.value.type
      && visitAttributePath(head, *root, visitor) // 1st visit root
      && visitor(arangodb::iresearch::getStringRef(node)); // 2nd visit current node
  } else if (!node.numMembers()) { // end of attribute path (base case)
    head = &node;

    return arangodb::aql::NODE_TYPE_REFERENCE == node.type
      || (arangodb::aql::NODE_TYPE_VALUE == node.type
          && visitor(arangodb::iresearch::getStringRef(node))
         );
  }

  return false;
}

struct NormalizedCmpNode {
  arangodb::aql::AstNode const* attribute;
  arangodb::aql::AstNode const* value;
  arangodb::aql::AstNodeType cmp;
};

////////////////////////////////////////////////////////////////////////////////
/// @returns pointer to type name for the specified value if it's present in 
///          TypeMap, nullptr otherwise
////////////////////////////////////////////////////////////////////////////////
inline std::string const* getNodeTypeName(
    arangodb::aql::AstNodeType type
) noexcept {
  auto const it = arangodb::aql::AstNode::TypeNames.find(type);

  if (arangodb::aql::AstNode::TypeNames.end() == it) {
    return nullptr;
  }

  return &it->second;
}

////////////////////////////////////////////////////////////////////////////////
/// @returns pointer to 'idx'th member of type 'expectedType', or nullptr
////////////////////////////////////////////////////////////////////////////////
inline arangodb::aql::AstNode const* getNode(
    arangodb::aql::AstNode const& node,
    size_t idx,
    arangodb::aql::AstNodeType expectedType
) {
  TRI_ASSERT(idx < node.numMembers());

  auto const* subNode = node.getMemberUnchecked(idx);
  TRI_ASSERT(subNode);

  return subNode->type != expectedType ? nullptr : subNode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes input binary comparison node (==, !=, <, <=, >, >=) and
///        fills the specified struct
/// @returns true if the specified 'in' nodes has been successfully normalized,
///          false otherwise
////////////////////////////////////////////////////////////////////////////////
bool normalizeCmpNode(arangodb::aql::AstNode const& in, NormalizedCmpNode& out);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks 2 nodes of type NODE_TYPE_ATTRIBUTE_ACCESS for equality
/// @returns true if the specified nodes are equal, false otherwise
////////////////////////////////////////////////////////////////////////////////
bool attributeAccessEqual(
  arangodb::aql::AstNode const* lhs,
  arangodb::aql::AstNode const* rhs
);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates field name from the specified 'node' and value 'type'
/// @returns generated name string
////////////////////////////////////////////////////////////////////////////////
std::string nameFromAttributeAccess(arangodb::aql::AstNode const& node);

} // iresearch
} // arangodb

#endif // ARANGOD_IRESEARCH__AST_HELPER_H

