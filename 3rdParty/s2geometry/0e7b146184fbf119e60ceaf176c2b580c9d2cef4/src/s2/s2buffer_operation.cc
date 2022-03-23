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
//
// The algorithm below essentially computes the offset curve of the original
// boundary, and uses this curve to divide the sphere into regions of constant
// winding number.  Since winding numbers on the sphere are relative rather
// than absolute (see s2winding_operation.h), we also need to keep track of
// the desired winding number at a fixed reference point.  The initial winding
// number for this point is the number of input shapes that contain it.  We
// then update it during the buffering process by imagining a "sweep edge"
// that extends from the current point A on the input boundary to the
// corresponding point B on the offset curve.  As we process an input loop and
// generate the corresponding offset curve, the sweep edge moves continuously
// and covers the entire buffer region (i.e., the region added to or
// subtracted from the input geometry).  We increase the winding number of the
// reference point by one whenever it crosses the sweep edge from left to
// right, and we decrease the winding number by one whenever it crosses the
// sweep edge from right to left.
//
// Concave vertices require special handling, because the corresponding offset
// curve can leave behind regions whose winding number is zero or negative.
// We handle this by splicing the concave vertex into the offset curve itself;
// this effectively terminates the current buffer region and starts a new one,
// such that the region of overlap is counted twice (i.e., its winding number
// increases by two).  The end result is the same as though we had computed
// the union of a sequence of buffered convex boundary segments.  This trick
// is described in the following paper: "Polygon Offsetting by Computing
// Winding Numbers" (Chen and McMains, Proceedings of IDETC/CIE 2005).
//
// TODO(ericv): The algorithm below is much faster than, say, computing the
// union of many buffered edges.  However further improvements are possible.
// In particular, there is an unimplemented optimization that would make it
// much faster to buffer concave boundaries when the buffer radius is large.

#include "s2/s2buffer_operation.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "s2/s2builder.h"
#include "s2/s2builder_layer.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2contains_point_query.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2edge_crossings.h"
#include "s2/s2edge_distances.h"
#include "s2/s2error.h"
#include "s2/s2lax_loop_shape.h"
#include "s2/s2predicates_internal.h"
#include "s2/s2shape_measures.h"
#include "s2/s2shapeutil_contains_brute_force.h"
#include "s2/util/math/mathutil.h"

using absl::make_unique;
using s2pred::DBL_ERR;
using std::ceil;
using std::max;
using std::min;
using std::vector;
using std::unique_ptr;

// The errors due to buffering can be categorized as follows:
//
//  1. Requested error.  This represents the error due to approximating the
//     buffered boundary as a sequence of line segments rather than a sequence
//     of circular arcs.  It is largely controlled by options.error_fraction(),
//     and can be bounded as
//
//       max(kMinRequestedError, error_fraction * buffer_radius)
//
//     where kMinRequestedError reflects the fact that S2Points do not have
//     infinite precision.  (For example, it makes no sense to attempt to
//     buffer geometry by 1e-100 radians because the spacing between
//     representable S2Points is only about 2e-16 radians in general.)
//
//  2. Relative interpolation errors.  These are numerical errors whose
//     magnitude is proportional to the buffer radius.  For such errors the
//     worst-case coefficient of proportionality turns out to be so tiny
//     compared to the smallest allowable error fraction (kMinErrorFraction)
//     that we simply ignore such errors.
//
//  3. Absolute interpolation errors.  These are numerical errors that are not
//     proportional to the buffer radius.  The main sources of such errors are
//     (1) calling S2::RobustCrossProd() to compute edge normals, and (2) calls
//     to S2::GetPointOnRay() to interpolate points along the buffered
//     boundary.  It is possible to show that this error is at most
//     kMaxAbsoluteInterpolationError as defined below.
//
// Putting these together, the final error bound looks like this:
//
//   max_error = kMaxAbsoluteInterpolationError +
//               max(kMinRequestedError,
//                   max(kMinErrorFraction, options.error_fraction()) *
//                   options.buffer_radius())

