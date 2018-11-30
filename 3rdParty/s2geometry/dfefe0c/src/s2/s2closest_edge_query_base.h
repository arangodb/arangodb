// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef S2_S2CLOSEST_EDGE_QUERY_BASE_H_
#define S2_S2CLOSEST_EDGE_QUERY_BASE_H_

#include <memory>
#include <vector>

#include "s2/base/logging.h"
#include "s2/third_party/absl/container/inlined_vector.h"
#include "s2/_fp_contract_off.h"
#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"
#include "s2/s2distance_target.h"
#include "s2/s2region_coverer.h"
#include "s2/s2shape_index.h"
#include "s2/s2shapeutil_count_edges.h"
#include "s2/s2shapeutil_shape_edge_id.h"
#include "s2/util/gtl/btree_set.h"
#include "s2/util/gtl/dense_hash_set.h"

// S2ClosestEdgeQueryBase is a templatized class for finding the closest
// edge(s) between two geometries.  It is not intended to be used directly,
// but rather to serve as the implementation of various specialized classes
// with more convenient APIs (such as S2ClosestEdgeQuery).  It is flexible
// enough so that it can be adapted to compute maximum distances and even
// potentially Hausdorff distances.
//
// By using the appropriate options, this class can answer questions such as:
//
//  - Find the minimum distance between two geometries A and B.
//  - Find all edges of geometry A that are within a distance D of geometry B.
//  - Find the k edges of geometry A that are closest to a given point P.
//
// You can also specify whether polygons should include their interiors (i.e.,
// if a point is contained by a polygon, should the distance be zero or should
// it be measured to the polygon boundary?)
//
// The input geometries may consist of any number of points, polylines, and
// polygons (collectively referred to as "shapes").  Shapes do not need to be
// disjoint; they may overlap or intersect arbitrarily.  The implementation is
// designed to be fast for both simple and complex geometries.
//
// The Distance template argument is used to represent distances.  Usually it
// is a thin wrapper around S1ChordAngle, but another distance type may be
// used as long as it implements the Distance concept described in
// s2distance_targets.h.  For example this can be used to measure maximum
// distances, to get more accuracy, or to measure non-spheroidal distances.
template <class Distance>
class S2ClosestEdgeQueryBase {
 public:
  using Delta = typename Distance::Delta;

  // Options that control the set of edges returned.  Note that by default
  // *all* edges are returned, so you will always want to set either the
  // max_results() option or the max_distance() option (or both).
  class Options {
   public:
    Options();

    // Specifies that at most "max_results" edges should be returned.
    //
    // REQUIRES: max_results >= 1
    // DEFAULT: kMaxMaxResults
    int max_results() const;
    void set_max_results(int max_results);
    static constexpr int kMaxMaxResults = std::numeric_limits<int>::max();

    // Specifies that only edges whose distance to the target is less than
    // "max_distance" should be returned.
    //
    // Note that edges whose distance is exactly equal to "max_distance" are
    // not returned.  In most cases this doesn't matter (since distances are
    // not computed exactly in the first place), but if such edges are needed
    // then you can retrieve them by specifying "max_distance" as the next
    // largest representable Distance.  For example, if Distance is an
    // S1ChordAngle then you can specify max_distance.Successor().
    //
    // DEFAULT: Distance::Infinity()
    Distance max_distance() const;
    void set_max_distance(Distance max_distance);

    // Specifies that edges up to max_error() further away than the true
    // closest edges may be substituted in the result set, as long as such
    // edges satisfy all the remaining search criteria (such as max_distance).
    // This option only has an effect if max_results() is also specified;
    // otherwise all edges closer than max_distance() will always be returned.
    //
    // Note that this does not affect how the distance between edges is
    // computed; it simply gives the algorithm permission to stop the search
    // early as soon as the best possible improvement drops below max_error().
    //
    // This can be used to implement distance predicates efficiently.  For
    // example, to determine whether the minimum distance is less than D, set
    // max_results() == 1 and max_distance() == max_error() == D.  This causes
    // the algorithm to terminate as soon as it finds any edge whose distance
    // is less than D, rather than continuing to search for an edge that is
    // even closer.
    //
    // DEFAULT: Distance::Delta::Zero()
    Delta max_error() const;
    void set_max_error(Delta max_error);

    // Specifies that polygon interiors should be included when measuring
    // distances.  In other words, polygons that contain the target should
    // have a distance of zero.  (For targets consisting of multiple connected
    // components, the distance is zero if any component is contained.)  This
    // is indicated in the results by returning a (shape_id, edge_id) pair
    // with edge_id == -1, i.e. this value denotes the polygons's interior.
    //
    // Note that for efficiency, any polygon that intersects the target may or
    // may not have an (edge_id == -1) result.  Such results are optional
    // because in that case the distance to the polygon is already zero.
    //
    // DEFAULT: true
    bool include_interiors() const;
    void set_include_interiors(bool include_interiors);

