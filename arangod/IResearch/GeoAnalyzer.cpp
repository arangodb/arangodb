////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "IResearch/GeoAnalyzer.h"

#include <string>

#include <s2/s2point_region.h>
#include <s2/s2latlng.h>

#include <velocypack/Builder.h>

#include "analysis/analyzers.hpp"

#include "Geo/GeoParams.h"
#include "IResearch/Geo.h"
#include "IResearch/GeoFilter.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "VPackDeserializer/deserializer.h"
#include "Basics/DownCast.h"
#include "Basics/Exceptions.h"
#include "Inspection/VPack.h"

#include <absl/strings/str_cat.h>
#include <frozen/map.h>

namespace arangodb::iresearch {
namespace {

constexpr auto kType2Str =
    frozen::make_map<GeoJsonAnalyzerBase::Type, std::string_view>({
        {GeoJsonAnalyzerBase::Type::SHAPE, "shape"},
        {GeoJsonAnalyzerBase::Type::CENTROID, "centroid"},
        {GeoJsonAnalyzerBase::Type::POINT, "point"},
    });
constexpr auto kStr2Type =
    frozen::make_map<std::string_view, GeoJsonAnalyzerBase::Type>({
        {"shape", GeoJsonAnalyzerBase::Type::SHAPE},
        {"centroid", GeoJsonAnalyzerBase::Type::CENTROID},
        {"point", GeoJsonAnalyzerBase::Type::POINT},
    });

constexpr std::string_view kTypeParam = "type";
constexpr std::string_view kOptionsParam = "options";
constexpr std::string_view kMaxCellsParam = "maxCells";
constexpr std::string_view kMinLevelParam = "minLevel";
constexpr std::string_view kMaxLevelParam = "maxLevel";
constexpr std::string_view kLevelModParam = "modLevel";
constexpr std::string_view kOptimizeForSpaceParam = "optimizeForSpace";
constexpr std::string_view kLatitudeParam = "latitude";
constexpr std::string_view kLongitudeParam = "longitude";
constexpr std::string_view kLegacyParam = "legacy";
constexpr std::string_view kCompressionParam = "compression";

Result getBool(velocypack::Slice object, std::string_view name, bool& output) {
  auto const value = object.get(name);
  if (value.isNone()) {
    return {};
  }
  if (!value.isBool()) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("'", name, "' should be bool.")};
  }
  output = value.getBool();
  return {};
}

Result fromVelocyPack(velocypack::Slice object, GeoOptions& options) {
  if (!object.isObject()) {
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("Failed to parse '", kOptionsParam,
                         "', expected Object.")};
  }
  auto get = [&]<typename T>(auto name, auto min, auto max,
                             T& output) -> Result {
    auto const value = object.get(name);
    if (value.isNone()) {
      return {};
    }
    auto error = [&] {
      return Result{
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("'", name, "' out of bounds: [", min, "..", max, "].")};
    };
    if (!value.template isNumber<T>()) {
      return error();
    }
    auto const v = value.template getNumber<T>();
    if (v < min || max < v) {
      return error();
    }
    output = v;
    return {};
  };
  auto r = get(kMaxCellsParam, GeoOptions::kMinCells, GeoOptions::kMaxCells,
               options.maxCells);
  if (!r.ok()) {
    return r;
  }
  r = get(kMinLevelParam, GeoOptions::kMinLevel, GeoOptions::kMaxLevel,
          options.minLevel);
  if (!r.ok()) {
    return r;
  }
  r = get(kMaxLevelParam, GeoOptions::kMinLevel, GeoOptions::kMaxLevel,
          options.maxLevel);
  if (!r.ok()) {
    return r;
  }
  r = get(kLevelModParam, GeoOptions::kMinLevelMod, GeoOptions::kMaxLevelMod,
          options.levelMod);
  if (!r.ok()) {
    return r;
  }
  if (options.minLevel > options.maxLevel) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("'", kMinLevelParam, "' should be less than or equal to '",
                     kMaxLevelParam, "'.")};
  }
  return getBool(object, kOptimizeForSpaceParam, options.optimizeForSpace);
}