// The maximum angular spacing between representable S2Points on the unit
// sphere is roughly 2 * DBL_ERR.  We require the requested absolute error to
// be at least this large because attempting to achieve a smaller error does
// not increase the precision of the result and can increase the running time
// and space requirements considerably.
static constexpr S1Angle kMinRequestedError = S1Angle::Radians(2 * DBL_ERR);

// The maximum absolute error due to interpolating points on the buffered
// boundary.  The following constant bounds the maximum additional error
// perpendicular to the buffered boundary due to all steps of the calculation
// (S2::RobustCrossProd, the two calls to GetPointOnRay, etc).
//
// This distance represents about 10 nanometers on the Earth's surface.  Note
// that this is a conservative upper bound and that it is difficult to
// construct inputs where the error is anywhere close to this large.
static constexpr S1Angle kMaxAbsoluteInterpolationError =
    S2::kGetPointOnLineError + S2::kGetPointOnRayPerpendicularError;

// TODO(user, b/210097200): Remove when we require c++17 for opensource.
constexpr double S2BufferOperation::Options::kMinErrorFraction;
constexpr double S2BufferOperation::Options::kMaxCircleSegments;

S2BufferOperation::Options::Options()
    : snap_function_(
          make_unique<s2builderutil::IdentitySnapFunction>(S1Angle::Zero())) {
}

S2BufferOperation::Options::Options(S1Angle buffer_radius) : Options() {
  buffer_radius_ = buffer_radius;
}

S2BufferOperation::Options::Options(const Options& options)
    : buffer_radius_(options.buffer_radius_),
      error_fraction_(options.error_fraction_),
      end_cap_style_(options.end_cap_style_),
      polyline_side_(options.polyline_side_),
      snap_function_(options.snap_function_->Clone()),
      memory_tracker_(options.memory_tracker_) {
}

S2BufferOperation::Options& S2BufferOperation::Options::operator=(
    const Options& options) {
  buffer_radius_ = options.buffer_radius_;
  error_fraction_ = options.error_fraction_;
  end_cap_style_ = options.end_cap_style_;
  polyline_side_ = options.polyline_side_;
  snap_function_ = options.snap_function_->Clone();
  memory_tracker_ = options.memory_tracker_;
  return *this;
}

S1Angle S2BufferOperation::Options::buffer_radius() const {
  return buffer_radius_;
}

void S2BufferOperation::Options::set_buffer_radius(S1Angle buffer_radius) {
  buffer_radius_ = buffer_radius;
}

double S2BufferOperation::Options::error_fraction() const {
  return error_fraction_;
}

void S2BufferOperation::Options::set_error_fraction(double error_fraction) {
  S2_DCHECK_GE(error_fraction, kMinErrorFraction);
  S2_DCHECK_LE(error_fraction, 1.0);
  error_fraction_ = max(kMinErrorFraction, min(1.0, error_fraction));
}

const S1Angle S2BufferOperation::Options::max_error() const {
  // See comments for kMinRequestedError above.
  S2Builder::Options builder_options(*snap_function_);
  builder_options.set_split_crossing_edges(true);
  return max(kMinRequestedError, error_fraction_ * abs(buffer_radius_))
      + kMaxAbsoluteInterpolationError + builder_options.max_edge_deviation();
}

double S2BufferOperation::Options::circle_segments() const {
#if 0
  // This formula assumes that vertices can be placed anywhere.  TODO(ericv).
  return M_PI / acos((1 - error_fraction_) / (1 + error_fraction_));
#else
  // This formula assumes that all vertices are placed on the midline.
  return M_PI / acos(1 - error_fraction_);
#endif
}