    // Specifies that distances should be computed by examining every edge
    // rather than using the S2ShapeIndex.  This is useful for testing,
    // benchmarking, and debugging.
    //
    // DEFAULT: false
    bool use_brute_force() const;
    void set_use_brute_force(bool use_brute_force);

   private:
    Distance max_distance_ = Distance::Infinity();
    Delta max_error_ = Delta::Zero();
    int max_results_ = kMaxMaxResults;
    bool include_interiors_ = true;
    bool use_brute_force_ = false;
  };

  // The Target class represents the geometry to which the distance is
  // measured.  For example, there can be subtypes for measuring the distance
  // to a point, an edge, or to an S2ShapeIndex (an arbitrary collection of
  // geometry).
  //
  // Implementations do *not* need to be thread-safe.  They may cache data or
  // allocate temporary data structures in order to improve performance.
  using Target = S2DistanceTarget<Distance>;

  // Each "Result" object represents a closest edge.  Note the following
  // special cases:
  //
  //  - (shape_id() >= 0) && (edge_id() < 0) represents the interior of a shape.
  //    Such results may be returned when options.include_interiors() is true.
  //    Such results can be identified using the is_interior() method.
  //
  //  - (shape_id() < 0) && (edge_id() < 0) is returned by `FindClosestEdge`
  //    to indicate that no edge satisfies the given query options.  Such
  //    results can be identified using is_empty() method.
  class Result {
   public:
    // The default constructor yields an empty result, with a distance() of
    // Infinity() and shape_id == edge_id == -1.
    Result() : distance_(Distance::Infinity()), shape_id_(-1), edge_id_(-1) {}

    // Constructs a Result object for the given arguments.
    Result(Distance distance, int32 shape_id, int32 edge_id)
        : distance_(distance), shape_id_(shape_id), edge_id_(edge_id) {}

    // The distance from the target to this edge.
    Distance distance() const { return distance_; }

    // Identifies an indexed shape.
    int32 shape_id() const { return shape_id_; }

    // Identifies an edge within the shape.
    int32 edge_id() const { return edge_id_; }

    // Returns true if this Result object represents the interior of a shape.
    // (Such results may be returned when options.include_interiors() is true.)
    bool is_interior() const { return shape_id_ >= 0 && edge_id_ < 0; }

    // Returns true if this Result object indicates that no edge satisfies the
    // given query options.  (This result is only returned in one special
    // case, namely when FindClosestEdge() does not find any suitable edges.
    // It is never returned by methods that return a vector of results.)
    bool is_empty() const { return shape_id_ < 0; }

    // Returns true if two Result objects are identical.
    friend bool operator==(const Result& x, const Result& y) {
      return (x.distance_ == y.distance_ &&
              x.shape_id_ == y.shape_id_ &&
              x.edge_id_ == y.edge_id_);
    }

    // Compares edges first by distance, then by (shape_id, edge_id).
    friend bool operator<(const Result& x, const Result& y) {
      if (x.distance_ < y.distance_) return true;
      if (y.distance_ < x.distance_) return false;
      if (x.shape_id_ < y.shape_id_) return true;
      if (y.shape_id_ < x.shape_id_) return false;
      return x.edge_id_ < y.edge_id_;
    }

    // Indicates that linear rather than binary search should be used when this
    // type is used as the key in gtl::btree data structures.
    using goog_btree_prefer_linear_node_search = std::true_type;

   private:
    Distance distance_;  // The distance from the target to this edge.
    int32 shape_id_;     // Identifies an indexed shape.
    int32 edge_id_;      // Identifies an edge within the shape.
  };

  // Default constructor; requires Init() to be called.
  S2ClosestEdgeQueryBase();
  ~S2ClosestEdgeQueryBase();

  // Convenience constructor that calls Init().
  explicit S2ClosestEdgeQueryBase(const S2ShapeIndex* index);

  // S2ClosestEdgeQueryBase is not copyable.
  S2ClosestEdgeQueryBase(const S2ClosestEdgeQueryBase&) = delete;
  void operator=(const S2ClosestEdgeQueryBase&) = delete;

  // Initializes the query.
  // REQUIRES: ReInit() must be called if "index" is modified.
  void Init(const S2ShapeIndex* index);

  // Reinitializes the query.  This method must be called whenever the
  // underlying index is modified.
  void ReInit();

