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

// otherwise define conflict between 3rdParty\date\include\date\date.h and 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
  #include "date/date.h"
  #undef NOEXCEPT
#endif

#include <cctype>
#include <type_traits>

#include "search/all_filter.hpp"
#include "search/boolean_filter.hpp"
#include "search/term_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/granular_range_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/phrase_filter.hpp"

#include "AqlHelper.h"
#include "ExpressionFilter.h"
#include "IResearchFeature.h"
#include "IResearchAnalyzerFeature.h"
#include "IResearchCommon.h"
#include "IResearchFilterFactory.h"
#include "IResearchDocument.h"
#include "IResearchKludge.h"
#include "IResearchPrimaryKeyFilter.h"
#include "Aql/Function.h"
#include "Aql/Ast.h"
#include "Logger/LogMacros.h"

using namespace arangodb::iresearch;

namespace {

struct FilterContext {
  FilterContext(
      IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer,
      irs::boost::boost_t boost
  ) noexcept
    : analyzer(analyzer), boost(boost) {
    TRI_ASSERT(analyzer);
  }

  FilterContext(FilterContext const&) = default;
  FilterContext& operator=(FilterContext const&) = delete;

  // need shared_ptr since pool could be deleted from the feature
  IResearchAnalyzerFeature::AnalyzerPool::ptr const analyzer;
  irs::boost::boost_t boost;
}; // FilterContext

typedef std::function<bool(
  irs::boolean_filter*,
  QueryContext const&,
  FilterContext const&,
  arangodb::aql::AstNode const&
)> ConvertionHandler;

// forward declaration
bool filter(
  irs::boolean_filter* filter,
  QueryContext const& queryCtx,
  FilterContext const& filterCtx,
  arangodb::aql::AstNode const& node
);

////////////////////////////////////////////////////////////////////////////////
/// @brief logs message about malformed AstNode with the specified type
////////////////////////////////////////////////////////////////////////////////
void logMalformedNode(arangodb::aql::AstNodeType type) {
  auto const* typeName = arangodb::iresearch::getNodeTypeName(type);

  if (typeName) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Can't process malformed AstNode of type '" << *typeName << "'";
  } else {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Can't process malformed AstNode of type '" << type << "'";
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

FORCE_INLINE void appendExpression(
    irs::boolean_filter& filter,
    arangodb::aql::AstNode const& node,
    QueryContext const& ctx,
    FilterContext const& filterCtx
) {
  auto& exprFilter = filter.add<arangodb::iresearch::ByExpression>();
  exprFilter.init(*ctx.plan, *ctx.ast, const_cast<arangodb::aql::AstNode&>(node));
  exprFilter.boost(filterCtx.boost);
}

FORCE_INLINE void appendExpression(
    irs::boolean_filter& filter,
    std::shared_ptr<arangodb::aql::AstNode>&& node,
    QueryContext const& ctx,
    FilterContext const& filterCtx
) {
  auto& exprFilter = filter.add<arangodb::iresearch::ByExpression>();
  exprFilter.init(*ctx.plan, *ctx.ast, std::move(node));
  exprFilter.boost(filterCtx.boost);
}

IResearchAnalyzerFeature::AnalyzerPool::ptr extractAnalyzerFromArg(
    irs::boolean_filter const* filter,
    arangodb::aql::AstNode const* analyzerArg,
    QueryContext const& ctx,
    size_t argIdx,
    irs::string_ref const& functionName
) {
  if (!analyzerArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'" << functionName << "' AQL function: " << argIdx << " argument is invalid analyzer";
    return nullptr;
  }

  auto* analyzerFeature = arangodb::application_features::ApplicationServer::lookupFeature<
    IResearchAnalyzerFeature
  >();

  if (!analyzerFeature) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'" << IResearchAnalyzerFeature::name()
      << "' feature is not registered, unable to evaluate '" << functionName << "' function";
    return nullptr;
  }

  ScopedAqlValue analyzerValue(*analyzerArg);
  IResearchAnalyzerFeature::AnalyzerPool::ptr analyzer = IResearchAnalyzerFeature::identity();

  if (filter || analyzerValue.isConstant()) {
    if (!analyzerValue.execute(ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'" << functionName << "' AQL function: Failed to evaluate " << argIdx << " argument";
      return nullptr;
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != analyzerValue.type()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'" << functionName << "' AQL function: " << argIdx << " argument has invalid type '" << analyzerValue.type()
        << "' (string expected)";
      return nullptr;
    }

    irs::string_ref analyzerId;

    if (!analyzerValue.getString(analyzerId)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'" << functionName << "' AQL function: Unable to parse " << argIdx << " argument as a string";
      return nullptr;
    }

    analyzer = analyzerFeature->get(analyzerId);

    if (!analyzer) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'" << functionName << "' AQL function: Unable to load requested analyzer '" << analyzerId << "'";
    }
  }

  return analyzer;
}

bool byTerm(
    irs::by_term* filter,
    arangodb::aql::AstNode const& attribute,
    ScopedAqlValue const& value,
    QueryContext const& ctx,
    FilterContext const& filterCtx
) {
  std::string name;

  if (filter && !arangodb::iresearch::nameFromAttributeAccess(name, attribute, ctx)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Failed to generate field name from node " << arangodb::aql::AstNode::toString(&attribute);
    return false;
  }

  switch (value.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_NULL:
      if (filter) {
        kludge::mangleNull(name);
        filter->field(std::move(name));
        filter->boost(filterCtx.boost);
        filter->term(irs::null_token_stream::value_null());
      }
      return true;
    case arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL:
      if (filter) {
        kludge::mangleBool(name);
        filter->field(std::move(name));
        filter->boost(filterCtx.boost);
        filter->term(irs::boolean_token_stream::value(value.getBoolean()));
      }
      return true;
    case arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE:
      if (filter) {
        double_t dblValue;

        if (!value.getDouble(dblValue)) {
          // something went wrong
          return false;
        }

        kludge::mangleNumeric(name);

        irs::numeric_token_stream stream;
        irs::term_attribute const* term = stream.attributes().get<irs::term_attribute>().get();
        TRI_ASSERT(term);
        stream.reset(dblValue);
        stream.next();

        filter->field(std::move(name));
        filter->boost(filterCtx.boost);
        filter->term(term->value());
      }
      return true;
    case arangodb::iresearch::SCOPED_VALUE_TYPE_STRING:
      if (filter) {
        irs::string_ref strValue;

        if (!value.getString(strValue)) {
          // something went wrong
          return false;
        }

        TRI_ASSERT(filterCtx.analyzer);
        kludge::mangleStringField(name, *filterCtx.analyzer);
        filter->field(std::move(name));
        filter->boost(filterCtx.boost);
        filter->term(strValue);
     }
      return true;
    default:
      // unsupported value type
      return false;
  }
}

bool byTerm(
    irs::by_term* filter,
    arangodb::iresearch::NormalizedCmpNode const& node,
    QueryContext const& ctx,
    FilterContext const& filterCtx
) {
  TRI_ASSERT(node.attribute && node.attribute->isDeterministic());
  TRI_ASSERT(node.value && node.value->isDeterministic());

  ScopedAqlValue value(*node.value);

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

  return byTerm(filter, *node.attribute, value, ctx, filterCtx);
}

bool byRange(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& attribute,
    arangodb::aql::Range const& rangeData,
    QueryContext const& ctx,
    FilterContext const& filterCtx
) {
  TRI_ASSERT(attribute.isDeterministic());

  std::string name;

  if (filter && !nameFromAttributeAccess(name, attribute, ctx)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Failed to generate field name from node " << arangodb::aql::AstNode::toString(&attribute);
    return false;
  }

  TRI_ASSERT(filter);
  auto& range = filter->add<irs::by_granular_range>();

  kludge::mangleNumeric(name);
  range.field(std::move(name));
  range.boost(filterCtx.boost);

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
    QueryContext const& ctx,
    FilterContext const& filterCtx
) {
  TRI_ASSERT(attributeNode.isDeterministic());
  TRI_ASSERT(minValueNode.isDeterministic());
  TRI_ASSERT(maxValueNode.isDeterministic());

  ScopedAqlValue min(minValueNode);

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

  ScopedAqlValue max(maxValueNode);

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

  if (filter && !nameFromAttributeAccess(name, attributeNode, ctx)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Failed to generate field name from node " << arangodb::aql::AstNode::toString(&attributeNode);
    return false;
  }

  switch (min.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_NULL: {
      if (filter) {
        kludge::mangleNull(name);

        auto& range = filter->add<irs::by_range>();
        range.field(std::move(name));
        range.boost(filterCtx.boost);
        range.term<irs::Bound::MIN>(irs::null_token_stream::value_null());
        range.include<irs::Bound::MIN>(minInclude);;
        range.term<irs::Bound::MAX>(irs::null_token_stream::value_null());
        range.include<irs::Bound::MAX>(maxInclude);;
      }

      return true;
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL: {
      if (filter) {
        kludge::mangleBool(name);

        auto& range = filter->add<irs::by_range>();
        range.field(std::move(name));
        range.boost(filterCtx.boost);
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

        kludge::mangleNumeric(name);
        range.field(std::move(name));
        range.boost(filterCtx.boost);

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

        TRI_ASSERT(filterCtx.analyzer);
        kludge::mangleStringField(name, *filterCtx.analyzer);
        range.field(std::move(name));
        range.boost(filterCtx.boost);

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
    QueryContext const& ctx,
    FilterContext const& filterCtx
) {
  TRI_ASSERT(node.attribute && node.attribute->isDeterministic());
  TRI_ASSERT(node.value && node.value->isDeterministic());

  ScopedAqlValue value(*node.value);

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

  if (filter && !nameFromAttributeAccess(name, *node.attribute, ctx)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Failed to generate field name from node " << arangodb::aql::AstNode::toString(node.attribute);
    return false;
  }

  switch (value.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_NULL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        kludge::mangleNull(name);
        range.field(std::move(name));
        range.boost(filterCtx.boost);
        range.term<Bound>(irs::null_token_stream::value_null());
        range.include<Bound>(incl);
      }

      return true;
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        kludge::mangleBool(name);
        range.field(std::move(name));
        range.boost(filterCtx.boost);
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

        kludge::mangleNumeric(name);
        range.field(std::move(name));
        range.boost(filterCtx.boost);

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

        TRI_ASSERT(filterCtx.analyzer);
        kludge::mangleStringField(name, *filterCtx.analyzer);
        range.field(std::move(name));
        range.boost(filterCtx.boost);
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
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    std::shared_ptr<arangodb::aql::AstNode> && node
) {
  if (!filter) {
    return true;
  }

  // non-deterministic condition or self-referenced variable
  if (!node->isDeterministic() || arangodb::iresearch::findReference(*node, *ctx.ref)) {
    // not supported by IResearch, but could be handled by ArangoDB
    appendExpression(*filter, std::move(node), ctx, filterCtx);
    return true;
  }

  bool result;

  if (node->isConstant()) {
    result = node->isTrue();
  } else { // deterministic expression
    ScopedAqlValue value(*node);

    if (!value.execute(ctx)) {
      // can't execute expression
      return false;
    }

    result = value.getBoolean();
  }

  if (result) {
    filter->add<irs::all>().boost(filterCtx.boost);
  } else {
    filter->add<irs::empty>();
  }

  return true;
}

bool fromExpression(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& node
) {
  if (!filter) {
    return true;
  }

  // non-deterministic condition or self-referenced variable
  if (!node.isDeterministic() || arangodb::iresearch::findReference(node, *ctx.ref)) {
    // not supported by IResearch, but could be handled by ArangoDB
    appendExpression(*filter, node, ctx, filterCtx);
    return true;
  }

  bool result;

  if (node.isConstant()) {
    result = node.isTrue();
  } else { // deterministic expression
    ScopedAqlValue value(node);

    if (!value.execute(ctx)) {
      // can't execute expression
      return false;
    }

    result = value.getBoolean();
  }

  if (result) {
    filter->add<irs::all>().boost(filterCtx.boost);
  } else {
    filter->add<irs::empty>();
  }

  return true;
}

bool fromInterval(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
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
    return fromExpression(filter, ctx, filterCtx, node);
  }

  bool const incl = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == normNode.cmp
                 || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == normNode.cmp;

  bool const min = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == normNode.cmp
                || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == normNode.cmp;

  return min ? byRange<irs::Bound::MIN>(filter, normNode, incl, ctx, filterCtx)
             : byRange<irs::Bound::MAX>(filter, normNode, incl, ctx, filterCtx);
}

bool fromBinaryEq(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type
  );

  arangodb::iresearch::NormalizedCmpNode normalized;

  if (!arangodb::iresearch::normalizeCmpNode(node, *ctx.ref, normalized)) {
    return fromExpression(filter, ctx, filterCtx, node);
  }

  irs::by_term* termFilter = nullptr;

  if (filter) {
    termFilter = &(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type
      ? filter->add<irs::Not>().filter<irs::by_term>()
      : filter->add<irs::by_term>());
  }

  return byTerm(termFilter, normalized, ctx, filterCtx);
}

bool fromRange(
    irs::boolean_filter* filter,
    QueryContext const& /*ctx*/,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_RANGE == node.type);

  if (node.numMembers() != 2) {
    logMalformedNode(node.type);
    return false; // wrong number of members
  }

  // ranges are always true
  if (filter) {
    filter->add<irs::all>().boost(filterCtx.boost);
  }

  return true;
}

bool fromInArray(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
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
    return fromExpression(filter, ctx, filterCtx, node);
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
      return fromExpression(filter, ctx, filterCtx, node);
    }
  }

