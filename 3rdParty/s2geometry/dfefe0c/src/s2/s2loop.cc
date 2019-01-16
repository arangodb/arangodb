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

#include "s2/s2loop.h"

#include <algorithm>
#include <atomic>
#include <bitset>
#include <cfloat>
#include <cmath>
#include <set>
#include <utility>
#include <vector>

#include "s2/base/commandlineflags.h"
#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/r1interval.h"
#include "s2/s1angle.h"
#include "s2/s1interval.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2centroids.h"
#include "s2/s2closest_edge_query.h"
#include "s2/s2coords.h"
#include "s2/s2crossing_edge_query.h"
#include "s2/s2debug.h"
#include "s2/s2edge_clipping.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2edge_distances.h"
#include "s2/s2error.h"
#include "s2/s2latlng_rect_bounder.h"
#include "s2/s2measures.h"
#include "s2/s2padded_cell.h"
#include "s2/s2point_compression.h"
#include "s2/s2pointutil.h"
#include "s2/s2predicates.h"
#include "s2/s2shape_index.h"
#include "s2/s2shapeutil_visit_crossing_edge_pairs.h"
#include "s2/s2wedge_relations.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/third_party/absl/types/span.h"
#include "s2/util/coding/coder.h"
#include "s2/util/coding/coder.h"
#include "s2/util/math/matrix3x3.h"

using absl::MakeSpan;
using absl::make_unique;
using std::fabs;
using std::max;
using std::min;
using std::pair;
using std::set;
using std::vector;

DEFINE_bool(
    s2loop_lazy_indexing, true,
    "Build the S2ShapeIndex only when it is first needed.  This can save "
    "significant amounts of memory and time when geometry is constructed but "
    "never queried, for example when loops are passed directly to S2Polygon, "
    "or when geometry is being converted from one format to another.");

// The maximum number of vertices we'll allow when decoding a loop.
// The default value of 50 million is about 30x bigger than the number of
DEFINE_int32(
    s2polygon_decode_max_num_vertices, 50000000,
    "The upper limit on the number of loops that are allowed by the "
    "S2Polygon::Decode method.");

static const unsigned char kCurrentLosslessEncodingVersionNumber = 1;

// Boolean properties for compressed loops.
// See GetCompressedEncodingProperties.
enum CompressedLoopProperty {
  kOriginInside,
  kBoundEncoded,
  kNumProperties
};

S2Loop::S2Loop() {
  // The loop is not valid until Init() is called.
}

S2Loop::S2Loop(const vector<S2Point>& vertices)
  : S2Loop(vertices, S2Debug::ALLOW) {}

S2Loop::S2Loop(const vector<S2Point>& vertices,
               S2Debug override)
  : s2debug_override_(override) {
  Init(vertices);
}

void S2Loop::set_s2debug_override(S2Debug override) {
  s2debug_override_ = override;
}

S2Debug S2Loop::s2debug_override() const {
  return s2debug_override_;
}

void S2Loop::ClearIndex() {
  unindexed_contains_calls_.store(0, std::memory_order_relaxed);
  index_.Clear();
}

void S2Loop::Init(const vector<S2Point>& vertices) {
  ClearIndex();
  if (owns_vertices_) delete[] vertices_;
  num_vertices_ = vertices.size();
  vertices_ = new S2Point[num_vertices_];
  std::copy(vertices.begin(), vertices.end(), &vertices_[0]);
  owns_vertices_ = true;
  InitOriginAndBound();
}

bool S2Loop::IsValid() const {
  S2Error error;
  if (FindValidationError(&error)) {
    S2_LOG_IF(ERROR, FLAGS_s2debug) << error;
    return false;
  }
  return true;
}

bool S2Loop::FindValidationError(S2Error* error) const {
  return (FindValidationErrorNoIndex(error) ||
          s2shapeutil::FindSelfIntersection(index_, error));
}

bool S2Loop::FindValidationErrorNoIndex(S2Error* error) const {
  // subregion_bound_ must be at least as large as bound_.  (This is an
  // internal consistency check rather than a test of client data.)
  S2_DCHECK(subregion_bound_.Contains(bound_));

  // All vertices must be unit length.  (Unfortunately this check happens too
  // late in debug mode, because S2Loop construction calls s2pred::Sign which
  // expects vertices to be unit length.  But it is still a useful check in
  // optimized builds.)
  for (int i = 0; i < num_vertices(); ++i) {
    if (!S2::IsUnitLength(vertex(i))) {
      error->Init(S2Error::NOT_UNIT_LENGTH,
                  "Vertex %d is not unit length", i);
      return true;
    }
  }
  // Loops must have at least 3 vertices (except for the empty and full loops).
  if (num_vertices() < 3) {
    if (is_empty_or_full()) {
      return false;  // Skip remaining tests.
    }
    error->Init(S2Error::LOOP_NOT_ENOUGH_VERTICES,
                "Non-empty, non-full loops must have at least 3 vertices");
    return true;
  }
  // Loops are not allowed to have any duplicate vertices or edge crossings.
  // We split this check into two parts.  First we check that no edge is
  // degenerate (identical endpoints).  Then we check that there are no
  // intersections between non-adjacent edges (including at vertices).  The
  // second part needs the S2ShapeIndex, so it does not fall within the scope
  // of this method.
  for (int i = 0; i < num_vertices(); ++i) {
    if (vertex(i) == vertex(i+1)) {
      error->Init(S2Error::DUPLICATE_VERTICES,
                  "Edge %d is degenerate (duplicate vertex)", i);
      return true;
    }
    if (vertex(i) == -vertex(i + 1)) {
      error->Init(S2Error::ANTIPODAL_VERTICES,
                  "Vertices %d and %d are antipodal", i,
                  (i + 1) % num_vertices());
      return true;
    }
  }
  return false;
}

void S2Loop::InitOriginAndBound() {
  if (num_vertices() < 3) {
    // Check for the special empty and full loops (which have one vertex).
    if (!is_empty_or_full()) {
      origin_inside_ = false;
      return;  // Bail out without trying to access non-existent vertices.
    }
    // If the vertex is in the southern hemisphere then the loop is full,
    // otherwise it is empty.
    origin_inside_ = (vertex(0).z() < 0);
  } else {
    // Point containment testing is done by counting edge crossings starting
    // at a fixed point on the sphere (S2::Origin()).  Historically this was
    // important, but it is now no longer necessary, and it may be worthwhile
    // experimenting with using a loop vertex as the reference point.  In any
    // case, we need to know whether the reference point (S2::Origin) is
    // inside or outside the loop before we can construct the S2ShapeIndex.
    // We do this by first guessing that it is outside, and then seeing
    // whether we get the correct containment result for vertex 1.  If the
    // result is incorrect, the origin must be inside the loop.
    //
    // A loop with consecutive vertices A,B,C contains vertex B if and only if
    // the fixed vector R = S2::Ortho(B) is contained by the wedge ABC.  The
    // wedge is closed at A and open at C, i.e. the point B is inside the loop
    // if A=R but not if C=R.  This convention is required for compatibility
    // with S2::VertexCrossing.  (Note that we can't use S2::Origin()
    // as the fixed vector because of the possibility that B == S2::Origin().)
    //
    // TODO(ericv): Investigate using vertex(0) as the reference point.

    origin_inside_ = false;  // Initialize before calling Contains().
    bool v1_inside = s2pred::OrderedCCW(S2::Ortho(vertex(1)), vertex(0),
                                        vertex(2), vertex(1));
    // Note that Contains(S2Point) only does a bounds check once InitIndex()
    // has been called, so it doesn't matter that bound_ is undefined here.
    if (v1_inside != Contains(vertex(1))) {
      origin_inside_ = true;
    }
  }
  // We *must* call InitBound() before InitIndex(), because InitBound() calls
  // Contains(S2Point), and Contains(S2Point) does a bounds check whenever the
  // index is not fresh (i.e., the loop has been added to the index but the
  // index has not been updated yet).
  //
  // TODO(ericv): When fewer S2Loop methods depend on internal bounds checks,
  // consider computing the bound on demand as well.
  InitBound();
  InitIndex();
}

