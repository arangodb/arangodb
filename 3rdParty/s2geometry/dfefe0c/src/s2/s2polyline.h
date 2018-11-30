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

#ifndef S2_S2POLYLINE_H_
#define S2_S2POLYLINE_H_

#include <memory>
#include <vector>

#include "s2/base/logging.h"
#include "s2/_fp_contract_off.h"
#include "s2/s1angle.h"
#include "s2/s2debug.h"
#include "s2/s2error.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2region.h"
#include "s2/s2shape.h"
#include "s2/third_party/absl/base/macros.h"
#include "s2/third_party/absl/memory/memory.h"

class Decoder;
class Encoder;
class S1Angle;
class S2Cap;
class S2Cell;
class S2LatLng;

// An S2Polyline represents a sequence of zero or more vertices connected by
// straight edges (geodesics).  Edges of length 0 and 180 degrees are not
// allowed, i.e. adjacent vertices should not be identical or antipodal.
class S2Polyline final : public S2Region {
 public:
  // Creates an empty S2Polyline that should be initialized by calling Init()
  // or Decode().
  S2Polyline();

#ifndef SWIG
  // S2Polyline is movable, but only privately copyable.
  S2Polyline(S2Polyline&&);
  S2Polyline& operator=(S2Polyline&&);
#endif  // SWIG

  // Convenience constructors that call Init() with the given vertices.
  explicit S2Polyline(const std::vector<S2Point>& vertices);
  explicit S2Polyline(const std::vector<S2LatLng>& vertices);

  // Convenience constructors to disable the automatic validity checking
  // controlled by the --s2debug flag.  Example:
  //
  //   S2Polyline* line = new S2Polyline(vertices, S2Debug::DISABLE);
  //
  // This is equivalent to:
  //
  //   S2Polyline* line = new S2Polyline;
  //   line->set_s2debug_override(S2Debug::DISABLE);
  //   line->Init(vertices);
  //
  // The main reason to use this constructors is if you intend to call
  // IsValid() explicitly.  See set_s2debug_override() for details.
  S2Polyline(const std::vector<S2Point>& vertices, S2Debug override);
  S2Polyline(const std::vector<S2LatLng>& vertices, S2Debug override);

  // Initialize a polyline that connects the given vertices. Empty polylines are
  // allowed.  Adjacent vertices should not be identical or antipodal.  All
  // vertices should be unit length.
  void Init(const std::vector<S2Point>& vertices);

  // Convenience initialization function that accepts latitude-longitude
  // coordinates rather than S2Points.
  void Init(const std::vector<S2LatLng>& vertices);

  ~S2Polyline() override;

  // Allows overriding the automatic validity checks controlled by the
  // --s2debug flag.  If this flag is true, then polylines are automatically
  // checked for validity as they are initialized.  The main reason to disable
  // this flag is if you intend to call IsValid() explicitly, like this:
  //
  //   S2Polyline line;
  //   line.set_s2debug_override(S2Debug::DISABLE);
  //   line.Init(...);
  //   if (!line.IsValid()) { ... }
  //
  // Without the call to set_s2debug_override(), invalid data would cause a
  // fatal error in Init() whenever the --s2debug flag is enabled.
  void set_s2debug_override(S2Debug override);
  S2Debug s2debug_override() const;

  // Return true if the given vertices form a valid polyline.
  bool IsValid() const;

  // Returns true if this is *not* a valid polyline and sets "error"
  // appropriately.  Otherwise returns false and leaves "error" unchanged.
  //
  // REQUIRES: error != nullptr
  bool FindValidationError(S2Error* error) const;

  int num_vertices() const { return num_vertices_; }
  const S2Point& vertex(int k) const {
    S2_DCHECK_GE(k, 0);
    S2_DCHECK_LT(k, num_vertices_);
    return vertices_[k];
  }

  // Return the length of the polyline.
  S1Angle GetLength() const;

  // Return the true centroid of the polyline multiplied by the length of the
  // polyline (see s2centroids.h for details on centroids).  The result is not
  // unit length, so you may want to normalize it.
  //
  // Prescaling by the polyline length makes it easy to compute the centroid
  // of several polylines (by simply adding up their centroids).
  S2Point GetCentroid() const;

  // Return the point whose distance from vertex 0 along the polyline is the
  // given fraction of the polyline's total length.  Fractions less than zero
  // or greater than one are clamped.  The return value is unit length.  This
  // cost of this function is currently linear in the number of vertices.
  // The polyline must not be empty.
  S2Point Interpolate(double fraction) const;