  // Returns a reference to the underlying S2ShapeIndex.
  const S2ShapeIndex& index() const;

  // Returns the closest edges to the given target that satisfy the given
  // options.  This method may be called multiple times.
  //
  // Note that if options().include_interiors() is true, the result vector may
  // include some entries with edge_id == -1.  This indicates that the target
  // intersects the indexed polygon with the given shape_id.
  std::vector<Result> FindClosestEdges(Target* target, const Options& options);

  // This version can be more efficient when this method is called many times,
  // since it does not require allocating a new vector on each call.
  void FindClosestEdges(Target* target, const Options& options,
                        std::vector<Result>* results);

  // Convenience method that returns exactly one edge.  If no edges satisfy
  // the given search criteria, then a Result with distance == Infinity() and
  // shape_id == edge_id == -1 is returned.
  //
  // Note that if options.include_interiors() is true, edge_id == -1 is also
  // used to indicate that the target intersects an indexed polygon (but in
  // that case distance == Zero() and shape_id >= 0).
  //
  // REQUIRES: options.max_results() == 1
  Result FindClosestEdge(Target* target, const Options& options);

 private:
  class QueueEntry;

  const Options& options() const { return *options_; }
  void FindClosestEdgesInternal(Target* target, const Options& options);
  void FindClosestEdgesBruteForce();
  void FindClosestEdgesOptimized();
  void InitQueue();
  void InitCovering();
  void AddInitialRange(const S2ShapeIndex::Iterator& first,
                       const S2ShapeIndex::Iterator& last);
  void MaybeAddResult(const S2Shape& shape, int edge_id);
  void AddResult(const Result& result);
  void ProcessEdges(const QueueEntry& entry);
  void ProcessOrEnqueue(S2CellId id);
  void ProcessOrEnqueue(S2CellId id, const S2ShapeIndexCell* index_cell);

  const S2ShapeIndex* index_;
  const Options* options_;
  Target* target_;

  // True if max_error() must be subtracted from priority queue cell distances
  // in order to ensure that such distances are measured conservatively.  This
  // is true only if the target takes advantage of max_error() in order to
  // return faster results, and 0 < max_error() < distance_limit_.
  bool use_conservative_cell_distance_;

  // For the optimized algorihm we precompute the top-level S2CellIds that
  // will be added to the priority queue.  There can be at most 6 of these
  // cells.  Essentially this is just a covering of the indexed edges, except
  // that we also store pointers to the corresponding S2ShapeIndexCells to
  // reduce the number of index seeks required.
  //
  // The covering needs to be stored in a std::vector so that we can use
  // S2CellUnion::GetIntersection().
  std::vector<S2CellId> index_covering_;
  absl::InlinedVector<const S2ShapeIndexCell*, 6> index_cells_;

  // The decision about whether to use the brute force algorithm is based on
  // counting the total number of edges in the index.  However if the index
  // contains a large number of shapes, this in itself might take too long.
  // So instead we only count edges up to (max_brute_force_index_size() + 1)
  // for the current target type (stored as index_num_edges_limit_).
  int index_num_edges_;
  int index_num_edges_limit_;

  // The distance beyond which we can safely ignore further candidate edges.
  // (Candidates that are exactly at the limit are ignored; this is more
  // efficient for UpdateMinDistance() and should not affect clients since
  // distance measurements have a small amount of error anyway.)
  //
  // Initially this is the same as the maximum distance specified by the user,
  // but it can also be updated by the algorithm (see MaybeAddResult).
  Distance distance_limit_;

  // The current result set is stored in one of three ways:
  //
  //  - If max_results() == 1, the best result is kept in result_singleton_.
  //
  //  - If max_results() == "infinity", results are appended to result_vector_
  //    and sorted/uniqued at the end.
  //
  //  - Otherwise results are kept in a btree_set so that we can progressively
  //    reduce the distance limit once max_results() results have been found.
  //    (A priority queue is not sufficient because we need to be able to
  //    check whether a candidate edge is already in the result set.)
  //
  // TODO(ericv): Check whether it would be faster to use avoid_duplicates_
  // when result_set_ is used so that we could use a priority queue instead.
  Result result_singleton_;
  std::vector<Result> result_vector_;
  gtl::btree_set<Result> result_set_;

