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
#include "ExpressionFilter.h"
#include "ApplicationServerHelper.h"
#include "AqlHelper.h"

#include "Aql/Function.h"
#include "Aql/Ast.h"

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
  bool(irs::boolean_filter*, arangodb::iresearch::QueryContext const&, arangodb::aql::AstNode const&)
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

FORCE_INLINE void appendExpression(
    irs::boolean_filter& filter,
    arangodb::aql::AstNode const& node,
    arangodb::iresearch::QueryContext const& ctx
) {
  filter.add<arangodb::iresearch::ByExpression>().init(
    *ctx.plan, *ctx.ast, const_cast<arangodb::aql::AstNode&>(node)
  );
}

FORCE_INLINE void appendExpression(
    irs::boolean_filter& filter,
    std::shared_ptr<arangodb::aql::AstNode>&& node,
    arangodb::iresearch::QueryContext const& ctx
) {
  filter.add<arangodb::iresearch::ByExpression>().init(
    *ctx.plan, *ctx.ast, std::move(node)
  );
}

bool byTerm(
    irs::by_term* filter,
    arangodb::aql::AstNode const& attribute,
    arangodb::iresearch::ScopedAqlValue const& value,
    arangodb::iresearch::QueryContext const& ctx
) {
  std::string name;

  if (filter && !arangodb::iresearch::nameFromAttributeAccess(name, attribute, ctx)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "Failed to generate field name from node " << arangodb::aql::AstNode::toString(&attribute);
    return false;
  }

  switch (value.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_NULL:
      if (filter) {
        arangodb::iresearch::kludge::mangleNull(name);
        filter->field(std::move(name));
        filter->term(irs::null_token_stream::value_null());
      } return true;
    case arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL:
      if (filter) {
        arangodb::iresearch::kludge::mangleBool(name);
        filter->field(std::move(name));
        filter->term(irs::boolean_token_stream::value(value.getBoolean()));
      } return true;
    case arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE:
      if (filter) {
        double_t dblValue;

        if (!value.getDouble(dblValue)) {
          // something went wrong
          return false;
        }

        arangodb::iresearch::kludge::mangleNumeric(name);

        irs::numeric_token_stream stream;
        irs::term_attribute const* term = stream.attributes().get<irs::term_attribute>().get();
        TRI_ASSERT(term);
        stream.reset(dblValue);
        stream.next();

        filter->field(std::move(name));
        filter->term(term->value());
     } return true;
    case arangodb::iresearch::SCOPED_VALUE_TYPE_STRING:
      if (filter) {
        irs::string_ref strValue;

        if (!value.getString(strValue)) {
          // something went wrong
          return false;
        }

        arangodb::iresearch::kludge::mangleStringField(
           name,
           arangodb::iresearch::IResearchAnalyzerFeature::identity()
        );

        filter->field(std::move(name));
        filter->term(strValue);
    } return true;
    default:
      // unsupported value type
      return false;
  }
}

bool byTerm(
    irs::by_term* filter,
    arangodb::iresearch::NormalizedCmpNode const& node,
    arangodb::iresearch::QueryContext const& ctx
) {
  TRI_ASSERT(node.attribute && node.attribute->isDeterministic());
  TRI_ASSERT(node.value && node.value->isDeterministic());

  arangodb::iresearch::ScopedAqlValue value(*node.value);

  if (!value.isConstant()) {
    if (!filter) {
      // can't evaluate non constant filter before the execution
      return true;
    }

    if (!value.execute(ctx)) {
      // failed to execute expression
      return false;
    }
  }

  return byTerm(filter, *node.attribute, value, ctx);
}

bool byRange(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& attribute,
    arangodb::aql::Range const& rangeData,
    arangodb::iresearch::QueryContext const& ctx
) {
  TRI_ASSERT(attribute.isDeterministic());

  std::string name;

  if (filter && !arangodb::iresearch::nameFromAttributeAccess(name, attribute, ctx)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "Failed to generate field name from node " << arangodb::aql::AstNode::toString(&attribute);
    return false;
  }

  auto& range = filter->add<irs::by_granular_range>();

  arangodb::iresearch::kludge::mangleNumeric(name);
  range.field(std::move(name));

  irs::numeric_token_stream stream;

  // setup min bound
  stream.reset(static_cast<double_t>(rangeData._low));
  range.insert<irs::Bound::MIN>(stream);
  range.include<irs::Bound::MIN>(true);

  // setup max bound
  stream.reset(static_cast<double_t>(rangeData._high));
  range.insert<irs::Bound::MAX>(stream);
  range.include<irs::Bound::MAX>(true);

  return true;
}

