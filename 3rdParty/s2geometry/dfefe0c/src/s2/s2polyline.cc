// Copyright 2005 Google Inc. All Rights Reserved.
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

#include "s2/s2polyline.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <utility>
#include <vector>

#include "s2/base/commandlineflags.h"
#include "s2/base/logging.h"
#include "s2/third_party/absl/utility/utility.h"
#include "s2/util/coding/coder.h"
#include "s2/s1angle.h"
#include "s2/s1interval.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2debug.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2edge_distances.h"
#include "s2/s2error.h"
#include "s2/s2latlng_rect_bounder.h"
#include "s2/s2pointutil.h"
#include "s2/s2polyline_measures.h"
#include "s2/s2predicates.h"
#include "s2/util/math/matrix3x3.h"

using std::max;
using std::min;
using std::set;
using std::vector;

static const unsigned char kCurrentLosslessEncodingVersionNumber = 1;

S2Polyline::S2Polyline()
  : s2debug_override_(S2Debug::ALLOW) {}

S2Polyline::S2Polyline(S2Polyline&& other)
  : s2debug_override_(other.s2debug_override_),
    num_vertices_(absl::exchange(other.num_vertices_, 0)),
    vertices_(std::move(other.vertices_)) {
}

S2Polyline& S2Polyline::operator=(S2Polyline&& other) {
  s2debug_override_ = other.s2debug_override_;
  num_vertices_ = absl::exchange(other.num_vertices_, 0);
  vertices_ = std::move(other.vertices_);
  return *this;
}

S2Polyline::S2Polyline(const vector<S2Point>& vertices)
  : S2Polyline(vertices, S2Debug::ALLOW) {}

S2Polyline::S2Polyline(const vector<S2LatLng>& vertices)
  : S2Polyline(vertices, S2Debug::ALLOW) {}

S2Polyline::S2Polyline(const vector<S2Point>& vertices,
                       S2Debug override)
  : s2debug_override_(override) {
  Init(vertices);
}

S2Polyline::S2Polyline(const vector<S2LatLng>& vertices,
                       S2Debug override)
  : s2debug_override_(override) {
  Init(vertices);
}

S2Polyline::~S2Polyline() {
}

void S2Polyline::set_s2debug_override(S2Debug override) {
  s2debug_override_ = override;
}

S2Debug S2Polyline::s2debug_override() const {
  return s2debug_override_;
}

void S2Polyline::Init(const vector<S2Point>& vertices) {
  num_vertices_ = vertices.size();
  vertices_.reset(new S2Point[num_vertices_]);
  std::copy(vertices.begin(), vertices.end(), &vertices_[0]);
  if (FLAGS_s2debug && s2debug_override_ == S2Debug::ALLOW) {
    S2_CHECK(IsValid());
  }
}

void S2Polyline::Init(const vector<S2LatLng>& vertices) {
  num_vertices_ = vertices.size();
  vertices_.reset(new S2Point[num_vertices_]);
  for (int i = 0; i < num_vertices_; ++i) {
    vertices_[i] = vertices[i].ToPoint();
  }
  if (FLAGS_s2debug && s2debug_override_ == S2Debug::ALLOW) {
    S2_CHECK(IsValid());
  }
}

bool S2Polyline::IsValid() const {
  S2Error error;
  if (FindValidationError(&error)) {
    S2_LOG_IF(ERROR, FLAGS_s2debug) << error;
    return false;
  }
  return true;
}

bool S2Polyline::FindValidationError(S2Error* error) const {
  // All vertices must be unit length.
  for (int i = 0; i < num_vertices(); ++i) {
    if (!S2::IsUnitLength(vertex(i))) {
      error->Init(S2Error::NOT_UNIT_LENGTH, "Vertex %d is not unit length", i);
      return true;
    }
  }
  // Adjacent vertices must not be identical or antipodal.
  for (int i = 1; i < num_vertices(); ++i) {
    if (vertex(i - 1) == vertex(i)) {
      error->Init(S2Error::DUPLICATE_VERTICES,
                  "Vertices %d and %d are identical", i - 1, i);
      return true;
    }
    if (vertex(i - 1) == -vertex(i)) {
      error->Init(S2Error::ANTIPODAL_VERTICES,
                  "Vertices %d and %d are antipodal", i - 1, i);
      return true;
    }
  }
  return false;
}

