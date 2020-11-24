////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <string>
#include "AqlHelper.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Variable.h"
#include "Basics/fasthash.h"
#include "IResearchCommon.h"
#include "IResearchDocument.h"
#include "Logger/LogMacros.h"
#include "Misc.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace {

arangodb::aql::AstNodeType const CmpMap[]{
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ,  // NODE_TYPE_OPERATOR_BINARY_EQ: 3 == a <==> a == 3
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE,  // NODE_TYPE_OPERATOR_BINARY_NE: 3 != a <==> a != 3
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT,  // NODE_TYPE_OPERATOR_BINARY_LT: 3 < a  <==> a > 3
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE,  // NODE_TYPE_OPERATOR_BINARY_LE: 3 <= a <==> a >= 3
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT,  // NODE_TYPE_OPERATOR_BINARY_GT: 3 > a  <==> a < 3
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE  // NODE_TYPE_OPERATOR_BINARY_GE: 3 >= a <==> a <= 3
};

}

namespace arangodb {
namespace iresearch {

bool equalTo(aql::AstNode const* lhs, aql::AstNode const* rhs) {
  if (lhs == rhs) {
    return true;
  }

  if ((lhs == nullptr && rhs != nullptr) || (lhs != nullptr && rhs == nullptr)) {
    return false;
  }

  // cppcheck-suppress nullPointerRedundantCheck
  if (lhs->type != rhs->type) {
    return false;
  }

  size_t const n = lhs->numMembers();

  if (n != rhs->numMembers()) {
    return false;
  }

  // check members for equality
  for (size_t i = 0; i < n; ++i) {
    if (!equalTo(lhs->getMemberUnchecked(i), rhs->getMemberUnchecked(i))) {
      return false;
    }
  }

  switch (lhs->type) {
    case aql::NODE_TYPE_VARIABLE: {
      return lhs->getData() == rhs->getData();
    }

    case aql::NODE_TYPE_OPERATOR_UNARY_PLUS:
    case aql::NODE_TYPE_OPERATOR_UNARY_MINUS:
    case aql::NODE_TYPE_OPERATOR_UNARY_NOT:
    case aql::NODE_TYPE_OPERATOR_BINARY_AND:
    case aql::NODE_TYPE_OPERATOR_BINARY_OR:
    case aql::NODE_TYPE_OPERATOR_BINARY_PLUS:
    case aql::NODE_TYPE_OPERATOR_BINARY_MINUS:
    case aql::NODE_TYPE_OPERATOR_BINARY_TIMES:
    case aql::NODE_TYPE_OPERATOR_BINARY_DIV:
    case aql::NODE_TYPE_OPERATOR_BINARY_MOD:
    case aql::NODE_TYPE_OPERATOR_BINARY_EQ:
    case aql::NODE_TYPE_OPERATOR_BINARY_NE:
    case aql::NODE_TYPE_OPERATOR_BINARY_LT:
    case aql::NODE_TYPE_OPERATOR_BINARY_LE:
    case aql::NODE_TYPE_OPERATOR_BINARY_GT:
    case aql::NODE_TYPE_OPERATOR_BINARY_GE:
    case aql::NODE_TYPE_OPERATOR_BINARY_IN:
    case aql::NODE_TYPE_OPERATOR_BINARY_NIN:
    case aql::NODE_TYPE_OPERATOR_TERNARY:
    case aql::NODE_TYPE_OBJECT:
    case aql::NODE_TYPE_CALCULATED_OBJECT_ELEMENT:
    case aql::NODE_TYPE_ARRAY:
    case aql::NODE_TYPE_RANGE:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN: {
      return true;
    }

    case aql::NODE_TYPE_ATTRIBUTE_ACCESS:
    case aql::NODE_TYPE_INDEXED_ACCESS:
    case aql::NODE_TYPE_EXPANSION: {
      return attributeAccessEqual(lhs, rhs, nullptr);
    }

    case aql::NODE_TYPE_VALUE: {
      return 0 == aql::CompareAstNodes(lhs, rhs, true);
    }

    case aql::NODE_TYPE_OBJECT_ELEMENT: {
      irs::string_ref lhsValue, rhsValue;
      iresearch::parseValue(lhsValue, *lhs);
      iresearch::parseValue(rhsValue, *rhs);

      return lhsValue == rhsValue;
    }

    case aql::NODE_TYPE_REFERENCE: {
      return lhs->getData() == rhs->getData();
    }

    case aql::NODE_TYPE_FCALL: {
      return lhs->getData() == rhs->getData();
    }

    case aql::NODE_TYPE_FCALL_USER: {
      irs::string_ref lhsName, rhsName;
      iresearch::parseValue(lhsName, *lhs);
      iresearch::parseValue(rhsName, *rhs);

      return lhsName == rhsName;
    }

    case aql::NODE_TYPE_QUANTIFIER: {
      return lhs->value.value._int == rhs->value.value._int;
    }

    default: { return false; }
  }
}

size_t hash(aql::AstNode const* node, size_t hash /*= 0*/) noexcept {
  if (!node) {
    return hash;
  }

  // hash node type
  auto const& typeString = node->getTypeString();

  hash = fasthash64(static_cast<const void*>(typeString.c_str()), typeString.size(), hash);

  // hash node members
  for (size_t i = 0, n = node->numMembers(); i < n; ++i) {
    auto sub = node->getMemberUnchecked(i);

    if (sub) {
      hash = iresearch::hash(sub, hash);
    }
  }

  switch (node->type) {
    case aql::NODE_TYPE_VARIABLE: {
      return fasthash64(node->getData(), sizeof(void*), hash);
    }

    case aql::NODE_TYPE_OPERATOR_UNARY_PLUS:
    case aql::NODE_TYPE_OPERATOR_UNARY_MINUS:
    case aql::NODE_TYPE_OPERATOR_UNARY_NOT:
    case aql::NODE_TYPE_OPERATOR_BINARY_AND:
    case aql::NODE_TYPE_OPERATOR_BINARY_OR:
    case aql::NODE_TYPE_OPERATOR_BINARY_PLUS:
    case aql::NODE_TYPE_OPERATOR_BINARY_MINUS:
    case aql::NODE_TYPE_OPERATOR_BINARY_TIMES:
    case aql::NODE_TYPE_OPERATOR_BINARY_DIV:
    case aql::NODE_TYPE_OPERATOR_BINARY_MOD:
    case aql::NODE_TYPE_OPERATOR_BINARY_EQ:
    case aql::NODE_TYPE_OPERATOR_BINARY_NE:
    case aql::NODE_TYPE_OPERATOR_BINARY_LT:
    case aql::NODE_TYPE_OPERATOR_BINARY_LE:
    case aql::NODE_TYPE_OPERATOR_BINARY_GT:
    case aql::NODE_TYPE_OPERATOR_BINARY_GE:
    case aql::NODE_TYPE_OPERATOR_BINARY_IN:
    case aql::NODE_TYPE_OPERATOR_BINARY_NIN:
    case aql::NODE_TYPE_OPERATOR_TERNARY:
    case aql::NODE_TYPE_INDEXED_ACCESS:
    case aql::NODE_TYPE_EXPANSION:
    case aql::NODE_TYPE_ARRAY:
    case aql::NODE_TYPE_OBJECT:
    case aql::NODE_TYPE_CALCULATED_OBJECT_ELEMENT:
    case aql::NODE_TYPE_RANGE:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN: {
      return hash;
    }

    case aql::NODE_TYPE_ATTRIBUTE_ACCESS: {
      return aql::AstNode(node->value).hashValue(hash);
    }

    case aql::NODE_TYPE_VALUE: {
      switch (node->value.type) {
        case aql::VALUE_TYPE_NULL:
          return fasthash64(static_cast<const void*>("null"), 4, hash);
        case aql::VALUE_TYPE_BOOL:
          if (node->value.value._bool) {
            return fasthash64(static_cast<const void*>("true"), 4, hash);
          }
          return fasthash64(static_cast<const void*>("false"), 5, hash);
        case aql::VALUE_TYPE_INT:
          return fasthash64(static_cast<const void*>(&node->value.value._int),
                            sizeof(node->value.value._int), hash);
        case aql::VALUE_TYPE_DOUBLE:
          return fasthash64(static_cast<const void*>(&node->value.value._double),
                            sizeof(node->value.value._double), hash);
        case aql::VALUE_TYPE_STRING:
          return fasthash64(static_cast<const void*>(node->getStringValue()),
                            node->getStringLength(), hash);
      }
      return hash;
    }

    case aql::NODE_TYPE_OBJECT_ELEMENT: {
      return fasthash64(static_cast<const void*>(node->getStringValue()),
                        node->getStringLength(), hash);
    }

    case aql::NODE_TYPE_REFERENCE: {
      return fasthash64(node->getData(), sizeof(void*), hash);
    }

    case aql::NODE_TYPE_FCALL: {
      auto* fn = static_cast<aql::Function*>(node->getData());

      hash = fasthash64(node->getData(), sizeof(void*), hash);
      return fasthash64(fn->name.c_str(), fn->name.size(), hash);
    }

    case aql::NODE_TYPE_FCALL_USER: {
      return fasthash64(static_cast<const void*>(node->getStringValue()),
                        node->getStringLength(), hash);
    }

    case aql::NODE_TYPE_QUANTIFIER: {
      return fasthash64(static_cast<const void*>(&node->value.value._int),
                        sizeof(node->value.value._int), hash);
    }

    default: {
      return fasthash64(static_cast<void const*>(&node), sizeof(&node), hash);
    }
  }
}

irs::string_ref getFuncName(aql::AstNode const& node) {
  irs::string_ref fname;

  switch (node.type) {
    case aql::NODE_TYPE_FCALL:
      fname = reinterpret_cast<aql::Function const*>(node.getData())->name;
      break;

    case aql::NODE_TYPE_FCALL_USER:
      parseValue(fname, node);
      break;

    default:
      TRI_ASSERT(false);
  }

  return fname;
}

void visitReferencedVariables(aql::AstNode const& root,
                              std::function<void(aql::Variable const&)> const& visitor) {
  auto preVisitor = [](aql::AstNode const* node) -> bool {
    return !node->isConstant();
  };

  auto postVisitor = [&visitor](aql::AstNode const* node) {
    if (node == nullptr) {
      return;
    }

    // reference to a variable
    if (node->type == aql::NODE_TYPE_REFERENCE) {
      auto variable = static_cast<aql::Variable const*>(node->getData());

      if (!variable) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "invalid reference in AST");
      }

      if (variable->needsRegister()) {
        visitor(*variable);
      }
    }
  };