Result fromVelocyPackBase(velocypack::Slice object,
                          GeoJsonAnalyzerBase::OptionsBase& options) {
  TRI_ASSERT(object.isObject());
  auto value = object.get(kOptionsParam);
  if (!value.isNone()) {
    auto r = fromVelocyPack(value, options.options);
    if (!r.ok()) {
      return r;
    }
  }
  value = object.get(kTypeParam);
  if (!value.isNone()) {
    auto error = [&] {
      return Result{TRI_ERROR_BAD_PARAMETER,
                    absl::StrCat("'", kTypeParam,
                                 "' can be 'shape', 'centroid', 'point'.")};
    };
    if (!value.isString()) {
      return error();
    }
    auto it = kStr2Type.find(value.stringView());
    if (it == kStr2Type.end()) {
      return error();
    }
    options.type = it->second;
  }
  return {};
}

Result fromVelocyPack(velocypack::Slice object,
                      GeoPointAnalyzer::Options& options) {
  TRI_ASSERT(object.isObject());
  if (auto value = object.get(kOptionsParam); !value.isNone()) {
    auto r = fromVelocyPack(value, options.options);
    if (!r.ok()) {
      return r;
    }
  }
  auto get = [&](auto name, auto& output) -> Result {
    TRI_ASSERT(output.empty());
    auto const value = object.get(name);
    if (value.isNone()) {
      return {};
    }
    auto error = [&] {
      return Result{TRI_ERROR_BAD_PARAMETER,
                    absl::StrCat("'", name, "' should be array of strings")};
    };
    if (!value.isArray()) {
      return error();
    }
    velocypack::ArrayIterator it{value};
    output.reserve(it.size());
    for (; it.valid(); it.next()) {
      auto sub = *it;
      if (!sub.isString()) {
        output = {};
        return error();
      }
      output.emplace_back(sub.stringView());
    }
    return {};
  };
  auto r = get(kLatitudeParam, options.latitude);
  if (!r.ok()) {
    return r;
  }
  r = get(kLongitudeParam, options.longitude);
  if (!r.ok()) {
    return r;
  }
  if (options.latitude.empty() != options.longitude.empty()) {
    options.latitude = {};
    options.longitude = {};
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("'", kLatitudeParam, "' and '", kLongitudeParam,
                         "' should be both empty or non-empty.")};
  }
  return {};
}

Result fromVelocyPack(velocypack::Slice object,
                      GeoVPackAnalyzer::Options& options) {
  auto r = fromVelocyPackBase(object, options);
  if (!r.ok()) {
    return r;
  }
  return getBool(object, kLegacyParam, options.legacy);
}

Result fromVelocyPack(velocypack::Slice object,
                      GeoS2Analyzer::Options& options) {
  auto r = fromVelocyPackBase(object, options);
  if (r.ok()) {
    return r;
  }
  auto value = object.get(kCompressionParam);
  if (!value.isNone()) {
    uint8_t v = 0;
    if (!value.isNumber<uint8_t>() ||
        (v = value.getNumber<uint8_t>()) <
            static_cast<uint8_t>(s2coding::CodingHint::COMPACT)) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat("'", kCompressionParam, "' should be 0 or 1.")};
    }
    options.hint = static_cast<s2coding::CodingHint>(v);
  }
  return r;
}

template<typename Analyzer>
bool fromVelocyPackAnalyzer(std::string_view args,
                            typename Analyzer::Options& options) {
  auto const object = slice(args);
  Result r;
  if (!object.isObject()) {
    r = {TRI_ERROR_BAD_PARAMETER,
         "Cannot parse geo analyzer definition not from Object."};
  } else {
    r = fromVelocyPack(object, options);
  }
  if (!r.ok()) {
    LOG_TOPIC("4349c", WARN, TOPIC)
        << "Failed to deserialize options from JSON while constructing '"
        << irs::type<Analyzer>::name() << "' analyzer, error: '"
        << r.errorMessage() << "'";
    return false;
  }
  return true;
}