void S2BufferOperation::Options::set_circle_segments(double circle_segments) {
  S2_DCHECK_GE(circle_segments, 2.0);
  S2_DCHECK_LE(circle_segments, kMaxCircleSegments);
  circle_segments = max(2.0, min(kMaxCircleSegments, circle_segments));

  // We convert circle_segments to error_fraction using planar geometry,
  // because the number of segments required to approximate a circle on the
  // sphere to within a given tolerance is not constant.  Unlike in the plane,
  // the total curvature of a circle on the sphere decreases as the area
  // enclosed by the circle increases; great circles have no curvature at all.
  // We round up when converting to ensure that we won't generate any tiny
  // extra edges.
  //
#if 0
  // Note that we take advantage of both positive and negative errors when
  // approximating circles (i.e., vertices are not necessarily on the midline)
  // and thus the relationships between circle_segments and error_fraction are
  //        e = (1 - cos(Pi/n)) / (1 + cos(Pi/n))
  //        n = Pi / acos((1 - e) / (1 + e))
  double r = cos(M_PI / circle_segments);
  set_error_fraction((1 - r) / (1 + r) + 1e-15);
#else
  // When all vertices are on the midline, the relationships are
  //        e = 1 - cos(Pi/n)
  //        n = Pi / acos(1 - e)
  set_error_fraction(1 - cos(M_PI / circle_segments) + 1e-15);
#endif
}

S2BufferOperation::EndCapStyle S2BufferOperation::Options::end_cap_style()
    const {
  return end_cap_style_;
}

void S2BufferOperation::Options::set_end_cap_style(EndCapStyle end_cap_style) {
  end_cap_style_ = end_cap_style;
}

S2BufferOperation::PolylineSide S2BufferOperation::Options::polyline_side()
    const {
  return polyline_side_;
}

void S2BufferOperation::Options::set_polyline_side(
    PolylineSide polyline_side) {
  polyline_side_ = polyline_side;
}

const S2Builder::SnapFunction& S2BufferOperation::Options::snap_function()
    const {
  return *snap_function_;
}

void S2BufferOperation::Options::set_snap_function(
    const S2Builder::SnapFunction& snap_function) {
  snap_function_ = snap_function.Clone();
}

S2MemoryTracker* S2BufferOperation::Options::memory_tracker() const {
  return memory_tracker_;
}

void S2BufferOperation::Options::set_memory_tracker(S2MemoryTracker* tracker) {
  memory_tracker_ = tracker;
}

S2BufferOperation::S2BufferOperation() {
}

S2BufferOperation::S2BufferOperation(unique_ptr<S2Builder::Layer> result_layer,
                                     const Options& options) {
  Init(std::move(result_layer), options);
}

void S2BufferOperation::Init(std::unique_ptr<S2Builder::Layer> result_layer,
                             const Options& options) {
  options_ = options;
  ref_point_ = S2::Origin();
  ref_winding_ = 0;
  have_input_start_ = false;
  have_offset_start_ = false;
  buffer_sign_ = sgn(options_.buffer_radius().radians());
  S1Angle abs_radius = abs(options_.buffer_radius());
  S1Angle requested_error = max(kMinRequestedError,
                                options_.error_fraction() * abs_radius);
  S1Angle max_error = kMaxAbsoluteInterpolationError + requested_error;
  if (abs_radius <= max_error) {
    // If the requested radius is smaller than the maximum error, buffering
    // could yield points on the wrong side of the original input boundary
    // (e.g., shrinking geometry slightly rather than expanding it).  Rather
    // than taking that risk, we set the buffer radius to zero when this
    // happens (which causes the original geometry to be returned).
    abs_radius_ = S1ChordAngle::Zero();
    buffer_sign_ = 0;
  } else if (abs_radius + max_error >= S1Angle::Radians(M_PI)) {
    // If the permissible range of buffer angles includes Pi then we might
    // as well take advantage of that.
    abs_radius_ = S1ChordAngle::Straight();
  } else {
    abs_radius_ = S1ChordAngle(abs_radius);
    S1Angle vertex_step = GetMaxEdgeSpan(abs_radius, requested_error);
    vertex_step_ = S1ChordAngle(vertex_step);

    // We take extra care to ensure that points are buffered as regular
    // polygons.  The step angle is adjusted up slightly to ensure that we
    // don't wind up with a tiny extra edge.
    point_step_ = S1ChordAngle::Radians(
        2 * M_PI / ceil(2 * M_PI / vertex_step.radians()) + 1e-15);

    // Edges are buffered only if the buffer radius (including permissible
    // error) is less than 90 degrees.
    S1Angle edge_radius = S1Angle::Radians(M_PI_2) - abs_radius;
    if (edge_radius > max_error) {
      edge_step_ = S1ChordAngle(GetMaxEdgeSpan(edge_radius, requested_error));
    }
  }

  // The buffered output should include degeneracies (i.e., isolated points
  // and/or sibling edge pairs) only if (1) the user specified a non-negative
  // buffer radius, and (2) the adjusted buffer radius is zero.  The only
  // purpose of keeping degeneracies is to allow points/polylines in the input
  // geometry to be converted back to points/polylines in the output if the
  // client so desires.
  S2WindingOperation::Options winding_options{options.snap_function()};
  winding_options.set_include_degeneracies(
      buffer_sign_ == 0 && options_.buffer_radius() >= S1Angle::Zero());
  winding_options.set_memory_tracker(options.memory_tracker());
  op_.Init(std::move(result_layer), winding_options);
  tracker_.Init(options.memory_tracker());
}

