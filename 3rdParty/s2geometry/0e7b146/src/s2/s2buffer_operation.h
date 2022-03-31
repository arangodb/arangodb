// Copyright 2020 Google Inc. All Rights Reserved.
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

#ifndef S2_S2BUFFER_OPERATION_H_
#define S2_S2BUFFER_OPERATION_H_

#include <vector>

#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s2builder.h"
#include "s2/s2point_span.h"
#include "s2/s2winding_operation.h"

// This class provides a way to expand an arbitrary collection of geometry by
// a fixed radius (an operation variously known as "buffering", "offsetting",
// or "Minkowski sum with a disc").  The output consists of a polygon
// (possibly with multiple shells) that contains all points within the given
// radius of the original geometry.
//
// The radius can also be negative, in which case the geometry is contracted.
// This causes the boundaries of polygons to shrink or disappear, and removes
// all points and polylines.
//
// The input consists of a sequence of layers.  Each layer may consist of any
// combination of points, polylines, and polygons, with the restriction that
// polygon interiors within each layer may not intersect any other geometry
// (including other polygon interiors).  The output is the union of the
// buffered input layers.  Note that only a single layer is allowed if the
// buffer radius is negative.
//
// This class may be used to compute polygon unions by setting the buffer
// radius to zero.  The union is computed using a single snapping operation.
//
// Note that if you only want to compute an S2CellId covering of the buffered
// geometry, it is much faster to use S2ShapeIndexBufferedRegion instead.
//
// Keywords: buffer, buffering, expand, expanding, offset, offsetting,
//           widen, contract, shrink, Minkowski sum
class S2BufferOperation {
 public:
  // For polylines, specifies whether the end caps should be round or flat.
  // See Options::set_end_cap_style() below.
  enum class EndCapStyle : uint8 { ROUND, FLAT };

  // Specifies whether polylines should be buffered only on the left, only on
  // the right, or on both sides.
  enum class PolylineSide : uint8 { LEFT, RIGHT, BOTH };

  class Options {
   public:
    Options();

    // Convenience constructor that calls set_buffer_radius().
    explicit Options(S1Angle buffer_radius);

    // If positive, specifies that all points within the given radius of the
    // input geometry should be added to the output.  If negative, specifies
    // that all points within the given radius of complement of the input
    // geometry should be subtracted from the output.  If the buffer radius
    // is zero then the input geometry is passed through to the output layer
    // after first converting points and polylines into degenerate loops.
    //
    // DEFAULT: S1Angle::Zero()
    S1Angle buffer_radius() const;
    void set_buffer_radius(S1Angle buffer_radius);

    // Specifies the allowable error when buffering, expressed as a fraction
    // of buffer_radius().  The actual buffer distance will be in the range
    // [(1-f) * r - C, (1 + f) * r + C] where "f" is the error fraction, "r"
    // is the buffer radius, and "C" is S2BufferOperation::kAbsError.
    //
    // Be aware that the number of output edges increases proportionally to
    // (1 / sqrt(error_fraction)), so setting a small value may increase the
    // size of the output considerably.
    //
    // REQUIRES: error_fraction() >= kMinErrorFraction
    // REQUIRES: error_fraction() <= 1.0
    //
    // DEFAULT: 0.01  (i.e., maximum error of 1%)
    static constexpr double kMinErrorFraction = 1e-6;
    double error_fraction() const;
    void set_error_fraction(double error_fraction);

    // Returns the maximum error in the buffered result for the current
    // buffer_radius(), error_fraction(), and snap_function().  Note that the
    // error due to buffering consists of both relative errors (those
    // proportional to the buffer radius) and absolute errors.  The maximum
    // relative error is controlled by error_fraction(), while the maximum
    // absolute error is about 10 nanometers on the Earth's surface and is
    // defined internally.  The error due to snapping is defined by the
    // specified snap_function().
    const S1Angle max_error() const;

    // Alternatively, error_fraction() may be specified as the number of
    // polyline segments used to approximate a planar circle.  These two
    // values are related according to the formula
    //
    //    error_fraction = (1 - cos(theta)) / (1 + cos(theta))
    //                  ~= 0.25 * (theta ** 2)
    //
    // where (theta == Pi / circle_segments), i.e. error decreases
    // quadratically with the number of circle segments.
    //
    // REQUIRES: circle_segments() >= 2.0
    // REQUIRES: circle_segments() <= kMaxCircleSegments
    //           (about 1570; corresponds to kMinErrorFraction)
    //
    // DEFAULT: about 15.76 (corresponding to  error_fraction() default value)
    static constexpr double kMaxCircleSegments = 1570.7968503979573;
    double circle_segments() const;
    void set_circle_segments(double circle_segments);

