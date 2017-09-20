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
#include "IResearchFeature.h"
#include "IResearchKludge.h"
#include "ApplicationServerHelper.h"
#include "AstHelper.h"

#include "Aql/Function.h"

#include "Logger/Logger.h"
#include "Logger/LogMacros.h"

#include "search/all_filter.hpp"
#include "search/boolean_filter.hpp"
#include "search/term_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/granular_range_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/phrase_filter.hpp"

#include <cctype>
#include <type_traits>

NS_LOCAL

typedef std::function<
  bool(irs::boolean_filter*, arangodb::aql::AstNode const&)
> ConvertionHandler;

////////////////////////////////////////////////////////////////////////////////
/// @brief logs message about malformed AstNode with the specified type
////////////////////////////////////////////////////////////////////////////////
void logMalformedNode(arangodb::aql::AstNodeType type) {
  auto const* typeName = arangodb::iresearch::getNodeTypeName(type);

  if (typeName) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Can't process malformed AstNode of type '"
                                             << *typeName << "'";
  } else {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Can't process malformed AstNode of type '"
                                             << type << "'";
  }
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
  auto name = arangodb::iresearch::nameFromAttributeAccess(attributeNode);

  switch (valueNode.value.type) {
    case arangodb::aql::VALUE_TYPE_NULL:
      arangodb::iresearch::kludge::mangleNull(name);
      filter.term(irs::null_token_stream::value_null());
      break;
    case arangodb::aql::VALUE_TYPE_BOOL:
      arangodb::iresearch::kludge::mangleBool(name);
      filter.term(valueNode.getBoolValue()
        ? irs::boolean_token_stream::value_true()
        : irs::boolean_token_stream::value_false());
      break;
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE: {
      arangodb::iresearch::kludge::mangleNumeric(name);

      irs::numeric_token_stream stream;
      irs::term_attribute const* term = stream.attributes().get<irs::term_attribute>().get();
      TRI_ASSERT(term);
      stream.reset(valueNode.getDoubleValue());
      stream.next();

      filter.term(term->value());
     } break;
    case arangodb::aql::VALUE_TYPE_STRING: {
      arangodb::iresearch::kludge::mangleStringField(
         name,
         arangodb::iresearch::IResearchAnalyzerFeature::identity()
      );

      irs::bytes_ref value;
      arangodb::iresearch::parseValue(value, valueNode);
      filter.term(value);
    } break;
  }

  filter.field(std::move(name));

  return filter;
}