const S2BufferOperation::Options& S2BufferOperation::options() const {
  return options_;
}

S1Angle S2BufferOperation::GetMaxEdgeSpan(S1Angle radius,
                                          S1Angle requested_error) const {
  // If the allowable radius range spans Pi/2 then we can use edges as long as
  // we like, however we always use at least 3 edges to approximate a circle.
  S1Angle step = S1Angle::Radians(2 * M_PI / 3 + 1e-15);
  S1Angle min_radius = radius - requested_error;
  S2_DCHECK_GE(min_radius, S1Angle::Zero());
  if (radius.radians() < M_PI_2) {
    step = min(step, S1Angle::Radians(2 * acos(tan(min_radius) / tan(radius))));
  } else if (min_radius.radians() > M_PI_2) {
    step = min(step, S1Angle::Radians(2 * acos(tan(radius) / tan(min_radius))));
  }
  return step;
}

// The sweep edge AB (see introduction) consists of one point on the input
// boundary (A) and one point on the offset curve (B).  This function advances
// the sweep edge by moving its first vertex A to "new_a" and updating the
// winding number of the reference point if necessary.
void S2BufferOperation::SetInputVertex(const S2Point& new_a) {
  if (have_input_start_) {
    S2_DCHECK(have_offset_start_);
    UpdateRefWinding(sweep_a_, sweep_b_, new_a);
  } else {
    input_start_ = new_a;
    have_input_start_ = true;
  }
  sweep_a_ = new_a;
}

// Adds the point "new_b" to the offset path.  Also advances the sweep edge AB
// by moving its second vertex B to "new_b" and updating the winding number of
// the reference point if necessary (see introduction).
void S2BufferOperation::AddOffsetVertex(const S2Point& new_b) {
  if (!tracker_.AddSpace(&path_, 1)) return;
  path_.push_back(new_b);
  if (have_offset_start_) {
    S2_DCHECK(have_input_start_);
    UpdateRefWinding(sweep_a_, sweep_b_, new_b);
  } else {
    offset_start_ = new_b;
    have_offset_start_ = true;
  }
  sweep_b_ = new_b;
}

// Finishes buffering the current loop by advancing the sweep edge back to its
// starting location, updating the winding number of the reference point if
// necessary.
void S2BufferOperation::CloseBufferRegion() {
  if (have_offset_start_ && have_input_start_) {
    UpdateRefWinding(sweep_a_, sweep_b_, input_start_);
    UpdateRefWinding(input_start_, sweep_b_, offset_start_);
  }
}

// Outputs the current buffered path (which is assumed to be a loop), and
// resets the state to prepare for buffering a new loop.
void S2BufferOperation::OutputPath() {
  op_.AddLoop(path_);
  path_.clear();  // Does not change capacity.
  have_input_start_ = false;
  have_offset_start_ = false;
}

