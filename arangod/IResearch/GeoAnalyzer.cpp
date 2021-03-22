////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "GeoAnalyzer.h"

#include <string>

#include "s2/s2point_region.h"
#include "s2/s2latlng.h"


#include "analysis/analyzers.hpp"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"
#include "Geo/GeoJson.h"
#include "Geo/GeoParams.h"
#include "IResearch/Geo.h"



#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "VPackDeserializer/deserializer.h"

namespace  {

using namespace std::literals::string_literals;

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

struct GeoOptionsValidator {
  std::optional<deserialize_error> operator()(GeoOptions const& opts) const {
    if (opts.minLevel < 0 || opts.maxCells < 0 || opts.maxCells < 0) {
      return deserialize_error{"'minLevel', 'maxLevel', 'maxCells' must be a positive integer"};
    }

    if (opts.maxLevel > GeoOptions::MAX_LEVEL || opts.minLevel > GeoOptions::MAX_LEVEL) {
      return deserialize_error{
        "'minLevel', 'maxLevel' must not exceed '"s
        .append(std::to_string(GeoOptions::MAX_LEVEL))
        .append("'")};
    }

    if (opts.minLevel > opts.maxLevel) {
      return deserialize_error{"'minLevel' must be less or equal than 'maxLevel'"};
    }

    return {};
  }
};

using GeoOptionsDeserializer = utilities::constructing_deserializer<GeoOptions, parameter_list<
  factory_simple_parameter<MAX_CELLS_PARAM, int32_t, false, values::numeric_value<int32_t, GeoOptions::DEFAULT_MAX_CELLS>>,
  factory_simple_parameter<MIN_LEVEL_PARAM, int32_t, false, values::numeric_value<int32_t, GeoOptions::DEFAULT_MIN_LEVEL>>,
  factory_simple_parameter<MAX_LEVEL_PARAM, int32_t, false, values::numeric_value<int32_t, GeoOptions::DEFAULT_MAX_LEVEL>>
>>;

using ValidatingGeoOptionsDeserializer = validate<GeoOptionsDeserializer, GeoOptionsValidator>;

using GeoJSONOptionsDeserializer = utilities::constructing_deserializer<GeoJSONAnalyzer::Options, parameter_list<
  factory_deserialized_default<OPTIONS_PARAM, ValidatingGeoOptionsDeserializer>,
  factory_deserialized_default<TYPE_PARAM, GeoJSONTypeEnumDeserializer,
                                           values::numeric_value<GeoJSONAnalyzer::Type,
                                                                 static_cast<std::underlying_type_t<GeoJSONAnalyzer::Type>>(GeoJSONAnalyzer::Type::SHAPE)>>>
>;

struct GeoPointAnalyzerOptionsValidator {
  std::optional<deserialize_error> operator()(GeoPointAnalyzer::Options const& opts) const {
    bool const latitudeEmpty = opts.latitude.empty();
    bool const longitudeEmpty = opts.longitude.empty();

    if (latitudeEmpty && longitudeEmpty) {
      return {};
    }

    if (latitudeEmpty) {
      return deserialize_error{"'latitude' path is not set"};
    }

    if (longitudeEmpty) {
      return deserialize_error{"'longitude' path is not set"};
    }

    return {};
  }
};

template<typename T>
using vector = std::vector<T>;

using StringVectorDeserializer = array_deserializer<values::value_deserializer<std::string>, vector>;

using GeoPointsOptionsDeserializer = utilities::constructing_deserializer<GeoPointAnalyzer::Options, parameter_list<
  factory_deserialized_default<OPTIONS_PARAM, ValidatingGeoOptionsDeserializer>,
  factory_deserialized_default<LATITUDE_PARAM, StringVectorDeserializer, values::default_constructed_value<std::initializer_list<std::string>>>,
  factory_deserialized_default<LONGITUDE_PARAM, StringVectorDeserializer, values::default_constructed_value<std::initializer_list<std::string>>>
>>;

using ValidatingGeoPointsOptionsDeserializer = validate<GeoPointsOptionsDeserializer, GeoPointAnalyzerOptionsValidator>;

template<typename Analyzer>
struct Deserializer;

template<>
struct Deserializer<GeoJSONAnalyzer> {
  using type = GeoJSONOptionsDeserializer;
};

template<>
struct Deserializer<GeoPointAnalyzer> {
  using type = ValidatingGeoPointsOptionsDeserializer;
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
  auto addArray = [&builder](const char* name, std::vector<std::string> const& values) {
    VPackArrayBuilder arrayScope(&builder, name);
    for (auto& value: values) {
      builder.add(VPackValue(value));
    }
  };