  // When the result edges are stored in a btree_set (see above), usually
  // duplicates can be removed simply by inserting candidate edges in the
  // current set.  However this is not true if Options::max_error() > 0 and
  // the Target subtype takes advantage of this by returning suboptimal
  // distances.  This is because when UpdateMinDistance() is called with
  // different "min_dist" parameters (i.e., the distance to beat), the
  // implementation may return a different distance for the same edge.  Since
  // the btree_set is keyed by (distance, shape_id, edge_id) this can create
  // duplicate edges in the results.
  //
  // The flag below is true when duplicates must be avoided explicitly.  This
  // is achieved by maintaining a separate set keyed by (shape_id, edge_id)
  // only, and checking whether each edge is in that set before computing the
  // distance to it.
  //
  // TODO(ericv): Check whether it is faster to avoid duplicates by default
  // (even when Options::max_results() == 1), rather than just when we need to.
  bool avoid_duplicates_;
  using ShapeEdgeId = s2shapeutil::ShapeEdgeId;
  gtl::dense_hash_set<ShapeEdgeId, s2shapeutil::ShapeEdgeIdHash> tested_edges_;

  // The algorithm maintains a priority queue of unprocessed S2CellIds, sorted
  // in increasing order of distance from the target.
  struct QueueEntry {
    // A lower bound on the distance from the target to "id".  This is the key
    // of the priority queue.
    Distance distance;

    // The cell being queued.
    S2CellId id;

    // If "id" belongs to the index, this field stores the corresponding
    // S2ShapeIndexCell.  Otherwise "id" is a proper ancestor of one or more
    // S2ShapeIndexCells and this field stores nullptr.  The purpose of this
    // field is to avoid an extra Seek() when the queue entry is processed.
    const S2ShapeIndexCell* index_cell;

    QueueEntry(Distance _distance, S2CellId _id,
               const S2ShapeIndexCell* _index_cell)
        : distance(_distance), id(_id), index_cell(_index_cell) {
    }
    bool operator<(const QueueEntry& other) const {
      // The priority queue returns the largest elements first, so we want the
      // "largest" entry to have the smallest distance.
      return other.distance < distance;
    }
  };
  using CellQueue =
      std::priority_queue<QueueEntry, absl::InlinedVector<QueueEntry, 16>>;
  CellQueue queue_;

  // Temporaries, defined here to avoid multiple allocations / initializations.

  S2ShapeIndex::Iterator iter_;
  std::vector<S2CellId> max_distance_covering_;
  std::vector<S2CellId> initial_cells_;
};


//////////////////   Implementation details follow   ////////////////////


template <class Distance>
inline S2ClosestEdgeQueryBase<Distance>::Options::Options() {
}

template <class Distance>
inline int S2ClosestEdgeQueryBase<Distance>::Options::max_results() const {
  return max_results_;
}

template <class Distance>
inline void S2ClosestEdgeQueryBase<Distance>::Options::set_max_results(
    int max_results) {
  S2_DCHECK_GE(max_results, 1);
  max_results_ = max_results;
}

template <class Distance>
inline Distance S2ClosestEdgeQueryBase<Distance>::Options::max_distance()
    const {
  return max_distance_;
}

template <class Distance>
inline void S2ClosestEdgeQueryBase<Distance>::Options::set_max_distance(
    Distance max_distance) {
  max_distance_ = max_distance;
}

template <class Distance>
inline typename Distance::Delta
S2ClosestEdgeQueryBase<Distance>::Options::max_error() const {
  return max_error_;
}

template <class Distance>
inline void S2ClosestEdgeQueryBase<Distance>::Options::set_max_error(
    Delta max_error) {
  max_error_ = max_error;
}

template <class Distance>
inline bool S2ClosestEdgeQueryBase<Distance>::Options::include_interiors()
    const {
  return include_interiors_;
}

template <class Distance>
inline void S2ClosestEdgeQueryBase<Distance>::Options::set_include_interiors(
    bool include_interiors) {
  include_interiors_ = include_interiors;
}

template <class Distance>
inline bool S2ClosestEdgeQueryBase<Distance>::Options::use_brute_force() const {
  return use_brute_force_;
}

template <class Distance>
inline void S2ClosestEdgeQueryBase<Distance>::Options::set_use_brute_force(
    bool use_brute_force) {
  use_brute_force_ = use_brute_force;
}

template <class Distance>
S2ClosestEdgeQueryBase<Distance>::S2ClosestEdgeQueryBase()
    : tested_edges_(1) /* expected_max_elements*/ {
  tested_edges_.set_empty_key(ShapeEdgeId(-1, -1));
}

template <class Distance>
S2ClosestEdgeQueryBase<Distance>::~S2ClosestEdgeQueryBase() {
  // Prevent inline destructor bloat by providing a definition.
}

template <class Distance>
inline S2ClosestEdgeQueryBase<Distance>::S2ClosestEdgeQueryBase(
    const S2ShapeIndex* index) : S2ClosestEdgeQueryBase() {
  Init(index);
}