  if (!n) {
    if (filter) {
      if (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
        filter->add<irs::all>().boost(filterCtx.boost); // not in [] means 'all'
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
    filter->boost(filterCtx.boost);
  }

  FilterContext const subFilterCtx{
    filterCtx.analyzer,
    irs::boost::no_boost() // reset boost
  };

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
      if (!fromExpression(filter, ctx, subFilterCtx, std::move(exprNode))) {
        return false;
      }
    } else {
      auto* termFilter = filter
          ? &filter->add<irs::by_term>()
          : nullptr;

      if (!byTerm(termFilter, normalized, ctx, subFilterCtx)) {
        return false;
      }
    }
  }

  return true;
}

bool fromIn(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
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
    return fromInArray(filter, ctx, filterCtx, node);
  }

  auto* attributeNode = node.getMemberUnchecked(0);
  TRI_ASSERT(attributeNode);

  if (!node.isDeterministic()
      || !arangodb::iresearch::checkAttributeAccess(attributeNode, *ctx.ref)
      || arangodb::iresearch::findReference(*valueNode, *ctx.ref)) {
    return fromExpression(filter, ctx, filterCtx, node);
  }

  if (!filter) {
    // can't evaluate non constant filter before the execution
    return true;
  }

  if (arangodb::aql::NODE_TYPE_RANGE == valueNode->type) {
    ScopedAqlValue value(*valueNode);

    if (!value.execute(ctx)) {
      // con't execute expression
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
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

    return byRange(filter, *attributeNode, *range, ctx, filterCtx);
  }

  ScopedAqlValue value(*valueNode);

  if (!value.execute(ctx)) {
    // con't execute expression
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "Unable to extract value from 'IN' operator";
    return false;
  }

  switch (value.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_ARRAY: {
      size_t const n = value.size();

      if (!n) {
        if (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
          filter->add<irs::all>().boost(filterCtx.boost); // not in [] means 'all'
        } else {
          filter->add<irs::empty>();
        }

        // nothing to do more
        return true;
      }

      filter = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type
          ? &static_cast<irs::boolean_filter&>(filter->add<irs::Not>().filter<irs::And>())
          : &static_cast<irs::boolean_filter&>(filter->add<irs::Or>());
      filter->boost(filterCtx.boost);

      FilterContext const subFilterCtx{
        filterCtx.analyzer,
        irs::boost::no_boost() // reset boost
      };

      for (size_t i = 0; i < n; ++i) {
        if (!byTerm(&filter->add<irs::by_term>(), *attributeNode, value.at(i), ctx, subFilterCtx)) {
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

      return byRange(filter, *attributeNode, *range, ctx, filterCtx);
    }
    default:
      break;
  }

  // wrong value node type
  return false;
}

bool fromNegation(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
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
    auto& notFilter = filter->add<irs::Not>();
    notFilter.boost(filterCtx.boost);

    filter = &notFilter.filter<irs::And>();
  }

  FilterContext const subFilterCtx{
    filterCtx.analyzer,
    irs::boost::no_boost() // reset boost
  };

  return ::filter(filter, ctx, subFilterCtx, *member);
}

bool rangeFromBinaryAnd(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
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

        if (byRange(filter, *lhsAttr, *lhsValue, lhsInclude, *rhsValue, rhsInclude, ctx, filterCtx)) {
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
    QueryContext const& ctx,
    FilterContext const& filterCtx,
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

  if (std::is_same<Filter, irs::And>::value && 2 == n && rangeFromBinaryAnd(filter, ctx, filterCtx, node)) {
    // range case
    return true;
  }

  if (filter) {
    filter = &filter->add<Filter>();
    filter->boost(filterCtx.boost);
  }

  FilterContext const subFilterCtx{
    filterCtx.analyzer,
    irs::boost::no_boost() // reset boost
  };

  for (size_t i = 0; i < n; ++i) {
    auto const* valueNode = node.getMemberUnchecked(i);
    TRI_ASSERT(valueNode);

    if (!::filter(filter, ctx, subFilterCtx, *valueNode)) {
      return false;
    }
  }

  return true;
}

// Analyze(<filter-expression>, analyzer)
bool fromFuncAnalyzer(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args
) {
  auto const argc = args.numMembers();

  if (argc != 2) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'ANALYZER' AQL function: Invalid number of arguments passed (must be 2)";
    return false;
  }

  // 1st argument defines filter expression
  auto expressionArg = args.getMemberUnchecked(0);

  if (!expressionArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'ANALYZER' AQL function: 1st argument is invalid";
    return false;
  }

  // 2nd argument defines a boost
  auto const analyzerArg = args.getMemberUnchecked(1);

  if (!analyzerArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'ANALYZER' AQL function: 2nd argument is invalid";
    return false;
  }

  if (!analyzerArg->isDeterministic()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'ANALYZER' AQL function: 2nd argument is intended to be deterministic";
    return false;
  }

  ScopedAqlValue analyzerId(*analyzerArg);
  auto analyzer = IResearchAnalyzerFeature::identity(); // default analyzer

  if (filter || analyzerId.isConstant()) {
    if (!analyzerId.execute(ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'ANALYZER' AQL function: Failed to evaluate 2nd argument";
      return false;
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != analyzerId.type()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "'ANALYZER' AQL function: 'analyzer' argument has invalid type '" << analyzerId.type()
          << "' (string expected)";
      return false;
    }

    irs::string_ref analyzerIdValue;

    if (!analyzerId.getString(analyzerIdValue)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'ANALYZER' AQL function: Unable to parse 2nd argument as string";
      return false;
    }

    auto* analyzerFeature = arangodb::application_features::ApplicationServer::lookupFeature<
      IResearchAnalyzerFeature
    >();

    if (!analyzerFeature) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'" << IResearchAnalyzerFeature::name()
        << "' feature is not registered, unable to evaluate 'ANALYZER' function";
      return false;
    }

    analyzer = analyzerFeature->get(analyzerIdValue);

    if (!analyzer) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'ANALYZER' AQL function: Unable to lookup analyzer '" << analyzerIdValue << "'";
      return false;
    }
  }

  FilterContext const subFilterContext{
    analyzer, // override analyzer
    filterCtx.boost
  };

  return ::filter(filter, ctx, subFilterContext, *expressionArg);
}