  aql::Ast::traverseReadOnly(&root, preVisitor, postVisitor);
}

// ----------------------------------------------------------------------------
// --SECTION--                                    AqlValueTraits implementation
// ----------------------------------------------------------------------------

aql::AstNode const ScopedAqlValue::INVALID_NODE(aql::NODE_TYPE_ROOT);

/*static*/ irs::string_ref const& ScopedAqlValue::typeString(ScopedValueType type) noexcept {
  static irs::string_ref const TYPE_NAMES[] = {
    "invalid", "null", "boolean", "double", "string", "array", "range", "object"
  };

  TRI_ASSERT(size_t(type) < IRESEARCH_COUNTOF(TYPE_NAMES));

  return TYPE_NAMES[size_t(type)];
}

// ----------------------------------------------------------------------------
// --SECTION--                                    ScopedAqlValue implementation
// ----------------------------------------------------------------------------

bool ScopedAqlValue::execute(iresearch::QueryContext const& ctx) {
  if (_executed && _node->isDeterministic()) {
    // constant expression, nothing to do
    return true;
  }

  if (!ctx.plan) {  // || !ctx.ctx) {
    // can't execute expression without `ExecutionPlan`
    return false;
  }

  TRI_ASSERT(ctx.ctx);  // FIXME remove, uncomment condition

  if (!ctx.ast) {
    // can't execute expression without `AST` and `ExpressionContext`
    return false;
  }

  aql::Expression expr(ctx.ast, const_cast<aql::AstNode*>(_node));

  destroy();

  try {
    _value = expr.execute(ctx.ctx, _destroy);
  } catch (basics::Exception const& e) {
    // can't execute expression
    LOG_TOPIC("0c06a", WARN, iresearch::TOPIC) << e.message();
    return false;
  } catch (...) {
    // can't execute expression
    return false;
  }

  _type = AqlValueTraits::type(_value);
  _executed = true;
  return true;
}

