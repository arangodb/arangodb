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

// otherwise define conflict between 3rdParty\date\include\date\date.h and
// 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
#include "date/date.h"
#endif

#include "search/all_filter.hpp"
#include "search/boolean_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/granular_range_filter.hpp"
#include "search/phrase_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/term_filter.hpp"
#include "search/wildcard_filter.hpp"

#include "Aql/Ast.h"
#include "Aql/Function.h"
#include "Aql/Range.h"
#include "Aql/Quantifier.h"
#include "Basics/StringUtils.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchKludge.h"
#include "Logger/LogMacros.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Transaction/Methods.h"

using namespace arangodb::iresearch;
using namespace std::literals::string_literals;

namespace {

struct FilterContext {
  FilterContext( // constructor
      arangodb::iresearch::FieldMeta::Analyzer const& analyzer, // analyzer
                irs::boost_t boost) noexcept
      : analyzer(analyzer), boost(boost) {
    TRI_ASSERT(analyzer._pool);
  }

  FilterContext(FilterContext const&) = default;
  FilterContext& operator=(FilterContext const&) = delete;

  // need shared_ptr since pool could be deleted from the feature
  arangodb::iresearch::FieldMeta::Analyzer const& analyzer;
  irs::boost_t boost;
};  // FilterContext

typedef std::function<
  arangodb::Result(irs::boolean_filter*,
                   QueryContext const&,
                   FilterContext const&,
                   arangodb::aql::AstNode const&)
> ConvertionHandler;

// forward declaration
arangodb::Result filter(irs::boolean_filter* filter,
                        QueryContext const& queryctx,
                        FilterContext const& filterCtx,
                        arangodb::aql::AstNode const& node);

////////////////////////////////////////////////////////////////////////////////
/// @brief logs message about malformed AstNode with the specified type
////////////////////////////////////////////////////////////////////////////////
arangodb::Result logMalformedNode(arangodb::aql::AstNodeType type) {
  auto const* typeName = arangodb::iresearch::getNodeTypeName(type);

  std::string message("Can't process malformed AstNode of type '");
  if (typeName) {
    message += *typeName;
  } else {
    message += std::to_string(type);
  }
  message += "'";

  LOG_TOPIC("5070f", WARN, arangodb::iresearch::TOPIC) << message;
  return {TRI_ERROR_BAD_PARAMETER, message};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends value tokens to a phrase filter
////////////////////////////////////////////////////////////////////////////////
void appendTerms(irs::by_phrase& filter, irs::string_ref const& value,
                 irs::analysis::analyzer& stream, size_t firstOffset) {
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

FORCE_INLINE void appendExpression(irs::boolean_filter& filter,
                                   arangodb::aql::AstNode const& node,
                                   QueryContext const& ctx, FilterContext const& filterCtx) {
  auto& exprFilter = filter.add<arangodb::iresearch::ByExpression>();
  exprFilter.init(*ctx.plan, *ctx.ast, const_cast<arangodb::aql::AstNode&>(node));
  exprFilter.boost(filterCtx.boost);
}

FORCE_INLINE void appendExpression(irs::boolean_filter& filter,
                                   std::shared_ptr<arangodb::aql::AstNode>&& node,
                                   QueryContext const& ctx, FilterContext const& filterCtx) {
  auto& exprFilter = filter.add<arangodb::iresearch::ByExpression>();
  exprFilter.init(*ctx.plan, *ctx.ast, std::move(node));
  exprFilter.boost(filterCtx.boost);
}

arangodb::iresearch::FieldMeta::Analyzer extractAnalyzerFromArg(
    irs::boolean_filter const* filter, arangodb::aql::AstNode const* analyzerArg,
    QueryContext const& ctx, size_t argIdx, irs::string_ref const& functionName) {
  static const arangodb::iresearch::FieldMeta::Analyzer invalid( // invalid analyzer
    nullptr, "");

  if (!analyzerArg) {
    auto message =  "'"s + std::string(functionName.c_str(), functionName.size()) + "' AQL function: " + std::to_string(argIdx) + " argument is invalid analyzer";
    LOG_TOPIC("a33c4", WARN, arangodb::iresearch::TOPIC) << message;
    THROW_ARANGO_EXCEPTION((arangodb::Result{TRI_ERROR_BAD_PARAMETER, message}));
    return invalid;
  }

  auto& server = arangodb::application_features::ApplicationServer::server();
  if (!server.hasFeature<IResearchAnalyzerFeature>()) {
    LOG_TOPIC("03314", WARN, arangodb::iresearch::TOPIC)
        << "'" << IResearchAnalyzerFeature::name()
        << "' feature is not registered, unable to evaluate '" << functionName
        << "' function";

    return invalid;
  }
  auto& analyzerFeature = server.getFeature<IResearchAnalyzerFeature>();

  ScopedAqlValue analyzerValue(*analyzerArg);

  if (!filter && !analyzerValue.isConstant()) {
    return arangodb::iresearch::FieldMeta::Analyzer();
  }

  if (!analyzerValue.execute(ctx)) {
    LOG_TOPIC("9f918", WARN, arangodb::iresearch::TOPIC)
      << "'" << functionName << "' AQL function: Failed to evaluate " << argIdx << " argument";

    return invalid;
  }

  if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != analyzerValue.type()) {
    LOG_TOPIC("3ac4c", WARN, arangodb::iresearch::TOPIC)
      << "'" << functionName << "' AQL function: " << argIdx << " argument has invalid type '" << arangodb::iresearch::ScopedAqlValue::typeString(analyzerValue.type()) << "' (string expected)";

    return invalid;
  }

  irs::string_ref analyzerId;

  if (!analyzerValue.getString(analyzerId)) {
    LOG_TOPIC("73ca9", WARN, arangodb::iresearch::TOPIC)
      << "'" << functionName << "' AQL function: Unable to parse " << argIdx << " argument as a string";

    return invalid;
  }

  arangodb::iresearch::FieldMeta::Analyzer result(nullptr, analyzerId);
  auto& analyzer = result._pool;
  auto& shortName = result._shortName;

  if (ctx.trx) {
    auto& server = arangodb::application_features::ApplicationServer::server();
    auto sysVocbase = server.hasFeature<arangodb::SystemDatabaseFeature>()
                          ? server.getFeature<arangodb::SystemDatabaseFeature>().use()
                          : nullptr;

    if (sysVocbase) {
      analyzer = analyzerFeature.get(analyzerId, ctx.trx->vocbase(), *sysVocbase);

      shortName = arangodb::iresearch::IResearchAnalyzerFeature::normalize( // normalize
        analyzerId, ctx.trx->vocbase(), *sysVocbase, false); // args
    }
  } else {
    analyzer = analyzerFeature.get(analyzerId);  // verbatim
  }

  if (!analyzer) {
    LOG_TOPIC("402b1", WARN, arangodb::iresearch::TOPIC)
      << "'" << functionName << "' AQL function: Unable to load requested analyzer '" << analyzerId << "'";
  }

  return result;
}

arangodb::Result byTerm(irs::by_term* filter, std::string name,
                        ScopedAqlValue const& value, QueryContext const& ctx,
                        FilterContext const& filterCtx) {
  switch (value.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_NULL:
      if (filter) {
        kludge::mangleNull(name);
        filter->field(std::move(name));
        filter->boost(filterCtx.boost);
        filter->term(irs::null_token_stream::value_null());
      }
      return {};
    case arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL:
      if (filter) {
        kludge::mangleBool(name);
        filter->field(std::move(name));
        filter->boost(filterCtx.boost);
        filter->term(irs::boolean_token_stream::value(value.getBoolean()));
      }
      return {};
    case arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE:
      if (filter) {
        double_t dblValue;

        if (!value.getDouble(dblValue)) {
          // something went wrong
          return {TRI_ERROR_BAD_PARAMETER, "could not get double value"};
        }

        kludge::mangleNumeric(name);

        irs::numeric_token_stream stream;
        irs::term_attribute const* term =
            stream.attributes().get<irs::term_attribute>().get();
        TRI_ASSERT(term);
        stream.reset(dblValue);
        stream.next();

        filter->field(std::move(name));
        filter->boost(filterCtx.boost);
        filter->term(term->value());
      }
      return {};
    case arangodb::iresearch::SCOPED_VALUE_TYPE_STRING:
      if (filter) {
        irs::string_ref strValue;

        if (!value.getString(strValue)) {
          // something went wrong
          return {TRI_ERROR_BAD_PARAMETER, "could not get string value"};
        }

        TRI_ASSERT(filterCtx.analyzer._pool);
        kludge::mangleStringField(name, filterCtx.analyzer);
        filter->field(std::move(name));
        filter->boost(filterCtx.boost);
        filter->term(strValue);
      }
      return {};
    default:
      // unsupported value type
      return {TRI_ERROR_BAD_PARAMETER, "unsupported type"};
  }
}

arangodb::Result byTerm(irs::by_term* filter, arangodb::aql::AstNode const& attribute,
            ScopedAqlValue const& value, QueryContext const& ctx,
            FilterContext const& filterCtx) {
  std::string name;
  if (filter && !arangodb::iresearch::nameFromAttributeAccess(name, attribute, ctx)) {
    auto message = "Failed to generate field name from node " + arangodb::aql::AstNode::toString(&attribute);
    LOG_TOPIC("d4b6e", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }
  return byTerm(filter, std::move(name), value, ctx, filterCtx);
}

arangodb::Result byTerm(irs::by_term* filter, arangodb::iresearch::NormalizedCmpNode const& node,
            QueryContext const& ctx, FilterContext const& filterCtx) {
  TRI_ASSERT(node.attribute && node.attribute->isDeterministic());
  TRI_ASSERT(node.value && node.value->isDeterministic());

  ScopedAqlValue value(*node.value);

  if (!value.isConstant()) {
    if (!filter) {
      // can't evaluate non constant filter before the execution
      return {};
    }

    if (!value.execute(ctx)) {
      // failed to execute expression
      return {TRI_ERROR_BAD_PARAMETER, "could not execute expression"};
    }
  }

  return byTerm(filter, *node.attribute, value, ctx, filterCtx);
}

arangodb::Result byRange(irs::boolean_filter* filter, arangodb::aql::AstNode const& attribute,
             arangodb::aql::Range const& rangeData, QueryContext const& ctx,
             FilterContext const& filterCtx) {
  TRI_ASSERT(attribute.isDeterministic());

  std::string name;

  if (filter && !nameFromAttributeAccess(name, attribute, ctx)) {
    auto message = "Failed to generate field name from node " + arangodb::aql::AstNode::toString(&attribute);
    LOG_TOPIC("cb2d8", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
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

  return {};
}

arangodb::Result byRange(irs::boolean_filter* filter, arangodb::aql::AstNode const& attributeNode,
             arangodb::aql::AstNode const& minValueNode, bool const minInclude,
             arangodb::aql::AstNode const& maxValueNode, bool const maxInclude,
             QueryContext const& ctx, FilterContext const& filterCtx) {
  TRI_ASSERT(attributeNode.isDeterministic());
  TRI_ASSERT(minValueNode.isDeterministic());
  TRI_ASSERT(maxValueNode.isDeterministic());

  ScopedAqlValue min(minValueNode);

  if (!min.isConstant()) {
    if (!filter) {
      // can't evaluate non constant filter before the execution
      return {};
    }

    if (!min.execute(ctx)) {
      auto message = "Failed to evaluate lower boundary from node '" + arangodb::aql::AstNode::toString(&minValueNode) + "'";
      LOG_TOPIC("1e23d", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }
  }

  ScopedAqlValue max(maxValueNode);

  if (!max.isConstant()) {
    if (!filter) {
      // can't evaluate non constant filter before the execution
      return {};
    }

    if (!max.execute(ctx)) {
      auto message = "Failed to evaluate upper boundary from node '" + arangodb::aql::AstNode::toString(&maxValueNode) + "'";
      LOG_TOPIC("5a0a4", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }
  }

  if (min.type() != max.type()) {
    auto message = "Failed to build range query, lower boundary '" + arangodb::aql::AstNode::toString(&minValueNode) +
                   "' mismatches upper boundary '" + arangodb::aql::AstNode::toString(&maxValueNode) + "'";
    LOG_TOPIC("03078", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  std::string name;

  if (filter && !nameFromAttributeAccess(name, attributeNode, ctx)) {
    auto message = "Failed to generate field name from node " + arangodb::aql::AstNode::toString(&attributeNode);
    LOG_TOPIC("00282", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  switch (min.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_NULL: {
      if (filter) {
        kludge::mangleNull(name);

        auto& range = filter->add<irs::by_range>();
        range.field(std::move(name));
        range.boost(filterCtx.boost);
        range.term<irs::Bound::MIN>(irs::null_token_stream::value_null());
        range.include<irs::Bound::MIN>(minInclude);
        range.term<irs::Bound::MAX>(irs::null_token_stream::value_null());
        range.include<irs::Bound::MAX>(maxInclude);
      }

      return {};
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL: {
      if (filter) {
        kludge::mangleBool(name);

        auto& range = filter->add<irs::by_range>();
        range.field(std::move(name));
        range.boost(filterCtx.boost);
        range.term<irs::Bound::MIN>(irs::boolean_token_stream::value(min.getBoolean()));
        range.include<irs::Bound::MIN>(minInclude);
        range.term<irs::Bound::MAX>(irs::boolean_token_stream::value(max.getBoolean()));
        range.include<irs::Bound::MAX>(maxInclude);
      }

      return {};
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE: {
      if (filter) {
        double_t minDblValue, maxDblValue;

        if (!min.getDouble(minDblValue) || !max.getDouble(maxDblValue)) {
          // can't parse value as double
          return {TRI_ERROR_BAD_PARAMETER, "can not get double parameter"};
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

      return {};
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_STRING: {
      if (filter) {
        irs::string_ref minStrValue, maxStrValue;

        if (!min.getString(minStrValue) || !max.getString(maxStrValue)) {
          // failed to get string value
          return {TRI_ERROR_BAD_PARAMETER, "failed to get string value"};
        }

        auto& range = filter->add<irs::by_range>();

        TRI_ASSERT(filterCtx.analyzer._pool);
        kludge::mangleStringField(name, filterCtx.analyzer);
        range.field(std::move(name));
        range.boost(filterCtx.boost);

        range.term<irs::Bound::MIN>(minStrValue);
        range.include<irs::Bound::MIN>(minInclude);
        range.term<irs::Bound::MAX>(maxStrValue);
        range.include<irs::Bound::MAX>(maxInclude);
      }

      return {};
    }
    default:
      // wrong value type
      return {TRI_ERROR_BAD_PARAMETER, "invalid value type"};
  }
}

template <irs::Bound Bound>
arangodb::Result byRange(irs::boolean_filter* filter, std::string name, const ScopedAqlValue& value,
                              bool const incl, QueryContext const& ctx, FilterContext const& filterCtx) {
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

      return {};
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        kludge::mangleBool(name);
        range.field(std::move(name));
        range.boost(filterCtx.boost);
        range.term<Bound>(irs::boolean_token_stream::value(value.getBoolean()));
        range.include<Bound>(incl);
      }

      return {};
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE: {
      if (filter) {
        double_t dblValue;

        if (!value.getDouble(dblValue)) {
          // can't parse as double
          return {TRI_ERROR_BAD_PARAMETER, "could not parse double value"};
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

      return {};
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_STRING: {
      if (filter) {
        irs::string_ref strValue;

        if (!value.getString(strValue)) {
          // can't parse as string
          return {TRI_ERROR_BAD_PARAMETER, "could not parse string value"};
        }

        auto& range = filter->add<irs::by_range>();

        TRI_ASSERT(filterCtx.analyzer._pool);
        kludge::mangleStringField(name, filterCtx.analyzer);
        range.field(std::move(name));
        range.boost(filterCtx.boost);
        range.term<Bound>(strValue);
        range.include<Bound>(incl);
      }

      return {};
    }
    default:
      // wrong value type
      return {TRI_ERROR_BAD_PARAMETER, "invalid value type"};
  }
}

template <irs::Bound Bound>
arangodb::Result byRange(irs::boolean_filter* filter,
             arangodb::iresearch::NormalizedCmpNode const& node, bool const incl,
             QueryContext const& ctx, FilterContext const& filterCtx) {
  TRI_ASSERT(node.attribute && node.attribute->isDeterministic());
  TRI_ASSERT(node.value && node.value->isDeterministic());

  std::string name;
  if (filter && !nameFromAttributeAccess(name, *node.attribute, ctx)) {
    auto message = "Failed to generate field name from node " + arangodb::aql::AstNode::toString(node.attribute);
    LOG_TOPIC("1a218", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }
  auto value = ScopedAqlValue(*node.value);
  if (!value.isConstant()) {
    if (!filter) {
      // can't evaluate non constant filter before the execution
      return {};
    }

    if (!value.execute(ctx)) {
      // could not execute expression
      return { TRI_ERROR_BAD_PARAMETER, "can not execute expression" };
    }
  }
  return byRange<Bound>(filter, name, value, incl, ctx, filterCtx);
}

arangodb::Result fromExpression(irs::boolean_filter* filter, QueryContext const& ctx,
                    FilterContext const& filterCtx,
                    std::shared_ptr<arangodb::aql::AstNode>&& node) {
  if (!filter) {
    return {};
  }

  // non-deterministic condition or self-referenced variable
  if (!node->isDeterministic() || arangodb::iresearch::findReference(*node, *ctx.ref)) {
    // not supported by IResearch, but could be handled by ArangoDB
    appendExpression(*filter, std::move(node), ctx, filterCtx);
    return {};
  }

  bool result;

  if (node->isConstant()) {
    result = node->isTrue();
  } else {  // deterministic expression
    ScopedAqlValue value(*node);

    if (!value.execute(ctx)) {
      // can't execute expression
      return {TRI_ERROR_BAD_PARAMETER, "can not execute expression"};
    }

    result = value.getBoolean();
  }

  if (result) {
    filter->add<irs::all>().boost(filterCtx.boost);
  } else {
    filter->add<irs::empty>();
  }

  return {};
}

arangodb::Result fromExpression(irs::boolean_filter* filter, QueryContext const& ctx,
                    FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  if (!filter) {
    return {};
  }

  // non-deterministic condition or self-referenced variable
  if (!node.isDeterministic() || arangodb::iresearch::findReference(node, *ctx.ref)) {
    // not supported by IResearch, but could be handled by ArangoDB
    appendExpression(*filter, node, ctx, filterCtx);
    return {};
  }

  bool result;

  if (node.isConstant()) {
    result = node.isTrue();
  } else {  // deterministic expression
    ScopedAqlValue value(node);

    if (!value.execute(ctx)) {
      // can't execute expression
      return {TRI_ERROR_BAD_PARAMETER, "can not execute expression"};
    }

    result = value.getBoolean();
  }

  if (result) {
    filter->add<irs::all>().boost(filterCtx.boost);
  } else {
    filter->add<irs::empty>();
  }

  return {};
}

arangodb::Result fromInterval(irs::boolean_filter* filter, QueryContext const& ctx,
                  FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == node.type);

  arangodb::iresearch::NormalizedCmpNode normNode;

  if (!arangodb::iresearch::normalizeCmpNode(node, *ctx.ref, normNode)) {
    return fromExpression(filter, ctx, filterCtx, node);
  }

  bool const incl = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == normNode.cmp ||
                    arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == normNode.cmp;

  bool const min = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == normNode.cmp ||
                   arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == normNode.cmp;

  return min ? byRange<irs::Bound::MIN>(filter, normNode, incl, ctx, filterCtx)
             : byRange<irs::Bound::MAX>(filter, normNode, incl, ctx, filterCtx);
}

arangodb::Result fromBinaryEq(irs::boolean_filter* filter, QueryContext const& ctx,
                  FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type);

  arangodb::iresearch::NormalizedCmpNode normalized;

  if (!arangodb::iresearch::normalizeCmpNode(node, *ctx.ref, normalized)) {
    auto rv = fromExpression(filter, ctx, filterCtx, node);
    return rv.reset(rv.errorNumber(), "in from binary equation" + rv.errorMessage());
  }

  irs::by_term* termFilter = nullptr;

  if (filter) {
    termFilter = &(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type
                       ? filter->add<irs::Not>().filter<irs::by_term>()
                       : filter->add<irs::by_term>());
  }

  return byTerm(termFilter, normalized, ctx, filterCtx);
}

arangodb::Result fromRange(irs::boolean_filter* filter, QueryContext const& /*ctx*/,
               FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_RANGE == node.type);

  if (node.numMembers() != 2) {
    logMalformedNode(node.type);
    return {TRI_ERROR_BAD_PARAMETER, "wrong number of arguments in range expression"};  // wrong number of members
  }

  // ranges are always true
  if (filter) {
    filter->add<irs::all>().boost(filterCtx.boost);
  }

  return {};
}

std::pair<arangodb::Result, arangodb::aql::AstNodeType> buildBinaryArrayComparisonPreFilter(
    irs::boolean_filter* &filter, arangodb::aql::AstNodeType arrayComparison,
    const arangodb::aql::AstNode* qualifierNode, size_t arraySize) {
  TRI_ASSERT(qualifierNode);
  auto qualifierType = qualifierNode->getIntValue(true);
  arangodb::aql::AstNodeType expansionNodeType = arangodb::aql::NODE_TYPE_ROOT;
  if (0 == arraySize) {
    expansionNodeType = arangodb::aql::NODE_TYPE_ROOT; // no subfilters expansion needed
    switch (qualifierType) {
      case arangodb::aql::Quantifier::ANY:
        if (filter) {
          filter->add<irs::empty>();
        }
        break;
      case arangodb::aql::Quantifier::ALL:
      case arangodb::aql::Quantifier::NONE:
        if (filter) {
          filter->add<irs::all>();
        }
        break;
      default:
        TRI_ASSERT(false); // new qualifier added ?
        return std::make_pair(
            arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED, "Unknown qualifier in Array comparison operator"),
            arangodb::aql::AstNodeType::NODE_TYPE_ROOT);
    }
  } else {
    // NONE is inverted ALL so do conversion
    if (arangodb::aql::Quantifier::NONE == qualifierType) {
      qualifierType = arangodb::aql::Quantifier::ALL;
      switch (arrayComparison) {
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
          arrayComparison = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN;
          break;
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
          arrayComparison = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN;
          break;
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
          arrayComparison = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT;
          break;
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
          arrayComparison = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE;
          break;
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
          arrayComparison = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT;
          break;
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
          arrayComparison = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE;
          break;
        default:
          TRI_ASSERT(false); // new array comparison operator?
          return std::make_pair(
              arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED, "Unknown Array NONE comparison operator"),
              arangodb::aql::AstNodeType::NODE_TYPE_ROOT);
      }
    }
    switch (qualifierType) {
      case arangodb::aql::Quantifier::ALL:
        // calculate node type for expanding operation
        // As soon as array is left argument but for filter we place document to the left
        // we reverse comparison operation
        switch (arrayComparison) {
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::And>());
            }
            expansionNodeType = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ;
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Not>().filter<irs::Or>());
            }
            expansionNodeType = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ;
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::And>());
            }
            expansionNodeType = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT;
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::And>());
            }
            expansionNodeType = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE;
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::And>());
            }
            expansionNodeType = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT;
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::And>());
            }
            expansionNodeType = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE;
            break;
          default:
            TRI_ASSERT(false); // new array comparison operator?
            return std::make_pair(
                arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED, "Unknown Array ALL/NONE comparison operator"),
                arangodb::aql::AstNodeType::NODE_TYPE_ROOT);
        }
        break;
      case arangodb::aql::Quantifier::ANY: {
        switch (arrayComparison) {
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Or>());
            }
            expansionNodeType = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ;
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Not>().filter<irs::And>());
            }
            expansionNodeType = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ;
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Or>());
            }
            expansionNodeType = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT;
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Or>());
            }
            expansionNodeType = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE;
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Or>());
            }
            expansionNodeType = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT;
            break;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Or>());
            }
            expansionNodeType = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE;
            break;
          default:
            TRI_ASSERT(false); // new array comparison operator?
            return std::make_pair(
                arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED, "Unknown Array ANY comparison operator"),
                arangodb::aql::AstNodeType::NODE_TYPE_ROOT);
        }
        break;
      }
      default:
        TRI_ASSERT(false); // new qualifier added ?
        return std::make_pair(
            arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED, "Unknown qualifier in Array comparison operator"),
            arangodb::aql::AstNodeType::NODE_TYPE_ROOT);
    }
  }
  return std::make_pair(TRI_ERROR_NO_ERROR, expansionNodeType);
}

