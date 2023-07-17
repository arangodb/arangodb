////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/DownCast.h"

#include <frozen/map.h>

// otherwise define conflict between 3rdParty\date\include\date\date.h and
// 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
#include "date/date.h"
#endif

#include "Aql/Functions.h"
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
#include "IResearch/AqlHelper.h"
#include "IResearch/GeoAnalyzer.h"
#include "IResearch/GeoFilter.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchFilterFactoryCommon.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterOptimization.h"
#include "IResearch/IResearchIdentityAnalyzer.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/IResearchPDP.h"
#include "Logger/LogMacros.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"

#include <absl/strings/str_cat.h>

using namespace std::literals::string_literals;

namespace arangodb::iresearch {

Result makeFilter(irs::boolean_filter* filter, FilterContext const& filterCtx,
                  aql::AstNode const& node);

Result fromExpression(irs::boolean_filter* filter,
                      FilterContext const& filterCtx, aql::AstNode const& node);

#ifdef USE_ENTERPRISE
Result fromBooleanExpansion(irs::boolean_filter* filter,
                            FilterContext const& filterCtx,
                            aql::AstNode const& node);
#endif

Result fromFuncMinHashMatch(char const* funcName, irs::boolean_filter* filter,
                            FilterContext const& filterCtx,
                            aql::AstNode const& args);

irs::ColumnAcceptor makeColumnAcceptor(bool) noexcept;

#ifndef USE_ENTERPRISE
irs::filter::ptr makeAll(std::string_view) {
  return std::make_unique<irs::all>();
}
std::string_view makeAllColumn(QueryContext const&) noexcept { return {}; }

#endif

irs::AllDocsProvider::ProviderFunc makeAllProvider(QueryContext const& ctx) {
  auto const allColumn = makeAllColumn(ctx);

  if (allColumn.empty()) {
    return {};
  }

  return [field = std::string{allColumn}](irs::score_t boost) {
    auto filter = makeAll(field);
    filter->boost(boost);
    return filter;
  };
}

}  // namespace arangodb::iresearch

namespace {

using namespace arangodb;
using namespace arangodb::iresearch;

constexpr char const* GEO_INTERSECT_FUNC = "GEO_INTERSECTS";
constexpr char const* GEO_DISTANCE_FUNC = "GEO_DISTANCE";
constexpr char const* TERMS_FUNC = "TERMS";

void setupAllTypedFilter(irs::Or& disjunction, FilterContext const& ctx,
                         std::string&& mangledName) {
  auto& allDocs = append<irs::by_column_existence>(disjunction, ctx);
  *allDocs.mutable_field() = std::move(mangledName);
  allDocs.boost(ctx.boost);
}

Result setupGeoFilter(FieldMeta::Analyzer const& a,
                      GeoFilterOptionsBase& options) {
  if (!a._pool) {
    return {TRI_ERROR_INTERNAL, "Malformed analyzer pool."};
  }
  auto& pool = *a._pool;
  if (!kludge::isGeoAnalyzer(pool.type())) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Analyzer '", std::string_view{pool.type()},
                         "' is not a geo analyzer.")};
  }
  auto stream = pool.get();
  if (!stream) {
    return {TRI_ERROR_INTERNAL, "Malformed geo analyzer stream."};
  }
  auto const& impl = basics::downCast<GeoAnalyzer>(*stream);
  impl.prepare(options);
  return {};
}

Result getLatLng(ScopedAqlValue const& value, S2Point& point,
                 char const* funcName, size_t argIdx,
                 GeoFilterOptionsBase const& options) {
  auto to_point = [&](S2LatLng& latLng) noexcept {
    if (options.coding == geo::coding::Options::kS2LatLngInt) {
      geo::toLatLngInt(latLng);
    }
    point = latLng.ToPoint();
  };
  switch (value.type()) {
    case SCOPED_VALUE_TYPE_ARRAY: {  // [lng, lat] is valid input
      if (value.size() != 2) {
        return error::failedToEvaluate(funcName, argIdx);
      }

      auto const latValue = value.at(1);
      auto const lonValue = value.at(0);

      if (!latValue.isDouble() || !lonValue.isDouble()) {
        return error::failedToEvaluate(funcName, argIdx);
      }

      double lat, lon;

      if (!latValue.getDouble(lat) || !lonValue.getDouble(lon)) {
        return error::failedToEvaluate(funcName, argIdx);
      }

      auto latLng = S2LatLng::FromDegrees(lat, lon).Normalized();
      to_point(latLng);
    } break;
    case SCOPED_VALUE_TYPE_OBJECT: {
      auto const json = value.slice();
      TRI_ASSERT(json.isObject());
      const bool withoutSerialization =
          options.stored == StoredType::S2Centroid &&
          geo::json::type(json) != geo::json::Type::POINT &&
          !geo::coding::isOptionsS2(options.coding);
      geo::ShapeContainer region;
      std::vector<S2LatLng> cache;
      auto r = geo::json::parseRegion<true>(
          json, region, cache, options.stored == StoredType::VPackLegacy,
          withoutSerialization ? geo::coding::Options::kInvalid
                               : options.coding,
          nullptr);
      if (!r.ok()) {
        return error::failedToEvaluate(funcName, argIdx);
      }
      point = region.centroid();
      if (withoutSerialization) {
        S2LatLng latLng{point};
        to_point(latLng);
      }
    } break;
    default:
      return error::invalidArgument(funcName, argIdx);
  }
  return {};
}

using ConversionHandler = Result (*)(char const* funcName, irs::boolean_filter*,
                                     FilterContext const&, aql::AstNode const&);

/// Appends value tokens to a phrase filter
void appendTerms(irs::by_phrase& filter, std::string_view value,
                 irs::analysis::analyzer& stream, size_t firstOffset) {
  // reset stream
  stream.reset(value);

  // get token attribute
  TRI_ASSERT(irs::get<irs::term_attribute>(stream));
  irs::term_attribute const* token = irs::get<irs::term_attribute>(stream);
  TRI_ASSERT(token);

  // add tokens
  for (auto* options = filter.mutable_options(); stream.next();) {
    options->push_back<irs::by_term_options>(firstOffset)
        .term.assign(token->value);

    firstOffset = 0;
  }
}

Result byTerm(irs::by_term* filter, std::string&& name,
              ScopedAqlValue const& value, FilterContext const& filterCtx) {
  switch (value.type()) {
    case SCOPED_VALUE_TYPE_NULL:
      if (filter) {
        kludge::mangleNull(name);
        *filter->mutable_field() = std::move(name);
        filter->boost(filterCtx.boost);
        filter->mutable_options()->term.assign(irs::ViewCast<irs::byte_type>(
            irs::null_token_stream::value_null()));
      }
      return {};
    case SCOPED_VALUE_TYPE_BOOL:
      if (filter) {
        kludge::mangleBool(name);
        *filter->mutable_field() = std::move(name);
        filter->boost(filterCtx.boost);
        filter->mutable_options()->term.assign(irs::ViewCast<irs::byte_type>(
            irs::boolean_token_stream::value(value.getBoolean())));
      }
      return {};
    case SCOPED_VALUE_TYPE_DOUBLE:
      if (filter) {
        double dblValue;

        if (!value.getDouble(dblValue)) {
          // something went wrong
          return {TRI_ERROR_BAD_PARAMETER, "could not get double value"};
        }

        kludge::mangleNumeric(name);

        irs::numeric_token_stream stream;
        irs::term_attribute const* token =
            irs::get<irs::term_attribute>(stream);
        TRI_ASSERT(token);
        stream.reset(dblValue);
        stream.next();

        *filter->mutable_field() = std::move(name);
        filter->boost(filterCtx.boost);
        filter->mutable_options()->term.assign(token->value);
      }
      return {};
    case SCOPED_VALUE_TYPE_STRING:
      if (filter) {
        std::string_view strValue;
        if (!value.getString(strValue)) {
          // something went wrong
          return {TRI_ERROR_BAD_PARAMETER, "could not get string value"};
        }

        auto const& ctx = filterCtx.query;

        auto& analyzer = filterCtx.fieldAnalyzer(name, ctx.ctx);
        if (!analyzer) {
          return {TRI_ERROR_INTERNAL, "Malformed search/filter context"};
        }

        kludge::mangleField(name, ctx.isOldMangling, analyzer);
        *filter->mutable_field() = std::move(name);
        filter->boost(filterCtx.boost);
        filter->mutable_options()->term.assign(
            irs::ViewCast<irs::byte_type>(strValue));
      }
      return {};
    default:
      // unsupported value type
      return {TRI_ERROR_BAD_PARAMETER, "unsupported type"};
  }
}

Result byTerm(irs::by_term* filter, aql::AstNode const& attribute,
              ScopedAqlValue const& value, FilterContext const& filterCtx) {
  std::string name{filterCtx.query.namePrefix};

  if (!nameFromAttributeAccess(name, attribute, filterCtx.query,
                               filter != nullptr)) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Failed to generate field name from node ",
                         aql::AstNode::toString(&attribute))};
  }

  return byTerm(filter, std::move(name), value, filterCtx);
}

Result byTerm(irs::by_term* filter, NormalizedCmpNode const& node,
              FilterContext const& filterCtx) {
  TRI_ASSERT(node.attribute && node.attribute->isDeterministic());
  TRI_ASSERT(node.value && node.value->isDeterministic());

  ScopedAqlValue value(*node.value);

  if (!value.isConstant()) {
    if (!filter) {
      // can't evaluate non constant filter before the execution
      if (!filterCtx.query.isSearchQuery) {
        // but at least we must validate attribute name
        std::string fieldName{filterCtx.query.namePrefix};
        if (!nameFromAttributeAccess(fieldName, *node.attribute,
                                     filterCtx.query, false)) {
          return error::failedToGenerateName("byTerm", 1);
        }
      }
      return {};
    }

    if (!value.execute(filterCtx.query)) {
      // failed to execute expression
      return {TRI_ERROR_BAD_PARAMETER, "could not execute expression"};
    }
  }

  return byTerm(filter, *node.attribute, value, filterCtx);
}

Result byRange(irs::boolean_filter* filter, aql::AstNode const& attribute,
               aql::Range const& rangeData, FilterContext const& filterCtx) {
  TRI_ASSERT(attribute.isDeterministic());

  auto const& ctx = filterCtx.query;

  std::string name{ctx.namePrefix};
  if (!nameFromAttributeAccess(name, attribute, ctx, filter != nullptr)) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Failed to generate field name from node ",
                         aql::AstNode::toString(&attribute))};
  }

  TRI_ASSERT(filter);
  auto& range = append<irs::by_granular_range>(*filter, filterCtx);

  kludge::mangleNumeric(name);
  *range.mutable_field() = std::move(name);
  range.boost(filterCtx.boost);

  irs::numeric_token_stream stream;

  // setup min bound
  stream.reset(static_cast<double>(rangeData._low));

  auto* opts = range.mutable_options();
  irs::set_granular_term(opts->range.min, stream);
  opts->range.min_type = irs::BoundType::INCLUSIVE;

  // setup max bound
  stream.reset(static_cast<double>(rangeData._high));
  irs::set_granular_term(opts->range.max, stream);
  opts->range.max_type = irs::BoundType::INCLUSIVE;

  return {};
}

Result byRange(irs::boolean_filter* filter, aql::AstNode const& attributeNode,
               ScopedAqlValue const& min, bool const minInclude,
               ScopedAqlValue const& max, bool const maxInclude,
               FilterContext const& filterCtx) {
  auto const& ctx = filterCtx.query;

  std::string name{ctx.namePrefix};
  if (!nameFromAttributeAccess(name, attributeNode, ctx, filter != nullptr)) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Failed to generate field name from node ",
                         aql::AstNode::toString(&attributeNode))};
  }

  switch (min.type()) {
    case SCOPED_VALUE_TYPE_NULL: {
      if (filter) {
        kludge::mangleNull(name);

        auto& range = append<irs::by_range>(*filter, filterCtx);
        *range.mutable_field() = std::move(name);
        range.boost(filterCtx.boost);
        auto* opts = range.mutable_options();
        opts->range.min.assign(irs::ViewCast<irs::byte_type>(
            irs::null_token_stream::value_null()));
        opts->range.min_type =
            minInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
        opts->range.max.assign(irs::ViewCast<irs::byte_type>(
            irs::null_token_stream::value_null()));
        opts->range.max_type =
            maxInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
      }

      return {};
    }
    case SCOPED_VALUE_TYPE_BOOL: {
      if (filter) {
        kludge::mangleBool(name);

        auto& range = append<irs::by_range>(*filter, filterCtx);
        *range.mutable_field() = std::move(name);
        range.boost(filterCtx.boost);
        auto* opts = range.mutable_options();
        opts->range.min.assign(irs::ViewCast<irs::byte_type>(
            irs::boolean_token_stream::value(min.getBoolean())));
        opts->range.min_type =
            minInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
        opts->range.max.assign(irs::ViewCast<irs::byte_type>(
            irs::boolean_token_stream::value(max.getBoolean())));
        opts->range.max_type =
            maxInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
      }

      return {};
    }
    case SCOPED_VALUE_TYPE_DOUBLE: {
      if (filter) {
        double minDblValue, maxDblValue;

        if (!min.getDouble(minDblValue) || !max.getDouble(maxDblValue)) {
          // can't parse value as double
          return {TRI_ERROR_BAD_PARAMETER, "can not get double parameter"};
        }

        auto& range = append<irs::by_granular_range>(*filter, filterCtx);

        kludge::mangleNumeric(name);
        *range.mutable_field() = std::move(name);
        range.boost(filterCtx.boost);

        irs::numeric_token_stream stream;
        auto* opts = range.mutable_options();

        // setup min bound
        stream.reset(minDblValue);
        irs::set_granular_term(opts->range.min, stream);
        opts->range.min_type =
            minInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;

        // setup max bound
        stream.reset(maxDblValue);
        irs::set_granular_term(opts->range.max, stream);
        opts->range.max_type =
            maxInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
      }

      return {};
    }
    case SCOPED_VALUE_TYPE_STRING: {
      if (filter) {
        std::string_view minStrValue, maxStrValue;
        if (!min.getString(minStrValue) || !max.getString(maxStrValue)) {
          // failed to get string value
          return {TRI_ERROR_BAD_PARAMETER, "failed to get string value"};
        }

        auto& analyzer = filterCtx.fieldAnalyzer(name, ctx.ctx);
        if (!analyzer) {
          return {TRI_ERROR_INTERNAL, "Malformed search/filter context"};
        }

        auto& range = append<irs::by_range>(*filter, filterCtx);
        kludge::mangleField(name, ctx.isOldMangling, analyzer);
        *range.mutable_field() = std::move(name);
        range.boost(filterCtx.boost);

        auto* opts = range.mutable_options();
        opts->range.min.assign(irs::ViewCast<irs::byte_type>(minStrValue));
        opts->range.min_type =
            minInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
        opts->range.max.assign(irs::ViewCast<irs::byte_type>(maxStrValue));
        opts->range.max_type =
            maxInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
      }

      return {};
    }
    default:
      // wrong value type
      return {TRI_ERROR_BAD_PARAMETER, "invalid value type"};
  }
}

using TypeRangeHandler = void (*)(irs::Or& rangeOr, FilterContext const& ctx,
                                  std::string name);

std::array<TypeRangeHandler, 4> constexpr kTypeRangeHandlers{
    {[](irs::Or& rangeOr, FilterContext const& ctx, std::string name) {
       kludge::mangleNull(name);
       setupAllTypedFilter(rangeOr, ctx, std::move(name));
     },
     [](irs::Or& rangeOr, FilterContext const& ctx, std::string name) {
       kludge::mangleBool(name);
       setupAllTypedFilter(rangeOr, ctx, std::move(name));
     },
     [](irs::Or& rangeOr, FilterContext const& ctx, std::string name) {
       kludge::mangleNumeric(name);
       setupAllTypedFilter(rangeOr, ctx, std::move(name));
     },
     [](irs::Or& rangeOr, FilterContext const& ctx, std::string name) {
       kludge::mangleString(name);
       setupAllTypedFilter(rangeOr, ctx, std::move(name));
     }}};