S2Polyline::S2Polyline(const S2Polyline& src)
  : num_vertices_(src.num_vertices_),
    vertices_(new S2Point[num_vertices_]) {
  std::copy(&src.vertices_[0], &src.vertices_[num_vertices_], &vertices_[0]);
}

S2Polyline* S2Polyline::Clone() const {
  return new S2Polyline(*this);
}

S1Angle S2Polyline::GetLength() const {
  return S2::GetLength(S2PointSpan(&vertices_[0], num_vertices_));
}

S2Point S2Polyline::GetCentroid() const {
  return S2::GetCentroid(S2PointSpan(&vertices_[0], num_vertices_));
}

S2Point S2Polyline::GetSuffix(double fraction, int* next_vertex) const {
  S2_DCHECK_GT(num_vertices(), 0);
  // We intentionally let the (fraction >= 1) case fall through, since
  // we need to handle it in the loop below in any case because of
  // possible roundoff errors.
  if (fraction <= 0) {
    *next_vertex = 1;
    return vertex(0);
  }
  S1Angle length_sum;
  for (int i = 1; i < num_vertices(); ++i) {
    length_sum += S1Angle(vertex(i-1), vertex(i));
  }
  S1Angle target = fraction * length_sum;
  for (int i = 1; i < num_vertices(); ++i) {
    S1Angle length(vertex(i-1), vertex(i));
    if (target < length) {
      // This interpolates with respect to arc length rather than
      // straight-line distance, and produces a unit-length result.
      S2Point result = S2::InterpolateAtDistance(target, vertex(i-1),
                                                         vertex(i));
      // It is possible that (result == vertex(i)) due to rounding errors.
      *next_vertex = (result == vertex(i)) ? (i + 1) : i;
      return result;
    }
    target -= length;
  }
  *next_vertex = num_vertices();
  return vertex(num_vertices() - 1);
}

S2Point S2Polyline::Interpolate(double fraction) const {
  int next_vertex;
  return GetSuffix(fraction, &next_vertex);
}

double S2Polyline::UnInterpolate(const S2Point& point, int next_vertex) const {
  S2_DCHECK_GT(num_vertices(), 0);
  if (num_vertices() < 2) {
    return 0;
  }
  S1Angle length_sum;
  for (int i = 1; i < next_vertex; ++i) {
    length_sum += S1Angle(vertex(i-1), vertex(i));
  }
  S1Angle length_to_point = length_sum + S1Angle(vertex(next_vertex-1), point);
  for (int i = next_vertex; i < num_vertices(); ++i) {
    length_sum += S1Angle(vertex(i-1), vertex(i));
  }
  // The ratio can be greater than 1.0 due to rounding errors or because the
  // point is not exactly on the polyline.
  return min(1.0, length_to_point / length_sum);
}

S2Point S2Polyline::Project(const S2Point& point, int* next_vertex) const {
  S2_DCHECK_GT(num_vertices(), 0);

  if (num_vertices() == 1) {
    // If there is only one vertex, it is always closest to any given point.
    *next_vertex = 1;
    return vertex(0);
  }

  // Initial value larger than any possible distance on the unit sphere.
  S1Angle min_distance = S1Angle::Radians(10);
  int min_index = -1;

  // Find the line segment in the polyline that is closest to the point given.
  for (int i = 1; i < num_vertices(); ++i) {
    S1Angle distance_to_segment = S2::GetDistance(point, vertex(i-1),
                                                          vertex(i));
    if (distance_to_segment < min_distance) {
      min_distance = distance_to_segment;
      min_index = i;
    }
  }
  S2_DCHECK_NE(min_index, -1);

  // Compute the point on the segment found that is closest to the point given.
  S2Point closest_point =
      S2::Project(point, vertex(min_index - 1), vertex(min_index));

  *next_vertex = min_index + (closest_point == vertex(min_index) ? 1 : 0);
  return closest_point;
}