// Given a triangle ABC that has just been covered by the sweep edge AB,
// updates the winding number of the reference point if necessary.
void S2BufferOperation::UpdateRefWinding(
    const S2Point& a, const S2Point& b, const S2Point& c) {
  // TODO(ericv): This code could be made much faster by maintaining a
  // bounding plane that separates the current sweep edge from the reference
  // point.  Whenever the sweep_a_ or sweep_b_ is updated we would just need
  // to check that the new vertex is still on the opposite side of the
  // bounding plane (i.e., one dot product).  If not, we test the current
  // triangle using the code below and then compute a new bounding plane.
  //
  // Another optimization would be to choose the reference point to be 90
  // degrees away from the first input vertex, since then triangle tests would
  // not be needed unless the input geometry spans more than 90 degrees.  This
  // would involve adding a new flag have_ref_point_ rather than always
  // choosing the reference point to be S2::Origin().
  //
  // According to profiling these optimizations are not currently worthwhile,
  // but this is worth revisiting if and when other improvements are made.
  int sign = s2pred::Sign(a, b, c);
  if (sign == 0) return;
  bool inside = S2::AngleContainsVertex(a, b, c) == (sign > 0);
  S2EdgeCrosser crosser(&b, &ref_point_);
  inside ^= crosser.EdgeOrVertexCrossing(&a, &b);
  inside ^= crosser.EdgeOrVertexCrossing(&b, &c);
  inside ^= crosser.EdgeOrVertexCrossing(&c, &a);
  if (inside) ref_winding_ += sign;
}

// Ensures that the output will be the full polygon.
void S2BufferOperation::AddFullPolygon() {
  ref_winding_ += 1;
}

void S2BufferOperation::AddPoint(const S2Point& point) {
  // If buffer_radius < 0, points are discarded.
  if (buffer_sign_ < 0) return;

  // Buffering by 180 degrees or more always yields the full polygon.
  // (We don't need to worry about buffering by 180 degrees yielding
  // a degenerate hole because error_fraction_ is always positive.
  if (abs_radius_ >= S1ChordAngle::Straight()) {
    return AddFullPolygon();
  }

  // If buffer_radius == 0, points are converted into degenerate loops.
  if (buffer_sign_ == 0) {
    if (!tracker_.AddSpace(&path_, 1)) return;
    path_.push_back(point);
  } else {
    // Since S1ChordAngle can only represent angles between 0 and 180 degrees,
    // we generate the circle in four 90 degree increments.
    SetInputVertex(point);
    S2Point start = S2::Ortho(point);
    S1ChordAngle angle = S1ChordAngle::Zero();
    for (int quadrant = 0; quadrant < 4; ++quadrant) {
      // Generate 90 degrees of the circular arc.  Normalize "rotate_dir" at
      // each iteration to avoid magnifying normalization errors in "point".
      S2Point rotate_dir = point.CrossProd(start).Normalize();
      for (; angle < S1ChordAngle::Right(); angle += point_step_) {
        S2Point dir = S2::GetPointOnRay(start, rotate_dir, angle);
        AddOffsetVertex(S2::GetPointOnRay(point, dir, abs_radius_));
      }
      angle -= S1ChordAngle::Right();
      start = rotate_dir;
    }
    CloseBufferRegion();
  }
  OutputPath();
}

// Returns the edge normal for the given edge AB.  The sign is chosen such
// that the normal is on the right of AB if buffer_sign_ > 0, and on the left
// of AB if buffer_sign_ < 0.
inline S2Point S2BufferOperation::GetEdgeAxis(const S2Point& a,
                                              const S2Point& b) const {
  S2_DCHECK_NE(buffer_sign_, 0);
  return buffer_sign_ * S2::RobustCrossProd(b, a).Normalize();
}

// Adds a semi-open offset arc around vertex V.  The arc proceeds CCW from
// "start" to "end" (both of which must be perpendicular to V).
void S2BufferOperation::AddVertexArc(const S2Point& v, const S2Point& start,
                                     const S2Point& end) {
  // Make sure that we output at least one point even when span == 0.
  S2Point rotate_dir = buffer_sign_ * v.CrossProd(start).Normalize();
  S1ChordAngle angle, span(start, end);
  do {
    S2Point dir = S2::GetPointOnRay(start, rotate_dir, angle);
    AddOffsetVertex(S2::GetPointOnRay(v, dir, abs_radius_));
  } while ((angle += vertex_step_) < span);
}

