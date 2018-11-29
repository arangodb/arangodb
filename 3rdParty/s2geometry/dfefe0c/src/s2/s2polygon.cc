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

#include "s2/s2polygon.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <set>
#include <stack>
#include <utility>
#include <vector>


#include "s2/base/casts.h"
#include "s2/base/commandlineflags.h"
#include "s2/base/logging.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s1angle.h"
#include "s2/s1interval.h"
#include "s2/s2boolean_operation.h"
#include "s2/s2builder.h"
#include "s2/s2builderutil_s2polygon_layer.h"
#include "s2/s2builderutil_s2polyline_layer.h"
#include "s2/s2builderutil_s2polyline_vector_layer.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"
#include "s2/s2closest_edge_query.h"
#include "s2/s2contains_point_query.h"
#include "s2/s2coords.h"
#include "s2/s2crossing_edge_query.h"
#include "s2/s2debug.h"
#include "s2/s2edge_clipping.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2edge_crossings.h"
#include "s2/s2error.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2latlng_rect_bounder.h"
#include "s2/s2loop.h"
#include "s2/s2measures.h"
#include "s2/s2metrics.h"
#include "s2/s2point_compression.h"
#include "s2/s2polyline.h"
#include "s2/s2predicates.h"
#include "s2/s2shape_index.h"
#include "s2/s2shape_index_region.h"
#include "s2/s2shapeutil_visit_crossing_edge_pairs.h"
#include "s2/third_party/absl/container/fixed_array.h"
#include "s2/third_party/absl/container/inlined_vector.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/util/coding/coder.h"

using absl::make_unique;
using s2builderutil::IdentitySnapFunction;
using s2builderutil::S2PolygonLayer;
using s2builderutil::S2PolylineLayer;
using s2builderutil::S2PolylineVectorLayer;
using s2builderutil::S2CellIdSnapFunction;
using std::fabs;
using std::max;
using std::min;
using std::pair;
using std::set;
using std::sqrt;
using std::unique_ptr;
using std::vector;

DEFINE_bool(
    s2polygon_lazy_indexing, true,
    "Build the S2ShapeIndex only when it is first needed.  This can save "
    "significant amounts of memory and time when geometry is constructed but "
    "never queried, for example when converting from one format to another.");

// The maximum number of loops we'll allow when decoding a polygon.
// The default value of 10 million is 200x bigger than the number of
DEFINE_int32(
    s2polygon_decode_max_num_loops, 10000000,
    "The upper limit on the number of loops that are allowed by the "
    "S2Polygon::Decode method.");

// When adding a new encoding, be aware that old binaries will not
// be able to decode it.
static const unsigned char kCurrentUncompressedEncodingVersionNumber = 1;
static const unsigned char kCurrentCompressedEncodingVersionNumber = 4;

S2Polygon::S2Polygon()
    : s2debug_override_(S2Debug::ALLOW),
      error_inconsistent_loop_orientations_(false),
      num_vertices_(0),
      unindexed_contains_calls_(0) {
}

S2Polygon::S2Polygon(vector<unique_ptr<S2Loop>> loops, S2Debug override)
    : s2debug_override_(override) {
  InitNested(std::move(loops));
}

S2Polygon::S2Polygon(unique_ptr<S2Loop> loop, S2Debug override)
    : s2debug_override_(override) {
  Init(std::move(loop));
}

S2Polygon::S2Polygon(const S2Cell& cell)
    : s2debug_override_(S2Debug::ALLOW) {
  Init(make_unique<S2Loop>(cell));
}

void S2Polygon::set_s2debug_override(S2Debug override) {
  s2debug_override_ = override;
}

S2Debug S2Polygon::s2debug_override() const {
  return s2debug_override_;
}

void S2Polygon::Copy(const S2Polygon* src) {
  ClearLoops();
  for (int i = 0; i < src->num_loops(); ++i) {
    loops_.emplace_back(src->loop(i)->Clone());
  }
  s2debug_override_ = src->s2debug_override_;
  // Don't copy error_inconsistent_loop_orientations_, since this is not a
  // property of the polygon but only of the way the polygon was constructed.
  num_vertices_ = src->num_vertices();
  unindexed_contains_calls_.store(0, std::memory_order_relaxed);
  bound_ = src->bound_;
  subregion_bound_ = src->subregion_bound_;
  InitIndex();  // TODO(ericv): Copy the index efficiently.
}

S2Polygon* S2Polygon::Clone() const {
  S2Polygon* result = new S2Polygon;
  result->Copy(this);
  return result;
}

vector<unique_ptr<S2Loop>> S2Polygon::Release() {
  // Reset the polygon to be empty.
  vector<unique_ptr<S2Loop>> loops;
  loops.swap(loops_);
  ClearLoops();
  num_vertices_ = 0;
  bound_ = S2LatLngRect::Empty();
  subregion_bound_ = S2LatLngRect::Empty();
  return loops;
}

void S2Polygon::ClearLoops() {
  ClearIndex();
  loops_.clear();
  error_inconsistent_loop_orientations_ = false;
}

S2Polygon::~S2Polygon() {
  ClearLoops();
}

bool S2Polygon::IsValid() const {
  S2Error error;
  if (FindValidationError(&error)) {
    S2_LOG_IF(ERROR, FLAGS_s2debug) << error;
    return false;
  }
  return true;
}

bool S2Polygon::FindValidationError(S2Error* error) const {
  for (int i = 0; i < num_loops(); ++i) {
    // Check for loop errors that don't require building an S2ShapeIndex.
    if (loop(i)->FindValidationErrorNoIndex(error)) {
      error->Init(error->code(),
                  "Loop %d: %s", i, error->text().c_str());
      return true;
    }
    // Check that no loop is empty, and that the full loop only appears in the
    // full polygon.
    if (loop(i)->is_empty()) {
      error->Init(S2Error::POLYGON_EMPTY_LOOP,
                  "Loop %d: empty loops are not allowed", i);
      return true;
    }
    if (loop(i)->is_full() && num_loops() > 1) {
      error->Init(S2Error::POLYGON_EXCESS_FULL_LOOP,
                  "Loop %d: full loop appears in non-full polygon", i);
      return true;
    }
  }

  // Check for loop self-intersections and loop pairs that cross
  // (including duplicate edges and vertices).
  if (s2shapeutil::FindSelfIntersection(index_, error)) return true;

  // Check whether InitOriented detected inconsistent loop orientations.
  if (error_inconsistent_loop_orientations_) {
    error->Init(S2Error::POLYGON_INCONSISTENT_LOOP_ORIENTATIONS,
                "Inconsistent loop orientations detected");
    return true;
  }

  // Finally, verify the loop nesting hierarchy.
  return FindLoopNestingError(error);
}

bool S2Polygon::FindLoopNestingError(S2Error* error) const {
  // First check that the loop depths make sense.
  for (int last_depth = -1, i = 0; i < num_loops(); ++i) {
    int depth = loop(i)->depth();
    if (depth < 0 || depth > last_depth + 1) {
      error->Init(S2Error::POLYGON_INVALID_LOOP_DEPTH,
                  "Loop %d: invalid loop depth (%d)", i, depth);
      return true;
    }
    last_depth = depth;
  }
  // Then check that they correspond to the actual loop nesting.  This test
  // is quadratic in the number of loops but the cost per iteration is small.
  for (int i = 0; i < num_loops(); ++i) {
    int last = GetLastDescendant(i);
    for (int j = 0; j < num_loops(); ++j) {
      if (i == j) continue;
      bool nested = (j >= i + 1) && (j <= last);
      const bool reverse_b = false;
      if (loop(i)->ContainsNonCrossingBoundary(loop(j), reverse_b) != nested) {
        error->Init(S2Error::POLYGON_INVALID_LOOP_NESTING,
                    "Invalid nesting: loop %d should %scontain loop %d",
                    i, nested ? "" : "not ", j);
        return true;
      }
    }
  }
  return false;
}

