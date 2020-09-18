////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "GeoAnalyzer.h"

#include "s2/s2point_region.h"
#include "s2/s2latlng.h"

#include "analysis/analyzers.hpp"

#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

#include "Geo/GeoJson.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "VPackDeserializer/deserializer.h"

namespace  {

using namespace arangodb;
using namespace arangodb::iresearch;
using namespace arangodb::velocypack::deserializer;

constexpr const char GEOJSON_POINT_TYPE[] = "point";
constexpr const char GEOJSON_CENTROID_TYPE[] = "centroid";
constexpr const char GEOJSON_SHAPE_TYPE[] = "shape";
constexpr const char TYPE_PARAM[] = "type";
constexpr const char OPTIONS_PARAM[] = "options";
constexpr const char MAX_CELLS_PARAM[] = "maxCells";
constexpr const char MIN_LEVEL_PARAM[] = "minLevel";
constexpr const char MAX_LEVEL_PARAM[] = "maxLevel";
constexpr const char LATITUDE_PARAM[] = "latitude";
constexpr const char LONGITUDE_PARAM[] = "longitude";

using GeoJSONTypeEnumDeserializer = enum_deserializer<
  GeoJSONAnalyzer::Type,
  enum_member<GeoJSONAnalyzer::Type::SHAPE, values::string_value<GEOJSON_SHAPE_TYPE>>,
  enum_member<GeoJSONAnalyzer::Type::CENTROID, values::string_value<GEOJSON_CENTROID_TYPE>>,
  enum_member<GeoJSONAnalyzer::Type::POINT, values::string_value<GEOJSON_POINT_TYPE>>
>;

using GeoOptionsDeserializer = utilities::constructing_deserializer<GeoOptions, parameter_list<
  factory_simple_parameter<MAX_CELLS_PARAM, int32_t, false, values::numeric_value<int32_t, S2RegionCoverer::Options::kDefaultMaxCells>>,
  factory_simple_parameter<MIN_LEVEL_PARAM, int32_t, false, values::numeric_value<int32_t, 0>>,
  factory_simple_parameter<MAX_LEVEL_PARAM, int32_t, false, values::numeric_value<int32_t, S2CellId::kMaxLevel>>>
>;

using GeoJSONOptionsDeserializer = utilities::constructing_deserializer<GeoJSONAnalyzer::Options, parameter_list<
  factory_deserialized_parameter<OPTIONS_PARAM, GeoOptionsDeserializer, true>,
  factory_deserialized_parameter<TYPE_PARAM, GeoJSONTypeEnumDeserializer, true>>
>;

using GeoPointsOptionsDeserializer = utilities::constructing_deserializer<GeoPointAnalyzer::Options, parameter_list<
  factory_deserialized_parameter<OPTIONS_PARAM, GeoOptionsDeserializer, true>,
  factory_deserialized_parameter<LATITUDE_PARAM, values::value_deserializer<std::string>, true>,
  factory_deserialized_parameter<LONGITUDE_PARAM, values::value_deserializer<std::string>, true>>
>;

template<typename Analyzer>
struct Deserializer;

template<>
struct Deserializer<GeoJSONAnalyzer> {
  using type = GeoJSONOptionsDeserializer;
};

template<>
struct Deserializer<GeoPointAnalyzer> {
  using type = GeoPointsOptionsDeserializer;
};

void toVelocyPack(VPackBuilder& builder, GeoOptions const& opts) {
  VPackObjectBuilder root(&builder);
  builder.add(MAX_CELLS_PARAM, VPackValue(opts.maxCells));
  builder.add(MIN_LEVEL_PARAM, VPackValue(opts.minLevel));
  builder.add(MAX_LEVEL_PARAM, VPackValue(opts.maxLevel));
}

void toVelocyPack(VPackBuilder& builder, GeoJSONAnalyzer::Options const& opts) {
  VPackObjectBuilder root(&builder);
  switch (opts.type) {
    case GeoJSONAnalyzer::Type::SHAPE:
      builder.add(TYPE_PARAM, VPackValue(GEOJSON_SHAPE_TYPE));
      break;
    case GeoJSONAnalyzer::Type::CENTROID:
      builder.add(TYPE_PARAM, VPackValue(GEOJSON_CENTROID_TYPE));
      break;
    case GeoJSONAnalyzer::Type::POINT:
      builder.add(TYPE_PARAM, VPackValue(GEOJSON_POINT_TYPE));
      break;
  }
  builder.add(VPackValue(OPTIONS_PARAM));
  toVelocyPack(builder, opts.options);
}

void toVelocyPack(VPackBuilder& builder, GeoPointAnalyzer::Options const& opts) {
  VPackObjectBuilder root(&builder);
  builder.add(LATITUDE_PARAM, VPackValue(opts.latitude));
  builder.add(LONGITUDE_PARAM, VPackValue(opts.longitude));
  builder.add(VPackValue(OPTIONS_PARAM));
  toVelocyPack(builder, opts.options);
}

template<typename Analyzer>
bool fromVelocyPack(irs::string_ref const& args, typename Analyzer::Options& out) {
  auto const slice = arangodb::iresearch::slice(args);

  auto const res = deserialize<typename Deserializer<Analyzer>::type>(slice);

  if (!res.ok()) {
    LOG_TOPIC("4349c", WARN, arangodb::iresearch::TOPIC) <<
      "Failed to deserialize options from JSON while constructing '"
        << irs::type<Analyzer>::name() << "' analyzer, error: '"
        << res.error().message << "'";
    return false;
  }

  out = res.get();
  return true;
}

S2RegionTermIndexer::Options S2Options(
    GeoOptions const& opts) {
  S2RegionTermIndexer::Options s2opts;
  s2opts.set_max_cells(opts.maxCells);
  s2opts.set_min_level(opts.minLevel);
  s2opts.set_max_level(opts.maxLevel);

  return s2opts;
}

bool parseShape(VPackSlice slice, geo::ShapeContainer& shape, bool onlyPoint) {
  Result res;
  if (slice.isObject()) {
    if (onlyPoint) {
      S2LatLng ll;
      res = geo::geojson::parsePoint(slice, ll);

      if (res.ok()) {
        shape.resetCoordinates(ll);
      }
    } else {
      res = geo::geojson::parseRegion(slice, shape);
    }
  } else if (slice.isArray()) {
    if (slice.isArray() && slice.length() >= 2) {
      res = shape.parseCoordinates(slice, /*geoJson*/ true);
    }
  } else {
    LOG_TOPIC("4449c", WARN, arangodb::iresearch::TOPIC)
      << "Geo JSON or array of coordinates expected, got '"
      << slice.typeName() << "'";

    return false;
  }

  if (res.fail()) {
    LOG_TOPIC("4549c", WARN, arangodb::iresearch::TOPIC)
      << "Failed to parse value as GEO JSON or array of coordinates, error '"
      << res.errorMessage() << "'";

    return false;
  }

  return true;
}

bool parsePoint(VPackSlice latSlice, VPackSlice lngSlice, S2Point& out) {
  if (!latSlice.isNumber() || !lngSlice.isNumber()) {
    return false;
  }

  double_t lat, lng;
  try {
    lat = latSlice.getNumber<double_t>();
    lng = lngSlice.getNumber<double_t>();
  } catch (...) {
    return false;
  }

  auto const latlng = S2LatLng::FromDegrees(lat, lng);

  if (latlng.is_valid()) {
    return false;
  }

  out = latlng.ToPoint();
  return true;
}

template<typename Analyzer>
bool normalize(const irs::string_ref& args,  std::string& out) {
  typename Analyzer::Options opts;

  if (!fromVelocyPack<Analyzer>(args, opts)) {
    return false;
  }

  VPackBuilder root;
  toVelocyPack(root, opts);

  out.resize(root.slice().byteSize());
  std::memcpy(&out[0], root.slice().begin(), out.size());
  return true;
}

template<typename Analyzer>
irs::analysis::analyzer::ptr make(
    irs::string_ref const& args) {
  typename Analyzer::Options opts;

  if (!fromVelocyPack<Analyzer>(args, opts)) {
    return nullptr;
  }

  return std::make_shared<Analyzer>(opts);
}

}