// Closes the semi-open arc generated by AddVertexArc().
void S2BufferOperation::CloseVertexArc(const S2Point& v, const S2Point& end) {
  AddOffsetVertex(S2::GetPointOnRay(v, end, abs_radius_));
}

// Adds a semi-open offset arc for the given edge AB.
void S2BufferOperation::AddEdgeArc(const S2Point& a, const S2Point& b) {
  S2Point ab_axis = GetEdgeAxis(a, b);
  if (edge_step_ == S1ChordAngle::Zero()) {
    // If the buffer radius is more than 90 degrees, edges do not contribute to
    // the buffered boundary.  Instead we force the offset path to pass
    // through a vertex located at the edge normal.  This is similar to the
    // case of concave vertices (below) where it is necessary to route the
    // offset path through the concave vertex to ensure that the winding
    // numbers in all output regions have the correct sign.
    AddOffsetVertex(ab_axis);
  } else {
    // Make sure that we output at least one point even when span == 0.
    S2Point rotate_dir = buffer_sign_ * a.CrossProd(ab_axis).Normalize();
    S1ChordAngle angle, span(a, b);
    do {
      S2Point p = S2::GetPointOnRay(a, rotate_dir, angle);
      AddOffsetVertex(S2::GetPointOnRay(p, ab_axis, abs_radius_));
    } while ((angle += edge_step_) < span);
  }
  SetInputVertex(b);
}

// Closes the semi-open arc generated by AddEdgeArc().
void S2BufferOperation::CloseEdgeArc(const S2Point& a, const S2Point& b) {
  if (edge_step_ != S1ChordAngle::Zero()) {
    AddOffsetVertex(S2::GetPointOnRay(b, GetEdgeAxis(a, b), abs_radius_));
  }
}

// Buffers the edge AB and the vertex B.  (The vertex C is used to determine
// the range of angles that should be buffered at B.)
//
// TODO(ericv): Let A* denote the possible offset points of A with respect to
// the edge AB for buffer radii in the range specified by "radius" and
// "error_fraction".  Rather than requiring that the path so far terminates at
// a point in A*, as you might expect, instead we only require that the path
// terminates at a point X such that for any point Y in A*, the edge XY does
// not leave the valid buffer zone of the previous edge and vertex.
void S2BufferOperation::BufferEdgeAndVertex(const S2Point& a, const S2Point& b,
                                            const S2Point& c) {
  S2_DCHECK_NE(a, b);
  S2_DCHECK_NE(b, c);
  S2_DCHECK_NE(buffer_sign_, 0);
  if (!tracker_.ok()) return;

  // For left (convex) turns we need to add an offset arc.  For right
  // (concave) turns we connect the end of the current offset path to the
  // vertex itself and then to the start of the offset path for the next edge.
  // Note that A == C is considered to represent a convex (left) turn.
  AddEdgeArc(a, b);
  if (buffer_sign_ * s2pred::Sign(a, b, c) >= 0) {
    // The boundary makes a convex turn.  If there is no following edge arc
    // then we need to generate a closed vertex arc.
    S2Point start = GetEdgeAxis(a, b);
    S2Point end = GetEdgeAxis(b, c);
    AddVertexArc(b, start, end);
    if (edge_step_ == S1ChordAngle::Zero()) CloseVertexArc(b, end);
  } else {
    // The boundary makes a concave turn.  It is tempting to simply connect
    // the end of the current offset path to the start of the offset path for
    // the next edge, however this can create output regions where the winding
    // number is incorrect.  A solution that always works is to terminate the
    // current offset path and start a new one by connecting the two offset
    // paths through the input vertex whenever it is concave.  We first need
    // to close the previous semi-open edge arc if necessary.
    CloseEdgeArc(a, b);
    AddOffsetVertex(b);  // Connect through the input vertex.
  }
}

