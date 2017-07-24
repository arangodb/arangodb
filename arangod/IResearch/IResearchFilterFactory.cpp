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

#include "IResearchFilterFactory.h"
#include "IResearchDocument.h"
#include "IResearchAnalyzerFeature.h"
#include "IResearchKludge.h"
#include "ApplicationServerHelper.h"

#include "Aql/AstNode.h"
#include "Aql/Function.h"

#include "Logger/Logger.h"
#include "Logger/LogMacros.h"

#include "search/boolean_filter.hpp"
#include "search/term_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/granular_range_filter.hpp"
#include "search/phrase_filter.hpp"

NS_LOCAL

// ----------------------------------------------------------------------------
// --SECTION--                                        FilerFactory dependencies
// ----------------------------------------------------------------------------

template<arangodb::aql::AstNodeType Max>
constexpr bool checkAdjacency() {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @returns true if values from the specified range [Min;Max] are adjacent
////////////////////////////////////////////////////////////////////////////////
template<
  arangodb::aql::AstNodeType Max,
  arangodb::aql::AstNodeType Min,
  arangodb::aql::AstNodeType... Types
> constexpr bool checkAdjacency() {
  return (Max > Min) && (1 == (Max - Min)) && checkAdjacency<Min, Types...>();
}

std::string const* getNodeTypeName(arangodb::aql::AstNodeType type) noexcept {
  auto const it = arangodb::aql::AstNode::TypeNames.find(type);

  if (arangodb::aql::AstNode::TypeNames.end() == it) {
    return nullptr;
  }

  return &it->second;
}

void logMalformedNode(arangodb::aql::AstNodeType type) {
  auto const* typeName = getNodeTypeName(type);

  if (typeName) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Can't process malformed AstNode of type '"
                                             << *typeName << "'";
  } else {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Can't process malformed AstNode of type '"
                                             << type << "'";
  }
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
/// @brief normalizes input binary comparison node (==, !=, <, <=, >, >=) and
///        fills the specified struct
/// @returns true if the specified 'in' nodes has been successfully normalized,
///          false otherwise
////////////////////////////////////////////////////////////////////////////////
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

  if (attribute->type != arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    if (value->type != arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
      // no attribute access node found
      return false;
    }

    static arangodb::aql::AstNodeType const CmpMap[] {
      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ, // NODE_TYPE_OPERATOR_BINARY_EQ: 3 == a <==> a == 3
      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE, // NODE_TYPE_OPERATOR_BINARY_NE: 3 != a <==> a != 3
      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT, // NODE_TYPE_OPERATOR_BINARY_LT: 3 < a  <==> a > 3
      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE, // NODE_TYPE_OPERATOR_BINARY_LE: 3 <= a <==> a >= 3
      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT, // NODE_TYPE_OPERATOR_BINARY_GT: 3 > a  <==> a < 3
      arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE  // NODE_TYPE_OPERATOR_BINARY_GE: 3 >= a <==> a <= 3
    };

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

////////////////////////////////////////////////////////////////////////////////
/// @brief checks 2 nodes of type NODE_TYPE_ATTRIBUTE_ACCESS for equality
/// @returns true if the specified nodes are equal, false otherwise
////////////////////////////////////////////////////////////////////////////////
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

bool parseValue(size_t& value, arangodb::aql::AstNode const& node) {
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

template<typename Char>
bool parseValue(
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

// generates field name from the specified 'node' and value 'type'
std::string nameFromAttributeAccess(
    arangodb::aql::AstNode const& node,
    arangodb::aql::AstNodeValueType type
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS == node.type);

  std::string name;

  auto visitor = [&name](arangodb::aql::AstNode const& node) mutable {
    if (arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS == node.type) {
      TRI_ASSERT(arangodb::aql::VALUE_TYPE_STRING == node.value.type);
      name.append(node.getStringValue(), node.getStringLength());
      name += '.';
    }
    return true;
  };

  visit<false>(node, visitor);
  name.pop_back(); // remove extra '.'

  switch (type) {
    case arangodb::aql::VALUE_TYPE_NULL:
      arangodb::iresearch::kludge::mangleNull(name);
      break;
    case arangodb::aql::VALUE_TYPE_BOOL:
      arangodb::iresearch::kludge::mangleBool(name);
      break;
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE:
      arangodb::iresearch::kludge::mangleNumeric(name);
      break;
    case arangodb::aql::VALUE_TYPE_STRING:
      // FIXME TODO enable for string field comparison
      //mangleStringField(name);
      break;
  }

  return name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends value tokens to a phrase filter
////////////////////////////////////////////////////////////////////////////////
void appendTerms(
    irs::by_phrase& filter,
    irs::string_ref const& value,
    irs::analysis::analyzer& stream,
    size_t firstOffset) {
  // reset stream
  stream.reset(value);

  // get token attribute
  irs::term_attribute const& token = *stream.attributes().get<irs::term_attribute>();

  // add tokens
  while (stream.next()) {
    filter.push_back(token.value(), firstOffset);
    firstOffset = 0;
  }
}

bool processSubnode(
  irs::boolean_filter* filter,
  arangodb::aql::AstNode const& node
);

template<typename Filter>
bool fromGroup(
  irs::boolean_filter* filter,
  arangodb::aql::AstNode const& node
);

irs::by_term& byTerm(
    irs::by_term& filter,
    arangodb::aql::AstNode const& attributeNode,
    arangodb::aql::AstNode const& valueNode
) {
  switch (valueNode.value.type) {
    case arangodb::aql::VALUE_TYPE_NULL:
      filter.term(irs::null_token_stream::value_null());
      break;
    case arangodb::aql::VALUE_TYPE_BOOL:
      filter.term(valueNode.getBoolValue()
        ? irs::boolean_token_stream::value_true()
        : irs::boolean_token_stream::value_false());
      break;
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE: {
      irs::numeric_token_stream stream;
      irs::term_attribute const* term = stream.attributes().get<irs::term_attribute>().get();
      TRI_ASSERT(term);
      stream.reset(valueNode.getDoubleValue());
      stream.next();

      filter.term(term->value());
     } break;
    case arangodb::aql::VALUE_TYPE_STRING: {
      irs::bytes_ref value;
      parseValue(value, valueNode);
      filter.term(value);
    } break;
  }

  filter.field(
    nameFromAttributeAccess(attributeNode, valueNode.value.type)
  );

  return filter;
}

bool byPrefix(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& attributeNode,
    arangodb::aql::AstNode const& valueNode,
    arangodb::aql::AstNode const* scoringLimitNode) {
  size_t scoringLimit = 128; // FIXME make configurable

  if (scoringLimitNode && !parseValue(scoringLimit, *scoringLimitNode)) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'STARTS_WITH' AQL function: Unable to parse scoring limit, default value "
                                             << scoringLimit << " is going to be used";
    return false;
  }

  switch (valueNode.value.type) {
    case arangodb::aql::VALUE_TYPE_NULL:
    case arangodb::aql::VALUE_TYPE_BOOL:
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE:
      break;
    case arangodb::aql::VALUE_TYPE_STRING: {
      if (filter) {
        auto& prefixFilter = filter->add<irs::by_prefix>();
        byTerm(prefixFilter, attributeNode, valueNode);
        prefixFilter.scored_terms_limit(scoringLimit);
      }
      return true;
    }
  }

  try {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'STARTS_WITH' AQL function: Unable to parse specified value '" << valueNode.toString()
                                             << "' as a string";
  } catch (...) {
    // valueNode.toString() may throw
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'STARTS_WITH' AQL function: Unable to parse specified value as a string";
  }

  return false;
}

bool byRange(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& attributeNode,
    arangodb::aql::AstNode const& minValueNode,
    bool const minInclude,
    arangodb::aql::AstNode const& maxValueNode,
    bool const maxInclude
) {
  if (!minValueNode.isConstant() || !maxValueNode.isConstant()) { // can't process non constant nodes
    return false;
  }

  auto const type = minValueNode.value.type;

  if (type != maxValueNode.value.type && !(minValueNode.isNumericValue() && maxValueNode.isNumericValue())) {
    // type mismatch
    return false;
  }

  switch (type) {
    case arangodb::aql::VALUE_TYPE_NULL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        range.field(nameFromAttributeAccess(attributeNode, type));
        range.term<irs::Bound::MIN>(irs::null_token_stream::value_null());
        range.include<irs::Bound::MIN>(minInclude);;
        range.term<irs::Bound::MAX>(irs::null_token_stream::value_null());
        range.include<irs::Bound::MAX>(maxInclude);;
      }

      return true;
    }
    case arangodb::aql::VALUE_TYPE_BOOL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        auto const& minValue = minValueNode.getBoolValue()
          ? irs::boolean_token_stream::value_true()
          : irs::boolean_token_stream::value_false();

        auto const& maxValue = maxValueNode.getBoolValue()
          ? irs::boolean_token_stream::value_true()
          : irs::boolean_token_stream::value_false();

        range.field(nameFromAttributeAccess(attributeNode, type));
        range.term<irs::Bound::MIN>(minValue);
        range.include<irs::Bound::MIN>(minInclude);
        range.term<irs::Bound::MAX>(maxValue);
        range.include<irs::Bound::MAX>(maxInclude);
      }

      return true;
    }
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE: {
      if (filter) {
        auto& range = filter->add<irs::by_granular_range>();

        range.field(nameFromAttributeAccess(attributeNode, type));

        irs::numeric_token_stream stream;

        // setup min bound
        stream.reset(minValueNode.getDoubleValue());
        range.insert<irs::Bound::MIN>(stream);
        range.include<irs::Bound::MIN>(minInclude);

        // setup max bound
        stream.reset(maxValueNode.getDoubleValue());
        range.insert<irs::Bound::MAX>(stream);
        range.include<irs::Bound::MAX>(maxInclude);
      }

      return true;
    }
    case arangodb::aql::VALUE_TYPE_STRING: {
      irs::bytes_ref minValue, maxValue;

      if (!parseValue(minValue, minValueNode) || !parseValue(maxValue, maxValueNode)) {
        // unable to parse value
        return false;
      }

      if (filter) {
        auto& range = filter->add<irs::by_range>();

        range.field(nameFromAttributeAccess(attributeNode, type));
        range.term<irs::Bound::MIN>(minValue);
        range.include<irs::Bound::MIN>(minInclude);
        range.term<irs::Bound::MAX>(maxValue);
        range.include<irs::Bound::MAX>(maxInclude);
      }

      return true;
    }
  }

  return false;
}