void S2Polygon::InsertLoop(S2Loop* new_loop, S2Loop* parent,
                           LoopMap* loop_map) {
  vector<S2Loop*>* children;
  for (bool done = false; !done; ) {
    children = &(*loop_map)[parent];
    done = true;
    for (S2Loop* child : *children) {
      if (child->ContainsNested(new_loop)) {
        parent = child;
        done = false;
        break;
      }
    }
  }

  // Some of the children of the parent loop may now be children of
  // the new loop.
  vector<S2Loop*>* new_children = &(*loop_map)[new_loop];
  for (int i = 0; i < children->size();) {
    S2Loop* child = (*children)[i];
    if (new_loop->ContainsNested(child)) {
      new_children->push_back(child);
      children->erase(children->begin() + i);
    } else {
      ++i;
    }
  }
  children->push_back(new_loop);
}

void S2Polygon::InitLoops(LoopMap* loop_map) {
  std::stack<S2Loop*> loop_stack({nullptr});
  int depth = -1;
  while (!loop_stack.empty()) {
    S2Loop* loop = loop_stack.top();
    loop_stack.pop();
    if (loop != nullptr) {
      depth = loop->depth();
      loops_.emplace_back(loop);
    }
    const vector<S2Loop*>& children = (*loop_map)[loop];
    for (int i = children.size() - 1; i >= 0; --i) {
      S2Loop* child = children[i];
      S2_DCHECK(child != nullptr);
      child->set_depth(depth + 1);
      loop_stack.push(child);
    }
  }
}

void S2Polygon::InitIndex() {
  S2_DCHECK_EQ(0, index_.num_shape_ids());
  index_.Add(make_unique<Shape>(this));
  if (!FLAGS_s2polygon_lazy_indexing) {
    index_.ForceBuild();
  }
  if (FLAGS_s2debug && s2debug_override_ == S2Debug::ALLOW) {
    // Note that FLAGS_s2debug is false in optimized builds (by default).
    S2_CHECK(IsValid());
  }
}

void S2Polygon::ClearIndex() {
  unindexed_contains_calls_.store(0, std::memory_order_relaxed);
  index_.Clear();
}

void S2Polygon::InitNested(vector<unique_ptr<S2Loop>> loops) {
  ClearLoops();
  loops_.swap(loops);

  if (num_loops() == 1) {
    InitOneLoop();
    return;
  }
  LoopMap loop_map;
  for (int i = 0; i < num_loops(); ++i) {
    InsertLoop(loop(i), nullptr, &loop_map);
  }
  // Reorder the loops in depth-first traversal order.
  // Loops are now owned by loop_map, don't let them be
  // deleted by clear().
  for (auto& loop : loops_) loop.release();
  loops_.clear();
  InitLoops(&loop_map);

  // Compute num_vertices_, bound_, subregion_bound_.
  InitLoopProperties();
}

void S2Polygon::Init(unique_ptr<S2Loop> loop) {
  // We don't allow empty loops in the other Init() methods because deleting
  // them changes the number of loops, which is awkward to handle.
  ClearLoops();
  if (loop->is_empty()) {
    InitLoopProperties();
  } else {
    loops_.push_back(std::move(loop));
    InitOneLoop();
  }
}

// This is an internal method that expects that loops_ has already been
// initialized with a single non-empty loop.
void S2Polygon::InitOneLoop() {
  S2_DCHECK_EQ(1, num_loops());
  S2Loop* loop = loops_[0].get();
  loop->set_depth(0);
  error_inconsistent_loop_orientations_ = false;
  num_vertices_ = loop->num_vertices();
  bound_ = loop->GetRectBound();
  subregion_bound_ = S2LatLngRectBounder::ExpandForSubregions(bound_);
  InitIndex();
}

void S2Polygon::InitOriented(vector<unique_ptr<S2Loop>> loops) {
  // Here is the algorithm:
  //
  // 1. Remember which of the given loops contain S2::Origin().
  //
  // 2. Invert loops as necessary to ensure that they are nestable (i.e., no
  //    loop contains the complement of any other loop).  This may result in a
  //    set of loops corresponding to the complement of the given polygon, but
  //    we will fix that problem later.
  //
  //    We make the loops nestable by first normalizing all the loops (i.e.,
  //    inverting any loops whose curvature is negative).  This handles
  //    all loops except those whose curvature is very close to zero
  //    (within the maximum error tolerance).  Any such loops are inverted if
  //    and only if they contain S2::Origin().  (In theory this step is only
  //    necessary if there are at least two such loops.)  The resulting set of
  //    loops is guaranteed to be nestable.
  //
  // 3. Build the polygon.  This yields either the desired polygon or its
  //    complement.
  //
  // 4. If there is at least one loop, we find a loop L that is adjacent to
  //    S2::Origin() (where "adjacent" means that there exists a path
  //    connecting S2::Origin() to some vertex of L such that the path does
  //    not cross any loop).  There may be a single such adjacent loop, or
  //    there may be several (in which case they should all have the same
  //    contains_origin() value).  We choose L to be the loop containing the
  //    origin whose depth is greatest, or loop(0) (a top-level shell) if no
  //    such loop exists.
  //
  // 5. If (L originally contained origin) != (polygon contains origin), we
  //    invert the polygon.  This is done by inverting a top-level shell whose
  //    curvature is minimal and then fixing the nesting hierarchy.  Note
  //    that because we normalized all the loops initially, this step is only
  //    necessary if the polygon requires at least one non-normalized loop to
  //    represent it.

  set<const S2Loop*> contained_origin;
  for (int i = 0; i < loops.size(); ++i) {
    S2Loop* loop = loops[i].get();
    if (loop->contains_origin()) {
      contained_origin.insert(loop);
    }
    double angle = loop->GetCurvature();
    if (fabs(angle) > loop->GetCurvatureMaxError()) {
      // Normalize the loop.
      if (angle < 0) loop->Invert();
    } else {
      // Ensure that the loop does not contain the origin.
      if (loop->contains_origin()) loop->Invert();
    }
  }
  InitNested(std::move(loops));
  if (num_loops() > 0) {
    S2Loop* origin_loop = loop(0);
    bool polygon_contains_origin = false;
    for (int i = 0; i < num_loops(); ++i) {
      if (loop(i)->contains_origin()) {
        polygon_contains_origin ^= true;
        origin_loop = loop(i);
      }
    }
    if (contained_origin.count(origin_loop) != polygon_contains_origin) {
      Invert();
    }
  }
  // Verify that the original loops had consistent shell/hole orientations.
  // Each original loop L should have been inverted if and only if it now
  // represents a hole.
  for (int i = 0; i < loops_.size(); ++i) {
    if ((contained_origin.count(loop(i)) != loop(i)->contains_origin()) !=
        loop(i)->is_hole()) {
      // There is no point in saving the loop index, because the error is a
      // property of the entire set of loops.  In general there is no way to
      // determine which ones are incorrect.
      error_inconsistent_loop_orientations_ = true;
      if (FLAGS_s2debug && s2debug_override_ == S2Debug::ALLOW) {
        // The FLAGS_s2debug validity checking usually happens in InitIndex(),
        // but this error is detected too late for that.
        S2_CHECK(IsValid());  // Always fails.
      }
    }
  }
}

void S2Polygon::InitLoopProperties() {
  num_vertices_ = 0;
  bound_ = S2LatLngRect::Empty();
  for (int i = 0; i < num_loops(); ++i) {
    if (loop(i)->depth() == 0) {
      bound_ = bound_.Union(loop(i)->GetRectBound());
    }
    num_vertices_ += loop(i)->num_vertices();
  }
  subregion_bound_ = S2LatLngRectBounder::ExpandForSubregions(bound_);
  InitIndex();
}

int S2Polygon::GetParent(int k) const {
  int depth = loop(k)->depth();
  if (depth == 0) return -1;  // Optimization.
  while (--k >= 0 && loop(k)->depth() >= depth) continue;
  return k;
}

int S2Polygon::GetLastDescendant(int k) const {
  if (k < 0) return num_loops() - 1;
  int depth = loop(k)->depth();
  while (++k < num_loops() && loop(k)->depth() > depth) continue;
  return k - 1;
}