template<bool Min, size_t typeIdx>
void setupTypeOrderRangeFilter(irs::Or& rangeOr, FilterContext const& ctx,
                               std::string_view name) {
  static_assert(typeIdx < kTypeRangeHandlers.size());
  static_assert(typeIdx >= 0);
  if constexpr (Min) {
    for (size_t i = typeIdx + 1; i < kTypeRangeHandlers.size(); ++i) {
      kTypeRangeHandlers[i](rangeOr, ctx, std::string{name});
    }
  } else if constexpr (typeIdx > 0) {
    for (size_t i = 0; i < typeIdx; ++i) {
      kTypeRangeHandlers[i](rangeOr, ctx, std::string{name});
    }
  }
}

template<bool Min>
Result byRange(irs::boolean_filter* filter, std::string name,
               const ScopedAqlValue& value, bool const incl,
               FilterContext const& filterCtx) {
  // ArangoDB type order
  // null  <  bool  <  number  <  string  <  array/list  <  object/document
  switch (auto const& ctx = filterCtx.query; value.type()) {
    case SCOPED_VALUE_TYPE_NULL: {
      if (filter) {
        irs::by_range* range{nullptr};
        if (ctx.isSearchQuery || !Min) {
          range = &append<irs::by_range>(*filter, filterCtx);
        } else {
          auto& rangeOr = append<irs::Or>(*filter, filterCtx);
          range = &append<irs::by_range>(rangeOr, filterCtx);
          setupTypeOrderRangeFilter<Min, 0>(rangeOr, filterCtx, name);
        }
        kludge::mangleNull(name);
        *range->mutable_field() = std::move(name);
        range->boost(filterCtx.boost);
        auto* opts = range->mutable_options();
        (Min ? opts->range.min : opts->range.max)
            .assign(irs::ViewCast<irs::byte_type>(
                irs::null_token_stream::value_null()));
        (Min ? opts->range.min_type : opts->range.max_type) =
            incl ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
      }
      return {};
    }
    case SCOPED_VALUE_TYPE_BOOL: {
      if (filter) {
        irs::by_range* range{nullptr};
        if (ctx.isSearchQuery) {
          range = &append<irs::by_range>(*filter, filterCtx);
        } else {
          auto& rangeOr = append<irs::Or>(*filter, filterCtx);
          range = &append<irs::by_range>(rangeOr, filterCtx);
          setupTypeOrderRangeFilter<Min, 1>(rangeOr, filterCtx, name);
        }
        TRI_ASSERT(range);
        kludge::mangleBool(name);
        *range->mutable_field() = std::move(name);
        range->boost(filterCtx.boost);
        auto* opts = range->mutable_options();
        (Min ? opts->range.min : opts->range.max)
            .assign(irs::ViewCast<irs::byte_type>(
                irs::boolean_token_stream::value(value.getBoolean())));
        (Min ? opts->range.min_type : opts->range.max_type) =
            incl ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
      }

      return {};
    }
    case SCOPED_VALUE_TYPE_DOUBLE: {
      if (filter) {
        double dblValue;

        if (!value.getDouble(dblValue)) {
          // can't parse as double
          return {TRI_ERROR_BAD_PARAMETER, "could not parse double value"};
        }

        irs::by_granular_range* range{nullptr};
        if (ctx.isSearchQuery) {
          range = &append<irs::by_granular_range>(*filter, filterCtx);
        } else {
          auto& rangeOr = append<irs::Or>(*filter, filterCtx);
          range = &append<irs::by_granular_range>(rangeOr, filterCtx);
          setupTypeOrderRangeFilter<Min, 2>(rangeOr, filterCtx, name);
        }
        irs::numeric_token_stream stream;

        kludge::mangleNumeric(name);
        *range->mutable_field() = std::move(name);
        range->boost(filterCtx.boost);

        stream.reset(dblValue);
        auto* opts = range->mutable_options();
        irs::set_granular_term(Min ? opts->range.min : opts->range.max, stream);
        (Min ? opts->range.min_type : opts->range.max_type) =
            incl ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
      }
      return {};
    }
    case SCOPED_VALUE_TYPE_STRING: {
      if (filter) {
        std::string_view strValue;

        if (!value.getString(strValue)) {
          // can't parse as string
          return {TRI_ERROR_BAD_PARAMETER, "could not parse string value"};
        }

        auto& analyzer = filterCtx.fieldAnalyzer(name, ctx.ctx);
        if (!analyzer) {
          return {TRI_ERROR_INTERNAL, "Malformed search/filter context"};
        }

        irs::by_range* range{nullptr};
        if (ctx.isSearchQuery || Min) {
          range = &append<irs::by_range>(*filter, filterCtx);
        } else {
          auto& rangeOr = append<irs::Or>(*filter, filterCtx);
          range = &append<irs::by_range>(rangeOr, filterCtx);
          setupTypeOrderRangeFilter<Min, 3>(rangeOr, filterCtx, name);
        }
        kludge::mangleField(name, ctx.isOldMangling, analyzer);
        *range->mutable_field() = std::move(name);
        range->boost(filterCtx.boost);

        auto* opts = range->mutable_options();
        (Min ? opts->range.min : opts->range.max)
            .assign(irs::ViewCast<irs::byte_type>(strValue));
        (Min ? opts->range.min_type : opts->range.max_type) =
            incl ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
      }

      return {};
    }
    default:
      // wrong value type
      return {TRI_ERROR_BAD_PARAMETER, "invalid value type"};
  }
}

template<bool Min>
Result byRange(irs::boolean_filter* filter, NormalizedCmpNode const& node,
               bool const incl, FilterContext const& filterCtx) {
  TRI_ASSERT(node.attribute && node.attribute->isDeterministic());
  TRI_ASSERT(node.value && node.value->isDeterministic());

  auto const& ctx = filterCtx.query;

  std::string name{ctx.namePrefix};
  if (!nameFromAttributeAccess(name, *node.attribute, ctx, filter != nullptr)) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Failed to generate field name from node ",
                         aql::AstNode::toString(node.attribute))};
  }
  auto value = ScopedAqlValue(*node.value);
  if (!value.isConstant()) {
    if (!filter) {
      // can't evaluate non constant filter before the execution
      return {};
    }

    if (!value.execute(ctx)) {
      // could not execute expression
      return {TRI_ERROR_BAD_PARAMETER, "can not execute expression"};
    }
  }
  return byRange<Min>(filter, name, value, incl, filterCtx);
}

Result fromExpression(irs::boolean_filter* filter,
                      FilterContext const& filterCtx,
                      std::shared_ptr<aql::AstNode>&& node) {
  // redirect to existing function for AstNode const& nodes to
  // avoid coding the logic twice
  return fromExpression(filter, filterCtx, *node);
}

// GEO_IN_RANGE(attribute, shape, lower, upper[, includeLower = true,
// includeUpper = true])
Result fromFuncGeoInRange(char const* funcName, irs::boolean_filter* filter,
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

  auto const& ctx = filterCtx.query;
  auto const* fieldNode = args.getMemberUnchecked(0);
  auto const* centroidNode = args.getMemberUnchecked(1);
  size_t fieldNodeIdx = 1;
  size_t centroidNodeIdx = 2;

  if (!checkAttributeAccess(fieldNode, *ctx.ref, !ctx.isSearchQuery)) {
    if (!checkAttributeAccess(centroidNode, *ctx.ref, !ctx.isSearchQuery)) {
      return {
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("'", funcName,
                       "' AQL function: Unable to find argument denoting an "
                       "attribute identifier")};
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

  bool const buildFilter = filter != nullptr;

  ScopedAqlValue tmpValue;

  double minDistance = 0;

  auto rv =
      evaluateArg(minDistance, tmpValue, funcName, args, 2, buildFilter, ctx);

  if (rv.fail()) {
    return rv;
  }

  double maxDistance = 0;

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
      rv = evaluateArg(includeMax, tmpValue, funcName, args, 5, buildFilter,
                       ctx);

      if (rv.fail()) {
        return rv;
      }
    }
  }

  std::string name{ctx.namePrefix};
  if (!nameFromAttributeAccess(name, *fieldNode, ctx, filter != nullptr)) {
    return error::failedToGenerateName(funcName, fieldNodeIdx);
  }

  if (buildFilter) {
    tmpValue.reset(*centroidNode);
    if (!tmpValue.execute(ctx)) {
      return error::failedToEvaluate(funcName, centroidNodeIdx);
    }
    auto& analyzer = filterCtx.fieldAnalyzer(name, ctx.ctx);
    if (!analyzer) {
      return {TRI_ERROR_INTERNAL, "Malformed search/filter context"};
    }

    auto& geo_filter = append<GeoDistanceFilter>(*filter, filterCtx);
    geo_filter.boost(filterCtx.boost);

    auto* options = geo_filter.mutable_options();
    auto r = setupGeoFilter(analyzer, *options);
    if (!r.ok()) {
      return r;
    }

    S2Point centroid;
    r = getLatLng(tmpValue, centroid, funcName, centroidNodeIdx, *options);
    if (!r.ok()) {
      return r;
    }

    options->origin = centroid;
    if (minDistance != 0.) {
      options->range.min = minDistance;
      options->range.min_type =
          includeMin ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
    }
    options->range.max = maxDistance;
    options->range.max_type =
        includeMax ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;

    kludge::mangleField(name, ctx.isOldMangling, analyzer);
    *geo_filter.mutable_field() = std::move(name);
  }

  return {};
}

// GEO_DISTANCE(.. , ..) <|<=|==|>|>= Distance
Result fromGeoDistanceInterval(irs::boolean_filter* filter,
                               NormalizedCmpNode const& node,
                               FilterContext const& filterCtx) {
  TRI_ASSERT(
      node.attribute && node.attribute->isDeterministic() &&
      aql::NODE_TYPE_FCALL == node.attribute->type &&
      &aql::functions::GeoDistance ==
          reinterpret_cast<aql::Function const*>(node.attribute->getData())
              ->implementation);
  TRI_ASSERT(node.value && node.value->isDeterministic());

  auto* args = node.attribute->getMemberUnchecked(0);
  TRI_ASSERT(args);

  if (args->numMembers() != 2) {
    return {TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH};
  }

  auto const& ctx = filterCtx.query;
  auto* fieldNode = args->getMemberUnchecked(0);
  auto* centroidNode = args->getMemberUnchecked(1);
  size_t fieldNodeIdx = 1;
  size_t centroidNodeIdx = 2;

  if (!checkAttributeAccess(fieldNode, *ctx.ref, !ctx.isSearchQuery)) {
    if (!checkAttributeAccess(centroidNode, *ctx.ref, !ctx.isSearchQuery)) {
      return {TRI_ERROR_BAD_PARAMETER};
    }

    std::swap(fieldNode, centroidNode);
    centroidNodeIdx = 1;
    fieldNodeIdx = 2;
  }

  if (findReference(*centroidNode, *ctx.ref)) {
    // centroid contains referenced variable
    return {TRI_ERROR_BAD_PARAMETER};
  }

  double distance{};
  ScopedAqlValue tmpValue(*node.value);
  if (filter || tmpValue.isConstant()) {
    if (!tmpValue.execute(ctx)) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat(
                  "Failed to evaluate an argument denoting a distance near '",
                  GEO_DISTANCE_FUNC, "' function")};
    }

    if (SCOPED_VALUE_TYPE_DOUBLE != tmpValue.type() ||
        !tmpValue.getDouble(distance)) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("Failed to parse an argument denoting a distance as "
                           "a number near '",
                           GEO_DISTANCE_FUNC, "' function")};
    }
  }

  std::string name{ctx.namePrefix};
  if (!nameFromAttributeAccess(name, *fieldNode, ctx, filter != nullptr)) {
    return error::failedToGenerateName(GEO_DISTANCE_FUNC, fieldNodeIdx);
  }

  if (filter) {
    tmpValue.reset(*centroidNode);
    if (!tmpValue.execute(ctx)) {
      return error::failedToEvaluate(GEO_DISTANCE_FUNC, centroidNodeIdx);
    }
    auto& analyzer = filterCtx.fieldAnalyzer(name, ctx.ctx);
    if (!analyzer) {
      return {TRI_ERROR_INTERNAL, "Malformed search/filter context"};
    }
    GeoDistanceFilterOptions options;
    auto r = setupGeoFilter(analyzer, options);
    if (!r.ok()) {
      return r;
    }
    r = getLatLng(tmpValue, options.origin, GEO_DISTANCE_FUNC, centroidNodeIdx,
                  options);
    if (!r.ok()) {
      return r;
    }

    switch (node.cmp) {
      case aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      case aql::NODE_TYPE_OPERATOR_BINARY_NE:
        options.range.min = distance;
        options.range.min_type = irs::BoundType::INCLUSIVE;
        options.range.max = distance;
        options.range.max_type = irs::BoundType::INCLUSIVE;
        break;
      case aql::NODE_TYPE_OPERATOR_BINARY_LT:
      case aql::NODE_TYPE_OPERATOR_BINARY_LE:
        options.range.max = distance;
        options.range.max_type = aql::NODE_TYPE_OPERATOR_BINARY_LE == node.cmp
                                     ? irs::BoundType::INCLUSIVE
                                     : irs::BoundType::EXCLUSIVE;
        break;
      case aql::NODE_TYPE_OPERATOR_BINARY_GT:
      case aql::NODE_TYPE_OPERATOR_BINARY_GE:
        options.range.min = distance;
        options.range.min_type = aql::NODE_TYPE_OPERATOR_BINARY_GE == node.cmp
                                     ? irs::BoundType::INCLUSIVE
                                     : irs::BoundType::EXCLUSIVE;
        break;
      default:
        TRI_ASSERT(false);
        return {TRI_ERROR_BAD_PARAMETER};
    }

    auto& geo_filter = (aql::NODE_TYPE_OPERATOR_BINARY_NE == node.cmp
                            ? appendNot<GeoDistanceFilter>(*filter, filterCtx)
                            : append<GeoDistanceFilter>(*filter, filterCtx));
    geo_filter.boost(filterCtx.boost);
    *geo_filter.mutable_options() = std::move(options);

    kludge::mangleField(name, ctx.isOldMangling, analyzer);
    *geo_filter.mutable_field() = std::move(name);
  }

  return {};
}

Result fromInterval(irs::boolean_filter* filter, FilterContext const& filterCtx,
                    aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == node.type ||
             arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == node.type);

  auto const& ctx = filterCtx.query;
  NormalizedCmpNode normNode;

  if (!normalizeCmpNode(node, *ctx.ref, !ctx.isSearchQuery, normNode)) {
    if (normalizeGeoDistanceCmpNode(node, *ctx.ref, normNode)) {
      if (fromGeoDistanceInterval(filter, normNode, filterCtx).ok()) {
        return {};
      }
    }

    return fromExpression(filter, filterCtx, node);
  }

  bool const incl = aql::NODE_TYPE_OPERATOR_BINARY_GE == normNode.cmp ||
                    aql::NODE_TYPE_OPERATOR_BINARY_LE == normNode.cmp;

  bool const min = aql::NODE_TYPE_OPERATOR_BINARY_GT == normNode.cmp ||
                   aql::NODE_TYPE_OPERATOR_BINARY_GE == normNode.cmp;

  return min ? byRange<true>(filter, normNode, incl, filterCtx)
             : byRange<false>(filter, normNode, incl, filterCtx);
}