// BOOST(<filter-expression>, boost)
bool fromFuncBoost(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args
) {
  auto const argc = args.numMembers();

  if (argc != 2) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'BOOST' AQL function: Invalid number of arguments passed (must be 2)";
    return false;
  }

  // 1st argument defines filter expression
  auto expressionArg = args.getMemberUnchecked(0);

  if (!expressionArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'BOOST' AQL function: 1st argument is invalid";
    return false;
  }

  // 2nd argument defines a boost
  auto const boostArg = args.getMemberUnchecked(1);

  if (!boostArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'BOOST' AQL function: 2nd argument is invalid";
    return false;
  }

  if (!boostArg->isDeterministic()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'BOOST' AQL function: 2nd argument is intended to be deterministic";
    return false;
  }

  ScopedAqlValue boost(*boostArg);
  double_t boostValue = 0;

  if (filter || boost.isConstant()) {
    if (!boost.execute(ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'BOOST' AQL function: Failed to evaluate 2nd argument";
      return false;
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE != boost.type()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'BOOST' AQL function: 2nd argument has invalid type '" << boost.type()
        << "' (double expected)";
      return false;
    }

    if (!boost.getDouble(boostValue)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'BOOST' AQL function: Failed to parse 2nd argument as string";
      return false;
    }
  }

  FilterContext const subFilterContext {
    filterCtx.analyzer,
    filterCtx.boost * static_cast<float_t>(boostValue)
  };

  return ::filter(filter, ctx, subFilterContext, *expressionArg);
}

