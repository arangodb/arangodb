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
#include "search/levenshtein_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/term_filter.hpp"
#include "search/wildcard_filter.hpp"
#include "search/ngram_similarity_filter.hpp"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/Function.h"
#include "Aql/Quantifier.h"
#include "Aql/Range.h"
#include "Basics/StringUtils.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/IResearchPDP.h"
#include "Logger/LogMacros.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Transaction/Methods.h"

using namespace arangodb::iresearch;
using namespace std::literals::string_literals;

namespace {

namespace error {

template<size_t Min, size_t Max>
struct Range {
  static constexpr size_t MIN = Min;
  static constexpr size_t MAX = Max;
};

template<typename>
struct isRange : std::false_type { };
template<size_t Min, size_t Max>
struct isRange<Range<Min, Max>> : std::true_type { };

template<bool MaxBound, size_t Value>
struct OpenRange {
  static constexpr bool MAX_BOUND = MaxBound;
  static constexpr size_t VALUE = Value;
};

template<typename>
struct isOpenRange : std::false_type { };
template<bool MaxBound, size_t Value>
struct isOpenRange<OpenRange<MaxBound, Value>> : std::true_type { };

template<size_t Value>
struct ExactValue {
  static constexpr size_t VALUE = Value;
};

template<typename>
struct isExactValue : std::false_type { };
template<size_t Value>
struct isExactValue<ExactValue<Value>> : std::true_type { };

template<typename RangeType>
arangodb::Result invalidArgsCount(char const* funcName) {
  if constexpr (isRange<RangeType>::value) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function: Invalid number of arguments passed (expected >= ")
               .append(std::to_string(RangeType::MIN)).append(" and <= ").append(std::to_string(RangeType::MAX)).append(")")
    };
  } else if constexpr (isOpenRange<RangeType>::value) {
    if constexpr (RangeType::MAX_BOUND) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "'"s.append(funcName).append("' AQL function: Invalid number of arguments passed (expected <= ")
                             .append(std::to_string(RangeType::VALUE)).append(")")
      };
    }

    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function: Invalid number of arguments passed (expected >= ")
                           .append(std::to_string(RangeType::VALUE)).append(")")
    };
  } else if constexpr (isExactValue<RangeType>::value) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function: Invalid number of arguments passed (expected ")
               .append(std::to_string(RangeType::VALUE)).append(")")
    };
  }

  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: Invalid number of arguments passed")
  };
}

arangodb::Result negativeNumber(char const* funcName, size_t i) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: argument at position '")
        .append(std::to_string(i)).append("' must be a positive number")
  };
}

arangodb::Result nondeterministicArgs(char const* funcName) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "Unable to handle non-deterministic arguments for '"s
        .append(funcName).append("' function")
  };
}

arangodb::Result nondeterministicArg(char const* funcName, size_t i) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: argument at position '")
        .append(std::to_string(i)).append("' is intended to be dterministic")
  };
}

arangodb::Result invalidAttribute(char const* funcName, size_t i) {
  return {
    TRI_ERROR_BAD_PARAMETER,
     "'"s.append(funcName).append("' AQL function: Unable to parse argument at position '")
         .append(std::to_string(i)).append("' as an attribute identifier")
  };
}

arangodb::Result invalidArgument(char const* funcName, size_t i) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: argument at position '")
        .append(std::to_string(i)).append("' is invalid")
  };
}

arangodb::Result failedToEvaluate(const char* funcName, size_t i) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: Failed to evaluate argument at position '")
        .append(std::to_string(i)).append(("'"))
  };
}

arangodb::Result typeMismatch(const char* funcName, size_t i,
                              ScopedValueType expectedType,
                              ScopedValueType actualType) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: argument at position '").append(std::to_string(i))
             .append("' has invalid type '").append(ScopedAqlValue::typeString(actualType).c_str())
             .append("' ('").append(ScopedAqlValue::typeString(expectedType).c_str()).append("' expected)")
  };
}

arangodb::Result failedToParse(char const* funcName, size_t i, ScopedValueType expectedType) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: Unable to parse argument at position '")
        .append(std::to_string(i)).append("' as ").append(ScopedAqlValue::typeString(expectedType).c_str())
  };
}

arangodb::Result failedToGenerateName(char const* funcName, size_t i) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: Failed to generate field name from the argument at position '")
        .append(std::to_string(i)).append("'")
  };
}

arangodb::Result malformedNode(arangodb::aql::AstNodeType type) {
  auto const* typeName = arangodb::iresearch::getNodeTypeName(type);

  std::string message("Can't process malformed AstNode of type '");
  if (typeName) {
    message += *typeName;
  } else {
    message += std::to_string(type);
  }
  message += "'";

  return {TRI_ERROR_BAD_PARAMETER, message};
}

}

template<typename T, bool CheckDeterminism = false>
arangodb::Result evaluateArg(
    T& out, ScopedAqlValue& value,
    char const* funcName,
    arangodb::aql::AstNode const& args,
    size_t i,
    bool isFilter,
    QueryContext const& ctx) {
  static_assert(
    std::is_same<T, irs::string_ref>::value ||
    std::is_same<T, int64_t>::value ||
    std::is_same<T, double_t>::value ||
    std::is_same<T, bool>::value);

  auto const* arg = args.getMemberUnchecked(i);

  if (!arg) {
    return error::invalidArgument(funcName, 2);
  }

  if constexpr (CheckDeterminism) {
    if (!arg->isDeterministic()) {
      return error::nondeterministicArg(funcName, i);
    }
  }

  value.reset(*arg);

  if (isFilter || value.isConstant()) {
    if (!value.execute(ctx)) {
      return error::failedToEvaluate(funcName, i + 1);
    }

    ScopedValueType expectedType = ScopedValueType::SCOPED_VALUE_TYPE_INVALID;
    if constexpr (std::is_same<T, irs::string_ref>::value) {
      expectedType = arangodb::iresearch::SCOPED_VALUE_TYPE_STRING;
    } else if constexpr (std::is_same<T, int64_t>::value || std::is_same<T, double_t>::value) {
      expectedType = arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE;
    } else if constexpr (std::is_same<T, bool>::value) {
      expectedType = arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL;
    }

    if (expectedType != value.type()) {
      return error::typeMismatch(funcName, i + 1, expectedType, value.type());
    }

    if constexpr (std::is_same<T, irs::string_ref>::value) {
      if (!value.getString(out)) {
        return error::failedToParse(funcName, i + 1, expectedType);
      }
    } else if constexpr (std::is_same<T, int64_t>::value) {
      out = value.getInt64();
    } else if constexpr (std::is_same<T, double>::value) {
      if (!value.getDouble(out)) {
        return error::failedToParse(funcName, i + 1, expectedType);
      }
    } else if constexpr (std::is_same<T, bool>::value) {
      out = value.getBoolean();
    }
  }

  return {};
}