Result fromBinaryEq(irs::boolean_filter* filter, FilterContext const& filterCtx,
                    aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_EQ == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type);

  auto const& ctx = filterCtx.query;

  NormalizedCmpNode normalized;

  if (!normalizeCmpNode(node, *ctx.ref, !ctx.isSearchQuery, normalized)) {
    if (normalizeGeoDistanceCmpNode(node, *ctx.ref, normalized)) {
      if (fromGeoDistanceInterval(filter, normalized, filterCtx).ok()) {
        return {};
      }
    }

    auto rv = fromExpression(filter, filterCtx, node);
    return rv.withError([&](arangodb::result::Error& err) {
      err.resetErrorMessage(
          absl::StrCat("in from binary equation", rv.errorMessage()));
    });
  }

  irs::by_term* termFilter = nullptr;

  if (filter) {
    termFilter = &(aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type
                       ? appendNot<irs::by_term>(*filter, filterCtx)
                       : append<irs::by_term>(*filter, filterCtx));
  }

  return byTerm(termFilter, normalized, filterCtx);
}

Result fromRange(irs::boolean_filter* filter, FilterContext const& filterCtx,
                 aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_RANGE == node.type);

  if (node.numMembers() != 2) {
    auto rv = error::malformedNode(node.type);
    return rv.reset(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("wrong number of arguments in range expression: ",
                     rv.errorMessage()));
  }

  // ranges are always true
  if (filter) {
    append<irs::all>(*filter, filterCtx).boost(filterCtx.boost);
  }

  return {};
}