    // For polylines, specifies whether the end caps should be round or flat.
    //
    // Note that with flat end caps, there is no buffering beyond the polyline
    // endpoints (unlike "square" end caps, which are not implemented).
    //
    // DEFAULT: EndCapStyle::ROUND
    EndCapStyle end_cap_style() const;
    void set_end_cap_style(EndCapStyle end_cap_style);

    // Specifies whether polylines should be buffered only on the left, only
    // on the right, or on both sides.  For one-sided buffering please note
    // the following:
    //
    //  - EndCapStyle::ROUND yields two quarter-circles, one at each end.
    //
    //  - To buffer by a different radius on each side of the polyline, you
    //    can use two S2BufferOperations and compute their union.  (Note that
    //    round end caps will yield two quarter-circles at each end of the
    //    polyline with different radii.)
    //
    //  - Polylines consisting of a single degenerate edge are always buffered
    //    identically to points, i.e. this option has no effect.
    //
    //  - When the polyline turns right by more than 90 degrees, buffering may
    //    or may not extend to the non-buffered side of the polyline.  For
    //    example if ABC makes a 170 degree right turn at B, it is unspecified
    //    whether the buffering of AB extends across edge BC and vice versa.
    //    Similarly if ABCD represents two right turns of 90 degrees where AB
    //    and CD are separated by less than the buffer radius, it is
    //    unspecified whether buffering of AB extends across CD and vice versa.
    //
    // DEFAULT: PolylineSide::BOTH
    PolylineSide polyline_side() const;
    void set_polyline_side(PolylineSide polyline_side);

    // Specifies the function used for snap rounding the output during the
    // call to Build().  Note that any errors due to snapping are in addition
    // to those specified by error_fraction().
    //
    // DEFAULT: s2builderutil::IdentitySnapFunction(S1Angle::Zero())
    const S2Builder::SnapFunction& snap_function() const;
    void set_snap_function(const S2Builder::SnapFunction& snap_function);

    // Specifies that internal memory usage should be tracked using the given
    // S2MemoryTracker.  If a memory limit is specified and more more memory
    // than this is required then an error will be returned.  Example usage:
    //
    //   S2MemoryTracker tracker;
    //   tracker.set_limit(500 << 20);  // 500 MB
    //   S2BufferOperation::Options options;
    //   options.set_buffer_radius(S1Angle::Degrees(1e-5));
    //   options.set_memory_tracker(&tracker);
    //   S2BufferOperation op{options};
    //   ...
    //   S2Error error;
    //   if (!op.Build(&error)) {
    //     if (error.code() == S2Error::RESOURCE_EXHAUSTED) {
    //       S2_LOG(ERROR) << error;  // Memory limit exceeded
    //     }
    //   }
    //
    // CAVEATS:
    //
    //  - Memory allocated by the output S2Builder layer is not tracked.
    //
    //  - While memory tracking is reasonably complete and accurate, it does
    //    not account for every last byte.  It is intended only for the
    //    purpose of preventing clients from running out of memory.
    //
    // DEFAULT: nullptr (memory tracking disabled)
    S2MemoryTracker* memory_tracker() const;
    void set_memory_tracker(S2MemoryTracker* tracker);

    // Options may be assigned and copied.
    Options(const Options& options);
    Options& operator=(const Options& options);

   private:
    S1Angle buffer_radius_ = S1Angle::Zero();
    //    double error_fraction_ = 0.01;
    double error_fraction_ = 0.02;
    EndCapStyle end_cap_style_ = EndCapStyle::ROUND;
    PolylineSide polyline_side_ = PolylineSide::BOTH;
    std::unique_ptr<S2Builder::SnapFunction> snap_function_;
    S2MemoryTracker* memory_tracker_ = nullptr;
  };

  // Default constructor; requires Init() to be called.
  S2BufferOperation();

  // Convenience constructor that calls Init().
  explicit S2BufferOperation(std::unique_ptr<S2Builder::Layer> result_layer,
                             const Options& options = Options{});

  // Starts a buffer operation that sends the output polygon to the given
  // S2Builder layer.  This method may be called more than once.
  //
  // Note that buffering always yields a polygon, even if the input includes
  // polylines and points.  If the buffer radius is zero, points and polylines
  // will be converted into degenerate polygon loops; if the buffer radius is
  // negative, points and polylines will be removed.
  void Init(std::unique_ptr<S2Builder::Layer> result_layer,
            const Options& options = Options());

  const Options& options() const;

  // Each call below represents a different input layer.  Note that if the
  // buffer radius is negative, then at most one input layer is allowed
  // (ignoring any layers that contain only points and polylines).

  // Adds an input layer containing a single point.
  void AddPoint(const S2Point& point);