void S2Loop::InitBound() {
  // Check for the special empty and full loops.
  if (is_empty_or_full()) {
    if (is_empty()) {
      subregion_bound_ = bound_ = S2LatLngRect::Empty();
    } else {
      subregion_bound_ = bound_ = S2LatLngRect::Full();
    }
    return;
  }

  // The bounding rectangle of a loop is not necessarily the same as the
  // bounding rectangle of its vertices.  First, the maximal latitude may be
  // attained along the interior of an edge.  Second, the loop may wrap
  // entirely around the sphere (e.g. a loop that defines two revolutions of a
  // candy-cane stripe).  Third, the loop may include one or both poles.
  // Note that a small clockwise loop near the equator contains both poles.

  S2LatLngRectBounder bounder;
  for (int i = 0; i <= num_vertices(); ++i) {
    bounder.AddPoint(vertex(i));
  }
  S2LatLngRect b = bounder.GetBound();
  if (Contains(S2Point(0, 0, 1))) {
    b = S2LatLngRect(R1Interval(b.lat().lo(), M_PI_2), S1Interval::Full());
  }
  // If a loop contains the south pole, then either it wraps entirely
  // around the sphere (full longitude range), or it also contains the
  // north pole in which case b.lng().is_full() due to the test above.
  // Either way, we only need to do the south pole containment test if
  // b.lng().is_full().
  if (b.lng().is_full() && Contains(S2Point(0, 0, -1))) {
    b.mutable_lat()->set_lo(-M_PI_2);
  }
  bound_ = b;
  subregion_bound_ = S2LatLngRectBounder::ExpandForSubregions(bound_);
}

void S2Loop::InitIndex() {
  index_.Add(make_unique<Shape>(this));
  if (!FLAGS_s2loop_lazy_indexing) {
    index_.ForceBuild();
  }
  if (FLAGS_s2debug && s2debug_override_ == S2Debug::ALLOW) {
    // Note that FLAGS_s2debug is false in optimized builds (by default).
    S2_CHECK(IsValid());
  }
}

S2Loop::S2Loop(const S2Cell& cell)
    : depth_(0),
      num_vertices_(4),
      vertices_(new S2Point[num_vertices_]),
      owns_vertices_(true),
      s2debug_override_(S2Debug::ALLOW),
      unindexed_contains_calls_(0) {
  for (int i = 0; i < 4; ++i) {
    vertices_[i] = cell.GetVertex(i);
  }
  // We recompute the bounding rectangle ourselves, since S2Cell uses a
  // different method and we need all the bounds to be consistent.
  InitOriginAndBound();
}

S2Loop::~S2Loop() {
  if (owns_vertices_) delete[] vertices_;
}

S2Loop::S2Loop(const S2Loop& src)
    : depth_(src.depth_),
      num_vertices_(src.num_vertices_),
      vertices_(new S2Point[num_vertices_]),
      owns_vertices_(true),
      s2debug_override_(src.s2debug_override_),
      origin_inside_(src.origin_inside_),
      unindexed_contains_calls_(0),
      bound_(src.bound_),
      subregion_bound_(src.subregion_bound_) {
  std::copy(&src.vertices_[0], &src.vertices_[num_vertices_], &vertices_[0]);
  InitIndex();
}

S2Loop* S2Loop::Clone() const {
  return new S2Loop(*this);
}

int S2Loop::FindVertex(const S2Point& p) const {
  if (num_vertices() < 10) {
    // Exhaustive search.  Return value must be in the range [1..N].
    for (int i = 1; i <= num_vertices(); ++i) {
      if (vertex(i) == p) return i;
    }
    return -1;
  }
  MutableS2ShapeIndex::Iterator it(&index_);
  if (!it.Locate(p)) return -1;

  const S2ClippedShape& a_clipped = it.cell().clipped(0);
  for (int i = a_clipped.num_edges() - 1; i >= 0; --i) {
    int ai = a_clipped.edge(i);
    // Return value must be in the range [1..N].
    if (vertex(ai) == p) return (ai == 0) ? num_vertices() : ai;
    if (vertex(ai+1) == p) return ai+1;
  }
  return -1;
}

bool S2Loop::IsNormalized() const {
  // Optimization: if the longitude span is less than 180 degrees, then the
  // loop covers less than half the sphere and is therefore normalized.
  if (bound_.lng().GetLength() < M_PI) return true;

  return S2::IsNormalized(vertices_span());
}

void S2Loop::Normalize() {
  S2_CHECK(owns_vertices_);
  if (!IsNormalized()) Invert();
  S2_DCHECK(IsNormalized());
}

void S2Loop::Invert() {
  S2_CHECK(owns_vertices_);
  ClearIndex();
  if (is_empty_or_full()) {
    vertices_[0] = is_full() ? kEmptyVertex() : kFullVertex();
  } else {
    std::reverse(vertices_, vertices_ + num_vertices());
  }
  // origin_inside_ must be set correctly before building the S2ShapeIndex.
  origin_inside_ ^= true;
  if (bound_.lat().lo() > -M_PI_2 && bound_.lat().hi() < M_PI_2) {
    // The complement of this loop contains both poles.
    subregion_bound_ = bound_ = S2LatLngRect::Full();
  } else {
    InitBound();
  }
  InitIndex();
}

double S2Loop::GetArea() const {
  // S2Loop has its own convention for empty and full loops.
  if (is_empty_or_full()) {
    return contains_origin() ? (4 * M_PI) : 0;
  }
  return S2::GetArea(vertices_span());
}

S2Point S2Loop::GetCentroid() const {
  // Empty and full loops are handled correctly.
  return S2::GetCentroid(vertices_span());
}

S2::LoopOrder S2Loop::GetCanonicalLoopOrder() const {
  return S2::GetCanonicalLoopOrder(vertices_span());
}

S1Angle S2Loop::GetDistance(const S2Point& x) const {
  // Note that S2Loop::Contains(S2Point) is slightly more efficient than the
  // generic version used by S2ClosestEdgeQuery.
  if (Contains(x)) return S1Angle::Zero();
  return GetDistanceToBoundary(x);
}

S1Angle S2Loop::GetDistanceToBoundary(const S2Point& x) const {
  S2ClosestEdgeQuery::Options options;
  options.set_include_interiors(false);
  S2ClosestEdgeQuery::PointTarget t(x);
  return S2ClosestEdgeQuery(&index_, options).GetDistance(&t).ToAngle();
}

S2Point S2Loop::Project(const S2Point& x) const {
  if (Contains(x)) return x;
  return ProjectToBoundary(x);
}

