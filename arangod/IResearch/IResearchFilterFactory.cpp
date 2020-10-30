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

// otherwise define conflict between 3rdParty\date\include\date\date.h and
// 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
#include "date/date.h"
#endif

#include "IResearch/IResearchFilterFactory.h"

#include "s2/s2latlng.h"
#include "s2/s2point_region.h"
#include "s2/s2region_term_indexer.h"

#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"
#include "search/all_filter.hpp"
#include "search/boolean_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/filter_visitor.hpp"
#include "search/granular_range_filter.hpp"
#include "search/levenshtein_filter.hpp"
#include "search/ngram_similarity_filter.hpp"
#include "search/phrase_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/term_filter.hpp"
#include "search/top_terms_collector.hpp"
#include "search/wildcard_filter.hpp"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/Function.h"
#include "Aql/Quantifier.h"
#include "Aql/Range.h"
#include "Geo/GeoJson.h"
#include "Geo/ShapeContainer.h"
#include "Basics/StringUtils.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/GeoAnalyzer.h"
#include "IResearch/GeoFilter.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/IResearchPDP.h"
#include "Logger/LogMacros.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::iresearch;
using namespace std::literals::string_literals;

namespace {

constexpr char const* GEO_INTERSECT_FUNC = "GEO_INTERSECTS";
constexpr char const* GEO_DISTANCE_FUNC = "GEO_DISTANCE";
constexpr char const* TERMS_FUNC = "TERMS";

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
Result invalidArgsCount(char const* funcName) {
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

Result negativeNumber(char const* funcName, size_t i) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: argument at position '")
        .append(std::to_string(i)).append("' must be a positive number")
  };
}

Result nondeterministicArgs(char const* funcName) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "Unable to handle non-deterministic arguments for '"s
        .append(funcName).append("' function")
  };
}

Result nondeterministicArg(char const* funcName, size_t i) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: argument at position '")
        .append(std::to_string(i)).append("' is intended to be deterministic")
  };
}

Result invalidAttribute(char const* funcName, size_t i) {
  return {
    TRI_ERROR_BAD_PARAMETER,
     "'"s.append(funcName).append("' AQL function: Unable to parse argument at position '")
         .append(std::to_string(i)).append("' as an attribute identifier")
  };
}

Result invalidArgument(char const* funcName, size_t i) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: argument at position '")
        .append(std::to_string(i)).append("' is invalid")
  };
}

Result failedToEvaluate(const char* funcName, size_t i) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: Failed to evaluate argument at position '")
        .append(std::to_string(i)).append(("'"))
  };
}

Result typeMismatch(const char* funcName, size_t i,
                    ScopedValueType expectedType,
                    ScopedValueType actualType) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: argument at position '").append(std::to_string(i))
             .append("' has invalid type '").append(ScopedAqlValue::typeString(actualType).c_str())
             .append("' ('").append(ScopedAqlValue::typeString(expectedType).c_str()).append("' expected)")
  };
}

Result failedToParse(char const* funcName, size_t i, ScopedValueType expectedType) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: Unable to parse argument at position '")
        .append(std::to_string(i)).append("' as ").append(ScopedAqlValue::typeString(expectedType).c_str())
  };
}

Result failedToGenerateName(char const* funcName, size_t i) {
  return {
    TRI_ERROR_BAD_PARAMETER,
    "'"s.append(funcName).append("' AQL function: Failed to generate field name from the argument at position '")
        .append(std::to_string(i)).append("'")
  };
}

Result malformedNode(aql::AstNodeType type) {
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

} // error

bool setupGeoFilter(FieldMeta::Analyzer const& a,
                    S2RegionTermIndexer::Options& opts) {
  if (!a._pool) {
    return false;
  }

  auto& pool = *a._pool;

  if (isGeoAnalyzer(pool.type())) {
    auto stream = pool.get();

    if (!stream) {
      return false;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto& impl = dynamic_cast<GeoAnalyzer const&>(*stream);
#else
    auto& impl = static_cast<GeoAnalyzer const&>(*stream);
#endif

    impl.prepare(opts);
    return true;
  }

  return false;
}

template<typename T, bool CheckDeterminism = false>
Result evaluateArg(
    T& out, ScopedAqlValue& value,
    char const* funcName,
    aql::AstNode const& args,
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

Result getLatLong(
    ScopedAqlValue const& value,
    S2LatLng& point,
    char const* funcName,
    size_t argIdx) {
  switch (value.type()) {
    case SCOPED_VALUE_TYPE_ARRAY: { // [lng, lat] is valid input
      if (value.size() < 2) {
        return error::failedToEvaluate(funcName, argIdx);
      }

      auto const latValue = value.at(1);
      auto const lonValue = value.at(0);

      if (!latValue.isDouble() || !lonValue.isDouble()) {
        return error::failedToEvaluate(funcName, argIdx);
      }

      double_t lat, lon;

      if (!latValue.getDouble(lat) || !lonValue.getDouble(lon)) {
        return error::failedToEvaluate(funcName, argIdx);
      }

      point = S2LatLng::FromDegrees(lat, lon);
      return {};
    }
    case SCOPED_VALUE_TYPE_OBJECT: {
      VPackSlice const json = value.slice();
      geo::ShapeContainer shape;
      Result res;
      if (json.isArray() && json.length() >= 2) {
        res = shape.parseCoordinates(json, /*GeoJson*/ true);
      } else {
        res = geo::geojson::parseRegion(json, shape);
      }
      if (res.fail()) {
        return res;
      }
      point = S2LatLng(shape.centroid());
      return {};
    }
    default: {
      return error::invalidArgument(funcName, argIdx);
    }
  }
}

Result getAnalyzerByName(
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

  analyzer = analyzerFeature.get(analyzerId, ctx.trx->vocbase(),	
                                 ctx.trx->state()->analyzersRevision());

  if (!analyzer) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append("' AQL function: Unable to load requested analyzer '")
          .append(analyzerId.c_str(), analyzerId.size()).append("'")
    };
  }

  shortName = arangodb::iresearch::IResearchAnalyzerFeature::normalize(
    analyzerId, ctx.trx->vocbase().name(), false);

  return {};
}

Result extractAnalyzerFromArg(
    arangodb::iresearch::FieldMeta::Analyzer& out,
    char const* funcName,
    irs::boolean_filter const* filter,
    aql::AstNode const& args,
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
  FilterContext(
      arangodb::iresearch::FieldMeta::Analyzer const& analyzer,
      irs::boost_t boost) noexcept
    : analyzer(analyzer),
      boost(boost) {
    TRI_ASSERT(analyzer._pool);
  }

  FilterContext(FilterContext const&) = default;
  FilterContext& operator=(FilterContext const&) = delete;

  // need shared_ptr since pool could be deleted from the feature
  arangodb::iresearch::FieldMeta::Analyzer const& analyzer;
  irs::boost_t boost;
};  // FilterContext

typedef std::function<
  Result(char const* funcName, irs::boolean_filter*,
         QueryContext const&, FilterContext const&,
         aql::AstNode const&)
> ConvertionHandler;

// forward declaration
Result filter(irs::boolean_filter* filter,
              QueryContext const& queryctx,
              FilterContext const& filterCtx,
              aql::AstNode const& node);

////////////////////////////////////////////////////////////////////////////////
/// @brief appends value tokens to a phrase filter
////////////////////////////////////////////////////////////////////////////////
void appendTerms(irs::by_phrase& filter, irs::string_ref const& value,
                 irs::analysis::analyzer& stream, size_t firstOffset) {
  // reset stream
  stream.reset(value);

  // get token attribute
  TRI_ASSERT(irs::get<irs::term_attribute>(stream));
  irs::term_attribute const* token = irs::get<irs::term_attribute>(stream);
  TRI_ASSERT(token);

  // add tokens
  for (auto* options = filter.mutable_options(); stream.next(); ) {
    irs::assign(options->push_back<irs::by_term_options>(firstOffset).term,
                token->value);

    firstOffset = 0;
  }
}

FORCE_INLINE void appendExpression(irs::boolean_filter& filter,
                                   aql::AstNode const& node,
                                   QueryContext const& ctx, FilterContext const& filterCtx) {
  auto& exprFilter = filter.add<arangodb::iresearch::ByExpression>();
  exprFilter.init(*ctx.plan, *ctx.ast, const_cast<aql::AstNode&>(node));
  exprFilter.boost(filterCtx.boost);
}

FORCE_INLINE void appendExpression(irs::boolean_filter& filter,
                                   std::shared_ptr<aql::AstNode>&& node,
                                   QueryContext const& ctx, FilterContext const& filterCtx) {
  auto& exprFilter = filter.add<arangodb::iresearch::ByExpression>();
  exprFilter.init(*ctx.plan, *ctx.ast, std::move(node));
  exprFilter.boost(filterCtx.boost);
}

Result byTerm(irs::by_term* filter, std::string&& name,
              ScopedAqlValue const& value, QueryContext const& /*ctx*/,
              FilterContext const& filterCtx) {
  switch (value.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_NULL:
      if (filter) {
        kludge::mangleNull(name);
        *filter->mutable_field() = std::move(name);
        filter->boost(filterCtx.boost);
        irs::assign(filter->mutable_options()->term,
                    irs::null_token_stream::value_null());
      }
      return {};
    case arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL:
      if (filter) {
        kludge::mangleBool(name);
        *filter->mutable_field() = std::move(name);
        filter->boost(filterCtx.boost);
        irs::assign(filter->mutable_options()->term,
                    irs::boolean_token_stream::value(value.getBoolean()));
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
        irs::term_attribute const* token = irs::get<irs::term_attribute>(stream);
        TRI_ASSERT(token);
        stream.reset(dblValue);
        stream.next();

        *filter->mutable_field() = std::move(name);
        filter->boost(filterCtx.boost);
        irs::assign(filter->mutable_options()->term,
                    token->value);
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
        kludge::mangleField(name, filterCtx.analyzer);
        *filter->mutable_field() = std::move(name);
        filter->boost(filterCtx.boost);
        irs::assign(filter->mutable_options()->term,
                    irs::ref_cast<irs::byte_type>(strValue));
      }
      return {};
    default:
      // unsupported value type
      return {TRI_ERROR_BAD_PARAMETER, "unsupported type"};
  }
}

Result byTerm(irs::by_term* filter, aql::AstNode const& attribute,
              ScopedAqlValue const& value, QueryContext const& ctx,
              FilterContext const& filterCtx) {
  std::string name;
  if (filter && !arangodb::iresearch::nameFromAttributeAccess(name, attribute, ctx)) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Failed to generate field name from node "s.append(aql::AstNode::toString(&attribute))
    };
  }

  return byTerm(filter, std::move(name), value, ctx, filterCtx);
}