template<irs::Bound Bound>
bool byRange(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& attributeNode,
    arangodb::aql::AstNode const& valueNode,
    bool const incl
) {
  if (!valueNode.isConstant()) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to handle non constant values for interval";
    return false; // can't process non constant nodes
  }

  switch (valueNode.value.type) {
    case arangodb::aql::VALUE_TYPE_NULL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        range.field(nameFromAttributeAccess(attributeNode, valueNode.value.type));
        range.term<Bound>(irs::null_token_stream::value_null());
        range.include<Bound>(incl);
      }

      return true;
    }
    case arangodb::aql::VALUE_TYPE_BOOL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();
        auto const& value = valueNode.getBoolValue()
                          ? irs::boolean_token_stream::value_true()
                          : irs::boolean_token_stream::value_false();

        range.field(nameFromAttributeAccess(attributeNode, valueNode.value.type));
        range.term<Bound>(value);
        range.include<Bound>(incl);
      }

      return true;
    }
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE: {
      if (filter) {
        auto& range = filter->add<irs::by_granular_range>();
        irs::numeric_token_stream stream;

        stream.reset(valueNode.getDoubleValue());
        range.field(nameFromAttributeAccess(attributeNode, valueNode.value.type));
        range.insert<Bound>(stream);
        range.include<Bound>(incl);
      }

      return true;
    }
    case arangodb::aql::VALUE_TYPE_STRING: {
      irs::bytes_ref value;

      if (!parseValue(value, valueNode)) {
        // unable to parse value
        return false;
      }

      if (filter) {
        auto& range = filter->add<irs::by_range>();

        range.field(nameFromAttributeAccess(attributeNode, valueNode.value.type));
        range.term<Bound>(value);
        range.include<Bound>(incl);
      }

      return true;
    }
  }

  return false;
}