S2Point S2Loop::ProjectToBoundary(const S2Point& x) const {
  S2ClosestEdgeQuery::Options options;
  options.set_include_interiors(false);
  S2ClosestEdgeQuery q(&index_, options);
  S2ClosestEdgeQuery::PointTarget target(x);
  S2ClosestEdgeQuery::Result edge = q.FindClosestEdge(&target);
  return q.Project(x, edge);
}

double S2Loop::GetCurvature() const {
  // S2Loop has its own convention for empty and full loops.  For such loops,
  // we return the limit value as the area approaches 0 or 4*Pi respectively.
  if (is_empty_or_full()) {
    return contains_origin() ? (-2 * M_PI) : (2 * M_PI);
  }
  return S2::GetCurvature(vertices_span());
}

double S2Loop::GetCurvatureMaxError() const {
  return S2::GetCurvatureMaxError(vertices_span());
}

S2Cap S2Loop::GetCapBound() const {
  return bound_.GetCapBound();
}

bool S2Loop::Contains(const S2Cell& target) const {
  MutableS2ShapeIndex::Iterator it(&index_);
  S2ShapeIndex::CellRelation relation = it.Locate(target.id());

  // If "target" is disjoint from all index cells, it is not contained.
  // Similarly, if "target" is subdivided into one or more index cells then it
  // is not contained, since index cells are subdivided only if they (nearly)
  // intersect a sufficient number of edges.  (But note that if "target" itself
  // is an index cell then it may be contained, since it could be a cell with
  // no edges in the loop interior.)
  if (relation != S2ShapeIndex::INDEXED) return false;

  // Otherwise check if any edges intersect "target".
  if (BoundaryApproxIntersects(it, target)) return false;

  // Otherwise check if the loop contains the center of "target".
  return Contains(it, target.GetCenter());
}

bool S2Loop::MayIntersect(const S2Cell& target) const {
  MutableS2ShapeIndex::Iterator it(&index_);
  S2ShapeIndex::CellRelation relation = it.Locate(target.id());

  // If "target" does not overlap any index cell, there is no intersection.
  if (relation == S2ShapeIndex::DISJOINT) return false;

  // If "target" is subdivided into one or more index cells, there is an
  // intersection to within the S2ShapeIndex error bound (see Contains).
  if (relation == S2ShapeIndex::SUBDIVIDED) return true;

  // If "target" is an index cell, there is an intersection because index cells
  // are created only if they have at least one edge or they are entirely
  // contained by the loop.
  if (it.id() == target.id()) return true;

  // Otherwise check if any edges intersect "target".
  if (BoundaryApproxIntersects(it, target)) return true;

  // Otherwise check if the loop contains the center of "target".
  return Contains(it, target.GetCenter());
}

bool S2Loop::BoundaryApproxIntersects(const MutableS2ShapeIndex::Iterator& it,
                                      const S2Cell& target) const {
  S2_DCHECK(it.id().contains(target.id()));
  const S2ClippedShape& a_clipped = it.cell().clipped(0);
  int a_num_edges = a_clipped.num_edges();

  // If there are no edges, there is no intersection.
  if (a_num_edges == 0) return false;

  // We can save some work if "target" is the index cell itself.
  if (it.id() == target.id()) return true;

  // Otherwise check whether any of the edges intersect "target".
  static const double kMaxError = (S2::kFaceClipErrorUVCoord +
                                   S2::kIntersectsRectErrorUVDist);
  R2Rect bound = target.GetBoundUV().Expanded(kMaxError);
  for (int i = 0; i < a_num_edges; ++i) {
    int ai = a_clipped.edge(i);
    R2Point v0, v1;
    if (S2::ClipToPaddedFace(vertex(ai), vertex(ai+1), target.face(),
                                     kMaxError, &v0, &v1) &&
        S2::IntersectsRect(v0, v1, bound)) {
      return true;
    }
  }
  return false;
}

bool S2Loop::Contains(const S2Point& p) const {
  // NOTE(ericv): A bounds check slows down this function by about 50%.  It is
  // worthwhile only when it might allow us to delay building the index.
  if (!index_.is_fresh() && !bound_.Contains(p)) return false;

  // For small loops it is faster to just check all the crossings.  We also
  // use this method during loop initialization because InitOriginAndBound()
  // calls Contains() before InitIndex().  Otherwise, we keep track of the
  // number of calls to Contains() and only build the index when enough calls
  // have been made so that we think it is worth the effort.  Note that the
  // code below is structured so that if many calls are made in parallel only
  // one thread builds the index, while the rest continue using brute force
  // until the index is actually available.
  //
  // The constants below were tuned using the benchmarks.  It turns out that
  // building the index costs roughly 50x as much as Contains().  (The ratio
  // increases slowly from 46x with 64 points to 61x with 256k points.)  The
  // textbook approach to this problem would be to wait until the cumulative
  // time we would have saved with an index approximately equals the cost of
  // building the index, and then build it.  (This gives the optimal
  // competitive ratio of 2; look up "competitive algorithms" for details.)
  // We set the limit somewhat lower than this (20 rather than 50) because
  // building the index may be forced anyway by other API calls, and so we
  // want to err on the side of building it too early.

  static const int kMaxBruteForceVertices = 32;
  static const int kMaxUnindexedContainsCalls = 20;  // See notes above.
  if (index_.num_shape_ids() == 0 ||  // InitIndex() not called yet
      num_vertices() <= kMaxBruteForceVertices ||
      (!index_.is_fresh() &&
       ++unindexed_contains_calls_ != kMaxUnindexedContainsCalls)) {
    return BruteForceContains(p);
  }
  // Otherwise we look up the S2ShapeIndex cell containing this point.  Note
  // the index is built automatically the first time an iterator is created.
  MutableS2ShapeIndex::Iterator it(&index_);
  if (!it.Locate(p)) return false;
  return Contains(it, p);
}

bool S2Loop::BruteForceContains(const S2Point& p) const {
  // Empty and full loops don't need a special case, but invalid loops with
  // zero vertices do, so we might as well handle them all at once.
  if (num_vertices() < 3) return origin_inside_;

  S2Point origin = S2::Origin();
  S2EdgeCrosser crosser(&origin, &p, &vertex(0));
  bool inside = origin_inside_;
  for (int i = 1; i <= num_vertices(); ++i) {
    inside ^= crosser.EdgeOrVertexCrossing(&vertex(i));
  }
  return inside;
}

bool S2Loop::Contains(const MutableS2ShapeIndex::Iterator& it,
                      const S2Point& p) const {
  // Test containment by drawing a line segment from the cell center to the
  // given point and counting edge crossings.
  const S2ClippedShape& a_clipped = it.cell().clipped(0);
  bool inside = a_clipped.contains_center();
  int a_num_edges = a_clipped.num_edges();
  if (a_num_edges > 0) {
    S2Point center = it.center();
    S2EdgeCrosser crosser(&center, &p);
    int ai_prev = -2;
    for (int i = 0; i < a_num_edges; ++i) {
      int ai = a_clipped.edge(i);
      if (ai != ai_prev + 1) crosser.RestartAt(&vertex(ai));
      ai_prev = ai;
      inside ^= crosser.EdgeOrVertexCrossing(&vertex(ai+1));
    }
  }
  return inside;
}