bool S2Polyline::IsOnRight(const S2Point& point) const {
  S2_DCHECK_GE(num_vertices(), 2);

  int next_vertex;
  S2Point closest_point = Project(point, &next_vertex);

  S2_DCHECK_GE(next_vertex, 1);
  S2_DCHECK_LE(next_vertex, num_vertices());

  // If the closest point C is an interior vertex of the polyline, let B and D
  // be the previous and next vertices.  The given point P is on the right of
  // the polyline (locally) if B, P, D are ordered CCW around vertex C.
  if (closest_point == vertex(next_vertex-1) && next_vertex > 1 &&
      next_vertex < num_vertices()) {
    if (point == vertex(next_vertex-1))
      return false;  // Polyline vertices are not on the RHS.
    return s2pred::OrderedCCW(vertex(next_vertex-2), point, vertex(next_vertex),
                              vertex(next_vertex-1));
  }

  // Otherwise, the closest point C is incident to exactly one polyline edge.
  // We test the point P against that edge.
  if (next_vertex == num_vertices())
    --next_vertex;

  return s2pred::Sign(point, vertex(next_vertex), vertex(next_vertex - 1)) > 0;
}

bool S2Polyline::Intersects(const S2Polyline* line) const {
  if (num_vertices() <= 0 || line->num_vertices() <= 0) {
    return false;
  }

  if (!GetRectBound().Intersects(line->GetRectBound())) {
    return false;
  }

  // TODO(ericv): Use S2ShapeIndex here.
  for (int i = 1; i < num_vertices(); ++i) {
    S2EdgeCrosser crosser(
        &vertex(i - 1), &vertex(i), &line->vertex(0));
    for (int j = 1; j < line->num_vertices(); ++j) {
      if (crosser.CrossingSign(&line->vertex(j)) >= 0) {
        return true;
      }
    }
  }
  return false;
}

void S2Polyline::Reverse() {
  std::reverse(&vertices_[0], &vertices_[num_vertices_]);
}

S2LatLngRect S2Polyline::GetRectBound() const {
  S2LatLngRectBounder bounder;
  for (int i = 0; i < num_vertices(); ++i) {
    bounder.AddPoint(vertex(i));
  }
  return bounder.GetBound();
}

S2Cap S2Polyline::GetCapBound() const {
  return GetRectBound().GetCapBound();
}

bool S2Polyline::MayIntersect(const S2Cell& cell) const {
  if (num_vertices() == 0) return false;

  // We only need to check whether the cell contains vertex 0 for correctness,
  // but these tests are cheap compared to edge crossings so we might as well
  // check all the vertices.
  for (int i = 0; i < num_vertices(); ++i) {
    if (cell.Contains(vertex(i))) return true;
  }
  S2Point cell_vertices[4];
  for (int i = 0; i < 4; ++i) {
    cell_vertices[i] = cell.GetVertex(i);
  }
  for (int j = 0; j < 4; ++j) {
    S2EdgeCrosser crosser(&cell_vertices[j], &cell_vertices[(j+1)&3],
                                    &vertex(0));
    for (int i = 1; i < num_vertices(); ++i) {
      if (crosser.CrossingSign(&vertex(i)) >= 0) {
        // There is a proper crossing, or two vertices were the same.
        return true;
      }
    }
  }
  return false;
}

void S2Polyline::Encode(Encoder* const encoder) const {
  encoder->Ensure(num_vertices_ * sizeof(vertices_[0]) + 10);  // sufficient

  encoder->put8(kCurrentLosslessEncodingVersionNumber);
  encoder->put32(num_vertices_);
  encoder->putn(&vertices_[0], sizeof(vertices_[0]) * num_vertices_);

  S2_DCHECK_GE(encoder->avail(), 0);
}