bool fromInterval(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == node.type
  );

  NormalizedCmpNode normNode;

  if (!normalizeCmpNode(node, normNode)) {
    auto const* typeName = getNodeTypeName(node);

    if (typeName) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to normalize operator " << *typeName;
    } else {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to normalize operator " << node.type;
    }

    return false; // unable to normalize node
  }

  bool const incl = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == normNode.cmp
                 || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == normNode.cmp;

  bool const min = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == normNode.cmp
                || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == normNode.cmp;

  return min ? byRange<irs::Bound::MIN>(filter, *normNode.attribute, *normNode.value, incl)
             : byRange<irs::Bound::MAX>(filter, *normNode.attribute, *normNode.value, incl);
}

bool fromBinaryEq(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type
  );

  NormalizedCmpNode normalized;

  if (!normalizeCmpNode(node, normalized)) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to normalize operator '=='";
    return false; // unable to normalize node
  }

  if (filter) {
    auto& termFilter = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type
                     ? filter->add<irs::Not>().filter<irs::by_term>()
                     : filter->add<irs::by_term>();

    byTerm(termFilter, *normalized.attribute, *normalized.value);
  }

  return true;
}

bool fromRange(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_RANGE == node.type);

  if (node.numMembers() != 2) {
    logMalformedNode(node.type);
    return false; // wrong number of members
  }

  // ranges are always true
  if (filter) {
    filter->add<irs::all>();
  }

  return true;
}