  VPackObjectBuilder root(&builder);
  addArray(LATITUDE_PARAM, opts.latitude);
  addArray(LONGITUDE_PARAM, opts.longitude);
  builder.add(VPackValue(OPTIONS_PARAM));
  toVelocyPack(builder, opts.options);
}

template<typename Analyzer>
bool fromVelocyPack(irs::string_ref const& args, typename Analyzer::Options& out) {
  auto const slice = arangodb::iresearch::slice(args);

  auto const res = deserialize<typename Deserializer<Analyzer>::type,
                               hints::hint_list<hints::ignore_unknown>>(slice);

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

/*static*/ VPackSlice GeoJSONAnalyzer::store(
    irs::token_stream const* ctx,
    VPackSlice slice,
    velocypack::Buffer<uint8_t>& buf) noexcept {
  TRI_ASSERT(ctx);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* impl = dynamic_cast<GeoJSONAnalyzer const*>(ctx);
  TRI_ASSERT(impl);
#else
  auto* impl = static_cast<GeoJSONAnalyzer const*>(ctx);
#endif

  if (Type::CENTROID == impl->shapeType()) {
    TRI_ASSERT(!impl->_shape.empty());
    S2LatLng const centroid(impl->_shape.centroid());

    VPackBuilder array(buf);
    toVelocyPack(array, centroid);

    return array.slice();
  }

  return slice;
}

GeoJSONAnalyzer::GeoJSONAnalyzer(Options const& opts)
  : GeoAnalyzer(irs::type<GeoJSONAnalyzer>::get()),
    _indexer(S2Options(opts.options)),
    _type(opts.type) {
}

void GeoJSONAnalyzer::prepare(S2RegionTermIndexer::Options& opts) const {
  opts = _indexer.options();
  opts.set_index_contains_points_only(_type != Type::SHAPE);
}

bool GeoJSONAnalyzer::reset(const irs::string_ref& value) {
  auto const slice = iresearch::slice(value);

  if (!parseShape(slice, _shape, _type == Type::POINT)) {
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
    _fromArray{opts.latitude.empty() || opts.longitude.empty()},
    _latitude(!_fromArray ? opts.latitude : std::vector<std::string>{}),
    _longitude(!_fromArray ? opts.longitude : std::vector<std::string>{}) {
}

void GeoPointAnalyzer::prepare(S2RegionTermIndexer::Options& opts) const {
  opts = _indexer.options();
  opts.set_index_contains_points_only(true);
}

bool GeoPointAnalyzer::parsePoint(VPackSlice json, S2LatLng& point) const {
  VPackSlice latitude, longitude;
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

  return iresearch::parsePoint(latitude, longitude, point);
}

bool GeoPointAnalyzer::reset(const irs::string_ref& value) {
  if (!parsePoint(iresearch::slice(value), _point)) {
    return false;
  }

  GeoAnalyzer::reset(_indexer.GetIndexTerms(_point.ToPoint(), {}));
  return true;
}

/*static*/ VPackSlice GeoPointAnalyzer::store(
    irs::token_stream const* ctx,
    [[maybe_unused]] VPackSlice slice,
    VPackBuffer<uint8_t>& buf) {
  TRI_ASSERT(ctx);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* impl = dynamic_cast<GeoPointAnalyzer const*>(ctx);
  TRI_ASSERT(impl);

  {
    S2LatLng point;
    if (!impl->parsePoint(slice, point)) {
      return VPackSlice::noneSlice();
    }
    TRI_ASSERT(point == impl->_point);
  }
#else
  auto* impl = static_cast<GeoPointAnalyzer const*>(ctx);
#endif

  // reuse already parsed point
  auto& point = impl->_point;

  if (!point.is_valid()) {
    return VPackSlice::noneSlice();
  }

  VPackBuilder array(buf);
  toVelocyPack(array, point);

  return array.slice();
}

} // iresearch
} // arangodb
