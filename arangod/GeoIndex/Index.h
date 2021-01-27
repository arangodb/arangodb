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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GEO_INDEX_H
#define ARANGOD_GEO_INDEX_H 1

#include <s2/s2cell_id.h>
#include <s2/s2latlng.h>

#include "Basics/Result.h"
#include "Geo/GeoParams.h"

namespace arangodb {
namespace aql {
struct AstNode;
struct Variable;
}  // namespace aql
namespace basics {
struct AttributeName;
}
namespace velocypack {
class Slice;
}

namespace geo {
struct Coordinate;
struct QueryParams;
class ShapeContainer;
}  // namespace geo

namespace geo_index {

/// Mixin for geo indexes
struct Index {
  /// @brief geo index variants
  enum class Variant : uint8_t {
    NONE = 0,
    /// two distinct fields representing GeoJSON Point
    INDIVIDUAL_LAT_LON,
    /// pair [<latitude>, <longitude>] eqvivalent to GeoJSON Point
    COMBINED_LAT_LON,
    // geojson object or legacy coordinate
    // pair [<longitude>, <latitude>]. Should also support
    // other geojson object types.
    GEOJSON
  };

 protected:
  /// @brief Initialize coverParams
  Index(velocypack::Slice const&, std::vector<std::vector<basics::AttributeName>> const&);

 public:
  /// @brief Parse document and return cells for indexing
  Result indexCells(velocypack::Slice const& doc, std::vector<S2CellId>& cells,
                    S2Point& centroid) const;

  Result shape(velocypack::Slice const& doc, geo::ShapeContainer& shape) const;

  /// @brief Parse AQL condition into query parameters
  /// Public to allow usage by legacy geo indexes
  static void parseCondition(aql::AstNode const* node, aql::Variable const* reference,
                             geo::QueryParams& params);

  Variant variant() const { return _variant; }

 private:
  static S2LatLng parseGeoDistance(aql::AstNode const* node, aql::Variable const* ref);

  static S2LatLng parseDistFCall(aql::AstNode const* node, aql::Variable const* ref);
  static void handleNode(aql::AstNode const* node, aql::Variable const* ref,
                         geo::QueryParams& params);

 protected:
  /// @brief immutable region coverer parameters
  geo::RegionCoverParams _coverParams;
  /// @brief the type of geo we support
  Variant _variant;

  /// @brief attribute paths
  std::vector<std::string> _location;
  std::vector<std::string> _latitude;
  std::vector<std::string> _longitude;
};

}  // namespace geo_index
}  // namespace arangodb

#endif