bool normalizeGeoDistanceCmpNode(
    aql::AstNode const& in,
    aql::Variable const& ref,
    NormalizedCmpNode& out) {
  static_assert(adjacencyChecker<aql::AstNodeType>::checkAdjacency<
                aql::NODE_TYPE_OPERATOR_BINARY_GE, aql::NODE_TYPE_OPERATOR_BINARY_GT,
                aql::NODE_TYPE_OPERATOR_BINARY_LE, aql::NODE_TYPE_OPERATOR_BINARY_LT,
                aql::NODE_TYPE_OPERATOR_BINARY_NE, aql::NODE_TYPE_OPERATOR_BINARY_EQ>(),
                "Values are not adjacent");

  auto checkFCallGeoDistance = [](aql::AstNode const* node, aql::Variable const& ref) {
    if (!node || aql::NODE_TYPE_FCALL != node->type) {
      return false;
    }

    auto* impl = reinterpret_cast<aql::Function const*>(node->getData())->implementation;

   if (impl != &aql::Functions::GeoDistance) {
      return false;
    }

    auto* args = node->getMemberUnchecked(0);
    return args && findReference(*args, ref);
  };

  if (!in.isDeterministic()) {
    // unable normalize nondeterministic node
    return false;
  }

  auto cmp = in.type;

  if (cmp < aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
      cmp > aql::NODE_TYPE_OPERATOR_BINARY_GE ||
      in.numMembers() != 2) {
    // wrong `in` type
    return false;
  }

  auto const* fcall = in.getMemberUnchecked(0);
  TRI_ASSERT(fcall);
  auto const* value = in.getMemberUnchecked(1);
  TRI_ASSERT(value);

  if (!checkFCallGeoDistance(fcall, ref)) {
    if (!checkFCallGeoDistance(value, ref)) {
      return false;
    }

    std::swap(fcall, value);
    cmp = CmpMap[cmp - aql::NODE_TYPE_OPERATOR_BINARY_EQ];
  }

  if (iresearch::findReference(*value, ref)) {
    // value contains referenced variable
    return false;
  }

  out.attribute = fcall;
  out.value = value;
  out.cmp = cmp;

  return true;
}