bool fromIn(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type
  );

  if (node.numMembers() != 2) {
    logMalformedNode(node.type);
    return false; // wrong number of members
  }

  auto const* attributeNode = getNode(node, 0, arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (!attributeNode) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to extract attribute name from 'IN' operator";
    return false; // wrong attriubte node type
  }

  auto* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(valueNode);

  size_t const n = valueNode->numMembers();

  if (arangodb::aql::NODE_TYPE_ARRAY == valueNode->type) { // array of values
    if (!n) {
      if (filter) {
        if (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
          filter->add<irs::all>(); // not in [] means 'all'
        } else {
          filter->add<irs::empty>();
        }
      }

      // nothing to do more
      return true;
    }

    if (filter) {
      filter = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type
        ? &static_cast<irs::boolean_filter&>(filter->add<irs::Not>().filter<irs::And>())
        : &static_cast<irs::boolean_filter&>(filter->add<irs::Or>());
    }

    for (size_t i = 0; i < n; ++i) {
      auto const* elementNode = valueNode->getMemberUnchecked(i);
      TRI_ASSERT(valueNode);

      if (elementNode->type != arangodb::aql::NODE_TYPE_VALUE
          || !elementNode->isConstant()) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to process non constant array value";
        return false;
      }

      if (filter) {
        byTerm(filter->add<irs::by_term>(), *attributeNode, *elementNode);
      }
    }

    return true;
  } else if (arangodb::aql::NODE_TYPE_RANGE == valueNode->type) { // inclusive range
    if (n != 2) {
      logMalformedNode(valueNode->type);
      return false; // wrong range
    }

    auto const* minValueNode = getNode(*valueNode, 0, arangodb::aql::NODE_TYPE_VALUE);

    if (!minValueNode || !minValueNode->isConstant()) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to parse left bound of the RANGE node";
      return false; // wrong left node
    }

    auto const* maxValueNode = getNode(*valueNode, 1, arangodb::aql::NODE_TYPE_VALUE);

    if (!maxValueNode || !maxValueNode->isConstant()) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to parse right bound of the RANGE node";
      return false; // wrong right node
    }

    if (filter && arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
      // handle negation
      filter = &filter->add<irs::Not>().filter<irs::Or>();
    }

    return byRange(filter, *attributeNode, *minValueNode, true, *maxValueNode, true);
  }

  auto const* typeName = getNodeTypeName(valueNode->type);

  if (typeName) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Wrong Ast node of type '" << *typeName << "' detected in 'IN' operator";
  } else {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Wrong Ast node of type '" << valueNode->type << "' detected in 'IN' operator";
  }

  // wrong value node type
  return false;
}

