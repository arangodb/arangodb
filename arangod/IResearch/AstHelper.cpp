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

#include "IResearchDocument.h"

#include <inttypes.h>

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
  struct NodeValue {
    enum class Type {
      INVALID = 0,
      EXPANSION, // [*]
      ACCESS,    // [<offset>] | [<string>] | .
      VALUE      // REFERENCE | VALUE
    };

    bool read(arangodb::aql::AstNode const* node) noexcept {
      this->strVal = irs::string_ref::nil;
      this->iVal= 0;
      this->type = Type::INVALID;
      this->root = nullptr;

      if (!node) {
        return false;
      }

      auto const n = node->numMembers();
      auto const type = node->type;

      if (n >= 2 && arangodb::aql::NODE_TYPE_EXPANSION == type) { // [*]
        auto* itr = node->getMemberUnchecked(0);
        auto* ref = node->getMemberUnchecked(1);

        if (itr && itr->numMembers() == 2) {
          auto* var = itr->getMemberUnchecked(0);
          auto* root = itr->getMemberUnchecked(1);

          if (ref
              && arangodb::aql::NODE_TYPE_ITERATOR == itr->type
              && arangodb::aql::NODE_TYPE_REFERENCE == ref->type
              && root && var
              && arangodb::aql::NODE_TYPE_VARIABLE == var->type) {
            this->type = Type::EXPANSION;
            this->root = root;
            return true;
          }
        }

      } else if (n == 2 && arangodb::aql::NODE_TYPE_INDEXED_ACCESS == type) { // [<something>]
        auto* root = node->getMemberUnchecked(0);
        auto* offset = node->getMemberUnchecked(1);

        if (root && offset) {
          if (offset->isIntValue()) {
            this->iVal = offset->getIntValue();
            this->type = Type::ACCESS;
            this->root = root;
            return true;
          } else if (offset->isStringValue()){
            this->strVal = getStringRef(*offset);
            this->type = Type::ACCESS;
            this->root = root;
            return true;
          }
        }

      } else if (n == 1 && arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS == type) {
        auto* root = node->getMemberUnchecked(0);

        if (root && arangodb::aql::VALUE_TYPE_STRING == node->value.type) {
          this->strVal = getStringRef(*node);
          this->type = Type::ACCESS;
          this->root = root;
          return true;
        }

      } else if (!n) { // end of attribute path (base case)

        if (arangodb::aql::NODE_TYPE_REFERENCE == type) {
          this->iVal = reinterpret_cast<int64_t>(node->value.value._data);
          this->type = Type::VALUE;
          this->root = node;
          return false; // end of path
        } else if (arangodb::aql::VALUE_TYPE_STRING == node->value.type) {
          this->strVal = getStringRef(*node);
          this->type = Type::VALUE;
          this->root = node;
          return false; // end of path
        }

      }

      return false; // invalid input
    }

    bool operator==(const NodeValue& rhs) const noexcept {
      return type == rhs.type
        && strVal == rhs.strVal
        && iVal == rhs.iVal;
    }

    bool operator!=(const NodeValue& rhs) const noexcept {
      return !(*this == rhs);
    }

    irs::string_ref strVal;
    int64_t iVal;
    Type type { Type::INVALID };
    arangodb::aql::AstNode const* root;
  } lhsValue, rhsValue;

  while (lhsValue.read(lhs) & rhsValue.read(rhs)) {
    if (lhsValue != rhsValue) {
      return false;
    }

    lhs = lhsValue.root;
    rhs = rhsValue.root;
  }

  return lhsValue.type != NodeValue::Type::INVALID
   && lhsValue.type != NodeValue::Type::INVALID
   && rhsValue == lhsValue;
}

std::string nameFromAttributeAccess(arangodb::aql::AstNode const& node) {
  class nameBuilder {
   public:
    bool operator()(irs::string_ref const& value) {
      if (!str_.empty()) {
        str_ += NESTING_LEVEL_DELIMITER;
      }
      str_.append(value.c_str(), value.size());
      return true;
    }

    bool operator()() const {
      return false; // do not support [*]
    }

    bool operator()(uint64_t i) {
      str_ += NESTING_LIST_OFFSET_PREFIX;
      auto const written = sprintf(buf_, "%" PRIu64, i);
      str_.append(buf_, written);
      str_ += NESTING_LIST_OFFSET_SUFFIX;
      return true;
    }

    std::string&& str() noexcept {
      return std::move(str_);
    }

   private:
    std::string str_;
    char buf_[21]; // enough to hold all numbers up to 64-bits
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
      return true;
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
