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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#include "Assertions/Assert.h"
#include "Geo/LaxShapeContainer.h"
#include "Geo/S2/S2MultiPointShape.h"
#include "Geo/S2/S2MultiPolylineShape.h"
#include "Geo/S2/S2PointShape.h"
#include "Basics/DownCast.h"

#include <s2/s2lax_polygon_shape.h>
#include <s2/s2lax_polyline_shape.h>
#include <s2/s2shape_measures.h>
#include <s2/s2polyline_measures.h>
#include <s2/s2loop_measures.h>

S2Point GetCentroid(const S2LaxPolygonShape& shape) noexcept {
  TRI_ASSERT(shape.dimension() == 2);
  S2Point centroid;
  int num_chains = shape.num_chains();
  TRI_ASSERT(num_chains > 0);
  for (int chain_id = 0; chain_id < num_chains; ++chain_id) {
    S2Shape::Chain chain = shape.chain(chain_id);
    centroid += S2::GetCentroid(S2PointLoopSpan{
        shape.vertices_.get() + static_cast<size_t>(chain.start),
        static_cast<size_t>(chain.length)});
  }
  TRI_ASSERT(centroid == S2::GetCentroid(shape));
  return centroid;
}

S2Point GetCentroid(const S2LaxPolylineShape& shape) noexcept {
  TRI_ASSERT(shape.dimension() == 1);
  TRI_ASSERT(shape.num_chains() <= 1);
  auto centroid = S2::GetCentroid(S2PointSpan{
      shape.vertices_.get(), static_cast<size_t>(shape.num_vertices())});
  TRI_ASSERT(centroid == S2::GetCentroid(shape));
  return centroid;
}

namespace arangodb::geo {

S2Point LaxShapeContainer::centroid() const noexcept {
  TRI_ASSERT(_data != nullptr);
  // return S2::GetCentroid(*_data).Normalize();
  // S2::GetCentroid makes unnecessary allocation,
  // we can call it in query, so we want to avoid it
  switch (_type) {
    case Type::S2Point:
      return basics::downCast<S2PointShape>(*_data).GetCentroid();
    case Type::S2Polyline:
      return GetCentroid(basics::downCast<S2LaxPolylineShape>(*_data))
          .Normalize();
    case Type::S2Polygon:
      return GetCentroid(basics::downCast<S2LaxPolygonShape>(*_data))
          .Normalize();
    case Type::S2MultiPoint:
      return basics::downCast<S2MultiPointShape>(*_data)
          .GetCentroid()
          .Normalize();
    case Type::S2MultiPolyline:
      return basics::downCast<S2MultiPolylineShape>(*_data)
          .GetCentroid()
          .Normalize();
    default:
      TRI_ASSERT(false);
      break;
  }
  return {};
}

bool LaxShapeContainer::Decode(Decoder& decoder) { return false; }

}  // namespace arangodb::geo
