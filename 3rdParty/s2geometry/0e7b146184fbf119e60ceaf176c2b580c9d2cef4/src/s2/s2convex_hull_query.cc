// Copyright 2015 Google Inc. All Rights Reserved.
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

// Author: ericv@google.com (Eric Veach)
//
// This implement Andrew's monotone chain algorithm, which is a variant of the
// Graham scan (see https://en.wikipedia.org/wiki/Graham_scan).  The time
// complexity is O(n log n), and the space required is O(n).  In fact only the
// call to "sort" takes O(n log n) time; the rest of the algorithm is linear.
//
// Demonstration of the algorithm and code:
// en.wikibooks.org/wiki/Algorithm_Implementation/Geometry/Convex_hull/Monotone_chain

#include "s2/s2convex_hull_query.h"

#include "s2/third_party/absl/memory/memory.h"
#include "s2/s2pointutil.h"
#include "s2/s2predicates.h"

using absl::make_unique;
using std::unique_ptr;
using std::vector;

S2ConvexHullQuery::S2ConvexHullQuery()
    : bound_(S2LatLngRect::Empty()), points_() {
}

void S2ConvexHullQuery::AddPoint(const S2Point& point) {
  bound_.AddPoint(point);
  points_.push_back(point);
}

void S2ConvexHullQuery::AddPolyline(const S2Polyline& polyline) {
  bound_ = bound_.Union(polyline.GetRectBound());
  for (int i = 0; i < polyline.num_vertices(); ++i) {
    points_.push_back(polyline.vertex(i));
  }
}

void S2ConvexHullQuery::AddLoop(const S2Loop& loop) {
  bound_ = bound_.Union(loop.GetRectBound());
  if (loop.is_empty_or_full()) {
    // The empty and full loops consist of a single fake "vertex" that should
    // not be added to our point collection.
    return;
  }
  for (int i = 0; i < loop.num_vertices(); ++i) {
    points_.push_back(loop.vertex(i));
  }
}

void S2ConvexHullQuery::AddPolygon(const S2Polygon& polygon) {
  for (int i = 0; i < polygon.num_loops(); ++i) {
    const S2Loop& loop = *polygon.loop(i);
    // Only loops at depth 0 can contribute to the convex hull.
    if (loop.depth() == 0) {
      AddLoop(loop);
    }
  }
}

S2Cap S2ConvexHullQuery::GetCapBound() {
  // We keep track of a rectangular bound rather than a spherical cap because
  // it is easy to compute a tight bound for a union of rectangles, whereas it
  // is quite difficult to compute a tight bound around a union of caps.
  // Also, polygons and polylines implement GetCapBound() in terms of
  // GetRectBound() for this same reason, so it is much better to keep track
  // of a rectangular bound as we go along and convert it at the end.
  //
  // TODO(ericv): We could compute an optimal bound by implementing Welzl's
  // algorithm.  However we would still need to have special handling of loops
  // and polygons, since if a loop spans more than 180 degrees in any
  // direction (i.e., if it contains two antipodal points), then it is not
  // enough just to bound its vertices.  In this case the only convex bounding
  // cap is S2Cap::Full(), and the only convex bounding loop is the full loop.
  return bound_.GetCapBound();
}

// A comparator for sorting points in CCW around a central point "center".
class OrderedCcwAround {
 public:
  explicit OrderedCcwAround(const S2Point& center) : center_(center) {}
  bool operator()(const S2Point& x, const S2Point& y) const {
    // If X and Y are equal, this will return false (as desired).
    return s2pred::Sign(center_, x, y) > 0;
  }
 private:
  S2Point center_;
};