bool S2Polyline::Decode(Decoder* const decoder) {
  if (decoder->avail() < sizeof(unsigned char) + sizeof(uint32)) return false;
  unsigned char version = decoder->get8();
  if (version > kCurrentLosslessEncodingVersionNumber) return false;

  num_vertices_ = decoder->get32();
  vertices_.reset(new S2Point[num_vertices_]);
  if (decoder->avail() < num_vertices_ * sizeof(vertices_[0])) return false;
  decoder->getn(&vertices_[0], num_vertices_ * sizeof(vertices_[0]));

  if (FLAGS_s2debug && s2debug_override_ == S2Debug::ALLOW) {
    S2_CHECK(IsValid());
  }
  return true;
}

namespace {

// Given a polyline, a tolerance distance, and a start index, this function
// returns the maximal end index such that the line segment between these two
// vertices passes within "tolerance" of all interior vertices, in order.
int FindEndVertex(const S2Polyline& polyline,
                  S1Angle tolerance, int index) {
  S2_DCHECK_GE(tolerance.radians(), 0);
  S2_DCHECK_LT((index + 1), polyline.num_vertices());

  // The basic idea is to keep track of the "pie wedge" of angles from the
  // starting vertex such that a ray from the starting vertex at that angle
  // will pass through the discs of radius "tolerance" centered around all
  // vertices processed so far.

  // First we define a "coordinate frame" for the tangent and normal spaces
  // at the starting vertex.  Essentially this means picking three
  // orthonormal vectors X,Y,Z such that X and Y span the tangent plane at
  // the starting vertex, and Z is "up".  We use the coordinate frame to
  // define a mapping from 3D direction vectors to a one-dimensional "ray
  // angle" in the range (-Pi, Pi].  The angle of a direction vector is
  // computed by transforming it into the X,Y,Z basis, and then calculating
  // atan2(y,x).  This mapping allows us to represent a wedge of angles as a
  // 1D interval.  Since the interval wraps around, we represent it as an
  // S1Interval, i.e. an interval on the unit circle.
  Matrix3x3_d frame;
  const S2Point& origin = polyline.vertex(index);
  S2::GetFrame(origin, &frame);

  // As we go along, we keep track of the current wedge of angles and the
  // distance to the last vertex (which must be non-decreasing).
  S1Interval current_wedge = S1Interval::Full();
  double last_distance = 0;

  for (++index; index < polyline.num_vertices(); ++index) {
    const S2Point& candidate = polyline.vertex(index);
    double distance = origin.Angle(candidate);

    // We don't allow simplification to create edges longer than 90 degrees,
    // to avoid numeric instability as lengths approach 180 degrees.  (We do
    // need to allow for original edges longer than 90 degrees, though.)
    if (distance > M_PI/2 && last_distance > 0) break;

    // Vertices must be in increasing order along the ray, except for the
    // initial disc around the origin.
    if (distance < last_distance && last_distance > tolerance.radians()) break;
    last_distance = distance;

    // Points that are within the tolerance distance of the origin do not
    // constrain the ray direction, so we can ignore them.
    if (distance <= tolerance.radians()) continue;

    // If the current wedge of angles does not contain the angle to this
    // vertex, then stop right now.  Note that the wedge of possible ray
    // angles is not necessarily empty yet, but we can't continue unless we
    // are willing to backtrack to the last vertex that was contained within
    // the wedge (since we don't create new vertices).  This would be more
    // complicated and also make the worst-case running time more than linear.
    S2Point direction = S2::ToFrame(frame, candidate);
    double center = atan2(direction.y(), direction.x());
    if (!current_wedge.Contains(center)) break;

    // To determine how this vertex constrains the possible ray angles,
    // consider the triangle ABC where A is the origin, B is the candidate
    // vertex, and C is one of the two tangent points between A and the
    // spherical cap of radius "tolerance" centered at B.  Then from the
    // spherical law of sines, sin(a)/sin(A) = sin(c)/sin(C), where "a" and
    // "c" are the lengths of the edges opposite A and C.  In our case C is a
    // 90 degree angle, therefore A = asin(sin(a) / sin(c)).  Angle A is the
    // half-angle of the allowable wedge.

    double half_angle = asin(sin(tolerance.radians()) / sin(distance));
    S1Interval target = S1Interval::FromPoint(center).Expanded(half_angle);
    current_wedge = current_wedge.Intersection(target);
    S2_DCHECK(!current_wedge.is_empty());
  }
  // We break out of the loop when we reach a vertex index that can't be
  // included in the line segment, so back up by one vertex.
  return index - 1;
}
}  // namespace

