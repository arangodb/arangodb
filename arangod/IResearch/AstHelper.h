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
template<typename Char>
inline bool parseValue(
    irs::basic_string_ref<Char>& value,
    arangodb::aql::AstNode const& node
) {
  switch (node.value.type) {
    case arangodb::aql::VALUE_TYPE_NULL:
    case arangodb::aql::VALUE_TYPE_BOOL:
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE:
      return false;
    case arangodb::aql::VALUE_TYPE_STRING:
      value = irs::basic_string_ref<Char>(
        reinterpret_cast<Char const*>(node.getStringValue()),
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