bool fromValue(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    arangodb::aql::NODE_TYPE_VALUE == node.type
    || arangodb::aql::NODE_TYPE_ARRAY == node.type
    || arangodb::aql::NODE_TYPE_OBJECT == node.type
  );

  if (!filter) {
    return true; // nothing more to validate
  } else if (node.isTrue()) {
    filter->add<irs::all>();
  } else {
    filter->add<irs::empty>();
  }

  return true;
}

bool fromNegation(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_UNARY_NOT == node.type);

  if (node.numMembers() != 1) {
    logMalformedNode(node.type);
    return false; // wrong number of members
  }

  auto const* member = node.getMemberUnchecked(0);
  TRI_ASSERT(member);

  if (filter) {
    filter = &filter->add<irs::Not>().filter<irs::And>();
  }

  return processSubnode(filter, *member);
}

bool fromBinaryAnd(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND == node.type);

  if (node.numMembers() != 2) {
    logMalformedNode(node.type);
    return false; // wrong number of members
  }

  auto const* lhsNode = node.getMemberUnchecked(0);
  TRI_ASSERT(lhsNode);
  auto const* rhsNode = node.getMemberUnchecked(1);
  TRI_ASSERT(rhsNode);

  NormalizedCmpNode lhsNormNode, rhsNormNode;

  if (normalizeCmpNode(*lhsNode, lhsNormNode) && normalizeCmpNode(*rhsNode, rhsNormNode)) {
    bool const lhsInclude = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == lhsNormNode.cmp;
    bool const rhsInclude = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == rhsNormNode.cmp;

    if ((lhsInclude || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == lhsNormNode.cmp)
         && (rhsInclude || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == rhsNormNode.cmp)) {

      auto const* lhsAttr = lhsNormNode.attribute;
      auto const* rhsAttr = rhsNormNode.attribute;

      if (attributeAccessEqual(lhsAttr, rhsAttr)) {
        auto const* lhsValue = lhsNormNode.value;
        auto const* rhsValue = rhsNormNode.value;

        if (byRange(filter, *lhsAttr, *lhsValue, lhsInclude, *rhsValue, rhsInclude)) {
          // successsfully parsed as range
          return true;
        }
      }
    }
  }

  // treat as ordinal 'And'
  return fromGroup<irs::And>(filter, node);
}

template<typename Filter>
bool fromGroup(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND == node.type
   || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_OR == node.type
   || arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND == node.type
   || arangodb::aql::NODE_TYPE_OPERATOR_NARY_OR == node.type);

  size_t const n = node.numMembers();

  if (!n) {
    // nothing to do
    return true;
  }

  if (filter) {
    filter = &filter->add<Filter>();
  }

  for (size_t i = 0; i < n; ++i) {
    auto const* valueNode = node.getMemberUnchecked(i);
    TRI_ASSERT(valueNode);

    if (!processSubnode(filter, *valueNode)) {
      return false;
    }
  }

  return true;
}

// EXISTS(<attribute>)
bool fromFuncExists(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& args
) {
  auto const argc = args.numMembers();

  if (argc != 1) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'EXISTS' AQL function: Invalid number of arguments passed (must be == 1)";
    return false;
  }

  // 1st argument defines a field
  auto const* field = getNode(args, 0, arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (!field) {
    return false;
  }

  // FIXME TODO

  return false;
}