double S2Polygon::GetArea() const {
  double area = 0;
  for (int i = 0; i < num_loops(); ++i) {
    area += loop(i)->sign() * loop(i)->GetArea();
  }
  return area;
}

S2Point S2Polygon::GetCentroid() const {
  S2Point centroid;
  for (int i = 0; i < num_loops(); ++i) {
    centroid += loop(i)->sign() * loop(i)->GetCentroid();
  }
  return centroid;
}

int S2Polygon::GetSnapLevel() const {
  int snap_level = -1;
  for (const unique_ptr<S2Loop>& child : loops_) {
    for (int j = 0; j < child->num_vertices(); ++j) {
      int face;
      unsigned int si, ti;
      int level = S2::XYZtoFaceSiTi(child->vertex(j), &face, &si, &ti);
      if (level < 0) return level;  // Vertex is not a cell center.
      if (level != snap_level) {
        if (snap_level < 0) {
          snap_level = level;  // First vertex.
        } else {
          return -1;  // Vertices at more than one cell level.
        }
      }
    }
  }
  return snap_level;
}

S1Angle S2Polygon::GetDistance(const S2Point& x) const {
  // Note that S2Polygon::Contains(S2Point) is slightly more efficient than
  // the generic version used by S2ClosestEdgeQuery.
  if (Contains(x)) return S1Angle::Zero();
  return GetDistanceToBoundary(x);
}

S1Angle S2Polygon::GetDistanceToBoundary(const S2Point& x) const {
  S2ClosestEdgeQuery::Options options;
  options.set_include_interiors(false);
  S2ClosestEdgeQuery::PointTarget t(x);
  return S2ClosestEdgeQuery(&index_, options).GetDistance(&t).ToAngle();
}

/*static*/ pair<double, double> S2Polygon::GetOverlapFractions(
    const S2Polygon* a, const S2Polygon* b) {
  S2Polygon intersection;
  intersection.InitToIntersection(a, b);
  double intersection_area = intersection.GetArea();
  double a_area = a->GetArea();
  double b_area = b->GetArea();
  return std::make_pair(
      intersection_area >= a_area ? 1 : intersection_area / a_area,
      intersection_area >= b_area ? 1 : intersection_area / b_area);
}

S2Point S2Polygon::Project(const S2Point& x) const {
  if (Contains(x)) return x;
  return ProjectToBoundary(x);
}

S2Point S2Polygon::ProjectToBoundary(const S2Point& x) const {
  S2ClosestEdgeQuery::Options options;
  options.set_include_interiors(false);
  S2ClosestEdgeQuery q(&index_, options);
  S2ClosestEdgeQuery::PointTarget target(x);
  S2ClosestEdgeQuery::Result edge = q.FindClosestEdge(&target);
  return q.Project(x, edge);
}

bool S2Polygon::Contains(const S2Polygon* b) const {
  // It's worth checking bounding rectangles, since they are precomputed.
  // Note that the first bound has been expanded to account for possible
  // numerical errors in the second bound.
  if (!subregion_bound_.Contains(b->bound_)) {
    // It is possible that A contains B even though Bound(A) does not contain
    // Bound(B).  This can only happen when polygon B has at least two outer
    // shells and the union of the two bounds spans all longitudes.  For
    // example, suppose that B consists of two shells with a longitude gap
    // between them, while A consists of one shell that surrounds both shells
    // of B but goes the other way around the sphere (so that it does not
    // intersect the longitude gap).
    //
    // For convenience we just check whether B has at least two loops rather
    // than two outer shells.
    if (b->num_loops() == 1 || !bound_.lng().Union(b->bound_.lng()).is_full()) {
      return false;
    }
  }

  // The following case is not handled by S2BooleanOperation because it only
  // determines whether the boundary of the result is empty (which does not
  // distinguish between the full and empty polygons).
  if (is_empty() && b->is_full()) return false;

  return S2BooleanOperation::Contains(index_, b->index_);
}

bool S2Polygon::Intersects(const S2Polygon* b) const {
  // It's worth checking bounding rectangles, since they are precomputed.
  if (!bound_.Intersects(b->bound_)) return false;

  // The following case is not handled by S2BooleanOperation because it only
  // determines whether the boundary of the result is empty (which does not
  // distinguish between the full and empty polygons).
  if (is_full() && b->is_full()) return true;

  return S2BooleanOperation::Intersects(index_, b->index_);
}

S2Cap S2Polygon::GetCapBound() const {
  return bound_.GetCapBound();
}

void S2Polygon::GetCellUnionBound(vector<S2CellId> *cell_ids) const {
  return MakeS2ShapeIndexRegion(&index_).GetCellUnionBound(cell_ids);
}

bool S2Polygon::Contains(const S2Cell& target) const {
  return MakeS2ShapeIndexRegion(&index_).Contains(target);
}

bool S2Polygon::ApproxContains(const S2Polygon* b, S1Angle tolerance) const {
  S2Polygon difference;
  difference.InitToApproxDifference(b, this, tolerance);
  return difference.is_empty();
}

bool S2Polygon::ApproxDisjoint(const S2Polygon* b, S1Angle tolerance) const {
  S2Polygon intersection;
  intersection.InitToApproxIntersection(b, this, tolerance);
  return intersection.is_empty();
}

bool S2Polygon::ApproxEquals(const S2Polygon* b, S1Angle tolerance) const {
  // TODO(ericv): This can be implemented more cheaply with S2Builder, by
  // simply adding all the edges from one polygon, adding the reversed edge
  // from the other polygon, and turning on the options to split edges and
  // discard sibling pairs.  Then the polygons are approximately equal if the
  // output graph has no edges.
  S2Polygon symmetric_difference;
  symmetric_difference.InitToApproxSymmetricDifference(b, this, tolerance);
  return symmetric_difference.is_empty();
}

bool S2Polygon::MayIntersect(const S2Cell& target) const {
  return MakeS2ShapeIndexRegion(&index_).MayIntersect(target);
}

bool S2Polygon::Contains(const S2Point& p) const {
  // NOTE(ericv): A bounds check slows down this function by about 50%.  It is
  // worthwhile only when it might allow us to delay building the index.
  if (!index_.is_fresh() && !bound_.Contains(p)) return false;

  // For small polygons it is faster to just check all the crossings.
  // Otherwise we keep track of the number of calls to Contains() and only
  // build the index once enough calls have been made so that we think it is
  // worth the effort.  See S2Loop::Contains(S2Point) for detailed comments.
  static const int kMaxBruteForceVertices = 32;
  static const int kMaxUnindexedContainsCalls = 20;
  if (num_vertices() <= kMaxBruteForceVertices ||
      (!index_.is_fresh() &&
       ++unindexed_contains_calls_ != kMaxUnindexedContainsCalls)) {
    bool inside = false;
    for (int i = 0; i < num_loops(); ++i) {
      // Use brute force to avoid building the loop's S2ShapeIndex.
      inside ^= loop(i)->BruteForceContains(p);
    }
    return inside;
  }
  // Otherwise we look up the S2ShapeIndex cell containing this point.
  return MakeS2ContainsPointQuery(&index_).Contains(p);
}