Result byTerm(irs::by_term* filter, arangodb::iresearch::NormalizedCmpNode const& node,
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

Result byRange(irs::boolean_filter* filter, aql::AstNode const& attribute,
               aql::Range const& rangeData, QueryContext const& ctx,
               FilterContext const& filterCtx) {
  TRI_ASSERT(attribute.isDeterministic());

  std::string name;

  if (filter && !nameFromAttributeAccess(name, attribute, ctx)) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Failed to generate field name from node "s.append(aql::AstNode::toString(&attribute))
    };
  }

  TRI_ASSERT(filter);
  auto& range = filter->add<irs::by_granular_range>();

  kludge::mangleNumeric(name);
  *range.mutable_field() = std::move(name);
  range.boost(filterCtx.boost);

  irs::numeric_token_stream stream;

  // setup min bound
  stream.reset(static_cast<double_t>(rangeData._low));

  auto* opts = range.mutable_options();
  irs::set_granular_term(opts->range.min, stream);
  opts->range.min_type = irs::BoundType::INCLUSIVE;

  // setup max bound
  stream.reset(static_cast<double_t>(rangeData._high));
  irs::set_granular_term(opts->range.max, stream);
  opts->range.max_type = irs::BoundType::INCLUSIVE;

  return {};
}

Result byRange(irs::boolean_filter* filter, aql::AstNode const& attributeNode,
               ScopedAqlValue const& min, bool const minInclude,
               ScopedAqlValue const& max, bool const maxInclude,
               QueryContext const& ctx, FilterContext const& filterCtx) {
  std::string name;

  if (filter && !nameFromAttributeAccess(name, attributeNode, ctx)) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Failed to generate field name from node "s.append(aql::AstNode::toString(&attributeNode))
    };
  }

  switch (min.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_NULL: {
      if (filter) {
        kludge::mangleNull(name);

        auto& range = filter->add<irs::by_range>();
        *range.mutable_field() = std::move(name);
        range.boost(filterCtx.boost);
        auto* opts = range.mutable_options();
        irs::assign(opts->range.min, irs::null_token_stream::value_null());
        opts->range.min_type = minInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
        irs::assign(opts->range.max, irs::null_token_stream::value_null());
        opts->range.max_type = maxInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
      }

      return {};
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL: {
      if (filter) {
        kludge::mangleBool(name);

        auto& range = filter->add<irs::by_range>();
        *range.mutable_field() = std::move(name);
        range.boost(filterCtx.boost);
        auto* opts = range.mutable_options();
        irs::assign(opts->range.min, irs::boolean_token_stream::value(min.getBoolean()));
        opts->range.min_type = minInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
        irs::assign(opts->range.max, irs::boolean_token_stream::value(max.getBoolean()));
        opts->range.max_type = maxInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
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
        *range.mutable_field() = std::move(name);
        range.boost(filterCtx.boost);

        irs::numeric_token_stream stream;
        auto* opts = range.mutable_options();

        // setup min bound
        stream.reset(minDblValue);
        irs::set_granular_term(opts->range.min, stream);
        opts->range.min_type = minInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;

        // setup max bound
        stream.reset(maxDblValue);
        irs::set_granular_term(opts->range.max, stream);
        opts->range.max_type = maxInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
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
        kludge::mangleField(name, filterCtx.analyzer);
        *range.mutable_field() = std::move(name);
        range.boost(filterCtx.boost);

        auto* opts = range.mutable_options();
        irs::assign(opts->range.min, irs::ref_cast<irs::byte_type>(minStrValue));
        opts->range.min_type = minInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
        irs::assign(opts->range.max, irs::ref_cast<irs::byte_type>(maxStrValue));
        opts->range.max_type = maxInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
      }

      return {};
    }
    default:
      // wrong value type
      return {TRI_ERROR_BAD_PARAMETER, "invalid value type"};
  }
}

template<bool Min>
Result byRange(irs::boolean_filter* filter, std::string name, const ScopedAqlValue& value,
               bool const incl, QueryContext const& /*ctx*/, FilterContext const& filterCtx) {
  switch (value.type()) {
    case arangodb::iresearch::SCOPED_VALUE_TYPE_NULL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        kludge::mangleNull(name);
        *range.mutable_field() = std::move(name);
        range.boost(filterCtx.boost);
        auto* opts = range.mutable_options();
        irs::assign(Min ? opts->range.min : opts->range.max,
                    irs::null_token_stream::value_null());
        (Min ? opts->range.min_type : opts->range.max_type) =
            incl ? irs::BoundType::INCLUSIVE  : irs::BoundType::EXCLUSIVE;
      }

      return {};
    }
    case arangodb::iresearch::SCOPED_VALUE_TYPE_BOOL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        kludge::mangleBool(name);
        *range.mutable_field() = std::move(name);
        range.boost(filterCtx.boost);
        auto* opts = range.mutable_options();
        irs::assign(Min ? opts->range.min : opts->range.max,
                    irs::boolean_token_stream::value(value.getBoolean()));
        (Min ? opts->range.min_type : opts->range.max_type) =
            incl ? irs::BoundType::INCLUSIVE  : irs::BoundType::EXCLUSIVE;
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
        *range.mutable_field() = std::move(name);
        range.boost(filterCtx.boost);

        stream.reset(dblValue);
        auto* opts = range.mutable_options();
        irs::set_granular_term(Min ? opts->range.min : opts->range.max,
                               stream);
        (Min ? opts->range.min_type : opts->range.max_type) =
            incl ? irs::BoundType::INCLUSIVE  : irs::BoundType::EXCLUSIVE;
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
        kludge::mangleField(name, filterCtx.analyzer);
        *range.mutable_field() = std::move(name);
        range.boost(filterCtx.boost);
        auto* opts = range.mutable_options();
        irs::assign(Min ? opts->range.min : opts->range.max,
                    irs::ref_cast<irs::byte_type>(strValue));
        (Min ? opts->range.min_type : opts->range.max_type) =
            incl ? irs::BoundType::INCLUSIVE  : irs::BoundType::EXCLUSIVE;
      }

      return {};
    }
    default:
      // wrong value type
      return {TRI_ERROR_BAD_PARAMETER, "invalid value type"};
  }
}

template<bool Min>
Result byRange(irs::boolean_filter* filter,
               arangodb::iresearch::NormalizedCmpNode const& node, bool const incl,
               QueryContext const& ctx, FilterContext const& filterCtx) {
  TRI_ASSERT(node.attribute && node.attribute->isDeterministic());
  TRI_ASSERT(node.value && node.value->isDeterministic());

  std::string name;
  if (filter && !nameFromAttributeAccess(name, *node.attribute, ctx)) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Failed to generate field name from node "s.append(aql::AstNode::toString(node.attribute))
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
  return byRange<Min>(filter, name, value, incl, ctx, filterCtx);
}