// PHRASE(<attribute>, <value> [, <offset>, <value>, ...] [, <locale>])
bool fromFuncPhrase(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& args
) {
  auto* analyzerFeature = arangodb::iresearch::getFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();

  if (!analyzerFeature) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'" << arangodb::iresearch::IResearchAnalyzerFeature::name()
                                   << "' feature is not registered, unable to evaluate 'PHRASE' function";
    return false;
  }

  auto const argc = args.numMembers();

  if (argc < 2) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'PHRASE' AQL function: Invalid number of arguments passed (must be >= 2)";
    return false;
  }

  // 1st argument defines a field
  auto const* fieldArg = getNode(args, 0, arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (!fieldArg) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'PHRASE' AQL function: 1st argument is invalid";
    return false;
  }

  // 2nd argument defines a value
  auto const* valueArg = getNode(args, 1, arangodb::aql::NODE_TYPE_VALUE);

  if (!valueArg) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'PHRASE' AQL function: 2nd argument is invalid";
    return false;
  }

  irs::string_ref value;

  if (!parseValue(value, *valueArg)) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'PHRASE' AQL function: Unable to parse 2nd argument as a string";
    return false;
  }

  // if custom analyzer is present
  // as the last argument then use it
  bool const customAnalyzer = argc & 1;

  irs::string_ref analyzerName = analyzerFeature->identity();
  irs::analysis::analyzer::ptr analyzer;

  if (customAnalyzer) {
    decltype(fieldArg) analyzerArg = getNode(args, argc - 1, arangodb::aql::NODE_TYPE_VALUE);

    if (!analyzerArg || !parseValue(analyzerName, *analyzerArg)) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'PHRASE' AQL function: Unable to parse analyzer";
      return false;
    }
  }

  auto pool = analyzerFeature->get(analyzerName);

  if (!pool) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'PHRASE' AQL function: Unable to load requested analyzer '" << analyzerName << "'";
    return false;
  }

  analyzer = pool.get(); // get analyzer from pool

  if (!analyzer) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'PHRASE' AQL function: Unable to instantiate analyzer '" << analyzerName << "'";
    return false;
  }

  irs::by_phrase* phrase = nullptr;

  if (filter) {
    phrase = &filter->add<irs::by_phrase>();
    phrase->field(nameFromAttributeAccess(*fieldArg, arangodb::aql::VALUE_TYPE_STRING));

    appendTerms(*phrase, value, *analyzer, 0);
  }

  decltype(fieldArg) offsetArg = nullptr;
  size_t offset;
  for (size_t idx = 2, end = argc - customAnalyzer; idx < end; idx += 2) {
    offsetArg = getNode(args, idx, arangodb::aql::NODE_TYPE_VALUE);

    if (!offsetArg || !parseValue(offset, *offsetArg)) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'PHRASE' AQL function: Unable to parse argument on position " << idx << " as an offset";
      return false;
    }

    valueArg = getNode(args, idx + 1, arangodb::aql::NODE_TYPE_VALUE);

    if (!valueArg || !parseValue(value, *valueArg)) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'PHRASE' AQL function: Unable to parse argument on position " << idx + 1 << " as a value";
      return false;
    }

    if (phrase) {
      appendTerms(*phrase, value, *analyzer, offset);
    }
  }

  return true;
}

// STARTS_WITH(<attribute>, <prefix>, [<scoring-limit>])
bool fromFuncStartsWith(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& args
) {
  auto const argc = args.numMembers();

  if (argc < 2) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'STARTS_WITH' AQL function: Invalid number of arguments passed (should be >= 2)";
    return false;
  }

  // 1st argument defines a field
  auto const* field = getNode(args, 0, arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (!field) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'STARTS_WITH' AQL function: Unable to parse 1st argument as an attribute identifier";
    return false;
  }

  // 2nd argument defines a value
  auto const* prefix = getNode(args, 1, arangodb::aql::NODE_TYPE_VALUE);

  if (!prefix) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "'STARTS_WITH' AQL function: Unable to parse 2nd argument as a prefix";
    return false;
  }

  // 3rd (optional) argument defines a number of scored terms
  decltype(prefix) scoringLimit = nullptr;

  if (argc > 2) {
    scoringLimit = getNode(args, 2, arangodb::aql::NODE_TYPE_VALUE);
  }

  return byPrefix(filter, *field, *prefix, scoringLimit);
}

bool fromFCallUser(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL_USER == node.type);

  if (node.numMembers() != 1) {
    logMalformedNode(node.type);
    return false;
  }

  auto const* args = getNode(node, 0, arangodb::aql::NODE_TYPE_ARRAY);

  if (!args) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to parse user function arguments as an array'";
    return false; // invalid args
  }

  typedef std::function<
    bool(irs::boolean_filter*, arangodb::aql::AstNode const&)
  > ConvertionHandler;

  static std::map<irs::string_ref, ConvertionHandler> convHandlers {
    { "IR::PHRASE", fromFuncPhrase },
    { "IR::STARTS_WITH", fromFuncStartsWith },
    { "IR::EXISTS", fromFuncExists }
//  { "MIN_MATCH", fromFuncMinMatch } // add when AQL will support filters as the function parameters
  };

  irs::string_ref name;

  if (!parseValue(name, node)) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to parse user function name";
    return false;
  }

  auto const entry = convHandlers.find(name);

  if (entry == convHandlers.end()) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to find user function '" << name << "'";
    return false;
  }

  return entry->second(filter, *args);
}