bool byPrefix(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& attributeNode,
    arangodb::aql::AstNode const& valueNode,
    arangodb::aql::AstNode const* scoringLimitNode) {
  size_t scoringLimit = 128; // FIXME make configurable

  if (scoringLimitNode && !arangodb::iresearch::parseValue(scoringLimit, *scoringLimitNode)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'STARTS_WITH' AQL function: Unable to parse scoring limit, default value "
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
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'STARTS_WITH' AQL function: Unable to parse specified value '" << valueNode.toString()
                                             << "' as string";
  } catch (...) {
    // valueNode.toString() may throw
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'STARTS_WITH' AQL function: Unable to parse specified value as string";
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

  auto name = arangodb::iresearch::nameFromAttributeAccess(attributeNode);

  switch (type) {
    case arangodb::aql::VALUE_TYPE_NULL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        arangodb::iresearch::kludge::mangleNull(name);
        range.field(std::move(name));
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

        arangodb::iresearch::kludge::mangleBool(name);
        range.field(std::move(name));
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

        arangodb::iresearch::kludge::mangleNumeric(name);
        range.field(std::move(name));

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

      if (!arangodb::iresearch::parseValue(minValue, minValueNode)
          || !arangodb::iresearch::parseValue(maxValue, maxValueNode)) {
        // unable to parse value
        return false;
      }

      if (filter) {
        auto& range = filter->add<irs::by_range>();

        arangodb::iresearch::kludge::mangleStringField(
          name,
          arangodb::iresearch::IResearchAnalyzerFeature::identity()
        );
        range.field(std::move(name));

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
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to handle non constant values for interval";
    return false; // can't process non constant nodes
  }

  auto name = arangodb::iresearch::nameFromAttributeAccess(attributeNode);

  switch (valueNode.value.type) {
    case arangodb::aql::VALUE_TYPE_NULL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        arangodb::iresearch::kludge::mangleNull(name);
        range.field(std::move(name));
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

        arangodb::iresearch::kludge::mangleBool(name);
        range.field(std::move(name));
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

        arangodb::iresearch::kludge::mangleNumeric(name);
        range.field(std::move(name));

        stream.reset(valueNode.getDoubleValue());
        range.insert<Bound>(stream);
        range.include<Bound>(incl);
      }

      return true;
    }
    case arangodb::aql::VALUE_TYPE_STRING: {
      irs::bytes_ref value;

      if (!arangodb::iresearch::parseValue(value, valueNode)) {
        // unable to parse value
        return false;
      }

      if (filter) {
        auto& range = filter->add<irs::by_range>();

        arangodb::iresearch::kludge::mangleStringField(
          name,
          arangodb::iresearch::IResearchAnalyzerFeature::identity()
        );
        range.field(std::move(name));
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

  arangodb::iresearch::NormalizedCmpNode normNode;

  if (!arangodb::iresearch::normalizeCmpNode(node, normNode)) {
    auto const* typeName = arangodb::iresearch::getNodeTypeName(node.type);

    if (typeName) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to normalize operator " << *typeName;
    } else {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to normalize operator " << node.type;
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

  arangodb::iresearch::NormalizedCmpNode normalized;

  if (!arangodb::iresearch::normalizeCmpNode(node, normalized)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to normalize operator '=='";
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

  auto const* attributeNode = arangodb::iresearch::checkAttributeAccess(
    node.getMemberUnchecked(0)
  );

  if (!attributeNode) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to extract attribute name from 'IN' operator";
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
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to process non constant array value";
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

    auto const* minValueNode = arangodb::iresearch::getNode(
      *valueNode, 0, arangodb::aql::NODE_TYPE_VALUE
    );

    if (!minValueNode || !minValueNode->isConstant()) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to parse left bound of the RANGE node";
      return false; // wrong left node
    }

    auto const* maxValueNode = arangodb::iresearch::getNode(
      *valueNode, 1, arangodb::aql::NODE_TYPE_VALUE
    );

    if (!maxValueNode || !maxValueNode->isConstant()) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to parse right bound of the RANGE node";
      return false; // wrong right node
    }

    if (filter && arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
      // handle negation
      filter = &filter->add<irs::Not>().filter<irs::Or>();
    }

    return byRange(filter, *attributeNode, *minValueNode, true, *maxValueNode, true);
  }

  auto const* typeName = arangodb::iresearch::getNodeTypeName(valueNode->type);

  if (typeName) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Wrong Ast node of type '" << *typeName << "' detected in 'IN' operator";
  } else {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Wrong Ast node of type '" << valueNode->type << "' detected in 'IN' operator";
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

  return arangodb::iresearch::FilterFactory::filter(filter, *member);
}

bool rangeFromBinaryAnd(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND == node.type
  );

  if (node.numMembers() != 2) {
    logMalformedNode(node.type);
    return false; // wrong number of members
  }

  auto const* lhsNode = node.getMemberUnchecked(0);
  TRI_ASSERT(lhsNode);
  auto const* rhsNode = node.getMemberUnchecked(1);
  TRI_ASSERT(rhsNode);

  arangodb::iresearch::NormalizedCmpNode lhsNormNode, rhsNormNode;

  if (arangodb::iresearch::normalizeCmpNode(*lhsNode, lhsNormNode)
      && arangodb::iresearch::normalizeCmpNode(*rhsNode, rhsNormNode)) {
    bool const lhsInclude = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == lhsNormNode.cmp;
    bool const rhsInclude = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == rhsNormNode.cmp;

    if ((lhsInclude || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == lhsNormNode.cmp)
         && (rhsInclude || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == rhsNormNode.cmp)) {

      auto const* lhsAttr = lhsNormNode.attribute;
      auto const* rhsAttr = rhsNormNode.attribute;

      if (arangodb::iresearch::attributeAccessEqual(lhsAttr, rhsAttr)) {
        auto const* lhsValue = lhsNormNode.value;
        auto const* rhsValue = rhsNormNode.value;

        if (byRange(filter, *lhsAttr, *lhsValue, lhsInclude, *rhsValue, rhsInclude)) {
          // successsfully parsed as range
          return true;
        }
      }
    }
  }

  // unable to create range
  return false;
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

  if (std::is_same<Filter, irs::And>::value && 2 == n && rangeFromBinaryAnd(filter, node)) {
    // range case
    return true;
  }

  if (filter) {
    filter = &filter->add<Filter>();
  }

  for (size_t i = 0; i < n; ++i) {
    auto const* valueNode = node.getMemberUnchecked(i);
    TRI_ASSERT(valueNode);

    if (!arangodb::iresearch::FilterFactory::filter(filter, *valueNode)) {
      return false;
    }
  }

  return true;
}

// EXISTS(<attribute>, <"type"|"analyzer">, <analyzer-name|"string"|"null"|"bool","numeric">)
bool fromFuncExists(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& args
) {
  auto const argc = args.numMembers();

  if (argc < 1 || argc > 3) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'EXISTS' AQL function: Invalid number of arguments passed (must be >= 1 and <= 3)";
    return false;
  }

  // 1st argument defines a field
  auto const* fieldArg = arangodb::iresearch::checkAttributeAccess(
    args.getMemberUnchecked(0)
  );

  if (!fieldArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'PHRASE' AQL function: 1st argument is invalid";
    return false;
  }

  bool prefix_match = true;
  std::string fieldName;

  if (filter) {
    fieldName = arangodb::iresearch::nameFromAttributeAccess(*fieldArg);
  }

  if (argc > 1) {
    // 2nd argument defines a type (if present)
    auto const typeArg = arangodb::iresearch::getNode(
      args, 1, arangodb::aql::NODE_TYPE_VALUE
    );

    if (!typeArg) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'EXISTS' AQL function: 2nd argument is invalid";
      return false;
    }

    irs::string_ref arg;

    if (!arangodb::iresearch::parseValue(arg, *typeArg)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'EXISTS' AQL function: Unable to parse 2nd argument as string";
      return false;
    }

    static irs::string_ref const TYPE = "type";
    static irs::string_ref const ANALYZER = "analyzer";

    bool const bAnalyzer = (ANALYZER == arg);

    if (!bAnalyzer && TYPE != arg) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "'EXISTS' AQL function: 2nd argument must be equal to '" << TYPE
          << "' or '" << ANALYZER << "', got '" << arg << "'";
      return false;
    }

    if (argc > 2) {

      // 3rd argument defines a value (if present)
      auto const valueArg = arangodb::iresearch::getNode(
        args, 2, arangodb::aql::NODE_TYPE_VALUE
      );

      if (!valueArg) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'EXISTS' AQL function: 3rd argument is invalid";
        return false;
      }

      if (!arangodb::iresearch::parseValue(arg, *valueArg)) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'EXISTS' AQL function: Unable to parse 3rd argument as string";
        return false;
      }

      if (bAnalyzer) {

        auto* analyzerFeature = arangodb::iresearch::getFeature<
          arangodb::iresearch::IResearchAnalyzerFeature
        >();

        if (!analyzerFeature) {
          LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'" << arangodb::iresearch::IResearchAnalyzerFeature::name()
                                                   << "' feature is not registered, unable to evaluate 'EXISTS' function";
          return false;
        }

        auto analyzer = analyzerFeature->get(arg);

        if (!analyzer) {
          LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'EXISTS' AQL function: Unable to lookup analyzer '" << arg << "'";
          return false;
        }

        if (filter) {
          arangodb::iresearch::kludge::mangleStringField(fieldName, analyzer);
        }
      } else {

        typedef void(*TypeHandler)(std::string&);

        static std::unordered_map<irs::hashed_string_ref, TypeHandler> const TypeHandlers {
          {
            irs::make_hashed_ref(irs::string_ref("string"), std::hash<irs::string_ref>()),
            [] (std::string& name) {
              arangodb::iresearch::kludge::mangleStringField(
                name,
                arangodb::iresearch::IResearchAnalyzerFeature::identity()
              );
            }
          },
          {
            irs::make_hashed_ref(irs::string_ref("numeric"), std::hash<irs::string_ref>()),
            [] (std::string& name) {
              arangodb::iresearch::kludge::mangleNumeric(name);
            }
          },
          {
            irs::make_hashed_ref(irs::string_ref("bool"), std::hash<irs::string_ref>()),
            [] (std::string& name) {
              arangodb::iresearch::kludge::mangleBool(name);
            }
          },
          {
            irs::make_hashed_ref(irs::string_ref("boolean"), std::hash<irs::string_ref>()),
            [] (std::string& name) {
              arangodb::iresearch::kludge::mangleBool(name);
            }
          },
          {
            irs::make_hashed_ref(irs::string_ref("null"), std::hash<irs::string_ref>()),
            [] (std::string& name) {
              arangodb::iresearch::kludge::mangleNull(name);
            }
          }
        };

        const auto it = TypeHandlers.find(
          irs::make_hashed_ref(arg, std::hash<irs::string_ref>())
        );

        if (TypeHandlers.end() == it) {
          LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
            << "'EXISTS' AQL function: 3rd argument must be equal to one of the following 'string', 'numeric', 'bool', 'null', got '"
            << arg << "'";

          return false;
        }

        if (filter) {
          it->second(fieldName);
        }
      }

      prefix_match = false;
    } else if (filter) {

      bAnalyzer
        ? arangodb::iresearch::kludge::mangleAnalyzer(fieldName)
        : arangodb::iresearch::kludge::mangleType(fieldName);

    }
  }

  if (filter) {
    auto& exists = filter->add<irs::by_column_existence>();
    exists.field(std::move(fieldName));
    exists.prefix_match(prefix_match);
  }

  return true;
}