Result fromExpression(irs::boolean_filter* filter, QueryContext const& ctx,
                    FilterContext const& filterCtx,
                    std::shared_ptr<aql::AstNode>&& node) {
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

Result fromExpression(irs::boolean_filter* filter, QueryContext const& ctx,
                      FilterContext const& filterCtx, aql::AstNode const& node) {
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

// GEO_IN_RANGE(attribute, shape, lower, upper[, includeLower = true, includeUpper = true])
Result fromFuncGeoInRange(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& args) {
  TRI_ASSERT(funcName);
  using ArgsRange = error::Range<4, 6>;

  if (!args.isDeterministic()) {
    return error::nondeterministicArgs(funcName);
  }

  auto const argc = args.numMembers();


  if (argc < ArgsRange::MIN || argc > ArgsRange::MAX) {
    return error::invalidArgsCount<ArgsRange>(funcName);
  }

  auto const* fieldNode = args.getMemberUnchecked(0);
  auto const* centroidNode = args.getMemberUnchecked(1);
  size_t fieldNodeIdx = 1;
  size_t centroidNodeIdx = 2;

  if (!arangodb::iresearch::checkAttributeAccess(fieldNode, *ctx.ref)) {
    if (!arangodb::iresearch::checkAttributeAccess(centroidNode, *ctx.ref)) {
      return {
        TRI_ERROR_BAD_PARAMETER,
         "'"s.append(funcName)
             .append("' AQL function: Unable to find argument denoting an attribute identifier")
      };
    }

    std::swap(fieldNode, centroidNode);
    fieldNodeIdx = 2;
    centroidNodeIdx = 1;
  }

  if (!fieldNode) {
    return error::invalidAttribute(funcName, fieldNodeIdx);
  }

  if (!centroidNode) {
    return error::invalidAttribute(funcName, centroidNodeIdx);
  }

  bool const buildFilter = filter;

  S2LatLng centroid;
  ScopedAqlValue tmpValue(*centroidNode);
  if (buildFilter || tmpValue.isConstant()) {
    if (!tmpValue.execute(ctx)) {
      return error::failedToEvaluate(funcName, centroidNodeIdx);
    }

    auto const res = getLatLong(tmpValue, centroid, funcName, centroidNodeIdx);

    if (res.fail()) {
      return res;
    }
  }

  double_t minDistance = 0;

  auto rv = evaluateArg(minDistance, tmpValue, funcName, args, 2, buildFilter, ctx);

  if (rv.fail()) {
    return rv;
  }

  double_t maxDistance = 0;

  rv = evaluateArg(maxDistance, tmpValue, funcName, args, 3, buildFilter, ctx);

  if (rv.fail()) {
    return rv;
  }

  bool includeMin = true;
  bool includeMax = true;

  if (argc > 4) {
    rv = evaluateArg(includeMin, tmpValue, funcName, args, 4, buildFilter, ctx);

    if (rv.fail()) {
      return rv;
    }

    if (argc > 5) {
      rv = evaluateArg(includeMax, tmpValue, funcName, args, 5, buildFilter, ctx);

      if (rv.fail()) {
        return rv;
      }
    }
  }

  if (filter) {
    std::string name;

    if (!nameFromAttributeAccess(name, *fieldNode, ctx)) {
      return error::failedToGenerateName(funcName, fieldNodeIdx);
    }

    auto& geo_filter = filter->add<GeoDistanceFilter>();
    geo_filter.boost(filterCtx.boost);

    auto* options = geo_filter.mutable_options();
    setupGeoFilter(filterCtx.analyzer, options->options);

    options->origin = centroid.ToPoint();
    if (minDistance != 0.) {
      options->range.min = minDistance;
      options->range.min_type = includeMin
        ? irs::BoundType::INCLUSIVE
        : irs::BoundType::EXCLUSIVE;
    }
    options->range.max = maxDistance;
    options->range.max_type = includeMax
      ? irs::BoundType::INCLUSIVE
      : irs::BoundType::EXCLUSIVE;

    TRI_ASSERT(filterCtx.analyzer);
    kludge::mangleField(name, filterCtx.analyzer);
    *geo_filter.mutable_field() = std::move(name);
  }

  return {};
}

// GEO_DISTANCE(.. , ..) <|<=|==|>|>= Distance
Result fromGeoDistanceInterval(
    irs::boolean_filter* filter,
    arangodb::iresearch::NormalizedCmpNode const& node,
    QueryContext const& ctx, FilterContext const& filterCtx) {

  TRI_ASSERT(node.attribute &&
             node.attribute->isDeterministic() &&
             aql::NODE_TYPE_FCALL == node.attribute->type &&
             &aql::Functions::GeoDistance ==
               reinterpret_cast<aql::Function const*>(node.attribute->getData())->implementation);
  TRI_ASSERT(node.value && node.value->isDeterministic());

  auto* args = node.attribute->getMemberUnchecked(0);
  TRI_ASSERT(args);

  if (args->numMembers() != 2) {
    return {TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH};
  }

  auto* fieldNode = args->getMemberUnchecked(0);
  auto* centroidNode = args->getMemberUnchecked(1);
  size_t fieldNodeIdx = 1;
  size_t centroidNodeIdx = 2;

  if (!arangodb::iresearch::checkAttributeAccess(fieldNode, *ctx.ref)) {
    if (!arangodb::iresearch::checkAttributeAccess(centroidNode, *ctx.ref)) {
      return {TRI_ERROR_BAD_PARAMETER};
    }

    std::swap(fieldNode, centroidNode);
    centroidNodeIdx = 1;
    fieldNodeIdx = 2;
  }

  if (arangodb::iresearch::findReference(*centroidNode, *ctx.ref)) {
    // centroid contains referenced variable
    return {TRI_ERROR_BAD_PARAMETER};
  }

  S2LatLng centroid;
  ScopedAqlValue centroidValue(*centroidNode);
  if (filter || centroidValue.isConstant()) {
    if (!centroidValue.execute(ctx)) {
      return error::failedToEvaluate(GEO_DISTANCE_FUNC, centroidNodeIdx);
    }

    auto const res = getLatLong(centroidValue, centroid, GEO_DISTANCE_FUNC, centroidNodeIdx);

    if (res.fail()) {
      return res;
    }
  }

  double_t distance;
  ScopedAqlValue distanceValue(*node.value);
  if (filter || distanceValue.isConstant()) {
    if (!distanceValue.execute(ctx)) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "Failed to evaluate an argument denoting a distance near '"s +
        GEO_DISTANCE_FUNC + "' function"
      };
    }

    if (SCOPED_VALUE_TYPE_DOUBLE != distanceValue.type() ||
        !distanceValue.getDouble(distance)) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "Failed to parse an argument denoting a distance as a number near '"s +
        GEO_DISTANCE_FUNC + "' function"
      };
    }
  }

  if (filter) {
    std::string name;

    if (!nameFromAttributeAccess(name, *fieldNode, ctx)) {
      return error::failedToGenerateName(GEO_DISTANCE_FUNC, fieldNodeIdx);
    }

    auto& geo_filter = (aql::NODE_TYPE_OPERATOR_BINARY_NE == node.cmp
                       ? filter->add<irs::Not>().filter<GeoDistanceFilter>()
                       : filter->add<GeoDistanceFilter>());

    geo_filter.boost(filterCtx.boost);

    auto* options = geo_filter.mutable_options();
    setupGeoFilter(filterCtx.analyzer, options->options);

    options->origin = centroid.ToPoint();

    switch (node.cmp) {
      case aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      case aql::NODE_TYPE_OPERATOR_BINARY_NE:
        options->range.min = distance;
        options->range.min_type = irs::BoundType::INCLUSIVE;
        options->range.max = distance;
        options->range.max_type = irs::BoundType::INCLUSIVE;
        break;
      case aql::NODE_TYPE_OPERATOR_BINARY_LT:
      case aql::NODE_TYPE_OPERATOR_BINARY_LE:
        options->range.max = distance;
        options->range.max_type = aql::NODE_TYPE_OPERATOR_BINARY_LE == node.cmp
          ? irs::BoundType::INCLUSIVE
          : irs::BoundType::EXCLUSIVE;
        break;
      case aql::NODE_TYPE_OPERATOR_BINARY_GT:
      case aql::NODE_TYPE_OPERATOR_BINARY_GE:
        options->range.min = distance;
        options->range.min_type = aql::NODE_TYPE_OPERATOR_BINARY_GE == node.cmp
          ? irs::BoundType::INCLUSIVE
          : irs::BoundType::EXCLUSIVE;
        break;
      default:
        TRI_ASSERT(false);
        return {TRI_ERROR_BAD_PARAMETER};
    }

    TRI_ASSERT(filterCtx.analyzer);
    kludge::mangleField(name, filterCtx.analyzer);
    *geo_filter.mutable_field() = std::move(name);
  }

  return {};
}

Result fromInterval(irs::boolean_filter* filter, QueryContext const& ctx,
                    FilterContext const& filterCtx, aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == node.type);

  arangodb::iresearch::NormalizedCmpNode normNode;

  if (!arangodb::iresearch::normalizeCmpNode(node, *ctx.ref, normNode)) {
    if (arangodb::iresearch::normalizeGeoDistanceCmpNode(node, *ctx.ref, normNode)) {
      if (fromGeoDistanceInterval(filter, normNode, ctx, filterCtx).ok()) {
        return {};
      }
    }

    return fromExpression(filter, ctx, filterCtx, node);
  }

  bool const incl = aql::NODE_TYPE_OPERATOR_BINARY_GE == normNode.cmp ||
                    aql::NODE_TYPE_OPERATOR_BINARY_LE == normNode.cmp;

  bool const min = aql::NODE_TYPE_OPERATOR_BINARY_GT == normNode.cmp ||
                   aql::NODE_TYPE_OPERATOR_BINARY_GE == normNode.cmp;

  return min ? byRange<true>(filter, normNode, incl, ctx, filterCtx)
             : byRange<false>(filter, normNode, incl, ctx, filterCtx);
}

Result fromBinaryEq(irs::boolean_filter* filter, QueryContext const& ctx,
                  FilterContext const& filterCtx, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_EQ == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type);

  arangodb::iresearch::NormalizedCmpNode normalized;

  if (!arangodb::iresearch::normalizeCmpNode(node, *ctx.ref, normalized)) {
    if (arangodb::iresearch::normalizeGeoDistanceCmpNode(node, *ctx.ref, normalized)) {
      if (fromGeoDistanceInterval(filter, normalized, ctx, filterCtx).ok()) {
        return {};
      }
    }

    auto rv = fromExpression(filter, ctx, filterCtx, node);
    return rv.reset(rv.errorNumber(), "in from binary equation" + rv.errorMessage());
  }

  irs::by_term* termFilter = nullptr;

  if (filter) {
    termFilter = &(aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type
                       ? filter->add<irs::Not>().filter<irs::by_term>()
                       : filter->add<irs::by_term>());
  }

  return byTerm(termFilter, normalized, ctx, filterCtx);
}

Result fromRange(irs::boolean_filter* filter, QueryContext const& /*ctx*/,
                 FilterContext const& filterCtx, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_RANGE == node.type);

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

std::pair<Result, aql::AstNodeType> buildBinaryArrayComparisonPreFilter(
    irs::boolean_filter* &filter, aql::AstNodeType arrayComparison,
    const aql::AstNode* qualifierNode, size_t arraySize) {
  TRI_ASSERT(qualifierNode);
  auto qualifierType = qualifierNode->getIntValue(true);
  aql::AstNodeType expansionNodeType = aql::NODE_TYPE_ROOT;
  if (0 == arraySize) {
    expansionNodeType = aql::NODE_TYPE_ROOT; // no subfilters expansion needed
    switch (qualifierType) {
      case aql::Quantifier::ANY:
        if (filter) {
          filter->add<irs::empty>();
        }
        break;
      case aql::Quantifier::ALL:
      case aql::Quantifier::NONE:
        if (filter) {
          filter->add<irs::all>();
        }
        break;
      default:
        TRI_ASSERT(false); // new qualifier added ?
        return std::make_pair(
            Result(TRI_ERROR_NOT_IMPLEMENTED, "Unknown qualifier in Array comparison operator"),
            aql::AstNodeType::NODE_TYPE_ROOT);
    }
  } else {
    // NONE is inverted ALL so do conversion
    if (aql::Quantifier::NONE == qualifierType) {
      qualifierType = aql::Quantifier::ALL;
      switch (arrayComparison) {
        case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
        case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
          arrayComparison = aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN;
          break;
        case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
        case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
          arrayComparison = aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN;
          break;
        case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
          arrayComparison = aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT;
          break;
        case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
          arrayComparison = aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE;
          break;
        case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
          arrayComparison = aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT;
          break;
        case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
          arrayComparison = aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE;
          break;
        default:
          TRI_ASSERT(false); // new array comparison operator?
          return std::make_pair(
              Result(TRI_ERROR_NOT_IMPLEMENTED, "Unknown Array NONE comparison operator"),
              aql::AstNodeType::NODE_TYPE_ROOT);
      }
    }
    switch (qualifierType) {
      case aql::Quantifier::ALL:
        // calculate node type for expanding operation
        // As soon as array is left argument but for filter we place document to the left
        // we reverse comparison operation
        switch (arrayComparison) {
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::And>());
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_EQ;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Not>().filter<irs::Or>());
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_EQ;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::And>());
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_GT;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::And>());
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_GE;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::And>());
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_LT;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::And>());
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_LE;
            break;
          default:
            TRI_ASSERT(false); // new array comparison operator?
            return std::make_pair(
                Result(TRI_ERROR_NOT_IMPLEMENTED, "Unknown Array ALL/NONE comparison operator"),
                aql::AstNodeType::NODE_TYPE_ROOT);
        }
        break;
      case aql::Quantifier::ANY: {
        switch (arrayComparison) {
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Or>());
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_EQ;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Not>().filter<irs::And>());
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_EQ;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Or>());
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_LT;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Or>());
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_LE;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Or>());
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_GT;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
            if (filter) {
              filter = static_cast<irs::boolean_filter*>(&filter->add<irs::Or>());
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_GE;
            break;
          default:
            TRI_ASSERT(false); // new array comparison operator?
            return std::make_pair(
                Result(TRI_ERROR_NOT_IMPLEMENTED, "Unknown Array ANY comparison operator"),
                aql::AstNodeType::NODE_TYPE_ROOT);
        }
        break;
      }
      default:
        TRI_ASSERT(false); // new qualifier added ?
        return std::make_pair(
            Result(TRI_ERROR_NOT_IMPLEMENTED, "Unknown qualifier in Array comparison operator"),
            aql::AstNodeType::NODE_TYPE_ROOT);
    }
  }
  return std::make_pair(TRI_ERROR_NO_ERROR, expansionNodeType);
}