bool byRange(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& attributeNode,
    arangodb::aql::AstNode const& minValueNode,
    bool const minInclude,
    arangodb::aql::AstNode const& maxValueNode,
    bool const maxInclude,
    arangodb::iresearch::QueryContext const& ctx
) {
  TRI_ASSERT(attributeNode.isDeterministic());
  TRI_ASSERT(minValueNode.isDeterministic());
  TRI_ASSERT(maxValueNode.isDeterministic());

  arangodb::iresearch::ScopedAqlValue min(minValueNode);

  if (!min.isConstant()) {
    if (!filter) {
      // can't evaluate non constant filter before the execution
      return true;
    }

    if (!min.execute(ctx)) {
      // failed to execute expression
      return false;
    }
  }

  arangodb::iresearch::ScopedAqlValue max(maxValueNode);

  if (!max.isConstant()) {
    if (!filter) {
      // can't evaluate non constant filter before the execution
      return true;
    }

    if (!max.execute(ctx)) {
      // failed to execute expression
      return false;
    }
  }

  if (min.type() != max.type()) {
    // type mismatch
    return false;
  }

  std::string name;

  if (filter && !arangodb::iresearch::nameFromAttributeAccess(name, attributeNode, ctx)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "Failed to generate field name from node " << arangodb::aql::AstNode::toString(&attributeNode);
    return false;
  }

  switch (min.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_NULL: {
      if (filter) {
        arangodb::iresearch::kludge::mangleNull(name);

        auto& range = filter->add<irs::by_range>();
        range.field(std::move(name));
        range.term<irs::Bound::MIN>(irs::null_token_stream::value_null());
        range.include<irs::Bound::MIN>(minInclude);;
        range.term<irs::Bound::MAX>(irs::null_token_stream::value_null());
        range.include<irs::Bound::MAX>(maxInclude);;
      }

      return true;
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL: {
      if (filter) {
        arangodb::iresearch::kludge::mangleBool(name);

        auto& range = filter->add<irs::by_range>();
        range.field(std::move(name));
        range.term<irs::Bound::MIN>(
          irs::boolean_token_stream::value(min.getBoolean())
        );
        range.include<irs::Bound::MIN>(minInclude);
        range.term<irs::Bound::MAX>(
          irs::boolean_token_stream::value(max.getBoolean())
        );
        range.include<irs::Bound::MAX>(maxInclude);
      }

      return true;
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE: {
      if (filter) {
        double_t minDblValue, maxDblValue;

        if (!min.getDouble(minDblValue) || !max.getDouble(maxDblValue)) {
          // can't parse value as double
          return false;
        }

        auto& range = filter->add<irs::by_granular_range>();

        arangodb::iresearch::kludge::mangleNumeric(name);
        range.field(std::move(name));

        irs::numeric_token_stream stream;

        // setup min bound
        stream.reset(minDblValue);
        range.insert<irs::Bound::MIN>(stream);
        range.include<irs::Bound::MIN>(minInclude);

        // setup max bound
        stream.reset(maxDblValue);
        range.insert<irs::Bound::MAX>(stream);
        range.include<irs::Bound::MAX>(maxInclude);
      }

      return true;
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_STRING: {
      if (filter) {
        irs::string_ref minStrValue, maxStrValue;

        if (!min.getString(minStrValue) || !max.getString(maxStrValue)) {
          // failed to get string value
          return false;
        }

        auto& range = filter->add<irs::by_range>();

        arangodb::iresearch::kludge::mangleStringField(
          name,
          arangodb::iresearch::IResearchAnalyzerFeature::identity()
        );
        range.field(std::move(name));

        range.term<irs::Bound::MIN>(minStrValue);
        range.include<irs::Bound::MIN>(minInclude);
        range.term<irs::Bound::MAX>(maxStrValue);
        range.include<irs::Bound::MAX>(maxInclude);
      }

      return true;
    }
    default:
      // wrong value type
      return false;
  }
}

template<irs::Bound Bound>
bool byRange(
    irs::boolean_filter* filter,
    arangodb::iresearch::NormalizedCmpNode const& node,
    bool const incl,
    arangodb::iresearch::QueryContext const& ctx
) {
  TRI_ASSERT(node.attribute && node.attribute->isDeterministic());
  TRI_ASSERT(node.value && node.value->isDeterministic());

  arangodb::iresearch::ScopedAqlValue value(*node.value);

  if (!value.isConstant()) {
    if (!filter) {
      // can't evaluate non constant filter before the execution
      return true;
    }

    if (!value.execute(ctx)) {
      // con't execute expression
      return false;
    }
  }

  std::string name;

  if (filter && !arangodb::iresearch::nameFromAttributeAccess(name, *node.attribute, ctx)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "Failed to generate field name from node " << arangodb::aql::AstNode::toString(node.attribute);
    return false;
  }

  switch (value.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_NULL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        arangodb::iresearch::kludge::mangleNull(name);
        range.field(std::move(name));
        range.term<Bound>(irs::null_token_stream::value_null());
        range.include<Bound>(incl);
      }

      return true;
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        arangodb::iresearch::kludge::mangleBool(name);
        range.field(std::move(name));
        range.term<Bound>(
          irs::boolean_token_stream::value(value.getBoolean())
        );
        range.include<Bound>(incl);
      }

      return true;
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE: {
      if (filter) {
        double_t dblValue;

        if (!value.getDouble(dblValue)) {
          // can't parse as double
          return false;
        }

        auto& range = filter->add<irs::by_granular_range>();
        irs::numeric_token_stream stream;

        arangodb::iresearch::kludge::mangleNumeric(name);
        range.field(std::move(name));

        stream.reset(dblValue);
        range.insert<Bound>(stream);
        range.include<Bound>(incl);
      }

      return true;
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_STRING: {
      if (filter) {
        irs::string_ref strValue;

        if (!value.getString(strValue)) {
          // can't parse as string
          return false;
        }

        auto& range = filter->add<irs::by_range>();

        arangodb::iresearch::kludge::mangleStringField(
          name,
          arangodb::iresearch::IResearchAnalyzerFeature::identity()
        );
        range.field(std::move(name));
        range.term<Bound>(strValue);
        range.include<Bound>(incl);
      }

      return true;
    }
    default:
      // wrong value type
      return false;
  }
}

bool fromExpression(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
    std::shared_ptr<arangodb::aql::AstNode> && node
) {
  if (!filter) {
    return true;
  }

  // non-deterministic condition or self-referenced variable
  if (!node->isDeterministic() || arangodb::iresearch::findReference(*node, *ctx.ref)) {
    // not supported by IResearch, but could be handled by ArangoDB
    appendExpression(*filter, std::move(node), ctx);
    return true;
  }

  bool result;

  if (node->isConstant()) {
    result = node->isTrue();
  } else { // deterministic expression
    arangodb::iresearch::ScopedAqlValue value(*node);

    if (!value.execute(ctx)) {
      // can't execute expression
      return false;
    }

    result = value.getBoolean();
  }

  if (result) {
    filter->add<irs::all>();
  } else {
    filter->add<irs::empty>();
  }

  return true;
}

bool fromExpression(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& node
) {
  if (!filter) {
    return true;
  }

  // non-deterministic condition or self-referenced variable
  if (!node.isDeterministic() || arangodb::iresearch::findReference(node, *ctx.ref)) {
    // not supported by IResearch, but could be handled by ArangoDB
    appendExpression(*filter, node, ctx);
    return true;
  }

  bool result;

  if (node.isConstant()) {
    result = node.isTrue();
  } else { // deterministic expression
    arangodb::iresearch::ScopedAqlValue value(node);

    if (!value.execute(ctx)) {
      // can't execute expression
      return false;
    }

    result = value.getBoolean();
  }

  if (result) {
    filter->add<irs::all>();
  } else {
    filter->add<irs::empty>();
  }

  return true;
}

bool fromInterval(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == node.type
  );

  arangodb::iresearch::NormalizedCmpNode normNode;

  if (!arangodb::iresearch::normalizeCmpNode(node, *ctx.ref, normNode)) {
    return fromExpression(filter, ctx, node);
  }

  bool const incl = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == normNode.cmp
                 || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == normNode.cmp;

  bool const min = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == normNode.cmp
                || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == normNode.cmp;

  return min ? byRange<irs::Bound::MIN>(filter, normNode, incl, ctx)
             : byRange<irs::Bound::MAX>(filter, normNode, incl, ctx);
}

bool fromBinaryEq(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type
  );

  arangodb::iresearch::NormalizedCmpNode normalized;

  if (!arangodb::iresearch::normalizeCmpNode(node, *ctx.ref, normalized)) {
    return fromExpression(filter, ctx, node);
  }

  irs::by_term* termFilter = nullptr;

  if (filter) {
    termFilter = &(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type
      ? filter->add<irs::Not>().filter<irs::by_term>()
      : filter->add<irs::by_term>());
  }

  return byTerm(termFilter, normalized, ctx);
}