  // Like Interpolate(), but also return the index of the next polyline
  // vertex after the interpolated point P.  This allows the caller to easily
  // construct a given suffix of the polyline by concatenating P with the
  // polyline vertices starting at "next_vertex".  Note that P is guaranteed
  // to be different than vertex(*next_vertex), so this will never result in
  // a duplicate vertex.
  //
  // The polyline must not be empty.  Note that if "fraction" >= 1.0, then
  // "next_vertex" will be set to num_vertices() (indicating that no vertices
  // from the polyline need to be appended).  The value of "next_vertex" is
  // always between 1 and num_vertices().
  //
  // This method can also be used to construct a prefix of the polyline, by
  // taking the polyline vertices up to "next_vertex - 1" and appending the
  // returned point P if it is different from the last vertex (since in this
  // case there is no guarantee of distinctness).
  S2Point GetSuffix(double fraction, int* next_vertex) const;

  // The inverse operation of GetSuffix/Interpolate.  Given a point on the
  // polyline, returns the ratio of the distance to the point from the
  // beginning of the polyline over the length of the polyline.  The return
  // value is always betwen 0 and 1 inclusive.  See GetSuffix() for the
  // meaning of "next_vertex".
  //
  // The polyline should not be empty.  If it has fewer than 2 vertices, the
  // return value is zero.
  double UnInterpolate(const S2Point& point, int next_vertex) const;

  // Given a point, returns a point on the polyline that is closest to the given
  // point.  See GetSuffix() for the meaning of "next_vertex", which is chosen
  // here w.r.t. the projected point as opposed to the interpolated point in
  // GetSuffix().
  //
  // The polyline must be non-empty.
  S2Point Project(const S2Point& point, int* next_vertex) const;

  // Returns true if the point given is on the right hand side of the polyline,
  // using a naive definition of "right-hand-sideness" where the point is on
  // the RHS of the polyline iff the point is on the RHS of the line segment in
  // the polyline which it is closest to.
  //
  // The polyline must have at least 2 vertices.
  bool IsOnRight(const S2Point& point) const;

  // Return true if this polyline intersects the given polyline. If the
  // polylines share a vertex they are considered to be intersecting. When a
  // polyline endpoint is the only intersection with the other polyline, the
  // function may return true or false arbitrarily.
  //
  // The running time is quadratic in the number of vertices.  (To intersect
  // polylines more efficiently, or compute the actual intersection geometry,
  // use S2BooleanOperation.)
  bool Intersects(const S2Polyline* line) const;

  // Reverse the order of the polyline vertices.
  void Reverse();

  // Return a subsequence of vertex indices such that the polyline connecting
  // these vertices is never further than "tolerance" from the original
  // polyline.  Provided the first and last vertices are distinct, they are
  // always preserved; if they are not, the subsequence may contain only a
  // single index.
  //
  // Some useful properties of the algorithm:
  //
  //  - It runs in linear time.
  //
  //  - The output is always a valid polyline.  In particular, adjacent
  //    output vertices are never identical or antipodal.
  //
  //  - The method is not optimal, but it tends to produce 2-3% fewer
  //    vertices than the Douglas-Peucker algorithm with the same tolerance.
  //
  //  - The output is *parametrically* equivalent to the original polyline to
  //    within the given tolerance.  For example, if a polyline backtracks on
  //    itself and then proceeds onwards, the backtracking will be preserved
  //    (to within the given tolerance).  This is different than the
  //    Douglas-Peucker algorithm, which only guarantees geometric equivalence.
  //
  // See also S2PolylineSimplifier, which uses the same algorithm but is more
  // efficient and supports more features, and also S2Builder, which can
  // simplify polylines and polygons, supports snapping (e.g. to E7 lat/lng
  // coordinates or S2CellId centers), and can split polylines at intersection
  // points.
  void SubsampleVertices(S1Angle tolerance, std::vector<int>* indices) const;

  // Return true if two polylines are exactly the same.
  bool Equals(const S2Polyline* b) const;

  // Return true if two polylines have the same number of vertices, and
  // corresponding vertex pairs are separated by no more than "max_error".
  // (For testing purposes.)
  bool ApproxEquals(const S2Polyline& b,
                    S1Angle max_error = S1Angle::Radians(1e-15)) const;

  // Return true if "covered" is within "max_error" of a contiguous subpath of
  // this polyline over its entire length.  Specifically, this method returns
  // true if this polyline has parameterization a:[0,1] -> S^2, "covered" has
  // parameterization b:[0,1] -> S^2, and there is a non-decreasing function
  // f:[0,1] -> [0,1] such that distance(a(f(t)), b(t)) <= max_error for all t.
  //
  // You can think of this as testing whether it is possible to drive a car
  // along "covered" and a car along some subpath of this polyline such that no
  // car ever goes backward, and the cars are always within "max_error" of each
  // other.
  //
  // This function is well-defined for empty polylines:
  //    anything.covers(empty) = true
  //    empty.covers(nonempty) = false
  bool NearlyCovers(const S2Polyline& covered, S1Angle max_error) const;