class ByTermSubFilterFactory {
 public:
  static arangodb::Result byNodeSubFilter(irs::boolean_filter* filter,
                                          arangodb::iresearch::NormalizedCmpNode const& node,
                                          QueryContext const& ctx, FilterContext const& filterCtx) {
    TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ == node.cmp);
    iresearch::by_term* termFilter =  nullptr;
    if (filter) {
      termFilter = &filter->add<irs::by_term>();
    }
    return byTerm(termFilter, node, ctx, filterCtx);
  }

  static arangodb::Result byValueSubFilter(irs::boolean_filter* filter, std::string fieldName, const ScopedAqlValue& value,
                                           arangodb::aql::AstNodeType arrayExpansionNodeType,
                                           QueryContext const& ctx, FilterContext const& filterCtx) {
    TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ == arrayExpansionNodeType);
    iresearch::by_term* termFilter =  nullptr;
    if (filter) {
      termFilter = &filter->add<irs::by_term>();
    }
    return byTerm(termFilter, std::move(fieldName), value, ctx, filterCtx);
  }
};

class ByRangeSubFilterFactory {
 public:
  static arangodb::Result byNodeSubFilter(irs::boolean_filter* filter,
                                          arangodb::iresearch::NormalizedCmpNode const& node,
                                          QueryContext const& ctx, FilterContext const& filterCtx) {
    bool incl, min;
    std::tie(min, incl) = calcMinInclude(node.cmp);
    return min ? byRange<irs::Bound::MIN>(filter, node, incl, ctx, filterCtx)
               : byRange<irs::Bound::MAX>(filter, node, incl, ctx, filterCtx);
  }