arangodb::Result getAnalyzerByName(
    arangodb::iresearch::FieldMeta::Analyzer& out,
    const irs::string_ref& analyzerId,
    char const* funcName,
    QueryContext const& ctx) {
  auto& analyzer = out._pool;
  auto& shortName = out._shortName;

  TRI_ASSERT(ctx.trx);
  auto& server = ctx.trx->vocbase().server();
  if (!server.hasFeature<IResearchAnalyzerFeature>()) {
    return {
      TRI_ERROR_INTERNAL,
      "'"s.append(IResearchAnalyzerFeature::name())
          .append("' feature is not registered, unable to evaluate '")
          .append(funcName).append("' function")
    };
  }
  auto& analyzerFeature = server.getFeature<IResearchAnalyzerFeature>();

  auto sysVocbase = server.hasFeature<arangodb::SystemDatabaseFeature>()
    ? server.getFeature<arangodb::SystemDatabaseFeature>().use()
    : nullptr;

  if (sysVocbase) {
    analyzer = analyzerFeature.get(analyzerId, ctx.trx->vocbase(), *sysVocbase);

    shortName = arangodb::iresearch::IResearchAnalyzerFeature::normalize(  // normalize
      analyzerId, ctx.trx->vocbase(), *sysVocbase, false);  // args
  }

  if (!analyzer) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append("' AQL function: Unable to load requested analyzer '")
          .append(analyzerId.c_str(), analyzerId.size()).append("'")
    };
  }

  return {};
}

arangodb::Result extractAnalyzerFromArg(
    arangodb::iresearch::FieldMeta::Analyzer& out,
    char const* funcName,
    irs::boolean_filter const* filter,
    arangodb::aql::AstNode const& args,
    size_t i,
    QueryContext const& ctx) {
  auto const* analyzerArg = args.getMemberUnchecked(i);

  if (!analyzerArg) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function: ")
          .append(std::to_string(i + 1)).append(" argument is invalid analyzer")
    };
  }

  ScopedAqlValue analyzerValue(*analyzerArg);
  irs::string_ref analyzerId;

  auto rv = evaluateArg(analyzerId, analyzerValue, funcName, args, i, filter, ctx);

  if (rv.fail()) {
    return rv;
  }

  if (!filter && !analyzerValue.isConstant()) {
    return {};
  }

  return getAnalyzerByName(out, analyzerId, funcName, ctx);
}

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
  arangodb::Result(char const* funcName,
                   irs::boolean_filter*,
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
    filter.push_back(irs::by_phrase::simple_term{token.value()}, firstOffset);
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
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Failed to generate field name from node "s.append(arangodb::aql::AstNode::toString(&attribute))
    };
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
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Failed to generate field name from node "s.append(arangodb::aql::AstNode::toString(&attribute))
    };
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
      return {
        TRI_ERROR_BAD_PARAMETER,
        "Failed to evaluate lower boundary from node '"s
          .append(arangodb::aql::AstNode::toString(&minValueNode)).append("'")
      };
    }
  }

  ScopedAqlValue max(maxValueNode);

  if (!max.isConstant()) {
    if (!filter) {
      // can't evaluate non constant filter before the execution
      return {};
    }

    if (!max.execute(ctx)) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "Failed to evaluate upper boundary from node '"s
          .append(arangodb::aql::AstNode::toString(&maxValueNode)).append("'")
      };
    }
  }

  if (min.type() != max.type()) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Failed to build range query, lower boundary '"s
          .append(arangodb::aql::AstNode::toString(&minValueNode))
          .append("' mismatches upper boundary '")
          .append(arangodb::aql::AstNode::toString(&maxValueNode)).append("'")
    };
  }

  std::string name;

  if (filter && !nameFromAttributeAccess(name, attributeNode, ctx)) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Failed to generate field name from node "s.append(arangodb::aql::AstNode::toString(&attributeNode))
    };
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
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Failed to generate field name from node "s.append(arangodb::aql::AstNode::toString(node.attribute))
    };
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
    auto rv = error::malformedNode(node.type);
    return rv.reset(TRI_ERROR_BAD_PARAMETER, "wrong number of arguments in range expression: " + rv.errorMessage());
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
    auto rv = error::malformedNode(node.type);
    return rv.reset(rv.errorNumber(), "error in Array comparison operator: " + rv.errorMessage());
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
    return {TRI_ERROR_BAD_PARAMETER, "Unable to extract value from Array comparison operator"};
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
        return {
          TRI_ERROR_BAD_PARAMETER,
          "Failed to generate field name from node " + arangodb::aql::AstNode::toString(attributeNode)
        };
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
    auto rv = error::malformedNode(node.type);
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
      // con't execute expression
      return {TRI_ERROR_BAD_PARAMETER, "Unable to extract value from 'IN' operator"};
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
    return {TRI_ERROR_BAD_PARAMETER, "Unable to extract value from 'IN' operator"};
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
    auto rv = error::malformedNode(node.type);
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