// Given a polyline that starts with the edge AB, adds an end cap (as
// specified by end_cap_style() and polyline_side()) for the vertex A.
void S2BufferOperation::AddStartCap(const S2Point& a, const S2Point& b) {
  S2Point axis = GetEdgeAxis(a, b);
  if (options_.end_cap_style() == EndCapStyle::FLAT) {
    // One-sided flat end caps require no additional vertices since the
    // "offset curve" for the opposite side is simply the reversed polyline.
    if (options_.polyline_side() == PolylineSide::BOTH) {
      AddOffsetVertex(S2::GetPointOnRay(a, -axis, abs_radius_));
    }
  } else {
    S2_DCHECK(options_.end_cap_style() == EndCapStyle::ROUND);
    if (options_.polyline_side() == PolylineSide::BOTH) {
      // The end cap consists of a semicircle.
      AddVertexArc(a, -axis, axis);
    } else {
      // The end cap consists of a quarter circle.  Note that for
      // PolylineSide::LEFT, the polyline direction has been reversed.
      AddVertexArc(a, axis.CrossProd(a).Normalize(), axis);
    }
  }
}

// Given a polyline that ends with the edge AB, adds an end cap (as specified
// by end_cap_style() and polyline_side()) for the vertex B.
void S2BufferOperation::AddEndCap(const S2Point& a, const S2Point& b) {
  S2Point axis = GetEdgeAxis(a, b);
  if (options_.end_cap_style() == EndCapStyle::FLAT) {
    CloseEdgeArc(a, b);  // Close the previous semi-open edge arc if necessary.
  } else {
    S2_DCHECK(options_.end_cap_style() == EndCapStyle::ROUND);
    if (options_.polyline_side() == PolylineSide::BOTH) {
      // The end cap consists of a semicircle.
      AddVertexArc(b, axis, -axis);
    } else {
      // The end cap consists of a quarter circle.  We close the arc since it
      // will be followed by the reversed polyline vertices.  Note that for
      // PolylineSide::LEFT, the polyline direction has been reversed.
      S2Point end = b.CrossProd(axis).Normalize();
      AddVertexArc(b, axis, end);
      CloseVertexArc(b, end);
    }
  }
}

// Helper function that buffers the given loop.
void S2BufferOperation::BufferLoop(S2PointLoopSpan loop) {
  // Empty loops always yield an empty path.
  if (loop.empty() || !tracker_.ok()) return;

  // Loops with one degenerate edge are treated as points.
  if (loop.size() == 1) return AddPoint(loop[0]);

  // Buffering by 180 degrees or more always yields the full polygon.
  // Buffering by -180 degrees or more always yields the empty polygon.
  if (abs_radius_ >= S1ChordAngle::Straight()) {
    if (buffer_sign_ > 0) AddFullPolygon();
    return;
  }

  // If buffer_radius == 0, the loop is passed through unchanged.
  if (buffer_sign_ == 0) {
    if (!tracker_.AddSpace(&path_, loop.size())) return;
    path_.assign(loop.begin(), loop.end());
  } else {
    SetInputVertex(loop[0]);
    for (int i = 0; i < loop.size(); ++i) {
      BufferEdgeAndVertex(loop[i], loop[i + 1], loop[i + 2]);
    }
    CloseBufferRegion();
  }
  OutputPath();
}