void S2Polygon::Encode(Encoder* const encoder) const {
  if (num_vertices_ == 0) {
    EncodeCompressed(encoder, nullptr, S2::kMaxCellLevel);
    return;
  }
  // Converts all the polygon vertices to S2XYZFaceSiTi format.
  absl::FixedArray<S2XYZFaceSiTi> all_vertices(num_vertices_);
  S2XYZFaceSiTi* current_loop_vertices = all_vertices.data();
  for (const unique_ptr<S2Loop>& loop : loops_) {
    loop->GetXYZFaceSiTiVertices(current_loop_vertices);
    current_loop_vertices += loop->num_vertices();
  }
  // Computes a histogram of the cell levels at which the vertices are snapped.
  // cell_level is -1 for unsnapped, or 0 through kMaxCellLevel if snapped,
  // so we add one to it to get a non-negative index.  (histogram[0] is the
  // number of unsnapped vertices, histogram[i] the number of vertices
  // snapped at level i-1).
  std::array<int, S2::kMaxCellLevel + 2> histogram;
  histogram.fill(0);
  for (const auto& v : all_vertices) {
    histogram[v.cell_level + 1] += 1;
  }
  // Compute the level at which most of the vertices are snapped.
  // If multiple levels have the same maximum number of vertices
  // snapped to it, the first one (lowest level number / largest
  // area / smallest encoding length) will be chosen, so this
  // is desired.  Start with histogram[1] since histogram[0] is
  // the number of unsnapped vertices, which we don't care about.
  const auto max_iter =
      std::max_element(histogram.begin() + 1, histogram.end());
  // snap_level will be at position histogram[snap_level + 1], see above.
  const int snap_level = max_iter - (histogram.begin() + 1);
  const int num_snapped = *max_iter;
  // Choose an encoding format based on the number of unsnapped vertices and a
  // rough estimate of the encoded sizes.

  // The compressed encoding requires approximately 4 bytes per vertex plus
  // "exact_point_size" for each unsnapped vertex (encoded as an S2Point plus
  // the index at which it is located).
  int exact_point_size = sizeof(S2Point) + 2;
  int num_unsnapped = num_vertices_ - num_snapped;
  int compressed_size = 4 * num_vertices_ + exact_point_size * num_unsnapped;
  int lossless_size = sizeof(S2Point) * num_vertices_;
  if (compressed_size < lossless_size) {
    EncodeCompressed(encoder, all_vertices.data(), snap_level);
  } else {
    EncodeUncompressed(encoder);
  }
}

void S2Polygon::EncodeUncompressed(Encoder* const encoder) const {
  encoder->Ensure(10);  // Sufficient
  encoder->put8(kCurrentUncompressedEncodingVersionNumber);
  // This code used to write "owns_loops_", so write "true" for compatibility.
  encoder->put8(true);
  // Encode obsolete "has_holes_" field for backwards compatibility.
  bool has_holes = false;
  for (int i = 0; i < num_loops(); ++i) {
    if (loop(i)->is_hole()) has_holes = true;
  }
  encoder->put8(has_holes);
  encoder->put32(loops_.size());
  S2_DCHECK_GE(encoder->avail(), 0);

  for (int i = 0; i < num_loops(); ++i) {
    loop(i)->Encode(encoder);
  }
  bound_.Encode(encoder);
}

bool S2Polygon::Decode(Decoder* const decoder) {
  if (decoder->avail() < sizeof(unsigned char)) return false;
  unsigned char version = decoder->get8();
  switch (version) {
    case kCurrentUncompressedEncodingVersionNumber:
      return DecodeUncompressed(decoder, false);
    case kCurrentCompressedEncodingVersionNumber:
      return DecodeCompressed(decoder);
  }
  return false;
}

bool S2Polygon::DecodeWithinScope(Decoder* const decoder) {
  if (decoder->avail() < sizeof(unsigned char)) return false;
  unsigned char version = decoder->get8();
  switch (version) {
    case kCurrentUncompressedEncodingVersionNumber:
      return DecodeUncompressed(decoder, true);
    case kCurrentCompressedEncodingVersionNumber:
      return DecodeCompressed(decoder);
  }
  return false;
}

bool S2Polygon::DecodeUncompressed(Decoder* const decoder, bool within_scope) {
  if (decoder->avail() < 2 * sizeof(uint8) + sizeof(uint32)) return false;
  ClearLoops();
  decoder->get8();  // Ignore irrelevant serialized owns_loops_ value.
  decoder->get8();  // Ignore irrelevant serialized has_holes_ value.
  // Polygons with no loops are explicitly allowed here: a newly created
  // polygon has zero loops and such polygons encode and decode properly.
  const uint32 num_loops = decoder->get32();
  if (num_loops > FLAGS_s2polygon_decode_max_num_loops) return false;
  loops_.reserve(num_loops);
  num_vertices_ = 0;
  for (int i = 0; i < num_loops; ++i) {
    loops_.push_back(make_unique<S2Loop>());
    loops_.back()->set_s2debug_override(s2debug_override());
    if (within_scope) {
      if (!loops_.back()->DecodeWithinScope(decoder)) return false;
    } else {
      if (!loops_.back()->Decode(decoder)) return false;
    }
    num_vertices_ += loops_.back()->num_vertices();
  }
  if (!bound_.Decode(decoder)) return false;
  subregion_bound_ = S2LatLngRectBounder::ExpandForSubregions(bound_);
  InitIndex();
  return true;
}

// TODO(ericv): Consider adding this to the S2Loop API.  (May also want an
// undirected version (CompareDirected vs CompareUndirected); should they
// return a sign, or have separate "<" and "==" methods?)
int S2Polygon::CompareLoops(const S2Loop* a, const S2Loop* b) {
  if (a->num_vertices() != b->num_vertices()) {
    return a->num_vertices() - b->num_vertices();
  }
  S2::LoopOrder ao = a->GetCanonicalLoopOrder();
  S2::LoopOrder bo = b->GetCanonicalLoopOrder();
  if (ao.dir != bo.dir) return ao.dir - bo.dir;
  for (int n = a->num_vertices(), ai = ao.first, bi = bo.first;
       --n >= 0; ai += ao.dir, bi += bo.dir) {
    if (a->vertex(ai) < b->vertex(bi)) return -1;
    if (a->vertex(ai) > b->vertex(bi)) return 1;
  }
  return 0;
}

void S2Polygon::Invert() {
  // Inverting any one loop will invert the polygon.  The best loop to invert
  // is the one whose area is largest, since this yields the smallest area
  // after inversion.  The loop with the largest area is always at depth 0.
  // The descendents of this loop all have their depth reduced by 1, while the
  // former siblings of this loop all have their depth increased by 1.

  // The empty and full polygons are handled specially.
  if (is_empty()) {
    loops_.push_back(make_unique<S2Loop>(S2Loop::kFull()));
  } else if (is_full()) {
    ClearLoops();
  } else {
    // Find the loop whose area is largest (i.e., whose curvature is
    // smallest), minimizing calls to GetCurvature().  In particular, for
    // polygons with a single shell at level 0 there is not need to call
    // GetCurvature() at all.  (This method is relatively expensive.)
    int best = 0;
    const double kNone = 10.0;  // Flag that means "not computed yet"
    double best_angle = kNone;
    for (int i = 1; i < num_loops(); ++i) {
      if (loop(i)->depth() == 0) {
        // We defer computing the curvature of loop 0 until we discover
        // that the polygon has another top-level shell.
        if (best_angle == kNone) best_angle = loop(best)->GetCurvature();
        double angle = loop(i)->GetCurvature();
        // We break ties deterministically in order to avoid having the output
        // depend on the input order of the loops.
        if (angle < best_angle ||
            (angle == best_angle && CompareLoops(loop(i), loop(best)) < 0)) {
          best = i;
          best_angle = angle;
        }
      }
    }
    // Build the new loops vector, starting with the inverted loop.
    loop(best)->Invert();
    vector<unique_ptr<S2Loop>> new_loops;
    new_loops.reserve(num_loops());
    // Add the former siblings of this loop as descendants.
    int last_best = GetLastDescendant(best);
    new_loops.push_back(std::move(loops_[best]));
    for (int i = 0; i < num_loops(); ++i) {
      if (i < best || i > last_best) {
        loop(i)->set_depth(loop(i)->depth() + 1);
        new_loops.push_back(std::move(loops_[i]));
      }
    }
    // Add the former children of this loop as siblings.
    for (int i = 0; i < num_loops(); ++i) {
      if (i > best && i <= last_best) {
        loop(i)->set_depth(loop(i)->depth() - 1);
        new_loops.push_back(std::move(loops_[i]));
      }
    }
    loops_.swap(new_loops);
    S2_DCHECK_EQ(new_loops.size(), num_loops());
  }
  ClearIndex();
  InitLoopProperties();
}

void S2Polygon::InitToComplement(const S2Polygon* a) {
  Copy(a);
  Invert();
}