class ByTermSubFilterFactory {
 public:
  static Result byNodeSubFilter(irs::boolean_filter* filter,
                                arangodb::iresearch::NormalizedCmpNode const& node,
                                QueryContext const& ctx, FilterContext const& filterCtx) {
    TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_EQ == node.cmp);
    irs::by_term* termFilter =  nullptr;
    if (filter) {
      termFilter = &filter->add<irs::by_term>();
    }
    return byTerm(termFilter, node, ctx, filterCtx);
  }

  static Result byValueSubFilter(irs::boolean_filter* filter, std::string fieldName, const ScopedAqlValue& value,
                                 aql::AstNodeType arrayExpansionNodeType,
                                 QueryContext const& ctx, FilterContext const& filterCtx) {
    TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_EQ == arrayExpansionNodeType);
    irs::by_term* termFilter =  nullptr;
    if (filter) {
      termFilter = &filter->add<irs::by_term>();
    }
    return byTerm(termFilter, std::move(fieldName), value, ctx, filterCtx);
  }
};

class ByRangeSubFilterFactory {
 public:
  static Result byNodeSubFilter(irs::boolean_filter* filter,
                                arangodb::iresearch::NormalizedCmpNode const& node,
                                QueryContext const& ctx, FilterContext const& filterCtx) {
    bool incl, min;
    std::tie(min, incl) = calcMinInclude(node.cmp);
    return min ? byRange<true>(filter, node, incl, ctx, filterCtx)
               : byRange<false>(filter, node, incl, ctx, filterCtx);
  }

  static Result byValueSubFilter(irs::boolean_filter* filter, std::string fieldName, const ScopedAqlValue& value,
                                 aql::AstNodeType arrayExpansionNodeType,
                                 QueryContext const& ctx, FilterContext const& filterCtx) {
    bool incl, min;
    std::tie(min, incl) = calcMinInclude(arrayExpansionNodeType);
    return min ? byRange<true>(filter, fieldName, value, incl, ctx, filterCtx)
               : byRange<false>(filter, fieldName, value, incl, ctx, filterCtx);
  }

 private:
  static std::pair<bool, bool> calcMinInclude(aql::AstNodeType arrayExpansionNodeType) {
    TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_LT == arrayExpansionNodeType ||
               aql::NODE_TYPE_OPERATOR_BINARY_LE == arrayExpansionNodeType ||
               aql::NODE_TYPE_OPERATOR_BINARY_GT == arrayExpansionNodeType ||
               aql::NODE_TYPE_OPERATOR_BINARY_GE == arrayExpansionNodeType);
    return std::pair<bool, bool>(
        // min
        aql::NODE_TYPE_OPERATOR_BINARY_GT == arrayExpansionNodeType ||
        aql::NODE_TYPE_OPERATOR_BINARY_GE == arrayExpansionNodeType,
        // incl
        aql::NODE_TYPE_OPERATOR_BINARY_GE == arrayExpansionNodeType ||
        aql::NODE_TYPE_OPERATOR_BINARY_LE == arrayExpansionNodeType);
  }
};


template<typename SubFilterFactory>
Result fromArrayComparison(irs::boolean_filter*& filter, QueryContext const& ctx,
                           FilterContext const& filterCtx, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN == node.type);
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

  if (qualifierNode->type != aql::NODE_TYPE_QUANTIFIER) {
    return { TRI_ERROR_BAD_PARAMETER, "wrong qualifier node type for Array comparison operator" };
  }
    if (aql::NODE_TYPE_ARRAY == valueNode->type) {
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
    Result buildRes;
    aql::AstNodeType arrayExpansionNodeType;
    std::tie(buildRes, arrayExpansionNodeType) = buildBinaryArrayComparisonPreFilter(filter, node.type, qualifierNode, n);
    if (!buildRes.ok()) {
      return buildRes;
    }
    if (filter) {
      filter->boost(filterCtx.boost);
    }
    if (aql::NODE_TYPE_ROOT == arrayExpansionNodeType) {
      // nothing to do more
      return {};
    }
    FilterContext const subFilterCtx{
      filterCtx.analyzer,
      irs::no_boost() };  // reset boost
    // Expand array interval as several binaryInterval nodes ('array' feature is ensured by pre-filter)
    arangodb::iresearch::NormalizedCmpNode normalized;
    aql::AstNode toNormalize(arrayExpansionNodeType);
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
        auto exprNode = std::make_shared<aql::AstNode>(arrayExpansionNodeType);
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
      Result buildRes;
      aql::AstNodeType arrayExpansionNodeType;
      std::tie(buildRes, arrayExpansionNodeType) = buildBinaryArrayComparisonPreFilter(filter, node.type, qualifierNode, n);
      if (!buildRes.ok()) {
        return buildRes;
      }
      filter->boost(filterCtx.boost);
      if (aql::NODE_TYPE_ROOT == arrayExpansionNodeType) {
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
          "Failed to generate field name from node " + aql::AstNode::toString(attributeNode)
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

Result fromInArray(irs::boolean_filter* filter, QueryContext const& ctx,
                   FilterContext const& filterCtx, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_IN == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type);

  // `attributeNode` IN `valueNode`
  auto const* attributeNode = node.getMemberUnchecked(0);
  TRI_ASSERT(attributeNode);
  auto const* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(valueNode && aql::NODE_TYPE_ARRAY == valueNode->type);

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
      if (aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
        filter->add<irs::all>().boost(filterCtx.boost);  // not in [] means 'all'
      } else {
        filter->add<irs::empty>();
      }
    }

    // nothing to do more
    return {};
  }

  if (filter) {
    filter = aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type
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
  aql::AstNode toNormalize(aql::NODE_TYPE_OPERATOR_BINARY_EQ);
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
      auto exprNode = std::make_shared<aql::AstNode>(aql::NODE_TYPE_OPERATOR_BINARY_EQ);
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

Result fromIn(irs::boolean_filter* filter, QueryContext const& ctx,
              FilterContext const& filterCtx, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_IN == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type);

  if (node.numMembers() != 2) {
    auto rv = error::malformedNode(node.type);
    return rv.reset(rv.errorNumber(), "error in from In" + rv.errorMessage());
  }

  auto const* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(valueNode);

  if (aql::NODE_TYPE_ARRAY == valueNode->type) {
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

  if (aql::NODE_TYPE_RANGE == valueNode->type) {
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

    if (aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
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
        if (aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
          filter->add<irs::all>().boost(filterCtx.boost);  // not in [] means 'all'
        } else {
          filter->add<irs::empty>();
        }

        // nothing to do more
        return {};
      }

      filter = aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type
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

      if (aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
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

Result fromNegation(irs::boolean_filter* filter, QueryContext const& ctx,
                    FilterContext const& filterCtx, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_OPERATOR_UNARY_NOT == node.type);

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
                        aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_AND == node.type ||
             aql::NODE_TYPE_OPERATOR_NARY_AND == node.type);

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
        aql::NODE_TYPE_OPERATOR_BINARY_GE == lhsNormNode.cmp;
    bool const rhsInclude =
        aql::NODE_TYPE_OPERATOR_BINARY_LE == rhsNormNode.cmp;

    if ((lhsInclude ||
         aql::NODE_TYPE_OPERATOR_BINARY_GT == lhsNormNode.cmp) &&
        (rhsInclude ||
         aql::NODE_TYPE_OPERATOR_BINARY_LT == rhsNormNode.cmp)) {
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
Result fromGroup(irs::boolean_filter* filter, QueryContext const& ctx,
                 FilterContext const& filterCtx, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_AND == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_OR == node.type ||
             aql::NODE_TYPE_OPERATOR_NARY_AND == node.type ||
             aql::NODE_TYPE_OPERATOR_NARY_OR == node.type);

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

    auto const rv = ::filter(filter, ctx, subFilterCtx, *valueNode);
    if (rv.fail()) {
      return rv;
    }
  }

  return {};
}

// ANALYZER(<filter-expression>, analyzer)
Result fromFuncAnalyzer(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& args) {
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
    analyzer = analyzerFeature.get(analyzerId, ctx.trx->vocbase(),
                                   ctx.trx->state()->analyzersRevision());
    if (!analyzer) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "'"s.append(funcName).append("' AQL function: Unable to lookup analyzer '")
            .append(analyzerId.c_str()).append("'")
      };
    }

    shortName = arangodb::iresearch::IResearchAnalyzerFeature::normalize(  // normalize
      analyzerId, ctx.trx->vocbase().name(), false);  // args

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
Result fromFuncBoost(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& args) {
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
Result fromFuncExists(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& args) {
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
      basics::StringUtils::tolowerInPlace(strArg);  // normalize user input
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
             kludge::mangleField(name, analyzer);
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
    *exists.mutable_field() = std::move(fieldName);
    exists.boost(filterCtx.boost);
    auto* opts = exists.mutable_options();
    opts->prefix_match = prefixMatch;
  }

  return {};
}

// MIN_MATCH(<filter-expression>[, <filter-expression>,...], <min-match-count>)
Result fromFuncMinMatch(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& args) {
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

template<typename ElementType>
class ArgsTraits;

template<>
class ArgsTraits<aql::AstNode> {
 public:
  using ValueType = ScopedAqlValue;

  static ScopedValueType scopedType(ValueType const& v) {
    return v.type();
  }

  static VPackSlice valueSlice(ValueType const& v) {
    return v.slice();
  }

  static size_t numValueMembers(ValueType const& v) {
    return v.size();
  }

  static bool isValueNumber(ValueType const& v) noexcept {
    return v.isDouble();
  }

  static int64_t getValueInt64(ValueType const& v) {
    TRI_ASSERT(v.isDouble());
    return v.getInt64();
  }

  static bool getValueString(ValueType const& v, irs::string_ref& str) {
    return v.getString(str);
  }

  static bool isDeterministic(aql::AstNode const& arg) {
    return arg.isDeterministic();
  }

  static auto numMembers(aql::AstNode const& arg) {
    return arg.numMembers();
  }

  static Result getMemberValue(aql::AstNode const& arg, size_t idx,
                               char const* funcName, ValueType& value,
                               bool isFilter, QueryContext const& ctx,
                               bool& skippedEvaluation) {
    TRI_ASSERT(arg.isArray());
    TRI_ASSERT(arg.numMembers() > idx);
    auto member = arg.getMemberUnchecked(idx);
    if (member) {
      value.reset(*member);
      if (!member->isConstant()) {
        if (isFilter) {
          if (!value.execute(ctx)) {
            return error::failedToEvaluate(funcName, idx);
          }
        } else {
          skippedEvaluation = true;
        }
      }
    } else {
      return error::invalidArgument(funcName, idx);
    }
    return {};
  }

  template<typename T, bool CheckDeterminism = false>
  static Result evaluateArg(T& out, ValueType& value, char const* funcName,
                                      aql::AstNode const& args, size_t i, bool isFilter,
                                      QueryContext const& ctx) {
    return ::evaluateArg<T, CheckDeterminism>(out, value, funcName, args, i, isFilter, ctx);
  }
};

template<>
class ArgsTraits<VPackSlice> {
 public:
  using ValueType = VPackSlice;

  static ScopedValueType scopedType(ValueType const& v) {
    if (v.isNumber()) {
      return SCOPED_VALUE_TYPE_DOUBLE;
    }
    switch (v.type()) {
      case VPackValueType::String:
        return SCOPED_VALUE_TYPE_STRING;
      case VPackValueType::Bool:
        return SCOPED_VALUE_TYPE_BOOL;
      case VPackValueType::Array:
        return SCOPED_VALUE_TYPE_ARRAY;
      case VPackValueType::Object:
        return SCOPED_VALUE_TYPE_OBJECT;
      case VPackValueType::Null:
        return SCOPED_VALUE_TYPE_NULL;
      default:
        break; // Make Clang happy
    }
    return SCOPED_VALUE_TYPE_INVALID;
  }

  static ValueType valueSlice(ValueType v) noexcept {
    return v;
  }

  static size_t numValueMembers(ValueType const& v) {
    TRI_ASSERT(v.isArray());
    return v.length();
  }

  static bool isValueNumber(ValueType const& v) noexcept {
    return v.isNumber();
  }

  static int64_t getValueInt64(ValueType const& v) {
    TRI_ASSERT(v.isNumber());
    return v.getNumber<int64_t>();
  }

  static bool getValueString(ValueType const& v, irs::string_ref& str) {
    if (v.isString()) {
      str = ::getStringRef(v);
      return true;
    }
    return false;
  }

  constexpr static bool isDeterministic(VPackSlice) {
    return true;
  }

  static size_t numMembers(VPackSlice arg) {
    if (arg.isArray()) {
      return arg.length();
    }
    return 1;
  }

  static Result getMemberValue(VPackSlice arg, size_t idx,
                                         char const* funcName, ValueType& value,
                                         bool, QueryContext const&, bool&) {
    TRI_ASSERT(arg.isArray());
    TRI_ASSERT(arg.length() > idx);
    value = arg.at(idx);
    return {};
  }

  template<typename T>
  static Result evaluateArg(T& out, ValueType& value, char const* funcName,
    VPackSlice args, size_t i, bool isFilter, QueryContext const& ctx) {
    static_assert(
      std::is_same<T, irs::string_ref>::value ||
      std::is_same<T, int64_t>::value ||
      std::is_same<T, double_t>::value ||
      std::is_same<T, bool>::value);

    if (!args.isArray() || args.length() <= i) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "'"s.append(funcName).append("' AQL function: invalid argument index ")
        .append(std::to_string(i))
      };
    }
    value = args.at(i);
    if constexpr (std::is_same<T, irs::string_ref>::value) {
      if (value.isString()) {
        out = getStringRef(value);
        return {};
      }
    } else if constexpr (std::is_same<T, int64_t>::value) {
      if (value.isNumber()) {
        out = value.getInt();
        return {};
      }
    } else if constexpr (std::is_same<T, double>::value) {
      if (value.getDouble(out)) {
        return {};
      }
    } else if constexpr (std::is_same<T, bool>::value) {
      if (value.isBoolean()) {
        out = value.getBoolean();
        return {};
      }
    }
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function: argument at position '").append(std::to_string(i+1))
      .append("' has invalid type '").append(value.typeName()).append("'")
    };
  }
};