// EXISTS(<attribute>, <"analyzer">, <"analyzer-name">)
// EXISTS(<attribute>, <"string"|"null"|"bool"|"numeric">)
bool fromFuncExists(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args
) {
  if (!args.isDeterministic()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Unable to handle non-deterministic arguments for 'EXISTS' function";
    return false; // nondeterministic
  }

  auto const argc = args.numMembers();

  if (argc < 1 || argc > 3) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'EXISTS' AQL function: Invalid number of arguments passed (must be >= 1 and <= 3)";
    return false;
  }

  // 1st argument defines a field
  auto const* fieldArg = arangodb::iresearch::checkAttributeAccess(
    args.getMemberUnchecked(0), *ctx.ref
  );

  if (!fieldArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'EXISTS' AQL function: 1st argument is invalid";
    return false;
  }

  bool prefixMatch = true;
  std::string fieldName;
  auto analyzer = filterCtx.analyzer;

  if (filter && !arangodb::iresearch::nameFromAttributeAccess(fieldName, *fieldArg, ctx)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'EXISTS' AQL function: Failed to generate field name from the 1st argument";
    return false;
  }

  if (argc > 1) {
    // 2nd argument defines a type (if present)
    auto const typeArg = args.getMemberUnchecked(1);

    if (!typeArg) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'EXISTS' AQL function: 2nd argument is invalid";
      return false;
    }

    irs::string_ref arg;
    ScopedAqlValue type(*typeArg);

    if (filter || type.isConstant()) {
      if (!type.execute(ctx)) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
            << "'EXISTS' AQL function: Failed to evaluate 2nd argument";
        return false;
      }

      if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != type.type()) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
            << "'EXISTS' AQL function: 2nd argument has invalid type '" << type.type()
            << "' (string expected)";
        return false;
      }

      if (!type.getString(arg)) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
            << "'EXISTS' AQL function: Failed to parse 2nd argument as string";
        return false;
      }

      std::string strArg(arg);
      arangodb::basics::StringUtils::tolowerInPlace(&strArg); // normalize user input
      irs::string_ref const TypeAnalyzer("analyzer");

      typedef bool(*TypeHandler)(std::string&, IResearchAnalyzerFeature::AnalyzerPool const&);

      static std::map<irs::string_ref, TypeHandler> const TypeHandlers {
        // any string
        {
          irs::string_ref("string"),
          [] (std::string& name,
              IResearchAnalyzerFeature::AnalyzerPool const&) {
            kludge::mangleAnalyzer(name);
            return true; // a prefix match
          }
        },
        // any non-string type
        {
          irs::string_ref("type"),
          [](std::string& name,
             IResearchAnalyzerFeature::AnalyzerPool const&) {
            kludge::mangleType(name);
            return true; // a prefix match
          }
        },
        // concrete analyzer from the context
        {
          TypeAnalyzer,
          [](std::string& name,
             IResearchAnalyzerFeature::AnalyzerPool const& analyzer) {
            kludge::mangleStringField(name, analyzer);
            return false; // not a prefix match
          }
        },
        {
          irs::string_ref("numeric"),
          [] (std::string& name, IResearchAnalyzerFeature::AnalyzerPool const&) {
            kludge::mangleNumeric(name);
            return false; // not a prefix match
          }
        },
        {
          irs::string_ref("bool"),
          [] (std::string& name, IResearchAnalyzerFeature::AnalyzerPool const&) {
            kludge::mangleBool(name);
            return false; // not a prefix match
          }
        },
        {
          irs::string_ref("boolean"),
          [] (std::string& name, IResearchAnalyzerFeature::AnalyzerPool const&) {
            kludge::mangleBool(name);
            return false; // not a prefix match
          }
        },
        {
          irs::string_ref("null"),
          [] (std::string& name, IResearchAnalyzerFeature::AnalyzerPool const&) {
            kludge::mangleNull(name);
            return false; // not a prefix match
          }
        }
      };

      auto const typeHandler = TypeHandlers.find(strArg);

      if (TypeHandlers.end() == typeHandler) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
            << "'EXISTS' AQL function: 2nd argument must be equal to one of the following:"
               " 'string', 'type', 'analyzer', 'numeric', 'bool', 'boolean', 'null', but got '" << arg << "'";
        return false;
      }

      if (argc > 2) {
        if (TypeAnalyzer.c_str() != typeHandler->first.c_str()) {
          LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
              << "'EXISTS' AQL function: 3rd argument is intended to use with 'analyzer' type only";
          return false;
        }

        analyzer = extractAnalyzerFromArg(
          filter, args.getMemberUnchecked(2),  ctx,  2, "EXISTS"
        );

        if (!analyzer) {
          return false;
        }
      }

      prefixMatch = typeHandler->second(fieldName, *analyzer);
    }
  }

  if (filter) {
    auto& exists = filter->add<irs::by_column_existence>();
    exists.field(std::move(fieldName));
    exists.boost(filterCtx.boost);
    exists.prefix_match(prefixMatch);
  }

  return true;
}