template<typename Analyzer>
bool normalizeImpl(std::string_view args, std::string& out) {
  typename Analyzer::Options options;
  if (!fromVelocyPackAnalyzer<Analyzer>(args, options)) {
    return false;
  }
  velocypack::Builder root;
  toVelocyPack(root, options);
  out.resize(root.slice().byteSize());
  std::memcpy(&out[0], root.slice().begin(), out.size());
  return true;
}

template<typename Analyzer>
irs::analysis::analyzer::ptr makeImpl(std::string_view args) {
  typename Analyzer::Options options;
  if (!fromVelocyPackAnalyzer<Analyzer>(args, options)) {
    return {};
  }
  return std::make_unique<Analyzer>(options);
}

void toVelocyPack(velocypack::Builder& builder, GeoOptions const& options) {
  velocypack::ObjectBuilder scope{&builder, kOptionsParam};
  builder.add(kMaxCellsParam, velocypack::Value{options.maxCells});
  builder.add(kMinLevelParam, velocypack::Value{options.minLevel});
  builder.add(kMaxLevelParam, velocypack::Value{options.maxLevel});
}

void toVelocyPackBase(velocypack::Builder& builder,
                      GeoJsonAnalyzerBase::OptionsBase const& options) {
  TRI_ASSERT(builder.isOpenObject());
  toVelocyPack(builder, options.options);
  builder.add(kTypeParam, velocypack::Value{kType2Str.at(options.type)});
}

}  // namespace

void toVelocyPack(velocypack::Builder& builder,
                  GeoPointAnalyzer::Options const& options) {
  auto addArray = [&](auto name, auto const& values) {
    velocypack::ArrayBuilder scope{&builder, name};
    for (auto const& value : values) {
      builder.add(velocypack::Value{value});
    }
  };
  velocypack::ObjectBuilder scope{&builder};
  toVelocyPack(builder, options.options);
  addArray(kLatitudeParam, options.latitude);
  addArray(kLongitudeParam, options.longitude);
}

void toVelocyPack(velocypack::Builder& builder,
                  GeoVPackAnalyzer::Options const& options) {
  velocypack::ObjectBuilder scope{&builder};
  toVelocyPackBase(builder, options);
  builder.add(kLegacyParam, velocypack::Value{options.legacy});
}

void toVelocyPack(velocypack::Builder& builder,
                  GeoS2Analyzer::Options const& options) {
  velocypack::ObjectBuilder scope{&builder};
  toVelocyPackBase(builder, options);
  static_assert(static_cast<uint8_t>(s2coding::CodingHint::FAST) == 0);
  static_assert(static_cast<uint8_t>(s2coding::CodingHint::COMPACT) == 1);
  builder.add(kCompressionParam,
              velocypack::Value{static_cast<uint8_t>(options.hint)});
}

GeoAnalyzer::GeoAnalyzer(irs::type_info const& type,
                         S2RegionTermIndexer::Options const& options)
    : irs::analysis::analyzer{type}, _indexer{options}, _coverer{options} {}

bool GeoAnalyzer::next() noexcept {
  if (_begin >= _end) {
    return false;
  }
  auto& value = *_begin++;
  std::get<irs::term_attribute>(_attrs).value = {
      reinterpret_cast<irs::byte_type const*>(value.data()), value.size()};
  return true;
}

void GeoAnalyzer::reset(std::vector<std::string>&& terms) noexcept {
  _terms = std::move(terms);
  _begin = _terms.data();
  _end = _begin + _terms.size();
}