typedef std::function<
  Result(char const*,
         size_t const,
         char const*,
         irs::by_phrase*,
         QueryContext const&,
         VPackSlice,
         size_t,
         irs::analysis::analyzer*)
> ConversionPhraseHandler;

std::string getSubFuncErrorSuffix(char const* funcName, size_t const funcArgumentPosition) {
  return " (in '"s.append(funcName).append("' AQL function at position '")
      .append(std::to_string(funcArgumentPosition + 1)).append("')");
}

Result oneArgumentfromFuncPhrase(char const* funcName,
                                 size_t const funcArgumentPosition,
                                 char const* subFuncName,
                                 VPackSlice elem,
                                 irs::string_ref& term) {
  if (elem.isArray() && elem.length() != 1) {
    auto res = error::invalidArgsCount<error::ExactValue<1>>(subFuncName);
    return {
      res.errorNumber(),
      res.errorMessage().append(getSubFuncErrorSuffix(funcName, funcArgumentPosition))
    };
  }
  auto actualArg = elem.isArray() ? elem.at(0) : elem;

  if (!actualArg.isString()) {
    return error::typeMismatch(subFuncName, funcArgumentPosition, SCOPED_VALUE_TYPE_STRING,
                               ArgsTraits<VPackSlice>::scopedType(actualArg));
  }
  term = getStringRef(actualArg);
  return {};
}

// {<TERM>: [ '[' ] <term> [ ']' ] }
Result fromFuncPhraseTerm(char const* funcName,
                          size_t funcArgumentPosition,
                          char const* subFuncName,
                          irs::by_phrase* filter,
                          QueryContext const& ctx,
                          VPackSlice elem,
                          size_t firstOffset,
                          irs::analysis::analyzer* /*analyzer*/ = nullptr) {
  irs::string_ref term;
  auto res = oneArgumentfromFuncPhrase(funcName, funcArgumentPosition, subFuncName,
                                       elem, term);
  if (res.fail()) {
    return res;
  }

  if (filter) {
    auto* opts = filter->mutable_options();
    irs::assign(opts->push_back<irs::by_term_options>(firstOffset).term,
                irs::ref_cast<irs::byte_type>(term));
  }

  return {};
}

// {<STARTS_WITH>: [ '[' ] <term> [ ']' ] }
Result fromFuncPhraseStartsWith(char const* funcName,
                                size_t funcArgumentPosition,
                                char const* subFuncName,
                                irs::by_phrase* filter,
                                QueryContext const& ctx,
                                VPackSlice elem,
                                size_t firstOffset,
                                irs::analysis::analyzer* /*analyzer*/ = nullptr) {
  irs::string_ref term;
  auto res = oneArgumentfromFuncPhrase(funcName, funcArgumentPosition, subFuncName,
                                       elem, term);
  if (res.fail()) {
    return res;
  }
  if (filter) {
    auto& prefix = filter->mutable_options()->push_back<irs::by_prefix_options>(firstOffset);
    irs::assign(prefix.term, irs::ref_cast<irs::byte_type>(term));
    prefix.scored_terms_limit = FilterConstants::DefaultScoringTermsLimit;
  }
  return {};
}

// {<WILDCARD>: [ '[' ] <term> [ ']' ] }
Result fromFuncPhraseLike(char const* funcName,
                          size_t const funcArgumentPosition,
                          char const* subFuncName,
                          irs::by_phrase* filter,
                          QueryContext const& ctx,
                          VPackSlice elem,
                          size_t firstOffset,
                          irs::analysis::analyzer* /*analyzer*/ = nullptr) {
  irs::string_ref term;
  auto res = oneArgumentfromFuncPhrase(funcName, funcArgumentPosition, subFuncName,
                                       elem, term);
  if (res.fail()) {
    return res;
  }
  if (filter) {
    auto& wildcard = filter->mutable_options()->push_back<irs::by_wildcard_options>(firstOffset);
    irs::assign(wildcard.term, irs::ref_cast<irs::byte_type>(term));
    wildcard.scored_terms_limit = FilterConstants::DefaultScoringTermsLimit;
  }
  return {};
}

template<size_t First, typename ElementType, typename ElementTraits = ArgsTraits<ElementType>>
Result getLevenshteinArguments(char const* funcName, bool isFilter,
                               QueryContext const& ctx,
                               ElementType const& args,
                               aql::AstNode const** field,
                               typename ElementTraits::ValueType& targetValue,
                               irs::by_edit_distance_options& opts,
                               std::string const& errorSuffix = std::string()) {
  if (!ElementTraits::isDeterministic(args)) {
    auto res = error::nondeterministicArgs(funcName);
    return {
      res.errorNumber(),
      res.errorMessage().append(errorSuffix)
    };
  }
  auto const argc = ElementTraits::numMembers(args);
  constexpr size_t min = 3 - First;
  constexpr size_t max = 5 - First;
  if (argc < min || argc > max) {
    auto res = error::invalidArgsCount<error::Range<min, max>>(funcName);
    return {
      res.errorNumber(),
      res.errorMessage().append(errorSuffix)
    };
  }

  if constexpr (0 == First) { // this is done only for AstNode so don`t bother with traits
    static_assert(std::is_same_v<aql::AstNode, ElementType>, "Only AstNode supported for parsing attribute");
    TRI_ASSERT(field);
    // (0 - First) argument defines a field
    *field = arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref);

    if (!*field) {
      return error::invalidAttribute(funcName, 1);
    }
  }

  // (1 - First) argument defines a target
  irs::string_ref target;
  auto res = ElementTraits::evaluateArg(target, targetValue, funcName, args, 1 - First, isFilter, ctx);

  if (res.fail()) {
    return {
      res.errorNumber(),
      res.errorMessage().append(errorSuffix)
    };
  }

  typename ElementTraits::ValueType tmpValue; // can reuse value for int64_t and bool

  // (2 - First) argument defines a max distance
  int64_t maxDistance = 0;
  res = ElementTraits::evaluateArg(maxDistance, tmpValue, funcName, args, 2 - First, isFilter, ctx);

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
  bool withTranspositions = true;
  if (3 - First < argc) {
    res = ElementTraits::evaluateArg(withTranspositions, tmpValue, funcName, args, 3 - First, isFilter, ctx);

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

  // optional (4 - First) argument defines terms limit
  int64_t maxTerms = FilterConstants::DefaultLevenshteinTermsLimit;
  if (4 - First < argc) {
    res = ElementTraits::evaluateArg(maxTerms, tmpValue, funcName, args, 4 - First, isFilter, ctx);

    if (res.fail()) {
      return {
        res.errorNumber(),
        res.errorMessage().append(errorSuffix)
      };
    }
  }

  irs::assign(opts.term, irs::ref_cast<irs::byte_type>(target));
  opts.with_transpositions = withTranspositions;
  opts.max_distance = static_cast<irs::byte_type>(maxDistance);
  opts.max_terms = static_cast<size_t>(maxTerms);
  opts.provider = &arangodb::iresearch::getParametricDescription;

  return {};
}