// PHRASE(<attribute>, <value> [, <offset>, <value>, ...], <analyzer>)
bool fromFuncPhrase(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& args
) {
  auto* analyzerFeature = arangodb::iresearch::getFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();

  if (!analyzerFeature) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'" << arangodb::iresearch::IResearchAnalyzerFeature::name()
                                   << "' feature is not registered, unable to evaluate 'PHRASE' function";
    return false;
  }

  auto const argc = args.numMembers();

  if (argc < 2) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'PHRASE' AQL function: Invalid number of arguments passed (must be >= 2)";
    return false;
  }

  if (!(argc & 1)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'PHRASE' AQL function: Invalid number of arguments passed (must be an odd number)";
    return false;
  }

  // 1st argument defines a field
  auto const* fieldArg = arangodb::iresearch::checkAttributeAccess(
    args.getMemberUnchecked(0)
  );

  if (!fieldArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'PHRASE' AQL function: 1st argument is invalid";
    return false;
  }

  // 2nd argument defines a value
  auto const* valueArg = arangodb::iresearch::getNode(
    args, 1, arangodb::aql::NODE_TYPE_VALUE
  );

  if (!valueArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'PHRASE' AQL function: 2nd argument is invalid";
    return false;
  }

  irs::string_ref value;

  if (!arangodb::iresearch::parseValue(value, *valueArg)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'PHRASE' AQL function: Unable to parse 2nd argument as string";
    return false;
  }

  irs::string_ref analyzerName;
  auto const* analyzerArg = arangodb::iresearch::getNode(
    args, argc - 1, arangodb::aql::NODE_TYPE_VALUE
  );

  if (!analyzerArg || !arangodb::iresearch::parseValue(analyzerName, *analyzerArg)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'PHRASE' AQL function: Unable to parse analyzer value";
    return false;
  }

  auto pool = analyzerFeature->get(analyzerName);

  if (!pool) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'PHRASE' AQL function: Unable to load requested analyzer '" << analyzerName << "'";
    return false;
  }

  auto analyzer = pool->get(); // get analyzer from pool

  if (!analyzer) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'PHRASE' AQL function: Unable to instantiate analyzer '" << pool->name() << "'";
    return false;
  }

  irs::by_phrase* phrase = nullptr;

  if (filter) {
    phrase = &filter->add<irs::by_phrase>();

    auto name = arangodb::iresearch::nameFromAttributeAccess(*fieldArg);
    arangodb::iresearch::kludge::mangleStringField(name, pool);
    phrase->field(std::move(name));

    appendTerms(*phrase, value, *analyzer, 0);
  }

  decltype(fieldArg) offsetArg = nullptr;
  size_t offset;

  for (size_t idx = 2, end = argc - 1; idx < end; idx += 2) {
    offsetArg = arangodb::iresearch::getNode(
      args, idx, arangodb::aql::NODE_TYPE_VALUE
    );

    if (!offsetArg || !arangodb::iresearch::parseValue(offset, *offsetArg)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'PHRASE' AQL function: Unable to parse argument on position " << idx << " as an offset";
      return false;
    }

    valueArg = arangodb::iresearch::getNode(
      args, idx + 1, arangodb::aql::NODE_TYPE_VALUE
    );

    if (!valueArg || !arangodb::iresearch::parseValue(value, *valueArg)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'PHRASE' AQL function: Unable to parse argument on position " << idx + 1 << " as a value";
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

  if (argc < 2 || argc > 3) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'STARTS_WITH' AQL function: Invalid number of arguments passed (should be >= 2 and <= 3)";
    return false;
  }

  // 1st argument defines a field
  auto const* field = arangodb::iresearch::checkAttributeAccess(
    args.getMemberUnchecked(0)
  );

  if (!field) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'STARTS_WITH' AQL function: Unable to parse 1st argument as an attribute identifier";
    return false;
  }

  // 2nd argument defines a value
  auto const* prefix = arangodb::iresearch::getNode(
    args, 1, arangodb::aql::NODE_TYPE_VALUE
  );

  if (!prefix) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'STARTS_WITH' AQL function: Unable to parse 2nd argument as a prefix";
    return false;
  }

  // 3rd (optional) argument defines a number of scored terms
  decltype(prefix) scoringLimit = nullptr;

  if (argc > 2) {
    scoringLimit = arangodb::iresearch::getNode(
      args, 2, arangodb::aql::NODE_TYPE_VALUE
    );
  }

  return byPrefix(filter, *field, *prefix, scoringLimit);
}