std::pair<Result, aql::AstNodeType> buildBinaryArrayComparisonPreFilter(
    irs::boolean_filter*& filter, aql::AstNodeType arrayComparison,
    const aql::AstNode* quantifierNode, size_t arraySize,
    FilterContext const& filterCtx) {
  TRI_ASSERT(quantifierNode);
  auto quantifierType =
      static_cast<aql::Quantifier::Type>(quantifierNode->getIntValue(true));

  int64_t atLeastCount{1};
  if (quantifierType == aql::Quantifier::Type::kAtLeast) {
    constexpr std::string_view kFuncName = "AT LEAST";
    if (quantifierNode->numMembers() != 1) {
      return {Result{TRI_ERROR_INTERNAL, "Malformed AT LEAST node"},
              aql::AstNodeType::NODE_TYPE_ROOT};
    }
    auto const& ctx = filterCtx.query;
    auto number = quantifierNode->getMemberUnchecked(0);
    if (!ctx.isSearchQuery && !number->isConstant()) {
      return {Result{TRI_ERROR_NOT_IMPLEMENTED,
                     "Non const AT LEAST is not supported for FILTER"},
              aql::AstNodeType::NODE_TYPE_ROOT};
    }
    ScopedAqlValue atLeastCountValue;
    auto rv = evaluateArg<decltype(atLeastCount), true>(
        atLeastCount, atLeastCountValue, kFuncName.data(), *quantifierNode, 0,
        filter != nullptr, ctx);

    if (rv.fail()) {
      return {rv, aql::AstNodeType::NODE_TYPE_ROOT};
    }

    if (atLeastCount < 0) {
      return {error::negativeNumber(kFuncName.data(), 0),
              aql::AstNodeType::NODE_TYPE_ROOT};
    }

    if (arraySize < static_cast<size_t>(atLeastCount)) {
      if (filter) {
        append<irs::empty>(*filter, filterCtx);
      }
      return {Result{}, aql::NODE_TYPE_ROOT};
    } else if (static_cast<size_t>(atLeastCount) == arraySize) {
      quantifierType = aql::Quantifier::Type::kAll;
    } else if (arrayComparison != aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN &&
               arrayComparison != aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE) {
      // processing  of AT LEAST is different only for NIN/NE case
      quantifierType = aql::Quantifier::Type::kAny;
    }
  }

  aql::AstNodeType expansionNodeType = aql::NODE_TYPE_ROOT;
  if (0 == arraySize) {
    expansionNodeType = aql::NODE_TYPE_ROOT;  // no subfilters expansion needed
    switch (quantifierType) {
      case aql::Quantifier::Type::kAny:
        if (filter) {
          append<irs::empty>(*filter, filterCtx);
        }
        break;
      case aql::Quantifier::Type::kAll:
      case aql::Quantifier::Type::kNone:
        if (filter) {
          append<irs::all>(*filter, filterCtx);
        }
        break;
      case aql::Quantifier::Type::kAtLeast:
        if (filter) {
          if (atLeastCount > 0) {
            append<irs::empty>(*filter, filterCtx);
          } else {
            append<irs::all>(*filter, filterCtx);
          }
        }
        break;
      default:
        TRI_ASSERT(false);  // new qualifier added ?
        return {Result{TRI_ERROR_NOT_IMPLEMENTED,
                       "Unknown qualifier in Array comparison operator"},
                aql::AstNodeType::NODE_TYPE_ROOT};
    }
  } else {
    // NONE is inverted ALL so do conversion
    if (aql::Quantifier::Type::kNone == quantifierType) {
      quantifierType = aql::Quantifier::Type::kAll;
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
          TRI_ASSERT(false);  // new array comparison operator?
          return {Result{TRI_ERROR_NOT_IMPLEMENTED,
                         "Unknown Array NONE comparison operator"},
                  aql::AstNodeType::NODE_TYPE_ROOT};
      }
    }
    switch (quantifierType) {
      case aql::Quantifier::Type::kAll:
        // calculate node type for expanding operation
        // As soon as array is left argument but for filter we place document to
        // the left we reverse comparison operation
        switch (arrayComparison) {
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
            if (filter) {
              filter = &append<irs::And>(*filter, filterCtx);
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_EQ;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
            if (filter) {
              filter = &appendNot<irs::Or>(*filter, filterCtx);
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_EQ;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
            if (filter) {
              filter = &append<irs::And>(*filter, filterCtx);
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_GT;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
            if (filter) {
              filter = &append<irs::And>(*filter, filterCtx);
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_GE;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
            if (filter) {
              filter = &append<irs::And>(*filter, filterCtx);
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_LT;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
            if (filter) {
              filter = &append<irs::And>(*filter, filterCtx);
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_LE;
            break;
          default:
            TRI_ASSERT(false);  // new array comparison operator?
            return {Result{TRI_ERROR_NOT_IMPLEMENTED,
                           "Unknown Array ALL/NONE comparison operator"},
                    aql::AstNodeType::NODE_TYPE_ROOT};
        }
        break;
      case aql::Quantifier::Type::kAny: {
        switch (arrayComparison) {
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
            if (filter) {
              filter = &append<irs::Or>(*filter, filterCtx)
                            .min_match_count(static_cast<size_t>(atLeastCount));
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_EQ;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
            if (filter) {
              filter = &appendNot<irs::And>(*filter, filterCtx);
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_EQ;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
            if (filter) {
              filter = &append<irs::Or>(*filter, filterCtx)
                            .min_match_count(static_cast<size_t>(atLeastCount));
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_LT;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
            if (filter) {
              filter = &append<irs::Or>(*filter, filterCtx)
                            .min_match_count(static_cast<size_t>(atLeastCount));
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_LE;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
            if (filter) {
              filter = &append<irs::Or>(*filter, filterCtx)
                            .min_match_count(static_cast<size_t>(atLeastCount));
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_GT;
            break;
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
            if (filter) {
              filter = &append<irs::Or>(*filter, filterCtx)
                            .min_match_count(static_cast<size_t>(atLeastCount));
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_GE;
            break;
          default:
            TRI_ASSERT(false);  // new array comparison operator?
            return {Result{TRI_ERROR_NOT_IMPLEMENTED,
                           "Unknown Array ANY/AT LEAST comparison operator"},
                    aql::AstNodeType::NODE_TYPE_ROOT};
        }
        break;
      }
      case aql::Quantifier::Type::kAtLeast:
        switch (arrayComparison) {
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
          case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
            if (filter) {
              filter = &append<irs::Or>(*filter, filterCtx)
                            .min_match_count(static_cast<size_t>(atLeastCount));
            }
            expansionNodeType = aql::NODE_TYPE_OPERATOR_BINARY_NE;
            break;
          default:
            TRI_ASSERT(false);  // new array comparison operator?
            return {Result{TRI_ERROR_NOT_IMPLEMENTED,
                           "Unknown Array AT LEAST comparison operator"},
                    aql::AstNodeType::NODE_TYPE_ROOT};
        }
        break;
      default:
        TRI_ASSERT(false);  // new qualifier added ?
        return {Result{TRI_ERROR_NOT_IMPLEMENTED,
                       "Unknown qualifier in Array comparison operator"},
                aql::AstNodeType::NODE_TYPE_ROOT};
    }
  }
  return std::make_pair(TRI_ERROR_NO_ERROR, expansionNodeType);
}

class ByTermSubFilterFactory {
 public:
  static Result byNodeSubFilter(irs::boolean_filter* filter,
                                NormalizedCmpNode const& node,
                                FilterContext const& filterCtx) {
    TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_EQ == node.cmp ||
               aql::NODE_TYPE_OPERATOR_BINARY_NE == node.cmp);
    irs::by_term* termFilter = nullptr;
    if (filter) {
      if (aql::NODE_TYPE_OPERATOR_BINARY_NE == node.cmp) {
        termFilter = &appendNot<irs::by_term>(
            append<irs::And>(*filter, filterCtx), filterCtx);
      } else {
        termFilter = &append<irs::by_term>(*filter, filterCtx);
      }
    }
    return byTerm(termFilter, node, filterCtx);
  }

  static Result byValueSubFilter(irs::boolean_filter* filter,
                                 std::string fieldName,
                                 const ScopedAqlValue& value,
                                 aql::AstNodeType arrayExpansionNodeType,
                                 FilterContext const& filterCtx) {
    TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_EQ == arrayExpansionNodeType ||
               aql::NODE_TYPE_OPERATOR_BINARY_NE == arrayExpansionNodeType);
    irs::by_term* termFilter = nullptr;
    if (filter) {
      if (aql::NODE_TYPE_OPERATOR_BINARY_NE == arrayExpansionNodeType) {
        termFilter = &appendNot<irs::by_term>(
            append<irs::And>(*filter, filterCtx), filterCtx);
      } else {
        termFilter = &append<irs::by_term>(*filter, filterCtx);
      }
    }
    return byTerm(termFilter, std::move(fieldName), value, filterCtx);
  }
};

class ByRangeSubFilterFactory {
 public:
  static Result byNodeSubFilter(irs::boolean_filter* filter,
                                NormalizedCmpNode const& node,
                                FilterContext const& filterCtx) {
    bool incl, min;
    std::tie(min, incl) = calcMinInclude(node.cmp);
    return min ? byRange<true>(filter, node, incl, filterCtx)
               : byRange<false>(filter, node, incl, filterCtx);
  }

  static Result byValueSubFilter(irs::boolean_filter* filter,
                                 std::string fieldName,
                                 const ScopedAqlValue& value,
                                 aql::AstNodeType arrayExpansionNodeType,
                                 FilterContext const& filterCtx) {
    bool incl, min;
    std::tie(min, incl) = calcMinInclude(arrayExpansionNodeType);
    return min ? byRange<true>(filter, fieldName, value, incl, filterCtx)
               : byRange<false>(filter, fieldName, value, incl, filterCtx);
  }

 private:
  static std::pair<bool, bool> calcMinInclude(
      aql::AstNodeType arrayExpansionNodeType) {
    TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_LT == arrayExpansionNodeType ||
               aql::NODE_TYPE_OPERATOR_BINARY_LE == arrayExpansionNodeType ||
               aql::NODE_TYPE_OPERATOR_BINARY_GT == arrayExpansionNodeType ||
               aql::NODE_TYPE_OPERATOR_BINARY_GE == arrayExpansionNodeType);
    return std::pair<bool, bool>{
        // min
        aql::NODE_TYPE_OPERATOR_BINARY_GT == arrayExpansionNodeType ||
            aql::NODE_TYPE_OPERATOR_BINARY_GE == arrayExpansionNodeType,
        // incl
        aql::NODE_TYPE_OPERATOR_BINARY_GE == arrayExpansionNodeType ||
            aql::NODE_TYPE_OPERATOR_BINARY_LE == arrayExpansionNodeType};
  }
};

template<typename SubFilterFactory>
Result fromArrayComparison(irs::boolean_filter*& filter,
                           FilterContext const& filterCtx,
                           aql::AstNode const& node) {
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
    return rv.reset(rv.errorNumber(),
                    absl::StrCat("error in Array comparison operator: ",
                                 rv.errorMessage()));
  }

  auto const* valueNode = node.getMemberUnchecked(0);
  TRI_ASSERT(valueNode);

  auto const* attributeNode = node.getMemberUnchecked(1);
  TRI_ASSERT(attributeNode);

  auto const* quantifierNode = node.getMemberUnchecked(2);
  TRI_ASSERT(quantifierNode);

  if (quantifierNode->type != aql::NODE_TYPE_QUANTIFIER) {
    return {TRI_ERROR_BAD_PARAMETER,
            "wrong qualifier node type for Array comparison operator"};
  }

  auto const& ctx = filterCtx.query;

  if (aql::NODE_TYPE_ARRAY == valueNode->type) {
    if (!attributeNode->isDeterministic()) {
      // not supported by IResearch, but could be handled by ArangoDB
      return fromExpression(filter, filterCtx, node);
    }
    size_t const n = valueNode->numMembers();
    if (!checkAttributeAccess(attributeNode, *ctx.ref, !ctx.isSearchQuery)) {
      // no attribute access specified in attribute node, try to
      // find it in value node
      bool attributeAccessFound = false;
      for (size_t i = 0; i < n; ++i) {
        attributeAccessFound |=
            (nullptr != checkAttributeAccess(valueNode->getMemberUnchecked(i),
                                             *ctx.ref, !ctx.isSearchQuery));
      }
      if (!attributeAccessFound) {
        return fromExpression(filter, filterCtx, node);
      }
    }
    Result buildRes;
    aql::AstNodeType arrayExpansionNodeType;
    std::tie(buildRes, arrayExpansionNodeType) =
        buildBinaryArrayComparisonPreFilter(filter, node.type, quantifierNode,
                                            n, filterCtx);
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
        .query = filterCtx.query,
        .contextAnalyzer = filterCtx.contextAnalyzer,
        .fieldAnalyzerProvider = filterCtx.fieldAnalyzerProvider,
        .boost = irs::kNoBoost};  // reset boost

    // Expand array interval as several binaryInterval nodes ('array' feature is
    // ensured by pre-filter)
    NormalizedCmpNode normalized;
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
      if (!normalizeCmpNode(toNormalize, *ctx.ref, !ctx.isSearchQuery,
                            normalized)) {
        if (!filter) {
          // can't evaluate non constant filter before the execution
          if (ctx.isSearchQuery) {
            return {};
          } else {
            return {TRI_ERROR_NOT_IMPLEMENTED,
                    "ByExpression filter is supported for SEARCH only"};
          }
        }
        // use std::shared_ptr since AstNode is not copyable/moveable
        auto exprNode = std::make_shared<aql::AstNode>(arrayExpansionNodeType);
        exprNode->reserve(2);
        exprNode->addMember(attributeNode);
        exprNode->addMember(member);

        // not supported by IResearch, but could be handled by ArangoDB
        auto rv = fromExpression(filter, subFilterCtx, std::move(exprNode));
        if (rv.fail()) {
          return rv.reset(
              rv.errorNumber(),
              absl::StrCat("while getting array: ", rv.errorMessage()));
        }
      } else {
        auto rv =
            SubFilterFactory::byNodeSubFilter(filter, normalized, subFilterCtx);
        if (rv.fail()) {
          return rv.reset(
              rv.errorNumber(),
              absl::StrCat("while getting array: ", rv.errorMessage()));
        }
      }
    }
    return {};
  }

  if (!node.isDeterministic() ||
      !checkAttributeAccess(attributeNode, *ctx.ref, !ctx.isSearchQuery) ||
      findReference(*valueNode, *ctx.ref)) {
    return fromExpression(filter, filterCtx, node);
  }

  std::string fieldName{ctx.namePrefix};
  if (!nameFromAttributeAccess(fieldName, *attributeNode, ctx,
                               filter != nullptr)) {
    return error::failedToGenerateName("fromArrayComparison", 1);
  }

  if (!filter) {
    // can't evaluate non constant filter before the execution
    return {};
  }

  ScopedAqlValue value(*valueNode);
  if (!value.execute(ctx)) {
    // can't execute expression
    return {TRI_ERROR_BAD_PARAMETER,
            "Unable to extract value from Array comparison operator"};
  }

  switch (value.type()) {
    case SCOPED_VALUE_TYPE_ARRAY: {
      size_t const n = value.size();
      Result buildRes;
      aql::AstNodeType arrayExpansionNodeType;
      std::tie(buildRes, arrayExpansionNodeType) =
          buildBinaryArrayComparisonPreFilter(filter, node.type, quantifierNode,
                                              n, filterCtx);
      if (!buildRes.ok()) {
        return buildRes;
      }
      filter->boost(filterCtx.boost);
      if (aql::NODE_TYPE_ROOT == arrayExpansionNodeType) {
        // nothing to do more
        return {};
      }

      FilterContext const subFilterCtx{
          .query = filterCtx.query,
          .contextAnalyzer = filterCtx.contextAnalyzer,
          .fieldAnalyzerProvider = filterCtx.fieldAnalyzerProvider,
          .boost = irs::kNoBoost};  // reset boost

      for (size_t i = 0; i < n; ++i) {
        auto rv = SubFilterFactory::byValueSubFilter(
            filter, fieldName, value.at(i), arrayExpansionNodeType,
            subFilterCtx);
        if (rv.fail()) {
          return rv.reset(rv.errorNumber(),
                          absl::StrCat("failed to create filter because: ",
                                       rv.errorMessage()));
        }
      }
      return {};
    }
    default:
      break;
  }

  // wrong value node type
  return {TRI_ERROR_BAD_PARAMETER,
          "wrong value node type for Array comparison operator"};
}

Result fromInArray(irs::boolean_filter* filter, FilterContext const& filterCtx,
                   aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_IN == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type);

  // `attributeNode` IN `valueNode`
  auto const* attributeNode = node.getMemberUnchecked(0);
  TRI_ASSERT(attributeNode);
  auto const* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(valueNode && aql::NODE_TYPE_ARRAY == valueNode->type);

  if (!attributeNode->isDeterministic()) {
    // not supported by IResearch, but could be handled by ArangoDB
    return fromExpression(filter, filterCtx, node);
  }

  auto const& ctx = filterCtx.query;
  size_t const n = valueNode->numMembers();

  if (!checkAttributeAccess(attributeNode, *ctx.ref, !ctx.isSearchQuery)) {
    // no attribute access specified in attribute node, try to
    // find it in value node

    bool attributeAccessFound = false;
    for (size_t i = 0; i < n; ++i) {
      attributeAccessFound |=
          (nullptr != checkAttributeAccess(valueNode->getMemberUnchecked(i),
                                           *ctx.ref, !ctx.isSearchQuery));
    }

    if (!attributeAccessFound) {
      return fromExpression(filter, filterCtx, node);
    }
  }

  if (!filter && !ctx.isSearchQuery) {
    // but at least we must validate attribute name
    std::string fieldName{ctx.namePrefix};
    if (!nameFromAttributeAccess(fieldName, *attributeNode, ctx, false)) {
      return error::failedToGenerateName("fromInArray", 1);
    }
  }

  if (!n) {
    if (filter) {
      if (aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
        // not in [] means 'all'
        append<irs::all>(*filter, filterCtx).boost(filterCtx.boost);
      } else {
        append<irs::empty>(*filter, filterCtx);
      }
    }

    // nothing to do more
    return {};
  }

  if (filter) {
    filter = aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type
                 ? &static_cast<irs::boolean_filter&>(
                       appendNot<irs::Or>(*filter, filterCtx))
                 : &static_cast<irs::boolean_filter&>(
                       append<irs::Or>(*filter, filterCtx));
    filter->boost(filterCtx.boost);
  }

  FilterContext const subFilterCtx{
      .query = filterCtx.query,
      .contextAnalyzer = filterCtx.contextAnalyzer,
      .fieldAnalyzerProvider = filterCtx.fieldAnalyzerProvider,
      .boost = irs::kNoBoost};  // reset boost

  NormalizedCmpNode normalized;
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

    if (!normalizeCmpNode(toNormalize, *ctx.ref, !ctx.isSearchQuery,
                          normalized)) {
      if (!filter) {
        // can't evaluate non constant filter before the execution
        if (ctx.isSearchQuery) {
          return {};
        } else {
          return {TRI_ERROR_NOT_IMPLEMENTED,
                  "ByExpression filter is supported for SEARCH only"};
        }
      }

      // use std::shared_ptr since AstNode is not copyable/moveable
      auto exprNode =
          std::make_shared<aql::AstNode>(aql::NODE_TYPE_OPERATOR_BINARY_EQ);
      exprNode->reserve(2);
      exprNode->addMember(attributeNode);
      exprNode->addMember(member);

      // not supported by IResearch, but could be handled by ArangoDB
      auto rv = fromExpression(filter, subFilterCtx, std::move(exprNode));
      if (rv.fail()) {
        return rv.reset(rv.errorNumber(), absl::StrCat("while getting array: ",
                                                       rv.errorMessage()));
      }
    } else {
      auto* termFilter =
          filter ? &append<irs::by_term>(*filter, filterCtx) : nullptr;

      auto rv = byTerm(termFilter, normalized, subFilterCtx);
      if (rv.fail()) {
        return rv.reset(rv.errorNumber(), absl::StrCat("while getting array: ",
                                                       rv.errorMessage()));
      }
    }
  }

  return {};
}

Result fromIn(irs::boolean_filter* filter, FilterContext const& filterCtx,
              aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_IN == node.type ||
             aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type);

  if (node.numMembers() != 2) {
    auto rv = error::malformedNode(node.type);
    return rv.reset(rv.errorNumber(),
                    absl::StrCat("error in from In", rv.errorMessage()));
  }

  auto const* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(valueNode);

  if (aql::NODE_TYPE_ARRAY == valueNode->type) {
    return fromInArray(filter, filterCtx, node);
  }

  auto const& ctx = filterCtx.query;

  auto* attributeNode = node.getMemberUnchecked(0);
  TRI_ASSERT(attributeNode);

  if (!node.isDeterministic() ||
      !checkAttributeAccess(attributeNode, *ctx.ref, !ctx.isSearchQuery) ||
      findReference(*valueNode, *ctx.ref)) {
    return fromExpression(filter, filterCtx, node);
  }

  if (!filter) {
    // can't evaluate non constant filter before the execution
    if (!ctx.isSearchQuery) {
      // but at least we must validate attribute name
      std::string fieldName{ctx.namePrefix};
      if (!nameFromAttributeAccess(fieldName, *attributeNode, ctx, false)) {
        return error::failedToGenerateName("fromIn", 1);
      }
    }
    return {};
  }

  if (aql::NODE_TYPE_RANGE == valueNode->type) {
    ScopedAqlValue value(*valueNode);

    if (!value.execute(ctx)) {
      // con't execute expression
      return {TRI_ERROR_BAD_PARAMETER,
              "Unable to extract value from 'IN' operator"};
    }

    // range
    auto const* range = value.getRange();
    if (!range) {
      return {TRI_ERROR_BAD_PARAMETER, "no valid range"};
    }

    if (aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
      // handle negation
      filter = &appendNot<irs::Or>(*filter, filterCtx);
    }

    return byRange(filter, *attributeNode, *range, filterCtx);
  }

  ScopedAqlValue value(*valueNode);

  if (!value.execute(ctx)) {
    // con't execute expression
    return {TRI_ERROR_BAD_PARAMETER,
            "Unable to extract value from 'IN' operator"};
  }

  switch (value.type()) {
    case SCOPED_VALUE_TYPE_ARRAY: {
      size_t const n = value.size();

      if (!n) {
        if (aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
          // not in [] means 'all'
          append<irs::all>(*filter, filterCtx).boost(filterCtx.boost);
        } else {
          append<irs::empty>(*filter, filterCtx);
        }

        // nothing to do more
        return {};
      }

      filter = aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type
                   ? &static_cast<irs::boolean_filter&>(
                         appendNot<irs::Or>(*filter, filterCtx))
                   : &static_cast<irs::boolean_filter&>(
                         append<irs::Or>(*filter, filterCtx));
      filter->boost(filterCtx.boost);

      FilterContext const subFilterCtx{
          .query = filterCtx.query,
          .contextAnalyzer = filterCtx.contextAnalyzer,
          .fieldAnalyzerProvider = filterCtx.fieldAnalyzerProvider,
          .boost = irs::kNoBoost};  // reset boost

      for (size_t i = 0; i < n; ++i) {
        // failed to create a filter
        auto rv = byTerm(&append<irs::by_term>(*filter, filterCtx),
                         *attributeNode, value.at(i), subFilterCtx);
        if (rv.fail()) {
          return rv.reset(rv.errorNumber(),
                          absl::StrCat("failed to create filter because: ",
                                       rv.errorMessage()));
        }
      }

      return {};
    }
    case SCOPED_VALUE_TYPE_RANGE: {
      // range
      auto const* range = value.getRange();

      if (!range) {
        return {TRI_ERROR_BAD_PARAMETER, "no valid range"};
      }

      if (aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type) {
        // handle negation
        filter = &appendNot<irs::Or>(*filter, filterCtx);
      }

      return byRange(filter, *attributeNode, *range, filterCtx);
    }
    default:
      break;
  }

  // wrong value node type
  return {TRI_ERROR_BAD_PARAMETER, "wrong value node type"};
}

Result fromNegation(irs::boolean_filter* filter, FilterContext const& filterCtx,
                    aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_OPERATOR_UNARY_NOT == node.type);

  if (node.numMembers() != 1) {
    auto rv = error::malformedNode(node.type);
    return rv.reset(rv.errorNumber(),
                    absl::StrCat("Bad node in negation", rv.errorMessage()));
  }

  auto const* member = node.getMemberUnchecked(0);
  TRI_ASSERT(member);

  if (filter) {
    if (filter->type() == irs::type<irs::Or>::id()) {
      // wrap negation in a disjunction into a dedicated conjunction
      filter = &append<irs::And>(*filter, filterCtx);
    }
    auto& notFilter = append<irs::Not>(*filter, filterCtx);
    notFilter.boost(filterCtx.boost);

    filter = &append<irs::And>(notFilter, filterCtx);
  }

  FilterContext const subFilterCtx{
      .query = filterCtx.query,
      .contextAnalyzer = filterCtx.contextAnalyzer,
      .fieldAnalyzerProvider = filterCtx.fieldAnalyzerProvider,
      .boost = irs::kNoBoost};  // reset boost

#ifdef USE_ENTERPRISE
  if (member->type == aql::NODE_TYPE_EXPANSION &&
      member->hasFlag(aql::FLAG_BOOLEAN_EXPANSION)) {
    // Special handling for negative nested queries
    return fromBooleanExpansion(filter, subFilterCtx, *member);
  }
#endif

  return makeFilter(filter, subFilterCtx, *member);
}

/*
bool rangeFromBinaryAnd(irs::boolean_filter* filter,
                        FilterContext const& filterCtx,
                        aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_OPERATOR_BINARY_AND == node.type ||
             aql::NODE_TYPE_OPERATOR_NARY_AND == node.type);

  if (node.numMembers() != 2) {
    logMalformedNode(node);
    return false;  // wrong number of members
  }

  auto const& ctx = filterCtx.query;

  auto const* lhsNode = node.getMemberUnchecked(0);
  TRI_ASSERT(lhsNode);
  auto const* rhsNode = node.getMemberUnchecked(1);
  TRI_ASSERT(rhsNode);

  NormalizedCmpNode lhsNormNode, rhsNormNode;

  if (normalizeCmpNode(*lhsNode, *ctx.ref, lhsNormNode) &&
      normalizeCmpNode(*rhsNode, *ctx.ref, rhsNormNode)) {
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

      if (attributeAccessEqual(lhsAttr, rhsAttr, filter ?
&ctx : nullptr)) { auto const* lhsValue = lhsNormNode.value; auto const*
rhsValue = rhsNormNode.value;

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

template<typename Filter>
Result fromGroup(irs::boolean_filter* filter, FilterContext const& filterCtx,
                 aql::AstNode const& node) {
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
    filter = &append<Filter>(*filter, filterCtx);
    filter->boost(filterCtx.boost);
  }

  FilterContext const subFilterCtx{
      .query = filterCtx.query,
      .contextAnalyzer = filterCtx.contextAnalyzer,
      .fieldAnalyzerProvider = filterCtx.fieldAnalyzerProvider,
      .boost = irs::kNoBoost};  // reset boost

  for (size_t i = 0; i < n; ++i) {
    auto const* valueNode = node.getMemberUnchecked(i);
    TRI_ASSERT(valueNode);

    auto const rv = makeFilter(filter, subFilterCtx, *valueNode);
    if (rv.fail()) {
      return rv;
    }
  }

  return {};
}

// ANALYZER(<filter-expression>, analyzer)
Result fromFuncAnalyzer(char const* funcName, irs::boolean_filter* filter,
                        FilterContext const& filterCtx,
                        aql::AstNode const& args) {
  TRI_ASSERT(funcName);
  auto const& ctx = filterCtx.query;

  if (!ctx.isSearchQuery) {
    return {TRI_ERROR_NOT_IMPLEMENTED, "ANALYZER is supported for SEARCH only"};
  }

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
  std::string_view analyzerId;
  ScopedAqlValue analyzerIdValue;

  auto rv = evaluateArg<decltype(analyzerId), true>(
      analyzerId, analyzerIdValue, funcName, args, 1, filter != nullptr, ctx);

  if (rv.fail()) {
    return rv;
  }

  // default analyzer
  FieldMeta::Analyzer analyzer{FieldMeta::identity()};
  if (filter || analyzerIdValue.isConstant()) {
    TRI_ASSERT(ctx.trx);
    auto& vocbase = ctx.trx->vocbase();
    auto& server = vocbase.server();
    if (!server.hasFeature<IResearchAnalyzerFeature>()) {
      return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
              absl::StrCat("'", IResearchAnalyzerFeature::name(),
                           "' feature is not registered, unable to evaluate '",
                           funcName, "' function")};
    }
    auto& analyzerFeature = server.getFeature<IResearchAnalyzerFeature>();
    analyzer._pool = analyzerFeature.get(analyzerId, ctx.trx->vocbase(),
                                         ctx.trx->state()->analyzersRevision());
    if (!analyzer) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat(
                  "'", funcName, "' AQL function: Unable to lookup analyzer '",
                  std::string_view{analyzerId.data(), analyzerId.size()}, "'")};
    }
    analyzer._shortName =
        IResearchAnalyzerFeature::normalize(analyzerId, vocbase.name(), false);
  }

  // override analyzer and throw away provider
  FilterContext const subFilterContext{
      .query = filterCtx.query,
      .contextAnalyzer = analyzer,
      .fieldAnalyzerProvider =
          ctx.isOldMangling ? nullptr : filterCtx.fieldAnalyzerProvider,
      .boost = filterCtx.boost};

  rv = makeFilter(filter, subFilterContext, *expressionArg);

  if (rv.fail()) {
    return {rv.errorNumber(),
            absl::StrCat("failed to get filter for analyzer: ",
                         analyzer->name(), " : ", rv.errorMessage())};
  }
  return rv;
}

// BOOST(<filter-expression>, boost)
Result fromFuncBoost(char const* funcName, irs::boolean_filter* filter,
                     FilterContext const& filterCtx, aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  auto const& ctx = filterCtx.query;

  if (!ctx.isSearchQuery) {
    return {TRI_ERROR_NOT_IMPLEMENTED, "BOOST is supported for SEARCH only"};
  }

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
  double boostValue = 0;
  auto rv = evaluateArg<decltype(boostValue), true>(
      boostValue, tmpValue, funcName, args, 1, filter != nullptr, ctx);

  if (rv.fail()) {
    return rv;
  }

  FilterContext const subFilterContext{
      .query = filterCtx.query,
      .contextAnalyzer = filterCtx.contextAnalyzer,
      .fieldAnalyzerProvider = filterCtx.fieldAnalyzerProvider,
      .boost = filterCtx.boost * static_cast<float_t>(boostValue)};

  rv = makeFilter(filter, subFilterContext, *expressionArg);

  if (rv.fail()) {
    return {rv.errorNumber(),
            absl::StrCat("error in sub-filter context: ", rv.errorMessage())};
  }

  return {};
}

static constexpr std::string_view kTypeAnalyzer{"analyzer"};

using TypeHandler = bool (*)(std::string&, bool isOldMangling,
                             FieldMeta::Analyzer const&);

static constexpr frozen::map<std::string_view, TypeHandler, 8> kTypeHandlers{
    // any string
    {"string",
     [](std::string& name, bool isOldMangling,
        FieldMeta::Analyzer const&) -> bool {
       // We don't make '|| analyzer->requireMangled()'
       // because we don't want to give opportunity
       // to find analyzer field by 'string'
       if (isOldMangling) {
         kludge::mangleAnalyzer(name);
       } else {
         kludge::mangleString(name);
       }
       return true;  // a prefix match
     }},
    // any non-string type
    {"type",
     [](std::string& name, bool, FieldMeta::Analyzer const&) -> bool {
       kludge::mangleType(name);
       return true;  // a prefix match
     }},
    // concrete analyzer from the context
    {kTypeAnalyzer,
     [](std::string& name, bool isOldMangling,
        FieldMeta::Analyzer const& analyzer) -> bool {
       kludge::mangleField(name, isOldMangling, analyzer);
       return false;  // not a prefix match
     }},
    {"numeric",
     [](std::string& name, bool, FieldMeta::Analyzer const&) -> bool {
       kludge::mangleNumeric(name);
       return false;  // not a prefix match
     }},
    {"bool",
     [](std::string& name, bool, FieldMeta::Analyzer const&) -> bool {
       kludge::mangleBool(name);
       return false;  // not a prefix match
     }},
    {"boolean",
     [](std::string& name, bool, FieldMeta::Analyzer const&) -> bool {
       kludge::mangleBool(name);
       return false;  // not a prefix match
     }},
    {"nested",
     [](std::string& name, bool, FieldMeta::Analyzer const&) -> bool {
       kludge::mangleNested(name);
       return false;  // not a prefix match
     }},
    {"null", [](std::string& name, bool, FieldMeta::Analyzer const&) -> bool {
       kludge::mangleNull(name);
       return false;  // not a prefix match
     }}};

// EXISTS(<attribute>, <"analyzer">, <"analyzer-name">)
// EXISTS(<attribute>, <"string"|"null"|"bool"|"numeric"|"nested">)
Result fromFuncExists(char const* funcName, irs::boolean_filter* filter,
                      FilterContext const& filterCtx,
                      aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  if (!args.isDeterministic()) {
    return error::nondeterministicArgs(funcName);
  }

  auto const& ctx = filterCtx.query;

  auto const argc = args.numMembers();

  if (argc < 1 || argc > 3) {
    return error::invalidArgsCount<error::Range<1, 3>>(funcName);
  }

  // 1st argument defines a field
  auto const* fieldArg = checkAttributeAccess(args.getMemberUnchecked(0),
                                              *ctx.ref, !ctx.isSearchQuery);

  if (!fieldArg) {
    return error::invalidAttribute(funcName, 1);
  }

  bool prefixMatch = true;
  auto const isOldMangling = ctx.isOldMangling;

  std::string fieldName{ctx.namePrefix};
  if (!nameFromAttributeAccess(fieldName, *fieldArg, ctx, filter != nullptr)) {
    return error::failedToGenerateName(funcName, 1);
  }

  auto analyzer = filterCtx.fieldAnalyzer(fieldName, ctx.ctx);
  if (!analyzer) {
    return {TRI_ERROR_INTERNAL, "Malformed search/filter context"};
  }

  if (argc > 1) {
    // 2nd argument defines a type (if present)
    ScopedAqlValue argValue;
    std::string_view arg;
    auto rv =
        evaluateArg(arg, argValue, funcName, args, 1, filter != nullptr, ctx);

    if (rv.fail()) {
      return rv;
    }

    if (filter || argValue.isConstant()) {  // arg is constant
      std::string strArg(arg);
      std::transform(strArg.begin(), strArg.end(), strArg.begin(), ::tolower);

      auto const typeHandler = kTypeHandlers.find(strArg);

      if (kTypeHandlers.end() == typeHandler) {
        return {
            TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("'", funcName,
                         "' AQL function: 2nd argument must be equal to one of "
                         "the following: 'string', 'type', 'analyzer', "
                         "'numeric', 'bool', 'boolean', 'null', but got '",
                         std::string_view{arg}, "'")};
      }

      if (argc > 2) {
        if (kTypeAnalyzer.data() != typeHandler->first.data()) {
          return {TRI_ERROR_BAD_PARAMETER,
                  absl::StrCat("'", funcName,
                               "' AQL function: 3rd argument is intended to be "
                               "used with 'analyzer' type only")};
        }

        rv = extractAnalyzerFromArg(analyzer, funcName, filter, args, 2, ctx);

        if (rv.fail()) {
          return rv;
        }

        if (!filter && !analyzer) {
          // use default context analyzer for the optimization stage
          // if actual analyzer could not be evaluated for now
          analyzer = filterCtx.contextAnalyzer;
        }

        if (!analyzer) {
          TRI_ASSERT(false);
          return {TRI_ERROR_INTERNAL, "analyzer not found"};
        }
      }

      prefixMatch = typeHandler->second(fieldName, isOldMangling, analyzer);
    }
  }

  if (filter) {
    auto& exists = append<irs::by_column_existence>(*filter, filterCtx);
    if (auto* opts = exists.mutable_options(); prefixMatch) {
      opts->acceptor = makeColumnAcceptor(!ctx.namePrefix.empty());
    }
    *exists.mutable_field() = std::move(fieldName);
    exists.boost(filterCtx.boost);
  }

  return {};
}

// MIN_MATCH(<filter-expression>[, <filter-expression>,...], <min-match-count>)
Result fromFuncMinMatch(char const* funcName, irs::boolean_filter* filter,
                        FilterContext const& filterCtx,
                        aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  auto const argc = args.numMembers();

  if (argc < 2) {
    return error::invalidArgsCount<error::OpenRange<false, 2>>(funcName);
  }

  auto const& ctx = filterCtx.query;

  // last argument defines min match count

  auto const lastArg = argc - 1;
  ScopedAqlValue minMatchCountValue;
  int64_t minMatchCount = 0;

  auto rv = evaluateArg<decltype(minMatchCount), true>(
      minMatchCount, minMatchCountValue, funcName, args, lastArg,
      filter != nullptr, ctx);

  if (rv.fail()) {
    return rv;
  }

  if (minMatchCount < 0) {
    return error::negativeNumber(funcName, argc);
  }

  if (filter) {
    auto& minMatchFilter = append<irs::Or>(*filter, filterCtx);
    minMatchFilter.min_match_count(static_cast<size_t>(minMatchCount));
    minMatchFilter.boost(filterCtx.boost);

    // become a new root
    filter = &minMatchFilter;
  }

  FilterContext const subFilterCtx{
      .query = filterCtx.query,
      .contextAnalyzer = filterCtx.contextAnalyzer,
      .fieldAnalyzerProvider = filterCtx.fieldAnalyzerProvider};

  for (size_t i = 0; i < lastArg; ++i) {
    auto subFilterExpression = args.getMemberUnchecked(i);

    if (!subFilterExpression) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat(
                  "'", funcName,
                  "' AQL function: Failed to evaluate argument at position '",
                  i, "'")};
    }

    irs::boolean_filter* subFilter =
        filter ? &append<irs::Or>(*filter, filterCtx) : nullptr;

    rv = makeFilter(subFilter, subFilterCtx, *subFilterExpression);
    if (rv.fail()) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("'", funcName,
                           "' AQL function: Failed to instantiate sub-filter "
                           "for argument at position '",
                           i, "': ", rv.errorMessage())};
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

  static ScopedValueType scopedType(ValueType const& v) { return v.type(); }

  static VPackSlice valueSlice(ValueType const& v) { return v.slice(); }

  static size_t numValueMembers(ValueType const& v) { return v.size(); }

  static bool isValueNumber(ValueType const& v) noexcept {
    return v.isDouble();
  }

  static int64_t getValueInt64(ValueType const& v) {
    TRI_ASSERT(v.isDouble());
    return v.getInt64();
  }

  static bool getValueString(ValueType const& v, std::string_view& str) {
    return v.getString(str);
  }

  static bool isDeterministic(aql::AstNode const& arg) {
    return arg.isDeterministic();
  }

  static auto numMembers(aql::AstNode const& arg) { return arg.numMembers(); }

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
    return ::evaluateArg<T, CheckDeterminism>(out, value, funcName, args, i,
                                              isFilter, ctx);
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
        break;  // Make Clang happy
    }
    return SCOPED_VALUE_TYPE_INVALID;
  }

  static ValueType valueSlice(ValueType v) noexcept { return v; }

  static size_t numValueMembers(ValueType v) {
    TRI_ASSERT(v.isArray());
    return v.length();
  }

  static bool isValueNumber(ValueType v) noexcept { return v.isNumber(); }

  static int64_t getValueInt64(ValueType v) {
    TRI_ASSERT(v.isNumber());
    return v.getNumber<int64_t>();
  }

  static bool getValueString(ValueType v, std::string_view& str) {
    if (v.isString()) {
      str = v.stringView();
      return true;
    }
    return false;
  }

  constexpr static bool isDeterministic(VPackSlice) { return true; }

  static size_t numMembers(VPackSlice arg) {
    if (arg.isArray()) {
      return arg.length();
    }
    return 1;
  }

  static Result getMemberValue(VPackSlice arg, size_t idx,
                               char const* /*funcName*/, ValueType& value, bool,
                               QueryContext const&, bool&) {
    TRI_ASSERT(arg.isArray());
    TRI_ASSERT(arg.length() > idx);
    value = arg.at(idx);
    return {};
  }

  template<typename T>
  static Result evaluateArg(T& out, ValueType& value, char const* funcName,
                            VPackSlice args, size_t i, bool /*isFilter*/,
                            QueryContext const& /*ctx*/) {
    static_assert(std::is_same_v<T, std::string_view> ||
                  std::is_same_v<T, int64_t> || std::is_same_v<T, double> ||
                  std::is_same_v<T, bool>);

    if (!args.isArray() || args.length() <= i) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("'", funcName,
                           "' AQL function: invalid argument index ", i)};
    }
    value = args.at(i);
    if constexpr (std::is_same_v<T, std::string_view>) {
      if (value.isString()) {
        out = value.stringView();
        return {};
      }
    } else if constexpr (std::is_same_v<T, int64_t>) {
      if (value.isNumber()) {
        out = value.getInt();
        return {};
      }
    } else if constexpr (std::is_same_v<T, double>) {
      if (value.getDouble(out)) {
        return {};
      }
    } else if constexpr (std::is_same_v<T, bool>) {
      if (value.isBoolean()) {
        out = value.getBoolean();
        return {};
      }
    }
    return {
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("'", funcName, "' AQL function: argument at position '",
                     i + 1, "' has invalid type '", value.typeName(), "'")};
  }
};

