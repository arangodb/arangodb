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

#include "analysis/analyzers.hpp"

#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

#include "Geo/ShapeContainer.h"
#include "Geo/GeoJson.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "VPackDeserializer/deserializer.h"

namespace  {

using namespace arangodb;
using namespace arangodb::iresearch;
using namespace arangodb::velocypack::deserializer;

constexpr const char MAX_CELLS_PARAM[] = "maxCells";
constexpr const char MIN_LEVEL_PARAM[] = "minLevel";
constexpr const char MAX_LEVEL_PARAM[] = "maxLevel";
constexpr const char POINTS_ONLY_PARAM[] = "pointsOnly";
constexpr const char OPTIMIZE_FOR_SPACE_PARAM[] = "optimizeForSpace";

using OptionsDeserializer = velocypack::deserializer::utilities::constructing_deserializer<GeoAnalyzer::Options, parameter_list<
  factory_simple_parameter<MAX_CELLS_PARAM, int32_t, false, values::numeric_value<int32_t, S2RegionCoverer::Options::kDefaultMaxCells>>,
  factory_simple_parameter<MIN_LEVEL_PARAM, int32_t, false, values::numeric_value<int32_t, 0>>,
  factory_simple_parameter<MAX_LEVEL_PARAM, int32_t, false, values::numeric_value<int32_t, S2CellId::kMaxLevel>>,
  factory_simple_parameter<POINTS_ONLY_PARAM, bool, false, std::false_type>,
  factory_simple_parameter<OPTIMIZE_FOR_SPACE_PARAM, bool, false, std::false_type>>
>;

void toVelocyPack(VPackBuilder& builder, GeoAnalyzer::Options const& opts) {
  VPackObjectBuilder root(&builder);
  builder.add(MAX_CELLS_PARAM, VPackValue(opts.maxCells));
  builder.add(MIN_LEVEL_PARAM, VPackValue(opts.minLevel));
  builder.add(MAX_LEVEL_PARAM, VPackValue(opts.maxLevel));
  builder.add(POINTS_ONLY_PARAM, VPackValue(opts.pointsOnly));
  builder.add(OPTIMIZE_FOR_SPACE_PARAM, VPackValue(opts.optimizeForSpace));
}

bool fromVelocyPack(irs::string_ref const& args, GeoAnalyzer::Options& out) {
  auto const slice = arangodb::iresearch::slice(args);

  auto const res = deserialize<OptionsDeserializer>(slice);

  if (!res.ok()) {
    LOG_TOPIC("4349c", WARN, arangodb::iresearch::TOPIC) <<
      "Failed to deserialize options from JSON while constructing geo_analyzer, error: '" <<
      res.error().message << "'";
    return false;
  }

  out = res.get();
  return true;
}

S2RegionTermIndexer::Options S2Options(
    GeoAnalyzer::Options const& opts) {
  S2RegionTermIndexer::Options s2opts;
  s2opts.set_max_cells(opts.maxCells);
  s2opts.set_min_level(opts.minLevel);
  s2opts.set_max_level(opts.maxLevel);
  s2opts.set_index_contains_points_only(opts.pointsOnly);
  s2opts.set_optimize_for_space(opts.optimizeForSpace);

  return s2opts;
}

geo::ShapeContainer shape(VPackSlice slice) {
  geo::ShapeContainer shape;

  Result res;
  if (slice.isObject()) {
    res = geo::geojson::parseRegion(slice, shape);
  } else if (slice.isArray()) {
    if (slice.isArray() && slice.length() >= 2) {
      res = shape.parseCoordinates(slice, /*geoJson*/ true);
    }
  } else {
    LOG_TOPIC("4449c", WARN, arangodb::iresearch::TOPIC)
      << "Geo JSON or array of coordinates expected, got '"
      << slice.typeName() << "'";

    return {};
  }

  if (res.fail()) {
    LOG_TOPIC("4549c", WARN, arangodb::iresearch::TOPIC)
      << "Failed to parse value as GEO JSON or array of coordinates, error '"
      << res.errorMessage() << "'";

    return {};
  }

  return shape;
}

}

namespace arangodb {
namespace iresearch {

/*static*/ bool GeoAnalyzer::normalize(
    const irs::string_ref& args,  std::string& out) {
  Options opts;

  if (!fromVelocyPack(args, opts)) {
    return false;
  }

  VPackBuilder root;
  toVelocyPack(root, opts);

  out.resize(root.slice().byteSize());
  std::memcpy(&out[0], root.slice().begin(), out.size());
  return true;
}

/*static*/ irs::analysis::analyzer::ptr GeoAnalyzer::make(
    irs::string_ref const& args) {
  Options opts;

  if (!fromVelocyPack(args, opts)) {
    return nullptr;
  }

  return std::make_shared<GeoAnalyzer>(opts, irs::string_ref::EMPTY);
}

GeoAnalyzer::GeoAnalyzer(const Options& opts,
                         const irs::string_ref& prefix)
  : attributes{{
      { irs::type<irs::increment>::id(), &_inc       },
      { irs::type<irs::offset>::id(), &_offset       },
      { irs::type<irs::term_attribute>::id(), &_term }},
      irs::type<GeoAnalyzer>::get()
    },
    _indexer(S2Options(opts)),
    _prefix(prefix) {
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


bool GeoAnalyzer::reset(irs::string_ref const& value) {
  geo::ShapeContainer const shape = ::shape(iresearch::slice(value));

  if (shape.empty()) {
    return false;
  }

  auto const* region = shape.region();
  TRI_ASSERT(region);

  if (arangodb::geo::ShapeContainer::Type::S2_POINT == shape.type()) {
    _terms = _indexer.GetIndexTerms(static_cast<S2PointRegion const*>(region)->point(), _prefix);
  } else {
    _terms = _indexer.GetIndexTerms(*region, _prefix);
  }

  _begin = _terms.data();
  _end = _begin + _terms.size();

  return true;
}

bool GeoAnalyzer::reset_for_querying(irs::string_ref const& value) {
  geo::ShapeContainer const shape = ::shape(iresearch::slice(value));

  if (shape.empty()) {
    return false;
  }

  auto const* region = shape.region();
  TRI_ASSERT(region);

  if (arangodb::geo::ShapeContainer::Type::S2_POINT == shape.type()) {
    _terms = _indexer.GetQueryTerms(static_cast<S2PointRegion const*>(region)->point(), _prefix);
  } else {
    _terms = _indexer.GetQueryTerms(*region, _prefix);
  }

  _begin = _terms.data();
  _end = _begin + _terms.size();

  return true;
}

} // iresearch
} // arangodb