void S2Loop::Encode(Encoder* const encoder) const {
  encoder->Ensure(num_vertices_ * sizeof(*vertices_) + 20);  // sufficient

  encoder->put8(kCurrentLosslessEncodingVersionNumber);
  encoder->put32(num_vertices_);
  encoder->putn(vertices_, sizeof(*vertices_) * num_vertices_);
  encoder->put8(origin_inside_);
  encoder->put32(depth_);
  S2_DCHECK_GE(encoder->avail(), 0);

  bound_.Encode(encoder);
}

bool S2Loop::Decode(Decoder* const decoder) {
  if (decoder->avail() < sizeof(unsigned char)) return false;
  unsigned char version = decoder->get8();
  switch (version) {
    case kCurrentLosslessEncodingVersionNumber:
      return DecodeInternal(decoder, false);
  }
  return false;
}

bool S2Loop::DecodeWithinScope(Decoder* const decoder) {
  if (decoder->avail() < sizeof(unsigned char)) return false;
  unsigned char version = decoder->get8();
  switch (version) {
    case kCurrentLosslessEncodingVersionNumber:
      return DecodeInternal(decoder, true);
  }
  return false;
}

bool S2Loop::DecodeInternal(Decoder* const decoder,
                            bool within_scope) {
  // Perform all checks before modifying vertex state. Empty loops are
  // explicitly allowed here: a newly created loop has zero vertices
  // and such loops encode and decode properly.
  if (decoder->avail() < sizeof(uint32)) return false;
  const uint32 num_vertices = decoder->get32();
  if (num_vertices > FLAGS_s2polygon_decode_max_num_vertices) {
    return false;
  }
  if (decoder->avail() < (num_vertices * sizeof(*vertices_) +
                          sizeof(uint8) + sizeof(uint32))) {
    return false;
  }
  ClearIndex();
  if (owns_vertices_) delete[] vertices_;
  num_vertices_ = num_vertices;

  // x86 can do unaligned floating-point reads; however, many other
  // platforms can not. Do not use the zero-copy version if we are on
  // an architecture that does not support unaligned reads, and the
  // pointer is not correctly aligned.
#if defined(ARCH_PIII) || defined(ARCH_K8)
  bool is_misaligned = false;
#else
  bool is_misaligned = ((intptr_t)decoder->ptr() % sizeof(double) != 0);
#endif
  if (within_scope && !is_misaligned) {
    vertices_ = const_cast<S2Point *>(reinterpret_cast<const S2Point*>(
                    decoder->ptr()));
    decoder->skip(num_vertices_ * sizeof(*vertices_));
    owns_vertices_ = false;
  } else {
    vertices_ = new S2Point[num_vertices_];
    decoder->getn(vertices_, num_vertices_ * sizeof(*vertices_));
    owns_vertices_ = true;
  }
  origin_inside_ = decoder->get8();
  depth_ = decoder->get32();
  if (!bound_.Decode(decoder)) return false;
  subregion_bound_ = S2LatLngRectBounder::ExpandForSubregions(bound_);

  // An initialized loop will have some non-zero count of vertices. A default
  // (uninitialized) has zero vertices. This code supports encoding and
  // decoding of uninitialized loops, but we only want to call InitIndex for
  // initialized loops. Otherwise we defer InitIndex until the call to Init().
  if (num_vertices > 0) {
    InitIndex();
  }

  return true;
}

// LoopRelation is an abstract class that defines a relationship between two
// loops (Contains, Intersects, or CompareBoundary).
class LoopRelation {
 public:
  LoopRelation() {}
  virtual ~LoopRelation() {}

  // Optionally, a_target() and b_target() can specify an early-exit condition
  // for the loop relation.  If any point P is found such that
  //
  //   A.Contains(P) == a_crossing_target() &&
  //   B.Contains(P) == b_crossing_target()
  //
  // then the loop relation is assumed to be the same as if a pair of crossing
  // edges were found.  For example, the Contains() relation has
  //
  //   a_crossing_target() == 0
  //   b_crossing_target() == 1
  //
  // because if A.Contains(P) == 0 (false) and B.Contains(P) == 1 (true) for
  // any point P, then it is equivalent to finding an edge crossing (i.e.,
  // since Contains() returns false in both cases).
  //
  // Loop relations that do not have an early-exit condition of this form
  // should return -1 for both crossing targets.
  virtual int a_crossing_target() const = 0;
  virtual int b_crossing_target() const = 0;

  // Given a vertex "ab1" that is shared between the two loops, return true if
  // the two associated wedges (a0, ab1, b2) and (b0, ab1, b2) are equivalent
  // to an edge crossing.  The loop relation is also allowed to maintain its
  // own internal state, and can return true if it observes any sequence of
  // wedges that are equivalent to an edge crossing.
  virtual bool WedgesCross(const S2Point& a0, const S2Point& ab1,
                           const S2Point& a2, const S2Point& b0,
                           const S2Point& b2) = 0;
};

// RangeIterator is a wrapper over MutableS2ShapeIndex::Iterator with extra
// methods that are useful for merging the contents of two or more
// S2ShapeIndexes.
class RangeIterator {
 public:
  // Construct a new RangeIterator positioned at the first cell of the index.
  explicit RangeIterator(const MutableS2ShapeIndex* index)
      : it_(index, S2ShapeIndex::BEGIN) {
    Refresh();
  }

  // The current S2CellId and cell contents.
  S2CellId id() const { return it_.id(); }
  const S2ShapeIndexCell& cell() const { return it_.cell(); }

  // The min and max leaf cell ids covered by the current cell.  If Done() is
  // true, these methods return a value larger than any valid cell id.
  S2CellId range_min() const { return range_min_; }
  S2CellId range_max() const { return range_max_; }

  // Various other convenience methods for the current cell.
  const S2ClippedShape& clipped() const { return cell().clipped(0); }

  int num_edges() const { return clipped().num_edges(); }
  bool contains_center() const { return clipped().contains_center(); }

  void Next() { it_.Next(); Refresh(); }
  bool Done() { return it_.done(); }

  // Position the iterator at the first cell that overlaps or follows
  // "target", i.e. such that range_max() >= target.range_min().
  void SeekTo(const RangeIterator& target) {
    it_.Seek(target.range_min());
    // If the current cell does not overlap "target", it is possible that the
    // previous cell is the one we are looking for.  This can only happen when
    // the previous cell contains "target" but has a smaller S2CellId.
    if (it_.done() || it_.id().range_min() > target.range_max()) {
      if (it_.Prev() && it_.id().range_max() < target.id()) it_.Next();
    }
    Refresh();
  }

  // Position the iterator at the first cell that follows "target", i.e. the
  // first cell such that range_min() > target.range_max().
  void SeekBeyond(const RangeIterator& target) {
    it_.Seek(target.range_max().next());
    if (!it_.done() && it_.id().range_min() <= target.range_max()) {
      it_.Next();
    }
    Refresh();
  }

 private:
  // Updates internal state after the iterator has been repositioned.
  void Refresh() {
    range_min_ = id().range_min();
    range_max_ = id().range_max();
  }

  MutableS2ShapeIndex::Iterator it_;
  S2CellId range_min_, range_max_;
};