unique_ptr<S2Loop> S2ConvexHullQuery::GetConvexHull() {
  S2Cap cap = GetCapBound();
  if (cap.height() >= 1) {
    // The bounding cap is not convex.  The current bounding cap
    // implementation is not optimal, but nevertheless it is likely that the
    // input geometry itself is not contained by any convex polygon.  In any
    // case, we need a convex bounding cap to proceed with the algorithm below
    // (in order to construct a point "origin" that is definitely outside the
    // convex hull).
    return make_unique<S2Loop>(S2Loop::kFull());
  }
  // This code implements Andrew's monotone chain algorithm, which is a simple
  // variant of the Graham scan.  Rather than sorting by x-coordinate, instead
  // we sort the points in CCW order around an origin O such that all points
  // are guaranteed to be on one side of some geodesic through O.  This
  // ensures that as we scan through the points, each new point can only
  // belong at the end of the chain (i.e., the chain is monotone in terms of
  // the angle around O from the starting point).
  S2Point origin = cap.center().Ortho();
  std::sort(points_.begin(), points_.end(), OrderedCcwAround(origin));

  // Remove duplicates.  We need to do this before checking whether there are
  // fewer than 3 points.
  points_.erase(std::unique(points_.begin(), points_.end()), points_.end());

  // Special cases for fewer than 3 points.
  if (points_.size() < 3) {
    if (points_.empty()) {
      return make_unique<S2Loop>(S2Loop::kEmpty());
    } else if (points_.size() == 1) {
      return GetSinglePointLoop(points_[0]);
    } else {
      return GetSingleEdgeLoop(points_[0], points_[1]);
    }
  }

  // Verify that all points lie within a 180 degree span around the origin.
  S2_DCHECK_GE(s2pred::Sign(origin, points_.front(), points_.back()), 0);

  // Generate the lower and upper halves of the convex hull.  Each half
  // consists of the maximal subset of vertices such that the edge chain makes
  // only left (CCW) turns.
  vector<S2Point> lower, upper;
  GetMonotoneChain(&lower);
  std::reverse(points_.begin(), points_.end());
  GetMonotoneChain(&upper);

  // Remove the duplicate vertices and combine the chains.
  S2_DCHECK_EQ(lower.front(), upper.back());
  S2_DCHECK_EQ(lower.back(), upper.front());
  lower.pop_back();
  upper.pop_back();
  lower.insert(lower.end(), upper.begin(), upper.end());
  return make_unique<S2Loop>(lower);
}

// Iterate through the given points, selecting the maximal subset of points
// such that the edge chain makes only left (CCW) turns.
void S2ConvexHullQuery::GetMonotoneChain(vector<S2Point>* output) {
  S2_DCHECK(output->empty());
  for (const S2Point& p : points_) {
    // Remove any points that would cause the chain to make a clockwise turn.
    while (output->size() >= 2 &&
           s2pred::Sign(output->end()[-2], output->back(), p) <= 0) {
      output->pop_back();
    }
    output->push_back(p);
  }
}

unique_ptr<S2Loop> S2ConvexHullQuery::GetSinglePointLoop(const S2Point& p) {
  // Construct a 3-vertex polygon consisting of "p" and two nearby vertices.
  // Note that Contains(p) may be false for the resulting loop (see comments
  // in header file).
  static const double kOffset = 1e-15;
  S2Point d0 = S2::Ortho(p);
  S2Point d1 = p.CrossProd(d0);
  vector<S2Point> vertices;
  vertices.push_back(p);
  vertices.push_back((p + kOffset * d0).Normalize());
  vertices.push_back((p + kOffset * d1).Normalize());
  return make_unique<S2Loop>(vertices);
}

unique_ptr<S2Loop> S2ConvexHullQuery::GetSingleEdgeLoop(const S2Point& a,
                                                        const S2Point& b) {
  // Construct a loop consisting of the two vertices and their midpoint.
  vector<S2Point> vertices;
  vertices.push_back(a);
  vertices.push_back(b);
  vertices.push_back((a + b).Normalize());
  auto loop = make_unique<S2Loop>(vertices);
  // The resulting loop may be clockwise, so invert it if necessary.
  loop->Normalize();
  return loop;
}
