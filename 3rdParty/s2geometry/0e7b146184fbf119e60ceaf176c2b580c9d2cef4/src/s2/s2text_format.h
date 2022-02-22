// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef S2_S2TEXT_FORMAT_H_
#define S2_S2TEXT_FORMAT_H_

// s2text_format contains a collection of functions for converting
// geometry to and from a human-readable format.  It is mainly
// intended for testing and debugging.  Be aware that the
// human-readable format is *not* designed to preserve the full
// precision of the original object, so it should not be used
// for data storage.

#include <memory>
#include <string>
#include <vector>

#include "s2/third_party/absl/base/attributes.h"
#include "s2/third_party/absl/strings/string_view.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"
#include "s2/s2debug.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2point_span.h"

class MutableS2ShapeIndex;
class S2LaxPolygonShape;
class S2LaxPolylineShape;
class S2Loop;
class S2Polygon;
class S2Polyline;
class S2ShapeIndex;

namespace s2textformat {

// Returns an S2Point corresponding to the given a latitude-longitude
// coordinate in degrees.  Example of the input format:
//     "-20:150"
S2Point MakePointOrDie(absl::string_view str);

// As above, but do not S2_CHECK-fail on invalid input. Returns true if conversion
// is successful.
ABSL_MUST_USE_RESULT bool MakePoint(absl::string_view str, S2Point* point);

ABSL_DEPRECATED("Use MakePointOrDie.")
inline S2Point MakePoint(absl::string_view str) { return MakePointOrDie(str); }

// Parses a string of one or more latitude-longitude coordinates in degrees,
// and return the corresponding vector of S2LatLng points.
// Examples of the input format:
//     ""                            // no points
//     "-20:150"                     // one point
//     "-20:150, -20:151, -19:150"   // three points
std::vector<S2LatLng> ParseLatLngsOrDie(absl::string_view str);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool ParseLatLngs(absl::string_view str,
                                       std::vector<S2LatLng>* latlngs);

ABSL_DEPRECATED("Use ParseLatLngsOrDie.")
inline std::vector<S2LatLng> ParseLatLngs(absl::string_view str) {
  return ParseLatLngsOrDie(str);
}

// Parses a string in the same format as ParseLatLngs, and return the
// corresponding vector of S2Point values.
std::vector<S2Point> ParsePointsOrDie(absl::string_view str);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool ParsePoints(absl::string_view str,
                                      std::vector<S2Point>* vertices);

ABSL_DEPRECATED("Use ParsePointsOrDie.")
inline std::vector<S2Point> ParsePoints(absl::string_view str) {
  return ParsePointsOrDie(str);
}

// Given a string in the same format as ParseLatLngs, returns a single S2LatLng.
S2LatLng MakeLatLngOrDie(absl::string_view str);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool MakeLatLng(absl::string_view str, S2LatLng* latlng);

// Given a string in the same format as ParseLatLngs, returns the minimal
// bounding S2LatLngRect that contains the coordinates.
S2LatLngRect MakeLatLngRectOrDie(absl::string_view str);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool MakeLatLngRect(absl::string_view str,
                                         S2LatLngRect* rect);

ABSL_DEPRECATED("Use MakeLatLngRectOrDie.")
inline S2LatLngRect MakeLatLngRect(absl::string_view str) {
  return MakeLatLngRectOrDie(str);
}

// Parses an S2CellId in the format "f/dd..d" where "f" is a digit in the
// range [0-5] representing the S2CellId face, and "dd..d" is a string of
// digits in the range [0-3] representing each child's position with respect
// to its parent.  (Note that the latter string may be empty.)
//
// For example "4/" represents S2CellId::FromFace(4), and "3/02" represents
// S2CellId::FromFace(3).child(0).child(2).
//
// This function is a wrapper for S2CellId::FromDebugString().
S2CellId MakeCellIdOrDie(absl::string_view str);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool MakeCellId(absl::string_view str, S2CellId* cell_id);

// Parses a comma-separated list of S2CellIds in the format above, and returns
// the corresponding S2CellUnion.  (Note that S2CellUnions are automatically
// normalized by sorting, removing duplicates, and replacing groups of 4 child
// cells by their parent cell.)
S2CellUnion MakeCellUnionOrDie(absl::string_view str);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool MakeCellUnion(absl::string_view str,
                                        S2CellUnion* cell_union);

// Given a string of latitude-longitude coordinates in degrees,
// returns a newly allocated loop.  Example of the input format:
//     "-20:150, 10:-120, 0.123:-170.652"
// The strings "empty" or "full" create an empty or full loop respectively.
std::unique_ptr<S2Loop> MakeLoopOrDie(absl::string_view str,
                                      S2Debug debug_override = S2Debug::ALLOW);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool MakeLoop(absl::string_view str,
                                   std::unique_ptr<S2Loop>* loop,
                                   S2Debug debug_override = S2Debug::ALLOW);

ABSL_DEPRECATED("Use MakeLoopOrDie.")
std::unique_ptr<S2Loop> MakeLoop(absl::string_view str,
                                 S2Debug debug_override = S2Debug::ALLOW);

// Similar to MakeLoop(), but returns an S2Polyline rather than an S2Loop.
std::unique_ptr<S2Polyline> MakePolylineOrDie(
    absl::string_view str,
    S2Debug debug_override = S2Debug::ALLOW);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool MakePolyline(absl::string_view str,
                                       std::unique_ptr<S2Polyline>* polyline,
                                       S2Debug debug_override = S2Debug::ALLOW);

ABSL_DEPRECATED("Use MakePolylineOrDie.")
std::unique_ptr<S2Polyline> MakePolyline(
    absl::string_view str,
    S2Debug debug_override = S2Debug::ALLOW);

// Like MakePolyline, but returns an S2LaxPolylineShape instead.
std::unique_ptr<S2LaxPolylineShape> MakeLaxPolylineOrDie(absl::string_view str);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool MakeLaxPolyline(
    absl::string_view str, std::unique_ptr<S2LaxPolylineShape>* lax_polyline);

ABSL_DEPRECATED("Use MakeLaxPolylineOrDie.")
std::unique_ptr<S2LaxPolylineShape> MakeLaxPolyline(absl::string_view str);

// Given a sequence of loops separated by semicolons, returns a newly
// allocated polygon.  Loops are automatically normalized by inverting them
// if necessary so that they enclose at most half of the unit sphere.
// (Historically this was once a requirement of polygon loops.  It also
// hides the problem that if the user thinks of the coordinates as X:Y
// rather than LAT:LNG, it yields a loop with the opposite orientation.)
//
// Examples of the input format:
//     "10:20, 90:0, 20:30"                                  // one loop
//     "10:20, 90:0, 20:30; 5.5:6.5, -90:-180, -15.2:20.3"   // two loops
//     ""       // the empty polygon (consisting of no loops)
//     "empty"  // the empty polygon (consisting of no loops)
//     "full"   // the full polygon (consisting of one full loop).
std::unique_ptr<S2Polygon> MakePolygonOrDie(
    absl::string_view str,
    S2Debug debug_override = S2Debug::ALLOW);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool MakePolygon(absl::string_view str,
                                      std::unique_ptr<S2Polygon>* polygon,
                                      S2Debug debug_override = S2Debug::ALLOW);

ABSL_DEPRECATED("Use MakePolygonOrDie.")
std::unique_ptr<S2Polygon> MakePolygon(absl::string_view str,
                                       S2Debug debug_override = S2Debug::ALLOW);

// Like MakePolygon(), except that it does not normalize loops (i.e., it
// gives you exactly what you asked for).
std::unique_ptr<S2Polygon> MakeVerbatimPolygonOrDie(absl::string_view str);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool MakeVerbatimPolygon(
    absl::string_view str, std::unique_ptr<S2Polygon>* polygon);

ABSL_DEPRECATED("Use MakeVerbatimPolygonOrDie.")
std::unique_ptr<S2Polygon> MakeVerbatimPolygon(absl::string_view str);

// Parses a string in the same format as MakePolygon, except that loops must
// be oriented so that the interior of the loop is always on the left, and
// polygons with degeneracies are supported.  As with MakePolygon, "full" and
// denotes the full polygon and "" or "empty" denote the empty polygon.
std::unique_ptr<S2LaxPolygonShape> MakeLaxPolygonOrDie(absl::string_view str);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool MakeLaxPolygon(
    absl::string_view str, std::unique_ptr<S2LaxPolygonShape>* lax_polygon);

ABSL_DEPRECATED("Use MakeLaxPolygonOrDie.")
std::unique_ptr<S2LaxPolygonShape> MakeLaxPolygon(absl::string_view str);

// Returns a MutableS2ShapeIndex containing the points, polylines, and loops
// (in the form of a single polygon) described by the following format:
//
//   point1|point2|... # line1|line2|... # polygon1|polygon2|...
//
// Examples:
//   1:2 | 2:3 # #                     // Two points
//   # 0:0, 1:1, 2:2 | 3:3, 4:4 #      // Two polylines
//   # # 0:0, 0:3, 3:0; 1:1, 2:1, 1:2  // Two nested loops (one polygon)
//   5:5 # 6:6, 7:7 # 0:0, 0:1, 1:0    // One of each
//   # # empty                         // One empty polygon
//   # # empty | full                  // One empty polygon, one full polygon
//
// Loops should be directed so that the region's interior is on the left.
// Loops can be degenerate (they do not need to meet S2Loop requirements).
//
// CAVEAT: Because whitespace is ignored, empty polygons must be specified
//         as the string "empty" rather than as the empty string ("").
std::unique_ptr<MutableS2ShapeIndex> MakeIndexOrDie(absl::string_view str);

// As above, but does not S2_CHECK-fail on invalid input. Returns true if
// conversion is successful.
ABSL_MUST_USE_RESULT bool MakeIndex(
    absl::string_view str, std::unique_ptr<MutableS2ShapeIndex>* index);

ABSL_DEPRECATED("Use MakeIndexOrDie.")
std::unique_ptr<MutableS2ShapeIndex> MakeIndex(absl::string_view str);

// Convert an S2Point, S2LatLng, S2LatLngRect, S2CellId, S2CellUnion, loop,
// polyline, or polygon to the string format above.
string ToString(const S2Point& point);
string ToString(const S2LatLng& latlng);
string ToString(const S2LatLngRect& rect);
string ToString(const S2CellId& cell_id);
string ToString(const S2CellUnion& cell_union);
string ToString(const S2Loop& loop);
string ToString(S2PointLoopSpan loop);
string ToString(const S2Polyline& polyline);
string ToString(const S2Polygon& polygon, const char* loop_separator = ";\n");
string ToString(const std::vector<S2Point>& points);
string ToString(const std::vector<S2LatLng>& points);
string ToString(const S2LaxPolylineShape& polyline);
string ToString(const S2LaxPolygonShape& polygon,
                const char* loop_separator = ";\n");

// Convert the contents of an S2ShapeIndex to the format above.  The index may
// contain S2Shapes of any type.  Shapes are reordered if necessary so that
// all point geometry (shapes of dimension 0) are first, followed by all
// polyline geometry, followed by all polygon geometry.
string ToString(const S2ShapeIndex& index);

}  // namespace s2textformat

#endif  // S2_S2TEXT_FORMAT_H_