// LoopCrosser is a helper class for determining whether two loops cross.
// It is instantiated twice for each pair of loops to be tested, once for the
// pair (A,B) and once for the pair (B,A), in order to be able to process
// edges in either loop nesting order.
class LoopCrosser {
 public:
  // If "swapped" is true, the loops A and B have been swapped.  This affects
  // how arguments are passed to the given loop relation, since for example
  // A.Contains(B) is not the same as B.Contains(A).
  LoopCrosser(const S2Loop& a, const S2Loop& b,
              LoopRelation* relation, bool swapped)
      : a_(a), b_(b), relation_(relation), swapped_(swapped),
        a_crossing_target_(relation->a_crossing_target()),
        b_crossing_target_(relation->b_crossing_target()),
        b_query_(&b.index_) {
    using std::swap;
    if (swapped) swap(a_crossing_target_, b_crossing_target_);
  }

  // Return the crossing targets for the loop relation, taking into account
  // whether the loops have been swapped.
  int a_crossing_target() const { return a_crossing_target_; }
  int b_crossing_target() const { return b_crossing_target_; }

  // Given two iterators positioned such that ai->id().Contains(bi->id()),
  // return true if there is a crossing relationship anywhere within ai->id().
  // Specifically, this method returns true if there is an edge crossing, a
  // wedge crossing, or a point P that matches both "crossing targets".
  // Advances both iterators past ai->id().
  bool HasCrossingRelation(RangeIterator* ai, RangeIterator* bi);

  // Given two index cells, return true if there are any edge crossings or
  // wedge crossings within those cells.
  bool CellCrossesCell(const S2ClippedShape& a_clipped,
                       const S2ClippedShape& b_clipped);

 private:
  // Given two iterators positioned such that ai->id().Contains(bi->id()),
  // return true if there is an edge crossing or wedge crosssing anywhere
  // within ai->id().  Advances "bi" (only) past ai->id().
  bool HasCrossing(RangeIterator* ai, RangeIterator* bi);

  // Given an index cell of A, return true if there are any edge or wedge
  // crossings with any index cell of B contained within "b_id".
  bool CellCrossesAnySubcell(const S2ClippedShape& a_clipped, S2CellId b_id);

  // Prepare to check the given edge of loop A for crossings.
  void StartEdge(int aj);

  // Check the current edge of loop A for crossings with all edges of the
  // given index cell of loop B.
  bool EdgeCrossesCell(const S2ClippedShape& b_clipped);

  const S2Loop& a_;
  const S2Loop& b_;
  LoopRelation* const relation_;
  const bool swapped_;
  int a_crossing_target_, b_crossing_target_;

  // State maintained by StartEdge() and EdgeCrossesCell().
  S2EdgeCrosser crosser_;
  int aj_, bj_prev_;

  // Temporary data declared here to avoid repeated memory allocations.
  S2CrossingEdgeQuery b_query_;
  vector<const S2ShapeIndexCell*> b_cells_;
};

inline void LoopCrosser::StartEdge(int aj) {
  // Start testing the given edge of A for crossings.
  crosser_.Init(&a_.vertex(aj), &a_.vertex(aj+1));
  aj_ = aj;
  bj_prev_ = -2;
}

inline bool LoopCrosser::EdgeCrossesCell(const S2ClippedShape& b_clipped) {
  // Test the current edge of A against all edges of "b_clipped".
  int b_num_edges = b_clipped.num_edges();
  for (int j = 0; j < b_num_edges; ++j) {
    int bj = b_clipped.edge(j);
    if (bj != bj_prev_ + 1) crosser_.RestartAt(&b_.vertex(bj));
    bj_prev_ = bj;
    int crossing = crosser_.CrossingSign(&b_.vertex(bj + 1));
    if (crossing < 0) continue;
    if (crossing > 0) return true;
    // We only need to check each shared vertex once, so we only
    // consider the case where a_vertex(aj_+1) == b_.vertex(bj+1).
    if (a_.vertex(aj_+1) == b_.vertex(bj+1)) {
      if (swapped_ ?
          relation_->WedgesCross(
              b_.vertex(bj), b_.vertex(bj+1), b_.vertex(bj+2),
              a_.vertex(aj_), a_.vertex(aj_+2)) :
          relation_->WedgesCross(
              a_.vertex(aj_), a_.vertex(aj_+1), a_.vertex(aj_+2),
              b_.vertex(bj), b_.vertex(bj+2))) {
        return true;
      }
    }
  }
  return false;
}

bool LoopCrosser::CellCrossesCell(const S2ClippedShape& a_clipped,
                                  const S2ClippedShape& b_clipped) {
  // Test all edges of "a_clipped" against all edges of "b_clipped".
  int a_num_edges = a_clipped.num_edges();
  for (int i = 0; i < a_num_edges; ++i) {
    StartEdge(a_clipped.edge(i));
    if (EdgeCrossesCell(b_clipped)) return true;
  }
  return false;
}

bool LoopCrosser::CellCrossesAnySubcell(const S2ClippedShape& a_clipped,
                                        S2CellId b_id) {
  // Test all edges of "a_clipped" against all edges of B.  The relevant B
  // edges are guaranteed to be children of "b_id", which lets us find the
  // correct index cells more efficiently.
  S2PaddedCell b_root(b_id, 0);
  int a_num_edges = a_clipped.num_edges();
  for (int i = 0; i < a_num_edges; ++i) {
    int aj = a_clipped.edge(i);
    // Use an S2CrossingEdgeQuery starting at "b_root" to find the index cells
    // of B that might contain crossing edges.
    b_query_.GetCells(a_.vertex(aj), a_.vertex(aj+1), b_root, &b_cells_);
    if (b_cells_.empty()) continue;
    StartEdge(aj);
    for (const S2ShapeIndexCell* b_cell : b_cells_) {
      if (EdgeCrossesCell(b_cell->clipped(0))) return true;
    }
  }
  return false;
}

bool LoopCrosser::HasCrossing(RangeIterator* ai, RangeIterator* bi) {
  S2_DCHECK(ai->id().contains(bi->id()));
  // If ai->id() intersects many edges of B, then it is faster to use
  // S2CrossingEdgeQuery to narrow down the candidates.  But if it intersects
  // only a few edges, it is faster to check all the crossings directly.
  // We handle this by advancing "bi" and keeping track of how many edges we
  // would need to test.

  static const int kEdgeQueryMinEdges = 20;  // Tuned using benchmarks.
  int total_edges = 0;
  b_cells_.clear();
  do {
    if (bi->num_edges() > 0) {
      total_edges += bi->num_edges();
      if (total_edges >= kEdgeQueryMinEdges) {
        // There are too many edges to test them directly, so use
        // S2CrossingEdgeQuery.
        if (CellCrossesAnySubcell(ai->clipped(), ai->id())) return true;
        bi->SeekBeyond(*ai);
        return false;
      }
      b_cells_.push_back(&bi->cell());
    }
    bi->Next();
  } while (bi->id() <= ai->range_max());

  // Test all the edge crossings directly.
  for (const S2ShapeIndexCell* b_cell : b_cells_) {
    if (CellCrossesCell(ai->clipped(), b_cell->clipped(0))) {
      return true;
    }
  }
  return false;
}

bool LoopCrosser::HasCrossingRelation(RangeIterator* ai, RangeIterator* bi) {
  S2_DCHECK(ai->id().contains(bi->id()));
  if (ai->num_edges() == 0) {
    if (ai->contains_center() == a_crossing_target_) {
      // All points within ai->id() satisfy the crossing target for A, so it's
      // worth iterating through the cells of B to see whether any cell
      // centers also satisfy the crossing target for B.
      do {
        if (bi->contains_center() == b_crossing_target_) return true;
        bi->Next();
      } while (bi->id() <= ai->range_max());
    } else {
      // The crossing target for A is not satisfied, so we skip over the cells
      // of B using binary search.
      bi->SeekBeyond(*ai);
    }
  } else {
    // The current cell of A has at least one edge, so check for crossings.
    if (HasCrossing(ai, bi)) return true;
  }
  ai->Next();
  return false;
}