bool normalizeCmpNode(aql::AstNode const& in,
                      aql::Variable const& ref, NormalizedCmpNode& out) {
  static_assert(adjacencyChecker<aql::AstNodeType>::checkAdjacency<
                aql::NODE_TYPE_OPERATOR_BINARY_GE, aql::NODE_TYPE_OPERATOR_BINARY_GT,
                aql::NODE_TYPE_OPERATOR_BINARY_LE, aql::NODE_TYPE_OPERATOR_BINARY_LT,
                aql::NODE_TYPE_OPERATOR_BINARY_NE, aql::NODE_TYPE_OPERATOR_BINARY_EQ>(),
                "Values are not adjacent");

  if (!in.isDeterministic()) {
    // unable normalize nondeterministic node
    return false;
  }

  auto cmp = in.type;

  if (cmp < aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
      cmp > aql::NODE_TYPE_OPERATOR_BINARY_GE || in.numMembers() != 2) {
    // wrong `in` type
    return false;
  }

  auto const* attribute = in.getMemberUnchecked(0);
  TRI_ASSERT(attribute);
  auto const* value = in.getMemberUnchecked(1);
  TRI_ASSERT(value);

  if (!iresearch::checkAttributeAccess(attribute, ref)) {
    if (!iresearch::checkAttributeAccess(value, ref)) {
      // no suitable attribute access node found
      return false;
    }

    std::swap(attribute, value);
    cmp = CmpMap[cmp - aql::NODE_TYPE_OPERATOR_BINARY_EQ];
  }

  if (iresearch::findReference(*value, ref)) {
    // value contains referenced variable
    return false;
  }

  out.attribute = attribute;
  out.value = value;
  out.cmp = cmp;

  return true;
}

