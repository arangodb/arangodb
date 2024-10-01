////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFilterContext.h"
#include "IResearch/IResearchInvertedIndexMeta.h"
#include "Logger/LogMacros.h"
#include "Misc.h"

#include <absl/strings/str_cat.h>

namespace arangodb {
namespace iresearch {
namespace {

aql::AstNodeType constexpr kCmpMap[]{
    aql::NODE_TYPE_OPERATOR_BINARY_EQ,  // NODE_TYPE_OPERATOR_BINARY_EQ:
                                        // 3 == a <==> a == 3
    aql::NODE_TYPE_OPERATOR_BINARY_NE,  // NODE_TYPE_OPERATOR_BINARY_NE:
                                        // 3 != a <==> a != 3
    aql::NODE_TYPE_OPERATOR_BINARY_GT,  // NODE_TYPE_OPERATOR_BINARY_LT:
                                        // 3 < a  <==> a > 3
    aql::NODE_TYPE_OPERATOR_BINARY_GE,  // NODE_TYPE_OPERATOR_BINARY_LE:
                                        // 3 <= a <==> a >= 3
    aql::NODE_TYPE_OPERATOR_BINARY_LT,  // NODE_TYPE_OPERATOR_BINARY_GT:
                                        // 3 > a  <==> a < 3
    aql::NODE_TYPE_OPERATOR_BINARY_LE   // NODE_TYPE_OPERATOR_BINARY_GE:
                                        // 3 >= a <==> a <= 3
};

auto getNested(
    std::string_view parent,
    std::span<const arangodb::iresearch::InvertedIndexField> fields) noexcept {
  if (parent.ends_with("[*]")) {
    parent = parent.substr(0, parent.size() - 3);
  }
  return std::find_if(
      std::begin(fields), std::end(fields),
      [parent](const auto& field) noexcept { return field.path() == parent; });
}

}  // namespace

bool equalTo(aql::AstNode const* lhs, aql::AstNode const* rhs) {
  if (lhs == rhs) {
    return true;
  }

  if ((lhs == nullptr && rhs != nullptr) ||
      (lhs != nullptr && rhs == nullptr)) {
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
      return 0 == aql::compareAstNodes(lhs, rhs, true);
    }

    case aql::NODE_TYPE_OBJECT_ELEMENT: {
      std::string_view lhsValue, rhsValue;
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
      std::string_view lhsName, rhsName;
      iresearch::parseValue(lhsName, *lhs);
      iresearch::parseValue(rhsName, *rhs);

      return lhsName == rhsName;
    }

    case aql::NODE_TYPE_QUANTIFIER: {
      return lhs->value.value._int == rhs->value.value._int;
    }

    default: {
      return false;
    }
  }
}

size_t hash(aql::AstNode const* node, size_t hash /*= 0*/) noexcept {
  if (!node) {
    return hash;
  }

  // hash node type
  auto const& typeString = node->getTypeString();

  hash = fasthash64(static_cast<const void*>(typeString.data()),
                    typeString.size(), hash);

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
          return fasthash64(
              static_cast<const void*>(&node->value.value._double),
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

std::string_view getFuncName(aql::AstNode const& node) {
  std::string_view fname;

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

void visitReferencedVariables(
    aql::AstNode const& root,
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

// create the node as an InternalNode. this will already properly initialized
// the relevant node flags, so there are no races when multiple threads try to
// read and mutate the node's flags concurrently.
aql::AstNode const ScopedAqlValue::INVALID_NODE(aql::NODE_TYPE_ROOT,
                                                aql::AstNode::InternalNode{});

bool ScopedAqlValue::execute(iresearch::QueryContext const& ctx) {
  if (_executed && _node->isDeterministic()) {
    // constant expression, nothing to do
    return true;
  }

  TRI_ASSERT(ctx.ctx);

  if (!ctx.ast || !ctx.ctx) {
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

bool normalizeGeoDistanceCmpNode(aql::AstNode const& in,
                                 aql::Variable const& ref,
                                 NormalizedCmpNode& out) {
  static_assert(
      adjacencyChecker<aql::AstNodeType>::checkAdjacency<
          aql::NODE_TYPE_OPERATOR_BINARY_GE, aql::NODE_TYPE_OPERATOR_BINARY_GT,
          aql::NODE_TYPE_OPERATOR_BINARY_LE, aql::NODE_TYPE_OPERATOR_BINARY_LT,
          aql::NODE_TYPE_OPERATOR_BINARY_NE,
          aql::NODE_TYPE_OPERATOR_BINARY_EQ>(),
      "Values are not adjacent");

  auto checkFCallGeoDistance = [](aql::AstNode const* node,
                                  aql::Variable const& ref) {
    if (!node || aql::NODE_TYPE_FCALL != node->type) {
      return false;
    }

    auto* impl =
        reinterpret_cast<aql::Function const*>(node->getData())->implementation;

    if (impl != &aql::functions::GeoDistance) {
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
      cmp > aql::NODE_TYPE_OPERATOR_BINARY_GE || in.numMembers() != 2) {
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
    cmp = kCmpMap[cmp - aql::NODE_TYPE_OPERATOR_BINARY_EQ];
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

bool normalizeCmpNode(aql::AstNode const& in, aql::Variable const& ref,
                      bool allowExpansion, NormalizedCmpNode& out) {
  static_assert(
      adjacencyChecker<aql::AstNodeType>::checkAdjacency<
          aql::NODE_TYPE_OPERATOR_BINARY_GE, aql::NODE_TYPE_OPERATOR_BINARY_GT,
          aql::NODE_TYPE_OPERATOR_BINARY_LE, aql::NODE_TYPE_OPERATOR_BINARY_LT,
          aql::NODE_TYPE_OPERATOR_BINARY_NE,
          aql::NODE_TYPE_OPERATOR_BINARY_EQ>(),
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

  if (!iresearch::checkAttributeAccess(attribute, ref, allowExpansion)) {
    if (!iresearch::checkAttributeAccess(value, ref, allowExpansion)) {
      // no suitable attribute access node found
      return false;
    }

    std::swap(attribute, value);
    cmp = kCmpMap[cmp - aql::NODE_TYPE_OPERATOR_BINARY_EQ];
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

bool attributeAccessEqual(aql::AstNode const* lhs, aql::AstNode const* rhs,
                          QueryContext const* ctx) {
  struct NodeValue {
    enum class Type {
      INVALID = 0,
      EXPANSION,  // [*]
      ACCESS,     // [<offset>] | [<string>] | .
      VALUE       // REFERENCE | VALUE
    };

    bool read(aql::AstNode const* node, QueryContext const* ctx) noexcept {
      this->strVal = std::string_view{};
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

      } else if (n == 2 &&
                 aql::NODE_TYPE_INDEXED_ACCESS == type) {  // [<something>]
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

    iresearch::ScopedAqlValue aqlValue;
    std::string_view strVal;
    int64_t iVal;
    Type type{Type::INVALID};
    aql::AstNode const* root = nullptr;
  } lhsValue, rhsValue;

  // TODO: is the "&" intionally. If yes: why?
  // cppcheck-suppress uninitvar; false positive
  while (lhsValue.read(lhs, ctx) &
         static_cast<unsigned>(rhsValue.read(rhs, ctx))) {
    if (lhsValue != rhsValue) {
      return false;
    }

    lhs = lhsValue.root;
    rhs = rhsValue.root;
  }

  return lhsValue.type != NodeValue::Type::INVALID &&
         rhsValue.type != NodeValue::Type::INVALID && rhsValue == lhsValue;
}

bool nameFromAttributeAccess(
    std::string& name, aql::AstNode const& node, QueryContext const& ctx,
    bool filter, std::span<InvertedIndexField const>* subFields /*= nullptr*/) {
  class AttributeChecker {
   public:
    AttributeChecker(std::string& str, QueryContext const& ctx,
                     bool filter) noexcept
        : _str{str},
          _ctx{ctx},
          _expansion{!ctx.isSearchQuery},
          _filter{filter} {}

    bool attributeAccess(aql::AstNode const& node) {
      std::string_view strValue;

      if (!parseValue(strValue, node)) {
        // wrong type
        return false;
      }

      append(strValue);
      return true;
    }

    bool expansion(aql::AstNode const&) {
      if (!_expansion) {
        return false;
      }
      _str.append("[*]");
      return true;
    }

    bool indexAccess(aql::AstNode const& node) {
      _value.reset(node);

      if (!_filter && _ctx.isSearchQuery) {
        // view query parsing time. Just accept anything
        return true;
      }

      if ((!_ctx.isSearchQuery && !node.isConstant() && !_ctx.ctx) ||
          !_value.execute(_ctx)) {
        // failed to evaluate value
        return false;
      }

      switch (_value.type()) {
        case iresearch::SCOPED_VALUE_TYPE_DOUBLE:
          append(_value.getInt64());
          return true;
        case iresearch::SCOPED_VALUE_TYPE_STRING: {
          std::string_view strValue;

          if (!_value.getString(strValue)) {
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

    void append(std::string_view value) {
      if (!_str.empty()) {
        _str.push_back(NESTING_LEVEL_DELIMITER);
      }
      _str.append(value);
    }

    void append(int64_t value) {
      _str.push_back(NESTING_LIST_OFFSET_PREFIX);
      auto const len = static_cast<size_t>(
          absl::numbers_internal::FastIntToBuffer(value, _buf) - &_buf[0]);
      _str.append(_buf, len);
      _str.push_back(NESTING_LIST_OFFSET_SUFFIX);
    }

   private:
    ScopedAqlValue _value;
    std::string& _str;
    QueryContext const& _ctx;
    char _buf[21];  // enough to hold all numbers up to 64-bits
    bool _expansion;
    bool _filter;
  } builder{name, ctx, filter};

  aql::AstNode const* head = nullptr;
  auto visitRes = visitAttributeAccess(head, &node, builder) && head &&
                  aql::NODE_TYPE_REFERENCE == head->type;

  if (visitRes && !ctx.isSearchQuery) {
    auto const fields = ctx.fields;
    auto const it = getNested(name, fields);
    visitRes = it != std::end(fields) && !it->_isSearchField &&
               !it->_trackListPositions;
    if (visitRes && subFields) {
      *subFields = it->_fields;
    }
  }
  return visitRes;
}

aql::AstNode const* checkAttributeAccess(aql::AstNode const* node,
                                         aql::Variable const& ref,
                                         bool allowExpansion) noexcept {
  struct AttributeChecker {
    AttributeChecker(bool expansion) : _expansion(expansion) {}

    bool attributeAccess(aql::AstNode const&) const { return true; }

    bool indexAccess(aql::AstNode const&) const { return true; }

    bool expansion(aql::AstNode const&) const { return _expansion; }

    bool _expansion;
  } checker(allowExpansion);

  aql::AstNode const* head = nullptr;

  return node &&
                 aql::NODE_TYPE_REFERENCE !=
                     node->type  // do not allow root node to be REFERENCE
                 && visitAttributeAccess(head, node, checker) && head &&
                 aql::NODE_TYPE_REFERENCE == head->type &&
                 reinterpret_cast<void const*>(&ref) ==
                     head->getData()  // same variable
             ? node
             : nullptr;
}

aql::Variable const* getSearchFuncRef(aql::AstNode const* args) noexcept {
  if (!args || aql::NODE_TYPE_ARRAY != args->type) {
    return nullptr;
  }

  size_t const size = args->numMembers();

  if (size < 1) {
    return nullptr;  // invalid args
  }

  // 1st argument has to be reference to `ref`
  auto const* arg0 = args->getMemberUnchecked(0);

  if (!arg0 || aql::NODE_TYPE_REFERENCE != arg0->type) {
    return nullptr;
  }

  for (size_t i = 1, size = args->numMembers(); i < size; ++i) {
    auto const* arg = args->getMemberUnchecked(i);

    if (!arg || !arg->isDeterministic()) {
      // we don't support non-deterministic arguments for scorers
      return nullptr;
    }
  }

  return reinterpret_cast<aql::Variable const*>(arg0->getData());
}

}  // namespace iresearch
}  // namespace arangodb
