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
#include "Aql/Expression.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExpressionContext.h"

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

typedef typename std::underlying_type<
  arangodb::aql::AstNodeValueType
>::type AstNodeValueTypeUnderlyingType;

struct AqlValueTraits {
  constexpr static arangodb::aql::AstNodeValueType invalid_type() noexcept {
    typedef typename std::underlying_type<
      arangodb::aql::AstNodeValueType
    >::type underlying_t;

    return arangodb::aql::AstNodeValueType(
      irs::integer_traits<underlying_t>::const_max
    );
  }

  static arangodb::aql::AstNodeValueType type(
      arangodb::aql::AqlValue const& value
  ) noexcept {
    auto const typeIndex = value.isNull(false)
      + 2*value.isBoolean()
      + 3*value.isNumber()
      + 4*value.isString();

    return TYPE_MAP[typeIndex];
  }

  static arangodb::aql::AstNodeValueType type(
    arangodb::aql::AstNode const& node
  ) noexcept {
    if (arangodb::aql::NODE_TYPE_VALUE != node.type) {
      return invalid_type();
    }

    return node.isNumericValue()
      ? arangodb::aql::VALUE_TYPE_DOUBLE // all numerics are doubles here
      : node.value.type;
  }

  static arangodb::aql::AstNodeValueType const TYPE_MAP[];
}; // AqlValueTraits

arangodb::aql::AstNodeValueType const AqlValueTraits::TYPE_MAP[] {
  AqlValueTraits::invalid_type(),
  arangodb::aql::VALUE_TYPE_NULL,
  arangodb::aql::VALUE_TYPE_BOOL,
  arangodb::aql::VALUE_TYPE_DOUBLE,
  arangodb::aql::VALUE_TYPE_STRING
};

static_assert(
  AqlValueTraits::invalid_type() > arangodb::aql::VALUE_TYPE_STRING,
  "Invalid enum value"
);

////////////////////////////////////////////////////////////////////////////////
/// @class ScopedAqlValue
/// @brief convenient wrapper around `AqlValue` and `AstNode`
////////////////////////////////////////////////////////////////////////////////
class ScopedAqlValue : private irs::util::noncopyable {
 public:
  explicit ScopedAqlValue(arangodb::aql::AstNode const& node) noexcept
    : _node(&node), _type(AqlValueTraits::type(node)) {
  }

  ~ScopedAqlValue() noexcept {
    if (_destroy) {
      _value.destroy();
    }
  }