  // Adds an input layer containing a polyline.  Note the following:
  //
  //  - Polylines with 0 or 1 vertices are considered to be empty.
  //  - A polyline with 2 identical vertices is equivalent to a point.
  //  - Polylines have end caps (see Options::end_cap_style).
  //  - One-sided polyline buffering is supported (see Options::polyline_side).
  void AddPolyline(S2PointSpan polyline);

  // Adds an input layer containing a loop.  Note the following:
  //
  //  - A loop with no vertices is empty.
  //  - A loop with 1 vertex is equivalent to a point.
  //  - The interior of the loop is on its left.
  //  - Buffering a self-intersecting loop produces undefined results.
  void AddLoop(S2PointLoopSpan loop);

  // Adds an input layer containing the given shape.  Shapes are handled as
  // points, polylines, or polygons according to the rules above.  In addition
  // note the following:
  //
  //  - Polygons holes may be degenerate (e.g., consisting of a
  //    single vertex or entirely of sibling pairs such as ABCBDB).
  //  - Full polygons are supported.  Note that since full polygons do
  //    not have a boundary, they are not affected by buffering.
  void AddShape(const S2Shape& shape);

  // Adds an input layer containing all of the shapes in the given index.
  //
  // REQUIRES: The interiors of polygons must be disjoint from all other
  //           indexed geometry, including other polygon interiors.
  //           (S2BooleanOperation also requires this.)
  void AddShapeIndex(const S2ShapeIndex& index);

  // Computes the union of the buffered input shapes and sends the output
  // polygon to the S2Builder layer specified in the constructor.  Returns
  // true on success and otherwise sets "error" appropriately.
  //
  // Note that if the buffer radius is negative, only a single input layer is
  // allowed (ignoring any layers that contain only points and polylines).
  bool Build(S2Error* error);

 private:
  S1Angle GetMaxEdgeSpan(S1Angle radius, S1Angle requested_error) const;
  void SetInputVertex(const S2Point& new_a);
  void AddOffsetVertex(const S2Point& new_b);
  void CloseBufferRegion();
  void OutputPath();
  void UpdateRefWinding(const S2Point& a, const S2Point& b, const S2Point& c);
  void AddFullPolygon();
  S2Point GetEdgeAxis(const S2Point& a, const S2Point& b) const;
  void AddVertexArc(const S2Point& v, const S2Point& start, const S2Point& end);
  void CloseVertexArc(const S2Point& v, const S2Point& end);
  void AddEdgeArc(const S2Point& a, const S2Point& b);
  void CloseEdgeArc(const S2Point& a, const S2Point& b);
  void BufferEdgeAndVertex(const S2Point& a, const S2Point& b,
                           const S2Point& c);
  void AddStartCap(const S2Point& a, const S2Point& b);
  void AddEndCap(const S2Point& a, const S2Point& b);
  void BufferLoop(S2PointLoopSpan loop);
  void BufferShape(const S2Shape& shape);

  Options options_;

  // The number of layers containing two-dimension geometry that have been
  // added so far.  This is used to enforce the requirement that negative
  // buffer radii allow only a single such layer.
  int num_polygon_layers_ = 0;

  // Parameters for buffering vertices and edges.
  int buffer_sign_;  // The sign of buffer_radius (-1, 0, or +1).
  S1ChordAngle abs_radius_;
  S1ChordAngle vertex_step_, edge_step_;

  // We go to extra effort to ensure that points are transformed into regular
  // polygons.  (We don't do this for arcs in general because we would rather
  // use the allowable error to reduce the complexity of the output rather
  // than increase its symmetry.)
  S1ChordAngle point_step_;

  // Contains the buffered loops that have been accumulated so far.
  S2WindingOperation op_;

  // The current offset path.  When each path is completed into a loop it is
  // added to op_ (the S2WindingOperation).
  std::vector<S2Point> path_;

  // As buffered loops are added we keep track of the winding number of a
  // fixed reference point.  This is used to derive the winding numbers of
  // every region in the spherical partition induced by the buffered loops.
  S2Point ref_point_;

  // The winding number associated with ref_point_.
  int ref_winding_;

  // The endpoints of the current sweep edge.  sweep_a_ is a vertex of the
  // original geometry and sweep_b_ is a vertex of the current offset path.
  S2Point sweep_a_, sweep_b_;

  // The starting vertices of the current input loop and offset curve.  These
  // are used to close the buffer region when a loop is completed.
  S2Point input_start_, offset_start_;
  bool have_input_start_, have_offset_start_;

  // Used internally as a temporary to avoid excessive memory allocation.
  std::vector<S2Point> tmp_vertices_;

  S2MemoryTracker::Client tracker_;
};

#endif  // S2_S2BUFFER_OPERATION_H_