template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::Init(const S2ShapeIndex* index) {
  index_ = index;
  ReInit();
}

template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::ReInit() {
  index_num_edges_ = 0;
  index_num_edges_limit_ = 0;
  index_covering_.clear();
  index_cells_.clear();
  // We don't initialize iter_ here to make queries on small indexes a bit
  // faster (i.e., where brute force is used).
}

template <class Distance>
inline const S2ShapeIndex& S2ClosestEdgeQueryBase<Distance>::index() const {
  return *index_;
}

template <class Distance>
inline std::vector<typename S2ClosestEdgeQueryBase<Distance>::Result>
S2ClosestEdgeQueryBase<Distance>::FindClosestEdges(Target* target,
                                                   const Options& options) {
  std::vector<Result> results;
  FindClosestEdges(target, options, &results);
  return results;
}

template <class Distance>
typename S2ClosestEdgeQueryBase<Distance>::Result
S2ClosestEdgeQueryBase<Distance>::FindClosestEdge(Target* target,
                                                  const Options& options) {
  S2_DCHECK_EQ(options.max_results(), 1);
  FindClosestEdgesInternal(target, options);
  return result_singleton_;
}

template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::FindClosestEdges(
    Target* target, const Options& options,
    std::vector<Result>* results) {
  FindClosestEdgesInternal(target, options);
  results->clear();
  if (options.max_results() == 1) {
    if (result_singleton_.shape_id() >= 0) {
      results->push_back(result_singleton_);
    }
  } else if (options.max_results() == Options::kMaxMaxResults) {
    std::sort(result_vector_.begin(), result_vector_.end());
    std::unique_copy(result_vector_.begin(), result_vector_.end(),
                     std::back_inserter(*results));
    result_vector_.clear();
  } else {
    results->assign(result_set_.begin(), result_set_.end());
    result_set_.clear();
  }
}

template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::FindClosestEdgesInternal(
    Target* target, const Options& options) {
  target_ = target;
  options_ = &options;

  tested_edges_.clear();
  distance_limit_ = options.max_distance();
  result_singleton_ = Result();
  S2_DCHECK(result_vector_.empty());
  S2_DCHECK(result_set_.empty());
  S2_DCHECK_GE(target->max_brute_force_index_size(), 0);
  if (distance_limit_ == Distance::Zero()) return;

  if (options.max_results() == Options::kMaxMaxResults &&
      options.max_distance() == Distance::Infinity()) {
    S2_LOG(WARNING) << "Returning all edges (max_results/max_distance not set)";
  }

  if (options.include_interiors()) {
    gtl::btree_set<int32> shape_ids;
    (void) target->VisitContainingShapes(
        *index_, [&shape_ids, &options](S2Shape* containing_shape,
                                        const S2Point& target_point) {
          shape_ids.insert(containing_shape->id());
          return shape_ids.size() < options.max_results();
        });
    for (int shape_id : shape_ids) {
      AddResult(Result(Distance::Zero(), shape_id, -1));
    }
    if (distance_limit_ == Distance::Zero()) return;
  }

  // If max_error() > 0 and the target takes advantage of this, then we may
  // need to adjust the distance estimates to the priority queue cells to
  // ensure that they are always a lower bound on the true distance.  For
  // example, suppose max_distance == 100, max_error == 30, and we compute the
  // distance to the target from some cell C0 as d(C0) == 80.  Then because
  // the target takes advantage of max_error(), the true distance could be as
  // low as 50.  In order not to miss edges contained by such cells, we need
  // to subtract max_error() from the distance estimates.  This behavior is
  // controlled by the use_conservative_cell_distance_ flag.
  //
  // However there is one important case where this adjustment is not
  // necessary, namely when max_distance() < max_error().  This is because
  // max_error() only affects the algorithm once at least max_results() edges
  // have been found that satisfy the given distance limit.  At that point,
  // max_error() is subtracted from distance_limit_ in order to ensure that
  // any further matches are closer by at least that amount.  But when
  // max_distance() < max_error(), this reduces the distance limit to 0,
  // i.e. all remaining candidate cells and edges can safely be discarded.
  // (Note that this is how IsDistanceLess() and friends are implemented.)
  //
  // Note that Distance::Delta only supports operator==.
  bool target_uses_max_error = (!(options.max_error() == Delta::Zero()) &&
                                target_->set_max_error(options.max_error()));

  // Note that we can't compare max_error() and distance_limit_ directly
  // because one is a Delta and one is a Distance.  Instead we subtract them.
  use_conservative_cell_distance_ = target_uses_max_error &&
      (distance_limit_ == Distance::Infinity() ||
       Distance::Zero() < distance_limit_ - options.max_error());

  // Use the brute force algorithm if the index is small enough.  To avoid
  // spending too much time counting edges when there are many shapes, we stop
  // counting once there are too many edges.  We may need to recount the edges
  // if we later see a target with a larger brute force edge threshold.
  int min_optimized_edges = target_->max_brute_force_index_size() + 1;
  if (min_optimized_edges > index_num_edges_limit_ &&
      index_num_edges_ >= index_num_edges_limit_) {
    index_num_edges_ = s2shapeutil::CountEdgesUpTo(*index_,
                                                   min_optimized_edges);
    index_num_edges_limit_ = min_optimized_edges;
  }

  if (options.use_brute_force() || index_num_edges_ < min_optimized_edges) {
    // The brute force algorithm considers each edge exactly once.
    avoid_duplicates_ = false;
    FindClosestEdgesBruteForce();
  } else {
    // If the target takes advantage of max_error() then we need to avoid
    // duplicate edges explicitly.  (Otherwise it happens automatically.)
    avoid_duplicates_ = (target_uses_max_error && options.max_results() > 1);
    FindClosestEdgesOptimized();
  }
}