  static arangodb::Result byValueSubFilter(irs::boolean_filter* filter, std::string fieldName, const ScopedAqlValue& value,
                                           arangodb::aql::AstNodeType arrayExpansionNodeType,
                                           QueryContext const& ctx, FilterContext const& filterCtx) {
    bool incl, min;
    std::tie(min, incl) = calcMinInclude(arrayExpansionNodeType);
    return min ? byRange<irs::Bound::MIN>(filter, fieldName, value, incl, ctx, filterCtx)
               : byRange<irs::Bound::MAX>(filter, fieldName, value, incl, ctx, filterCtx);
  }

 private:
  static std::pair<bool, bool> calcMinInclude(arangodb::aql::AstNodeType arrayExpansionNodeType) {
    TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == arrayExpansionNodeType ||
               arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == arrayExpansionNodeType ||
               arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == arrayExpansionNodeType ||
               arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == arrayExpansionNodeType);
    return std::pair<bool, bool>(
        // min
        arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == arrayExpansionNodeType ||
        arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == arrayExpansionNodeType,
        // incl
        arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == arrayExpansionNodeType ||
        arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == arrayExpansionNodeType);
  }
};


template<typename SubFilterFactory>
arangodb::Result fromArrayComparison(irs::boolean_filter*& filter, QueryContext const& ctx,
                                     FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN == node.type);
  if (node.numMembers() != 3) {
    auto rv = logMalformedNode(node.type);
    return rv.reset(rv.errorNumber(), "error in Array comparison operator " + rv.errorMessage());
  }

  auto const* valueNode = node.getMemberUnchecked(0);
  TRI_ASSERT(valueNode);

  auto const* attributeNode = node.getMemberUnchecked(1);
  TRI_ASSERT(attributeNode);

  auto const* qualifierNode = node.getMemberUnchecked(2);
  TRI_ASSERT(qualifierNode);

  if (qualifierNode->type != arangodb::aql::NODE_TYPE_QUANTIFIER) {
    return { TRI_ERROR_BAD_PARAMETER, "wrong qualifier node type for Array comparison operator" };
  }
    if (arangodb::aql::NODE_TYPE_ARRAY == valueNode->type) {
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
        attributeAccessFound |=
            (nullptr != arangodb::iresearch::checkAttributeAccess(valueNode->getMemberUnchecked(i),
                                                                *ctx.ref));
      }
      if (!attributeAccessFound) {
        return fromExpression(filter, ctx, filterCtx, node);
      }
    }
    arangodb::Result buildRes;
    arangodb::aql::AstNodeType arrayExpansionNodeType;
    std::tie(buildRes, arrayExpansionNodeType) = buildBinaryArrayComparisonPreFilter(filter, node.type, qualifierNode, n);
    if (!buildRes.ok()) {
      return buildRes;
    }
    if (filter) {
      filter->boost(filterCtx.boost);
    }
    if (arangodb::aql::NODE_TYPE_ROOT == arrayExpansionNodeType) {
      // nothing to do more
      return {};
    }
    FilterContext const subFilterCtx{
      filterCtx.analyzer,
      irs::no_boost() };  // reset boost
    // Expand array interval as several binaryInterval nodes ('array' feature is ensured by pre-filter)
    arangodb::iresearch::NormalizedCmpNode normalized;
    arangodb::aql::AstNode toNormalize(arrayExpansionNodeType);
    toNormalize.reserve(2);
    for (size_t i = 0; i < n; ++i) {
      auto const* member = valueNode->getMemberUnchecked(i);
      TRI_ASSERT(member);

      // edit in place for now; TODO change so we can replace instead
      TEMPORARILY_UNLOCK_NODE(&toNormalize);
      toNormalize.clearMembers();
      toNormalize.addMember(attributeNode);
      toNormalize.addMember(member);
      toNormalize.flags = member->flags;
      if (!arangodb::iresearch::normalizeCmpNode(toNormalize, *ctx.ref, normalized)) {
        if (!filter) {
          // can't evaluate non constant filter before the execution
          return {};
        }
        // use std::shared_ptr since AstNode is not copyable/moveable
        auto exprNode = std::make_shared<arangodb::aql::AstNode>(arrayExpansionNodeType);
        exprNode->reserve(2);
        exprNode->addMember(attributeNode);
        exprNode->addMember(member);

        // not supported by IResearch, but could be handled by ArangoDB
        auto rv = fromExpression(filter, ctx, subFilterCtx, std::move(exprNode));
        if (rv.fail()) {
          return rv.reset(rv.errorNumber(), "while getting array: " + rv.errorMessage());
        }
      } else {
        auto rv = SubFilterFactory::byNodeSubFilter(filter, normalized, ctx, subFilterCtx);
        if (rv.fail()) {
          return rv.reset(rv.errorNumber(), "while getting array: " + rv.errorMessage());
        }
      }
    }
    return {};
  }

  if (!node.isDeterministic() ||
      !arangodb::iresearch::checkAttributeAccess(attributeNode, *ctx.ref) ||
      arangodb::iresearch::findReference(*valueNode, *ctx.ref)) {
    return fromExpression(filter, ctx, filterCtx, node);
  }

  if (!filter) {
    // can't evaluate non constant filter before the execution
    return {};
  }

  ScopedAqlValue value(*valueNode);
  if (!value.execute(ctx)) {
    // can't execute expression
    auto message = "Unable to extract value from Array comparison operator";
    LOG_TOPIC("f7a13", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  switch (value.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_ARRAY: {
      size_t const n = value.size();
      arangodb::Result buildRes;
      arangodb::aql::AstNodeType arrayExpansionNodeType;
      std::tie(buildRes, arrayExpansionNodeType) = buildBinaryArrayComparisonPreFilter(filter, node.type, qualifierNode, n);
      if (!buildRes.ok()) {
        return buildRes;
      }
      filter->boost(filterCtx.boost);
      if (arangodb::aql::NODE_TYPE_ROOT == arrayExpansionNodeType) {
        // nothing to do more
        return {};
      }
      FilterContext const subFilterCtx{
          filterCtx.analyzer,
          irs::no_boost()  // reset boost
      };

      std::string fieldName;
      if (filter && !nameFromAttributeAccess(fieldName, *attributeNode, ctx)) {
        auto message = "Failed to generate field name from node " + arangodb::aql::AstNode::toString(attributeNode);
        LOG_TOPIC("ff299", WARN, arangodb::iresearch::TOPIC) << message;
        return { TRI_ERROR_BAD_PARAMETER, message };
      }
      for (size_t i = 0; i < n; ++i) {
        auto rv = SubFilterFactory::byValueSubFilter(filter, fieldName, value.at(i), arrayExpansionNodeType, ctx, subFilterCtx);
        if (rv.fail()) {
          return rv.reset(rv.errorNumber(), "failed to create filter because: " + rv.errorMessage());
        }
      }
      return {};
    }
    default:
      break;
  }

  // wrong value node type
  return {TRI_ERROR_BAD_PARAMETER, "wrong value node type for Array comparison operator"};
}

arangodb::Result fromInArray(irs::boolean_filter* filter, QueryContext const& ctx,
                 FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type);

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
      attributeAccessFound |=
          (nullptr != arangodb::iresearch::checkAttributeAccess(valueNode->getMemberUnchecked(i),
                                                                *ctx.ref));
    }

    if (!attributeAccessFound) {
      return fromExpression(filter, ctx, filterCtx, node);
    }
  }

  if (!n) {
    if (filter) {
      if (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
        filter->add<irs::all>().boost(filterCtx.boost);  // not in [] means 'all'
      } else {
        filter->add<irs::empty>();
      }
    }

    // nothing to do more
    return {};
  }

  if (filter) {
    filter = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type
                 ? &static_cast<irs::boolean_filter&>(
                       filter->add<irs::Not>().filter<irs::Or>())
                 : &static_cast<irs::boolean_filter&>(filter->add<irs::Or>());
    filter->boost(filterCtx.boost);
  }

  FilterContext const subFilterCtx{
      filterCtx.analyzer,
      irs::no_boost()  // reset boost
  };

  arangodb::iresearch::NormalizedCmpNode normalized;
  arangodb::aql::AstNode toNormalize(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ);
  toNormalize.reserve(2);

  // FIXME better to rewrite expression the following way but there is no place
  // to store created `AstNode` d.a IN [1,RAND(),'3'+RAND()] -> (d.a == 1) OR
  // d.a IN [RAND(),'3'+RAND()]

  for (size_t i = 0; i < n; ++i) {
    auto const* member = valueNode->getMemberUnchecked(i);
    TRI_ASSERT(member);

    // edit in place for now; TODO change so we can replace instead
    TEMPORARILY_UNLOCK_NODE(&toNormalize);
    toNormalize.clearMembers();
    toNormalize.addMember(attributeNode);
    toNormalize.addMember(member);
    toNormalize.flags = member->flags;  // attributeNode is deterministic here

    if (!arangodb::iresearch::normalizeCmpNode(toNormalize, *ctx.ref, normalized)) {
      if (!filter) {
        // can't evaluate non constant filter before the execution
        return {};
      }

      // use std::shared_ptr since AstNode is not copyable/moveable
      auto exprNode = std::make_shared<arangodb::aql::AstNode>(
          arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ);
      exprNode->reserve(2);
      exprNode->addMember(attributeNode);
      exprNode->addMember(member);

      // not supported by IResearch, but could be handled by ArangoDB
      auto rv = fromExpression(filter, ctx, subFilterCtx, std::move(exprNode));
      if (rv.fail()) {
        return rv.reset(rv.errorNumber(), "while getting array: " + rv.errorMessage());
      }
    } else {
      auto* termFilter = filter ? &filter->add<irs::by_term>() : nullptr;

      auto rv = byTerm(termFilter, normalized, ctx, subFilterCtx);
      if (rv.fail()) {
        return rv.reset(rv.errorNumber(), "while getting array: " + rv.errorMessage());
      }
    }
  }

  return {};
}