// ANALYZER(<filter-expression>, analyzer)
arangodb::Result fromFuncAnalyzer(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  auto const argc = args.numMembers();

  if (argc != 2) {
    return error::invalidArgsCount<error::ExactValue<2>>(funcName);
  }

  // 1st argument defines filter expression
  auto expressionArg = args.getMemberUnchecked(0);

  if (!expressionArg) {
    return error::invalidArgument(funcName, 1);
  }

  // 2nd argument defines an analyzer
  irs::string_ref analyzerId;
  ScopedAqlValue analyzerIdValue;

  auto rv = evaluateArg<decltype(analyzerId), true>(
        analyzerId, analyzerIdValue, funcName,
        args, 1, filter != nullptr, ctx);

  if (rv.fail()) {
    return rv;
  }

  arangodb::iresearch::FieldMeta::Analyzer analyzerValue; // default analyzer
  auto& analyzer = analyzerValue._pool;
  auto& shortName = analyzerValue._shortName;

  if (filter || analyzerIdValue.isConstant()) {
    TRI_ASSERT(ctx.trx);
    auto& server = ctx.trx->vocbase().server();
    if (!server.hasFeature<IResearchAnalyzerFeature>()) {
      return {
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        "'"s.append(IResearchAnalyzerFeature::name())
            .append("' feature is not registered, unable to evaluate '")
            .append(funcName).append("' function")
      };
    }

    auto& analyzerFeature = server.getFeature<IResearchAnalyzerFeature>();

    shortName = analyzerId;

    auto sysVocbase = server.hasFeature<arangodb::SystemDatabaseFeature>()
                          ? server.getFeature<arangodb::SystemDatabaseFeature>().use()
                          : nullptr;

    if (sysVocbase) {
      analyzer = analyzerFeature.get(analyzerId, ctx.trx->vocbase(), *sysVocbase);

      shortName = arangodb::iresearch::IResearchAnalyzerFeature::normalize(  // normalize
          analyzerId, ctx.trx->vocbase(), *sysVocbase, false);  // args
    }

    if (!analyzer) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "'"s.append(funcName).append("' AQL function: Unable to lookup analyzer '")
            .append(analyzerId.c_str()).append("'")
      };
    }
  }

  FilterContext const subFilterContext(analyzerValue, filterCtx.boost); // override analyzer

  rv = ::filter(filter, ctx, subFilterContext, *expressionArg);

  if (rv.fail()) {
    return {
      rv.errorNumber(),
      "failed to get filter for analyzer: "s
          .append(analyzer->name()).append(" : ").append(rv.errorMessage())
    };
  }
  return rv;
}

// BOOST(<filter-expression>, boost)
arangodb::Result fromFuncBoost(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  auto const argc = args.numMembers();

  if (argc != 2) {
    return error::invalidArgsCount<error::ExactValue<2>>(funcName);
  }

  // 1st argument defines filter expression
  auto expressionArg = args.getMemberUnchecked(0);

  if (!expressionArg) {
    return error::invalidArgument(funcName, 1);
  }

  ScopedAqlValue tmpValue;

  // 2nd argument defines a boost
  double_t boostValue = 0;
  auto rv = evaluateArg<decltype(boostValue), true>(
        boostValue, tmpValue, funcName, args, 1, filter != nullptr, ctx);

  if (rv.fail()) {
    return rv;
  }

  FilterContext const subFilterContext{filterCtx.analyzer,
                                       filterCtx.boost * static_cast<float_t>(boostValue)};

  rv = ::filter(filter, ctx, subFilterContext, *expressionArg);

  if (rv.fail()) {
    return {rv.errorNumber(), "error in sub-filter context: " + rv.errorMessage()};
  }

  return {};
}

// EXISTS(<attribute>, <"analyzer">, <"analyzer-name">)
// EXISTS(<attribute>, <"string"|"null"|"bool"|"numeric">)
arangodb::Result fromFuncExists(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  if (!args.isDeterministic()) {
    return error::nondeterministicArgs(funcName);
  }

  auto const argc = args.numMembers();

  if (argc < 1 || argc > 3) {
    return error::invalidArgsCount<error::Range<1, 3>>(funcName);
  }

  // 1st argument defines a field
  auto const* fieldArg =
      arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref);

  if (!fieldArg) {
    return error::invalidAttribute(funcName, 1);
  }

  bool prefixMatch = true;
  std::string fieldName;
  auto analyzer = filterCtx.analyzer;

  if (filter && !arangodb::iresearch::nameFromAttributeAccess(fieldName, *fieldArg, ctx)) {
    return error::failedToGenerateName(funcName, 1);
  }

  if (argc > 1) {
    // 2nd argument defines a type (if present)
    ScopedAqlValue argValue;
    irs::string_ref arg;
    auto rv = evaluateArg(arg, argValue, funcName, args, 1, filter != nullptr, ctx);

    if (rv.fail()) {
      return rv;
    }

    if (filter || argValue.isConstant()) { // arg is constant
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
        return {
          TRI_ERROR_BAD_PARAMETER,
          "'"s.append("' AQL function: 2nd argument must be equal to one of the following: "
                      "'string', 'type', 'analyzer', 'numeric', 'bool', 'boolean', 'null', but got '")
             .append(arg.c_str()).append("'")
        };
      }

      if (argc > 2) {
        if (TypeAnalyzer.c_str() != typeHandler->first.c_str()) {
          return {
            TRI_ERROR_BAD_PARAMETER,
            "'"s.append(funcName).append("' AQL function: 3rd argument is intended to be used with 'analyzer' type only")
          };
        }

        rv = extractAnalyzerFromArg(analyzer, funcName, filter, args, 2, ctx);

        if (rv.fail()) {
          return rv;
        }

        TRI_ASSERT(analyzer._pool);
        if (!analyzer._pool) {
          return {TRI_ERROR_INTERNAL, "analyzer not found"};
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
arangodb::Result fromFuncMinMatch(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  auto const argc = args.numMembers();

  if (argc < 2) {
    return error::invalidArgsCount<error::OpenRange<false, 2>>(funcName);
  }

  // ...........................................................................
  // last argument defines min match count
  // ...........................................................................

  auto const lastArg = argc - 1;
  ScopedAqlValue minMatchCountValue;
  int64_t minMatchCount = 0;

  auto rv = evaluateArg<decltype(minMatchCount), true>(
        minMatchCount, minMatchCountValue, funcName, args, lastArg, filter != nullptr, ctx);

  if (rv.fail()) {
    return rv;
  }

  if (minMatchCount < 0) {
    return error::negativeNumber(funcName, argc);
  }

  if (filter) {
    auto& minMatchFilter = filter->add<irs::Or>();
    minMatchFilter.min_match_count(static_cast<size_t>(minMatchCount));
    minMatchFilter.boost(filterCtx.boost);

    // become a new root
    filter = &minMatchFilter;
  }

  FilterContext const subFilterCtx{
    filterCtx.analyzer,
    irs::no_boost() // reset boost
  };

  for (size_t i = 0; i < lastArg; ++i) {
    auto subFilterExpression = args.getMemberUnchecked(i);

    if (!subFilterExpression) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "'"s.append(funcName).append("' AQL function: Failed to evaluate argument at position '")
            .append(std::to_string(i)).append("'")
      };
    }

    irs::boolean_filter* subFilter = filter ? &filter->add<irs::Or>() : nullptr;

    rv = ::filter(subFilter, ctx, subFilterCtx, *subFilterExpression);
    if (rv.fail()) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "'"s.append(funcName).append("' AQL function: Failed to instantiate sub-filter for argument at position '")
            .append(std::to_string(i)).append("': ").append(rv.errorMessage())
      };
    }
  }

  return {};
}