void S2Polyline::SubsampleVertices(S1Angle tolerance,
                                   vector<int>* indices) const {
  indices->clear();
  if (num_vertices() == 0) return;

  indices->push_back(0);
  S1Angle clamped_tolerance = max(tolerance, S1Angle::Radians(0));
  for (int index = 0; index + 1 < num_vertices(); ) {
    int next_index = FindEndVertex(*this, clamped_tolerance, index);
    // Don't create duplicate adjacent vertices.
    if (vertex(next_index) != vertex(index)) {
      indices->push_back(next_index);
    }
    index = next_index;
  }
}

bool S2Polyline::Equals(const S2Polyline* b) const {
  if (num_vertices() != b->num_vertices()) return false;
  for (int offset = 0; offset < num_vertices(); ++offset) {
    if (vertex(offset) != b->vertex(offset)) return false;
  }
  return true;
}

bool S2Polyline::ApproxEquals(const S2Polyline& b, S1Angle max_error) const {
  if (num_vertices() != b.num_vertices()) return false;
  for (int offset = 0; offset < num_vertices(); ++offset) {
    if (!S2::ApproxEquals(vertex(offset), b.vertex(offset), max_error)) {
      return false;
    }
  }
  return true;
}

size_t S2Polyline::SpaceUsed() const {
  return sizeof(*this) + num_vertices() * sizeof(S2Point);
}

namespace {
// Return the first i > "index" such that the ith vertex of "pline" is not at
// the same point as the "index"th vertex.  Returns pline.num_vertices() if
// there is no such value of i.
inline int NextDistinctVertex(const S2Polyline& pline, int index) {
  const S2Point& initial = pline.vertex(index);
  do {
    ++index;
  } while (index < pline.num_vertices() && pline.vertex(index) == initial);
  return index;
}

// This struct represents a search state in the NearlyCovers algorithm
// below.  See the description of the algorithm for details.
struct SearchState {
  inline SearchState(int i_val, int j_val, bool i_in_progress_val)
      : i(i_val), j(j_val), i_in_progress(i_in_progress_val) {}

  int i;
  int j;
  bool i_in_progress;
};

// This operator is needed for storing SearchStates in a set.  The ordering
// chosen has no special meaning.
struct SearchStateKeyCompare {
  bool operator() (const SearchState& a, const SearchState& b) const {
    if (a.i != b.i) return a.i < b.i;
    if (a.j != b.j) return a.j < b.j;
    return a.i_in_progress < b.i_in_progress;
  }
};

}  // namespace