bool attributeAccessEqual(aql::AstNode const* lhs,
                          aql::AstNode const* rhs, QueryContext const* ctx) {
  struct NodeValue {
    enum class Type {
      INVALID = 0,
      EXPANSION,  // [*]
      ACCESS,     // [<offset>] | [<string>] | .
      VALUE       // REFERENCE | VALUE
    };

    bool read(aql::AstNode const* node, QueryContext const* ctx) noexcept {
      this->strVal = irs::string_ref::NIL;
      this->iVal = 0;
      this->type = Type::INVALID;
      this->root = nullptr;

      if (!node) {
        return false;
      }

      auto const n = node->numMembers();
      auto const type = node->type;

      if (n >= 2 && aql::NODE_TYPE_EXPANSION == type) {  // [*]
        auto* itr = node->getMemberUnchecked(0);
        auto* ref = node->getMemberUnchecked(1);

        if (itr && itr->numMembers() == 2) {
          auto* var = itr->getMemberUnchecked(0);
          auto* root = itr->getMemberUnchecked(1);

          if (ref && aql::NODE_TYPE_ITERATOR == itr->type &&
              aql::NODE_TYPE_REFERENCE == ref->type && root && var &&
              aql::NODE_TYPE_VARIABLE == var->type) {
            this->type = Type::EXPANSION;
            this->root = root;
            return true;
          }
        }

      } else if (n == 2 && aql::NODE_TYPE_INDEXED_ACCESS == type) {  // [<something>]
        auto* root = node->getMemberUnchecked(0);
        auto* offset = node->getMemberUnchecked(1);

        if (root && offset) {
          aqlValue.reset(*offset);

          if (!aqlValue.isConstant()) {
            if (!ctx) {
              // can't evaluate expression at compile time
              return true;
            }

            if (!aqlValue.execute(*ctx)) {
              // failed to execute expression
              return false;
            }
          }

          switch (aqlValue.type()) {
            case iresearch::SCOPED_VALUE_TYPE_DOUBLE:
              this->iVal = aqlValue.getInt64();
              this->type = Type::ACCESS;
              this->root = root;
              return true;
            case iresearch::SCOPED_VALUE_TYPE_STRING:
              if (!aqlValue.getString(this->strVal)) {
                // failed to parse value as string
                return false;
              }
              this->type = Type::ACCESS;
              this->root = root;
              return true;
            default:
              break;
          }
        }

      } else if (n == 1 && aql::NODE_TYPE_ATTRIBUTE_ACCESS == type) {
        auto* root = node->getMemberUnchecked(0);

        if (root && aql::VALUE_TYPE_STRING == node->value.type) {
          this->strVal = getStringRef(*node);
          this->type = Type::ACCESS;
          this->root = root;
          return true;
        }

      } else if (!n) {  // end of attribute path (base case)
        if (aql::NODE_TYPE_REFERENCE == type) {
          this->iVal = reinterpret_cast<int64_t>(node->value.value._data);
          this->type = Type::VALUE;
          this->root = node;
          return false;  // end of path
        } else if (aql::VALUE_TYPE_STRING == node->value.type) {
          this->strVal = getStringRef(*node);
          this->type = Type::VALUE;
          this->root = node;
          return false;  // end of path
        }
      }

      return false;  // invalid input
    }

    bool operator==(const NodeValue& rhs) const noexcept {
      return type == rhs.type && strVal == rhs.strVal && iVal == rhs.iVal;
    }

    bool operator!=(const NodeValue& rhs) const noexcept {
      return !(*this == rhs);
    }

    iresearch::ScopedAqlValue aqlValue;
    irs::string_ref strVal;
    int64_t iVal;
    Type type{Type::INVALID};
    aql::AstNode const* root = nullptr;
  } lhsValue, rhsValue;

  // TODO: is the "&" intionally. If yes: why?
  //cppcheck-suppress uninitvar; false positive
  while (lhsValue.read(lhs, ctx) & rhsValue.read(rhs, ctx)) {
    if (lhsValue != rhsValue) {
      return false;
    }

    lhs = lhsValue.root;
    rhs = rhsValue.root;
  }

  return lhsValue.type != NodeValue::Type::INVALID &&
         rhsValue.type != NodeValue::Type::INVALID && rhsValue == lhsValue;
}