// {<LEVENSHTEIN_MATCH>: '[' <term>, <max_distance> [, <with_transpositions> ] ']'}
Result fromFuncPhraseLevenshteinMatch(char const* funcName,
                                      size_t const funcArgumentPosition,
                                      char const* subFuncName,
                                      irs::by_phrase* filter,
                                      QueryContext const& ctx,
                                      VPackSlice array,
                                      size_t firstOffset,
                                      irs::analysis::analyzer* /*analyzer*/ = nullptr) {
  if (!array.isArray()) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function: '")
          .append(subFuncName)
          .append("' arguments must be in an array at position '")
          .append(std::to_string(funcArgumentPosition + 1)).append("'")
    };
  }

  VPackSlice targetValue;
  irs::by_edit_distance_options opts;
  auto res = getLevenshteinArguments<1>(subFuncName, filter != nullptr, ctx,
                                        array, nullptr, targetValue, opts,
                                        getSubFuncErrorSuffix(funcName, funcArgumentPosition));
  if (res.fail()) {
    return res;
  }

  if (filter) {
    auto* phrase = filter->mutable_options();

    if (0 != opts.max_terms) {
      TRI_ASSERT(ctx.index);

      struct top_term_visitor final : irs::filter_visitor {
        explicit top_term_visitor(size_t size)
          : collector(size) {
        }

        virtual void prepare(const irs::sub_reader& segment,
                             const irs::term_reader& field,
                             const irs::seek_term_iterator& terms) override {
          collector.prepare(segment, field, terms);
        }

        virtual void visit(irs::boost_t boost) override {
          collector.visit(boost);
        }

        irs::top_terms_collector<irs::top_term<irs::boost_t>> collector;
      } collector(opts.max_terms);

      irs::visit(*ctx.index, filter->field(),
                 irs::by_phrase::required(),
                 irs::by_edit_distance::visitor(opts),
                 collector);

      auto& terms = phrase->push_back<irs::by_terms_options>(firstOffset).terms;
      collector.collector.visit([&terms](const irs::top_term<irs::boost_t>& term) {
        terms.emplace(term.term, term.key);
      });
    } else {
      phrase->push_back<irs::by_edit_distance_filter_options>(std::move(opts), firstOffset);
    }
  }
  return {};
}



// {<TERMS>: '[' <term0> [, <term1>, ...] ']'}
template<typename ElementType, typename ElementTraits = ArgsTraits<ElementType>>
Result fromFuncPhraseTerms(char const* funcName,
                           size_t const funcArgumentPosition,
                           char const* subFuncName,
                           irs::by_phrase* filter,
                           QueryContext const& ctx,
                           ElementType const& array,
                           size_t firstOffset,
                           irs::analysis::analyzer* analyzer = nullptr) {
  if (!array.isArray()) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function: '")
          .append(subFuncName)
          .append("' arguments must be in an array at position '")
          .append(std::to_string(funcArgumentPosition + 1)).append("'")
    };
  }

  if (!ElementTraits::isDeterministic(array)) {
    auto res = error::nondeterministicArgs(subFuncName);
    return {
      res.errorNumber(),
      res.errorMessage().append(getSubFuncErrorSuffix(funcName, funcArgumentPosition))
    };
  }

  auto const argc = ElementTraits::numMembers(array);
  if (0 == argc) {
    auto res = error::invalidArgsCount<error::OpenRange<false, 1>>(subFuncName);
    return {
      res.errorNumber(),
      res.errorMessage().append(getSubFuncErrorSuffix(funcName, funcArgumentPosition))
    };
  }

  irs::by_terms_options::search_terms terms;
  typename ElementTraits::ValueType termValue;
  irs::string_ref term;
  for (size_t i = 0; i < argc; ++i) {
    auto res = ElementTraits::evaluateArg(term, termValue, subFuncName, array, i, filter != nullptr, ctx);

    if (res.fail()) {
      return {
        res.errorNumber(),
        res.errorMessage().append(getSubFuncErrorSuffix(funcName, funcArgumentPosition))
      };
    }
    if (analyzer != nullptr) {
      // reset analyzer
      analyzer->reset(term);
      // get token attribute
      irs::term_attribute const* token = irs::get<irs::term_attribute>(*analyzer);
      TRI_ASSERT(token);
      // add tokens
      while (analyzer->next()) {
        terms.emplace(token->value);
      }
    } else {
      terms.emplace(irs::ref_cast<irs::byte_type>(term));
    }
  }
  if (filter) {
    auto& opts = filter->mutable_options()->push_back<irs::by_terms_options>(firstOffset);
    opts.terms = std::move(terms);
  }
  return {};
}

template<size_t First, typename ElementType,
         typename ElementTraits = ArgsTraits<ElementType>>
Result getInRangeArguments(char const* funcName, bool isFilter,
                           QueryContext const& ctx,
                           ElementType const& args,
                           aql::AstNode const** field,
                           typename ElementTraits::ValueType& min, bool& minInclude,
                           typename ElementTraits::ValueType& max, bool& maxInclude,
                           bool& ret,
                           std::string const& errorSuffix = std::string()) {
  if (!ElementTraits::isDeterministic(args)) {
    auto res = error::nondeterministicArgs(funcName);
    return {
      res.errorNumber(),
      res.errorMessage().append(errorSuffix)
    };
  }
  auto const argc = ElementTraits::numMembers(args);

  if (5 - First != argc) {
    auto res = error::invalidArgsCount<error::ExactValue<5 - First>>(funcName);
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
    TRI_ASSERT((*field)->isDeterministic());
  }

  // (3 - First) argument defines inclusion of lower boundary
  typename ElementTraits::ValueType includeValue;
  auto res = ElementTraits::evaluateArg(minInclude, includeValue, funcName, args, 3 - First, isFilter, ctx);
  if (res.fail()) {
    return {
      res.errorNumber(),
      res.errorMessage().append(errorSuffix)
    };
  }

  // (4 - First) argument defines inclusion of upper boundary
  res = ElementTraits::evaluateArg(maxInclude, includeValue, funcName, args, 4 - First, isFilter, ctx);
  if (res.fail()) {
    return {
      res.errorNumber(),
      res.errorMessage().append(errorSuffix)
    };
  }

  // (1 - First) argument defines a lower boundary
  {
    auto res = ElementTraits::getMemberValue(args, 1 - First, funcName, min, isFilter, ctx, ret);
    if (res.fail()) {
      return {
        res.errorNumber(),
        res.errorMessage().append(errorSuffix)
      };
    }
  }
  // (2 - First) argument defines an upper boundary
  {
    auto res = ElementTraits::getMemberValue(args, 2 - First, funcName, max, isFilter, ctx, ret);
    if (res.fail()) {
      return {
        res.errorNumber(),
        res.errorMessage().append(errorSuffix)
      };
    }
  }

  if (ret) {
    return {};
  }

  if (ElementTraits::scopedType(min) != ElementTraits::scopedType(max)) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Failed to build range query, lower boundary mismatches upper boundary. "s.append(errorSuffix)
    };
  }
  return {};
}

// {<IN_RANGE>: '[' <term-low>, <term-high>, <include-low>, <include-high> ']'}
Result fromFuncPhraseInRange(char const* funcName,
                             size_t const funcArgumentPosition,
                             char const* subFuncName,
                             irs::by_phrase* filter,
                             QueryContext const& ctx,
                             VPackSlice array,
                             size_t firstOffset,
                             irs::analysis::analyzer* /*analyzer*/ = nullptr) {
  if (!array.isArray()) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function: '")
          .append(subFuncName)
          .append("' arguments must be in an array at position '")
          .append(std::to_string(funcArgumentPosition + 1)).append("'")
    };
  }

  std::string const errorSuffix = getSubFuncErrorSuffix(funcName, funcArgumentPosition);

  VPackSlice min, max;
  auto minInclude = false;
  auto maxInclude = false;
  auto ret = false;
  auto res = getInRangeArguments<1>(subFuncName, filter != nullptr, ctx, array, nullptr,
                                    min, minInclude, max, maxInclude, ret, errorSuffix);
  if (res.fail() || ret) {
    return res;
  }

  if (!min.isString()) {
    res = error::typeMismatch(subFuncName, 1, arangodb::iresearch::SCOPED_VALUE_TYPE_STRING,
                              ArgsTraits<VPackSlice>::scopedType(min));
    return {
      res.errorNumber(),
      res.errorMessage().append(errorSuffix)
    };
  }
  irs::string_ref const minStrValue = getStringRef(min);

  if (!max.isString()) {
    res = error::typeMismatch(subFuncName, 2, arangodb::iresearch::SCOPED_VALUE_TYPE_STRING,
                              ArgsTraits<VPackSlice>::scopedType(max));
    return {
      res.errorNumber(),
      res.errorMessage().append(errorSuffix)
    };
  }
irs::string_ref const maxStrValue = getStringRef(max);

  if (filter) {
    auto& opts = filter->mutable_options()->push_back<irs::by_range_options>(firstOffset);
    irs::assign(opts.range.min, irs::ref_cast<irs::byte_type>(minStrValue));
    opts.range.min_type = minInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
    irs::assign(opts.range.max, irs::ref_cast<irs::byte_type>(maxStrValue));
    opts.range.max_type = maxInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
    opts.scored_terms_limit = FilterConstants::DefaultScoringTermsLimit;
  }
  return {};
}