GeoJsonAnalyzerBase::GeoJsonAnalyzerBase(
    irs::type_info const& type, GeoJsonAnalyzerBase::OptionsBase const& options)
    : GeoAnalyzer{type,
                  S2Options(options.options, options.type != Type::SHAPE)},
      _type{options.type} {}

bool GeoJsonAnalyzerBase::resetImpl(std::string_view value, bool legacy,
                                    bool legacyCovering) {
  auto const data = slice(value);
  if (_type != Type::POINT) {
    if (!parseShape<Parsing::GeoJson>(data, _shape, _cache, legacy)) {
      return false;
    }
  } else if (!parseShape<Parsing::OnlyPoint>(data, _shape, _cache, legacy)) {
    return false;
  }

  std::vector<std::string> geoTerms;
  auto const type = _shape.type();
  if (_type == Type::CENTROID || type == geo::ShapeContainer::Type::S2_POINT) {
    geoTerms = _indexer.GetIndexTerms(_shape.centroid(), {});
  } else if (legacyCovering) {
    geoTerms = _indexer.GetIndexTerms(*_shape.region(), {});
  } else {
    auto cellIds = _shape.covering(_coverer);
    // TODO(MBkkt) S2_MULTIPOINT can be handled via GetIndexTerms(point)
    if (type == geo::ShapeContainer::Type::S2_MULTIPOINT ||
        type == geo::ShapeContainer::Type::S2_MULTIPOLYLINE) {
      _coverer.CanonicalizeCovering(&cellIds);
    } else {
      TRI_ASSERT(_coverer.IsCanonical(cellIds));
    }
    geoTerms = _indexer.GetIndexTermsForCanonicalCovering(
        S2CellUnion::FromVerbatim(std::move(cellIds)), {});
  }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  std::vector<std::string_view> terms{geoTerms.begin(), geoTerms.end()};
  std::sort(terms.begin(), terms.end());
  TRI_ASSERT(std::unique(terms.begin(), terms.end()) == terms.end());
#endif
  GeoAnalyzer::reset(std::move(geoTerms));
  return true;
}

bool GeoVPackAnalyzer::normalize(std::string_view args, std::string& out) {
  return normalizeImpl<GeoVPackAnalyzer>(args, out);
}

irs::analysis::analyzer::ptr GeoVPackAnalyzer::make(std::string_view args) {
  return makeImpl<GeoVPackAnalyzer>(args);
}

irs::bytes_view GeoVPackAnalyzer::store(irs::token_stream* ctx,
                                        velocypack::Slice slice) {
  TRI_ASSERT(ctx != nullptr);
  auto& impl = basics::downCast<GeoVPackAnalyzer>(*ctx);
  if (impl._type == Type::CENTROID) {
    TRI_ASSERT(!impl._shape.empty());
    S2LatLng const centroid{impl._shape.centroid()};
    impl._builder.clear();
    toVelocyPack(impl._builder, centroid);
    slice = impl._builder.slice();
  }
  return ref<irs::byte_type>(slice);
}

GeoVPackAnalyzer::GeoVPackAnalyzer(Options const& options)
    : GeoJsonAnalyzerBase{irs::type<GeoVPackAnalyzer>::get(), options},
      _legacy{options.legacy} {}

bool GeoVPackAnalyzer::reset(std::string_view value) {
  // TODO(MBkkt) legacyCovering should be equal _legacy
  return resetImpl(value, _legacy, /*legacyCovering=*/true);
}

void GeoVPackAnalyzer::prepare(GeoFilterOptionsBase& options) const {
  options.options = _indexer.options();
  options.stored = _legacy ? StoredType::VPackLegacy : StoredType::VPack;
}

bool GeoS2Analyzer::normalize(std::string_view args, std::string& out) {
  return normalizeImpl<GeoS2Analyzer>(args, out);
}

irs::analysis::analyzer::ptr GeoS2Analyzer::make(std::string_view args) {
  return makeImpl<GeoS2Analyzer>(args);
}