typedef std::function<
  arangodb::Result(char const*,
                   size_t const,
                   char const*,
                   irs::by_phrase*,
                   QueryContext const&,
                   arangodb::aql::AstNode const&,
                   size_t)
> ConversionPhraseHandler;

std::string getSubFuncErrorSuffix(char const* funcName, size_t const funcArgumentPosition) {
  return " (in '"s.append(funcName).append("' AQL function at position '")
      .append(std::to_string(funcArgumentPosition + 1)).append("')");
}

arangodb::Result oneArgumentfromFuncPhrase(char const* funcName,
                                           size_t const funcArgumentPosition,
                                           char const* subFuncName,
                                           irs::by_phrase* filter,
                                           QueryContext const& ctx,
                                           arangodb::aql::AstNode const& elem,
                                           irs::string_ref& term) {
  if (!elem.isDeterministic()) {
    auto res = error::nondeterministicArgs(subFuncName);
    return {
      res.errorNumber(),
      res.errorMessage().append(getSubFuncErrorSuffix(funcName, funcArgumentPosition))
    };
  }

  if (elem.numMembers() != 1) {
    auto res = error::invalidArgsCount<error::ExactValue<1>>(subFuncName);
    return {
      res.errorNumber(),
      res.errorMessage().append(getSubFuncErrorSuffix(funcName, funcArgumentPosition))
    };
  }

  ScopedAqlValue termValue;
  auto res = evaluateArg(term, termValue, subFuncName, elem, 0, filter != nullptr, ctx);

  if (res.fail()) {
    return {
      res.errorNumber(),
      res.errorMessage().append(getSubFuncErrorSuffix(funcName, funcArgumentPosition))
    };
  }
  return {};
}

// {<TERM>: [ '[' ] <term> [ ']' ] }
arangodb::Result fromFuncPhraseTerm(char const* funcName,
                                    size_t funcArgumentPosition,
                                    char const* subFuncName,
                                    irs::by_phrase* filter,
                                    QueryContext const& ctx,
                                    arangodb::aql::AstNode const& elem,
                                    size_t firstOffset) {
  irs::string_ref term;
  auto res = oneArgumentfromFuncPhrase(funcName, funcArgumentPosition, subFuncName, filter, ctx, elem, term);
  if (res.fail()) {
    return res;
  }
  if (filter) {
    filter->push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(term)}, firstOffset);
  }
  return {};
}

// {<STARTS_WITH>: [ '[' ] <term> [ ']' ] }
arangodb::Result fromFuncPhraseStartsWith(char const* funcName,
                                          size_t funcArgumentPosition,
                                          char const* subFuncName,
                                          irs::by_phrase* filter,
                                          QueryContext const& ctx,
                                          arangodb::aql::AstNode const& elem,
                                          size_t firstOffset) {
  irs::string_ref term;
  auto res = oneArgumentfromFuncPhrase(funcName, funcArgumentPosition, subFuncName, filter, ctx, elem, term);
  if (res.fail()) {
    return res;
  }
  if (filter) {
    // 128 - FIXME make configurable
    filter->push_back(irs::by_phrase::prefix_term{128, irs::ref_cast<irs::byte_type>(term)}, firstOffset);
  }
  return {};
}

// {<WILDCARD>: [ '[' ] <term> [ ']' ] }
arangodb::Result fromFuncPhraseLike(char const* funcName,
                                    size_t const funcArgumentPosition,
                                    char const* subFuncName,
                                    irs::by_phrase* filter,
                                    QueryContext const& ctx,
                                    arangodb::aql::AstNode const& elem,
                                    size_t firstOffset) {
  irs::string_ref term;
  auto res = oneArgumentfromFuncPhrase(funcName, funcArgumentPosition, subFuncName, filter, ctx, elem, term);
  if (res.fail()) {
    return res;
  }
  if (filter) {
    // 128 - FIXME make configurable
    filter->push_back(irs::by_phrase::wildcard_term{128, irs::ref_cast<irs::byte_type>(term)}, firstOffset);
  }
  return {};
}

template<size_t First>
arangodb::Result getLevenshteinArguments(char const* funcName, bool isFilter,
                                         QueryContext const& ctx,
                                         arangodb::aql::AstNode const& args,
                                         arangodb::aql::AstNode const** field,
                                         ScopedAqlValue& targetValue,
                                         irs::string_ref& target,
                                         size_t& scoringLimit, int64_t& maxDistance,
                                         bool& withTranspositions,
                                         std::string const& errorSuffix = std::string()) {
  if (!args.isDeterministic()) {
    auto res = error::nondeterministicArgs(funcName);
    return {
      res.errorNumber(),
      res.errorMessage().append(errorSuffix)
    };
  }
  auto const argc = args.numMembers();
  constexpr size_t min = 3 - First;
  constexpr size_t max = 4 - First;
  if (argc < min || argc > max) {
    auto res = error::invalidArgsCount<error::Range<min, max>>(funcName);
    return {
      res.errorNumber(),
      res.errorMessage().append(errorSuffix)
    };
  }

  if constexpr (0 == First) {
    TRI_ASSERT(field);
    // (0 - First) argument defines a field
    *field = arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref);

    if (!*field) {
      return error::invalidAttribute(funcName, 1);
    }
  }

  // (1 - First) argument defines a target
  auto res = evaluateArg(target, targetValue, funcName, args, 1 - First, isFilter, ctx);

  if (res.fail()) {
    return {
      res.errorNumber(),
      res.errorMessage().append(errorSuffix)
    };
  }

  ScopedAqlValue tmpValue; // can reuse value for int64_t and bool

  // (2 - First) argument defines a max distance
  res = evaluateArg(maxDistance, tmpValue, funcName, args, 2 - First, isFilter, ctx);

  if (res.fail()) {
    return {
      res.errorNumber(),
      res.errorMessage().append(errorSuffix)
    };
  }

  if (maxDistance < 0) {
    return {
      TRI_ERROR_BAD_PARAMETER, "'"s.append(funcName)
          .append("' AQL function: max distance must be a non-negative number")
          .append(errorSuffix)
    };
  }

  // optional (3 - First) argument defines transpositions
  if (4 - First == argc) {
    res = evaluateArg(withTranspositions, tmpValue, funcName, args, 3 - First, isFilter, ctx);

    if (res.fail()) {
      return {
        res.errorNumber(),
        res.errorMessage().append(errorSuffix)
      };
    }
  }

  if (!withTranspositions && maxDistance > MAX_LEVENSHTEIN_DISTANCE) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName)
          .append("' AQL function: max Levenshtein distance must be a number in range [0, ")
          .append(std::to_string(MAX_LEVENSHTEIN_DISTANCE)).append("]")
          .append(errorSuffix)
    };
  } else if (withTranspositions && maxDistance > MAX_DAMERAU_LEVENSHTEIN_DISTANCE) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName)
          .append("' AQL function: max Damerau-Levenshtein distance must be a number in range [0, ")
          .append(std::to_string(MAX_DAMERAU_LEVENSHTEIN_DISTANCE)).append("]")
          .append(errorSuffix)
    };
  }

  scoringLimit = FilterConstants::DefaultScoringTermsLimit;

  return {};
}