bool fromRange(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
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

bool fromInArray(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type
  );

  // `attributeNode` IN `valueNode`
  auto const* attributeNode = node.getMemberUnchecked(0);
  TRI_ASSERT(attributeNode);
  auto const* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(valueNode && arangodb::aql::NODE_TYPE_ARRAY == valueNode->type);

  if (!attributeNode->isDeterministic()) {
    // not supported by IResearch, but could be handled by ArangoDB
    return fromExpression(filter, ctx, node);
  }

  size_t const n = valueNode->numMembers();

  if (!arangodb::iresearch::checkAttributeAccess(attributeNode, *ctx.ref)) {
    // no attribute access specified in attribute node, try to
    // find it in value node

    bool attributeAccessFound = false;
    for (size_t i = 0; i < n; ++i) {
      attributeAccessFound |= (nullptr != arangodb::iresearch::checkAttributeAccess(
        valueNode->getMemberUnchecked(i), *ctx.ref
      ));
    }

    if (!attributeAccessFound) {
      return fromExpression(filter, ctx, node);
    }
  }

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

  arangodb::iresearch::NormalizedCmpNode normalized;
  arangodb::aql::AstNode toNormalize(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ);
  toNormalize.reserve(2);

  // FIXME better to rewrite expression the following way but there is no place to
  // store created `AstNode`
  // d.a IN [1,RAND(),'3'+RAND()] -> (d.a == 1) OR d.a IN [RAND(),'3'+RAND()]

  for (size_t i = 0; i < n; ++i) {
    auto const* member = valueNode->getMemberUnchecked(i);
    TRI_ASSERT(member);

    // edit in place for now; TODO change so we can replace instead
    TEMPORARILY_UNLOCK_NODE(&toNormalize);
    toNormalize.clearMembers();
    toNormalize.addMember(attributeNode);
    toNormalize.addMember(member);
    toNormalize.flags = member->flags; // attributeNode is deterministic here

    if (!arangodb::iresearch::normalizeCmpNode(toNormalize, *ctx.ref, normalized)) {
      if (!filter) {
        // can't evaluate non constant filter before the execution
        return true;
      }

      // use std::shared_ptr since AstNode is not copyable/moveable
      auto exprNode = std::make_shared<arangodb::aql::AstNode>(
        arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ
      );
      exprNode->reserve(2);
      exprNode->addMember(attributeNode);
      exprNode->addMember(member);

      // not supported by IResearch, but could be handled by ArangoDB
      if (!fromExpression(filter, ctx, std::move(exprNode))) {
        return false;
      }
    } else {
      auto* termFilter = filter
          ? &filter->add<irs::by_term>()
          : nullptr;

      if (!byTerm(termFilter, normalized, ctx)) {
        return false;
      }
    }
  }

  return true;
}