bool S2Polygon::InitToOperation(S2BooleanOperation::OpType op_type,
                                const S2Builder::SnapFunction& snap_function,
                                const S2Polygon& a, const S2Polygon& b) {
  S2BooleanOperation::Options options;
  options.set_snap_function(snap_function);
  S2BooleanOperation op(op_type, make_unique<S2PolygonLayer>(this),
                         options);
  S2Error error;
  if (!op.Build(a.index_, b.index_, &error)) {
    S2_LOG(DFATAL) << S2BooleanOperation::OpTypeToString(op_type)
                << " operation failed: " << error;
    return false;
  }
  return true;
}

void S2Polygon::InitToIntersection(const S2Polygon* a, const S2Polygon* b) {
  InitToApproxIntersection(a, b, S2::kIntersectionMergeRadius);
}

void S2Polygon::InitToApproxIntersection(const S2Polygon* a, const S2Polygon* b,
                                         S1Angle snap_radius) {
  InitToIntersection(*a, *b, IdentitySnapFunction(snap_radius));
}

void S2Polygon::InitToIntersection(
    const S2Polygon& a, const S2Polygon& b,
    const S2Builder::SnapFunction& snap_function) {
  if (!a.bound_.Intersects(b.bound_)) return;
  InitToOperation(S2BooleanOperation::OpType::INTERSECTION,
                  snap_function, a, b);

  // If the boundary is empty then there are two possible results: the empty
  // polygon or the full polygon.  Note that the (approximate) intersection of
  // two non-full polygons may be full, because one or both polygons may have
  // tiny cracks or holes that are eliminated by snapping.  Similarly, the
  // (approximate) intersection of two polygons that contain a common point
  // may be empty, since the point might be contained by tiny loops that are
  // snapped away.
  //
  // So instead we fall back to heuristics.  First we compute the minimum and
  // maximum intersection area based on the areas of the two input polygons.
  // If only one of {0, 4*Pi} is possible then we return that result.  If
  // neither is possible (before snapping) then we return the one that is
  // closest to being possible.  (It never true that both are possible.)
  if (num_loops() == 0) {
    // We know that both polygons are non-empty due to the initial bounds
    // check.  By far the most common case is that the intersection is empty,
    // so we want to make that case fast.  The intersection area satisfies:
    //
    //   max(0, A + B - 4*Pi) <= Intersection(A, B) <= min(A, B)
    //
    // where A, B can refer to a polygon or its area.  Note that if either A
    // or B is at most 2*Pi, the result must be empty.  We can use the
    // bounding rectangle areas as upper bounds on the polygon areas.
    if (a.bound_.Area() <= 2 * M_PI || b.bound_.Area() <= 2 * M_PI) return;
    double a_area = a.GetArea(), b_area = b.GetArea();
    double min_area = max(0.0, a_area + b_area - 4 * M_PI);
    double max_area = min(a_area, b_area);
    if (min_area > 4 * M_PI - max_area) {
      Invert();
    }
  }
}

void S2Polygon::InitToUnion(const S2Polygon* a, const S2Polygon* b) {
  InitToApproxUnion(a, b, S2::kIntersectionMergeRadius);
}

void S2Polygon::InitToApproxUnion(const S2Polygon* a, const S2Polygon* b,
                                  S1Angle snap_radius) {
  InitToUnion(*a, *b, IdentitySnapFunction(snap_radius));
}

void S2Polygon::InitToUnion(
    const S2Polygon& a, const S2Polygon& b,
    const S2Builder::SnapFunction& snap_function) {
  InitToOperation(S2BooleanOperation::OpType::UNION, snap_function, a, b);
  if (num_loops() == 0) {
    // See comments in InitToApproxIntersection().  In this case, the union
    // area satisfies:
    //
    //   max(A, B) <= Union(A, B) <= min(4*Pi, A + B)
    //
    // where A, B can refer to a polygon or its area.  The most common case is
    // that neither input polygon is empty, but the union is empty due to
    // snapping.
    if (a.bound_.Area() + b.bound_.Area() <= 2 * M_PI) return;
    double a_area = a.GetArea(), b_area = b.GetArea();
    double min_area = max(a_area, b_area);
    double max_area = min(4 * M_PI, a_area + b_area);
    if (min_area > 4 * M_PI - max_area) {
      Invert();
    }
  }
}

void S2Polygon::InitToDifference(const S2Polygon* a, const S2Polygon* b) {
  InitToApproxDifference(a, b, S2::kIntersectionMergeRadius);
}

void S2Polygon::InitToApproxDifference(const S2Polygon* a, const S2Polygon* b,
                                       S1Angle snap_radius) {
  InitToDifference(*a, *b, IdentitySnapFunction(snap_radius));
}

void S2Polygon::InitToDifference(
    const S2Polygon& a, const S2Polygon& b,
    const S2Builder::SnapFunction& snap_function) {
  InitToOperation(S2BooleanOperation::OpType::DIFFERENCE, snap_function, a, b);
  if (num_loops() == 0) {
    // See comments in InitToApproxIntersection().  In this case, the
    // difference area satisfies:
    //
    //   max(0, A - B) <= Difference(A, B) <= min(A, 4*Pi - B)
    //
    // where A, B can refer to a polygon or its area.  By far the most common
    // case is that result is empty.
    if (a.bound_.Area() <= 2 * M_PI || b.bound_.Area() >= 2 * M_PI) return;
    double a_area = a.GetArea(), b_area = b.GetArea();
    double min_area = max(0.0, a_area - b_area);
    double max_area = min(a_area, 4 * M_PI - b_area);
    if (min_area > 4 * M_PI - max_area) {
      Invert();
    }
  }
}

void S2Polygon::InitToSymmetricDifference(const S2Polygon* a,
                                          const S2Polygon* b) {
  InitToApproxSymmetricDifference(a, b, S2::kIntersectionMergeRadius);
}

void S2Polygon::InitToApproxSymmetricDifference(const S2Polygon* a,
                                                const S2Polygon* b,
                                                S1Angle snap_radius) {
  InitToSymmetricDifference(*a, *b, IdentitySnapFunction(snap_radius));
}

void S2Polygon::InitToSymmetricDifference(
    const S2Polygon& a, const S2Polygon& b,
    const S2Builder::SnapFunction& snap_function) {
  InitToOperation(S2BooleanOperation::OpType::SYMMETRIC_DIFFERENCE,
                  snap_function, a, b);
  if (num_loops() == 0) {
    // See comments in InitToApproxIntersection().  In this case, the
    // difference area satisfies:
    //
    //   |A - B| <= SymmetricDifference(A, B) <= 4*Pi - |4*Pi - (A + B)|
    //
    // where A, B can refer to a polygon or its area.  By far the most common
    // case is that result is empty.
    if (a.bound_.Area() + b.bound_.Area() <= 2 * M_PI) return;
    double a_area = a.GetArea(), b_area = b.GetArea();
    double min_area = fabs(a_area - b_area);
    double max_area = 4 * M_PI - fabs(4 * M_PI - (a_area + b_area));
    // If both input polygons have area 2*Pi, the result could be either empty
    // or full.  We explicitly want to choose "empty" in this case since it is
    // much more likely that the user is computing the difference between two
    // nearly identical polygons.  Hence the bias below.
    static constexpr double kBiasTowardsEmpty = 1e-14;
    if (min_area - kBiasTowardsEmpty > 4 * M_PI - max_area) {
      Invert();
    }
  }
}

void S2Polygon::InitFromBuilder(const S2Polygon& a, S2Builder* builder) {
  builder->StartLayer(make_unique<S2PolygonLayer>(this));
  builder->AddPolygon(a);
  S2Error error;
  if (!builder->Build(&error)) {
    S2_LOG(DFATAL) << "Could not build polygon: " << error;
  }
  // If there are no loops, check whether the result should be the full
  // polygon rather than the empty one.  (See InitToApproxIntersection.)
  if (num_loops() == 0) {
    if (a.bound_.Area() > 2 * M_PI && a.GetArea() > 2 * M_PI) Invert();
  }
}

void S2Polygon::InitToSnapped(const S2Polygon* a, int snap_level) {
  S2Builder builder{S2Builder::Options(S2CellIdSnapFunction(snap_level))};
  InitFromBuilder(*a, &builder);
}