// MIN_MATCH(<filter-expression>[, <filter-expression>,...], <min-match-count>)
bool fromFuncMinMatch(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args
) {
  auto const argc = args.numMembers();

  if (argc < 2) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'MIN_MATCH' AQL function: Invalid number of arguments passed (must be >= 2)";
    return false;
  }

  // ...........................................................................
  // last argument defines min match count
  // ...........................................................................

  auto const lastArg = argc - 1;
  auto minMatchCountArg = args.getMemberUnchecked(lastArg);

  if (!minMatchCountArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'MIN_MATCH' AQL function: " << lastArg << " argument is invalid";
    return false;
  }

  if (!minMatchCountArg->isDeterministic()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'MIN_MATCH' AQL function: " << lastArg << " argument is intended to be deterministic";
    return false;
  }

  ScopedAqlValue minMatchCount(*minMatchCountArg);
  int64_t minMatchCountValue = 0;

  if (filter || minMatchCount.isConstant()) {
    if (!minMatchCount.execute(ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'MIN_MATCH' AQL function: Failed to evaluate " << lastArg << " argument";
      return false;
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE != minMatchCount.type()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'MIN_MATCH' AQL function: " << lastArg << " argument has invalid type '" << minMatchCount.type()
        << "' (numeric expected)";
      return false;
    }

    minMatchCountValue = minMatchCount.getInt64();
  }

  if (filter) {
    auto& minMatchFilter = filter->add<irs::Or>();
    minMatchFilter.min_match_count(minMatchCountValue);
    minMatchFilter.boost(filterCtx.boost);

    // become a new root
    filter = &minMatchFilter;
  }

  FilterContext const subFilterCtx {
    filterCtx.analyzer,
    irs::boost::no_boost() // reset boost
  };

  for (size_t i = 0; i < lastArg; ++i) {
    auto subFilterExpression = args.getMemberUnchecked(i);

    if (!subFilterExpression) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'MIN_MATCH' AQL function: Failed to evaluate " << i << " argument";
      return false;
    }

    irs::boolean_filter* subFilter = filter
      ? &filter->add<irs::Or>()
      : nullptr;

    if (!::filter(subFilter, ctx, subFilterCtx, *subFilterExpression)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'MIN_MATCH' AQL function: Failed to instantiate sub-filter for " << i << " argument";
      return false;
    }
  }

  return true;
}

