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

#ifndef S2_S2WINDING_OPERATION_H_
#define S2_S2WINDING_OPERATION_H_

#include "s2/s2builder.h"
#include "s2/s2builder_graph.h"

namespace s2builderutil { class WindingLayer; }  // Forward declaration

// Given a set of possibly self-intersecting closed loops, this class computes
// a partitioning of the sphere into regions of constant winding number and
// returns the subset of those regions selected by a given winding rule.  This
// functionality can be used to implement N-way boolean polygon operations
// including union, intersection, and symmetric difference, as well as more
// exotic operations such as buffering and Minkowski sums.
//
// Recall that in the plane, the winding number of a closed curve around a
// point is an integer representing the number of times the curve travels
// counterclockwise around that point.  For example, points in the interior of
// a planar simple polygon with counterclockwise boundary have a winding
// number of +1, while points outside the polygon have a winding number of 0.
// If the boundary is clockwise then points in the interior have a winding
// number of -1.
//
// The interior of a complex self-intersecting closed boundary curve may be
// defined by choosing an appropriate winding rule.  For example, the interior
// could be defined as all regions whose winding number is positive, or all
// regions whose winding number is non-zero, or all regions whose winding
// number is odd.  Different winding rules are useful for implementing the
// various operations mentioned above (union, symmetric difference, etc).
//
// Unfortunately the concept of winding number on the sphere is not
// well-defined, due to the fact that a given closed curve does not partition
// the sphere into regions of constant winding number.  This is because the
// winding number changes when a point crosses either the given curve or that
// curve's reflection through the origin.
//
// Instead we engage in a slight abuse of terminology by modifying the concept
// of winding number on the sphere to be a relative one.  Given a set of
// closed curves on the sphere and the winding number of a reference point R,
// we define the winding number of every other point P by counting signed edge
// crossings.  When the edge RP crosses from the right side of a curve to its
// left the winding number increases by one, and when the edge RP crosses from
// the left side of a curve to its right the winding number decreases by one.
//
// This definition agrees with the classical one in the plane when R is taken
// to be the point at infinity with a winding number of zero.  It also agrees
// with the rule used by the S2 library to define polygon interiors, namely
// that the interior of a loop is the region to its left.  And most
// importantly, it satisfies the property that a closed curve partitions the
// sphere into regions of constant winding number.

class S2WindingOperation {
 public:
  class Options {
   public:
    Options();

    // Convenience constructor that calls set_snap_function().
    explicit Options(const S2Builder::SnapFunction& snap_function);

    // Specifies the function used for snap rounding the output during the
    // call to Build().
    //
    // DEFAULT: s2builderutil::IdentitySnapFunction(S1Angle::Zero())
    const S2Builder::SnapFunction& snap_function() const;
    void set_snap_function(const S2Builder::SnapFunction& snap_function);

    // This option can be enabled to provide limited support for degeneracies
    // (i.e., sibling edge pairs and isolated vertices).  By default the output
    // does not include such edges because they do not bound any interior.  If
    // this option is true, then the output includes additional degeneracies as
    // follows:
    //
    //  - WindingRule::ODD outputs degeneracies whose multiplicity is odd.
    //
    //  - All other winding rules output degeneracies contained by regions
    //    whose winding number is zero.
    //
    // These rules are sufficient to implement the following useful operations:
    //
    //  - WindingRule::Odd can be used to compute the N-way symmetric
    //    difference of any combination of points, polylines, and polygons.
    //
    //  - WindingRule::POSITIVE can be used to compute the N-way union of any
    //    combination of points, polylines, and polygons.
    //
    // These statements only apply when closed boundaries are being used (see
    // S2BooleanOperation::{Polygon,Polyline}Model::CLOSED) and require the
    // client to convert points and polylines to degenerate loops and then back
    // again (e.g. using s2builderutil::ClosedSetNormalizer).  Note that the
    // handling of degeneracies is NOT sufficient for other polygon operations
    // (e.g. intersection) or other boundary models (e.g, semi-open).
    //
    // DEFAULT: false
    bool include_degeneracies() const;
    void set_include_degeneracies(bool include_degeneracies);

    // Specifies that internal memory usage should be tracked using the given
    // S2MemoryTracker.  If a memory limit is specified and more more memory
    // than this is required then an error will be returned.  Example usage:
    //
    //   S2MemoryTracker tracker;
    //   tracker.set_limit(500 << 20);  // 500 MB
    //   S2WindingOperation::Options options;
    //   options.set_memory_tracker(&tracker);
    //   S2WindingOperation op{options};
    //   ...
    //   S2Error error;
    //   if (!op.Build(..., &error)) {
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
    std::unique_ptr<S2Builder::SnapFunction> snap_function_;
    bool include_degeneracies_ = false;
    S2MemoryTracker* memory_tracker_ = nullptr;
  };

  // Default constructor; requires Init() to be called.
  S2WindingOperation();

  // Convenience constructor that calls Init().
  explicit S2WindingOperation(std::unique_ptr<S2Builder::Layer> result_layer,
                              const Options& options = Options{});

  // Starts an operation that sends its output to the given S2Builder layer.
  // This method may be called more than once.
  void Init(std::unique_ptr<S2Builder::Layer> result_layer,
            const Options& options = Options{});

  const Options& options() const;

  // Adds a loop to the set of loops used to partition the sphere.  The given
  // loop may be degenerate or self-intersecting.
  void AddLoop(S2PointLoopSpan loop);

  // Specifies the winding rule used to determine which regions belong to the
  // result.
  //
  // Note that additional winding rules may be created by adjusting the
  // winding number of the reference point.  For example, to select regions
  // whose winding number is at least 10, simply subtract 9 from ref_winding
  // and then use WindingRule::POSITIVE.  (This can be used to implement to
  // implement N-way polygon intersection).  Similarly, additional behaviors
  // can be obtained by reversing some of the input loops (e.g., this can be
  // used to compute one polygon minus the union of several other polygons).
  enum class WindingRule {
    POSITIVE,  // Winding number > 0
    NEGATIVE,  // Winding number < 0
    NON_ZERO,  // Winding number != 0
    ODD        // Winding number % 2 == 1
  };

  // Given a reference point "ref_p" whose winding number is given to be
  // "ref_winding", snaps all the given input loops together using the given
  // snap_function() and then partitions the sphere into regions of constant
  // winding number.  As discussed above, the winding number increases by one
  // when any loop edge is crossed from right to left, and decreases by one
  // when any loop edge is crossed from left to right.  Each partition region
  // is classified as belonging to the interior of the result if and only if
  // its winding number matches the given rule (e.g. WindingRule::POSITIVE).
  //
  // The output consists of the boundary of this interior region plus possibly
  // certain degeneraices (as controlled by the include_degeneracies() option).
  // The boundary edges are sent to the S2Builder result layer specified in the
  // constructor, along with an appropriate IsFullPolygonPredicate that can be
  // used to distinguish whether the result is empty or full (even when
  // degeneracies are present).  Note that distingishing empty from full
  // results is a problem unique to spherical geometry.
  //
  // REQUIRES: error->ok() [an existing error will not be overwritten]
  bool Build(const S2Point& ref_p, int ref_winding, WindingRule rule,
             S2Error* error);

 private:
  // Most of the implementation is in the WindingLayer class.
  friend class s2builderutil::WindingLayer;

  Options options_;
  S2Builder builder_;
  S2Builder::Graph::InputEdgeId ref_input_edge_id_;
  int ref_winding_in_;
  WindingRule rule_;
};

#endif  // S2_S2WINDING_OPERATION_H_