using ConversionPhraseHandler = Result (*)(char const*, size_t, char const*,
                                           irs::by_phrase*,
                                           FilterContext const&,
                                           const VPackSlice&, size_t,
                                           irs::analysis::analyzer*);

std::string getSubFuncErrorSuffix(char const* funcName,
                                  size_t const funcArgumentPosition) {
  return absl::StrCat(" (in '", funcName, "' AQL function at position '",
                      funcArgumentPosition + 1, "')");
}

Result oneArgumentfromFuncPhrase(char const* funcName,
                                 size_t const funcArgumentPosition,
                                 char const* subFuncName, VPackSlice elem,
                                 std::string_view& term) {
  if (elem.isArray() && elem.length() != 1) {
    return error::invalidArgsCount<error::ExactValue<1>>(subFuncName)
        .withError([&](arangodb::result::Error& err) {
          err.appendErrorMessage(
              getSubFuncErrorSuffix(funcName, funcArgumentPosition));
        });
  }
  auto actualArg = elem.isArray() ? elem.at(0) : elem;

  if (!actualArg.isString()) {
    return error::typeMismatch(subFuncName, funcArgumentPosition,
                               SCOPED_VALUE_TYPE_STRING,
                               ArgsTraits<VPackSlice>::scopedType(actualArg));
  }
  term = actualArg.stringView();
  return {};
}

// {<TERM>: [ '[' ] <term> [ ']' ] }
Result fromFuncPhraseTerm(char const* funcName, size_t funcArgumentPosition,
                          char const* subFuncName, irs::by_phrase* filter,
                          FilterContext const&, const VPackSlice& elem,
                          size_t firstOffset,
                          irs::analysis::analyzer* /*analyzer*/ = nullptr) {
  std::string_view term;
  auto res = oneArgumentfromFuncPhrase(funcName, funcArgumentPosition,
                                       subFuncName, elem, term);
  if (res.fail()) {
    return res;
  }

  if (filter) {
    auto* opts = filter->mutable_options();
    opts->push_back<irs::by_term_options>(firstOffset)
        .term.assign(irs::ViewCast<irs::byte_type>(term));
  }

  return {};
}

// {<STARTS_WITH>: [ '[' ] <term> [ ']' ] }
Result fromFuncPhraseStartsWith(
    char const* funcName, size_t funcArgumentPosition, char const* subFuncName,
    irs::by_phrase* filter, FilterContext const&, const VPackSlice& elem,
    size_t firstOffset, irs::analysis::analyzer* /*analyzer*/ = nullptr) {
  std::string_view term;
  auto res = oneArgumentfromFuncPhrase(funcName, funcArgumentPosition,
                                       subFuncName, elem, term);
  if (res.fail()) {
    return res;
  }
  if (filter) {
    auto& prefix = filter->mutable_options()->push_back<irs::by_prefix_options>(
        firstOffset);
    prefix.term.assign(irs::ViewCast<irs::byte_type>(term));
    prefix.scored_terms_limit = FilterConstants::DefaultScoringTermsLimit;
  }
  return {};
}

// {<WILDCARD>: [ '[' ] <term> [ ']' ] }
Result fromFuncPhraseLike(char const* funcName,
                          size_t const funcArgumentPosition,
                          char const* subFuncName, irs::by_phrase* filter,
                          FilterContext const&, const VPackSlice& elem,
                          size_t firstOffset,
                          irs::analysis::analyzer* /*analyzer*/ = nullptr) {
  std::string_view term;
  auto res = oneArgumentfromFuncPhrase(funcName, funcArgumentPosition,
                                       subFuncName, elem, term);
  if (res.fail()) {
    return res;
  }
  if (filter) {
    auto& wildcard =
        filter->mutable_options()->push_back<irs::by_wildcard_options>(
            firstOffset);
    wildcard.term.assign(irs::ViewCast<irs::byte_type>(term));
    wildcard.scored_terms_limit = FilterConstants::DefaultScoringTermsLimit;
  }
  return {};
}

