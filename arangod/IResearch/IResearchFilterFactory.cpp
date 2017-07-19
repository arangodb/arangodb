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
#include "IResearchKludge.h"

#include "Aql/AstNode.h"
#include "Aql/Function.h"

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
    // can't parse scoring limit
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

template<irs::Bound Bound, bool Include>
bool byRange(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& attributeNode,
    arangodb::aql::AstNode const& valueNode
) {
  if (!valueNode.isConstant()) {
    // can't process non constant nodes
    return false;
  }

  switch (valueNode.value.type) {
    case arangodb::aql::VALUE_TYPE_NULL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        range.field(nameFromAttributeAccess(attributeNode, valueNode.value.type));
        range.term<Bound>(irs::null_token_stream::value_null());
        range.include<Bound>(Include);;
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
        range.include<Bound>(Include);
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
        range.include<Bound>(Include);
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
        range.include<Bound>(Include);
      }

      return true;
    }
  }

  return false;
}

template<irs::Bound Bound, bool Include>
bool fromInterval(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == node.type && irs::Bound::MAX == Bound && !Include)
     || (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == node.type && irs::Bound::MAX == Bound && Include)
     || (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == node.type && irs::Bound::MIN == Bound && !Include)
     || (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == node.type && irs::Bound::MIN == Bound && Include)
  );

  if (node.numMembers() != 2) {
    // wrong number of members
    return false;
  }

  auto const* attributeNode = getNode(node, 0, arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (!attributeNode) {
    // wrong attribute node type
    return false;
  }

  auto const* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(attributeNode);

  if (!valueNode->isConstant()) {
    return false;
  }

  return byRange<Bound, Include>(filter, *attributeNode, *valueNode);
}

bool fromBinaryEq(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type);

  if (node.numMembers() != 2) {
    // wrong number of members
    return false;
  }

  auto const* attributeNode = getNode(node, 0, arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (!attributeNode) {
    // wrong attribute node type
    return false;
  }

  auto* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(attributeNode);

  if (!valueNode->isConstant()) {
    return false;
  }

  if (filter) {
    auto& termFilter = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type
                     ? filter->add<irs::Not>().filter<irs::by_term>()
                     : filter->add<irs::by_term>();

    byTerm(termFilter, *attributeNode, *valueNode);
  }

  return true;
}