void S2Polygon::InitToSimplified(const S2Polygon& a,
                                 const S2Builder::SnapFunction& snap_function) {
  S2Builder::Options options(snap_function);
  options.set_simplify_edge_chains(true);
  S2Builder builder(options);
  InitFromBuilder(a, &builder);
}

// Given a point "p" inside an S2Cell or on its boundary, return a mask
// indicating which of the S2Cell edges the point lies on.  All boundary
// comparisons are to within a maximum "u" or "v" error of "tolerance_uv".
// Bit "i" in the result is set if and only "p" is incident to the edge
// corresponding to S2Cell::edge(i).
uint8 GetCellEdgeIncidenceMask(const S2Cell& cell, const S2Point& p,
                               double tolerance_uv) {
  uint8 mask = 0;
  R2Point uv;
  if (S2::FaceXYZtoUV(cell.face(), p, &uv)) {
    R2Rect bound = cell.GetBoundUV();
    if (FLAGS_s2debug) S2_DCHECK(bound.Expanded(tolerance_uv).Contains(uv));
    if (fabs(uv[1] - bound[1][0]) <= tolerance_uv) mask |= 1;
    if (fabs(uv[0] - bound[0][1]) <= tolerance_uv) mask |= 2;
    if (fabs(uv[1] - bound[1][1]) <= tolerance_uv) mask |= 4;
    if (fabs(uv[0] - bound[0][0]) <= tolerance_uv) mask |= 8;
  }
  return mask;
}

void S2Polygon::InitToSimplifiedInCell(
    const S2Polygon* a, const S2Cell& cell,
    S1Angle snap_radius, S1Angle boundary_tolerance) {
  // The polygon to be simplified consists of "boundary edges" that follow the
  // cell boundary and "interior edges" that do not.  We want to simplify the
  // interior edges while leaving the boundary edges unchanged.  It's not
  // sufficient to call S2Builder::ForceVertex() on all boundary vertices.
  // For example, suppose the polygon includes a triangle ABC where all three
  // vertices are on the cell boundary and B is a cell corner.  Then if
  // interior edge AC snaps to vertex B, this loop would become degenerate and
  // be removed.  Similarly, we don't want boundary edges to snap to interior
  // vertices, since this also would cause portions of the polygon along the
  // boundary to be removed.
  //
  // Instead we use a two-pass algorithm.  In the first pass, we simplify
  // *only* the interior edges, using ForceVertex() to ensure that any edge
  // endpoints on the cell boundary do not move.  In the second pass, we add
  // the boundary edges (which are guaranteed to still form loops with the
  // interior edges) and build the output polygon.
  //
  // Note that in theory, simplifying the interior edges could create an
  // intersection with one of the boundary edges, since if two interior edges
  // intersect very near the boundary then the intersection point could be
  // slightly outside the cell (by at most S2::kIntersectionError).
  // This is the *only* way that a self-intersection can be created, and it is
  // expected to be extremely rare.  Nevertheless we use a small snap radius
  // in the second pass in order to eliminate any such self-intersections.
  //
  // We also want to preserve the cyclic vertex order of loops, so that the
  // original polygon can be reconstructed when no simplification is possible
  // (i.e., idempotency).  In order to do this, we group the input edges into
  // a sequence of polylines.  Each polyline contains only one type of edge
  // (interior or boundary).  We use S2Builder to simplify the interior
  // polylines, while the boundary polylines are passed through unchanged.
  // Each interior polyline is in its own S2Builder layer in order to keep the
  // edges in sequence.  This lets us ensure that in the second pass, the
  // edges are added in their original order so that S2PolygonLayer can
  // reconstruct the original loops.

  // We want an upper bound on how much "u" or "v" can change when a point on
  // the boundary of the S2Cell is moved away by up to "boundary_tolerance".
  // Inverting this, instead we could compute a lower bound on how far a point
  // can move away from an S2Cell edge when "u" or "v" is changed by a given
  // amount.  The latter quantity is simply (S2::kMinWidth.deriv() / 2)
  // under the S2_LINEAR_PROJECTION model, where we divide by 2 because we
  // want the bound in terms of (u = 2 * s - 1) rather than "s" itself.
  // Consulting s2metrics.cc, this value is sqrt(2/3)/2 = sqrt(1/6).
  // Going back to the original problem, this gives:
  double boundary_tolerance_uv = sqrt(6) * boundary_tolerance.radians();

  // The first pass yields a collection of simplified polylines that preserve
  // the original cyclic vertex order.
  auto polylines = SimplifyEdgesInCell(*a, cell, boundary_tolerance_uv,
                                       snap_radius);

  // The second pass eliminates any intersections between interior edges and
  // boundary edges, and then assembles the edges into a polygon.
  S2Builder::Options options(
      (IdentitySnapFunction(S2::kIntersectionError)));
  options.set_idempotent(false);  // Force snapping up to the given radius
  S2Builder builder(options);
  builder.StartLayer(make_unique<S2PolygonLayer>(this));
  for (const auto& polyline : polylines) {
    builder.AddPolyline(*polyline);
  }
  S2Error error;
  if (!builder.Build(&error)) {
    S2_LOG(DFATAL) << "Could not build polygon: " << error;
    return;
  }
  // If there are no loops, check whether the result should be the full
  // polygon rather than the empty one.  (See InitToApproxIntersection.)
  if (num_loops() == 0) {
    if (a->bound_.Area() > 2 * M_PI && a->GetArea() > 2 * M_PI) Invert();
  }
}

// See comments in InitToSimplifiedInCell.
vector<unique_ptr<S2Polyline>> S2Polygon::SimplifyEdgesInCell(
    const S2Polygon& a, const S2Cell& cell,
    double tolerance_uv, S1Angle snap_radius) {
  S2Builder::Options options((IdentitySnapFunction(snap_radius)));
  options.set_simplify_edge_chains(true);
  S2Builder builder(options);
  // The output consists of a sequence of polylines.  Polylines consisting of
  // interior edges are simplified using S2Builder, while polylines consisting
  // of boundary edges are returned unchanged.
  vector<unique_ptr<S2Polyline>> polylines;
  for (int i = 0; i < a.num_loops(); ++i) {
    const S2Loop& a_loop = *a.loop(i);
    const S2Point* v0 = &a_loop.oriented_vertex(0);
    uint8 mask0 = GetCellEdgeIncidenceMask(cell, *v0, tolerance_uv);
    bool in_interior = false;  // Was the last edge an interior edge?
    for (int j = 1; j <= a_loop.num_vertices(); ++j) {
      const S2Point* v1 = &a_loop.oriented_vertex(j);
      uint8 mask1 = GetCellEdgeIncidenceMask(cell, *v1, tolerance_uv);
      if ((mask0 & mask1) != 0) {
        // This is an edge along the cell boundary.  Such edges do not get
        // simplified; we add them directly to the output.  (We create a
        // separate polyline for each edge to keep things simple.)  We call
        // ForceVertex on all boundary vertices to ensure that they don't
        // move, and so that nearby interior edges are snapped to them.
        S2_DCHECK(!in_interior);
        builder.ForceVertex(*v1);
        polylines.emplace_back(new S2Polyline(vector<S2Point>{*v0, *v1}));
      } else {
        // This is an interior edge.  If this is the first edge of an interior
        // chain, then start a new S2Builder layer.  Also ensure that any
        // polyline vertices on the boundary do not move, so that they will
        // still connect with any boundary edge(s) there.
        if (!in_interior) {
          S2Polyline* polyline = new S2Polyline;
          builder.StartLayer(make_unique<S2PolylineLayer>(polyline));
          polylines.emplace_back(polyline);
          in_interior = true;
        }
        builder.AddEdge(*v0, *v1);
        if (mask1 != 0) {
          builder.ForceVertex(*v1);
          in_interior = false;  // Terminate this polyline.
        }
      }
      v0 = v1;
      mask0 = mask1;
    }
  }
  S2Error error;
  if (!builder.Build(&error)) {
    S2_LOG(DFATAL) << "InitToSimplifiedInCell failed: " << error;
  }
  return polylines;
}