  // Returns the total number of bytes used by the polyline.
  size_t SpaceUsed() const;

  ////////////////////////////////////////////////////////////////////////
  // S2Region interface (see s2region.h for details):

  S2Polyline* Clone() const override;
  S2Cap GetCapBound() const override;
  S2LatLngRect GetRectBound() const override;
  bool Contains(const S2Cell& cell) const override { return false; }
  bool MayIntersect(const S2Cell& cell) const override;

  // Always return false, because "containment" is not numerically
  // well-defined except at the polyline vertices.
  bool Contains(const S2Point& p) const override { return false; }

  // Appends a serialized representation of the S2Polyline to "encoder".
  //
  // REQUIRES: "encoder" uses the default constructor, so that its buffer
  //           can be enlarged as necessary by calling Ensure(int).
  void Encode(Encoder* const encoder) const;

  // Decodes an S2Polyline encoded with Encode().  Returns true on success.
  bool Decode(Decoder* const decoder);

#ifndef SWIG
  // Wrapper class for indexing a polyline (see S2ShapeIndex).  Once this
  // object is inserted into an S2ShapeIndex it is owned by that index, and
  // will be automatically deleted when no longer needed by the index.  Note
  // that this class does not take ownership of the polyline itself (see
  // OwningShape below).  You can also subtype this class to store additional
  // data (see S2Shape for details).
  class Shape : public S2Shape {
   public:
    static constexpr TypeTag kTypeTag = 2;

    Shape() : polyline_(nullptr) {}  // Must call Init().

    // Initialization.  Does not take ownership of "polyline".
    //
    // Note that a polyline with one vertex is defined to have no edges.  Use
    // S2LaxPolylineShape or S2LaxClosedPolylineShape if you want to define a
    // polyline consisting of a single degenerate edge.
    explicit Shape(const S2Polyline* polyline) { Init(polyline); }
    void Init(const S2Polyline* polyline);

    const S2Polyline* polyline() const { return polyline_; }

    // Encodes the polyline using S2Polyline::Encode().
    void Encode(Encoder* encoder) const {
      // TODO(geometry-library): Support compressed encodings.
      polyline_->Encode(encoder);
    }

    // Decoding is defined only for S2Polyline::OwningShape below.

    // S2Shape interface:

    int num_edges() const final {
      return std::max(0, polyline_->num_vertices() - 1);
    }
    Edge edge(int e) const final {
      return Edge(polyline_->vertex(e), polyline_->vertex(e + 1));
    }
    int dimension() const final { return 1; }
    ReferencePoint GetReferencePoint() const final {
      return ReferencePoint::Contained(false);
    }
    int num_chains() const final;
    Chain chain(int i) const final;
    Edge chain_edge(int i, int j) const final {
      S2_DCHECK_EQ(i, 0);
      return Edge(polyline_->vertex(j), polyline_->vertex(j + 1));
    }
    ChainPosition chain_position(int e) const final {
      return ChainPosition(0, e);
    }
    TypeTag type_tag() const override { return kTypeTag; }

   private:
    const S2Polyline* polyline_;
  };

  // Like Shape, except that the S2Polyline is automatically deleted when this
  // object is deleted by the S2ShapeIndex.  This is useful when an S2Polyline
  // is constructed solely for the purpose of indexing it.
  class OwningShape : public Shape {
   public:
    OwningShape() {}  // Must call Init().

    explicit OwningShape(std::unique_ptr<const S2Polyline> polyline)
        : Shape(polyline.release()) {
    }

    void Init(std::unique_ptr<const S2Polyline> polyline) {
      Shape::Init(polyline.release());
    }

    bool Init(Decoder* decoder) {
      auto polyline = absl::make_unique<S2Polyline>();
      if (!polyline->Decode(decoder)) return false;
      Shape::Init(polyline.release());
      return true;
    }

    ~OwningShape() override { delete polyline(); }
  };
#endif  // SWIG

 private:
  // Internal copy constructor used only by Clone() that makes a deep copy of
  // its argument.
  S2Polyline(const S2Polyline& src);

  // Allows overriding the automatic validity checking controlled by the
  // --s2debug flag.
  S2Debug s2debug_override_;

  // We store the vertices in an array rather than a vector because we don't
  // need any STL methods, and computing the number of vertices using size()
  // would be relatively expensive (due to division by sizeof(S2Point) == 24).
  int num_vertices_ = 0;
  std::unique_ptr<S2Point[]> vertices_;

#ifndef SWIG
  void operator=(const S2Polyline&) = delete;
#endif  // SWIG
};

#endif  // S2_S2POLYLINE_H_