template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::FindClosestEdgesBruteForce() {
  for (S2Shape* shape : *index_) {
    if (shape == nullptr) continue;
    int num_edges = shape->num_edges();
    for (int e = 0; e < num_edges; ++e) {
      MaybeAddResult(*shape, e);
    }
  }
}

template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::FindClosestEdgesOptimized() {
  InitQueue();
  // Repeatedly find the closest S2Cell to "target" and either split it into
  // its four children or process all of its edges.
  while (!queue_.empty()) {
    // We need to copy the top entry before removing it, and we need to
    // remove it before adding any new entries to the queue.
    QueueEntry entry = queue_.top();
    queue_.pop();
    // Work around weird parse error in gcc 4.9 by using a local variable for
    // entry.distance.
    Distance distance = entry.distance;
    if (!(distance < distance_limit_)) {
      queue_ = CellQueue();  // Clear any remaining entries.
      break;
    }
    // If this is already known to be an index cell, just process it.
    if (entry.index_cell != nullptr) {
      ProcessEdges(entry);
      continue;
    }
    // Otherwise split the cell into its four children.  Before adding a
    // child back to the queue, we first check whether it is empty.  We do
    // this in two seek operations rather than four by seeking to the key
    // between children 0 and 1 and to the key between children 2 and 3.
    S2CellId id = entry.id;
    iter_.Seek(id.child(1).range_min());
    if (!iter_.done() && iter_.id() <= id.child(1).range_max()) {
      ProcessOrEnqueue(id.child(1));
    }
    if (iter_.Prev() && iter_.id() >= id.range_min()) {
      ProcessOrEnqueue(id.child(0));
    }
    iter_.Seek(id.child(3).range_min());
    if (!iter_.done() && iter_.id() <= id.range_max()) {
      ProcessOrEnqueue(id.child(3));
    }
    if (iter_.Prev() && iter_.id() >= id.child(2).range_min()) {
      ProcessOrEnqueue(id.child(2));
    }
  }
}