bool nameFromAttributeAccess(std::string& name, aql::AstNode const& node,
                             QueryContext const& ctx) {
  struct {
    bool attributeAccess(aql::AstNode const& node) {
      irs::string_ref strValue;

      if (!parseValue(strValue, node)) {
        // wrong type
        return false;
      }

      append(strValue);
      return true;
    }

    bool expansion(aql::AstNode const&) const {
      return false;  // do not support [*]
    }

    bool indexAccess(aql::AstNode const& node) {
      value_.reset(node);

      if (!value_.execute(*ctx_)) {
        // failed to evaluate value
        return false;
      }

      switch (value_.type()) {
        case iresearch::SCOPED_VALUE_TYPE_DOUBLE:
          append(value_.getInt64());
          return true;
        case iresearch::SCOPED_VALUE_TYPE_STRING: {
          irs::string_ref strValue;

          if (!value_.getString(strValue)) {
            // unable to parse value as string
            return false;
          }

          append(strValue);
          return true;
        }
        default:
          return false;
      }
    }

    void append(irs::string_ref const& value) {
      if (!str_->empty()) {
        (*str_) += NESTING_LEVEL_DELIMITER;
      }
      str_->append(value.c_str(), value.size());
    }

    void append(int64_t value) {
      (*str_) += NESTING_LIST_OFFSET_PREFIX;
      auto const written = sprintf(buf_, "%" PRIu64, value);
      str_->append(buf_, written);
      (*str_) += NESTING_LIST_OFFSET_SUFFIX;
    }

    ScopedAqlValue value_;
    std::string* str_;
    QueryContext const* ctx_;
    char buf_[21];  // enough to hold all numbers up to 64-bits
  } builder;

  name.clear();
  builder.str_ = &name;
  builder.ctx_ = &ctx;

  aql::AstNode const* head = nullptr;
  return visitAttributeAccess(head, &node, builder) && head &&
         aql::NODE_TYPE_REFERENCE == head->type;
}

aql::AstNode const* checkAttributeAccess(aql::AstNode const* node,
                                         aql::Variable const& ref) noexcept {
  struct {
    bool attributeAccess(aql::AstNode const&) const { return true; }

    bool indexAccess(aql::AstNode const&) const { return true; }

    bool expansion(aql::AstNode const&) const {
      return false;  // do not support [*]
    }
  } checker;

  aql::AstNode const* head = nullptr;

  return node && aql::NODE_TYPE_REFERENCE != node->type  // do not allow root node to be REFERENCE
                 && visitAttributeAccess(head, node, checker) && head &&
                 aql::NODE_TYPE_REFERENCE == head->type &&
                 reinterpret_cast<void const*>(&ref) == head->getData()  // same variable
             ? node
             : nullptr;
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