/*static*/ bool S2Loop::HasCrossingRelation(const S2Loop& a, const S2Loop& b,
                                            LoopRelation* relation) {
  // We look for S2CellId ranges where the indexes of A and B overlap, and
  // then test those edges for crossings.
  RangeIterator ai(&a.index_), bi(&b.index_);
  LoopCrosser ab(a, b, relation, false);  // Tests edges of A against B
  LoopCrosser ba(b, a, relation, true);   // Tests edges of B against A
  while (!ai.Done() || !bi.Done()) {
    if (ai.range_max() < bi.range_min()) {
      // The A and B cells don't overlap, and A precedes B.
      ai.SeekTo(bi);
    } else if (bi.range_max() < ai.range_min()) {
      // The A and B cells don't overlap, and B precedes A.
      bi.SeekTo(ai);
    } else {
      // One cell contains the other.  Determine which cell is larger.
      int64 ab_relation = ai.id().lsb() - bi.id().lsb();
      if (ab_relation > 0) {
        // A's index cell is larger.
        if (ab.HasCrossingRelation(&ai, &bi)) return true;
      } else if (ab_relation < 0) {
        // B's index cell is larger.
        if (ba.HasCrossingRelation(&bi, &ai)) return true;
      } else {
        // The A and B cells are the same.  Since the two cells have the same
        // center point P, check whether P satisfies the crossing targets.
        if (ai.contains_center() == ab.a_crossing_target() &&
            bi.contains_center() == ab.b_crossing_target()) {
          return true;
        }
        // Otherwise test all the edge crossings directly.
        if (ai.num_edges() > 0 && bi.num_edges() > 0 &&
            ab.CellCrossesCell(ai.clipped(), bi.clipped())) {
          return true;
        }
        ai.Next();
        bi.Next();
      }
    }
  }
  return false;
}

// Loop relation for Contains().
class ContainsRelation : public LoopRelation {
 public:
  ContainsRelation() : found_shared_vertex_(false) {}
  bool found_shared_vertex() const { return found_shared_vertex_; }

  // If A.Contains(P) == false && B.Contains(P) == true, it is equivalent to
  // having an edge crossing (i.e., Contains returns false).
  int a_crossing_target() const override { return false; }
  int b_crossing_target() const override { return true; }

  bool WedgesCross(const S2Point& a0, const S2Point& ab1, const S2Point& a2,
                   const S2Point& b0, const S2Point& b2) override {
    found_shared_vertex_ = true;
    return !S2::WedgeContains(a0, ab1, a2, b0, b2);
  }

 private:
  bool found_shared_vertex_;
};

bool S2Loop::Contains(const S2Loop* b) const {
  // For this loop A to contains the given loop B, all of the following must
  // be true:
  //
  //  (1) There are no edge crossings between A and B except at vertices.
  //
  //  (2) At every vertex that is shared between A and B, the local edge
  //      ordering implies that A contains B.
  //
  //  (3) If there are no shared vertices, then A must contain a vertex of B
  //      and B must not contain a vertex of A.  (An arbitrary vertex may be
  //      chosen in each case.)
  //
  // The second part of (3) is necessary to detect the case of two loops whose
  // union is the entire sphere, i.e. two loops that contains each other's
  // boundaries but not each other's interiors.
  if (!subregion_bound_.Contains(b->bound_)) return false;

  // Special cases to handle either loop being empty or full.
  if (is_empty_or_full() || b->is_empty_or_full()) {
    return is_full() || b->is_empty();
  }

  // Check whether there are any edge crossings, and also check the loop
  // relationship at any shared vertices.
  ContainsRelation relation;
  if (HasCrossingRelation(*this, *b, &relation)) return false;

  // There are no crossings, and if there are any shared vertices then A
  // contains B locally at each shared vertex.
  if (relation.found_shared_vertex()) return true;

  // Since there are no edge intersections or shared vertices, we just need to
  // test condition (3) above.  We can skip this test if we discovered that A
  // contains at least one point of B while checking for edge crossings.
  if (!Contains(b->vertex(0))) return false;

  // We still need to check whether (A union B) is the entire sphere.
  // Normally this check is very cheap due to the bounding box precondition.
  if ((b->subregion_bound_.Contains(bound_) ||
       b->bound_.Union(bound_).is_full()) && b->Contains(vertex(0))) {
    return false;
  }
  return true;
}


// Loop relation for Intersects().
class IntersectsRelation : public LoopRelation {
 public:
  IntersectsRelation() : found_shared_vertex_(false) {}
  bool found_shared_vertex() const { return found_shared_vertex_; }

  // If A.Contains(P) == true && B.Contains(P) == true, it is equivalent to
  // having an edge crossing (i.e., Intersects returns true).
  int a_crossing_target() const override { return true; }
  int b_crossing_target() const override { return true; }

  bool WedgesCross(const S2Point& a0, const S2Point& ab1, const S2Point& a2,
                   const S2Point& b0, const S2Point& b2) override {
    found_shared_vertex_ = true;
    return S2::WedgeIntersects(a0, ab1, a2, b0, b2);
  }

 private:
  bool found_shared_vertex_;
};

bool S2Loop::Intersects(const S2Loop* b) const {
  // a->Intersects(b) if and only if !a->Complement()->Contains(b).
  // This code is similar to Contains(), but is optimized for the case
  // where both loops enclose less than half of the sphere.
  if (!bound_.Intersects(b->bound_)) return false;

  // Check whether there are any edge crossings, and also check the loop
  // relationship at any shared vertices.
  IntersectsRelation relation;
  if (HasCrossingRelation(*this, *b, &relation)) return true;
  if (relation.found_shared_vertex()) return false;

  // Since there are no edge intersections or shared vertices, the loops
  // intersect only if A contains B, B contains A, or the two loops contain
  // each other's boundaries.  These checks are usually cheap because of the
  // bounding box preconditions.  Note that neither loop is empty (because of
  // the bounding box check above), so it is safe to access vertex(0).

  // Check whether A contains B, or A and B contain each other's boundaries.
  // (Note that A contains all the vertices of B in either case.)
  if (subregion_bound_.Contains(b->bound_) ||
      bound_.Union(b->bound_).is_full()) {
    if (Contains(b->vertex(0))) return true;
  }
  // Check whether B contains A.
  if (b->subregion_bound_.Contains(bound_)) {
    if (b->Contains(vertex(0))) return true;
  }
  return false;
}

// Returns true if the wedge (a0, ab1, a2) contains the "semiwedge" defined as
// any non-empty open set of rays immediately CCW from the edge (ab1, b2).  If
// "reverse_b" is true, then substitute "clockwise" for "CCW"; this simulates
// what would happen if the direction of loop B was reversed.
inline static bool WedgeContainsSemiwedge(const S2Point& a0, const S2Point& ab1,
                                          const S2Point& a2, const S2Point& b2,
                                          bool reverse_b) {
  if (b2 == a0 || b2 == a2) {
    // We have a shared or reversed edge.
    return (b2 == a0) == reverse_b;
  } else {
    return s2pred::OrderedCCW(a0, a2, b2, ab1);
  }
}