bool fromRange(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_RANGE == node.type);

  if (node.numMembers() != 2) {
    // wrong number of members
    return false;
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
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type);

  if (node.numMembers() != 2) {
    // wrong number of members
    return false;
  }

  auto const* attributeNode = getNode(node, 0, arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (!attributeNode) {
    // wrong attriubte node type
    return false;
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
        return false;
      }

      if (filter) {
        byTerm(filter->add<irs::by_term>(), *attributeNode, *elementNode);
      }
    }

    return true;
  } else if (arangodb::aql::NODE_TYPE_RANGE == valueNode->type) { // inclusive range
    if (n != 2) {
      // wrong range
      return false;
    }

    auto const* minValueNode = getNode(*valueNode, 0, arangodb::aql::NODE_TYPE_VALUE);

    if (!minValueNode || !minValueNode->isConstant()) {
      // wrong left node
      return false;
    }

    auto const* maxValueNode = getNode(*valueNode, 1, arangodb::aql::NODE_TYPE_VALUE);

    if (!maxValueNode || !maxValueNode->isConstant()) {
      // wrong right node
      return false;
    }

    if (filter && arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
      // handle negation
      filter = &filter->add<irs::Not>().filter<irs::Or>();
    }

    return byRange(filter, *attributeNode, *minValueNode, true, *maxValueNode, true);
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
    // wrong number of members
    return false;
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
    // wrong number of members
    return false;
  }

  auto const* lhsNode = node.getMemberUnchecked(0);
  TRI_ASSERT(lhsNode);
  auto const* rhsNode = node.getMemberUnchecked(1);
  TRI_ASSERT(rhsNode);

  bool const lhsInclude = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == lhsNode->type;
  bool const rhsInclude = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == rhsNode->type;

  if ((lhsInclude || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT == lhsNode->type)
       && (rhsInclude || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT == rhsNode->type)) {
    if (lhsNode->numMembers() != 2 || rhsNode->numMembers() != 2) {
      // wrong number of members
      return false;
    }

    auto const* lhsAttribute = getNode(*lhsNode, 0, arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

    if (!lhsAttribute) {
      // wrong attribute node type
      return false;
    }

    auto const* rhsAttribute = getNode(*rhsNode, 0, arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

    if (!rhsAttribute) {
      // wrong attribute node type
      return false;
    }

    if (0 == 1) {//arangodb::aql::CompareAstNodes(lhsAttribute, rhsAttribute, true)) {
      // range case
      auto const* lhsValue = getNode(*lhsNode, 1, arangodb::aql::NODE_TYPE_VALUE);

      if (!lhsValue) {
        // wrong value node type
        return false;
      }

      auto const* rhsValue = getNode(*rhsNode, 1, arangodb::aql::NODE_TYPE_VALUE);

      if (!rhsValue) {
        // wrong value node type
        return false;
      }

      return byRange(filter, *lhsAttribute, *lhsValue, lhsInclude, *rhsValue, rhsInclude);
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
    // wrong number of arguments
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
  auto const argc = args.numMembers();

  if (argc < 2) {
    // wrong number of arguments
    return false;
  }

  // 1st argument defines a field
  auto const* fieldArg = getNode(args, 0, arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (!fieldArg) {
    // wrong node type
    return false;
  }

  // 2nd argument defines a value
  auto const* valueArg = getNode(args, 1, arangodb::aql::NODE_TYPE_VALUE);

  if (!valueArg) {
    // wrong node type
    return false;
  }

  irs::bytes_ref value;

  if (!parseValue(value, *valueArg)) {
    // unable to parse value as string
    return false;
  }

  // if custom locale is present 
  // as the last argument then use it
  const bool hasLocale = argc & 1;

  if (hasLocale) {
    decltype(fieldArg) analyzerArg = getNode(args, argc - 1, arangodb::aql::NODE_TYPE_VALUE);

    if (!analyzerArg) {
      // wrong node type
      return false;
    }

    irs::string_ref analyzerName;

    if (!parseValue(analyzerName, *analyzerArg)) {
      // unable to parse value as string
      return false;
    }
  }

  irs::by_phrase* phrase = nullptr;

  if (filter) {
    phrase = &filter->add<irs::by_phrase>();
    phrase->field(nameFromAttributeAccess(*fieldArg, arangodb::aql::VALUE_TYPE_STRING));
    phrase->push_back(value); // FIXME push tokens produced by analyzer
  }

  // check nodes
  decltype(fieldArg) offsetArg = nullptr;
  size_t offset;
  for (size_t idx = 2, end = argc - hasLocale; idx < end; idx += 2) {
    offsetArg = getNode(args, idx, arangodb::aql::NODE_TYPE_VALUE);

    if (!offsetArg || !parseValue(offset, *offsetArg)) {
      // unable to parse offset value
      return false;
    }

    valueArg = getNode(args, idx + 1, arangodb::aql::NODE_TYPE_VALUE);

    if (!valueArg || !parseValue(value, *valueArg)) {
      // unable to parse phrase value
      return false;
    }

    if (phrase) {
      phrase->push_back(value, offset); // FIXME push tokens produced by analyzer
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
    // wrong number of arguments
    return false;
  }

  // 1st argument defines a field
  auto const* field = getNode(args, 0, arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

  if (!field) {
    return false;
  }

  // 2nd argument defines a value
  auto const* prefix = getNode(args, 1, arangodb::aql::NODE_TYPE_VALUE);

  if (!prefix) {
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
    // invalid number of members
    return false;
  }

  auto const* args = getNode(node, 0, arangodb::aql::NODE_TYPE_ARRAY);

  if (!args) {
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
    // unable to parse value as string
    return false;
  }

  auto const entry = convHandlers.find(name);

  if (entry == convHandlers.end()) {
    // user function is not registered
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
    return false; // no function
  }

  auto const* args = getNode(node, 0, arangodb::aql::NODE_TYPE_ARRAY);

  if (!args) {
    return false; // invalid args
  }

  const irs::string_ref name = fn->externalName;

  auto const entry = convHandlers.find(name);

  if (entry == convHandlers.end()) {
    // system function is not registered
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
      return fromInterval<irs::Bound::MAX, false>(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE: // compare <=
      return fromInterval<irs::Bound::MAX, true>(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT: // compare >
      return fromInterval<irs::Bound::MIN, false>(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE: // compare >=
      return fromInterval<irs::Bound::MIN, true>(filter, node);
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

  // unsupported type
  return false;
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
    // wrong number of members
    return false;
  }

  auto const* member = node.getMemberUnchecked(0);

  return member && processSubnode(filter, *member);
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