// PHRASE(<attribute>, <value> [, <offset>, <value>, ...] [, <analyzer>])
// PHRASE(<attribute>, '[' <value> [, <offset>, <value>, ...] ']' [, <analyzer>])
bool fromFuncPhrase(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args
) {
  if (!args.isDeterministic()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Unable to handle non-deterministic arguments for 'PHRASE' function";
    return false; // nondeterministic
  }

  auto argc = args.numMembers();

  if (argc < 2) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'PHRASE' AQL function: Invalid number of arguments passed (must be >= 2)";
    return false;
  }

  // ...........................................................................
  // last odd argument defines an analyzer
  // ...........................................................................

  auto analyzerPool = filterCtx.analyzer;

  if (0 != (argc & 1)) { // override analyzer
    --argc;

    analyzerPool = extractAnalyzerFromArg(
      filter, args.getMemberUnchecked(argc), ctx, argc, "PHRASE"
    );

    if (!analyzerPool) {
      return false;
    }
  }

  // ...........................................................................
  // 1st argument defines a field
  // ...........................................................................

  auto const* fieldArg = arangodb::iresearch::checkAttributeAccess(
    args.getMemberUnchecked(0), *ctx.ref
  );

  if (!fieldArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'PHRASE' AQL function: 1st argument is invalid";
    return false;
  }

  // ...........................................................................
  // 2nd argument defines a value
  // ...........................................................................

  auto const* valueArg = args.getMemberUnchecked(1);

  if (!valueArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'PHRASE' AQL function: 2nd argument is invalid";
    return false;
  }

  auto* valueArgs = &args;
  size_t valueArgsBegin = 1;
  size_t valueArgsEnd = argc;

  if (valueArg->isArray()) {
    valueArgs = valueArg;
    valueArgsBegin = 0;
    valueArgsEnd = valueArg->numMembers();

    if (0 == (valueArgsEnd & 1)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'PHRASE' AQL function: 2nd argument has an invalid number of members (must be an odd number)";
      return false;
    }

    valueArg = valueArgs->getMemberUnchecked(valueArgsBegin);

    if (!valueArg) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'PHRASE' AQL function: 2nd argument has an invalid member at offset: " << valueArg;
      return false;
    }
  }

  irs::string_ref value;
  ScopedAqlValue inputValue(*valueArg);

  if (filter || inputValue.isConstant()) {
    if (!inputValue.execute(ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "'PHRASE' AQL function: Failed to evaluate 2nd argument";
      return false;
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != inputValue.type()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "'PHRASE' AQL function: 2nd argument has invalid type '" << inputValue.type()
          << "' (string expected)";
      return false;
    }

    if (!inputValue.getString(value)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "'PHRASE' AQL function: Unable to parse 2nd argument as string";
      return false;
    }
  }

  irs::by_phrase* phrase = nullptr;
  irs::analysis::analyzer::ptr analyzer;

  if (filter) {
    std::string name;

    if (!arangodb::iresearch::nameFromAttributeAccess(name, *fieldArg, ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'PHRASE' AQL function: Failed to generate field name from the 1st argument";
      return false;
    }

    TRI_ASSERT(analyzerPool);
    analyzer = analyzerPool->get(); // get analyzer from pool

    if (!analyzer) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'PHRASE' AQL function: Unable to instantiate analyzer '" << analyzerPool->name() << "'";
      return false;
    }

    kludge::mangleStringField(name, *analyzerPool);

    phrase = &filter->add<irs::by_phrase>();
    phrase->field(std::move(name));
    phrase->boost(filterCtx.boost);

    TRI_ASSERT(analyzer);
    appendTerms(*phrase, value, *analyzer, 0);
  }

  decltype(fieldArg) offsetArg = nullptr;
  size_t offset = 0;

  for (size_t idx = valueArgsBegin + 1, end = valueArgsEnd; idx < end; idx += 2) {
    offsetArg = valueArgs->getMemberUnchecked(idx);

    if (!offsetArg) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'PHRASE' AQL function: Unable to parse argument on position " << idx << " as an offset";
      return false;
    }

    valueArg = valueArgs->getMemberUnchecked(idx + 1);

    if (!valueArg) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'PHRASE' AQL function: Unable to parse argument on position " << idx + 1 << " as a value";
      return false;
    }

    ScopedAqlValue offsetValue(*offsetArg);

    if (filter || offsetValue.isConstant()) {
      if (!offsetValue.execute(ctx) || arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE != offsetValue.type()) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
            << "'PHRASE' AQL function: Unable to parse argument on position " << idx << " as an offset";
        return false;
      }

      offset = static_cast<uint64_t>(offsetValue.getInt64());
    }

    ScopedAqlValue inputValue(*valueArg);

    if (filter || inputValue.isConstant()) {
      if (!inputValue.execute(ctx)
          || arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != inputValue.type()
          || !inputValue.getString(value)) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "'PHRASE' AQL function: Unable to parse argument on position " << idx + 1 << " as a value";
        return false;
      }
    }

    if (phrase) {
      TRI_ASSERT(analyzer);
      appendTerms(*phrase, value, *analyzer, offset);
    }
  }

  return true;
}

