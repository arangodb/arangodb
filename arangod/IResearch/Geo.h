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

#ifndef ARANGODB_IRESEARCH__IRESEARCH_GEO
#define ARANGODB_IRESEARCH__IRESEARCH_GEO 1

#include <s2/s2region_term_indexer.h>

namespace arangodb {

namespace velocypack {
class Slice;
class Builder;
}

namespace geo{
class ShapeContainer;
}

namespace iresearch {

struct GeoOptions {
  static constexpr int32_t MAX_CELLS = S2RegionCoverer::Options::kDefaultMaxCells;
  static constexpr int32_t MIN_LEVEL = 0;
  static constexpr int32_t MAX_LEVEL = S2CellId::kMaxLevel;

  static constexpr int32_t DEFAULT_MAX_CELLS = 20;
  static constexpr int32_t DEFAULT_MIN_LEVEL = 4;
  static constexpr int32_t DEFAULT_MAX_LEVEL = 23; // ~1m

  int32_t maxCells{DEFAULT_MAX_CELLS};
  int32_t minLevel{DEFAULT_MIN_LEVEL};
  int32_t maxLevel{DEFAULT_MAX_LEVEL};
};

inline S2RegionTermIndexer::Options S2Options(GeoOptions const& opts) {
  S2RegionTermIndexer::Options s2opts;
  s2opts.set_max_cells(opts.maxCells);
  s2opts.set_min_level(opts.minLevel);
  s2opts.set_max_level(opts.maxLevel);

  return s2opts;
}

bool parseShape(velocypack::Slice slice, geo::ShapeContainer& shape, bool onlyPoint);
bool parsePoint(velocypack::Slice latSlice, velocypack::Slice lngSlice, S2LatLng& out);

void toVelocyPack(velocypack::Builder& builder, S2LatLng const& point);

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_GEO