bool fromIn(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
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

  auto const* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(valueNode);

  if (arangodb::aql::NODE_TYPE_ARRAY == valueNode->type) {
    return fromInArray(filter, ctx, node);
  }

  auto* attributeNode = node.getMemberUnchecked(0);
  TRI_ASSERT(attributeNode);

  if (!node.isDeterministic()
      || !arangodb::iresearch::checkAttributeAccess(attributeNode, *ctx.ref)
      || arangodb::iresearch::findReference(*valueNode, *ctx.ref)) {
    return fromExpression(filter, ctx, node);
  }

  if (!filter) {
    // can't evaluate non constant filter before the execution
    return true;
  }

  if (arangodb::aql::NODE_TYPE_RANGE == valueNode->type) {
    arangodb::iresearch::ScopedAqlValue value(*valueNode);

    if (!value.execute(ctx)) {
      // con't execute expression
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "Unable to extract value from 'IN' operator";
      return false;
    }

    // range
    auto const* range = value.getRange();

    if (!range) {
      return false;
    }

    if (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
      // handle negation
      filter = &filter->add<irs::Not>().filter<irs::Or>();
    }

    return byRange(filter, *attributeNode, *range, ctx);
  }

  arangodb::iresearch::ScopedAqlValue value(*valueNode);

  if (!value.execute(ctx)) {
    // con't execute expression
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "Unable to extract value from 'IN' operator";
    return false;
  }

  switch (value.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_ARRAY: {
      size_t const n = value.size();

      if (!n) {
        if (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
          filter->add<irs::all>(); // not in [] means 'all'
        } else {
          filter->add<irs::empty>();
        }

        // nothing to do more
        return true;
      }

      filter = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type
          ? &static_cast<irs::boolean_filter&>(filter->add<irs::Not>().filter<irs::And>())
          : &static_cast<irs::boolean_filter&>(filter->add<irs::Or>());

      for (size_t i = 0; i < n; ++i) {
        if (!byTerm(&filter->add<irs::by_term>(), *attributeNode, value.at(i), ctx)) {
          // failed to create a filter
          return false;
        }
      }

      return true;
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_RANGE: {
      // range
      auto const* range = value.getRange();

      if (!range) {
        return false;
      }

      if (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
        // handle negation
        filter = &filter->add<irs::Not>().filter<irs::Or>();
      }

      return byRange(filter, *attributeNode, *range, ctx);
    }
    default:
      break;
  }

  // wrong value node type
  return false;
}

bool fromNegation(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
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

  return arangodb::iresearch::FilterFactory::filter(filter, ctx, *member);
}

bool rangeFromBinaryAnd(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
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

  if (arangodb::iresearch::normalizeCmpNode(*lhsNode, *ctx.ref, lhsNormNode)
      && arangodb::iresearch::normalizeCmpNode(*rhsNode, *ctx.ref, rhsNormNode)) {
    bool const lhsInclude = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == lhsNormNode.cmp;
    bool const rhsInclude = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == rhsNormNode.cmp;

    if ((lhsInclude || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == lhsNormNode.cmp)
         && (rhsInclude || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == rhsNormNode.cmp)) {

      auto const* lhsAttr = lhsNormNode.attribute;
      auto const* rhsAttr = rhsNormNode.attribute;

      if (arangodb::iresearch::attributeAccessEqual(lhsAttr, rhsAttr, filter ? &ctx : nullptr)) {
        auto const* lhsValue = lhsNormNode.value;
        auto const* rhsValue = rhsNormNode.value;

        if (byRange(filter, *lhsAttr, *lhsValue, lhsInclude, *rhsValue, rhsInclude, ctx)) {
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
    arangodb::iresearch::QueryContext const& ctx,
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

  // Note: cannot optimize for single member in AND/OR since 'a OR NOT b' translates to 'a OR (OR NOT b)'

  if (std::is_same<Filter, irs::And>::value && 2 == n && rangeFromBinaryAnd(filter, ctx, node)) {
    // range case
    return true;
  }

  if (filter) {
    filter = &filter->add<Filter>();
  }

  for (size_t i = 0; i < n; ++i) {
    auto const* valueNode = node.getMemberUnchecked(i);
    TRI_ASSERT(valueNode);

    if (!arangodb::iresearch::FilterFactory::filter(filter, ctx, *valueNode)) {
      return false;
    }
  }

  return true;
}

// EXISTS(<attribute>, <"type"|"analyzer">, <analyzer-name|"string"|"null"|"bool","numeric">)
bool fromFuncExists(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& args
) {
  TRI_ASSERT(args.isDeterministic());
  auto const argc = args.numMembers();

  if (argc < 1 || argc > 3) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "'EXISTS' AQL function: Invalid number of arguments passed (must be >= 1 and <= 3)";
    return false;
  }

  // 1st argument defines a field
  auto const* fieldArg = arangodb::iresearch::checkAttributeAccess(
    args.getMemberUnchecked(0), *ctx.ref
  );

  if (!fieldArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "'PHRASE' AQL function: 1st argument is invalid";
    return false;
  }

  bool prefix_match = true;
  std::string fieldName;

  if (filter && !arangodb::iresearch::nameFromAttributeAccess(fieldName, *fieldArg, ctx)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "'PHRASE' AQL function: Failed to generate field name from the 1st argument";
    return false;
  }

  if (argc > 1) {
    // 2nd argument defines a type (if present)
    auto const typeArg = args.getMemberUnchecked(1);

    if (!typeArg) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'EXISTS' AQL function: 2nd argument is invalid";
    }

    irs::string_ref arg;
    bool bAnalyzer = false;
    arangodb::iresearch::ScopedAqlValue type(*typeArg);

    if (filter || type.isConstant()) {
      if (!type.execute(ctx)) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
            << "'EXISTS' AQL function: Failed to evaluate 2nd argument";
        return false;
      }

      if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != type.type()) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
            << "'EXISTS' AQL function: 2nd argument has invalid type '" << type.type()
            << "' (string expected)";
        return false;
      }

      if (!type.getString(arg)) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
            << "'EXISTS' AQL function: Failed to parse 2nd argument as string";
        return false;
      }

      static irs::string_ref const TYPE = "type";
      static irs::string_ref const ANALYZER = "analyzer";

      bAnalyzer = (ANALYZER == arg);

      if (!bAnalyzer && TYPE != arg) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
            << "'EXISTS' AQL function: 2nd argument must be equal to '" << TYPE
            << "' or '" << ANALYZER << "', got '" << arg << "'";
        return false;
      }
    }

    if (argc > 2) {

      // 3rd argument defines a value (if present)
      auto const analyzerArg= args.getMemberUnchecked(2);

      if (!analyzerArg) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "'EXISTS' AQL function: 3rd argument is invalid";
      }

      arangodb::iresearch::ScopedAqlValue analyzerId(*analyzerArg);

      if (filter || analyzerId.isConstant()) {

        if (!analyzerId.execute(ctx)) {
          LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
            << "'EXISTS' AQL function: Failed to evaluate 3rd argument";
          return false;
        }

        if (!analyzerId.getString(arg)) {
          LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
            << "'EXISTS' AQL function: Unable to parse 3rd argument as string";
          return false;
        }

        if (bAnalyzer) {

          auto* analyzerFeature = arangodb::iresearch::getFeature<
            arangodb::iresearch::IResearchAnalyzerFeature
          >();

          if (!analyzerFeature) {
            LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
              << "'" << arangodb::iresearch::IResearchAnalyzerFeature::name()
              << "' feature is not registered, unable to evaluate 'EXISTS' function";
            return false;
          }

          auto analyzer = analyzerFeature->get(arg);

          if (!analyzer) {
            LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
              << "'EXISTS' AQL function: Unable to lookup analyzer '" << arg << "'";
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
// PHRASE(<attribute>, '[' <value> [, <offset>, <value>, ...] ']', <analyzer>)
bool fromFuncPhrase(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& args
) {
  TRI_ASSERT(args.isDeterministic());

  auto* analyzerFeature = arangodb::iresearch::getFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();

  if (!analyzerFeature) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "'" << arangodb::iresearch::IResearchAnalyzerFeature::name()
      << "' feature is not registered, unable to evaluate 'PHRASE' function";
    return false;
  }

  auto const argc = args.numMembers();

  if (argc < 2) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "'PHRASE' AQL function: Invalid number of arguments passed (must be >= 2)";
    return false;
  }

  if (!(argc & 1)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "'PHRASE' AQL function: Invalid number of arguments passed (must be an odd number)";
    return false;
  }

  // ...........................................................................
  // 1st argument defines a field
  // ...........................................................................

  auto const* fieldArg = arangodb::iresearch::checkAttributeAccess(
    args.getMemberUnchecked(0), *ctx.ref
  );

  if (!fieldArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "'PHRASE' AQL function: 1st argument is invalid";
    return false;
  }

  // ...........................................................................
  // last argument defines the analyzer to use
  // ...........................................................................

  auto const* analyzerArg = args.getMemberUnchecked(argc - 1);

  if (!analyzerArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "'PHRASE' AQL function: Unable to parse analyzer value";
    return false;
  }

  arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr pool;
  irs::analysis::analyzer::ptr analyzer;

  arangodb::iresearch::ScopedAqlValue analyzerValue(*analyzerArg);

  if (filter || analyzerValue.isConstant()) {
    if (!analyzerValue.execute(ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "'PHRASE' AQL function: Failed to evaluate 'analyzer' argument";
      return false;
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != analyzerValue.type()) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "'PHRASE' AQL function: 'analyzer' argument has invalid type '" << analyzerValue.type()
          << "' (string expected)";
      return false;
    }

    irs::string_ref analyzerName;

    if (!analyzerValue.getString(analyzerName)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "'PHRASE' AQL function: Unable to parse 'analyzer' argument as string";
      return false;
    }

    pool = analyzerFeature->get(analyzerName);

    if (!pool) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'PHRASE' AQL function: Unable to load requested analyzer '" << analyzerName << "'";
      return false;
    }

    analyzer = pool->get(); // get analyzer from pool

    if (!analyzer) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'PHRASE' AQL function: Unable to instantiate analyzer '" << pool->name() << "'";
      return false;
    }
  }

  // ...........................................................................
  // 2nd argument defines a value
  // ...........................................................................

  auto const* valueArg = args.getMemberUnchecked(1);

  if (!valueArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "'PHRASE' AQL function: 2nd argument is invalid";
    return false;
  }

  auto* valueArgs = &args;
  size_t valueArgsBegin = 1;
  size_t valueArgsEnd = argc - 1;

  if (valueArg->isArray()) {
    valueArgs = valueArg;
    valueArgsBegin = 0;
    valueArgsEnd = valueArg->numMembers();

    if (!(valueArgsEnd & 1)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'PHRASE' AQL function: 2nd argument has an invalid number of members (must be an odd number)";
      return false;
    }

    valueArg = valueArgs->getMemberUnchecked(valueArgsBegin);

    if (!valueArg) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'PHRASE' AQL function: 2nd argument has an invalid member at offset: " << valueArg;
      return false;
    }
  }

  irs::string_ref value;
  arangodb::iresearch::ScopedAqlValue inputValue(*valueArg);

  if (filter || inputValue.isConstant()) {
    if (!inputValue.execute(ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "'PHRASE' AQL function: Failed to evaluate 2nd argument";
      return false;
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != inputValue.type()) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "'PHRASE' AQL function: 2nd argument has invalid type '" << inputValue.type()
          << "' (string expected)";
      return false;
    }

    if (!inputValue.getString(value)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "'PHRASE' AQL function: Unable to parse 2nd argument as string";
      return false;
    }
  }

  irs::by_phrase* phrase = nullptr;

  if (filter) {
    std::string name;

    if (!arangodb::iresearch::nameFromAttributeAccess(name, *fieldArg, ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'PHRASE' AQL function: Failed to generate field name from the 1st argument";
      return false;
    }

    arangodb::iresearch::kludge::mangleStringField(name, pool);

    phrase = &filter->add<irs::by_phrase>();
    phrase->field(std::move(name));

    TRI_ASSERT(analyzer);
    appendTerms(*phrase, value, *analyzer, 0);
  }

  decltype(fieldArg) offsetArg = nullptr;
  size_t offset = 0;

  for (size_t idx = valueArgsBegin + 1, end = valueArgsEnd; idx < end; idx += 2) {
    offsetArg = valueArgs->getMemberUnchecked(idx);

    if (!offsetArg) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'PHRASE' AQL function: Unable to parse argument on position " << idx << " as an offset";
      return false;
    }

    valueArg = valueArgs->getMemberUnchecked(idx + 1);

    if (!valueArg) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'PHRASE' AQL function: Unable to parse argument on position " << idx + 1 << " as a value";
      return false;
    }

    arangodb::iresearch::ScopedAqlValue offsetValue(*offsetArg);

    if (filter || offsetValue.isConstant()) {
      if (!offsetValue.execute(ctx) || arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE != offsetValue.type()) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
            << "'PHRASE' AQL function: Unable to parse argument on position " << idx << " as an offset";
        return false;
      }

      offset = static_cast<uint64_t>(offsetValue.getInt64());
    }

    arangodb::iresearch::ScopedAqlValue inputValue(*valueArg);

    if (filter || inputValue.isConstant()) {
      if (!inputValue.execute(ctx)
          || arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != inputValue.type()
          || !inputValue.getString(value)) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "'PHRASE' AQL function: Unable to parse argument on position " << idx + 1 << " as a value";
        return false;
      }
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
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& args
) {
  TRI_ASSERT(args.isDeterministic());
  auto const argc = args.numMembers();

  if (argc < 2 || argc > 3) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "'STARTS_WITH' AQL function: Invalid number of arguments passed (should be >= 2 and <= 3)";
    return false;
  }

  // 1st argument defines a field
  auto const* field = arangodb::iresearch::checkAttributeAccess(
    args.getMemberUnchecked(0), *ctx.ref
  );

  if (!field) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "'STARTS_WITH' AQL function: Unable to parse 1st argument as an attribute identifier";
    return false;
  }

  // 2nd argument defines a value
  auto const* prefixArg = args.getMemberUnchecked(1);

  if (!prefixArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "'STARTS_WITH' AQL function: 2nd argument is invalid";
    return false;
  }

  irs::string_ref prefix;
  size_t scoringLimit = 128; // FIXME make configurable

  arangodb::iresearch::ScopedAqlValue prefixValue(*prefixArg);

  if (filter || prefixValue.isConstant()) {
    if (!prefixValue.execute(ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "'STARTS_WITH' AQL function: Failed to evaluate 2nd argument";
      return false;
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != prefixValue.type()) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "'STARTS_WITH' AQL function: 2nd argument has invalid type '" << prefixValue.type()
          << "' (string expected)";
      return false;
    }

    if (!prefixValue.getString(prefix)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "'STARTS_WITH' AQL function: Unable to parse 2nd argument as string";
      return false;
    }
  }

  if (argc > 2) {
    // 3rd (optional) argument defines a number of scored terms
    auto const* scoringLimitArg = args.getMemberUnchecked(2);

    if (!scoringLimitArg) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'STARTS_WITH' AQL function: 3rd argument is invalid";
      return false;
    }

    arangodb::iresearch::ScopedAqlValue scoringLimitValue(*scoringLimitArg);

    if (filter || scoringLimitValue.isConstant()) {
      if (!scoringLimitValue.execute(ctx)) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
            << "'STARTS_WITH' AQL function: Failed to evaluate 3rd argument";
        return false;
      }

      if (arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE != scoringLimitValue.type()) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
            << "'STARTS_WITH' AQL function: 3rd argument has invalid type '" << scoringLimitValue.type()
            << "' (numeric expected)";
        return false;
      }

      scoringLimit = static_cast<size_t>(scoringLimitValue.getInt64());
    }
  }

  if (filter) {
    std::string name;

    if (!arangodb::iresearch::nameFromAttributeAccess(name, *field, ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'STARTS_WITH' AQL function: Failed to generate field name from the 1st argument";
      return false;
    }

    arangodb::iresearch::kludge::mangleStringField(
      name,
      arangodb::iresearch::IResearchAnalyzerFeature::identity()
    );

    auto& prefixFilter = filter->add<irs::by_prefix>();
    prefixFilter.scored_terms_limit(scoringLimit);
    prefixFilter.field(std::move(name));
    prefixFilter.term(prefix);
  }

  return true;
}