// Loop relation for CompareBoundary().
class CompareBoundaryRelation : public LoopRelation {
 public:
  explicit CompareBoundaryRelation(bool reverse_b):
      reverse_b_(reverse_b), found_shared_vertex_(false),
      contains_edge_(false), excludes_edge_(false) {
  }
  bool found_shared_vertex() const { return found_shared_vertex_; }
  bool contains_edge() const { return contains_edge_; }

  // The CompareBoundary relation does not have a useful early-exit condition,
  // so we return -1 for both crossing targets.
  //
  // Aside: A possible early exit condition could be based on the following.
  //   If A contains a point of both B and ~B, then A intersects Boundary(B).
  //   If ~A contains a point of both B and ~B, then ~A intersects Boundary(B).
  //   So if the intersections of {A, ~A} with {B, ~B} are all non-empty,
  //   the return value is 0, i.e., Boundary(A) intersects Boundary(B).
  // Unfortunately it isn't worth detecting this situation because by the
  // time we have seen a point in all four intersection regions, we are also
  // guaranteed to have seen at least one pair of crossing edges.
  int a_crossing_target() const override { return -1; }
  int b_crossing_target() const override { return -1; }

  bool WedgesCross(const S2Point& a0, const S2Point& ab1, const S2Point& a2,
                   const S2Point& b0, const S2Point& b2) override {
    // Because we don't care about the interior of B, only its boundary, it is
    // sufficient to check whether A contains the semiwedge (ab1, b2).
    found_shared_vertex_ = true;
    if (WedgeContainsSemiwedge(a0, ab1, a2, b2, reverse_b_)) {
      contains_edge_ = true;
    } else {
      excludes_edge_ = true;
    }
    return contains_edge_ & excludes_edge_;
  }

 protected:
  const bool reverse_b_;      // True if loop B should be reversed.
  bool found_shared_vertex_;  // True if any wedge was processed.
  bool contains_edge_;        // True if any edge of B is contained by A.
  bool excludes_edge_;        // True if any edge of B is excluded by A.
};

int S2Loop::CompareBoundary(const S2Loop* b) const {
  S2_DCHECK(!is_empty() && !b->is_empty());
  S2_DCHECK(!b->is_full() || !b->is_hole());

  // The bounds must intersect for containment or crossing.
  if (!bound_.Intersects(b->bound_)) return -1;

  // Full loops are handled as though the loop surrounded the entire sphere.
  if (is_full()) return 1;
  if (b->is_full()) return -1;

  // Check whether there are any edge crossings, and also check the loop
  // relationship at any shared vertices.
  CompareBoundaryRelation relation(b->is_hole());
  if (HasCrossingRelation(*this, *b, &relation)) return 0;
  if (relation.found_shared_vertex()) {
    return relation.contains_edge() ? 1 : -1;
  }

  // There are no edge intersections or shared vertices, so we can check
  // whether A contains an arbitrary vertex of B.
  return Contains(b->vertex(0)) ? 1 : -1;
}

bool S2Loop::ContainsNonCrossingBoundary(const S2Loop* b, bool reverse_b)
    const {
  S2_DCHECK(!is_empty() && !b->is_empty());
  S2_DCHECK(!b->is_full() || !reverse_b);

  // The bounds must intersect for containment.
  if (!bound_.Intersects(b->bound_)) return false;

  // Full loops are handled as though the loop surrounded the entire sphere.
  if (is_full()) return true;
  if (b->is_full()) return false;

  int m = FindVertex(b->vertex(0));
  if (m < 0) {
    // Since vertex b0 is not shared, we can check whether A contains it.
    return Contains(b->vertex(0));
  }
  // Otherwise check whether the edge (b0, b1) is contained by A.
  return WedgeContainsSemiwedge(vertex(m-1), vertex(m), vertex(m+1),
                                b->vertex(1), reverse_b);
}

bool S2Loop::ContainsNested(const S2Loop* b) const {
  if (!subregion_bound_.Contains(b->bound_)) return false;

  // Special cases to handle either loop being empty or full.  Also bail out
  // when B has no vertices to avoid heap overflow on the vertex(1) call
  // below.  (This method is called during polygon initialization before the
  // client has an opportunity to call IsValid().)
  if (is_empty_or_full() || b->num_vertices() < 2) {
    return is_full() || b->is_empty();
  }

  // We are given that A and B do not share any edges, and that either one
  // loop contains the other or they do not intersect.
  int m = FindVertex(b->vertex(1));
  if (m < 0) {
    // Since b->vertex(1) is not shared, we can check whether A contains it.
    return Contains(b->vertex(1));
  }
  // Check whether the edge order around b->vertex(1) is compatible with
  // A containing B.
  return S2::WedgeContains(vertex(m-1), vertex(m), vertex(m+1),
                                   b->vertex(0), b->vertex(2));
}

bool S2Loop::Equals(const S2Loop* b) const {
  if (num_vertices() != b->num_vertices()) return false;
  for (int i = 0; i < num_vertices(); ++i) {
    if (vertex(i) != b->vertex(i)) return false;
  }
  return true;
}

bool S2Loop::BoundaryEquals(const S2Loop* b) const {
  if (num_vertices() != b->num_vertices()) return false;

  // Special case to handle empty or full loops.  Since they have the same
  // number of vertices, if one loop is empty/full then so is the other.
  if (is_empty_or_full()) return is_empty() == b->is_empty();

  for (int offset = 0; offset < num_vertices(); ++offset) {
    if (vertex(offset) == b->vertex(0)) {
      // There is at most one starting offset since loop vertices are unique.
      for (int i = 0; i < num_vertices(); ++i) {
        if (vertex(i + offset) != b->vertex(i)) return false;
      }
      return true;
    }
  }
  return false;
}

bool S2Loop::BoundaryApproxEquals(const S2Loop& b, S1Angle max_error) const {
  if (num_vertices() != b.num_vertices()) return false;

  // Special case to handle empty or full loops.  Since they have the same
  // number of vertices, if one loop is empty/full then so is the other.
  if (is_empty_or_full()) return is_empty() == b.is_empty();

  for (int offset = 0; offset < num_vertices(); ++offset) {
    if (S2::ApproxEquals(vertex(offset), b.vertex(0), max_error)) {
      bool success = true;
      for (int i = 0; i < num_vertices(); ++i) {
        if (!S2::ApproxEquals(vertex(i + offset), b.vertex(i), max_error)) {
          success = false;
          break;
        }
      }
      if (success) return true;
      // Otherwise continue looping.  There may be more than one candidate
      // starting offset since vertices are only matched approximately.
    }
  }
  return false;
}