template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::InitQueue() {
  S2_DCHECK(queue_.empty());
  if (index_covering_.empty()) {
    // We delay iterator initialization until now to make queries on very
    // small indexes a bit faster (i.e., where brute force is used).
    iter_.Init(index_, S2ShapeIndex::UNPOSITIONED);
  }

  // Optimization: if the user is searching for just the closest edge, and the
  // center of the target's bounding cap happens to intersect an index cell,
  // then we try to limit the search region to a small disc by first
  // processing the edges in that cell.  This sets distance_limit_ based on
  // the closest edge in that cell, which we can then use to limit the search
  // area.  This means that the cell containing "target" will be processed
  // twice, but in general this is still faster.
  //
  // TODO(ericv): Even if the cap center is not contained, we could still
  // process one or both of the adjacent index cells in S2CellId order,
  // provided that those cells are closer than distance_limit_.
  S2Cap cap = target_->GetCapBound();
  if (options().max_results() == 1 && iter_.Locate(cap.center())) {
    ProcessEdges(QueueEntry(Distance::Zero(), iter_.id(), &iter_.cell()));
    // Skip the rest of the algorithm if we found an intersecting edge.
    if (distance_limit_ == Distance::Zero()) return;
  }
  if (index_covering_.empty()) InitCovering();
  if (distance_limit_ == Distance::Infinity()) {
    // Start with the precomputed index covering.
    for (int i = 0; i < index_covering_.size(); ++i) {
      ProcessOrEnqueue(index_covering_[i], index_cells_[i]);
    }
  } else {
    // Compute a covering of the search disc and intersect it with the
    // precomputed index covering.
    S2RegionCoverer coverer;
    coverer.mutable_options()->set_max_cells(4);
    S1ChordAngle radius = cap.radius() + distance_limit_.GetChordAngleBound();
    S2Cap search_cap(cap.center(), radius);
    coverer.GetFastCovering(search_cap, &max_distance_covering_);
    S2CellUnion::GetIntersection(index_covering_, max_distance_covering_,
                                 &initial_cells_);

    // Now we need to clean up the initial cells to ensure that they all
    // contain at least one cell of the S2ShapeIndex.  (Some may not intersect
    // the index at all, while other may be descendants of an index cell.)
    for (int i = 0, j = 0; i < initial_cells_.size(); ) {
      S2CellId id_i = initial_cells_[i];
      // Find the top-level cell that contains this initial cell.
      while (index_covering_[j].range_max() < id_i) ++j;
      S2CellId id_j = index_covering_[j];
      if (id_i == id_j) {
        // This initial cell is one of the top-level cells.  Use the
        // precomputed S2ShapeIndexCell pointer to avoid an index seek.
        ProcessOrEnqueue(id_j, index_cells_[j]);
        ++i, ++j;
      } else {
        // This initial cell is a proper descendant of a top-level cell.
        // Check how it is related to the cells of the S2ShapeIndex.
        S2ShapeIndex::CellRelation r = iter_.Locate(id_i);
        if (r == S2ShapeIndex::INDEXED) {
          // This cell is a descendant of an index cell.  Enqueue it and skip
          // any other initial cells that are also descendants of this cell.
          ProcessOrEnqueue(iter_.id(), &iter_.cell());
          const S2CellId last_id = iter_.id().range_max();
          while (++i < initial_cells_.size() && initial_cells_[i] <= last_id)
            continue;
        } else {
          // Enqueue the cell only if it contains at least one index cell.
          if (r == S2ShapeIndex::SUBDIVIDED) ProcessOrEnqueue(id_i, nullptr);
          ++i;
        }
      }
    }
  }
}

template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::InitCovering() {
  // Find the range of S2Cells spanned by the index and choose a level such
  // that the entire index can be covered with just a few cells.  These are
  // the "top-level" cells.  There are two cases:
  //
  //  - If the index spans more than one face, then there is one top-level cell
  // per spanned face, just big enough to cover the index cells on that face.
  //
  //  - If the index spans only one face, then we find the smallest cell "C"
  // that covers the index cells on that face (just like the case above).
  // Then for each of the 4 children of "C", if the child contains any index
  // cells then we create a top-level cell that is big enough to just fit
  // those index cells (i.e., shrinking the child as much as possible to fit
  // its contents).  This essentially replicates what would happen if we
  // started with "C" as the top-level cell, since "C" would immediately be
  // split, except that we take the time to prune the children further since
  // this will save work on every subsequent query.

  // Don't need to reserve index_cells_ since it is an InlinedVector.
  index_covering_.reserve(6);

  // TODO(ericv): Use a single iterator (iter_) below and save position
  // information using pair<S2CellId, const S2ShapeIndexCell*> type.
  S2ShapeIndex::Iterator next(index_, S2ShapeIndex::BEGIN);
  S2ShapeIndex::Iterator last(index_, S2ShapeIndex::END);
  last.Prev();
  if (next.id() != last.id()) {
    // The index has at least two cells.  Choose a level such that the entire
    // index can be spanned with at most 6 cells (if the index spans multiple
    // faces) or 4 cells (it the index spans a single face).
    int level = next.id().GetCommonAncestorLevel(last.id()) + 1;

    // Visit each potential top-level cell except the last (handled below).
    S2CellId last_id = last.id().parent(level);
    for (S2CellId id = next.id().parent(level); id != last_id; id = id.next()) {
      // Skip any top-level cells that don't contain any index cells.
      if (id.range_max() < next.id()) continue;

      // Find the range of index cells contained by this top-level cell and
      // then shrink the cell if necessary so that it just covers them.
      S2ShapeIndex::Iterator cell_first = next;
      next.Seek(id.range_max().next());
      S2ShapeIndex::Iterator cell_last = next;
      cell_last.Prev();
      AddInitialRange(cell_first, cell_last);
    }
  }
  AddInitialRange(next, last);
}