// {<LEVENSHTEIN_MATCH>: '[' <term>, <max_distance> [, <with_transpositions> ] ']'}
arangodb::Result fromFuncPhraseLevenshteinMatch(char const* funcName,
                                                size_t const funcArgumentPosition,
                                                char const* subFuncName,
                                                irs::by_phrase* filter,
                                                QueryContext const& ctx,
                                                arangodb::aql::AstNode const& array,
                                                size_t firstOffset) {
  if (!array.isArray()) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function: '")
          .append(subFuncName)
          .append("' arguments must be in an array at position '")
          .append(std::to_string(funcArgumentPosition + 1)).append("'")
    };
  }

  ScopedAqlValue targetValue;
  irs::string_ref target;
  size_t scoringLimit = 0;
  int64_t maxDistance = 0;
  auto withTranspositions = false;
  auto res = getLevenshteinArguments<1>(subFuncName, filter != nullptr, ctx, array, nullptr,
                                        targetValue, target, scoringLimit, maxDistance,
                                        withTranspositions,
                                        getSubFuncErrorSuffix(funcName, funcArgumentPosition));
  if (res.fail()) {
    return res;
  }

  if (filter) {
    filter->push_back(
          irs::by_phrase::levenshtein_term{withTranspositions, static_cast<irs::byte_type>(maxDistance),
                                           scoringLimit, &arangodb::iresearch::getParametricDescription,
                                           irs::ref_cast<irs::byte_type>(target)}, firstOffset);
  }
  return {};
}

// {<TERMS>: '[' <term0> [, <term1>, ...] ']'}
arangodb::Result fromFuncPhraseTerms(char const* funcName,
                                     size_t const funcArgumentPosition,
                                     char const* subFuncName,
                                     irs::by_phrase* filter,
                                     QueryContext const& ctx,
                                     arangodb::aql::AstNode const& array,
                                     size_t firstOffset) {
  if (!array.isArray()) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function: '")
          .append(subFuncName)
          .append("' arguments must be in an array at position '")
          .append(std::to_string(funcArgumentPosition + 1)).append("'")
    };
  }

  if (!array.isDeterministic()) {
    auto res = error::nondeterministicArgs(subFuncName);
    return {
      res.errorNumber(),
      res.errorMessage().append(getSubFuncErrorSuffix(funcName, funcArgumentPosition))
    };
  }

  auto const argc = array.numMembers();
  if (0 == argc) {
    auto res = error::invalidArgsCount<error::OpenRange<false, 1>>(subFuncName);
    return {
      res.errorNumber(),
      res.errorMessage().append(getSubFuncErrorSuffix(funcName, funcArgumentPosition))
    };
  }

  std::vector<irs::bstring> terms;
  terms.reserve(argc);
  ScopedAqlValue termValue;
  irs::string_ref term;
  for (size_t i = 0; i < argc; ++i) {
    auto res = evaluateArg(term, termValue, subFuncName, array, i, filter != nullptr, ctx);

    if (res.fail()) {
      return {
        res.errorNumber(),
        res.errorMessage().append(getSubFuncErrorSuffix(funcName, funcArgumentPosition))
      };
    }
    terms.emplace_back(irs::ref_cast<irs::byte_type>(term));
  }
  if (filter) {
    filter->push_back(irs::by_phrase::set_term{std::move(terms)}, firstOffset);
  }
  return {};
}

constexpr char const* termsFuncName = "TERMS";

std::map<std::string, ConversionPhraseHandler> const FCallSystemConversionPhraseHandlers {
  {"TERM", fromFuncPhraseTerm},
  {"STARTS_WITH", fromFuncPhraseStartsWith},
  {"WILDCARD", fromFuncPhraseLike}, // 'LIKE' is a key word
  {"LEVENSHTEIN_MATCH", fromFuncPhraseLevenshteinMatch},
  {termsFuncName, fromFuncPhraseTerms}
};

// {<TERM>|<STARTS_WITH>|<WILDCARD>|<LEVENSHTEIN_MATCH>|<TERMS>: '[' <term> [, ...] ']'}
arangodb::Result processPhraseArgObjectType(char const* funcName,
                                            size_t const funcArgumentPosition,
                                            irs::by_phrase* filter,
                                            QueryContext const& ctx,
                                            arangodb::aql::AstNode const& object,
                                            size_t firstOffset) {
  TRI_ASSERT(object.isObject() && object.numMembers() == 1);
  auto const* objectElem = object.getMember(0);
  std::string name = objectElem->getStringValue();
  arangodb::basics::StringUtils::toupperInPlace(name);
  auto const entry = FCallSystemConversionPhraseHandlers.find(name);
  if (FCallSystemConversionPhraseHandlers.cend() == entry) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function: Unknown '")
          .append(objectElem->getStringValue()).append("' at position '")
          .append(std::to_string(funcArgumentPosition + 1)).append("'")
    };
  }
  TRI_ASSERT(objectElem->numMembers() == 1);
  auto const* elem = objectElem->getMember(0);
  if (!elem->isArray()) {
    elem = objectElem;
  }
  return entry->second(funcName, funcArgumentPosition, entry->first.c_str(), filter, ctx, *elem, firstOffset);
}