template<size_t First, typename ElementType,
         typename ElementTraits = ArgsTraits<ElementType>>
Result getLevenshteinArguments(char const* funcName, bool isFilter,
                               FilterContext const& filterCtx,
                               ElementType const& args,
                               aql::AstNode const** field,
                               typename ElementTraits::ValueType& targetValue,
                               irs::by_edit_distance_options& opts,
                               std::string const& errorSuffix = {}) {
  if (!ElementTraits::isDeterministic(args)) {
    return error::nondeterministicArgs(funcName).withError(
        [&](arangodb::result::Error& err) {
          err.appendErrorMessage(errorSuffix);
        });
  }
  auto const argc = ElementTraits::numMembers(args);
  constexpr size_t min = 3 - First;
  constexpr size_t max = 6 - First;
  if (argc < min || argc > max) {
    return error::invalidArgsCount<error::Range<min, max>>(funcName).withError(
        [&](arangodb::result::Error& err) {
          err.appendErrorMessage(errorSuffix);
        });
  }

  auto const& ctx = filterCtx.query;

  if constexpr (0 == First) {
    // this is done only for AstNode so don`t bother with traits
    static_assert(std::is_same_v<aql::AstNode, ElementType>,
                  "Only AstNode supported for parsing attribute");
    TRI_ASSERT(field);
    // (0 - First) argument defines a field
    *field = checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref,
                                  !ctx.isSearchQuery);

    if (!*field) {
      return error::invalidAttribute(funcName, 1);
    }
  }

  // (1 - First) argument defines a target
  std::string_view target;
  auto res = ElementTraits::evaluateArg(target, targetValue, funcName, args,
                                        1 - First, isFilter, ctx);

  if (res.fail()) {
    return res.withError([&](arangodb::result::Error& err) {
      err.appendErrorMessage(errorSuffix);
    });
  }

  typename ElementTraits::ValueType
      tmpValue;  // can reuse value for int64_t and bool

  // (2 - First) argument defines a max distance
  int64_t maxDistance = 0;
  res = ElementTraits::evaluateArg(maxDistance, tmpValue, funcName, args,
                                   2 - First, isFilter, ctx);

  if (res.fail()) {
    return res.withError([&](arangodb::result::Error& err) {
      err.appendErrorMessage(errorSuffix);
    });
  }

  if (maxDistance < 0) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat(
                "'", funcName,
                "' AQL function: max distance must be a non-negative number",
                errorSuffix)};
  }

  // optional (3 - First) argument defines transpositions
  bool withTranspositions = true;
  if (3 - First < argc) {
    res = ElementTraits::evaluateArg(withTranspositions, tmpValue, funcName,
                                     args, 3 - First, isFilter, ctx);

    if (res.fail()) {
      return res.withError([&](arangodb::result::Error& err) {
        err.appendErrorMessage(errorSuffix);
      });
    }
  }

  if (!withTranspositions && maxDistance > kMaxLevenshteinDistance) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("'", funcName,
                         "' AQL function: max Levenshtein distance must be a "
                         "number in range [0, ",
                         kMaxLevenshteinDistance, "]", errorSuffix)};
  } else if (withTranspositions &&
             maxDistance > kMaxDamerauLevenshteinDistance) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("'", funcName,
                     "' AQL function: max Damerau-Levenshtein distance must "
                     "be a number in range [0, ",
                     kMaxDamerauLevenshteinDistance, "]", errorSuffix)};
  }

  // optional (4 - First) argument defines terms limit
  int64_t maxTerms = FilterConstants::DefaultLevenshteinTermsLimit;
  if (4 - First < argc) {
    res = ElementTraits::evaluateArg(maxTerms, tmpValue, funcName, args,
                                     4 - First, isFilter, ctx);

    if (res.fail()) {
      return res.withError([&](arangodb::result::Error& err) {
        err.appendErrorMessage(errorSuffix);
      });
    }
  }

  // optional (5 - First) argument defines prefix for target
  std::string_view prefix = "";
  if (5 - First < argc) {
    res = ElementTraits::evaluateArg(prefix, tmpValue, funcName, args,
                                     5 - First, isFilter, ctx);

    if (res.fail()) {
      return res.withError([&](arangodb::result::Error& err) {
        err.appendErrorMessage(errorSuffix);
      });
    }
  }

  opts.term.assign(irs::ViewCast<irs::byte_type>(target));
  opts.with_transpositions = withTranspositions;
  opts.max_distance = static_cast<irs::byte_type>(maxDistance);
  opts.max_terms = static_cast<size_t>(maxTerms);
  opts.provider = &getParametricDescription;
  opts.prefix.assign(irs::ViewCast<irs::byte_type>(prefix));

  return {};
}

// {<LEVENSHTEIN_MATCH>: '[' <term>, <max_distance> [, <with_transpositions>,
// <prefix> ] ']'}
Result fromFuncPhraseLevenshteinMatch(
    char const* funcName, size_t const funcArgumentPosition,
    char const* subFuncName, irs::by_phrase* filter,
    FilterContext const& filterCtx, const VPackSlice& array, size_t firstOffset,
    irs::analysis::analyzer* /*analyzer*/ = nullptr) {
  if (!array.isArray()) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("'", funcName, "' AQL function: '", subFuncName,
                         "' arguments must be in an array at position '",
                         funcArgumentPosition + 1, "'")};
  }

  VPackSlice targetValue;
  irs::by_edit_distance_options opts;
  auto res = getLevenshteinArguments<1>(
      subFuncName, filter != nullptr, filterCtx, array, nullptr, targetValue,
      opts, getSubFuncErrorSuffix(funcName, funcArgumentPosition));
  if (res.fail()) {
    return res;
  }

  if (filter) {
    auto* phrase = filter->mutable_options();

    auto const& ctx = filterCtx.query;

    if (0 != opts.max_terms) {
      TRI_ASSERT(ctx.index);

      struct top_term_visitor final : irs::filter_visitor {
        explicit top_term_visitor(size_t size) : collector(size) {}

        virtual void prepare(const irs::SubReader& segment,
                             const irs::term_reader& field,
                             const irs::seek_term_iterator& terms) override {
          collector.prepare(segment, field, terms);
        }

        virtual void visit(irs::score_t boost) override {
          collector.visit(boost);
        }

        irs::top_terms_collector<irs::top_term<irs::score_t>> collector;
      } collector(opts.max_terms);

      irs::visit(*ctx.index, filter->field(),
                 irs::by_edit_distance::visitor(opts), collector);

      auto& terms = phrase->push_back<irs::by_terms_options>(firstOffset).terms;
      collector.collector.visit(
          [&terms](const irs::top_term<irs::score_t>& term) {
            terms.emplace(term.term, term.key);
          });
    } else {
      phrase->push_back<irs::by_edit_distance_filter_options>(std::move(opts),
                                                              firstOffset);
    }
  }
  return {};
}

// {<TERMS>: '[' <term0> [, <term1>, ...] ']'}
template<typename ElementType, typename ElementTraits = ArgsTraits<ElementType>>
Result fromFuncPhraseTerms(char const* funcName, size_t funcArgumentPosition,
                           char const* subFuncName, irs::by_phrase* filter,
                           FilterContext const& filterCtx,
                           ElementType const& array, size_t firstOffset,
                           irs::analysis::analyzer* analyzer = nullptr) {
  if (!array.isArray()) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("'", funcName, "' AQL function: '", subFuncName,
                         "' arguments must be in an array at position '",
                         funcArgumentPosition + 1, "'")};
  }

  if (!ElementTraits::isDeterministic(array)) {
    return error::nondeterministicArgs(subFuncName)
        .withError([&](arangodb::result::Error& err) {
          err.appendErrorMessage(
              getSubFuncErrorSuffix(funcName, funcArgumentPosition));
        });
  }

  auto const argc = ElementTraits::numMembers(array);
  if (0 == argc) {
    return error::invalidArgsCount<error::OpenRange<false, 1>>(subFuncName)
        .withError([&](arangodb::result::Error& err) {
          err.appendErrorMessage(
              getSubFuncErrorSuffix(funcName, funcArgumentPosition));
        });
  }

  irs::by_terms_options::search_terms terms;
  typename ElementTraits::ValueType termValue;
  std::string_view term;
  for (size_t i = 0; i < argc; ++i) {
    auto res =
        ElementTraits::evaluateArg(term, termValue, subFuncName, array, i,
                                   filter != nullptr, filterCtx.query);

    if (res.fail()) {
      return res.withError([&](arangodb::result::Error& err) {
        err.appendErrorMessage(
            getSubFuncErrorSuffix(funcName, funcArgumentPosition));
      });
    }
    if (analyzer != nullptr) {
      // reset analyzer
      analyzer->reset(term);
      // get token attribute
      irs::term_attribute const* token =
          irs::get<irs::term_attribute>(*analyzer);
      TRI_ASSERT(token);
      // add tokens
      while (analyzer->next()) {
        terms.emplace(token->value);
      }
    } else {
      terms.emplace(irs::ViewCast<irs::byte_type>(term));
    }
  }
  if (filter) {
    auto& opts = filter->mutable_options()->push_back<irs::by_terms_options>(
        firstOffset);
    opts.terms = std::move(terms);
  }
  return {};
}

template<size_t First, typename ElementType,
         typename ElementTraits = ArgsTraits<ElementType>>
Result getInRangeArguments(char const* funcName, bool isFilter,
                           FilterContext const& filterCtx,
                           ElementType const& args, aql::AstNode const** field,
                           typename ElementTraits::ValueType& min,
                           bool& minInclude,
                           typename ElementTraits::ValueType& max,
                           bool& maxInclude, bool& ret,
                           std::string const& errorSuffix = {}) {
  if (!ElementTraits::isDeterministic(args)) {
    return error::nondeterministicArgs(funcName).withError(
        [&](arangodb::result::Error& err) {
          err.appendErrorMessage(errorSuffix);
        });
  }
  auto const argc = ElementTraits::numMembers(args);

  if (5 - First != argc) {
    return error::invalidArgsCount<error::ExactValue<5 - First>>(funcName)
        .withError([&](arangodb::result::Error& err) {
          err.appendErrorMessage(errorSuffix);
        });
  }

  auto const& ctx = filterCtx.query;

  if constexpr (0 == First) {
    TRI_ASSERT(field);
    // (0 - First) argument defines a field
    *field = checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref,
                                  !ctx.isSearchQuery);

    if (!*field) {
      return error::invalidAttribute(funcName, 1);
    }
    TRI_ASSERT((*field)->isDeterministic());
  }

  // (3 - First) argument defines inclusion of lower boundary
  typename ElementTraits::ValueType includeValue;
  auto res = ElementTraits::evaluateArg(minInclude, includeValue, funcName,
                                        args, 3 - First, isFilter, ctx);
  if (res.fail()) {
    return res.withError([&](arangodb::result::Error& err) {
      err.appendErrorMessage(errorSuffix);
    });
  }

  // (4 - First) argument defines inclusion of upper boundary
  res = ElementTraits::evaluateArg(maxInclude, includeValue, funcName, args,
                                   4 - First, isFilter, ctx);
  if (res.fail()) {
    return res.withError([&](arangodb::result::Error& err) {
      err.appendErrorMessage(errorSuffix);
    });
  }

  // (1 - First) argument defines a lower boundary
  {
    auto res = ElementTraits::getMemberValue(args, 1 - First, funcName, min,
                                             isFilter, ctx, ret);
    if (res.fail()) {
      return res.withError([&](arangodb::result::Error& err) {
        err.appendErrorMessage(errorSuffix);
      });
    }
  }
  // (2 - First) argument defines an upper boundary
  {
    auto res = ElementTraits::getMemberValue(args, 2 - First, funcName, max,
                                             isFilter, ctx, ret);
    if (res.fail()) {
      return res.withError([&](arangodb::result::Error& err) {
        err.appendErrorMessage(errorSuffix);
      });
    }
  }

  if (ret) {
    return {};
  }

  if (ElementTraits::scopedType(min) != ElementTraits::scopedType(max)) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Failed to build range query, lower boundary "
                         "mismatches upper boundary. ",
                         errorSuffix)};
  }
  return {};
}

// {<IN_RANGE>: '[' <term-low>, <term-high>, <include-low>, <include-high> ']'}
Result fromFuncPhraseInRange(char const* funcName,
                             size_t const funcArgumentPosition,
                             char const* subFuncName, irs::by_phrase* filter,
                             FilterContext const& filterCtx,
                             const VPackSlice& array, size_t firstOffset,
                             irs::analysis::analyzer* /*analyzer*/ = nullptr) {
  if (!array.isArray()) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("'", funcName, "' AQL function: '", subFuncName,
                         "' arguments must be in an array at position '",
                         funcArgumentPosition + 1, "'")};
  }

  std::string const errorSuffix =
      getSubFuncErrorSuffix(funcName, funcArgumentPosition);

  VPackSlice min, max;
  auto minInclude = false;
  auto maxInclude = false;
  auto ret = false;
  auto res = getInRangeArguments<1>(subFuncName, filter != nullptr, filterCtx,
                                    array, nullptr, min, minInclude, max,
                                    maxInclude, ret, errorSuffix);
  if (res.fail() || ret) {
    return res;
  }

  if (!min.isString()) {
    return error::typeMismatch(subFuncName, 1, SCOPED_VALUE_TYPE_STRING,
                               ArgsTraits<VPackSlice>::scopedType(min))
        .withError([&](arangodb::result::Error& err) {
          err.appendErrorMessage(errorSuffix);
        });
  }
  auto const minStrValue = min.stringView();

  if (!max.isString()) {
    return error::typeMismatch(subFuncName, 2, SCOPED_VALUE_TYPE_STRING,
                               ArgsTraits<VPackSlice>::scopedType(max))
        .withError([&](arangodb::result::Error& err) {
          err.appendErrorMessage(errorSuffix);
        });
  }
  auto const maxStrValue = max.stringView();

  if (filter) {
    auto& opts = filter->mutable_options()->push_back<irs::by_range_options>(
        firstOffset);
    opts.range.min.assign(irs::ViewCast<irs::byte_type>(minStrValue));
    opts.range.min_type =
        minInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
    opts.range.max.assign(irs::ViewCast<irs::byte_type>(maxStrValue));
    opts.range.max_type =
        maxInclude ? irs::BoundType::INCLUSIVE : irs::BoundType::EXCLUSIVE;
    opts.scored_terms_limit = FilterConstants::DefaultScoringTermsLimit;
  }
  return {};
}

frozen::map<std::string_view, ConversionPhraseHandler,
            6> constexpr kFCallSystemConversionPhraseHandlers{
    {"TERM", fromFuncPhraseTerm},
    {"STARTS_WITH", fromFuncPhraseStartsWith},
    {"WILDCARD", fromFuncPhraseLike},  // 'LIKE' is a key word
    {"LEVENSHTEIN_MATCH", fromFuncPhraseLevenshteinMatch},
    {TERMS_FUNC, fromFuncPhraseTerms<VPackSlice>},
    {"IN_RANGE", fromFuncPhraseInRange}};