static bool MatchBoundaries(const S2Loop& a, const S2Loop& b, int a_offset,
                            S1Angle max_error) {
  // The state consists of a pair (i,j).  A state transition consists of
  // incrementing either "i" or "j".  "i" can be incremented only if
  // a(i+1+a_offset) is near the edge from b(j) to b(j+1), and a similar rule
  // applies to "j".  The function returns true iff we can proceed all the way
  // around both loops in this way.
  //
  // Note that when "i" and "j" can both be incremented, sometimes only one
  // choice leads to a solution.  We handle this using a stack and
  // backtracking.  We also keep track of which states have already been
  // explored to avoid duplicating work.

  vector<pair<int, int>> pending;
  set<pair<int, int>> done;
  pending.push_back(std::make_pair(0, 0));
  while (!pending.empty()) {
    int i = pending.back().first;
    int j = pending.back().second;
    pending.pop_back();
    if (i == a.num_vertices() && j == b.num_vertices()) {
      return true;
    }
    done.insert(std::make_pair(i, j));

    // If (i == na && offset == na-1) where na == a->num_vertices(), then
    // then (i+1+offset) overflows the [0, 2*na-1] range allowed by vertex().
    // So we reduce the range if necessary.
    int io = i + a_offset;
    if (io >= a.num_vertices()) io -= a.num_vertices();

    if (i < a.num_vertices() && done.count(std::make_pair(i + 1, j)) == 0 &&
        S2::GetDistance(a.vertex(io + 1), b.vertex(j),
                                b.vertex(j + 1)) <= max_error) {
      pending.push_back(std::make_pair(i + 1, j));
    }
    if (j < b.num_vertices() && done.count(std::make_pair(i, j + 1)) == 0 &&
        S2::GetDistance(b.vertex(j + 1), a.vertex(io),
                                a.vertex(io + 1)) <= max_error) {
      pending.push_back(std::make_pair(i, j + 1));
    }
  }
  return false;
}

bool S2Loop::BoundaryNear(const S2Loop& b, S1Angle max_error) const {
  // Special case to handle empty or full loops.
  if (is_empty_or_full() || b.is_empty_or_full()) {
    return (is_empty() && b.is_empty()) || (is_full() && b.is_full());
  }

  for (int a_offset = 0; a_offset < num_vertices(); ++a_offset) {
    if (MatchBoundaries(*this, b, a_offset, max_error)) return true;
  }
  return false;
}

void S2Loop::GetXYZFaceSiTiVertices(S2XYZFaceSiTi* vertices) const {
  for (int i = 0; i < num_vertices(); ++i) {
    vertices[i].xyz = vertex(i);
    vertices[i].cell_level = S2::XYZtoFaceSiTi(vertices[i].xyz,
        &vertices[i].face, &vertices[i].si, &vertices[i].ti);
  }
}

void S2Loop::EncodeCompressed(Encoder* encoder, const S2XYZFaceSiTi* vertices,
                              int snap_level) const {
  // Ensure enough for the data we write before S2EncodePointsCompressed.
  // S2EncodePointsCompressed ensures its space.
  encoder->Ensure(Encoder::kVarintMax32);
  encoder->put_varint32(num_vertices_);

  S2EncodePointsCompressed(MakeSpan(vertices, num_vertices_),
                           snap_level, encoder);

  std::bitset<kNumProperties> properties = GetCompressedEncodingProperties();

  // Ensure enough only for what we write.  Let the bound ensure its own
  // space.
  encoder->Ensure(2 * Encoder::kVarintMax32);
  encoder->put_varint32(properties.to_ulong());
  encoder->put_varint32(depth_);
  if (properties.test(kBoundEncoded)) {
    bound_.Encode(encoder);
  }
  S2_DCHECK_GE(encoder->avail(), 0);
}

bool S2Loop::DecodeCompressed(Decoder* decoder, int snap_level) {
  // get_varint32 takes a uint32*, but num_vertices_ is signed.
  // Decode to a temporary variable to avoid reinterpret_cast.
  uint32 unsigned_num_vertices;
  if (!decoder->get_varint32(&unsigned_num_vertices)) {
    return false;
  }
  if (unsigned_num_vertices == 0 ||
      unsigned_num_vertices > FLAGS_s2polygon_decode_max_num_vertices) {
    return false;
  }
  ClearIndex();
  if (owns_vertices_) delete[] vertices_;
  num_vertices_ = unsigned_num_vertices;
  vertices_ = new S2Point[num_vertices_];
  owns_vertices_ = true;

  if (!S2DecodePointsCompressed(decoder, snap_level,
                                MakeSpan(vertices_, num_vertices_))) {
    return false;
  }
  uint32 properties_uint32;
  if (!decoder->get_varint32(&properties_uint32)) {
    return false;
  }
  const std::bitset<kNumProperties> properties(properties_uint32);
  origin_inside_ = properties.test(kOriginInside);

  uint32 unsigned_depth;
  if (!decoder->get_varint32(&unsigned_depth)) {
    return false;
  }
  depth_ = unsigned_depth;

  if (properties.test(kBoundEncoded)) {
    if (!bound_.Decode(decoder)) {
      return false;
    }
    subregion_bound_ = S2LatLngRectBounder::ExpandForSubregions(bound_);
  } else {
    InitBound();
  }
  InitIndex();
  return true;
}

std::bitset<kNumProperties> S2Loop::GetCompressedEncodingProperties() const {
  std::bitset<kNumProperties> properties;
  if (origin_inside_) {
    properties.set(kOriginInside);
  }

  // Write whether there is a bound so we can change the threshold later.
  // Recomputing the bound multiplies the decode time taken per vertex
  // by a factor of about 3.5.  Without recomputing the bound, decode
  // takes approximately 125 ns / vertex.  A loop with 63 vertices
  // encoded without the bound will take ~30us to decode, which is
  // acceptable.  At ~3.5 bytes / vertex without the bound, adding
  // the bound will increase the size by <15%, which is also acceptable.
  static const int kMinVerticesForBound = 64;
  if (num_vertices_ >= kMinVerticesForBound) {
    properties.set(kBoundEncoded);
  }
  return properties;
}

/* static */
std::unique_ptr<S2Loop> S2Loop::MakeRegularLoop(const S2Point& center,
                                                S1Angle radius,
                                                int num_vertices) {
  Matrix3x3_d m;
  S2::GetFrame(center, &m);  // TODO(ericv): Return by value
  return MakeRegularLoop(m, radius, num_vertices);
}

/* static */
std::unique_ptr<S2Loop> S2Loop::MakeRegularLoop(const Matrix3x3_d& frame,
                                                S1Angle radius,
                                                int num_vertices) {
  // We construct the loop in the given frame coordinates, with the center at
  // (0, 0, 1).  For a loop of radius "r", the loop vertices have the form
  // (x, y, z) where x^2 + y^2 = sin(r) and z = cos(r).  The distance on the
  // sphere (arc length) from each vertex to the center is acos(cos(r)) = r.
  double z = cos(radius.radians());
  double r = sin(radius.radians());
  double radian_step = 2 * M_PI / num_vertices;
  vector<S2Point> vertices;
  for (int i = 0; i < num_vertices; ++i) {
    double angle = i * radian_step;
    S2Point p(r * cos(angle), r * sin(angle), z);
    vertices.push_back(S2::FromFrame(frame, p).Normalize());
  }
  return make_unique<S2Loop>(vertices);
}

size_t S2Loop::SpaceUsed() const {
  size_t size = sizeof(*this);
  size += num_vertices() * sizeof(S2Point);
  // index_ itself is already included in sizeof(*this).
  size += index_.SpaceUsed() - sizeof(index_);
  return size;
}

int S2Loop::Shape::num_chains() const {
  return loop_->is_empty() ? 0 : 1;
}

S2Shape::Chain S2Loop::Shape::chain(int i) const {
  S2_DCHECK_EQ(i, 0);
  return Chain(0, Shape::num_edges());  // Avoid virtual call.
}