bool fromFCall(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL == node.type);

  typedef std::function<
    bool(irs::boolean_filter*, arangodb::aql::AstNode const&)
  > ConvertionHandler;

  static std::map<irs::string_ref, ConvertionHandler> convHandlers;

  auto const* fn = static_cast<arangodb::aql::Function*>(node.getData());

  if (!fn || node.numMembers() != 1) {
    logMalformedNode(node.type);
    return false; // no function
  }

  auto const* args = getNode(node, 0, arangodb::aql::NODE_TYPE_ARRAY);

  if (!args) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to parse system function arguments as an array'";
    return false; // invalid args
  }

  const irs::string_ref name = fn->externalName;

  auto const entry = convHandlers.find(name);

  if (entry == convHandlers.end()) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to find system function '" << name << "'";
    return false;
  }

  return entry->second(filter, *args);
}

bool processSubnode(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  switch (node.type) {
    case arangodb::aql::NODE_TYPE_OPERATOR_UNARY_NOT: // unary minus
      return fromNegation(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND: // logical and
      return fromBinaryAnd(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_OR: // logical or
      return fromGroup<irs::Or>(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ: // compare ==
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE: // compare !=
      return fromBinaryEq(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT: // compare <
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE: // compare <=
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT: // compare >
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE: // compare >=
      return fromInterval(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN: // compare in
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN: // compare not in
      return fromIn(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_TERNARY: // ternary
      break;
    case arangodb::aql::NODE_TYPE_VALUE : // value
    case arangodb::aql::NODE_TYPE_ARRAY: // array
    case arangodb::aql::NODE_TYPE_OBJECT: // array
      return fromValue(filter, node);
    case arangodb::aql::NODE_TYPE_FCALL: // function call
      return fromFCall(filter, node);
    case arangodb::aql::NODE_TYPE_FCALL_USER: // user function call
      return fromFCallUser(filter, node);
    case arangodb::aql::NODE_TYPE_RANGE: // range
      return fromRange(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND: // n-ary and
      return fromGroup<irs::And>(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_NARY_OR: // n-ary or
      return fromGroup<irs::Or>(filter, node);
    default:
      break;
  }

  auto const* typeName = getNodeTypeName(node.type);

  if (typeName) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to process Ast node of type '" << *typeName << "'";
  } else {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Unable to process Ast node of type '" << node.type << "'";
  }

  return false; // unsupported node type
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

// ----------------------------------------------------------------------------
// --SECTION--                                      FilerFactory implementation
// ----------------------------------------------------------------------------

/*static*/ irs::filter::ptr FilterFactory::filter(TRI_voc_cid_t cid) {
  auto filter = irs::by_term::make();

  // filter matching on cid
  static_cast<irs::by_term&>(*filter)
    .field(DocumentPrimaryKey::CID()) // set field
    .term(DocumentPrimaryKey::encode(cid)); // set value

  return std::move(filter);
}

/*static*/ irs::filter::ptr FilterFactory::filter(
    TRI_voc_cid_t cid,
    TRI_voc_rid_t rid
) {
  auto filter = irs::And::make();

  // filter matching on cid and rid
  static_cast<irs::And&>(*filter).add<irs::by_term>()
    .field(DocumentPrimaryKey::CID()) // set field
    .term(DocumentPrimaryKey::encode(cid)); // set value

  static_cast<irs::And&>(*filter).add<irs::by_term>()
    .field(DocumentPrimaryKey::RID()) // set field
    .term(DocumentPrimaryKey::encode(rid)); // set value

  return std::move(filter);
}

/*static*/ bool FilterFactory::filter(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  if (arangodb::aql::NODE_TYPE_FILTER != node.type) {
    // wrong root node type
    return false;
  }

  if (node.numMembers() != 1) {
    logMalformedNode(node.type);
    return false; // wrong number of members
  }

  auto const* member = node.getMemberUnchecked(0);

  return member && processSubnode(filter, *member);
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