// Add an entry to index_covering_ and index_cells_ that covers the given
// inclusive range of cells.
//
// REQUIRES: "first" and "last" have a common ancestor.
template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::AddInitialRange(
    const S2ShapeIndex::Iterator& first,
    const S2ShapeIndex::Iterator& last) {
  if (first.id() == last.id()) {
    // The range consists of a single index cell.
    index_covering_.push_back(first.id());
    index_cells_.push_back(&first.cell());
  } else {
    // Add the lowest common ancestor of the given range.
    int level = first.id().GetCommonAncestorLevel(last.id());
    S2_DCHECK_GE(level, 0);
    index_covering_.push_back(first.id().parent(level));
    index_cells_.push_back(nullptr);
  }
}

template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::MaybeAddResult(
    const S2Shape& shape, int edge_id) {
  if (avoid_duplicates_ &&
      !tested_edges_.insert(ShapeEdgeId(shape.id(), edge_id)).second) {
    return;
  }
  auto edge = shape.edge(edge_id);
  Distance distance = distance_limit_;
  if (target_->UpdateMinDistance(edge.v0, edge.v1, &distance)) {
    AddResult(Result(distance, shape.id(), edge_id));
  }
}

template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::AddResult(const Result& result) {
  if (options().max_results() == 1) {
    // Optimization for the common case where only the closest edge is wanted.
    result_singleton_ = result;
    distance_limit_ = result.distance() - options().max_error();
  } else if (options().max_results() == Options::kMaxMaxResults) {
    result_vector_.push_back(result);  // Sort/unique at end.
  } else {
    // Add this edge to result_set_.  Note that even if we already have enough
    // edges, we can't erase an element before insertion because the "new"
    // edge might in fact be a duplicate.
    result_set_.insert(result);
    int size = result_set_.size();
    if (size >= options().max_results()) {
      if (size > options().max_results()) {
        result_set_.erase(--result_set_.end());
      }
      distance_limit_ = (--result_set_.end())->distance() -
                        options().max_error();
    }
  }
}

// Return the number of edges in the given index cell.
inline static int CountEdges(const S2ShapeIndexCell* cell) {
  int count = 0;
  for (int s = 0; s < cell->num_clipped(); ++s) {
    count += cell->clipped(s).num_edges();
  }
  return count;
}

// Process all the edges of the given index cell.
template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::ProcessEdges(const QueueEntry& entry) {
  const S2ShapeIndexCell* index_cell = entry.index_cell;
  for (int s = 0; s < index_cell->num_clipped(); ++s) {
    const S2ClippedShape& clipped = index_cell->clipped(s);
    const S2Shape* shape = index_->shape(clipped.shape_id());
    for (int j = 0; j < clipped.num_edges(); ++j) {
      MaybeAddResult(*shape, clipped.edge(j));
    }
  }
}

// Enqueue the given cell id.
// REQUIRES: iter_ is positioned at a cell contained by "id".
template <class Distance>
inline void S2ClosestEdgeQueryBase<Distance>::ProcessOrEnqueue(
    S2CellId id) {
  S2_DCHECK(id.contains(iter_.id()));
  if (iter_.id() == id) {
    ProcessOrEnqueue(id, &iter_.cell());
  } else {
    ProcessOrEnqueue(id, nullptr);
  }
}

// Add the given cell id to the queue.  "index_cell" is the corresponding
// S2ShapeIndexCell, or nullptr if "id" is not an index cell.
//
// This version is called directly only by InitQueue().
template <class Distance>
void S2ClosestEdgeQueryBase<Distance>::ProcessOrEnqueue(
    S2CellId id, const S2ShapeIndexCell* index_cell) {
  if (index_cell) {
    // If this index cell has only a few edges, then it is faster to check
    // them directly rather than computing the minimum distance to the S2Cell
    // and inserting it into the queue.
    static const int kMinEdgesToEnqueue = 10;
    int num_edges = CountEdges(index_cell);
    if (num_edges == 0) return;
    if (num_edges < kMinEdgesToEnqueue) {
      // Set "distance" to zero to avoid the expense of computing it.
      ProcessEdges(QueueEntry(Distance::Zero(), id, index_cell));
      return;
    }
  }
  // Otherwise compute the minimum distance to any point in the cell and add
  // it to the priority queue.
  S2Cell cell(id);
  Distance distance = distance_limit_;
  if (!target_->UpdateMinDistance(cell, &distance)) return;
  if (use_conservative_cell_distance_) {
    // Ensure that "distance" is a lower bound on the true distance to the cell.
    distance = distance - options().max_error();  // operator-=() not defined.
  }
  queue_.push(QueueEntry(distance, id, index_cell));
}

#endif  // S2_S2CLOSEST_EDGE_QUERY_BASE_H_