void S2BufferOperation::AddPolyline(S2PointSpan polyline) {
  // Left-sided buffering is supported by reversing the polyline and then
  // buffering on the right.
  vector<S2Point> reversed;
  if (options_.polyline_side() == PolylineSide::LEFT) {
    reversed.reserve(polyline.size());
    std::reverse_copy(polyline.begin(), polyline.end(),
                      std::back_inserter(reversed));
    polyline = reversed;
  }

  // If buffer_radius < 0, polylines are discarded.
  if (buffer_sign_ < 0 || !tracker_.ok()) return;

  // Polylines with 0 or 1 vertices are defined to have no edges.
  int n = polyline.size();
  if (n <= 1) return;

  // Polylines with one degenerate edge are treated as points.
  if (n == 2 && polyline[0] == polyline[1]) {
    return AddPoint(polyline[0]);
  }

  // Buffering by 180 degrees or more always yields the full polygon.
  if (abs_radius_ >= S1ChordAngle::Straight()) {
    return AddFullPolygon();
  }

  // If buffer_radius == 0, polylines are converted into degenerate loops.
  if (buffer_sign_ == 0) {
    if (!tracker_.AddSpace(&path_, 2 * (n - 1))) return;
    path_.assign(polyline.begin(), polyline.end() - 1);
    path_.insert(path_.end(), polyline.rbegin(), polyline.rend() - 1);
  } else {
    // Otherwise we buffer each side of the polyline separately.
    SetInputVertex(polyline[0]);
    AddStartCap(polyline[0], polyline[1]);
    for (int i = 0; i < n - 2; ++i) {
      BufferEdgeAndVertex(polyline[i], polyline[i + 1], polyline[i + 2]);
    }
    AddEdgeArc(polyline[n - 2], polyline[n - 1]);
    AddEndCap(polyline[n - 2], polyline[n - 1]);

    if (options_.polyline_side() == PolylineSide::BOTH) {
      for (int i = n - 3; i >= 0; --i) {
        BufferEdgeAndVertex(polyline[i + 2], polyline[i + 1], polyline[i]);
      }
      AddEdgeArc(polyline[1], polyline[0]);
      CloseBufferRegion();
    } else {
      // The other side of the polyline is not buffered.  Note that for
      // PolylineSide::LEFT, the polyline direction has been reversed.
      if (!tracker_.AddSpace(&path_, n)) return;
      path_.insert(path_.end(), polyline.rbegin(), polyline.rend());
      // Don't call CloseBufferRegion() since the path has already been closed.
    }
  }
  OutputPath();
}

void S2BufferOperation::AddLoop(S2PointLoopSpan loop) {
  if (loop.empty()) return;
  BufferLoop(loop);

  // The vertex copying below could be avoided by adding a version of
  // S2LaxLoopShape that doesn't own its vertices.
  if (!tracker_.ok()) return;
  ref_winding_ += s2shapeutil::ContainsBruteForce(S2LaxLoopShape(loop),
                                                  ref_point_);
  num_polygon_layers_ += 1;
}

void S2BufferOperation::BufferShape(const S2Shape& shape) {
  int dimension = shape.dimension();
  int num_chains = shape.num_chains();
  for (int c = 0; c < num_chains; ++c) {
    S2Shape::Chain chain = shape.chain(c);
    if (chain.length == 0) continue;
    if (dimension == 0) {
      AddPoint(shape.edge(c).v0);
    } else {
      S2::GetChainVertices(shape, c, &tmp_vertices_);
      if (dimension == 1) {
        AddPolyline(S2PointSpan(tmp_vertices_));
      } else {
        BufferLoop(S2PointLoopSpan(tmp_vertices_));
      }
    }
  }
}

void S2BufferOperation::AddShape(const S2Shape& shape) {
  BufferShape(shape);
  ref_winding_ += s2shapeutil::ContainsBruteForce(shape, ref_point_);
  num_polygon_layers_ += (shape.dimension() == 2);
}

void S2BufferOperation::AddShapeIndex(const S2ShapeIndex& index) {
  int max_dimension = -1;
  for (const S2Shape* shape : index) {
    if (shape == nullptr) continue;
    max_dimension = max(max_dimension, shape->dimension());
    BufferShape(*shape);
  }
  ref_winding_ += MakeS2ContainsPointQuery(&index).Contains(ref_point_);
  num_polygon_layers_ += (max_dimension == 2);
}

bool S2BufferOperation::Build(S2Error* error) {
  if (buffer_sign_ < 0 && num_polygon_layers_ > 1) {
    error->Init(S2Error::FAILED_PRECONDITION,
                "Negative buffer radius requires at most one polygon layer");
    return false;
  }
  return op_.Build(ref_point_, ref_winding_,
                   S2WindingOperation::WindingRule::POSITIVE, error);
}