Result processPhraseArgObjectType(char const* funcName,
                                  size_t const funcArgumentPosition,
                                  irs::by_phrase* filter,
                                  FilterContext const& filterCtx,
                                  VPackSlice object, size_t firstOffset,
                                  irs::analysis::analyzer* analyzer = nullptr) {
  TRI_ASSERT(object.isObject());
  VPackObjectIterator itr(object);
  if (ADB_LIKELY(itr.valid())) {
    auto key = itr.key();
    auto value = itr.value();
    if (!key.isString()) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("'", funcName,
                           "' AQL function: Unexpected object key type '",
                           key.typeName(), "' at position '",
                           funcArgumentPosition + 1, "'")};
    }
    std::string name{key.stringView()};
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
    auto const entry = kFCallSystemConversionPhraseHandlers.find(name);
    if (kFCallSystemConversionPhraseHandlers.cend() == entry) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("'", funcName, "' AQL function: Unknown '",
                           key.stringView(), "' at position '",
                           funcArgumentPosition + 1, "'")};
    }
    return entry->second(funcName, funcArgumentPosition, entry->first.data(),
                         filter, filterCtx, value, firstOffset, analyzer);
  } else {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("'", funcName,
                         "' AQL function: empty object at position '",
                         funcArgumentPosition + 1, "'")};
  }
}

template<typename ElementType, typename ElementTraits = ArgsTraits<ElementType>>
Result processPhraseArgs(char const* funcName, irs::by_phrase* phrase,
                         FilterContext const& filterCtx,
                         ElementType const& valueArgs, size_t valueArgsBegin,
                         size_t valueArgsEnd, irs::analysis::analyzer* analyzer,
                         size_t offset, bool allowDefaultOffset,
                         bool isInArray) {
  std::string_view value;
  bool expectingOffset = false;
  for (size_t idx = valueArgsBegin; idx < valueArgsEnd; ++idx) {
    typename ElementTraits::ValueType valueArg;
    {
      bool skippedEvaluation{false};
      auto res = ElementTraits::getMemberValue(
          valueArgs, idx, funcName, valueArg, phrase != nullptr,
          filterCtx.query, skippedEvaluation);
      if (res.fail()) return res;
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
          continue;  // just skip empty arrays. This is not error anymore as
                     // this case may arise while working with autocomplete
        }
        // array arg is processed with possible default 0 offsets - to be easily
        // compatible with TOKENS function
        if (!isInArray) {
          auto subRes = processPhraseArgs(
              funcName, phrase, filterCtx, ElementTraits::valueSlice(valueArg),
              0, valueSize, analyzer, offset, true, true);
          if (subRes.fail()) {
            return subRes;
          }
          expectingOffset = true;
          offset = 0;
          continue;
        } else {
          auto res = fromFuncPhraseTerms(
              funcName, idx, TERMS_FUNC, phrase, filterCtx,
              ElementTraits::valueSlice(valueArg), offset, analyzer);
          if (res.fail()) {
            return res;
          }
          expectingOffset = true;
          offset = 0;
          continue;
        }
      }
    } else if (valueArg.isObject()) {
      auto res = processPhraseArgObjectType(funcName, idx, phrase, filterCtx,
                                            ElementTraits::valueSlice(valueArg),
                                            offset);
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
      continue;  // got offset let`s go search for value
    } else if ((!valueArg.isString() ||
                !ElementTraits::getValueString(
                    valueArg, value)) ||  // value is not a string at all
               (expectingOffset &&
                !allowDefaultOffset)) {  // offset is expected mandatory but got
                                         // value
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
          absl::StrCat("'", funcName,
                       "' AQL function: Unable to parse argument at position ",
                       idx, expectedValue)};
    }

    if (phrase) {
      TRI_ASSERT(analyzer);
      appendTerms(*phrase, value, *analyzer, offset);
    }
    offset = 0;
    expectingOffset = true;
  }

  if (!expectingOffset) {  // that means last arg is numeric - this is error as
                           // no term to apply offset to
    return {
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("'", funcName,
                     "' AQL function : Unable to parse argument at position ",
                     valueArgsEnd - 1, "as a value")};
  }
  return {};
}

// note: <value> could be either string ether array of strings with offsets
// inbetween . Inside array 0 offset could be omitted e.g. [term1, term2, 2,
// term3] is equal to: [term1, 0, term2, 2, term3] PHRASE(<attribute>, <value>
// [, <offset>, <value>, ...] [, <analyzer>]) PHRASE(<attribute>, '[' <value> [,
// <offset>, <value>, ...] ']' [,<analyzer>])
Result fromFuncPhrase(char const* funcName, irs::boolean_filter* filter,
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

  auto const& ctx = filterCtx.query;

  // 1st argument defines a field

  auto const* fieldArg = checkAttributeAccess(args.getMemberUnchecked(0),
                                              *ctx.ref, !ctx.isSearchQuery);

  if (!fieldArg) {
    return error::invalidAttribute(funcName, 1);
  }

  // ...........................................................................
  // last odd argument defines an analyzer
  // ...........................................................................

  auto analyzerPool = makeEmptyAnalyzer();

  if (0 != (argc & 1)) {  // override analyzer
    --argc;

    auto rv =
        extractAnalyzerFromArg(analyzerPool, funcName, filter, args, argc, ctx);

    if (rv.fail()) {
      return rv;
    }

    if (!filter && !analyzerPool) {
      // use default context analyzer for the optimization stage
      // if actual analyzer could not be evaluated for now
      analyzerPool = filterCtx.contextAnalyzer;
    }

    if (!analyzerPool) {
      TRI_ASSERT(false);
      return {TRI_ERROR_BAD_PARAMETER};
    }
  }

  // 2nd argument and later defines a values

  auto* valueArgs = &args;
  size_t valueArgsBegin = 1;
  size_t valueArgsEnd = argc;

  irs::by_phrase* phrase = nullptr;
  AnalyzerPool::CacheType::ptr analyzer;

  std::string name{ctx.namePrefix};
  if (!nameFromAttributeAccess(name, *fieldArg, ctx, filter != nullptr)) {
    return error::failedToGenerateName(funcName, 1);
  }
  // prepare filter if execution phase
  if (filter) {
    // now get the actual analyzer for the known field name if it is not
    // overridden
    if (!analyzerPool._pool) {
      analyzerPool = filterCtx.fieldAnalyzer(name, ctx.ctx);
      if (!analyzerPool) {
        return {TRI_ERROR_INTERNAL, "Malformed search/filter context"};
      }
    }

    analyzer = analyzerPool->get();
    if (!analyzer) {
      return {TRI_ERROR_INTERNAL,
              absl::StrCat("'", funcName,
                           "' AQL function: Unable to instantiate analyzer '",
                           analyzerPool->name(), "'")};
    }

    kludge::mangleField(name, ctx.isOldMangling, analyzerPool);

    phrase = &append<irs::by_phrase>(*filter, filterCtx);
    *phrase->mutable_field() = std::move(name);
    phrase->boost(filterCtx.boost);
  }
  // on top level we require explicit offsets - to be backward compatible and be
  // able to distinguish last argument as analyzer or value Also we allow
  // recursion inside array to support older syntax (one array arg) and add
  // ability to pass several arrays as args
  return processPhraseArgs(funcName, phrase, filterCtx, *valueArgs,
                           valueArgsBegin, valueArgsEnd, analyzer.get(), 0,
                           false, false);
}

// NGRAM_MATCH (attribute, target, threshold [, analyzer])
// NGRAM_MATCH (attribute, target [, analyzer]) // default threshold is set to
// 0.7
Result fromFuncNgramMatch(char const* funcName, irs::boolean_filter* filter,
                          FilterContext const& filterCtx,
                          aql::AstNode const& args) {
  if (!args.isDeterministic()) {
    return error::nondeterministicArgs(funcName);
  }

  auto const argc = args.numMembers();

  if (argc < 2 || argc > 4) {
    return error::invalidArgsCount<error::Range<2, 4>>(funcName);
  }

  auto const& ctx = filterCtx.query;

  // 1st argument defines a field
  auto const* field = checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref,
                                           !ctx.isSearchQuery);

  if (!field) {
    return error::invalidAttribute(funcName, 1);
  }

  // 2nd argument defines a value
  ScopedAqlValue matchAqlValue;
  std::string_view matchValue;
  {
    auto res =
        evaluateArg(matchValue, matchAqlValue, funcName, args, 1, filter, ctx);
    if (!res.ok()) {
      return res;
    }
  }

  auto threshold = FilterConstants::DefaultNgramMatchThreshold;
  auto analyzerPool = makeEmptyAnalyzer();

  if (argc > 3) {  // 4 args given. 3rd is threshold
    ScopedAqlValue tmpValue;
    auto res = evaluateArg(threshold, tmpValue, funcName, args, 2, filter, ctx);

    if (!res.ok()) {
      return res;
    }
  } else if (argc > 2) {  // 3 args given  -  3rd argument defines a threshold
                          // (if double) or analyzer (if string)
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
      if (SCOPED_VALUE_TYPE_STRING == tmpValue.type()) {  // this is analyzer
        std::string_view analyzerId;
        if (!tmpValue.getString(analyzerId)) {
          return error::failedToParse(funcName, 3, SCOPED_VALUE_TYPE_STRING);
        }
        if (filter || tmpValue.isConstant()) {
          auto analyzerRes =
              getAnalyzerByName(analyzerPool, analyzerId, funcName, ctx);
          if (!analyzerRes.ok()) {
            return analyzerRes;
          }
        }
      } else if (SCOPED_VALUE_TYPE_DOUBLE == tmpValue.type()) {
        if (!tmpValue.getDouble(threshold)) {
          return error::failedToParse(funcName, 3, SCOPED_VALUE_TYPE_DOUBLE);
        }
      } else {
        return {
            TRI_ERROR_BAD_PARAMETER,
            absl::StrCat(
                "'", funcName,
                "' AQL function: argument at position '3' has invalid type '",
                std::string_view{ScopedAqlValue::typeString(tmpValue.type())},
                "' ('",
                std::string_view{
                    ScopedAqlValue::typeString(SCOPED_VALUE_TYPE_DOUBLE)},
                "' or '",
                std::string_view{
                    ScopedAqlValue::typeString(SCOPED_VALUE_TYPE_STRING)},
                "' expected)")};
      }
    }
  }

  if (threshold <= 0 || threshold > 1) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("'", funcName,
                         "' AQL function: threshold must be between 0 and 1")};
  }

  // 4th optional argument defines an analyzer
  if (argc > 3) {
    auto rv =
        extractAnalyzerFromArg(analyzerPool, funcName, filter, args, 3, ctx);

    if (rv.fail()) {
      return rv;
    }

    if (!filter && !analyzerPool) {
      // use default context analyzer for the optimization stage
      // if actual analyzer could not be evaluated for now
      analyzerPool = filterCtx.contextAnalyzer;
    }

    TRI_ASSERT(analyzerPool._pool);
    if (!analyzerPool._pool) {
      return {TRI_ERROR_BAD_PARAMETER};
    }
  }

  std::string name{ctx.namePrefix};
  if (!nameFromAttributeAccess(name, *field, ctx, filter != nullptr)) {
    return error::failedToGenerateName(funcName, 1);
  }

  if (filter) {
    if (!analyzerPool) {
      analyzerPool = filterCtx.fieldAnalyzer(name, ctx.ctx);
      if (!analyzerPool) {
        return {TRI_ERROR_INTERNAL, "Malformed search/filter context"};
      }
    }

    auto analyzer = analyzerPool->get();
    if (!analyzer) {
      return {TRI_ERROR_INTERNAL,
              absl::StrCat("'", funcName,
                           "' AQL function: Unable to instantiate analyzer '",
                           analyzerPool->name(), "'")};
    }

    auto& ngramFilter = append<irs::by_ngram_similarity>(*filter, filterCtx);
    kludge::mangleField(name, ctx.isOldMangling, analyzerPool);
    *ngramFilter.mutable_field() = std::move(name);
    auto* opts = ngramFilter.mutable_options();
    opts->threshold = static_cast<float_t>(threshold);
    ngramFilter.boost(filterCtx.boost);

    analyzer->reset(matchValue);
    irs::term_attribute const* token = irs::get<irs::term_attribute>(*analyzer);
    TRI_ASSERT(token);
    while (analyzer->next()) {
      opts->ngrams.emplace_back(token->value.data(), token->value.size());
    }
  }
  return {};
}

#ifndef USE_ENTERPRISE
Result fromFuncMinHashMatch(char const* funcName, irs::boolean_filter*,
                            FilterContext const&, aql::AstNode const&) {
  TRI_ASSERT(funcName);

  return {TRI_ERROR_NOT_IMPLEMENTED,
          absl::StrCat("Function ", funcName,
                       "' is available in ArangoDB Enterprise Edition only.")};
}
#endif

