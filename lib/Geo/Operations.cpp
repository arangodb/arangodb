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

#include "Geo/Operations.h"
#include "Geo/ShapeContainer.h"
#include "Geo/LaxShapeContainer.h"
#include "Geo/S2/S2MultiPointRegion.h"
#include "Assertions/Assert.h"
#include "Basics/DownCast.h"

#include <s2/s2point.h>
#include <s2/s2polygon.h>
#include <s2/s2lax_polygon_shape.h>

namespace arangodb::geo {
namespace {

constexpr uint8_t binOpCase(auto lhs, auto rhs) noexcept {
  // any >= 7 && <= 41 is ok, but compiler use 8, so why not?
  return (static_cast<uint8_t>(lhs) * 8) + static_cast<uint8_t>(rhs);
}

}  // namespace

bool intersects(ShapeContainer& lhs, LaxShapeContainer& rhs,
                S2ShapeIndex* lhsIndex, S2ShapeIndex* rhsIndex) {}

bool contains(ShapeContainer& lhs, LaxShapeContainer& rhs,
              S2ShapeIndex* lhsIndex, S2ShapeIndex* rhsIndex) {
  using Lhs = ShapeContainer::Type;
  using Rhs = LaxShapeContainer::Type;
  // In this implementation we force compiler to generate single jmp table.
  auto const sum = binOpCase(lhs.type(), rhs.type());
  auto* region = lhs.region();
  auto* shape = rhs.shape();
  switch (sum) {
    case binOpCase(Lhs::S2_POINT, Rhs::S2Point):
      return containsPoint<S2PointRegion>(*region, *shape);
    case binOpCase(Lhs::S2_POLYGON, Rhs::S2Point):
      return containsPoint<S2Polygon>(*region, *shape);
    case binOpCase(Lhs::S2_MULTIPOINT, Rhs::S2Point):
      return containsPoint<S2MultiPointRegion>(*region, *shape);

    case binOpCase(Lhs::S2_POLYLINE, Rhs::S2Polyline):
      return containsPolyline<S2Polyline>(*region, *shape);
    case binOpCase(Lhs::S2_POLYGON, Rhs::S2Polyline):
      return containsPolyline<S2Polygon>(*region, *shape);
    case binOpCase(Lhs::S2_MULTIPOLYLINE, Rhs::S2Polyline):
      return containsPolyline<S2MultiPointRegion>(*region, *shape);

    case binOpCase(Lhs::S2_POINT, Rhs::S2MultiPoint):
      return containsPoints<S2PointRegion>(*region, *shape);
    case binOpCase(Lhs::S2_POLYGON, Rhs::S2MultiPoint):
      return containsPoints<S2Polygon>(*region, *shape);
    case binOpCase(Lhs::S2_MULTIPOINT, Rhs::S2MultiPoint):
      return containsPoints<S2MultiPointRegion>(*region, *shape);

    case binOpCase(Lhs::S2_POLYGON, Rhs::S2Polygon):
      return containsPolygon(basics::downCast<S2Polygon>(*region),
                             basics::downCast<S2LaxPolygonShape>(*shape));

    case binOpCase(Lhs::S2_POLYLINE, Rhs::S2MultiPolyline):
      return containsPolylines<S2Polyline>(*region, *shape);
    case binOpCase(Lhs::S2_POLYGON, Rhs::S2MultiPolyline):
      return containsPolylines<S2Polygon>(*region, *shape);
    case binOpCase(Lhs::S2_MULTIPOLYLINE, Rhs::S2MultiPolyline):
      return containsPolylines<S2MultiPointRegion>(*region, *shape);

      // TODO(MBkkt) add warning in such cases
    case binOpCase(Lhs::S2_LATLNGRECT, Rhs::S2Point):
    case binOpCase(Lhs::S2_LATLNGRECT, Rhs::S2Polyline):
    case binOpCase(Lhs::S2_LATLNGRECT, Rhs::S2Polygon):
    case binOpCase(Lhs::S2_LATLNGRECT, Rhs::S2MultiPoint):
    case binOpCase(Lhs::S2_LATLNGRECT, Rhs::S2MultiPolyline):
      // we don't want support S2_LATLNGRECT
    case binOpCase(Lhs::S2_POINT, Rhs::S2Polygon):
    case binOpCase(Lhs::S2_POLYLINE, Rhs::S2Polygon):
    case binOpCase(Lhs::S2_MULTIPOINT, Rhs::S2Polygon):
    case binOpCase(Lhs::S2_MULTIPOLYLINE, Rhs::S2Polygon):
      // Can be true only for polygon which is point?
      // Not sure, but even in such case it is numerically unstable
    case binOpCase(Lhs::S2_POLYLINE, Rhs::S2Point):
    case binOpCase(Lhs::S2_MULTIPOLYLINE, Rhs::S2Point):
    case binOpCase(Lhs::S2_POINT, Rhs::S2Polyline):
    case binOpCase(Lhs::S2_MULTIPOINT, Rhs::S2Polyline):
    case binOpCase(Lhs::S2_POLYLINE, Rhs::S2MultiPoint):
    case binOpCase(Lhs::S2_MULTIPOLYLINE, Rhs::S2MultiPoint):
    case binOpCase(Lhs::S2_POINT, Rhs::S2MultiPolyline):
    case binOpCase(Lhs::S2_MULTIPOINT, Rhs::S2MultiPolyline):
      // is numerically unstable and thus always false
      return false;
    default:
      TRI_ASSERT(false);
  }
  return false;
}

bool contains(LaxShapeContainer& lhs, ShapeContainer& rhs,
              S2ShapeIndex* lhsIndex, S2ShapeIndex* rhsIndex) {}

}  // namespace arangodb::geo