arangodb::Result processPhraseArgs(
    char const* funcName,
    irs::by_phrase* phrase,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& valueArgs,
    size_t valueArgsBegin, size_t valueArgsEnd,
    irs::analysis::analyzer::ptr& analyzer,
    size_t offset,
    bool allowDefaultOffset,
    bool isInArray) {
  irs::string_ref value;
  bool expectingOffset = false;
  for (size_t idx = valueArgsBegin; idx < valueArgsEnd; ++idx) {
    auto currentArg = valueArgs.getMemberUnchecked(idx);
    if (!currentArg) {
      return error::invalidArgument(funcName, idx);
    }
    if (currentArg->isArray()) {
      // '[' <term0> [, <term1>, ...] ']'
      if (isInArray) {
        auto res = fromFuncPhraseTerms(funcName, idx, termsFuncName, phrase, ctx, *currentArg, offset);
        if (res.fail()) {
          return res;
        }
        expectingOffset = true;
        offset = 0;
        continue;
      }
      if (!expectingOffset || allowDefaultOffset) {
        if (0 == currentArg->numMembers()) {
          expectingOffset = true;
          // do not reset offset here as we should accumulate it
          continue; // just skip empty arrays. This is not error anymore as this case may arise while working with autocomplete
        }
        // array arg is processed with possible default 0 offsets - to be easily compatible with TOKENS function
        if (!isInArray) {
          auto subRes = processPhraseArgs(funcName, phrase, ctx, filterCtx, *currentArg, 0, currentArg->numMembers(), analyzer, offset, true, true);
          if (subRes.fail()) {
            return subRes;
          }
          expectingOffset = true;
          offset = 0;
          continue;
        }

        return {
          TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName).append("' AQL function: recursive arrays not allowed at position ")
              .append(std::to_string(idx))
        };
      }
    }
    if (currentArg->isObject()) {
      if (currentArg->numMembers() != 1) {
        return {
          TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName).append("' AQL function: Invalid number of fields in an object (expected ")
              .append(std::to_string(1)).append(") at position '").append(std::to_string(idx)).append("'")
        };
      }
      auto res = processPhraseArgObjectType(funcName, idx, phrase, ctx, *currentArg, offset);
      if (res.fail()) {
        return res;
      }
      offset = 0;
      expectingOffset = true;
      continue;
    }
    ScopedAqlValue currentValue(*currentArg);
    if (phrase || currentValue.isConstant()) {
      if (!currentValue.execute(ctx)) {
        return error::invalidArgument(funcName, idx);
      }
      if (arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE == currentValue.type() && expectingOffset) {
        offset += static_cast<uint64_t>(currentValue.getInt64());
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

        return {
          TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName).append("' AQL function: Unable to parse argument at position ")
              .append(std::to_string(idx)).append(expectedValue)
        };
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
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function : Unable to parse argument at position ")
          .append(std::to_string(valueArgsEnd - 1)).append("as a value")
    };
  }

  return {};
}

// note: <value> could be either string ether array of strings with offsets inbetween . Inside array
// 0 offset could be omitted e.g. [term1, term2, 2, term3] is equal to: [term1, 0, term2, 2, term3]
// PHRASE(<attribute>, <value> [, <offset>, <value>, ...] [, <analyzer>])
// PHRASE(<attribute>, '[' <value> [, <offset>, <value>, ...] ']' [,<analyzer>])
arangodb::Result fromFuncPhrase(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  if (!args.isDeterministic()) {
    return error::nondeterministicArgs(funcName);
  }

  auto argc = args.numMembers();

  if (argc < 2) {
    return error::invalidArgsCount<error::OpenRange<false, 2>>(funcName);
  }

  // ...........................................................................
  // last odd argument defines an analyzer
  // ...........................................................................

  auto analyzerPool = filterCtx.analyzer;

  if (0 != (argc & 1)) {  // override analyzer
    --argc;

    auto rv = extractAnalyzerFromArg(analyzerPool, funcName, filter, args, argc, ctx);

    if (rv.fail()) {
      return rv;
    }

    TRI_ASSERT(analyzerPool._pool);
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
    return error::invalidAttribute(funcName, 1);
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
      return error::failedToGenerateName(funcName, 1);
    }

    TRI_ASSERT(analyzerPool._pool);
    analyzer = analyzerPool._pool->get();

    if (!analyzer) {
      return {
        TRI_ERROR_INTERNAL,
        "'"s.append("' AQL function: Unable to instantiate analyzer '")
            .append(analyzerPool._pool->name()).append("'")
      };
    }

    kludge::mangleStringField(name, analyzerPool);

    phrase = &filter->add<irs::by_phrase>();
    phrase->field(std::move(name));
    phrase->boost(filterCtx.boost);
  }
  // on top level we require explicit offsets - to be backward compatible and be able to distinguish last argument as analyzer or value
  // Also we allow recursion inside array to support older syntax (one array arg) and add ability to pass several arrays as args
  return processPhraseArgs(funcName, phrase, ctx, filterCtx, *valueArgs, valueArgsBegin, valueArgsEnd, analyzer, 0, false, false);
}