arangodb::Result fromIn(irs::boolean_filter* filter, QueryContext const& ctx,
            FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type);

  if (node.numMembers() != 2) {
    auto rv = logMalformedNode(node.type);
    return rv.reset(rv.errorNumber(), "error in from In" + rv.errorMessage());
  }

  auto const* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(valueNode);

  if (arangodb::aql::NODE_TYPE_ARRAY == valueNode->type) {
    return fromInArray(filter, ctx, filterCtx, node);
  }

  auto* attributeNode = node.getMemberUnchecked(0);
  TRI_ASSERT(attributeNode);

  if (!node.isDeterministic() ||
      !arangodb::iresearch::checkAttributeAccess(attributeNode, *ctx.ref) ||
      arangodb::iresearch::findReference(*valueNode, *ctx.ref)) {
    return fromExpression(filter, ctx, filterCtx, node);
  }

  if (!filter) {
    // can't evaluate non constant filter before the execution
    return {};
  }

  if (arangodb::aql::NODE_TYPE_RANGE == valueNode->type) {
    ScopedAqlValue value(*valueNode);

    if (!value.execute(ctx)) {
      auto message = "Unable to extract value from 'IN' operator";
      // con't execute expression
      LOG_TOPIC("f43d1", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }

    // range
    auto const* range = value.getRange();
    if (!range) {
      return {TRI_ERROR_BAD_PARAMETER, "no valid range"};
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
    auto message = "Unable to extract value from 'IN' operator";
    LOG_TOPIC("dafaa", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  switch (value.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_ARRAY: {
      size_t const n = value.size();

      if (!n) {
        if (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
          filter->add<irs::all>().boost(filterCtx.boost);  // not in [] means 'all'
        } else {
          filter->add<irs::empty>();
        }

        // nothing to do more
        return {};
      }

      filter = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type
                   ? &static_cast<irs::boolean_filter&>(
                         filter->add<irs::Not>().filter<irs::Or>())
                   : &static_cast<irs::boolean_filter&>(filter->add<irs::Or>());
      filter->boost(filterCtx.boost);

      FilterContext const subFilterCtx{
          filterCtx.analyzer,
          irs::no_boost()  // reset boost
      };

      for (size_t i = 0; i < n; ++i) {
        // failed to create a filter
        auto rv = byTerm(&filter->add<irs::by_term>(), *attributeNode, value.at(i), ctx, subFilterCtx);
        if (rv.fail()) {
          return rv.reset(rv.errorNumber(), "failed to create filter because: " + rv.errorMessage());
        }
      }

      return {};
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_RANGE: {
      // range
      auto const* range = value.getRange();

      if (!range) {
        return {TRI_ERROR_BAD_PARAMETER, "no valid range"};
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
  return {TRI_ERROR_BAD_PARAMETER, "wrong value node type"};
}

arangodb::Result fromNegation(irs::boolean_filter* filter, QueryContext const& ctx,
                  FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_UNARY_NOT == node.type);

  if (node.numMembers() != 1) {
    auto rv = logMalformedNode(node.type);
    return rv.reset(rv.errorNumber(), "Bad node in negation" + rv.errorMessage());
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
      irs::no_boost()  // reset boost
  };

  return ::filter(filter, ctx, subFilterCtx, *member);
}

/*
bool rangeFromBinaryAnd(irs::boolean_filter* filter, QueryContext const& ctx,
                        FilterContext const& filterCtx,
                        arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND == node.type);

  if (node.numMembers() != 2) {
    logMalformedNode(node.type);
    return false;  // wrong number of members
  }

  auto const* lhsNode = node.getMemberUnchecked(0);
  TRI_ASSERT(lhsNode);
  auto const* rhsNode = node.getMemberUnchecked(1);
  TRI_ASSERT(rhsNode);

  arangodb::iresearch::NormalizedCmpNode lhsNormNode, rhsNormNode;

  if (arangodb::iresearch::normalizeCmpNode(*lhsNode, *ctx.ref, lhsNormNode) &&
      arangodb::iresearch::normalizeCmpNode(*rhsNode, *ctx.ref, rhsNormNode)) {
    bool const lhsInclude =
        arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == lhsNormNode.cmp;
    bool const rhsInclude =
        arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == rhsNormNode.cmp;

    if ((lhsInclude ||
         arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == lhsNormNode.cmp) &&
        (rhsInclude ||
         arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == rhsNormNode.cmp)) {
      auto const* lhsAttr = lhsNormNode.attribute;
      auto const* rhsAttr = rhsNormNode.attribute;

      if (arangodb::iresearch::attributeAccessEqual(lhsAttr, rhsAttr, filter ? &ctx : nullptr)) {
        auto const* lhsValue = lhsNormNode.value;
        auto const* rhsValue = rhsNormNode.value;

        if (byRange(filter, *lhsAttr, *lhsValue, lhsInclude, *rhsValue,
                    rhsInclude, ctx, filterCtx)) {
          // successsfully parsed as range
          return true;
        }
      }
    }
  }

  // unable to create range
  return false;
}
*/

template <typename Filter>
arangodb::Result fromGroup(irs::boolean_filter* filter, QueryContext const& ctx,
               FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_OR == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_NARY_OR == node.type);

  size_t const n = node.numMembers();

  if (!n) {
    // nothing to do
    return {};
  }

  // Note: cannot optimize for single member in AND/OR since 'a OR NOT b'
  // translates to 'a OR (OR NOT b)'

  if (filter) {
    filter = &filter->add<Filter>();
    filter->boost(filterCtx.boost);
  }

  FilterContext const subFilterCtx{
      filterCtx.analyzer,
      irs::no_boost()  // reset boost
  };

  for (size_t i = 0; i < n; ++i) {
    auto const* valueNode = node.getMemberUnchecked(i);
    TRI_ASSERT(valueNode);

    auto rv = ::filter(filter, ctx, subFilterCtx, *valueNode);
    if (rv.fail()) {
      auto node = arangodb::aql::AstNode::toString(valueNode);
      //return rv.reset(rv.errorNumber(), "error checking subNodes in node: " + node + ": " + rv.errorMessage());
      //probably too much for the user
      return rv;
    }
  }

  return {};
}

// Analyze(<filter-expression>, analyzer)
arangodb::Result fromFuncAnalyzer(irs::boolean_filter* filter, QueryContext const& ctx,
                      FilterContext const& filterCtx, arangodb::aql::AstNode const& args) {
  auto const argc = args.numMembers();

  if (argc != 2) {
    auto message = "'ANALYZER' AQL function: Invalid number of arguments passed (must be 2)";
    LOG_TOPIC("9bc36", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  // 1st argument defines filter expression
  auto expressionArg = args.getMemberUnchecked(0);

  if (!expressionArg) {
    auto message = "'ANALYZER' AQL function: 1st argument is invalid"s;
    LOG_TOPIC("7c828", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  // 2nd argument defines a boost
  auto const analyzerArg = args.getMemberUnchecked(1);

  if (!analyzerArg) {
    auto message = "'ANALYZER' AQL function: 2nd argument is invalid"s;
    LOG_TOPIC("d9b1c", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  if (!analyzerArg->isDeterministic()) {
    auto message = "'ANALYZER' AQL function: 2nd argument is intended to be deterministic"s;
    LOG_TOPIC("f7ad1", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  ScopedAqlValue analyzerId(*analyzerArg);
  arangodb::iresearch::FieldMeta::Analyzer analyzerValue; // default analyzer
  auto& analyzer = analyzerValue._pool;
  auto& shortName = analyzerValue._shortName;

  if (filter || analyzerId.isConstant()) {
    if (!analyzerId.execute(ctx)) {
      auto message = "'ANALYZER' AQL function: Failed to evaluate 2nd argument"s;
      LOG_TOPIC("f361f", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != analyzerId.type()) {
      auto message =
          "'ANALYZER' AQL function: 'analyzer' argument has invalid type '"s +
          std::string(ScopedAqlValue::typeString(analyzerId.type())) +
          "' (string expected)";
      LOG_TOPIC("624cc", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
    }

    irs::string_ref analyzerIdValue;

    if (!analyzerId.getString(analyzerIdValue)) {
      auto message = "'ANALYZER' AQL function: Unable to parse 2nd argument as string"s;
      LOG_TOPIC("0c41f", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
    }

    auto& server = arangodb::application_features::ApplicationServer::server();
    if (!server.hasFeature<IResearchAnalyzerFeature>()) {
      auto message =
          "'"s + IResearchAnalyzerFeature::name() +
          "' feature is not registered, unable to evaluate 'ANALYZER' function";
      LOG_TOPIC("26571", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, message};
    }
    auto& analyzerFeature = server.getFeature<IResearchAnalyzerFeature>();

    shortName = analyzerIdValue;

    if (ctx.trx) {
      auto& server = arangodb::application_features::ApplicationServer::server();
      auto sysVocbase =
          server.hasFeature<arangodb::SystemDatabaseFeature>()
              ? server.getFeature<arangodb::SystemDatabaseFeature>().use()
              : nullptr;

      if (sysVocbase) {
        analyzer = analyzerFeature.get(analyzerIdValue, ctx.trx->vocbase(), *sysVocbase);

        shortName = arangodb::iresearch::IResearchAnalyzerFeature::normalize( // normalize
          analyzerIdValue, ctx.trx->vocbase(), *sysVocbase, false); // args
      }
    } else {
      analyzer = analyzerFeature.get(analyzerIdValue);  // verbatim
    }

    if (!analyzer) {
      auto message = "'ANALYZER' AQL function: Unable to lookup analyzer '"s + analyzerIdValue.c_str() + "'"s;
      LOG_TOPIC("404c9", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
    }
  }

  FilterContext const subFilterContext(analyzerValue, filterCtx.boost); // override analyzer

  auto rv = ::filter(filter, ctx, subFilterContext, *expressionArg);
  if (rv.fail()) {
    return arangodb::Result{rv.errorNumber(), "failed to get filter for analyzer: " + analyzer->name() + " : " + rv.errorMessage()};
  }
  return rv;
}

// BOOST(<filter-expression>, boost)
arangodb::Result fromFuncBoost(irs::boolean_filter* filter, QueryContext const& ctx,
                   FilterContext const& filterCtx, arangodb::aql::AstNode const& args) {
  auto const argc = args.numMembers();

  if (argc != 2) {
    auto message = "'BOOST' AQL function: Invalid number of arguments passed (must be 2)";
    LOG_TOPIC("c22fa", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  // 1st argument defines filter expression
  auto expressionArg = args.getMemberUnchecked(0);

  if (!expressionArg) {
    auto message = "'BOOST' AQL function: 1st argument is invalid";
    LOG_TOPIC("f8c16", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  // 2nd argument defines a boost
  auto const boostArg = args.getMemberUnchecked(1);

  if (!boostArg) {
    auto message = "'BOOST' AQL function: 2nd argument is invalid";
    LOG_TOPIC("f2d0d", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  if (!boostArg->isDeterministic()) {
    auto message = "'BOOST' AQL function: 2nd argument is intended to be deterministic";
    LOG_TOPIC("6c133", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  ScopedAqlValue boost(*boostArg);
  double_t boostValue = 0;

  if (filter || boost.isConstant()) {
    if (!boost.execute(ctx)) {
      auto message = "'BOOST' AQL function: Failed to evaluate 2nd argument";
      LOG_TOPIC("82c3b", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE != boost.type()) {
      auto message = "'BOOST' AQL function: 2nd argument has invalid type '"s + ScopedAqlValue::typeString(boost.type()).c_str() + "' (double expected)"s;
      LOG_TOPIC("1a742", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
    }

    if (!boost.getDouble(boostValue)) {
      auto message = "'BOOST' AQL function: Failed to parse 2nd argument as string";
      LOG_TOPIC("53ba8", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
    }
  }

  FilterContext const subFilterContext{filterCtx.analyzer,
                                       filterCtx.boost * static_cast<float_t>(boostValue)};

  auto rv = ::filter(filter, ctx, subFilterContext, *expressionArg);
  if (rv.fail()) {
    return arangodb::Result{rv.errorNumber(), "error in sub-filter context: " + rv.errorMessage()};
  }
  return {};
}

// EXISTS(<attribute>, <"analyzer">, <"analyzer-name">)
// EXISTS(<attribute>, <"string"|"null"|"bool"|"numeric">)
arangodb::Result fromFuncExists(irs::boolean_filter* filter, QueryContext const& ctx,
                    FilterContext const& filterCtx, arangodb::aql::AstNode const& args) {
  if (!args.isDeterministic()) {
    auto message = "Unable to handle non-deterministic arguments for 'EXISTS' function";
    LOG_TOPIC("20cf9", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  auto const argc = args.numMembers();

  if (argc < 1 || argc > 3) {
    auto message = "'EXISTS' AQL function: Invalid number of arguments passed (must be >= 1 and <= 3)";
    LOG_TOPIC("90b23", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  // 1st argument defines a field
  auto const* fieldArg =
      arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref);

  if (!fieldArg) {
    auto message = "'EXISTS' AQL function: 1st argument is invalid";
    LOG_TOPIC("509c2", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  bool prefixMatch = true;
  std::string fieldName;
  auto analyzer = filterCtx.analyzer;

  if (filter && !arangodb::iresearch::nameFromAttributeAccess(fieldName, *fieldArg, ctx)) {
    auto message = "'EXISTS' AQL function: Failed to generate field name from the 1st argument";
    LOG_TOPIC("9c179", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  if (argc > 1) {
    // 2nd argument defines a type (if present)
    auto const typeArg = args.getMemberUnchecked(1);

    if (!typeArg) {
      auto message = "'EXISTS' AQL function: 2nd argument is invalid";
      LOG_TOPIC("d9ed2", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
    }

    irs::string_ref arg;
    ScopedAqlValue type(*typeArg);

    if (filter || type.isConstant()) {
      if (!type.execute(ctx)) {
        auto message = "'EXISTS' AQL function: Failed to evaluate 2nd argument";
        LOG_TOPIC("3d773", WARN, arangodb::iresearch::TOPIC) << message;
        return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
      }

      if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != type.type()) {
        auto message = "'EXISTS' AQL function: 2nd argument has invalid type '"s + ScopedAqlValue::typeString(type.type()).c_str() + "' (string expected)";
        LOG_TOPIC("0e630", WARN, arangodb::iresearch::TOPIC) << message;
        return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
      }

      if (!type.getString(arg)) {
        auto message = "'EXISTS' AQL function: Failed to parse 2nd argument as string";
        LOG_TOPIC("2e7a9", WARN, arangodb::iresearch::TOPIC) << message;
        return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
      }

      std::string strArg(arg);
      arangodb::basics::StringUtils::tolowerInPlace(strArg);  // normalize user input
      irs::string_ref const TypeAnalyzer("analyzer");

      typedef bool (*TypeHandler)(std::string&, arangodb::iresearch::FieldMeta::Analyzer const&);

      static std::map<irs::string_ref, TypeHandler> const TypeHandlers{
          // any string
          {irs::string_ref("string"),
           [](std::string& name, arangodb::iresearch::FieldMeta::Analyzer const&)->bool {
             kludge::mangleAnalyzer(name);
             return true;  // a prefix match
           }},
          // any non-string type
          {irs::string_ref("type"),
           [](std::string& name, arangodb::iresearch::FieldMeta::Analyzer const&)->bool {
             kludge::mangleType(name);
             return true;  // a prefix match
           }},
          // concrete analyzer from the context
          {TypeAnalyzer,
           [](std::string& name, arangodb::iresearch::FieldMeta::Analyzer const& analyzer)->bool {
             kludge::mangleStringField(name, analyzer);
             return false;  // not a prefix match
           }},
          {irs::string_ref("numeric"),
           [](std::string& name, arangodb::iresearch::FieldMeta::Analyzer const&)->bool {
             kludge::mangleNumeric(name);
             return false;  // not a prefix match
           }},
          {irs::string_ref("bool"),
           [](std::string& name, arangodb::iresearch::FieldMeta::Analyzer const&)->bool {
             kludge::mangleBool(name);
             return false;  // not a prefix match
           }},
          {irs::string_ref("boolean"),
           [](std::string& name, arangodb::iresearch::FieldMeta::Analyzer const&)->bool {
             kludge::mangleBool(name);
             return false;  // not a prefix match
           }},
          {irs::string_ref("null"),
           [](std::string& name, arangodb::iresearch::FieldMeta::Analyzer const&)->bool {
             kludge::mangleNull(name);
             return false;  // not a prefix match
           }}};

      auto const typeHandler = TypeHandlers.find(strArg);

      if (TypeHandlers.end() == typeHandler) {
        auto message =
            "'EXISTS' AQL function: 2nd argument must be equal to one of the following: 'string', 'type', 'analyzer', 'numeric', 'bool', 'boolean', 'null', but got '"s +
            arg.c_str() + "'";
        LOG_TOPIC("96e61", WARN, arangodb::iresearch::TOPIC) << message;
        return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
      }

      if (argc > 2) {
        if (TypeAnalyzer.c_str() != typeHandler->first.c_str()) {
          auto message = "'EXISTS' AQL function: 3rd argument is intended to use with 'analyzer' type only";
          LOG_TOPIC("d69e2", WARN, arangodb::iresearch::TOPIC) << message;
          return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
        }

        analyzer = extractAnalyzerFromArg(filter, args.getMemberUnchecked(2),
                                          ctx, 2, "EXISTS");

        if (!analyzer._pool) {
          return arangodb::Result{TRI_ERROR_INTERNAL, "analyzer not found"};
        }
      }

      prefixMatch = typeHandler->second(fieldName, analyzer);
    }
  }

  if (filter) {
    auto& exists = filter->add<irs::by_column_existence>();
    exists.field(std::move(fieldName));
    exists.boost(filterCtx.boost);
    exists.prefix_match(prefixMatch);
  }

  return {};
}

// MIN_MATCH(<filter-expression>[, <filter-expression>,...], <min-match-count>)
arangodb::Result fromFuncMinMatch(irs::boolean_filter* filter, QueryContext const& ctx,
                      FilterContext const& filterCtx, arangodb::aql::AstNode const& args) {
  auto const argc = args.numMembers();

  if (argc < 2) {
    auto message = "'MIN_MATCH' AQL function: Invalid number of arguments passed (must be >= 2)";
    LOG_TOPIC("6c8d4", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  // ...........................................................................
  // last argument defines min match count
  // ...........................................................................

  auto const lastArg = argc - 1;
  auto minMatchCountArg = args.getMemberUnchecked(lastArg);

  if (!minMatchCountArg) {
    auto message = "'MIN_MATCH' AQL function: " + std::to_string(lastArg) + " argument is invalid";
    LOG_TOPIC("eea57", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  if (!minMatchCountArg->isDeterministic()) {
    auto message = "'MIN_MATCH' AQL function: "s + std::to_string(lastArg) + " argument is intended to be deterministic"s;
    LOG_TOPIC("b58bc", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  ScopedAqlValue minMatchCount(*minMatchCountArg);
  int64_t minMatchCountValue = 0;

  if (filter || minMatchCount.isConstant()) {
    if (!minMatchCount.execute(ctx)) {
      auto message = "'MIN_MATCH' AQL function: Failed to evaluate "s + std::to_string(lastArg) + " argument"s;
      LOG_TOPIC("2c964", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE != minMatchCount.type()) {
      auto message = "'MIN_MATCH' AQL function: "s + std::to_string(lastArg) + " argument has invalid type '"s + ScopedAqlValue::typeString(minMatchCount.type()).c_str() + "' (numeric expected)"s;
      LOG_TOPIC("21df2", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
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

  FilterContext const subFilterCtx{
      filterCtx.analyzer,
      irs::no_boost()  // reset boost
  };

  for (size_t i = 0; i < lastArg; ++i) {
    auto subFilterExpression = args.getMemberUnchecked(i);

    if (!subFilterExpression) {
      auto message = "'MIN_MATCH' AQL function: Failed to evaluate " + std::to_string(i) + " argument";
      LOG_TOPIC("77f47", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
    }

    irs::boolean_filter* subFilter = filter ? &filter->add<irs::Or>() : nullptr;

    auto rv = ::filter(subFilter, ctx, subFilterCtx, *subFilterExpression);
    if (rv.fail()) {
      auto message = "'MIN_MATCH' AQL function: Failed to instantiate sub-filter for " + std::to_string(i) + " argument" + rv.errorMessage();
      LOG_TOPIC("79498", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
    }
  }

  return {};
}


arangodb::Result processPhraseArgs(
    irs::by_phrase* phrase, QueryContext const& ctx,
    FilterContext const& filterCtx, arangodb::aql::AstNode const& valueArgs,
    size_t valueArgsBegin, size_t valueArgsEnd, irs::analysis::analyzer::ptr& analyzer,
    size_t offset, bool allowDefaultOffset, bool allowRecursion) {
  irs::string_ref value;
  bool expectingOffset = false;
  for (size_t idx = valueArgsBegin; idx < valueArgsEnd; ++idx) {
    auto currentArg = valueArgs.getMemberUnchecked(idx);
    if (!currentArg) {
      auto message = "'PHRASE' AQL function: Unable to parse argument on position "s + std::to_string(idx);
      LOG_TOPIC("44bed", WARN, arangodb::iresearch::TOPIC) << message;
      return { TRI_ERROR_BAD_PARAMETER, message };
    }
    if (currentArg->isArray() && (!expectingOffset || allowDefaultOffset)) {
      // array arg is processed with possible default 0 offsets - to be easily compatible with TOKENS function
      // No array recursion allowed. This could be allowed, but just looks tangled.
      // Anyone interested coud use FLATTEN  to explicitly require processing all recurring arrays as one array
      if (allowRecursion) {
        auto subRes = processPhraseArgs(phrase, ctx, filterCtx, *currentArg, 0, currentArg->numMembers(), analyzer, offset, true, false);
        if (subRes.fail()) {
          return subRes;
        }
        expectingOffset = true;
        offset = 0;
        continue;
      } else {
        auto message = "'PHRASE' AQL function: recursive arrays not allowed at position "s + std::to_string(idx);
        LOG_TOPIC("66c24", WARN, arangodb::iresearch::TOPIC) << message;
        return { TRI_ERROR_BAD_PARAMETER, message };
      }
    }
    ScopedAqlValue currentValue(*currentArg);
    if (phrase || currentValue.isConstant()) {
      if (!currentValue.execute(ctx)) {
        auto message = "'PHRASE' AQL function: Unable to parse argument on position " + std::to_string(idx);
        LOG_TOPIC("d819d", WARN, arangodb::iresearch::TOPIC) << message;
        return { TRI_ERROR_BAD_PARAMETER, message };
      }
      if (arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE == currentValue.type() && expectingOffset) {
        offset = static_cast<uint64_t>(currentValue.getInt64());
        expectingOffset = false;
        continue; // got offset let`s go search for value
      } else if ( (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != currentValue.type() || !currentValue.getString(value)) || // value is not a string at all
                  (expectingOffset && !allowDefaultOffset)) { // offset is expected mandatory but got value
        std::string expectedValue;
        if (expectingOffset && allowDefaultOffset) {
          expectedValue = " as a value or offset";
        } else if (expectingOffset) {
          expectedValue = " as an offset";
        } else {
          expectedValue = " as a value";
        }
        auto message = "'PHRASE' AQL function: Unable to parse argument on position " + std::to_string(idx) + expectedValue;
        LOG_TOPIC("ac06b", WARN, arangodb::iresearch::TOPIC) << message;
        return { TRI_ERROR_BAD_PARAMETER, message };
      }
    } else {
      // in case of non const node encountered while parsing we can not decide if current and following args are correct before execution
      // so at this stage we say all is ok
      return {};
    }
    if (phrase) {
      TRI_ASSERT(analyzer);
      appendTerms(*phrase, value, *analyzer, offset);
    }
    offset = 0;
    expectingOffset = true;
  }
  if (!expectingOffset) { // that means last arg is numeric - this is error as no term to apply offset to
    auto message = "'PHRASE' AQL function : Unable to parse argument on position " + std::to_string(valueArgsEnd - 1) +  "as a value"s;
    LOG_TOPIC("5fafe", WARN, arangodb::iresearch::TOPIC) << message;
    return { TRI_ERROR_BAD_PARAMETER, message };
  }
  return {};
}

// note: <value> could be either string ether array of strings with offsets inbetween . Inside array
// 0 offset could be omitted e.g. [term1, term2, 2, term3] is equal to: [term1, 0, term2, 2, term3]
// PHRASE(<attribute>, <value> [, <offset>, <value>, ...] [, <analyzer>])
// PHRASE(<attribute>, '[' <value> [, <offset>, <value>, ...] ']' [,<analyzer>])
arangodb::Result fromFuncPhrase(irs::boolean_filter* filter, QueryContext const& ctx,
                    FilterContext const& filterCtx, arangodb::aql::AstNode const& args) {
  if (!args.isDeterministic()) {
    auto message = "Unable to handle non-deterministic arguments for 'PHRASE' function"s;
    LOG_TOPIC("df524", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};  // nondeterministic
  }

  auto argc = args.numMembers();

  if (argc < 2) {
    auto message = "'PHRASE' AQL function: Invalid number of arguments passed (must be >= 2)";
    LOG_TOPIC("4368f", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  // ...........................................................................
  // last odd argument defines an analyzer
  // ...........................................................................

  auto analyzerPool = filterCtx.analyzer;

  if (0 != (argc & 1)) {  // override analyzer
    --argc;

    analyzerPool = extractAnalyzerFromArg(filter, args.getMemberUnchecked(argc),
                                          ctx, argc, "PHRASE");

    if (!analyzerPool._pool) {
      return {TRI_ERROR_BAD_PARAMETER};
    }
  }

  // ...........................................................................
  // 1st argument defines a field
  // ...........................................................................

  auto const* fieldArg =
      arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref);

  if (!fieldArg) {
    auto message = "'PHRASE' AQL function: 1st argument is invalid";
    LOG_TOPIC("335b3", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  // ...........................................................................
  // 2nd argument and later defines a values
  // ...........................................................................
  auto* valueArgs = &args;
  size_t valueArgsBegin = 1;
  size_t valueArgsEnd = argc;

  irs::by_phrase* phrase = nullptr;
  irs::analysis::analyzer::ptr analyzer;
  // prepare filter if execution phase
  if (filter) {
    std::string name;

    if (!arangodb::iresearch::nameFromAttributeAccess(name, *fieldArg, ctx)) {
      auto message = "'PHRASE' AQL function: Failed to generate field name from the 1st argument";
      LOG_TOPIC("3b1e4", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }

    TRI_ASSERT(analyzerPool._pool);
    analyzer = analyzerPool._pool->get();

    if (!analyzer) {
      auto message = "'PHRASE' AQL function: Unable to instantiate analyzer '"s + analyzerPool._pool->name() + "'";
      LOG_TOPIC("4d142", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_INTERNAL, message};
    }

    kludge::mangleStringField(name, analyzerPool);

    phrase = &filter->add<irs::by_phrase>();
    phrase->field(std::move(name));
    phrase->boost(filterCtx.boost);
  }
  // on top level we require explicit offsets - to be backward compatible and be able to distinguish last argument as analyzer or value
  // Also we allow recursion inside array to support older syntax (one array arg) and add ability to pass several arrays as args
  return processPhraseArgs(phrase, ctx, filterCtx, *valueArgs, valueArgsBegin, valueArgsEnd, analyzer, 0, false, true);
}

// STARTS_WITH(<attribute>, <prefix>, [<scoring-limit>])
arangodb::Result fromFuncStartsWith(irs::boolean_filter* filter, QueryContext const& ctx,
                        FilterContext const& filterCtx,
                        arangodb::aql::AstNode const& args) {
  if (!args.isDeterministic()) {
    auto message = "Unable to handle non-deterministic arguments for 'STARTS_WITH' function";
    LOG_TOPIC("f2851", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  auto const argc = args.numMembers();

  if (argc < 2 || argc > 3) {
    auto message = "'STARTS_WITH' AQL function: Invalid number of arguments passed (should be >= 2 and <= 3)";
    LOG_TOPIC("b157e", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  // 1st argument defines a field
  auto const* field =
      arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref);

  if (!field) {
    auto message = "'STARTS_WITH' AQL function: Unable to parse 1st argument as an attribute identifier";
    LOG_TOPIC("4d7a8", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  // 2nd argument defines a value
  auto const* prefixArg = args.getMemberUnchecked(1);

  if (!prefixArg) {
    auto message = "'STARTS_WITH' AQL function: 2nd argument is invalid";
    LOG_TOPIC("6f500", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  irs::string_ref prefix;
  size_t scoringLimit = 128;  // FIXME make configurable

  ScopedAqlValue prefixValue(*prefixArg);

  if (filter || prefixValue.isConstant()) {
    if (!prefixValue.execute(ctx)) {
      auto message = "'STARTS_WITH' AQL function: Failed to evaluate 2nd argument";
      LOG_TOPIC("e196c", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != prefixValue.type()) {
      auto message = "'STARTS_WITH' AQL function: 2nd argument has invalid type '"s + ScopedAqlValue::typeString(prefixValue.type()).c_str() + "' (string expected)";
      LOG_TOPIC("bd9c8", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }

    if (!prefixValue.getString(prefix)) {
      auto message = "'STARTS_WITH' AQL function: Unable to parse 2nd argument as string";
      LOG_TOPIC("e4ebc", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }
  }

  if (argc > 2) {
    // 3rd (optional) argument defines a number of scored terms
    auto const* scoringLimitArg = args.getMemberUnchecked(2);

    if (!scoringLimitArg) {
      auto message = "'STARTS_WITH' AQL function: 3rd argument is invalid";
      LOG_TOPIC("f67b7", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }

    ScopedAqlValue scoringLimitValue(*scoringLimitArg);

    if (filter || scoringLimitValue.isConstant()) {
      if (!scoringLimitValue.execute(ctx)) {
        auto message = "'STARTS_WITH' AQL function: Failed to evaluate 3rd argument";
        LOG_TOPIC("1c334", WARN, arangodb::iresearch::TOPIC) << message;
        return {TRI_ERROR_BAD_PARAMETER, message};
      }

      if (arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE != scoringLimitValue.type()) {
        auto message = "'STARTS_WITH' AQL function: 3rd argument has invalid type '"s + ScopedAqlValue::typeString(scoringLimitValue.type()).c_str() + "' (numeric expected)";
        LOG_TOPIC("40130", WARN, arangodb::iresearch::TOPIC) << message;
        return {TRI_ERROR_BAD_PARAMETER, message};
      }

      scoringLimit = static_cast<size_t>(scoringLimitValue.getInt64());
    }
  }

  if (filter) {
    std::string name;

    if (!nameFromAttributeAccess(name, *field, ctx)) {
      auto message = "'STARTS_WITH' AQL function: Failed to generate field name from the 1st argument";
      LOG_TOPIC("91862", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }

    TRI_ASSERT(filterCtx.analyzer);
    kludge::mangleStringField(name, filterCtx.analyzer);

    auto& prefixFilter = filter->add<irs::by_prefix>();
    prefixFilter.scored_terms_limit(scoringLimit);
    prefixFilter.field(std::move(name));
    prefixFilter.boost(filterCtx.boost);
    prefixFilter.term(prefix);
  }

  return {};
}

// IN_RANGE(<attribute>, <low>, <high>, <include-low>, <include-high>)
arangodb::Result fromFuncInRange(irs::boolean_filter* filter, QueryContext const& ctx,
                     FilterContext const& filterCtx,
                     arangodb::aql::AstNode const& args) {
  if (!args.isDeterministic()) {
    auto message = "Unable to handle non-deterministic arguments for 'IN_RANGE' function";
    LOG_TOPIC("dff45", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message}; // nondeterministic
  }

  auto const argc = args.numMembers();

  if (argc != 5) {
    auto message = "'IN_RANGE' AQL function: Invalid number of arguments passed (should be 5)";
    LOG_TOPIC("2f5a8", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  // 1st argument defines a field
  auto const* field =
      arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref);

  if (!field) {
    auto message = "'IN_RANGE' AQL function: Unable to parse 1st argument as an attribute identifier";
    LOG_TOPIC("7c56a", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  ScopedAqlValue includeValue;
  auto getInclusion = [&ctx, filter, &includeValue](
      arangodb::aql::AstNode const* arg,
      bool& include,
      irs::string_ref const& argName) -> arangodb::Result {
    if (!arg) {
      auto message = "'IN_RANGE' AQL function: "s + argName.c_str() + " argument is invalid"s;
      LOG_TOPIC("8ec00", WARN, arangodb::iresearch::TOPIC) << message;
      return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
    }

    includeValue.reset(*arg);

    if (filter || includeValue.isConstant()) {
      if (!includeValue.execute(ctx)) {
        auto message = "'IN_RANGE' AQL function: Failed to evaluate "s + argName.c_str() + " argument"s;
        LOG_TOPIC("32f3b", WARN, arangodb::iresearch::TOPIC) << message;
        return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
      }

      if (arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL != includeValue.type()) {
        std::string type = ScopedAqlValue::typeString(includeValue.type());
        auto message = "'IN_RANGE' AQL function: "s + std::string(argName.c_str()) + " argument has invalid type '" + type + "' (boolean expected)";
        LOG_TOPIC("57a29", WARN, arangodb::iresearch::TOPIC) << message;
        return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
      }

      include = includeValue.getBoolean();
    }

    return {};
  };

  // 2nd argument defines a lower boundary
  auto const* lhsArg = args.getMemberUnchecked(1);

  if (!lhsArg) {
    auto message = "'IN_RANGE' AQL function: 2nd argument is invalid";
    LOG_TOPIC("f1167", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  // 3rd argument defines an upper boundary
  auto const* rhsArg = args.getMemberUnchecked(2);

  if (!rhsArg) {
    auto message = "'IN_RANGE' AQL function: 3rd argument is invalid";
    LOG_TOPIC("d5fe6", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  // 4th argument defines inclusion of lower boundary
  bool lhsInclude = false;
  auto inc1 = getInclusion(args.getMemberUnchecked(3), lhsInclude, "4th");
  if (inc1.fail()) {
    return inc1;
  }

  // 5th argument defines inclusion of upper boundary
  bool rhsInclude = false;
  auto inc2 = getInclusion(args.getMemberUnchecked(4), rhsInclude, "5th");
  if (inc2.fail()) {
    return inc2;
  }

  auto rv = ::byRange(filter, *field, *lhsArg, lhsInclude, *rhsArg, rhsInclude, ctx, filterCtx);
  if (rv.fail()) {
    return {rv.errorNumber(), "error in byRange: " + rv.errorMessage()};
  }
  return {};
}

// LIKE(<attribute>, <pattern>)
arangodb::Result fromFuncLike(
    irs::boolean_filter* filter, QueryContext const& ctx,
    FilterContext const& filterCtx, arangodb::aql::AstNode const& args) {
  if (!args.isDeterministic()) {
    auto message = "Unable to handle non-deterministic arguments for 'LIKE' function";
    LOG_TOPIC("dff15", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message}; // nondeterministic
  }

  auto const argc = args.numMembers();

  if (argc != 2) {
    auto message = "'LIKE' AQL function: Invalid number of arguments passed (should be 2)";
    LOG_TOPIC("2f1a8", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  // 1st argument defines a field
  auto const* field =
      arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref);

  if (!field) {
    auto message = "'LIKE' AQL function: Unable to parse 1st argument as an attribute identifier";
    LOG_TOPIC("7c51a", WARN, arangodb::iresearch::TOPIC) << message;
    return arangodb::Result{TRI_ERROR_BAD_PARAMETER, message};
  }

  // 2nd argument defines a matching pattern
  auto const* patternArg = args.getMemberUnchecked(1);

  if (!patternArg) {
    auto message = "'LIKE' AQL function: 2nd argument is invalid";
    LOG_TOPIC("6f501", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  irs::string_ref pattern;
  const size_t scoringLimit = 128;  // FIXME make configurable

  ScopedAqlValue patternValue(*patternArg);

  if (filter || patternValue.isConstant()) {
    if (!patternValue.execute(ctx)) {
      auto message = "'LIKE' AQL function: Failed to evaluate 2nd argument";
      LOG_TOPIC("e196b", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }

    if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING != patternValue.type()) {
      auto message = "'LIKE' AQL function: 2nd argument has invalid type '"s + ScopedAqlValue::typeString(patternValue.type()).c_str() + "' (string expected)";
      LOG_TOPIC("bd9c1", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }

    if (!patternValue.getString(pattern)) {
      auto message = "'LIKE' AQL function: Unable to parse 2nd argument as string";
      LOG_TOPIC("e1ebc", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }
  }

  if (filter) {
    std::string name;

    if (!nameFromAttributeAccess(name, *field, ctx)) {
      auto message = "'LIKE' AQL function: Failed to generate field name from the 1st argument";
      LOG_TOPIC("91861", WARN, arangodb::iresearch::TOPIC) << message;
      return {TRI_ERROR_BAD_PARAMETER, message};
    }

    TRI_ASSERT(filterCtx.analyzer);
    kludge::mangleStringField(name, filterCtx.analyzer);

    auto& wildcardFilter = filter->add<irs::by_wildcard>();
    wildcardFilter.scored_terms_limit(scoringLimit);
    wildcardFilter.field(std::move(name));
    wildcardFilter.boost(filterCtx.boost);
    wildcardFilter.term(pattern);
  }

  return {};
}

std::map<irs::string_ref, ConvertionHandler> const FCallUserConvertionHandlers;

arangodb::Result fromFCallUser(irs::boolean_filter* filter, QueryContext const& ctx,
                   FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL_USER == node.type);

  if (node.numMembers() != 1) {
    return logMalformedNode(node.type);
  }

  auto const* args = arangodb::iresearch::getNode(node, 0, arangodb::aql::NODE_TYPE_ARRAY);

  if (!args) {
    auto message = "Unable to parse user function arguments as an array'"s;
    LOG_TOPIC("457fe", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  irs::string_ref name;

  if (!arangodb::iresearch::parseValue(name, node)) {
    auto message ="Unable to parse user function name"s;
    LOG_TOPIC("3ca23", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  auto const entry = FCallUserConvertionHandlers.find(name);

  if (entry == FCallUserConvertionHandlers.end()) {
    return fromExpression(filter, ctx, filterCtx, node);
  }

  if (!args->isDeterministic()) {
    auto message = "Unable to handle non-deterministic function '"s + std::string(name.c_str(), name.size()) + "' arguments";
    LOG_TOPIC("b2265", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};
  }

  return entry->second(filter, ctx, filterCtx, *args);
}

std::map<std::string, ConvertionHandler> const FCallSystemConvertionHandlers{
    {"PHRASE", fromFuncPhrase},     {"STARTS_WITH", fromFuncStartsWith},
    {"EXISTS", fromFuncExists},     {"BOOST", fromFuncBoost},
    {"ANALYZER", fromFuncAnalyzer}, {"MIN_MATCH", fromFuncMinMatch},
    {"IN_RANGE", fromFuncInRange},  {"LIKE", fromFuncLike }
};

arangodb::Result fromFCall(irs::boolean_filter* filter, QueryContext const& ctx,
               FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL == node.type);

  auto const* fn = static_cast<arangodb::aql::Function*>(node.getData());

  if (!fn || node.numMembers() != 1) {
    return logMalformedNode(node.type);
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
    auto message = "Unable to parse arguments of system function '"s + fn->name + "' as an array'";
    LOG_TOPIC("ed878", WARN, arangodb::iresearch::TOPIC) << message;
    return {TRI_ERROR_BAD_PARAMETER, message};  // invalid args
  }

  return entry->second(filter, ctx, filterCtx, *args);
}

arangodb::Result fromFilter(irs::boolean_filter* filter, QueryContext const& ctx,
                FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FILTER == node.type);

  if (node.numMembers() != 1) {
    logMalformedNode(node.type);
    return {TRI_ERROR_BAD_PARAMETER, "wrong number of parameters"};  // wrong number of members
  }

  auto const* member = node.getMemberUnchecked(0);

  if (member) {
    return ::filter(filter, ctx, filterCtx, *member);
  } else {
    return {TRI_ERROR_INTERNAL, "could not get node member"};  // wrong number of members
  }
}

arangodb::Result filter(irs::boolean_filter* filter, QueryContext const& queryCtx,
            FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  switch (node.type) {
    case arangodb::aql::NODE_TYPE_FILTER:  // FILTER
      return fromFilter(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_VARIABLE:  // variable
      return fromExpression(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_UNARY_NOT:  // unary minus
      return fromNegation(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND:  // logical and
      return fromGroup<irs::And>(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_OR:  // logical or
      return fromGroup<irs::Or>(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:  // compare ==
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE:  // compare !=
      return fromBinaryEq(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:  // compare <
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:  // compare <=
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:  // compare >
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:  // compare >=
      return fromInterval(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN:   // compare in
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN:  // compare not in
      return fromIn(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_TERNARY:  // ternary
    case arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS:  // attribute access
    case arangodb::aql::NODE_TYPE_VALUE:             // value
    case arangodb::aql::NODE_TYPE_ARRAY:             // array
    case arangodb::aql::NODE_TYPE_OBJECT:            // object
    case arangodb::aql::NODE_TYPE_REFERENCE:         // reference
    case arangodb::aql::NODE_TYPE_PARAMETER:         // bind parameter
      return fromExpression(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_FCALL:  // function call
      return fromFCall(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_FCALL_USER:  // user function call
      return fromFCallUser(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_RANGE:  // range
      return fromRange(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND:  // n-ary and
      return fromGroup<irs::And>(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_NARY_OR:  // n-ary or
      return fromGroup<irs::Or>(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:   // compare ARRAY in
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:  // compare ARRAY not in
    // for iresearch filters IN and EQ queries will be actually the same
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:  // compare ARRAY ==
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:  // compare ARRAY !=
      return fromArrayComparison<ByTermSubFilterFactory>(filter, queryCtx, filterCtx, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:  // compare ARRAY <
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:  // compare ARRAY <=
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:  // compare ARRAY >
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:  // compare ARRAY >=
      return fromArrayComparison<ByRangeSubFilterFactory>(filter, queryCtx, filterCtx, node);
    default:
      return fromExpression(filter, queryCtx, filterCtx, node);
  }
}

}  // namespace

namespace arangodb {
namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                      FilerFactory implementation
// ----------------------------------------------------------------------------

/*static*/ arangodb::Result FilterFactory::filter(irs::boolean_filter* filter, QueryContext const& ctx,
                                      arangodb::aql::AstNode const& node) {
  // The analyzer is referenced in the FilterContext and used during the
  // following ::filter() call, so may not be a temporary.
  FieldMeta::Analyzer analyzer = FieldMeta::Analyzer();
  FilterContext const filterCtx(analyzer, irs::no_boost());

  return ::filter(filter, ctx, filterCtx, node);
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