// STARTS_WITH(<attribute>, <prefix>, [<scoring-limit>])
bool fromFuncStartsWith(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args
) {
  if (!args.isDeterministic()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Unable to handle non-deterministic arguments for 'STARTS_WITH' function";
    return false; // nondeterministic
  }

  auto const argc = args.numMembers();

  if (argc < 2 || argc > 3) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'STARTS_WITH' AQL function: Invalid number of arguments passed (should be >= 2 and <= 3)";
    return false;
  }

  // 1st argument defines a field
  auto const* field = arangodb::iresearch::checkAttributeAccess(
    args.getMemberUnchecked(0), *ctx.ref
  );

  if (!field) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'STARTS_WITH' AQL function: Unable to parse 1st argument as an attribute identifier";
    return false;
  }

  // 2nd argument defines a value
  auto const* prefixArg = args.getMemberUnchecked(1);

  if (!prefixArg) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "'STARTS_WITH' AQL function: 2nd argument is invalid";
    return false;
  }

  irs::string_ref prefix;
  size_t scoringLimit = 128; // FIXME make configurable

  ScopedAqlValue prefixValue(*prefixArg);

  if (filter || prefixValue.isConstant()) {
    if (!prefixValue.execute(ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "'STARTS_WITH' AQL function: Failed to evaluate 2nd argument";
      return false;
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != prefixValue.type()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "'STARTS_WITH' AQL function: 2nd argument has invalid type '" << prefixValue.type()
          << "' (string expected)";
      return false;
    }

    if (!prefixValue.getString(prefix)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "'STARTS_WITH' AQL function: Unable to parse 2nd argument as string";
      return false;
    }
  }

  if (argc > 2) {
    // 3rd (optional) argument defines a number of scored terms
    auto const* scoringLimitArg = args.getMemberUnchecked(2);

    if (!scoringLimitArg) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'STARTS_WITH' AQL function: 3rd argument is invalid";
      return false;
    }

    ScopedAqlValue scoringLimitValue(*scoringLimitArg);

    if (filter || scoringLimitValue.isConstant()) {
      if (!scoringLimitValue.execute(ctx)) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
            << "'STARTS_WITH' AQL function: Failed to evaluate 3rd argument";
        return false;
      }

      if (arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE != scoringLimitValue.type()) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
            << "'STARTS_WITH' AQL function: 3rd argument has invalid type '" << scoringLimitValue.type()
            << "' (numeric expected)";
        return false;
      }

      scoringLimit = static_cast<size_t>(scoringLimitValue.getInt64());
    }
  }

  if (filter) {
    std::string name;

    if (!nameFromAttributeAccess(name, *field, ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "'STARTS_WITH' AQL function: Failed to generate field name from the 1st argument";
      return false;
    }

    TRI_ASSERT(filterCtx.analyzer);
    kludge::mangleStringField(name, *filterCtx.analyzer);

    auto& prefixFilter = filter->add<irs::by_prefix>();
    prefixFilter.scored_terms_limit(scoringLimit);
    prefixFilter.field(std::move(name));
    prefixFilter.boost(filterCtx.boost);
    prefixFilter.term(prefix);
  }

  return true;
}