vector<unique_ptr<S2Polyline>> S2Polygon::OperationWithPolyline(
    S2BooleanOperation::OpType op_type,
    const S2Builder::SnapFunction& snap_function,
    const S2Polyline& a) const {
  S2BooleanOperation::Options options;
  options.set_snap_function(snap_function);
  vector<unique_ptr<S2Polyline>> result;
  S2PolylineVectorLayer::Options layer_options;
  layer_options.set_polyline_type(
      S2PolylineVectorLayer::Options::PolylineType::WALK);
  S2BooleanOperation op(
      op_type, make_unique<S2PolylineVectorLayer>(&result, layer_options),
      options);
  MutableS2ShapeIndex a_index;
  a_index.Add(make_unique<S2Polyline::Shape>(&a));
  S2Error error;
  if (!op.Build(a_index, index_, &error)) {
    S2_LOG(DFATAL) << "Polyline " << S2BooleanOperation::OpTypeToString(op_type)
                << " operation failed: " << error;
  }
  return result;
}

vector<unique_ptr<S2Polyline>> S2Polygon::IntersectWithPolyline(
    const S2Polyline& a) const {
  return ApproxIntersectWithPolyline(a, S2::kIntersectionMergeRadius);
}

vector<unique_ptr<S2Polyline>> S2Polygon::ApproxIntersectWithPolyline(
    const S2Polyline& a, S1Angle snap_radius) const {
  return IntersectWithPolyline(a, IdentitySnapFunction(snap_radius));
}

vector<unique_ptr<S2Polyline>> S2Polygon::IntersectWithPolyline(
    const S2Polyline& a, const S2Builder::SnapFunction& snap_function) const {
  return OperationWithPolyline(S2BooleanOperation::OpType::INTERSECTION,
                               snap_function, a);
}

vector<unique_ptr<S2Polyline>> S2Polygon::SubtractFromPolyline(
    const S2Polyline& a) const {
  return ApproxSubtractFromPolyline(a, S2::kIntersectionMergeRadius);
}

vector<unique_ptr<S2Polyline>> S2Polygon::ApproxSubtractFromPolyline(
    const S2Polyline& a, S1Angle snap_radius) const {
  return SubtractFromPolyline(a, IdentitySnapFunction(snap_radius));
}

vector<unique_ptr<S2Polyline>> S2Polygon::SubtractFromPolyline(
    const S2Polyline& a, const S2Builder::SnapFunction& snap_function) const {
  return OperationWithPolyline(S2BooleanOperation::OpType::DIFFERENCE,
                               snap_function, a);
}

bool S2Polygon::Contains(const S2Polyline& b) const {
  return ApproxContains(b, S2::kIntersectionMergeRadius);
}

bool S2Polygon::ApproxContains(const S2Polyline& b, S1Angle tolerance) const {
  auto difference = ApproxSubtractFromPolyline(b, tolerance);
  return difference.empty();
}

bool S2Polygon::Intersects(const S2Polyline& b) const {
  return !ApproxDisjoint(b, S2::kIntersectionMergeRadius);
}

bool S2Polygon::ApproxDisjoint(const S2Polyline& b, S1Angle tolerance) const {
  auto intersection = ApproxIntersectWithPolyline(b, tolerance);
  return intersection.empty();
}

unique_ptr<S2Polygon> S2Polygon::DestructiveUnion(
    vector<unique_ptr<S2Polygon>> polygons) {
  return DestructiveApproxUnion(std::move(polygons),
                                S2::kIntersectionMergeRadius);
}

unique_ptr<S2Polygon> S2Polygon::DestructiveApproxUnion(
    vector<unique_ptr<S2Polygon>> polygons, S1Angle snap_radius) {
  // Effectively create a priority queue of polygons in order of number of
  // vertices.  Repeatedly union the two smallest polygons and add the result
  // to the queue until we have a single polygon to return.
  using QueueType = std::multimap<int, unique_ptr<S2Polygon>>;
  QueueType queue;  // Map from # of vertices to polygon.
  for (auto& polygon : polygons)
    queue.insert(std::make_pair(polygon->num_vertices(), std::move(polygon)));

  while (queue.size() > 1) {
    // Pop two simplest polygons from queue.
    QueueType::iterator smallest_it = queue.begin();
    int a_size = smallest_it->first;
    unique_ptr<S2Polygon> a_polygon(std::move(smallest_it->second));
    queue.erase(smallest_it);
    smallest_it = queue.begin();
    int b_size = smallest_it->first;
    unique_ptr<S2Polygon> b_polygon(std::move(smallest_it->second));
    queue.erase(smallest_it);

    // Union and add result back to queue.
    auto union_polygon = make_unique<S2Polygon>();
    union_polygon->InitToApproxUnion(a_polygon.get(), b_polygon.get(),
                                     snap_radius);
    queue.insert(std::make_pair(a_size + b_size, std::move(union_polygon)));
    // We assume that the number of vertices in the union polygon is the
    // sum of the number of vertices in the original polygons, which is not
    // always true, but will almost always be a decent approximation, and
    // faster than recomputing.
  }

  if (queue.empty())
    return make_unique<S2Polygon>();
  else
    return std::move(queue.begin()->second);
}

void S2Polygon::InitToCellUnionBorder(const S2CellUnion& cells) {
  // We use S2Builder to compute the union.  Due to rounding errors, we can't
  // compute an exact union - when a small cell is adjacent to a larger cell,
  // the shared edges can fail to line up exactly.  Two cell edges cannot come
  // closer then kMinWidth, so if we have S2Builder snap edges within half
  // that distance, then we should always merge shared edges without merging
  // different edges.
  double snap_radius = 0.5 * S2::kMinWidth.GetValue(S2CellId::kMaxLevel);
  S2Builder builder{S2Builder::Options(
      IdentitySnapFunction(S1Angle::Radians(snap_radius)))};
  builder.StartLayer(make_unique<S2PolygonLayer>(this));
  for (S2CellId id : cells) {
    builder.AddLoop(S2Loop{S2Cell{id}});
  }
  S2Error error;
  if (!builder.Build(&error)) {
    S2_LOG(DFATAL) << "InitToCellUnionBorder failed: " << error;
  }
  // If there are no loops, check whether the result should be the full
  // polygon rather than the empty one.  There are only two ways that this can
  // happen: either the cell union is empty, or it consists of all six faces.
  if (num_loops() == 0) {
    if (cells.empty()) return;
    S2_DCHECK_EQ(uint64{6} << (2 * S2CellId::kMaxLevel),
              cells.LeafCellsCovered());
    Invert();
  }
}

bool S2Polygon::IsNormalized() const {
  // TODO(ericv): The condition tested here is insufficient.  The correct
  // condition is that each *connected component* of child loops can share at
  // most one vertex with their parent loop.  Example: suppose loop A has
  // children B, C, D, and the following pairs are connected: AB, BC, CD, DA.
  // Then the polygon is not normalized.
  set<S2Point> vertices;
  const S2Loop* last_parent = nullptr;
  for (int i = 0; i < num_loops(); ++i) {
    const S2Loop* child = loop(i);
    if (child->depth() == 0) continue;
    const S2Loop* parent = loop(GetParent(i));
    if (parent != last_parent) {
      vertices.clear();
      for (int j = 0; j < parent->num_vertices(); ++j) {
        vertices.insert(parent->vertex(j));
      }
      last_parent = parent;
    }
    int count = 0;
    for (int j = 0; j < child->num_vertices(); ++j) {
      if (vertices.count(child->vertex(j)) > 0) ++count;
    }
    if (count > 1) return false;
  }
  return true;
}

bool S2Polygon::Equals(const S2Polygon* b) const {
  if (num_loops() != b->num_loops()) return false;
  for (int i = 0; i < num_loops(); ++i) {
    const S2Loop* a_loop = loop(i);
    const S2Loop* b_loop = b->loop(i);
    if ((b_loop->depth() != a_loop->depth()) || !b_loop->Equals(a_loop)) {
      return false;
    }
  }
  return true;
}

