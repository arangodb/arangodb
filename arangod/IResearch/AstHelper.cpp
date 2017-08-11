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

#include "AstHelper.h"

namespace {

arangodb::aql::AstNodeType const CmpMap[] {
  arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ, // NODE_TYPE_OPERATOR_BINARY_EQ: 3 == a <==> a == 3
  arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE, // NODE_TYPE_OPERATOR_BINARY_NE: 3 != a <==> a != 3
  arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT, // NODE_TYPE_OPERATOR_BINARY_LT: 3 < a  <==> a > 3
  arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE, // NODE_TYPE_OPERATOR_BINARY_LE: 3 <= a <==> a >= 3
  arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT, // NODE_TYPE_OPERATOR_BINARY_GT: 3 > a  <==> a < 3
  arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE  // NODE_TYPE_OPERATOR_BINARY_GE: 3 >= a <==> a <= 3
};


////////////////////////////////////////////////////////////////////////////////
/// @returns true if values from the specified range [Min;Max] are adjacent
////////////////////////////////////////////////////////////////////////////////
template<arangodb::aql::AstNodeType Max>
constexpr bool checkAdjacency() {
  return true;
}

template<
  arangodb::aql::AstNodeType Max,
  arangodb::aql::AstNodeType Min,
  arangodb::aql::AstNodeType... Types
> constexpr bool checkAdjacency() {
  return (Max > Min) && (1 == (Max - Min)) && checkAdjacency<Min, Types...>();
}

}

namespace arangodb {
namespace iresearch {

bool normalizeCmpNode(arangodb::aql::AstNode const& in, NormalizedCmpNode& out) {
  static_assert(checkAdjacency<
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE, arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT,
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE, arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT,
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE, arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ>(),
    "Values are not adjacent"
  );

  auto cmp = in.type;

  if (cmp < arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ
      || cmp > arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE
      || in.numMembers() != 2) {
    // wrong 'in' type
    return false;
  }

  auto const* attribute = in.getMemberUnchecked(0);
  TRI_ASSERT(attribute);
  auto const* value = in.getMemberUnchecked(1);
  TRI_ASSERT(value);

  if (!arangodb::iresearch::checkAttributeAccess(attribute)) {
    if (!arangodb::iresearch::checkAttributeAccess(value)) {
      // no attribute access node found
      return false;
    }

    std::swap(attribute, value);
    cmp = CmpMap[cmp - arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ];
  }

  if (value->type != arangodb::aql::NODE_TYPE_VALUE || !value->isConstant()) {
    // can't handle non-constant values
    return false;
  }

  out.attribute = attribute;
  out.value = value;
  out.cmp = cmp;

  return true;
}

bool attributeAccessEqual(
    arangodb::aql::AstNode const* lhs,
    arangodb::aql::AstNode const* rhs
) {
  TRI_ASSERT(lhs && rhs);

  auto comparer = [rhs](arangodb::aql::AstNode const& node) mutable {
    auto const type = node.type;

    if (type != rhs->type) {
      // type mismatch
      return false;
    }

    if (type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
      if (node.numMembers() != 1 || rhs->numMembers() != 1) {
        // wrong and different number of members
        return false;
      }

      irs::string_ref const lhsValue(node.getStringValue(), node.getStringLength());
      irs::string_ref const rhsValue(rhs->getStringValue(), rhs->getStringLength());

      if (lhsValue != rhsValue) {
        // values are not equal
        return false;
      }

      rhs = rhs->getMemberUnchecked(0);
      return true;
    } else if (type == arangodb::aql::NODE_TYPE_REFERENCE) {
      if (node.numMembers() != 0 || rhs->numMembers() != 0) {
        // wrong and different number of members
        return false;
      }

      // equality means refering to the same memory location
      return node.value.value._data == rhs->value.value._data;
    }

    return false;
  };

  return visit<true>(*lhs, comparer);
}

std::string nameFromAttributeAccess(arangodb::aql::AstNode const& node) {
  class nameBuilder {
   public:
    bool operator()(irs::string_ref const& value) {
      str_.append(value.c_str(), value.size());
      str_ += '.';
      return true;
    }

    bool operator()() const {
      return false; // do not support [*]
    }

    bool operator()(int64_t) const {
      return false; // do not support [i]
    }

    std::string&& str() noexcept {
      str_.pop_back(); // remove trailing '.'
      return std::move(str_);
    }

   private:
    std::string str_;
  } builder;

  TRI_ASSERT(checkAttributeAccess(&node));

  arangodb::aql::AstNode const* head = nullptr;
  visitAttributePath(head, node, builder);
  return builder.str();
}

arangodb::aql::AstNode const* checkAttributeAccess(
    arangodb::aql::AstNode const* node
) noexcept {
  struct {
    bool operator()(irs::string_ref const&) {
      return true;
    }

    bool operator()() const {
      return false; // do not support [*]
    }

    bool operator()(int64_t) const {
      return false; // do not support [i]
    }
  } checker;

  arangodb::aql::AstNode const* head = nullptr;

  return node
      && arangodb::aql::NODE_TYPE_REFERENCE != node->type // do not allow root node to be REFERENCE
      && visitAttributePath(head, *node, checker)
      && head && !head->isConstant()
    ? node : nullptr;
}

} // iresearch
} // arangodb