// NGRAM_MATCH (attribute, target, threshold [, analyzer])
// NGRAM_MATCH (attribute, target [, analyzer]) // default threshold is set to 0.7
arangodb::Result fromFuncNgramMatch(
    char const* funcName,
    irs::boolean_filter* filter, QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args) {

  if (!args.isDeterministic()) {
    return error::nondeterministicArgs(funcName);
  }

  auto const argc = args.numMembers();

  if (argc < 2 || argc > 4) {
    return error::invalidArgsCount<error::Range<2, 4>>(funcName);
  }

  // 1st argument defines a field
  auto const* field =
    arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref);

  if (!field) {
    return error::invalidAttribute(funcName, 1);
  }

  // 2nd argument defines a value
  ScopedAqlValue matchAqlValue;
  irs::string_ref matchValue;
  {
    auto res = evaluateArg(matchValue, matchAqlValue, funcName, args, 1, filter, ctx);
    if (!res.ok()) {
      return res;
    }
  }

  auto threshold = FilterConstants::DefaultNgramMatchThreshold;
  TRI_ASSERT(filterCtx.analyzer);
  auto analyzerPool = filterCtx.analyzer;

  if (argc > 3) {// 4 args given. 3rd is threshold
    ScopedAqlValue tmpValue;
    auto res = evaluateArg(threshold, tmpValue, funcName, args, 2, filter, ctx);

    if (!res.ok()) {
      return res;
    }
  } else if (argc > 2) {  //3 args given  -  3rd argument defines a threshold (if double) or analyzer (if string)
    auto const* arg = args.getMemberUnchecked(2);

    if (!arg) {
      return error::invalidArgument(funcName, 3);
    }

    if (!arg->isDeterministic()) {
      return error::nondeterministicArg(funcName, 3);
    }
    ScopedAqlValue tmpValue(*arg);
    if (filter || tmpValue.isConstant()) {
      if (!tmpValue.execute(ctx)) {
        return error::failedToEvaluate(funcName, 3);
      }
      if (arangodb::iresearch::SCOPED_VALUE_TYPE_STRING == tmpValue.type()) { // this is analyzer
        irs::string_ref analyzerId;
        if (!tmpValue.getString(analyzerId)) {
          return error::failedToParse(funcName, 3, arangodb::iresearch::SCOPED_VALUE_TYPE_STRING);
        }
        if (filter || tmpValue.isConstant()) {
          auto analyzerRes = getAnalyzerByName(analyzerPool, analyzerId, funcName, ctx);
          if (!analyzerRes.ok()) {
            return analyzerRes;
          }
        }
      } else if (arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE == tmpValue.type()) {
        if (!tmpValue.getDouble(threshold)) {
          return error::failedToParse(funcName, 3, arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE);
        }
      } else {
        return {
            TRI_ERROR_BAD_PARAMETER,
            "'"s.append(funcName).append("' AQL function: argument at position '").append(std::to_string(3))
           .append("' has invalid type '").append(ScopedAqlValue::typeString(tmpValue.type()).c_str())
           .append("' ('").append(ScopedAqlValue::typeString(arangodb::iresearch::SCOPED_VALUE_TYPE_DOUBLE).c_str())
           .append("' or '").append(ScopedAqlValue::typeString(arangodb::iresearch::SCOPED_VALUE_TYPE_STRING).c_str())
           .append("' expected)")
        };
      }
    }
  }

  if (threshold <= 0 || threshold > 1) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName)
      .append("' AQL function: threshold must be between 0 and 1")
    };
  }

  // 4th optional argument defines an analyzer
  if (argc > 3) {
      auto rv = extractAnalyzerFromArg(analyzerPool, funcName, filter, args, 3, ctx);

      if (rv.fail()) {
        return rv;
      }
      TRI_ASSERT(analyzerPool._pool);
      if (!analyzerPool._pool) {
        return { TRI_ERROR_BAD_PARAMETER };
      }
  }

  if (filter) {
    std::string name;

    if (!nameFromAttributeAccess(name, *field, ctx)) {
      auto message = "'"s.append(funcName).append("' AQL function: Failed to generate field name from the 1st argument");
      LOG_TOPIC("91862", WARN, arangodb::iresearch::TOPIC) << message;
      return { TRI_ERROR_BAD_PARAMETER, message };
    }

    TRI_ASSERT(analyzerPool._pool);
    auto analyzer = analyzerPool._pool->get();

    if (!analyzer) {
      return {
        TRI_ERROR_INTERNAL,
        "'"s.append(funcName).append("' AQL function: Unable to instantiate analyzer '")
            .append(analyzerPool._pool->name()).append("'")
      };
    }


    kludge::mangleStringField(name, analyzerPool);

    auto& ngramFilter = filter->add<irs::by_ngram_similarity>();
    ngramFilter.field(std::move(name)).threshold(threshold).boost(filterCtx.boost);;

    analyzer->reset(matchValue);
    irs::term_attribute const& token = *analyzer->attributes().get<irs::term_attribute>();
    while (analyzer->next()) {
      ngramFilter.push_back(token.value());
    }
  }
  return {};
}