std::map<irs::string_ref, ConvertionHandler> const FCallUserConvertionHandlers;

bool fromFCallUser(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
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
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Unable to parse user function arguments as an array'";
    return false; // invalid args
  }

  irs::string_ref name;

  if (!arangodb::iresearch::parseValue(name, node)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Unable to parse user function name";

    return false;
  }

  auto const entry = FCallUserConvertionHandlers.find(name);

  if (entry == FCallUserConvertionHandlers.end()) {
    return fromExpression(filter, ctx, filterCtx, node);
  }

  if (!args->isDeterministic()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Unable to handle non-deterministic function '" << name << "' arguments";

    return false; // nondeterministic
  }

  return entry->second(filter, ctx, filterCtx, *args);
}

std::map<std::string, ConvertionHandler> const FCallSystemConvertionHandlers{
  { "PHRASE", fromFuncPhrase },
  { "STARTS_WITH", fromFuncStartsWith },
  { "EXISTS", fromFuncExists },
  { "BOOST", fromFuncBoost },
  { "ANALYZER", fromFuncAnalyzer },
  { "MIN_MATCH", fromFuncMinMatch }
};

bool fromFCall(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL == node.type);

  auto const* fn = static_cast<arangodb::aql::Function*>(node.getData());

  if (!fn || node.numMembers() != 1) {
    logMalformedNode(node.type);
    return false; // no function
  }

  if (!arangodb::iresearch::isFilter(*fn)) {
    // not a filter function
    return fromExpression(filter, ctx, filterCtx, node);
  }

  auto const entry = FCallSystemConvertionHandlers.find(fn->name);

  if (entry == FCallSystemConvertionHandlers.end()) {
    return fromExpression(filter, ctx, filterCtx, node);
  }

  auto const* args = arangodb::iresearch::getNode(node, 0, arangodb::aql::NODE_TYPE_ARRAY);

  if (!args) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "Unable to parse arguments of system function '" << fn->name << "' as an array'";
    return false; // invalid args
  }

  return entry->second(filter, ctx, filterCtx, *args);
}

bool fromFilter(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FILTER == node.type);

  if (node.numMembers() != 1) {
    logMalformedNode(node.type);
    return false; // wrong number of members
  }

  auto const* member = node.getMemberUnchecked(0);

  return member && ::filter(filter, ctx, filterCtx, *member);
}

bool filter(
    irs::boolean_filter* filter,
    QueryContext const& queryCtx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& node
) {
  switch (node.type) {
    case arangodb::aql::NODE_TYPE_FILTER: // FILTER
      return fromFilter(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_VARIABLE: // variable
      return fromExpression(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_UNARY_NOT: // unary minus
      return fromNegation(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND: // logical and
      return fromGroup<irs::And>(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_OR: // logical or
      return fromGroup<irs::Or>(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ: // compare ==
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE: // compare !=
      return fromBinaryEq(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT: // compare <
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE: // compare <=
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT: // compare >
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE: // compare >=
      return fromInterval(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN: // compare in
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN: // compare not in
      return fromIn(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_TERNARY: // ternary
    case arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS: // attribute access
    case arangodb::aql::NODE_TYPE_VALUE : // value
    case arangodb::aql::NODE_TYPE_ARRAY:  // array
    case arangodb::aql::NODE_TYPE_OBJECT: // object
    case arangodb::aql::NODE_TYPE_REFERENCE: // reference
    case arangodb::aql::NODE_TYPE_PARAMETER: // bind parameter
      return fromExpression(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_FCALL: // function call
      return fromFCall(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_FCALL_USER: // user function call
      return fromFCallUser(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_RANGE: // range
      return fromRange(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND: // n-ary and
      return fromGroup<irs::And>(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_NARY_OR: // n-ary or
      return fromGroup<irs::Or>(filter, queryCtx, filterCtx, node);
    default:
      return fromExpression(filter, queryCtx, filterCtx, node);
  }
}

}

namespace arangodb {
namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                      FilerFactory implementation
// ----------------------------------------------------------------------------

/*static*/ bool FilterFactory::filter(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    arangodb::aql::AstNode const& node
) {
  FilterContext const filterCtx{
    IResearchAnalyzerFeature::identity(),
    irs::boost::no_boost()
  };

  return ::filter(filter, ctx, filterCtx, node);
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
