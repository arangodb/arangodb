#
# Copyright 2006 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#



import unittest
import pywraps2 as s2


class PyWrapS2TestCase(unittest.TestCase):

  def testContainsIsWrappedCorrectly(self):
    london = s2.S2LatLngRect(s2.S2LatLng.FromDegrees(51.3368602, 0.4931979),
                             s2.S2LatLng.FromDegrees(51.7323965, 0.1495211))
    e14lj = s2.S2LatLngRect(s2.S2LatLng.FromDegrees(51.5213527, -0.0476026),
                            s2.S2LatLng.FromDegrees(51.5213527, -0.0476026))
    self.assertTrue(london.Contains(e14lj))

  def testS2CellIdEqualsIsWrappedCorrectly(self):
    london = s2.S2LatLng.FromDegrees(51.5001525, -0.1262355)
    cell = s2.S2CellId(london)
    same_cell = s2.S2CellId(london)
    self.assertEqual(cell, same_cell)

  def testS2CellIdComparsionIsWrappedCorrectly(self):
    london = s2.S2LatLng.FromDegrees(51.5001525, -0.1262355)
    cell = s2.S2CellId(london)
    self.assertLess(cell, cell.next())
    self.assertGreater(cell.next(), cell)

  def testS2CellIdGetEdgeNeighborsIsWrappedCorrectly(self):
    cell = s2.S2CellId(0x466d319000000000)
    expected_neighbors = [s2.S2CellId(0x466d31b000000000),
                          s2.S2CellId(0x466d317000000000),
                          s2.S2CellId(0x466d323000000000),
                          s2.S2CellId(0x466d31f000000000)]
    neighbors = cell.GetEdgeNeighbors()
    self.assertEqual(neighbors, expected_neighbors)

  def testS2CellIdIntersectsIsTrueForOverlap(self):
    cell1 = s2.S2CellId(0x89c259c000000000)
    cell2 = s2.S2CellId(0x89c2590000000000)
    self.assertTrue(cell1.intersects(cell2))

  def testS2CellIdIntersectsIsFalseForNonOverlap(self):
    cell1 = s2.S2CellId(0x89c259c000000000)
    cell2 = s2.S2CellId(0x89e83d0000000000)
    self.assertFalse(cell1.intersects(cell2))

  def testS2HashingIsWrappedCorrectly(self):
    london = s2.S2LatLng.FromDegrees(51.5001525, -0.1262355)
    cell = s2.S2CellId(london)
    same_cell = s2.S2CellId(london)
    self.assertEqual(hash(cell), hash(same_cell))

  def testCovererIsWrappedCorrectly(self):
    london = s2.S2LatLngRect(s2.S2LatLng.FromDegrees(51.3368602, 0.4931979),
                             s2.S2LatLng.FromDegrees(51.7323965, 0.1495211))
    e14lj = s2.S2LatLngRect(s2.S2LatLng.FromDegrees(51.5213527, -0.0476026),
                            s2.S2LatLng.FromDegrees(51.5213527, -0.0476026))
    coverer = s2.S2RegionCoverer()
    coverer.set_max_cells(6)
    self.assertEqual(6, coverer.max_cells())
    covering = coverer.GetCovering(e14lj)
    self.assertLessEqual(len(covering), 6)
    for cellid in covering:
      self.assertTrue(london.Contains(s2.S2Cell(cellid)))
    interior = coverer.GetInteriorCovering(e14lj)
    for cellid in interior:
      self.assertTrue(london.Contains(s2.S2Cell(cellid)))

  def testS2CellUnionIsWrappedCorrectly(self):
    cell_union = s2.S2CellUnion()
    cell_union.Init([0x466d319000000000, 0x466d31b000000000])
    self.assertEqual(cell_union.num_cells(), 2)
    trondheim = s2.S2LatLng.FromDegrees(63.431052, 10.395083)
    self.assertTrue(cell_union.Contains(s2.S2CellId(trondheim)))

  def testS2PolygonIsWrappedCorrectly(self):
    london = s2.S2LatLng.FromDegrees(51.5001525, -0.1262355)
    polygon = s2.S2Polygon(s2.S2Cell(s2.S2CellId(london)))
    self.assertEqual(polygon.num_loops(), 1)
    point = london.ToPoint()
    self.assertTrue(polygon.Contains(point))

  def testS2LoopIsWrappedCorrectly(self):
    london = s2.S2LatLng.FromDegrees(51.5001525, -0.1262355)
    polygon = s2.S2Polygon(s2.S2Cell(s2.S2CellId(london)))
    loop = polygon.loop(0)
    self.assertTrue(loop.IsValid())
    self.assertEqual(0, loop.depth())
    self.assertFalse(loop.is_hole())
    self.assertEqual(4, loop.num_vertices())
    point = london.ToPoint()
    self.assertTrue(loop.Contains(point))

  def testS2PolygonCopiesLoopInConstructorBecauseItTakesOwnership(self):
    london = s2.S2LatLng.FromDegrees(51.5001525, -0.1262355)
    loop = s2.S2Loop(s2.S2Cell(s2.S2CellId(london)))
    s2.S2Polygon(loop)

  def testS2PolygonInitNestedIsWrappedCorrectly(self):
    london = s2.S2LatLng.FromDegrees(51.5001525, -0.1262355)
    small_loop = s2.S2Loop(s2.S2Cell(s2.S2CellId(london)))
    big_loop = s2.S2Loop(s2.S2Cell(s2.S2CellId(london).parent(1)))
    polygon = s2.S2Polygon()
    polygon.InitNested([big_loop, small_loop])

  def testS2PolygonInitNestedWithIncorrectTypeIsWrappedCorrectly(self):
    london = s2.S2LatLng.FromDegrees(51.5001525, -0.1262355)
    loop = s2.S2Loop(s2.S2Cell(s2.S2CellId(london)))
    polygon = s2.S2Polygon()
    with self.assertRaises(TypeError):
      polygon.InitNested([loop, s2.S2CellId()])

  def testS2PolygonGetAreaIsWrappedCorrectly(self):
    # Cell at level 10 containing central London.

    london_level_10 = s2.S2CellId(
        s2.S2LatLng.FromDegrees(51.5001525, -0.1262355)).parent(10)
    polygon = s2.S2Polygon(s2.S2Cell(london_level_10))
    # Because S2Cell.ExactArea() isn't swigged, compare S2Polygon.GetArea() with
    # S2CellUnion.ExactArea().
    cell_union = s2.S2CellUnion()
    cell_union.Init([london_level_10.id()])
    self.assertAlmostEqual(cell_union.ExactArea(), polygon.GetArea(), places=10)

  def testGetS2LatLngVertexIsWrappedCorrectly(self):
    london = s2.S2LatLng.FromDegrees(51.5001525, -0.1262355)
    polygon = s2.S2Polygon(s2.S2Cell(s2.S2CellId(london)))
    loop = polygon.loop(0)
    first_vertex = loop.GetS2LatLngVertex(0)
    self.assertIsInstance(first_vertex, s2.S2LatLng)
    self.assertEqual("51.500152,-0.126235", first_vertex.ToStringInDegrees())
    second_vertex = loop.GetS2LatLngVertex(1)
    self.assertIsInstance(second_vertex, s2.S2LatLng)
    self.assertEqual("51.500153,-0.126235", second_vertex.ToStringInDegrees())

  def testS2PolylineInitFromS2LatLngs(self):
    e7_10deg = 0x5f5e100
    list_ll = []
    for lat, lng in [(0, 0), (0, e7_10deg), (e7_10deg, e7_10deg)]:
      list_ll.append(s2.S2LatLng.FromE7(lat, lng))
    line = s2.S2Polyline()
    line.InitFromS2LatLngs(list_ll)
    self.assertAlmostEqual(20.0, line.GetLength().degrees())

  def testS2PolylineInitFromS2Points(self):
    e7_10deg = 0x5f5e100
    list_points = []
    for lat, lng in [(0, 0), (0, e7_10deg), (e7_10deg, e7_10deg)]:
      list_points.append(s2.S2LatLng.FromE7(lat, lng).ToPoint())
    line = s2.S2Polyline()
    line.InitFromS2Points(list_points)
    self.assertAlmostEqual(20.0, line.GetLength().degrees())

  def testS2PointsCanBeNormalized(self):
    line = s2.S2Polyline()
    line.InitFromS2LatLngs([s2.S2LatLng.FromDegrees(37.794484, -122.394871),
                            s2.S2LatLng.FromDegrees(37.762699, -122.435158)])
    self.assertNotAlmostEqual(line.GetCentroid().Norm(), 1.0)
    self.assertAlmostEqual(line.GetCentroid().Normalize().Norm(), 1.0)

  def testS1AngleComparsionIsWrappedCorrectly(self):
    ten_degrees = s2.S1Angle.Degrees(10)
    one_hundred_degrees = s2.S1Angle.Degrees(100)
    self.assertLess(ten_degrees, one_hundred_degrees)
    self.assertGreater(one_hundred_degrees, ten_degrees)

  def testS2PolygonIntersectsWithPolyline(self):
    london = s2.S2LatLng.FromDegrees(51.5001525, -0.1262355)
    polygon = s2.S2Polygon(s2.S2Cell(s2.S2CellId(london).parent(15)))
    line = s2.S2Polyline()
    line.InitFromS2LatLngs([s2.S2LatLng.FromDegrees(51.5, -0.128),
                            s2.S2LatLng.FromDegrees(51.5, -0.125)])
    intersections = polygon.IntersectWithPolyline(line)
    self.assertEqual(1, len(intersections))

  def testCrossingSign(self):
    a = s2.S2LatLng.FromDegrees(-1, 0).ToPoint()
    b = s2.S2LatLng.FromDegrees(1, 0).ToPoint()
    c = s2.S2LatLng.FromDegrees(0, -1).ToPoint()
    d = s2.S2LatLng.FromDegrees(0, 1).ToPoint()
    # SWIG flattens namespaces, so this is just s2.CrossingSign,
    # not s2.S2.CrossingSign.
    self.assertEqual(1, s2.CrossingSign(a, b, c, d))

  def testGetIntersection(self):
    a = s2.S2LatLng.FromDegrees(-1, 0).ToPoint()
    b = s2.S2LatLng.FromDegrees(1, 0).ToPoint()
    c = s2.S2LatLng.FromDegrees(0, -1).ToPoint()
    d = s2.S2LatLng.FromDegrees(0, 1).ToPoint()
    # SWIG namespace flattening as above.
    intersection = s2.GetIntersection(a, b, c, d)
    self.assertEqual(
        "0.000000,0.000000", s2.S2LatLng(intersection).ToStringInDegrees())

  def testS2CellDistance(self):
    # Level-0 cell (i.e. face) centered at (0, 0)
    cell = s2.S2Cell(s2.S2CellId(0x1000000000000000))

    p1 = s2.S2LatLng.FromDegrees(0, 0).ToPoint()
    self.assertTrue(cell.Contains(p1))
    d1 = cell.GetDistance(p1).ToAngle().degrees()
    # Inside, so distance is 0, but boundary distance is not.
    self.assertEqual(0.0, d1)
    bd1 = cell.GetBoundaryDistance(p1).ToAngle().degrees()
    self.assertEqual(45.0, bd1)

    p2 = s2.S2LatLng.FromDegrees(0, 90).ToPoint()
    self.assertFalse(cell.Contains(p2))
    d2 = cell.GetDistance(p2).ToAngle().degrees()
    self.assertAlmostEqual(45.0, d2)
    bd2 = cell.GetBoundaryDistance(p2).ToAngle().degrees()
    # Outside, so distance and boundary distance are the same.
    self.assertAlmostEqual(45.0, bd2)

  def testS2Rotate(self):
    mtv_a = s2.S2LatLng.FromDegrees(37.4402777, -121.9638888).ToPoint()
    mtv_b = s2.S2LatLng.FromDegrees(37.3613888, -121.9283333).ToPoint()
    angle = s2.S1Angle.Radians(0.039678)
    point = s2.Rotate(mtv_a, mtv_b, angle)
    self.assertEqual("37.439095,-121.967802",
                     s2.S2LatLng(point).ToStringInDegrees())

  def testS2TurnAngle(self):
    mtv_a = s2.S2LatLng.FromDegrees(37.4402777, -121.9638888).ToPoint()
    mtv_b = s2.S2LatLng.FromDegrees(37.3613888, -121.9283333).ToPoint()
    mtv_c = s2.S2LatLng.FromDegrees(37.3447222, -122.0308333).ToPoint()
    angle = s2.TurnAngle(mtv_a, mtv_b, mtv_c)
    self.assertAlmostEqual(-1.7132025, angle)

  def testEncodeDecode(self):
    london = s2.S2LatLng.FromDegrees(51.5001525, -0.1262355)
    polygon = s2.S2Polygon(s2.S2Cell(s2.S2CellId(london).parent(15)))
    self.assertEqual(polygon.num_loops(), 1)

    encoder = s2.Encoder()
    polygon.Encode(encoder)

    encoded = encoder.buffer()
    decoder = s2.Decoder(encoded)
    decoded_polygon = s2.S2Polygon()
    self.assertTrue(decoded_polygon.Decode(decoder))

    self.assertEqual(decoded_polygon.num_loops(), 1)
    self.assertTrue(decoded_polygon.Equals(polygon))

  def testS2CapRegion(self):
    center = s2.S2LatLng.FromDegrees(2.0, 3.0).ToPoint()
    cap = s2.S2Cap(center, s2.S1Angle.Degrees(1.0))

    inside = s2.S2LatLng.FromDegrees(2.1, 2.9).ToPoint()
    outside = s2.S2LatLng.FromDegrees(0.0, 0.0).ToPoint()
    self.assertTrue(cap.Contains(inside))
    self.assertFalse(cap.Contains(outside))
    self.assertTrue(cap.Contains(s2.S2Cell(inside)))
    self.assertFalse(cap.Contains(s2.S2Cell(outside)))
    self.assertTrue(cap.MayIntersect(s2.S2Cell(inside)))
    self.assertFalse(cap.MayIntersect(s2.S2Cell(outside)))

    self.assertTrue(cap.ApproxEquals(cap.GetCapBound()))

    rect_bound = cap.GetRectBound()
    self.assertTrue(rect_bound.Contains(inside))
    self.assertFalse(rect_bound.Contains(outside))

  def testS2LatLngRectRegion(self):
    rect = s2.S2LatLngRect(s2.S2LatLng.FromDegrees(1.0, 2.0),
                           s2.S2LatLng.FromDegrees(3.0, 4.0))

    inside = s2.S2LatLng.FromDegrees(2.0, 3.0).ToPoint()
    outside = s2.S2LatLng.FromDegrees(0.0, 0.0).ToPoint()

    self.assertTrue(rect.Contains(inside))
    self.assertFalse(rect.Contains(outside))
    self.assertTrue(rect.Contains(s2.S2Cell(inside)))
    self.assertFalse(rect.Contains(s2.S2Cell(outside)))
    self.assertTrue(rect.MayIntersect(s2.S2Cell(inside)))
    self.assertFalse(rect.MayIntersect(s2.S2Cell(outside)))

    cap_bound = rect.GetCapBound()
    self.assertTrue(cap_bound.Contains(inside))
    self.assertFalse(cap_bound.Contains(outside))

    self.assertTrue(rect.ApproxEquals(rect.GetRectBound()))

  def testS2CellRegion(self):
    cell = s2.S2Cell(s2.S2CellId(s2.S2LatLng.FromDegrees(3.0, 4.0)).parent(8))

    inside = s2.S2LatLng.FromDegrees(3.0, 4.0).ToPoint()
    outside = s2.S2LatLng.FromDegrees(30.0, 40.0).ToPoint()

    self.assertTrue(cell.Contains(inside))
    self.assertFalse(cell.Contains(outside))
    self.assertTrue(cell.Contains(s2.S2Cell(inside)))
    self.assertFalse(cell.Contains(s2.S2Cell(outside)))
    self.assertTrue(cell.MayIntersect(s2.S2Cell(inside)))
    self.assertFalse(cell.MayIntersect(s2.S2Cell(outside)))

    cap_bound = cell.GetCapBound()
    self.assertTrue(cap_bound.Contains(inside))
    self.assertFalse(cap_bound.Contains(outside))

    rect_bound = cell.GetRectBound()
    self.assertTrue(rect_bound.Contains(inside))
    self.assertFalse(rect_bound.Contains(outside))

  def testS2CellUnionRegion(self):
    cell_id = s2.S2CellId(s2.S2LatLng.FromDegrees(3.0, 4.0)).parent(8)
    cell_union = s2.S2CellUnion()
    cell_union.Init([cell_id.id()])

    inside = s2.S2LatLng.FromDegrees(3.0, 4.0).ToPoint()
    outside = s2.S2LatLng.FromDegrees(30.0, 40.0).ToPoint()

    self.assertTrue(cell_union.Contains(inside))
    self.assertFalse(cell_union.Contains(outside))
    self.assertTrue(cell_union.Contains(s2.S2Cell(inside)))
    self.assertFalse(cell_union.Contains(s2.S2Cell(outside)))
    self.assertTrue(cell_union.MayIntersect(s2.S2Cell(inside)))
    self.assertFalse(cell_union.MayIntersect(s2.S2Cell(outside)))

    cap_bound = cell_union.GetCapBound()
    self.assertTrue(cap_bound.Contains(inside))
    self.assertFalse(cap_bound.Contains(outside))

    rect_bound = cell_union.GetRectBound()
    self.assertTrue(rect_bound.Contains(inside))
    self.assertFalse(rect_bound.Contains(outside))

  def testS2LoopRegion(self):
    cell = s2.S2Cell(s2.S2CellId(s2.S2LatLng.FromDegrees(3.0, 4.0)).parent(8))
    loop = s2.S2Loop(cell)

    inside = s2.S2LatLng.FromDegrees(3.0, 4.0).ToPoint()
    outside = s2.S2LatLng.FromDegrees(30.0, 40.0).ToPoint()

    self.assertTrue(loop.Contains(inside))
    self.assertFalse(loop.Contains(outside))
    self.assertTrue(loop.Contains(s2.S2Cell(inside)))
    self.assertFalse(loop.Contains(s2.S2Cell(outside)))
    self.assertTrue(loop.MayIntersect(s2.S2Cell(inside)))
    self.assertFalse(loop.MayIntersect(s2.S2Cell(outside)))

    cap_bound = loop.GetCapBound()
    self.assertTrue(cap_bound.Contains(inside))
    self.assertFalse(cap_bound.Contains(outside))

    rect_bound = loop.GetRectBound()
    self.assertTrue(rect_bound.Contains(inside))
    self.assertFalse(rect_bound.Contains(outside))

  def testS2PolygonRegion(self):
    cell = s2.S2Cell(s2.S2CellId(s2.S2LatLng.FromDegrees(3.0, 4.0)).parent(8))
    polygon = s2.S2Polygon(cell)

    inside = s2.S2LatLng.FromDegrees(3.0, 4.0).ToPoint()
    outside = s2.S2LatLng.FromDegrees(30.0, 40.0).ToPoint()

    self.assertTrue(polygon.Contains(inside))
    self.assertFalse(polygon.Contains(outside))
    self.assertTrue(polygon.Contains(s2.S2Cell(inside)))
    self.assertFalse(polygon.Contains(s2.S2Cell(outside)))
    self.assertTrue(polygon.MayIntersect(s2.S2Cell(inside)))
    self.assertFalse(polygon.MayIntersect(s2.S2Cell(outside)))

    cap_bound = polygon.GetCapBound()
    self.assertTrue(cap_bound.Contains(inside))
    self.assertFalse(cap_bound.Contains(outside))

    rect_bound = polygon.GetRectBound()
    self.assertTrue(rect_bound.Contains(inside))
    self.assertFalse(rect_bound.Contains(outside))

  def testS2PolylineRegion(self):
    polyline = s2.S2Polyline()
    polyline.InitFromS2LatLngs([s2.S2LatLng.FromDegrees(0.0, 0.0),
                                s2.S2LatLng.FromDegrees(1.0, 1.0)])

    # Contains(S2Point) always return false.
    self.assertFalse(
        polyline.Contains(s2.S2LatLng.FromDegrees(0.0, 0.0).ToPoint()))
    self.assertFalse(
        polyline.Contains(s2.S2Cell(s2.S2LatLng.FromDegrees(0.0, 0.0))))

    self.assertTrue(
        polyline.MayIntersect(s2.S2Cell(s2.S2LatLng.FromDegrees(0.0, 0.0))))
    self.assertFalse(
        polyline.MayIntersect(s2.S2Cell(s2.S2LatLng.FromDegrees(3.0, 4.0))))

    cap_bound = polyline.GetCapBound()
    self.assertTrue(
        cap_bound.Contains(s2.S2LatLng.FromDegrees(0.0, 0.0).ToPoint()))
    self.assertFalse(
        cap_bound.Contains(s2.S2LatLng.FromDegrees(2.0, 2.0).ToPoint()))

    rect_bound = polyline.GetRectBound()
    self.assertTrue(
        rect_bound.Contains(s2.S2LatLng.FromDegrees(0.0, 0.0).ToPoint()))
    self.assertFalse(
        rect_bound.Contains(s2.S2LatLng.FromDegrees(2.0, 2.0).ToPoint()))

  def testS2CellIdCenterSiTi(self):
    cell = s2.S2CellId.FromFacePosLevel(3, 0x12345678, s2.S2CellId.kMaxLevel)

    # Check that the (si, ti) coordinates of the center end in a
    # 1 followed by (30 - level) 0s.

    # Leaf level, 30.
    face, si, ti = cell.GetCenterSiTi()
    self.assertEqual(3, face)
    self.assertEqual(1 << 0, si & 1)
    self.assertEqual(1 << 0, ti & 1)

    # Level 29.
    face, si, ti = cell.parent(s2.S2CellId.kMaxLevel - 1).GetCenterSiTi()
    self.assertEqual(3, face)
    self.assertEqual(1 << 1, si & 3)
    self.assertEqual(1 << 1, ti & 3)

    # Level 28.
    face, si, ti = cell.parent(s2.S2CellId.kMaxLevel - 2).GetCenterSiTi()
    self.assertEqual(3, face)
    self.assertEqual(1 << 2, si & 7)
    self.assertEqual(1 << 2, ti & 7)

    # Level 20.
    face, si, ti = cell.parent(s2.S2CellId.kMaxLevel - 10).GetCenterSiTi()
    self.assertEqual(3, face)
    self.assertEqual(1 << 10, si & ((1 << 11) - 1))
    self.assertEqual(1 << 10, ti & ((1 << 11) - 1))

    # Level 10.
    face, si, ti = cell.parent(s2.S2CellId.kMaxLevel - 20).GetCenterSiTi()
    self.assertEqual(3, face)
    self.assertEqual(1 << 20, si & ((1 << 21) - 1))
    self.assertEqual(1 << 20, ti & ((1 << 21) - 1))

    # Level 0.
    face, si, ti = cell.parent(0).GetCenterSiTi()
    self.assertEqual(3, face)
    self.assertEqual(1 << 30, si & ((1 << 31) - 1))
    self.assertEqual(1 << 30, ti & ((1 << 31) - 1))

  def testS2CellIdToFromFaceIJ(self):
    cell = s2.S2CellId.FromFaceIJ(3, 1234, 5678)
    face, i, j, _ = cell.ToFaceIJOrientation()
    self.assertEqual(3, face)
    self.assertEqual(1234, i)
    self.assertEqual(5678, j)

if __name__ == "__main__":
  unittest.main()