std::map<irs::string_ref, ConvertionHandler> const FCallUserConvertionHandlers;

bool fromFCallUser(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
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
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "Unable to parse user function arguments as an array'";
    return false; // invalid args
  }

  irs::string_ref name;

  if (!arangodb::iresearch::parseValue(name, node)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "Unable to parse user function name";

    return false;
  }

  auto const entry = FCallUserConvertionHandlers.find(name);

  if (entry == FCallUserConvertionHandlers.end()) {
    return fromExpression(filter, ctx, node);
  }

  if (!args->isDeterministic()) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "Unable to handle non-deterministic function '" << name << "' arguments";

    return false; // nondeterministic
  }

  return entry->second(filter, ctx, *args);
}

std::map<std::string, ConvertionHandler> const FCallSystemConvertionHandlers{
  { "PHRASE", fromFuncPhrase },
  { "STARTS_WITH", fromFuncStartsWith },
  { "EXISTS", fromFuncExists }
  //  { "MIN_MATCH", fromFuncMinMatch } // add when AQL will support filters as the function parameters
};

bool fromFCall(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL == node.type);

  auto const* fn = static_cast<arangodb::aql::Function*>(node.getData());

  if (!fn || node.numMembers() != 1) {
    logMalformedNode(node.type);
    return false; // no function
  }

  auto const entry = FCallSystemConvertionHandlers.find(fn->name);

  if (entry == FCallSystemConvertionHandlers.end()) {
    return fromExpression(filter, ctx, node);
  }

  auto const* args = arangodb::iresearch::getNode(node, 0, arangodb::aql::NODE_TYPE_ARRAY);

  if (!args) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "Unable to parse arguments of system function '" << fn->name << "' as an array'";
    return false; // invalid args
  }

  if (!args->isDeterministic()) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "Unable to handle non-deterministic function '" << fn->name << "' arguments";

    return false; // nondeterministic
  }

  return entry->second(filter, ctx, *args);
}