// STARTS_WITH(<attribute>, <prefix>, [<scoring-limit>])
arangodb::Result fromFuncStartsWith(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  if (!args.isDeterministic()) {
    return error::nondeterministicArgs(funcName);
  }

  auto const argc = args.numMembers();

  if (argc < 2 || argc > 3) {
    return error::invalidArgsCount<error::Range<2, 3>>(funcName);
  }

  // 1st argument defines a field
  auto const* field =
      arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref);

  if (!field) {
    return error::invalidAttribute(funcName, 1);
  }

  // 2nd argument defines a value
  ScopedAqlValue prefixValue;
  irs::string_ref prefix;
  auto rv = evaluateArg(prefix, prefixValue, funcName, args, 1, filter != nullptr, ctx);

  if (rv.fail()) {
    return rv;
  }

  size_t scoringLimit = FilterConstants::DefaultScoringTermsLimit;

  if (argc > 2) {
    // 3rd (optional) argument defines a number of scored terms
    ScopedAqlValue scoringLimitValueBuf;
    int64_t scoringLimitValue = static_cast<int64_t>(scoringLimit);
    rv = evaluateArg(scoringLimitValue, scoringLimitValueBuf, funcName, args, 2, filter != nullptr, ctx);

    if (rv.fail()) {
      return rv;
    }

    if (scoringLimitValue < 0) {
      return error::negativeNumber(funcName, 3);
    }

    scoringLimit = static_cast<size_t>(scoringLimitValue);
  }

  if (filter) {
    std::string name;

    if (!nameFromAttributeAccess(name, *field, ctx)) {
      return error::failedToGenerateName(funcName, 1);
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
arangodb::Result fromFuncInRange(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  if (!args.isDeterministic()) {
    return error::nondeterministicArgs(funcName);
  }

  auto const argc = args.numMembers();

  if (argc != 5) {
    return error::invalidArgsCount<error::ExactValue<5>>(funcName);
  }

  // 1st argument defines a field
  auto const* field =
      arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref);

  if (!field) {
    return error::invalidAttribute(funcName, 1);
  }

  // 2nd argument defines a lower boundary
  auto const* lhsArg = args.getMemberUnchecked(1);

  if (!lhsArg) {
    return error::invalidArgument(funcName, 2);
  }

  // 3rd argument defines an upper boundary
  auto const* rhsArg = args.getMemberUnchecked(2);

  if (!rhsArg) {
    return error::invalidArgument(funcName, 3);
  }

  ScopedAqlValue includeValue;

  // 4th argument defines inclusion of lower boundary
  bool lhsInclude = false;

  auto rv = evaluateArg(lhsInclude, includeValue, funcName, args, 3, filter != nullptr, ctx);

  if (rv.fail()) {
    return rv;
  }

  // 5th argument defines inclusion of upper boundary
  bool rhsInclude = false;

  rv = evaluateArg(rhsInclude, includeValue, funcName, args, 4, filter != nullptr, ctx);

  if (rv.fail()) {
    return rv;
  }

  rv = ::byRange(filter, *field, *lhsArg, lhsInclude, *rhsArg, rhsInclude, ctx, filterCtx);

  if (rv.fail()) {
    return {rv.errorNumber(), "error in byRange: " + rv.errorMessage()};
  }

  return {};
}

// LIKE(<attribute>, <pattern>)
arangodb::Result fromFuncLike(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  if (!args.isDeterministic()) {
    return error::nondeterministicArgs(funcName);
  }

  auto const argc = args.numMembers();

  if (argc != 2) {
    return error::invalidArgsCount<error::ExactValue<2>>(funcName);
  }


  // 1st argument defines a field
  auto const* field =
      arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref);

  if (!field) {
    return error::invalidAttribute(funcName, 1);
  }

  // 2nd argument defines a matching pattern
  ScopedAqlValue patternValue;
  irs::string_ref pattern;
  arangodb::Result res = evaluateArg(pattern, patternValue, funcName, args, 1, filter != nullptr, ctx);

  if (!res.ok()) {
    return res;
  }

  const auto scoringLimit = FilterConstants::DefaultScoringTermsLimit;

  if (filter) {
    std::string name;

    if (!nameFromAttributeAccess(name, *field, ctx)) {
      return error::failedToGenerateName(funcName, 1);
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

// LEVENSHTEIN_MATCH(<attribute>, <target>, <max-distance> [, <include-transpositions>])
arangodb::Result fromFuncLevenshteinMatch(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  arangodb::aql::AstNode const* field = nullptr;
  ScopedAqlValue targetValue;
  irs::string_ref target;
  size_t scoringLimit = 0;
  int64_t maxDistance = 0;
  auto withTranspositions = false;
  auto res = getLevenshteinArguments<0>(funcName, filter != nullptr, ctx, args, &field,
                                        targetValue, target, scoringLimit, maxDistance,
                                        withTranspositions);
  if (res.fail()) {
    return res;
  }

  if (filter) {
    std::string name;

    if (!nameFromAttributeAccess(name, *field, ctx)) {
      return error::failedToGenerateName(funcName, 1);
    }

    TRI_ASSERT(filterCtx.analyzer);
    kludge::mangleStringField(name, filterCtx.analyzer);

    auto& levenshtein_filter = filter->add<irs::by_edit_distance>();
    levenshtein_filter.scored_terms_limit(scoringLimit);
    levenshtein_filter.field(std::move(name));
    levenshtein_filter.term(target);
    levenshtein_filter.boost(filterCtx.boost);
    levenshtein_filter.max_distance(irs::byte_type(maxDistance));
    levenshtein_filter.with_transpositions(withTranspositions);
    levenshtein_filter.provider(&arangodb::iresearch::getParametricDescription);
  }

  return {};
}

std::map<irs::string_ref, ConvertionHandler> const FCallUserConvertionHandlers;

arangodb::Result fromFCallUser(irs::boolean_filter* filter, QueryContext const& ctx,
                   FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL_USER == node.type);

  if (node.numMembers() != 1) {
    return error::malformedNode(node.type);
  }

  auto const* args = arangodb::iresearch::getNode(node, 0, arangodb::aql::NODE_TYPE_ARRAY);

  if (!args) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Unable to parse user function arguments as an array'"
    };
  }

  irs::string_ref name;

  if (!arangodb::iresearch::parseValue(name, node)) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Unable to parse user function name"
    };
  }

  auto const entry = FCallUserConvertionHandlers.find(name);

  if (entry == FCallUserConvertionHandlers.end()) {
    return fromExpression(filter, ctx, filterCtx, node);
  }

  if (!args->isDeterministic()) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Unable to handle non-deterministic function '"s.append(name.c_str(), name.size()).append("' arguments")
    };
  }

  return entry->second(entry->first.c_str(), filter, ctx, filterCtx, *args);
}

std::map<std::string, ConvertionHandler> const FCallSystemConvertionHandlers{
    // filter functions
    {"PHRASE", fromFuncPhrase},
    {"STARTS_WITH", fromFuncStartsWith},
    {"EXISTS", fromFuncExists},
    {"MIN_MATCH", fromFuncMinMatch},
    {"IN_RANGE", fromFuncInRange},
    {"LIKE", fromFuncLike },
    {"LEVENSHTEIN_MATCH", fromFuncLevenshteinMatch},
    {"NGRAM_MATCH", fromFuncNgramMatch},
    // context functions
    {"BOOST", fromFuncBoost},
    {"ANALYZER", fromFuncAnalyzer},
};

arangodb::Result fromFCall(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL == node.type);

  auto const* fn = static_cast<arangodb::aql::Function*>(node.getData());

  if (!fn || node.numMembers() != 1) {
    return error::malformedNode(node.type);
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
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Unable to parse arguments of system function '"s.append(fn->name).append("' as an array'")
    };
  }

  return entry->second(entry->first.c_str(), filter, ctx, filterCtx, *args);
}

arangodb::Result fromFilter(irs::boolean_filter* filter, QueryContext const& ctx,
                FilterContext const& filterCtx, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FILTER == node.type);

  if (node.numMembers() != 1) {
    auto rv = error::malformedNode(node.type);
    return rv.reset(rv.errorNumber(), "wrong number of parameters: " + rv.errorMessage());
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

/*static*/ arangodb::Result FilterFactory::filter(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    arangodb::aql::AstNode const& node) {
  // The analyzer is referenced in the FilterContext and used during the
  // following ::filter() call, so may not be a temporary.
  FieldMeta::Analyzer analyzer = FieldMeta::Analyzer();
  FilterContext const filterCtx(analyzer, irs::no_boost());

  const auto res = ::filter(filter, ctx, filterCtx, node);

  if (res.fail()) {
    LOG_TOPIC("dfa15", WARN, arangodb::iresearch::TOPIC) << res.errorMessage();
  }

  return res;
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