std::map<irs::string_ref, ConversionPhraseHandler> const FCallSystemConversionPhraseHandlers {
  {"TERM", fromFuncPhraseTerm},
  {"STARTS_WITH", fromFuncPhraseStartsWith},
  {"WILDCARD", fromFuncPhraseLike}, // 'LIKE' is a key word
  {"LEVENSHTEIN_MATCH", fromFuncPhraseLevenshteinMatch},
  {TERMS_FUNC, fromFuncPhraseTerms<VPackSlice>},
  {"IN_RANGE", fromFuncPhraseInRange}
};
Result processPhraseArgObjectType(char const* funcName,
                                  size_t const funcArgumentPosition,
                                  irs::by_phrase* filter,
                                  QueryContext const& ctx,
                                  VPackSlice object,
                                  size_t firstOffset,
                                  irs::analysis::analyzer* analyzer = nullptr) {
  TRI_ASSERT(object.isObject());
  VPackObjectIterator itr(object);
  if (ADB_LIKELY(itr.valid())) {
    auto key = itr.key();
    auto value = itr.value();
    if (!key.isString()) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "'"s.append(funcName).append("' AQL function: Unexpected object key type '"
        "' at position '").append(std::to_string(funcArgumentPosition + 1)).append("'")
      };
    }
    auto name = key.copyString();
    basics::StringUtils::toupperInPlace(name);
    auto const entry = FCallSystemConversionPhraseHandlers.find(name);
    if (FCallSystemConversionPhraseHandlers.cend() == entry) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        "'"s.append(funcName).append("' AQL function: Unknown '")
        .append(key.copyString()).append("' at position '")
        .append(std::to_string(funcArgumentPosition + 1)).append("'")
      };
    }
    return entry->second(funcName, funcArgumentPosition, entry->first.c_str(), filter, ctx, value, firstOffset, analyzer);
  } else {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "'"s.append(funcName).append("' AQL function: empty object at position '")
      .append(std::to_string(funcArgumentPosition + 1)).append("'")
    };
  }
}

template<typename ElementType, typename ElementTraits = ArgsTraits<ElementType>>
Result processPhraseArgs(char const* funcName,
                         irs::by_phrase* phrase,
                         QueryContext const& ctx,
                         FilterContext const& filterCtx,
                         ElementType const& valueArgs,
                         size_t valueArgsBegin, size_t valueArgsEnd,
                         irs::analysis::analyzer* analyzer,
                         size_t offset,
                         bool allowDefaultOffset,
                         bool isInArray) {
  irs::string_ref value;
  bool expectingOffset = false;
  for (size_t idx = valueArgsBegin; idx < valueArgsEnd; ++idx) {
    typename ElementTraits::ValueType valueArg;
    {
      bool skippedEvaluation{ false };
      auto res = ElementTraits::getMemberValue(valueArgs, idx, funcName, valueArg,
                                               phrase != nullptr, ctx, skippedEvaluation);
      if (res.fail())
        return res;
      if (skippedEvaluation) {
        // non-const argument. we can`t decide on parse/optimize
        // if it is ok. So just say it is ok for now and deal with it
        // at execution
        return {};
      }
    }
    if (valueArg.isArray()) {
      // '[' <term0> [, <term1>, ...] ']'
      auto const valueSize = ElementTraits::numValueMembers(valueArg);
      if (!expectingOffset || allowDefaultOffset) {
        if (0 == valueSize) {
          expectingOffset = true;
          // do not reset offset here as we should accumulate it
          continue; // just skip empty arrays. This is not error anymore as this case may arise while working with autocomplete
        }
        // array arg is processed with possible default 0 offsets - to be easily compatible with TOKENS function
        if (!isInArray) {
          auto subRes = processPhraseArgs(funcName, phrase, ctx, filterCtx, ElementTraits::valueSlice(valueArg), 0,
                                          valueSize, analyzer, offset, true, true);
          if (subRes.fail()) {
            return subRes;
          }
          expectingOffset = true;
          offset = 0;
          continue;
        } else {
          auto res = fromFuncPhraseTerms(funcName, idx, TERMS_FUNC, phrase, ctx, ElementTraits::valueSlice(valueArg),
                                         offset, analyzer);
          if (res.fail()) {
            return res;
          }
          expectingOffset = true;
          offset = 0;
          continue;
        }
      }
    } else if (valueArg.isObject()) {
      auto res = processPhraseArgObjectType(funcName, idx, phrase, ctx, ElementTraits::valueSlice(valueArg), offset);
      if (res.fail()) {
        return res;
      }
      offset = 0;
      expectingOffset = true;
      continue;
    }
    if (ElementTraits::isValueNumber(valueArg) && expectingOffset) {
      offset += static_cast<uint64_t>(ElementTraits::getValueInt64(valueArg));
      expectingOffset = false;
      continue; // got offset let`s go search for value
    } else if ( (!valueArg.isString() || !ElementTraits::getValueString(valueArg, value)) || // value is not a string at all
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
Result fromFuncPhrase(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& args) {
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

    kludge::mangleField(name, analyzerPool);

    phrase = &filter->add<irs::by_phrase>();
    *phrase->mutable_field() = std::move(name);
    phrase->boost(filterCtx.boost);
  }
  // on top level we require explicit offsets - to be backward compatible and be able to distinguish last argument as analyzer or value
  // Also we allow recursion inside array to support older syntax (one array arg) and add ability to pass several arrays as args
  return processPhraseArgs(funcName, phrase, ctx, filterCtx, *valueArgs, valueArgsBegin, valueArgsEnd, analyzer.get(), 0, false, false);
}

// NGRAM_MATCH (attribute, target, threshold [, analyzer])
// NGRAM_MATCH (attribute, target [, analyzer]) // default threshold is set to 0.7
Result fromFuncNgramMatch(
    char const* funcName,
    irs::boolean_filter* filter, QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& args) {

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


    kludge::mangleField(name, analyzerPool);

    auto& ngramFilter = filter->add<irs::by_ngram_similarity>();
    *ngramFilter.mutable_field() = std::move(name);
    auto* opts = ngramFilter.mutable_options();
    opts->threshold = static_cast<float_t>(threshold);
    ngramFilter.boost(filterCtx.boost);

    analyzer->reset(matchValue);
    irs::term_attribute const* token = irs::get<irs::term_attribute>(*analyzer);
    TRI_ASSERT(token);
    while (analyzer->next()) {
      opts->ngrams.push_back(token->value);
    }
  }
  return {};
}

// STARTS_WITH(<attribute>, [ '[' ] <prefix> [, <prefix>, ... ']' ], [ <scoring-limit>|<min-match-count> ] [, <scoring-limit> ])
Result fromFuncStartsWith(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  if (!args.isDeterministic()) {
    return error::nondeterministicArgs(funcName);
  }

  auto const argc = args.numMembers();

  if (argc < 2 || argc > 4) {
    return error::invalidArgsCount<error::Range<2, 4>>(funcName);
  }

  size_t currentArgNum = 0;

  // 1st argument defines a field
  auto const* field =
      arangodb::iresearch::checkAttributeAccess(args.getMemberUnchecked(currentArgNum), *ctx.ref);

  if (!field) {
    return error::invalidAttribute(funcName, currentArgNum + 1);
  }
  ++currentArgNum;

  // 2nd argument defines a value or array of values
  auto const* prefixesNode = args.getMemberUnchecked(currentArgNum);

  if (!prefixesNode) {
    return error::invalidAttribute(funcName, currentArgNum + 1);
  }

  ScopedAqlValue prefixesValue(*prefixesNode);

  if (!filter && !prefixesValue.isConstant()) {
    return {};
  }

  if (!prefixesValue.execute(ctx)) {
    return error::failedToEvaluate(funcName, currentArgNum + 1);
  }

  std::vector<std::pair<ScopedAqlValue, irs::string_ref>> prefixes;
  ScopedAqlValue minMatchCountValue;
  auto minMatchCount = FilterConstants::DefaultStartsWithMinMatchCount;
  bool const isMultiPrefix = prefixesValue.isArray();
  if (isMultiPrefix) {
    auto const size = prefixesValue.size();
    if (size > 0) {
      prefixes.reserve(size);
      for (size_t i = 0; i < size; ++i) {
        prefixes.emplace_back(prefixesValue.at(i), irs::string_ref::NIL);
        auto& value = prefixes.back();

        if (!value.first.getString(value.second)) {
          return error::invalidArgument(funcName, currentArgNum + 1);
        }
      }
    }
    ++currentArgNum;

    if (argc > currentArgNum) {
      // 3rd argument defines minimum match count
      auto rv = evaluateArg<decltype(minMatchCount), true>(
            minMatchCount, minMatchCountValue, funcName, args, currentArgNum, filter != nullptr, ctx);

      if (rv.fail()) {
        return rv;
      }

      if (minMatchCount < 0) {
        return error::negativeNumber(funcName, currentArgNum + 1);
      }
    }
  } else if (prefixesValue.isString()) {
    if (argc > 3) {
      return error::invalidArgsCount<error::Range<2, 3>>(funcName);
    }

    prefixes.emplace_back();
    auto& value = prefixes.back();

    if (!prefixesValue.getString(value.second)) {
      return error::invalidArgument(funcName, currentArgNum + 1);
    }
  } else {
    return error::invalidArgument(funcName, currentArgNum + 1);
  }
  ++currentArgNum;

  auto scoringLimit = FilterConstants::DefaultScoringTermsLimit;

  if (argc > currentArgNum) {
    // 3rd or 4th (optional) argument defines a number of scored terms
    ScopedAqlValue scoringLimitValueBuf;
    auto scoringLimitValue = static_cast<int64_t>(scoringLimit);
    auto rv = evaluateArg(scoringLimitValue, scoringLimitValueBuf, funcName, args, currentArgNum, filter != nullptr, ctx);

    if (rv.fail()) {
      return rv;
    }

    if (scoringLimitValue < 0) {
      return error::negativeNumber(funcName, currentArgNum + 1);
    }

    scoringLimit = static_cast<size_t>(scoringLimitValue);
  }

  if (filter) {
    std::string name;

    if (!nameFromAttributeAccess(name, *field, ctx)) {
      return error::failedToGenerateName(funcName, 1);
    }

    TRI_ASSERT(filterCtx.analyzer);
    kludge::mangleField(name, filterCtx.analyzer);
    filter->boost(filterCtx.boost);

    if (isMultiPrefix) {
      auto& minMatchFilter = filter->add<irs::Or>();
      minMatchFilter.min_match_count(static_cast<size_t>(minMatchCount));
      // become a new root
      filter = &minMatchFilter;
    }

    for (size_t i = 0, size = prefixes.size(); i < size; ++i) {
      auto& prefixFilter = filter->add<irs::by_prefix>();
      if (i + 1 < size) {
        *prefixFilter.mutable_field() = name;
      } else {
        *prefixFilter.mutable_field() = std::move(name);
      }
      auto* opts = prefixFilter.mutable_options();
      opts->scored_terms_limit = scoringLimit;
      irs::assign(opts->term, irs::ref_cast<irs::byte_type>(prefixes[i].second));
    }
  }

  return {};
}

// IN_RANGE(<attribute>, <low>, <high>, <include-low>, <include-high>)
Result fromFuncInRange(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& args) {
  TRI_ASSERT(funcName);
  aql::AstNode const* field = nullptr;
  ScopedAqlValue min, max;
  auto minInclude = false;
  auto maxInclude = false;
  auto ret = false;
  auto res = getInRangeArguments<0>(funcName, filter != nullptr, ctx, args, &field,
                                    min, minInclude, max, maxInclude, ret);
  if (res.fail() || ret) {
    return res;
  }

  TRI_ASSERT(field);

  res = ::byRange(filter, *field, min, minInclude, max, maxInclude, ctx, filterCtx);
  if (res.fail()) {
    return {
      res.errorNumber(),
      "error in byRange: " + res.errorMessage()
    };
  }
  return {};
}

// LIKE(<attribute>, <pattern>)
Result fromFuncLike(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& args) {
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
  Result res = evaluateArg(pattern, patternValue, funcName, args, 1, filter != nullptr, ctx);

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
    kludge::mangleField(name, filterCtx.analyzer);

    auto& wildcardFilter = filter->add<irs::by_wildcard>();
    *wildcardFilter.mutable_field() = std::move(name);
    wildcardFilter.boost(filterCtx.boost);
    auto* opts = wildcardFilter.mutable_options();
    opts->scored_terms_limit = scoringLimit;
    irs::assign(opts->term, irs::ref_cast<irs::byte_type>(pattern));
  }

  return {};
}