irs::bytes_view GeoS2Analyzer::store(irs::token_stream* ctx,
                                     velocypack::Slice slice) {
  TRI_ASSERT(ctx != nullptr);
  auto& impl = basics::downCast<GeoS2Analyzer>(*ctx);
  impl._encoder.Resize(0);
  if (impl._type == Type::SHAPE) {
    impl._shape.Encode(impl._encoder, impl._hint);
  } else {
    auto const centroid = impl._shape.centroid();
    geo::encodePoint(impl._encoder, centroid, impl._hint);
  }
  return irs::bytes_view{
      reinterpret_cast<const unsigned char*>(impl._encoder.base()),
      impl._encoder.length()};
}

GeoS2Analyzer::GeoS2Analyzer(Options const& options)
    : GeoJsonAnalyzerBase{irs::type<GeoS2Analyzer>::get(), options},
      _hint{options.hint} {
  _encoder.Ensure(30);
}

bool GeoS2Analyzer::reset(std::string_view value) {
  return resetImpl(value, /*legacy=*/false, /*legacyCovering=*/false);
}

void GeoS2Analyzer::prepare(GeoFilterOptionsBase& options) const {
  options.options = _indexer.options();
  options.stored = _type == GeoJsonAnalyzerBase::Type::SHAPE
                       ? StoredType::S2Shape
                       : StoredType::S2Point;
}

bool GeoPointAnalyzer::normalize(std::string_view args, std::string& out) {
  return normalizeImpl<GeoPointAnalyzer>(args, out);
}

irs::analysis::analyzer::ptr GeoPointAnalyzer::make(std::string_view args) {
  return makeImpl<GeoPointAnalyzer>(args);
}

GeoPointAnalyzer::GeoPointAnalyzer(Options const& options)
    : GeoAnalyzer{irs::type<GeoPointAnalyzer>::get(),
                  S2Options(options.options, true)},
      _fromArray{options.latitude.empty()},
      _latitude{options.latitude},
      _longitude{options.longitude} {
  TRI_ASSERT(_latitude.empty() == _longitude.empty());
}

void GeoPointAnalyzer::prepare(GeoFilterOptionsBase& options) const {
  options.options = _indexer.options();
  options.stored = StoredType::VPack;
}

bool GeoPointAnalyzer::parsePoint(velocypack::Slice json,
                                  S2LatLng& point) const {
  velocypack::Slice lat, lng;
  if (_fromArray) {
    if (!json.isArray()) {
      return false;
    }
    velocypack::ArrayIterator it{json};
    if (it.size() != 2) {
      return false;
    }
    lat = *it;
    it.next();
    lng = *it;
  } else {
    lat = json.get(_latitude);
    lng = json.get(_longitude);
  }
  if (ADB_UNLIKELY(!lat.isNumber<double>() || !lng.isNumber<double>())) {
    return false;
  }
  point =
      S2LatLng::FromDegrees(lat.getNumber<double>(), lng.getNumber<double>())
          .Normalized();
  return true;
}

bool GeoPointAnalyzer::reset(std::string_view value) {
  if (!parsePoint(slice(value), _point)) {
    return false;
  }

  GeoAnalyzer::reset(_indexer.GetIndexTerms(_point.ToPoint(), {}));
  return true;
}

irs::bytes_view GeoPointAnalyzer::store(irs::token_stream* ctx,
                                        velocypack::Slice slice) {
  TRI_ASSERT(ctx != nullptr);
  auto& impl = basics::downCast<GeoPointAnalyzer>(*ctx);
  // reuse already parsed point
  auto& point = impl._point;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  S2LatLng slicePoint;
  TRI_ASSERT(impl.parsePoint(slice, slicePoint));
  TRI_ASSERT(slicePoint == point);
#endif
  impl._builder.clear();
  toVelocyPack(impl._builder, point);
  return ref<irs::byte_type>(impl._builder.slice());
}

}  // namespace arangodb::iresearch