namespace arangodb {
namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                                      GeoAnalyzer
// ----------------------------------------------------------------------------

GeoAnalyzer::GeoAnalyzer(const irs::type_info& type)
  : attributes{{
      { irs::type<irs::increment>::id(), &_inc       },
      { irs::type<irs::offset>::id(), &_offset       },
      { irs::type<irs::term_attribute>::id(), &_term }},
      type
    } {
}

bool GeoAnalyzer::next() noexcept {
  if (_begin >= _end) {
    return false;
  }

  auto& value = *_begin++;

  _term.value = irs::bytes_ref(
    reinterpret_cast<const irs::byte_type*>(value.c_str()),
    value.size());

  return true;
}

void GeoAnalyzer::reset(std::vector<std::string>&& terms) noexcept {
  _terms = std::move(terms);
  _begin = _terms.data();
  _end = _begin + _terms.size();
}

// ----------------------------------------------------------------------------
// --SECTION--                                                  GeoJSONAnalyzer
// ----------------------------------------------------------------------------

/*static*/ bool GeoJSONAnalyzer::normalize(
    const irs::string_ref& args, std::string& out) {
  return ::normalize<GeoJSONAnalyzer>(args, out);
}

/*static*/ irs::analysis::analyzer::ptr GeoJSONAnalyzer::make(
    irs::string_ref const& args) {
  return ::make<GeoJSONAnalyzer>(args);
}

GeoJSONAnalyzer::GeoJSONAnalyzer(Options const& opts)
  : GeoAnalyzer(irs::type<GeoJSONAnalyzer>::get()),
    _indexer(S2Options(opts.options)),
    _type(opts.type) {
}

bool GeoJSONAnalyzer::reset(const irs::string_ref& value) {
  auto const slice = iresearch::slice(value);

  if (!::parseShape(slice, _shape, _type == Type::POINT)) {
    return false;
  }

  TRI_ASSERT(!_shape.empty());

  if (_type == Type::CENTROID) {
    GeoAnalyzer::reset(_indexer.GetIndexTerms(_shape.centroid(), {}));
  } else {
    auto const* region = _shape.region();
    TRI_ASSERT(region);

    if (arangodb::geo::ShapeContainer::Type::S2_POINT == _shape.type()) {
      GeoAnalyzer::reset(_indexer.GetIndexTerms(static_cast<S2PointRegion const*>(region)->point(), {}));
    } else {
      GeoAnalyzer::reset(_indexer.GetIndexTerms(*region, {}));
    }
  }

  return true;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                 GeoPointAnalyzer
// ----------------------------------------------------------------------------

/*static*/ bool GeoPointAnalyzer::normalize(
    const irs::string_ref& args, std::string& out) {
  return ::normalize<GeoPointAnalyzer>(args, out);
}

/*static*/ irs::analysis::analyzer::ptr GeoPointAnalyzer::make(
    irs::string_ref const& args) {
  return ::make<GeoPointAnalyzer>(args);
}

GeoPointAnalyzer::GeoPointAnalyzer(Options const& opts)
  : GeoAnalyzer(irs::type<GeoPointAnalyzer>::get()),
    _indexer(S2Options(opts.options)),
    _latitude(opts.latitude),
    _longitude(opts.longitude),
    _fromArray{!_latitude.empty() && !_longitude.empty()} {
}

bool GeoPointAnalyzer::reset(const irs::string_ref& value) {
  auto const json = iresearch::slice(value);

  VPackSlice latitude, longitude;
  S2Point point;
  if (_fromArray) {
    if (!json.isArray() || json.length() < 2) {
      return false;
    }

    latitude = json.at(0);
    longitude = json.at(1);
  } else {
    latitude = json.get(_latitude);
    longitude = json.get(_longitude);
  }

  if (!::parsePoint(latitude, longitude, point)) {
    return false;
  }

  GeoAnalyzer::reset(_indexer.GetIndexTerms(point, {}));
  return true;
}

} // iresearch
} // arangodb