// LEVENSHTEIN_MATCH(<attribute>, <target>, <max-distance> [, <include-transpositions>, <max-terms>])
Result fromFuncLevenshteinMatch(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  aql::AstNode const* field = nullptr;
  ScopedAqlValue targetValue;
  irs::by_edit_distance_options opts;
  auto res = getLevenshteinArguments<0>(funcName, filter != nullptr, ctx, args, &field,
                                        targetValue, opts);
  if (res.fail()) {
    return res;
  }

  if (filter) {
    std::string name;

    if (!nameFromAttributeAccess(name, *field, ctx)) {
      return error::failedToGenerateName(funcName, 1);
    }

    TRI_ASSERT(filterCtx.analyzer);
    kludge::mangleField(name, filterCtx.analyzer);

    auto& levenshtein_filter = filter->add<irs::by_edit_distance>();
    levenshtein_filter.boost(filterCtx.boost);
    *levenshtein_filter.mutable_field() = std::move(name);
    *levenshtein_filter.mutable_options() = std::move(opts);
  }

  return {};
}

Result fromFuncGeoContainsIntersect(
    char const* funcName,
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  if (!args.isDeterministic()) {
    return error::nondeterministicArgs(funcName);
  }

  auto const argc = args.numMembers();

  if (argc != 2) {
    return error::invalidArgsCount<error::ExactValue<2>>(funcName);
  }

  auto const* fieldNode = args.getMemberUnchecked(0);
  auto const* shapeNode = args.getMemberUnchecked(1);
  size_t fieldNodeIdx = 1;
  size_t shapeNodeIdx = 2;

  if (!arangodb::iresearch::checkAttributeAccess(fieldNode, *ctx.ref)) {
    if (!arangodb::iresearch::checkAttributeAccess(shapeNode, *ctx.ref)) {
      return {
        TRI_ERROR_BAD_PARAMETER,
         "'"s.append(funcName)
             .append("' AQL function: Unable to find argument denoting an attribute identifier")
      };
    }

    std::swap(fieldNode, shapeNode);
    fieldNodeIdx = 2;
    shapeNodeIdx = 1;
  }

  if (!fieldNode) {
    return error::invalidAttribute(funcName, fieldNodeIdx);
  }

  if (!shapeNode) {
    return error::invalidAttribute(funcName, shapeNodeIdx);
  }

  ScopedAqlValue shapeValue(*shapeNode);
  geo::ShapeContainer shape;

  if (filter || shapeValue.isConstant()) {
    if (!shapeValue.execute(ctx)) {
      return error::failedToEvaluate(funcName, shapeNodeIdx);
    }

    Result res;
    if (shapeValue.isObject()) {
      res = geo::geojson::parseRegion(shapeValue.slice(), shape);
    } else if (shapeValue.isArray()) {
      auto const slice = shapeValue.slice();

      if (slice.isArray() && slice.length() >= 2) {
        res = shape.parseCoordinates(slice, /*geoJson*/ true);
      }
    } else {
      return {
          TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName).append("' AQL function: argument at position '").append(std::to_string(shapeNodeIdx))
         .append("' has invalid type '").append(ScopedAqlValue::typeString(shapeValue.type()).c_str())
         .append("' ('").append(ScopedAqlValue::typeString(arangodb::iresearch::SCOPED_VALUE_TYPE_OBJECT).c_str())
         .append("' or '").append(ScopedAqlValue::typeString(arangodb::iresearch::SCOPED_VALUE_TYPE_ARRAY).c_str())
         .append("' expected)")
      };
    }

    if (res.fail()) {
      return {
          TRI_ERROR_BAD_PARAMETER,
          "'"s.append(funcName).append("' AQL function: failed to parse argument at position '")
              .append(std::to_string(shapeNodeIdx)).append("' due to the following error '")
              .append(res.errorMessage()).append("'")
      };
    }
  }

  if (filter) {
    std::string name;

    if (!nameFromAttributeAccess(name, *fieldNode, ctx)) {
      return error::failedToGenerateName(funcName, fieldNodeIdx);
    }

    auto& geo_filter = filter->add<GeoFilter>();
    geo_filter.boost(filterCtx.boost);

    auto* options = geo_filter.mutable_options();
    setupGeoFilter(filterCtx.analyzer, options->options);

    options->type = GEO_INTERSECT_FUNC == funcName
      ? GeoFilterType::INTERSECTS
      : (1 == shapeNodeIdx ? GeoFilterType::CONTAINS
                           : GeoFilterType::IS_CONTAINED);
    options->shape = std::move(shape);

    TRI_ASSERT(filterCtx.analyzer);
    kludge::mangleField(name, filterCtx.analyzer);
    *geo_filter.mutable_field() = std::move(name);
  }

  return {};
}

std::map<irs::string_ref, ConvertionHandler> const FCallUserConvertionHandlers;

Result fromFCallUser(irs::boolean_filter* filter, QueryContext const& ctx,
                     FilterContext const& filterCtx, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_FCALL_USER == node.type);

  if (node.numMembers() != 1) {
    return error::malformedNode(node.type);
  }

  auto const* args = arangodb::iresearch::getNode(node, 0, aql::NODE_TYPE_ARRAY);

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

std::map<irs::string_ref, ConvertionHandler> const FCallSystemConvertionHandlers{
  // filter functions
  {"PHRASE", fromFuncPhrase},
  {"STARTS_WITH", fromFuncStartsWith},
  {"EXISTS", fromFuncExists},
  {"MIN_MATCH", fromFuncMinMatch},
  {"IN_RANGE", fromFuncInRange},
  {"LIKE", fromFuncLike },
  {"LEVENSHTEIN_MATCH", fromFuncLevenshteinMatch},
  {"NGRAM_MATCH", fromFuncNgramMatch},
  // geo function
  {GEO_INTERSECT_FUNC, fromFuncGeoContainsIntersect},
  {"GEO_IN_RANGE", fromFuncGeoInRange},
  {"GEO_CONTAINS", fromFuncGeoContainsIntersect},
  // context functions
  {"BOOST", fromFuncBoost},
  {"ANALYZER", fromFuncAnalyzer},
};

Result fromFCall(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    FilterContext const& filterCtx,
    aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_FCALL == node.type);

  auto const* fn = static_cast<aql::Function*>(node.getData());

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

  auto const* args = arangodb::iresearch::getNode(node, 0, aql::NODE_TYPE_ARRAY);

  if (!args) {
    return {
      TRI_ERROR_BAD_PARAMETER,
      "Unable to parse arguments of system function '"s.append(fn->name).append("' as an array'")
    };
  }

  return entry->second(entry->first.c_str(), filter, ctx, filterCtx, *args);
}

Result fromFilter(irs::boolean_filter* filter, QueryContext const& ctx,
                  FilterContext const& filterCtx, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_FILTER == node.type);

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

Result filter(irs::boolean_filter* filter, QueryContext const& queryCtx,
              FilterContext const& filterCtx, aql::AstNode const& node) {
  switch (node.type) {
    case aql::NODE_TYPE_FILTER:  // FILTER
      return fromFilter(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_VARIABLE:  // variable
      return fromExpression(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_UNARY_NOT:  // unary minus
      return fromNegation(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_AND:  // logical and
      return fromGroup<irs::And>(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_OR:  // logical or
      return fromGroup<irs::Or>(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_EQ:  // compare ==
    case aql::NODE_TYPE_OPERATOR_BINARY_NE:  // compare !=
      return fromBinaryEq(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_LT:  // compare <
    case aql::NODE_TYPE_OPERATOR_BINARY_LE:  // compare <=
    case aql::NODE_TYPE_OPERATOR_BINARY_GT:  // compare >
    case aql::NODE_TYPE_OPERATOR_BINARY_GE:  // compare >=
      return fromInterval(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_IN:   // compare in
    case aql::NODE_TYPE_OPERATOR_BINARY_NIN:  // compare not in
      return fromIn(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_TERNARY:  // ternary
    case aql::NODE_TYPE_ATTRIBUTE_ACCESS:  // attribute access
    case aql::NODE_TYPE_VALUE:             // value
    case aql::NODE_TYPE_ARRAY:             // array
    case aql::NODE_TYPE_OBJECT:            // object
    case aql::NODE_TYPE_REFERENCE:         // reference
    case aql::NODE_TYPE_PARAMETER:         // bind parameter
      return fromExpression(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_FCALL:  // function call
      return fromFCall(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_FCALL_USER:  // user function call
      return fromFCallUser(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_RANGE:  // range
      return fromRange(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_NARY_AND:  // n-ary and
      return fromGroup<irs::And>(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_NARY_OR:  // n-ary or
      return fromGroup<irs::Or>(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:   // compare ARRAY in
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:  // compare ARRAY not in
    // for iresearch filters IN and EQ queries will be actually the same
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:  // compare ARRAY ==
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:  // compare ARRAY !=
      return fromArrayComparison<ByTermSubFilterFactory>(filter, queryCtx, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:  // compare ARRAY <
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:  // compare ARRAY <=
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:  // compare ARRAY >
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:  // compare ARRAY >=
      return fromArrayComparison<ByRangeSubFilterFactory>(filter, queryCtx, filterCtx, node);
    default:
      return fromExpression(filter, queryCtx, filterCtx, node);
  }
}

} // namespace

namespace arangodb {
namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                      FilerFactory implementation
// ----------------------------------------------------------------------------

/*static*/ Result FilterFactory::filter(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    aql::AstNode const& node) {
  if (node.willUseV8()) {
    return {
      TRI_ERROR_NOT_IMPLEMENTED,
      "using V8 dependent function is not allowed in SEARCH statement"
    };
  }

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