  bool isConstant() const noexcept {
    return _node->isConstant();
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief executes expression specified in the given `node`
  /// @returns true if expression has been executed, false otherwise
  ////////////////////////////////////////////////////////////////////////////////
  bool execute(arangodb::iresearch::QueryContext const& ctx) {
    if (_node->isConstant()) {
      // constant expression, nothing to do
      return true;
    }

    if (!ctx.ast) { // || !ctx.ctx) {
      // can't execute expression without `AST` and `ExpressionContext`
      return false;
    }

    TRI_ASSERT(ctx.ctx); //FIXME remove, uncomment condition

    // don't really understand why we need `ExecutionPlan` and `Ast` here
    arangodb::aql::Expression expr(
      ctx.plan, ctx.ast, const_cast<arangodb::aql::AstNode*>(_node)
    );

    try {
      _value = expr.execute(ctx.trx, ctx.ctx, _destroy);
    } catch (arangodb::basics::Exception const& e) {
      // can't execute expression
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << e.message();
      return false;
    } catch (...) {
      // can't execute expression
      return false;
    }

    _type = AqlValueTraits::type(_value);
    return true;
  }

  arangodb::aql::AstNodeValueType type() const noexcept {
    return _type;
  }

  bool getBoolean() const {
    return _node->isConstant()
      ? _node->getBoolValue()
      : _value.toBoolean();
  }

  bool getDouble(double_t& value) const {
    bool failed = false;
    value = _node->isConstant()
      ? _node->getDoubleValue()
      : _value.toDouble(nullptr, failed);
    return !failed;
  }

  bool getString(irs::string_ref& value) const {
    if (_node->isConstant()) {
      return arangodb::iresearch::parseValue(value, *_node);
    } else {
      value = arangodb::iresearch::getStringRef(_value.slice());
    }
    return true;
  }

 private:
  arangodb::aql::AqlValue _value;
  arangodb::aql::AstNode const* _node;
  arangodb::aql::AstNodeValueType _type;
  bool _destroy{ false };
}; // ScopedAqlValue

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

bool byTerm(
    irs::by_term* filter,
    arangodb::iresearch::NormalizedCmpNode const& node,
    arangodb::iresearch::QueryContext const& ctx
) {
  TRI_ASSERT(node.value && node.attribute);

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

  switch (value.type()) {
    case arangodb::aql::VALUE_TYPE_NULL:
      if (filter) {
        auto name = arangodb::iresearch::nameFromAttributeAccess(*node.attribute);
        arangodb::iresearch::kludge::mangleNull(name);
        filter->field(std::move(name));
        filter->term(irs::null_token_stream::value_null());
      } return true;
    case arangodb::aql::VALUE_TYPE_BOOL:
      if (filter) {
        auto name = arangodb::iresearch::nameFromAttributeAccess(*node.attribute);
        arangodb::iresearch::kludge::mangleBool(name);
        filter->field(std::move(name));
        filter->term(irs::boolean_token_stream::value(value.getBoolean()));
      } return true;
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE:
      if (filter) {
        double_t dblValue;

        if (!value.getDouble(dblValue)) {
          // something gone wrong
          return false;
        }

        auto name = arangodb::iresearch::nameFromAttributeAccess(*node.attribute);
        arangodb::iresearch::kludge::mangleNumeric(name);

        irs::numeric_token_stream stream;
        irs::term_attribute const* term = stream.attributes().get<irs::term_attribute>().get();
        TRI_ASSERT(term);
        stream.reset(dblValue);
        stream.next();

        filter->field(std::move(name));
        filter->term(term->value());
     } return true;
    case arangodb::aql::VALUE_TYPE_STRING:
      if (filter) {
        irs::string_ref strValue;

        if (!value.getString(strValue)) {
          // something gone wrong
          return false;
        }

        auto name = arangodb::iresearch::nameFromAttributeAccess(*node.attribute);
        arangodb::iresearch::kludge::mangleStringField(
           name,
           arangodb::iresearch::IResearchAnalyzerFeature::identity()
        );

        filter->field(std::move(name));
        filter->term(strValue);
    } return true;
  }

  // unsupported value type
  return false;
}

bool byPrefix(
    irs::boolean_filter* filter,
    arangodb::iresearch::NormalizedCmpNode const& node,
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const* scoringLimitNode) {
  size_t scoringLimit = 128; // FIXME make configurable

  if (scoringLimitNode && !arangodb::iresearch::parseValue(scoringLimit, *scoringLimitNode)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'STARTS_WITH' AQL function: Unable to parse scoring limit, default value "
                                             << scoringLimit << " is going to be used";
    return false;
  }

  TRI_ASSERT(node.value);
  auto const& valueNode = *node.value;

  switch (valueNode.value.type) {
    case arangodb::aql::VALUE_TYPE_NULL:
    case arangodb::aql::VALUE_TYPE_BOOL:
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE:
      break;
    case arangodb::aql::VALUE_TYPE_STRING: {
      if (filter) {
        auto& prefixFilter = filter->add<irs::by_prefix>();
        prefixFilter.scored_terms_limit(scoringLimit);
        return byTerm(&prefixFilter, node, ctx);
      }
      return true;
    }
  }

  try {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'STARTS_WITH' AQL function: Unable to parse specified value '" << valueNode.toString()
        << "' as string";
  } catch (...) {
    // valueNode.toString() may throw
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'STARTS_WITH' AQL function: Unable to parse specified value as string";
  }

  return false;
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

  switch (min.type()) {
    case arangodb::aql::VALUE_TYPE_NULL: {
      if (filter) {
        auto name = arangodb::iresearch::nameFromAttributeAccess(attributeNode);
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
    case arangodb::aql::VALUE_TYPE_BOOL: {
      if (filter) {
        auto name = arangodb::iresearch::nameFromAttributeAccess(attributeNode);
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
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE: {
      if (filter) {
        double_t minDblValue, maxDblValue;

        if (!min.getDouble(minDblValue) || !max.getDouble(maxDblValue)) {
          // can't parse value as double
          return false;
        }

        auto& range = filter->add<irs::by_granular_range>();

        auto name = arangodb::iresearch::nameFromAttributeAccess(attributeNode);
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
    case arangodb::aql::VALUE_TYPE_STRING: {
      if (filter) {
        irs::string_ref minStrValue, maxStrValue;

        if (!min.getString(minStrValue) || !max.getString(maxStrValue)) {
          // failed to get string value
          return false;
        }

        auto& range = filter->add<irs::by_range>();

        auto name = arangodb::iresearch::nameFromAttributeAccess(attributeNode);
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
  }

  return false;
}

template<irs::Bound Bound>
bool byRange(
    irs::boolean_filter* filter,
    arangodb::iresearch::NormalizedCmpNode const& node,
    bool const incl,
    arangodb::iresearch::QueryContext const& ctx
) {
  TRI_ASSERT(node.attribute && node.value);

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

  switch (value.type()) {
    case arangodb::aql::VALUE_TYPE_NULL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        auto name = arangodb::iresearch::nameFromAttributeAccess(*node.attribute);
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

        auto name = arangodb::iresearch::nameFromAttributeAccess(*node.attribute);
        arangodb::iresearch::kludge::mangleBool(name);
        range.field(std::move(name));
        range.term<Bound>(
          irs::boolean_token_stream::value(value.getBoolean())
        );
        range.include<Bound>(incl);
      }

      return true;
    }
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE: {
      if (filter) {
        double_t dblValue;

        if (!value.getDouble(dblValue)) {
          // can't parse as double
          return false;
        }

        auto& range = filter->add<irs::by_granular_range>();
        irs::numeric_token_stream stream;

        auto name = arangodb::iresearch::nameFromAttributeAccess(*node.attribute);
        arangodb::iresearch::kludge::mangleNumeric(name);
        range.field(std::move(name));

        stream.reset(dblValue);
        range.insert<Bound>(stream);
        range.include<Bound>(incl);
      }

      return true;
    }
    case arangodb::aql::VALUE_TYPE_STRING: {
      if (filter) {
        irs::string_ref strValue;

        if (!value.getString(strValue)) {
          // can't parse as string
          return false;
        }

        auto& range = filter->add<irs::by_range>();

        auto name = arangodb::iresearch::nameFromAttributeAccess(*node.attribute);
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
  }

  // invalid value type
  return false;
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
    // FIXME no reference to IResearch variable here -> wrap expression

    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to normalize operator '=='";
    return false; // unable to normalize node
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

  auto const* attributeNode = arangodb::iresearch::checkAttributeAccess(
    node.getMemberUnchecked(0), *ctx.ref
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

    arangodb::iresearch::NormalizedCmpNode normalized {
      attributeNode, nullptr, arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ
    };

    for (size_t i = 0; i < n; ++i) {
      TRI_ASSERT(valueNode->getMemberUnchecked(i));
      normalized.value = valueNode->getMemberUnchecked(i);

      if (!byTerm((filter  ? &filter->add<irs::by_term>() : nullptr), normalized, ctx)) {
        // failed to create a filter
        return false;
      }
    }

    return true;
  } else if (arangodb::aql::NODE_TYPE_RANGE == valueNode->type) { // inclusive range
    if (n != 2) {
      logMalformedNode(valueNode->type);
      return false; // wrong range
    }

    auto const* minValueNode = valueNode->getMemberUnchecked(0);

    if (!minValueNode) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to parse left bound of the RANGE node";
      return false; // wrong left node
    }

    auto const* maxValueNode = valueNode->getMemberUnchecked(1);

    if (!maxValueNode) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to parse right bound of the RANGE node";
      return false; // wrong right node
    }

    if (filter && arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
      // handle negation
      filter = &filter->add<irs::Not>().filter<irs::Or>();
    }

    return byRange(filter, *attributeNode, *minValueNode, true, *maxValueNode, true, ctx);
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

bool fromExpression(
    irs::boolean_filter* filter,
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& node
) {
  if (!filter) {
    return true;
  }

  bool result;

  if (node.isConstant()) {
    result = node.isTrue();
  } else {
    ScopedAqlValue value(node);

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

      if (arangodb::iresearch::attributeAccessEqual(lhsAttr, rhsAttr)) {
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
  auto const argc = args.numMembers();

  if (argc < 1 || argc > 3) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'EXISTS' AQL function: Invalid number of arguments passed (must be >= 1 and <= 3)";
    return false;
  }

  // 1st argument defines a field
  auto const* fieldArg = arangodb::iresearch::checkAttributeAccess(
    args.getMemberUnchecked(0), *ctx.ref
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
    auto const typeArgNode = args.getMemberUnchecked(1);

    if (!typeArgNode) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'EXISTS' AQL function: 2nd argument is invalid";
    }

    ScopedAqlValue type(*typeArgNode);

    if (!type.execute(ctx)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'EXISTS' AQL function: Failed to evaluate 2nd argument";
      return false;
    }

    if (arangodb::aql::VALUE_TYPE_STRING != type.type()) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "'EXISTS' AQL function: 2nd argument has invalid type '" << type.type()
        << "' (string expected)";
      return false;
    }

    irs::string_ref arg;

    if (!type.getString(arg)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'EXISTS' AQL function: Failed to parse 2nd argument as string";
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
      auto const analyzerArgNode = args.getMemberUnchecked(2);

      if (!analyzerArgNode) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'EXISTS' AQL function: 3rd argument is invalid";
      }

      ScopedAqlValue analyzerId(*analyzerArgNode);

      if (!analyzerId.execute(ctx)) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'EXISTS' AQL function: Failed to evaluate 3rd argument";
        return false;
      }

      if (!analyzerId.getString(arg)) {
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
    arangodb::iresearch::QueryContext const& ctx,
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
    args.getMemberUnchecked(0), *ctx.ref
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
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& args
) {
  auto const argc = args.numMembers();

  if (argc < 2 || argc > 3) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "'STARTS_WITH' AQL function: Invalid number of arguments passed (should be >= 2 and <= 3)";
    return false;
  }

  // 1st argument defines a field
  auto const* field = arangodb::iresearch::checkAttributeAccess(
    args.getMemberUnchecked(0), *ctx.ref
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

  arangodb::iresearch::NormalizedCmpNode const node {
    field, prefix, arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ
  };

  return byPrefix(filter, node, ctx, scoringLimit);
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
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Unable to parse system function arguments as an array'";
    return false; // invalid args
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
    arangodb::iresearch::QueryContext const& ctx,
    arangodb::aql::AstNode const& node
) {
  switch (node.type) {
    case arangodb::aql::NODE_TYPE_FILTER: // FILTER
      return fromFilter(filter, ctx, node);
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
      break;
    case arangodb::aql::NODE_TYPE_VALUE : // value
    case arangodb::aql::NODE_TYPE_ARRAY:  // array
    case arangodb::aql::NODE_TYPE_OBJECT: // object
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