bool S2Polygon::BoundaryEquals(const S2Polygon* b) const {
  if (num_loops() != b->num_loops()) return false;

  for (int i = 0; i < num_loops(); ++i) {
    const S2Loop* a_loop = loop(i);
    bool success = false;
    for (int j = 0; j < num_loops(); ++j) {
      const S2Loop* b_loop = b->loop(j);
      if ((b_loop->depth() == a_loop->depth()) &&
          b_loop->BoundaryEquals(a_loop)) {
        success = true;
        break;
      }
    }
    if (!success) return false;
  }
  return true;
}

bool S2Polygon::BoundaryApproxEquals(const S2Polygon& b,
                                     S1Angle max_error) const {
  if (num_loops() != b.num_loops()) return false;

  // For now, we assume that there is at most one candidate match for each
  // loop.  (So far this method is just used for testing.)

  for (int i = 0; i < num_loops(); ++i) {
    const S2Loop& a_loop = *loop(i);
    bool success = false;
    for (int j = 0; j < num_loops(); ++j) {
      const S2Loop& b_loop = *b.loop(j);
      if (b_loop.depth() == a_loop.depth() &&
          b_loop.BoundaryApproxEquals(a_loop, max_error)) {
        success = true;
        break;
      }
    }
    if (!success) return false;
  }
  return true;
}

bool S2Polygon::BoundaryNear(const S2Polygon& b, S1Angle max_error) const {
  if (num_loops() != b.num_loops()) return false;

  // For now, we assume that there is at most one candidate match for each
  // loop.  (So far this method is just used for testing.)

  for (int i = 0; i < num_loops(); ++i) {
    const S2Loop& a_loop = *loop(i);
    bool success = false;
    for (int j = 0; j < num_loops(); ++j) {
      const S2Loop& b_loop = *b.loop(j);
      if (b_loop.depth() == a_loop.depth() &&
          b_loop.BoundaryNear(a_loop, max_error)) {
        success = true;
        break;
      }
    }
    if (!success) return false;
  }
  return true;
}

void S2Polygon::EncodeCompressed(Encoder* encoder,
                                 const S2XYZFaceSiTi* all_vertices,
                                 int snap_level) const {
  S2_CHECK_GE(snap_level, 0);
  // Sufficient for what we write. Typically enough for a 4 vertex polygon.
  encoder->Ensure(40);
  encoder->put8(kCurrentCompressedEncodingVersionNumber);
  encoder->put8(snap_level);
  encoder->put_varint32(num_loops());
  S2_DCHECK_GE(encoder->avail(), 0);
  const S2XYZFaceSiTi* current_loop_vertices = all_vertices;
  for (int i = 0; i < num_loops(); ++i) {
    loops_[i]->EncodeCompressed(encoder, current_loop_vertices, snap_level);
    current_loop_vertices += loops_[i]->num_vertices();
  }
  // Do not write the bound or num_vertices as they can be cheaply recomputed
  // by DecodeCompressed.  Microbenchmarks show the speed difference is
  // inconsequential.
}

bool S2Polygon::DecodeCompressed(Decoder* decoder) {
  if (decoder->avail() < sizeof(uint8)) return false;
  ClearLoops();
  int snap_level = decoder->get8();
  if (snap_level > S2CellId::kMaxLevel) return false;
  // Polygons with no loops are explicitly allowed here: a newly created
  // polygon has zero loops and such polygons encode and decode properly.
  uint32 num_loops;
  if (!decoder->get_varint32(&num_loops)) return false;
  if (num_loops > FLAGS_s2polygon_decode_max_num_loops) return false;
  loops_.reserve(num_loops);
  for (int i = 0; i < num_loops; ++i) {
    auto loop = make_unique<S2Loop>();
    loop->set_s2debug_override(s2debug_override());
    if (!loop->DecodeCompressed(decoder, snap_level)) {
      return false;
    }
    loops_.push_back(std::move(loop));
  }
  InitLoopProperties();
  return true;
}

S2Polygon::Shape::Shape(const S2Polygon* polygon)
    : cumulative_edges_(nullptr) {
  Init(polygon);
}

void S2Polygon::Shape::Init(const S2Polygon* polygon) {
  polygon_ = polygon;
  delete[] cumulative_edges_;
  cumulative_edges_ = nullptr;
  num_edges_ = 0;
  if (!polygon->is_full()) {
    const int kMaxLinearSearchLoops = 12;  // From benchmarks.
    int num_loops = polygon->num_loops();
    if (num_loops > kMaxLinearSearchLoops) {
      cumulative_edges_ = new int[num_loops];
    }
    for (int i = 0; i < num_loops; ++i) {
      if (cumulative_edges_) cumulative_edges_[i] = num_edges_;
      num_edges_ += polygon->loop(i)->num_vertices();
    }
  }
}

S2Polygon::Shape::~Shape() {
  delete[] cumulative_edges_;
}

S2Shape::Edge S2Polygon::Shape::edge(int e) const {
  S2_DCHECK_LT(e, num_edges());
  const S2Polygon* p = polygon();
  int i;
  if (cumulative_edges_) {
    // "upper_bound" finds the loop just beyond the one we want.
    int* start = std::upper_bound(cumulative_edges_,
                                  cumulative_edges_ + p->num_loops(), e) - 1;
    i = start - cumulative_edges_;
    e -= *start;
  } else {
    // When the number of loops is small, linear search is faster.  Most often
    // there is exactly one loop and the code below executes zero times.
    for (i = 0; e >= p->loop(i)->num_vertices(); ++i) {
      e -= p->loop(i)->num_vertices();
    }
  }
  return Edge(p->loop(i)->oriented_vertex(e),
              p->loop(i)->oriented_vertex(e + 1));
}

S2Shape::ReferencePoint S2Polygon::Shape::GetReferencePoint() const {
  const S2Polygon* p = polygon();
  bool contains_origin = false;
  for (int i = 0; i < p->num_loops(); ++i) {
    contains_origin ^= p->loop(i)->contains_origin();
  }
  return ReferencePoint(S2::Origin(), contains_origin);
}

int S2Polygon::Shape::num_chains() const {
  return polygon_->num_loops();
}

S2Shape::Chain S2Polygon::Shape::chain(int i) const {
  S2_DCHECK_LT(i, Shape::num_chains());
  if (cumulative_edges_) {
    return Chain(cumulative_edges_[i], polygon_->loop(i)->num_vertices());
  } else {
    int e = 0;
    for (int j = 0; j < i; ++j) e += polygon_->loop(j)->num_vertices();
    // S2Polygon represents a full loop as a loop with one vertex, while
    // S2Shape represents a full loop as a chain with no vertices.
    int num_vertices = polygon_->loop(i)->num_vertices();
    return Chain(e, (num_vertices == 1) ? 0 : num_vertices);
  }
}

S2Shape::Edge S2Polygon::Shape::chain_edge(int i, int j) const {
  S2_DCHECK_LT(i, Shape::num_chains());
  S2_DCHECK_LT(j, polygon_->loop(i)->num_vertices());
  return Edge(polygon()->loop(i)->oriented_vertex(j),
              polygon()->loop(i)->oriented_vertex(j + 1));
}

S2Shape::ChainPosition S2Polygon::Shape::chain_position(int e) const {
  // TODO(ericv): Make inline to remove code duplication with GetEdge.
  S2_DCHECK_LT(e, num_edges());
  const S2Polygon* p = polygon();
  int i;
  if (cumulative_edges_) {
    // "upper_bound" finds the loop just beyond the one we want.
    int* start = std::upper_bound(cumulative_edges_,
                                  cumulative_edges_ + p->num_loops(), e) - 1;
    i = start - cumulative_edges_;
    e -= *start;
  } else {
    // When the number of loops is small, linear search is faster.  Most often
    // there is exactly one loop and the code below executes zero times.
    for (i = 0; e >= p->loop(i)->num_vertices(); ++i) {
      e -= p->loop(i)->num_vertices();
    }
  }
  return ChainPosition(i, e);
}

size_t S2Polygon::SpaceUsed() const {
  size_t size = sizeof(*this);
  for (int i = 0; i < num_loops(); ++i) {
    size += loop(i)->SpaceUsed();
  }
  size += index_.SpaceUsed() - sizeof(index_);
  return size;
}