std::map<irs::string_ref, ConvertionHandler> const FCallUserConvertionHandlers;

bool fromFCallUser(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL_USER == node.type);

  if (node.numMembers() != 1) {
    logMalformedNode(node.type);
    return false;
  }

  auto const* args = arangodb::iresearch::getNode(
    node, 0, arangodb::aql::NODE_TYPE_ARRAY
  );

  if (!args) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to parse user function arguments as an array'";
    return false; // invalid args
  }

  irs::string_ref name;

  if (!arangodb::iresearch::parseValue(name, node)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to parse user function name";
    return false;
  }

  auto const entry = FCallUserConvertionHandlers.find(name);

  if (entry == FCallUserConvertionHandlers.end()) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to find user function '" << name << "'";
    return false;
  }

  return entry->second(filter, *args);
}

std::map<std::string, ConvertionHandler> const FCallSystemConvertionHandlers{
  { "PHRASE", fromFuncPhrase },
  { "STARTS_WITH", fromFuncStartsWith },
  { "EXISTS", fromFuncExists }
  //  { "MIN_MATCH", fromFuncMinMatch } // add when AQL will support filters as the function parameters
};

bool fromFCall(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL == node.type);

  auto const* fn = static_cast<arangodb::aql::Function*>(node.getData());

  if (!fn || node.numMembers() != 1) {
    logMalformedNode(node.type);
    return false; // no function
  }

  auto const* args = arangodb::iresearch::getNode(node, 0, arangodb::aql::NODE_TYPE_ARRAY);

  if (!args) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to parse system function arguments as an array'";
    return false; // invalid args
  }

  auto const entry = FCallSystemConvertionHandlers.find(fn->name);

  if (entry == FCallSystemConvertionHandlers.end()) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to find system function '" << fn->name << "'";
    return false;
  }

  return entry->second(filter, *args);
}

bool fromFilter(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FILTER == node.type);

  if (node.numMembers() != 1) {
    logMalformedNode(node.type);
    return false; // wrong number of members
  }

  auto const* member = node.getMemberUnchecked(0);

  return member && arangodb::iresearch::FilterFactory::filter(filter, *member);
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
  switch (node.type) {
    case arangodb::aql::NODE_TYPE_FILTER: // FILTER
      return fromFilter(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_UNARY_NOT: // unary minus
      return fromNegation(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND: // logical and
      return fromGroup<irs::And>(filter, node);
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
    case arangodb::aql::NODE_TYPE_ARRAY:  // array
    case arangodb::aql::NODE_TYPE_OBJECT: // object
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

  auto const* typeName = arangodb::iresearch::getNodeTypeName(node.type);

  if (typeName) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to process Ast node of type '" << *typeName << "'";
  } else {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to process Ast node of type '" << node.type << "'";
  }

  return false; // unsupported node type
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