// STARTS_WITH(<attribute>, [ '[' ] <prefix> [, <prefix>, ... ']' ], [
// <scoring-limit>|<min-match-count> ] [, <scoring-limit> ])
Result fromFuncStartsWith(char const* funcName, irs::boolean_filter* filter,
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

  auto const& ctx = filterCtx.query;

  size_t currentArgNum = 0;

  // 1st argument defines a field
  auto const* field = checkAttributeAccess(
      args.getMemberUnchecked(currentArgNum), *ctx.ref, !ctx.isSearchQuery);

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

  std::vector<std::pair<ScopedAqlValue, std::string_view>> prefixes;
  ScopedAqlValue minMatchCountValue;
  auto minMatchCount = FilterConstants::DefaultStartsWithMinMatchCount;
  bool const isMultiPrefix = prefixesValue.isArray();
  if (isMultiPrefix) {
    auto const size = prefixesValue.size();
    if (size > 0) {
      prefixes.reserve(size);
      for (size_t i = 0; i < size; ++i) {
        prefixes.emplace_back(prefixesValue.at(i), std::string_view{});
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
          minMatchCount, minMatchCountValue, funcName, args, currentArgNum,
          filter != nullptr, ctx);

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
    auto rv = evaluateArg(scoringLimitValue, scoringLimitValueBuf, funcName,
                          args, currentArgNum, filter != nullptr, ctx);

    if (rv.fail()) {
      return rv;
    }

    if (scoringLimitValue < 0) {
      return error::negativeNumber(funcName, currentArgNum + 1);
    }

    scoringLimit = static_cast<size_t>(scoringLimitValue);
  }

  std::string name{ctx.namePrefix};
  if (!nameFromAttributeAccess(name, *field, ctx, filter != nullptr)) {
    return error::failedToGenerateName(funcName, 1);
  }

  if (filter) {
    auto& analyzer = filterCtx.fieldAnalyzer(name, ctx.ctx);
    if (!analyzer) {
      return {TRI_ERROR_INTERNAL, "Malformed search/filter context"};
    }
    kludge::mangleField(name, ctx.isOldMangling, analyzer);

    // Try to optimize us away
    if (!isMultiPrefix && !prefixes.empty() &&
        ctx.filterOptimization != FilterOptimization::NONE &&
        includeStartsWithInLevenshtein(filter, name, prefixes.back().second)) {
      return {};
    }

    if (isMultiPrefix) {
      auto& minMatchFilter = append<irs::Or>(*filter, filterCtx);
      minMatchFilter.min_match_count(static_cast<size_t>(minMatchCount));
      minMatchFilter.boost(filterCtx.boost);
      // become a new root
      filter = &minMatchFilter;
    }

    for (size_t i = 0, size = prefixes.size(); i < size; ++i) {
      auto& prefixFilter = append<irs::by_prefix>(*filter, filterCtx);
      if (!isMultiPrefix) {
        TRI_ASSERT(prefixes.size() == 1);
        prefixFilter.boost(filterCtx.boost);
      }
      if (i + 1 < size) {
        *prefixFilter.mutable_field() = name;
      } else {
        *prefixFilter.mutable_field() = std::move(name);
      }
      auto* opts = prefixFilter.mutable_options();
      opts->scored_terms_limit = scoringLimit;
      opts->term.assign(irs::ViewCast<irs::byte_type>(prefixes[i].second));
    }
  }

  return {};
}

// IN_RANGE(<attribute>, <low>, <high>, <include-low>, <include-high>)
Result fromFuncInRange(char const* funcName, irs::boolean_filter* filter,
                       FilterContext const& filterCtx,
                       aql::AstNode const& args) {
  TRI_ASSERT(funcName);
  aql::AstNode const* field = nullptr;
  ScopedAqlValue min, max;
  auto minInclude = false;
  auto maxInclude = false;
  auto ret = false;
  auto res =
      getInRangeArguments<0>(funcName, filter != nullptr, filterCtx, args,
                             &field, min, minInclude, max, maxInclude, ret);
  if (res.fail() || ret) {
    return res;
  }

  TRI_ASSERT(field);

  res = ::byRange(filter, *field, min, minInclude, max, maxInclude, filterCtx);
  if (res.fail()) {
    return {res.errorNumber(),
            absl::StrCat("error in byRange: ", res.errorMessage())};
  }
  return {};
}

// LIKE(<attribute>, <pattern>)
Result fromFuncLike(char const* funcName, irs::boolean_filter* filter,
                    FilterContext const& filterCtx, aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  if (!args.isDeterministic()) {
    return error::nondeterministicArgs(funcName);
  }

  auto const argc = args.numMembers();

  if (argc != 2) {
    return error::invalidArgsCount<error::ExactValue<2>>(funcName);
  }

  auto const& ctx = filterCtx.query;

  // 1st argument defines a field
  auto const* field = checkAttributeAccess(args.getMemberUnchecked(0), *ctx.ref,
                                           !ctx.isSearchQuery);

  if (!field) {
    return error::invalidAttribute(funcName, 1);
  }

  // 2nd argument defines a matching pattern
  ScopedAqlValue patternValue;
  std::string_view pattern;
  Result res = evaluateArg(pattern, patternValue, funcName, args, 1,
                           filter != nullptr, ctx);

  if (!res.ok()) {
    return res;
  }

  const auto scoringLimit = FilterConstants::DefaultScoringTermsLimit;

  std::string name{ctx.namePrefix};
  if (!nameFromAttributeAccess(name, *field, ctx, filter != nullptr)) {
    return error::failedToGenerateName(funcName, 1);
  }

  if (filter) {
    auto& analyzer = filterCtx.fieldAnalyzer(name, ctx.ctx);
    if (!analyzer) {
      return {TRI_ERROR_INTERNAL, "Malformed search/filter context"};
    }

    auto& wildcardFilter = append<irs::by_wildcard>(*filter, filterCtx);
    kludge::mangleField(name, ctx.isOldMangling, analyzer);
    *wildcardFilter.mutable_field() = std::move(name);
    wildcardFilter.boost(filterCtx.boost);
    auto* opts = wildcardFilter.mutable_options();
    opts->scored_terms_limit = scoringLimit;
    opts->term.assign(irs::ViewCast<irs::byte_type>(pattern));
  }

  return {};
}

// LEVENSHTEIN_MATCH(<attribute>, <target>, <max-distance> [,
// <include-transpositions>, <max-terms>])
Result fromFuncLevenshteinMatch(char const* funcName,
                                irs::boolean_filter* filter,
                                FilterContext const& filterCtx,
                                aql::AstNode const& args) {
  TRI_ASSERT(funcName);

  aql::AstNode const* field = nullptr;
  ScopedAqlValue targetValue;
  irs::by_edit_distance_options opts;
  auto res = getLevenshteinArguments<0>(funcName, filter != nullptr, filterCtx,
                                        args, &field, targetValue, opts);
  if (res.fail()) {
    return res;
  }

  auto const& ctx = filterCtx.query;

  std::string name{ctx.namePrefix};
  if (!nameFromAttributeAccess(name, *field, ctx, filter != nullptr)) {
    return error::failedToGenerateName(funcName, 1);
  }

  if (filter) {
    auto& analyzer = filterCtx.fieldAnalyzer(name, ctx.ctx);
    if (!analyzer) {
      return {TRI_ERROR_INTERNAL, "Malformed search/filter context"};
    }

    auto& levenshtein_filter =
        append<irs::by_edit_distance>(*filter, filterCtx);
    levenshtein_filter.boost(filterCtx.boost);
    kludge::mangleField(name, ctx.isOldMangling, analyzer);
    *levenshtein_filter.mutable_field() = std::move(name);
    *levenshtein_filter.mutable_options() = std::move(opts);
  }

  return {};
}

Result fromFuncGeoContainsIntersect(char const* funcName,
                                    irs::boolean_filter* filter,
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

  auto const& ctx = filterCtx.query;
  auto const* fieldNode = args.getMemberUnchecked(0);
  auto const* shapeNode = args.getMemberUnchecked(1);
  size_t fieldNodeIdx = 1;
  size_t shapeNodeIdx = 2;

  if (!checkAttributeAccess(fieldNode, *ctx.ref, !ctx.isSearchQuery)) {
    if (!checkAttributeAccess(shapeNode, *ctx.ref, !ctx.isSearchQuery)) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("'", funcName,
                           "' AQL function: Unable to find argument denoting "
                           "an attribute identifier")};
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
  if (filter || shapeValue.isConstant()) {
    if (!shapeValue.execute(ctx)) {
      return error::failedToEvaluate(funcName, shapeNodeIdx);
    }
  }

  std::string name{ctx.namePrefix};
  if (!nameFromAttributeAccess(name, *fieldNode, ctx, filter != nullptr)) {
    return error::failedToGenerateName(funcName, fieldNodeIdx);
  }

  if (filter) {
    auto& analyzer = filterCtx.fieldAnalyzer(name, ctx.ctx);
    if (!analyzer) {
      return {TRI_ERROR_INTERNAL, "Malformed search/filter context"};
    }

    auto& geo_filter = append<GeoFilter>(*filter, filterCtx);
    geo_filter.boost(filterCtx.boost);

    auto* options = geo_filter.mutable_options();
    auto r = setupGeoFilter(analyzer, *options);
    if (!r.ok()) {
      return r;
    }

    geo::ShapeContainer shape;
    std::vector<S2LatLng> cache;
    auto res = parseShape<Parsing::GeoJson>(
        shapeValue.slice(), shape, cache,
        options->stored == StoredType::VPackLegacy, options->coding, nullptr);
    if (!res) {  // TODO(MBkkt) better error handling
      return {
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("'", funcName,
                       "' AQL function: failed to parse argument at position '",
                       shapeNodeIdx, "'.")};
    }

    options->shape = std::move(shape);
    options->type = GEO_INTERSECT_FUNC == funcName
                        ? GeoFilterType::INTERSECTS
                        : (1 == shapeNodeIdx ? GeoFilterType::CONTAINS
                                             : GeoFilterType::IS_CONTAINED);

    kludge::mangleField(name, ctx.isOldMangling, analyzer);
    *geo_filter.mutable_field() = std::move(name);
  }

  return {};
}

frozen::map<std::string_view, ConversionHandler,
            0> constexpr kFCallUserConversionHandlers{};

Result fromFCallUser(irs::boolean_filter* filter,
                     FilterContext const& filterCtx, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_FCALL_USER == node.type);

  if (node.numMembers() != 1) {
    return error::malformedNode(node.type);
  }

  auto const* args = getNode(node, 0, aql::NODE_TYPE_ARRAY);

  if (!args) {
    return {TRI_ERROR_BAD_PARAMETER,
            "Unable to parse user function arguments as an array'"};
  }

  std::string_view name;

  if (!parseValue(name, node)) {
    return {TRI_ERROR_BAD_PARAMETER, "Unable to parse user function name"};
  }

  auto const entry = kFCallUserConversionHandlers.find(name);

  if (entry == kFCallUserConversionHandlers.end()) {
    return fromExpression(filter, filterCtx, node);
  }

  if (!args->isDeterministic()) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Unable to handle non-deterministic function '", name,
                         "' arguments")};
  }

  return entry->second(entry->first.data(), filter, filterCtx, *args);
}

frozen::map<std::string_view, ConversionHandler,
            14> constexpr kFCallSystemConversionHandlers{
    // filter functions
    {"PHRASE", fromFuncPhrase},
    {"STARTS_WITH", fromFuncStartsWith},
    {"EXISTS", fromFuncExists},
    {"MIN_MATCH", fromFuncMinMatch},
    {"IN_RANGE", fromFuncInRange},
    {"LIKE", fromFuncLike},
    {"LEVENSHTEIN_MATCH", fromFuncLevenshteinMatch},
    {"NGRAM_MATCH", fromFuncNgramMatch},
    {"MINHASH_MATCH", fromFuncMinHashMatch},
    // geo function
    {GEO_INTERSECT_FUNC, fromFuncGeoContainsIntersect},
    {"GEO_IN_RANGE", fromFuncGeoInRange},
    {"GEO_CONTAINS", fromFuncGeoContainsIntersect},
    // GEO_DISTANCE missing because it doesn't return boolean
    // context functions
    {"BOOST", fromFuncBoost},
    {"ANALYZER", fromFuncAnalyzer},
};

Result fromFCall(irs::boolean_filter* filter, FilterContext const& filterCtx,
                 aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_FCALL == node.type);

  auto const* fn = static_cast<aql::Function*>(node.getData());

  if (!fn || node.numMembers() != 1) {
    return error::malformedNode(node.type);
  }

  if (!isFilter(*fn)) {
    // not a filter function
    return fromExpression(filter, filterCtx, node);
  }

  auto const entry = kFCallSystemConversionHandlers.find(fn->name);

  if (entry == kFCallSystemConversionHandlers.end()) {
    return fromExpression(filter, filterCtx, node);
  }

  auto const* args = getNode(node, 0, aql::NODE_TYPE_ARRAY);

  if (!args) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Unable to parse arguments of system function '",
                         fn->name, "' as an array'")};
  }

  return entry->second(entry->first.data(), filter, filterCtx, *args);
}

Result fromFilter(irs::boolean_filter* filter, FilterContext const& filterCtx,
                  aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_FILTER == node.type);

  if (node.numMembers() != 1) {
    auto rv = error::malformedNode(node.type);
    return rv.reset(
        rv.errorNumber(),
        absl::StrCat("wrong number of parameters: ", rv.errorMessage()));
  }

  auto const* member = node.getMemberUnchecked(0);

  if (member) {
    return makeFilter(filter, filterCtx, *member);
  } else {
    return {TRI_ERROR_INTERNAL,
            "could not get node member"};  // wrong number of members
  }
}

Result fromExpansion(irs::boolean_filter* filter,
                     FilterContext const& filterCtx, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_EXPANSION == node.type);

#ifdef USE_ENTERPRISE
  if (node.hasFlag(aql::FLAG_BOOLEAN_EXPANSION)) {
    // fromNegation handles negative cases
    return fromBooleanExpansion(filter, filterCtx, node);
  }
#endif

  return fromExpression(filter, filterCtx, node);
}

}  // namespace

namespace arangodb::iresearch {

bool allColumnAcceptor(std::string_view, std::string_view) noexcept {
  return true;
}

#ifndef USE_ENTERPRISE
irs::ColumnAcceptor makeColumnAcceptor(bool) noexcept {
  return allColumnAcceptor;
}
#endif

Result fromExpression(irs::boolean_filter* filter,
                      FilterContext const& filterCtx,
                      aql::AstNode const& node) {
  auto const& ctx = filterCtx.query;

  // non-deterministic condition or self-referenced variable
  if (!node.isDeterministic() || findReference(node, *ctx.ref)) {
    // not supported by IResearch, but could be handled by ArangoDB
    if (ctx.isSearchQuery) {
      if (filter) {
        auto& exprFilter = append<ByExpression>(*filter, filterCtx);
        exprFilter.init(ctx, const_cast<aql::AstNode&>(node));
        exprFilter.boost(filterCtx.boost);
        return {};
      }
    } else {
      return {TRI_ERROR_NOT_IMPLEMENTED,
              "ByExpression filter is supported for SEARCH only"};
    }
  }

  if (!filter) {
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
    append<irs::all>(*filter, filterCtx).boost(filterCtx.boost);
  } else {
    append<irs::empty>(*filter, filterCtx);
  }

  return {};
}

Result makeFilter(irs::boolean_filter* filter, FilterContext const& filterCtx,
                  aql::AstNode const& node) {
  switch (node.type) {
    case aql::NODE_TYPE_FILTER:  // FILTER
      return fromFilter(filter, filterCtx, node);
    case aql::NODE_TYPE_VARIABLE:  // variable
      return fromExpression(filter, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_UNARY_NOT:  // unary minus
      return fromNegation(filter, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_AND:  // logical and
      return fromGroup<irs::And>(filter, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_OR:  // logical or
      return fromGroup<irs::Or>(filter, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_EQ:  // compare ==
    case aql::NODE_TYPE_OPERATOR_BINARY_NE:  // compare !=
      return fromBinaryEq(filter, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_LT:  // compare <
    case aql::NODE_TYPE_OPERATOR_BINARY_LE:  // compare <=
    case aql::NODE_TYPE_OPERATOR_BINARY_GT:  // compare >
    case aql::NODE_TYPE_OPERATOR_BINARY_GE:  // compare >=
      return fromInterval(filter, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_IN:   // compare in
    case aql::NODE_TYPE_OPERATOR_BINARY_NIN:  // compare not in
      return fromIn(filter, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_TERNARY:  // ternary
    case aql::NODE_TYPE_ATTRIBUTE_ACCESS:  // attribute access
    case aql::NODE_TYPE_VALUE:             // value
    case aql::NODE_TYPE_ARRAY:             // array
    case aql::NODE_TYPE_OBJECT:            // object
    case aql::NODE_TYPE_REFERENCE:         // reference
    case aql::NODE_TYPE_PARAMETER:         // bind parameter
      return fromExpression(filter, filterCtx, node);
    case aql::NODE_TYPE_FCALL:  // function call
      return fromFCall(filter, filterCtx, node);
    case aql::NODE_TYPE_FCALL_USER:  // user function call
      return fromFCallUser(filter, filterCtx, node);
    case aql::NODE_TYPE_RANGE:  // range
      return fromRange(filter, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_NARY_AND:  // n-ary and
      return fromGroup<irs::And>(filter, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_NARY_OR:  // n-ary or
      return fromGroup<irs::Or>(filter, filterCtx, node);
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:   // compare ARRAY in
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:  // compare ARRAY not in
    // for iresearch filters IN and EQ queries will be actually the same
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:  // compare ARRAY ==
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:  // compare ARRAY !=
      return fromArrayComparison<ByTermSubFilterFactory>(filter, filterCtx,
                                                         node);
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:  // compare ARRAY <
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:  // compare ARRAY <=
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:  // compare ARRAY >
    case aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:  // compare ARRAY >=
      return fromArrayComparison<ByRangeSubFilterFactory>(filter, filterCtx,
                                                          node);
    case aql::NODE_TYPE_EXPANSION:  // [?|* ...]
      return fromExpansion(filter, filterCtx, node);
    default:
      return fromExpression(filter, filterCtx, node);
  }
}

Result FilterFactory::filter(irs::boolean_filter* filter,
                             FilterContext const& filterCtx,
                             aql::AstNode const& node) {
  if (node.willUseV8()) {
    return {TRI_ERROR_NOT_IMPLEMENTED,
            "using V8 dependent function is not allowed in SEARCH statement"};
  }

  const auto res = makeFilter(filter, filterCtx, node);

  if (res.fail()) {
    if (filter) {
      LOG_TOPIC("dfa15", WARN, TOPIC) << res.errorMessage();
    } else {
      LOG_TOPIC("dfa16", TRACE, TOPIC) << res.errorMessage();
    }
  }

  return res;
}

}  // namespace arangodb::iresearch