bool fromFilter(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FILTER == node.type);

  if (node.numMembers() != 1) {
    logMalformedNode(node.type);
    return false; // wrong number of members
  }

  auto const* member = node.getMemberUnchecked(0);

  return member && arangodb::iresearch::FilterFactory::filter(filter, ctx, *member);
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

  FilterFactory::filter(static_cast<irs::And&>(*filter), cid, rid);

  return std::move(filter);
}

/*static*/ irs::filter& FilterFactory::filter(
    irs::boolean_filter& buf,
    TRI_voc_cid_t cid,
    TRI_voc_rid_t rid
) {
  // filter matching on cid and rid
  auto& filter = buf.add<irs::And>();

  // filter matching on cid
  filter.add<irs::by_term>()
    .field(DocumentPrimaryKey::CID()) // set field
    .term(DocumentPrimaryKey::encode(cid)); // set value

  // filter matching on rid
  filter.add<irs::by_term>()
    .field(DocumentPrimaryKey::RID()) // set field
    .term(DocumentPrimaryKey::encode(rid)); // set value

  return filter;
}

/*static*/ bool FilterFactory::filter(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& node
) {
  switch (node.type) {
    case arangodb::aql::NODE_TYPE_FILTER: // FILTER
      return fromFilter(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_VARIABLE: // variable
      return fromExpression(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_UNARY_NOT: // unary minus
      return fromNegation(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND: // logical and
      return fromGroup<irs::And>(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_OR: // logical or
      return fromGroup<irs::Or>(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ: // compare ==
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE: // compare !=
      return fromBinaryEq(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT: // compare <
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE: // compare <=
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT: // compare >
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE: // compare >=
      return fromInterval(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN: // compare in
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN: // compare not in
      return fromIn(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_TERNARY: // ternary
    case arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS: // attribute access
    case arangodb::aql::NODE_TYPE_VALUE : // value
    case arangodb::aql::NODE_TYPE_ARRAY:  // array
    case arangodb::aql::NODE_TYPE_OBJECT: // object
    case arangodb::aql::NODE_TYPE_REFERENCE: // reference
    case arangodb::aql::NODE_TYPE_PARAMETER: // bind parameter
      return fromExpression(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_FCALL: // function call
      return fromFCall(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_FCALL_USER: // user function call
      return fromFCallUser(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_RANGE: // range
      return fromRange(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND: // n-ary and
      return fromGroup<irs::And>(filter, ctx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_NARY_OR: // n-ary or
      return fromGroup<irs::Or>(filter, ctx, node);
    default:
      return fromExpression(filter, ctx, node);
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