bool S2Polyline::NearlyCovers(const S2Polyline& covered,
                              S1Angle max_error) const {
  // NOTE: This algorithm is described assuming that adjacent vertices in a
  // polyline are never at the same point.  That is, the ith and i+1th vertices
  // of a polyline are never at the same point in space.  The implementation
  // does not make this assumption.

  // DEFINITIONS:
  //   - edge "i" of a polyline is the edge from the ith to i+1th vertex.
  //   - covered_j is a polyline consisting of edges 0 through j of "covered."
  //   - this_i is a polyline consisting of edges 0 through i of this polyline.
  //
  // A search state is represented as an (int, int, bool) tuple, (i, j,
  // i_in_progress).  Using the "drive a car" analogy from the header comment, a
  // search state signifies that you can drive one car along "covered" from its
  // first vertex through a point on its jth edge, and another car along this
  // polyline from some point on or before its ith edge to a to a point on its
  // ith edge, such that no car ever goes backward, and the cars are always
  // within "max_error" of each other.  If i_in_progress is true, it means that
  // you can definitely drive along "covered" through the jth vertex (beginning
  // of the jth edge). Otherwise, you can definitely drive along "covered"
  // through the point on the jth edge of "covered" closest to the ith vertex of
  // this polyline.
  //
  // The algorithm begins by finding all edges of this polyline that are within
  // "max_error" of the first vertex of "covered," and adding search states
  // representing all of these possible starting states to the stack of
  // "pending" states.
  //
  // The algorithm proceeds by popping the next pending state,
  // (i,j,i_in_progress), off of the stack.  First it checks to see if that
  // state represents finding a valid covering of "covered" and returns true if
  // so.  Next, if the state represents reaching the end of this polyline
  // without finding a successful covering, the algorithm moves on to the next
  // state in the stack.  Otherwise, if state (i+1,j,false) is valid, it is
  // added to the stack of pending states.  Same for state (i,j+1,true).
  //
  // We need the stack because when "i" and "j" can both be incremented,
  // sometimes only one choice leads to a solution.  We use a set to keep track
  // of visited states to avoid duplicating work.  With the set, the worst-case
  // number of states examined is O(n+m) where n = this->num_vertices() and m =
  // covered.num_vertices().  Without it, the amount of work could be as high as
  // O((n*m)^2).  Using set, the running time is O((n*m) log (n*m)).
  //
  // TODO(user): Benchmark this, and see if the set is worth it.

  if (covered.num_vertices() == 0) return true;
  if (this->num_vertices() == 0) return false;

  vector<SearchState> pending;
  set<SearchState, SearchStateKeyCompare> done;

  // Find all possible starting states.
  for (int i = 0, next_i = NextDistinctVertex(*this, 0), next_next_i;
       next_i < this->num_vertices(); i = next_i, next_i = next_next_i) {
    next_next_i = NextDistinctVertex(*this, next_i);
    S2Point closest_point = S2::Project(
        covered.vertex(0), this->vertex(i), this->vertex(next_i));

    // In order to avoid duplicate starting states, we exclude the end vertex
    // of each edge *except* for the last non-degenerate edge.
    if ((next_next_i == this->num_vertices() ||
         closest_point != this->vertex(next_i)) &&
        S1Angle(closest_point, covered.vertex(0)) <= max_error) {
      pending.push_back(SearchState(i, 0, true));
    }
  }

  while (!pending.empty()) {
    const SearchState state = pending.back();
    pending.pop_back();
    if (!done.insert(state).second) continue;

    const int next_i = NextDistinctVertex(*this, state.i);
    const int next_j = NextDistinctVertex(covered, state.j);
    if (next_j == covered.num_vertices()) {
      return true;
    } else if (next_i == this->num_vertices()) {
      continue;
    }

    S2Point i_begin, j_begin;
    if (state.i_in_progress) {
      j_begin = covered.vertex(state.j);
      i_begin = S2::Project(
          j_begin, this->vertex(state.i), this->vertex(next_i));
    } else {
      i_begin = this->vertex(state.i);
      j_begin = S2::Project(
          i_begin, covered.vertex(state.j), covered.vertex(next_j));
    }

    if (S2::IsEdgeBNearEdgeA(j_begin, covered.vertex(next_j),
                             i_begin, this->vertex(next_i), max_error)) {
      pending.push_back(SearchState(next_i, state.j, false));
    }
    if (S2::IsEdgeBNearEdgeA(i_begin, this->vertex(next_i),
                             j_begin, covered.vertex(next_j), max_error)) {
      pending.push_back(SearchState(state.i, next_j, true));
    }
  }
  return false;
}

void S2Polyline::Shape::Init(const S2Polyline* polyline) {
  S2_LOG_IF(WARNING, polyline->num_vertices() == 1)
      << "S2Polyline::Shape with one vertex has no edges";
  polyline_ = polyline;
}

int S2Polyline::Shape::num_chains() const {
  return min(1, Shape::num_edges());  // Avoid virtual call.
}

S2Shape::Chain S2Polyline::Shape::chain(int i) const {
  S2_DCHECK_EQ(i, 0);
  return Chain(0, Shape::num_edges());  // Avoid virtual call.
}
