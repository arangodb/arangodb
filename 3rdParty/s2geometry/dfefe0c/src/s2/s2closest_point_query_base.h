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
//
// See S2ClosestPointQueryBase (defined below) for an overview.

#ifndef S2_S2CLOSEST_POINT_QUERY_BASE_H_
#define S2_S2CLOSEST_POINT_QUERY_BASE_H_

#include <vector>

#include "s2/base/logging.h"
#include "s2/third_party/absl/container/inlined_vector.h"
#include "s2/s1chord_angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"
#include "s2/s2distance_target.h"
#include "s2/s2edge_distances.h"
#include "s2/s2point_index.h"
#include "s2/s2region_coverer.h"

// Options that control the set of points returned.  Note that by default
// *all* points are returned, so you will always want to set either the
// max_results() option or the max_distance() option (or both).
//
// This class is also available as S2ClosestPointQueryBase<Data>::Options.
// (It is defined here to avoid depending on the "Data" template argument.)
//
// The Distance template argument is described below.
template <class Distance>
class S2ClosestPointQueryBaseOptions {
 public:
  using Delta = typename Distance::Delta;

  S2ClosestPointQueryBaseOptions();

  // Specifies that at most "max_results" points should be returned.
  //
  // REQUIRES: max_results >= 1
  // DEFAULT: numeric_limits<int>::max()
  int max_results() const;
  void set_max_results(int max_results);
  static constexpr int kMaxMaxResults = std::numeric_limits<int>::max();

  // Specifies that only points whose distance to the target is less than
  // "max_distance" should be returned.
  //
  // Note that points whose distance is exactly equal to "max_distance" are
  // not returned.  In most cases this doesn't matter (since distances are
  // not computed exactly in the first place), but if such points are needed
  // then you can retrieve them by specifying "max_distance" as the next
  // largest representable Distance.  For example, if Distance is an
  // S1ChordAngle then you can specify max_distance.Successor().
  //
  // DEFAULT: Distance::Infinity()
  Distance max_distance() const;
  void set_max_distance(Distance max_distance);

  // Specifies that points up to max_error() further away than the true
  // closest points may be substituted in the result set, as long as such
  // points satisfy all the remaining search criteria (such as max_distance).
  // This option only has an effect if max_results() is also specified;
  // otherwise all points closer than max_distance() will always be returned.
  //
  // Note that this does not affect how the distance between points is
  // computed; it simply gives the algorithm permission to stop the search
  // early as soon as the best possible improvement drops below max_error().
  //
  // This can be used to implement distance predicates efficiently.  For
  // example, to determine whether the minimum distance is less than D, the
  // IsDistanceLess() method sets max_results() == 1 and max_distance() ==
  // max_error() == D.  This causes the algorithm to terminate as soon as it
  // finds any point whose distance is less than D, rather than continuing to
  // search for a point that is even closer.
  //
  // DEFAULT: Distance::Delta::Zero()
  Delta max_error() const;
  void set_max_error(Delta max_error);

  // Specifies that points must be contained by the given S2Region.  "region"
  // is owned by the caller and must persist during the lifetime of this
  // object.  The value may be changed between calls to FindClosestPoints(),
  // or reset by calling set_region(nullptr).
  //
  // Note that if you want to set the region to a disc around a target point,
  // it is faster to use a PointTarget with set_max_distance() instead.  You
  // can also call both methods, e.g. to set a maximum distance and also
  // require that points lie within a given rectangle.
  const S2Region* region() const;
  void set_region(const S2Region* region);

  // Specifies that distances should be computed by examining every point
  // rather than using the S2ShapeIndex.  This is useful for testing,
  // benchmarking, and debugging.
  //
  // DEFAULT: false
  bool use_brute_force() const;
  void set_use_brute_force(bool use_brute_force);

 private:
  Distance max_distance_ = Distance::Infinity();
  Delta max_error_ = Delta::Zero();
  const S2Region* region_ = nullptr;
  int max_results_ = kMaxMaxResults;
  bool use_brute_force_ = false;
};

// S2ClosestPointQueryBase is a templatized class for finding the closest
// point(s) to a given target.  It is not intended to be used directly, but
// rather to serve as the implementation of various specialized classes with
// more convenient APIs (such as S2ClosestPointQuery).  It is flexible enough
// so that it can be adapted to compute maximum distances and even potentially
// Hausdorff distances.
//
// By using the appropriate options, this class can answer questions such as:
//
//  - Find the minimum distance between a point collection A and a target B.
//  - Find all points in collection A that are within a distance D of target B.
//  - Find the k points of collection A that are closest to a given point P.
//
// The target is any class that implements the S2DistanceTarget interface.
// There are predefined targets for points, edges, S2Cells, and S2ShapeIndexes
// (arbitrary collctions of points, polylines, and polygons).
//
// The Distance template argument is used to represent distances.  Usually it
// is a thin wrapper around S1ChordAngle, but another distance type may be
// used as long as it implements the Distance concept described in
// s2distance_targets.h.  For example this can be used to measure maximum
// distances, to get more accuracy, or to measure non-spheroidal distances.
template <class Distance, class Data>
class S2ClosestPointQueryBase {
 public:
  using Delta = typename Distance::Delta;
  using Index = S2PointIndex<Data>;
  using PointData = typename Index::PointData;
  using Options = S2ClosestPointQueryBaseOptions<Distance>;

  // The Target class represents the geometry to which the distance is
  // measured.  For example, there can be subtypes for measuring the distance
  // to a point, an edge, or to an S2ShapeIndex (an arbitrary collection of
  // geometry).
  //
  // Implementations do *not* need to be thread-safe.  They may cache data or
  // allocate temporary data structures in order to improve performance.
  using Target = S2DistanceTarget<Distance>;

  // Each "Result" object represents a closest point.
  class Result {
   public:
    // The default constructor creates an "empty" result, with a distance() of
    // Infinity() and non-dereferencable point() and data() values.
    Result() : distance_(Distance::Infinity()), point_data_(nullptr) {}

    // Constructs a Result object for the given point.
    Result(Distance distance, const PointData* point_data)
        : distance_(distance), point_data_(point_data) {}

    // Returns true if this Result object does not refer to any data point.
    // (The only case where an empty Result is returned is when the
    // FindClosestPoint() method does not find any points that meet the
    // specified criteria.)
    bool is_empty() const { return point_data_ == nullptr; }

    // The distance from the target to this point.
    Distance distance() const { return distance_; }

    // The point itself.
    const S2Point& point() const { return point_data_->point(); }

    // The client-specified data associated with this point.
    const Data& data() const { return point_data_->data(); }

    // Returns true if two Result objects are identical.
    friend bool operator==(const Result& x, const Result& y) {
      return (x.distance_ == y.distance_ && x.point_data_ == y.point_data_);
    }

    // Compares two Result objects first by distance, then by point_data().
    friend bool operator<(const Result& x, const Result& y) {
      if (x.distance_ < y.distance_) return true;
      if (y.distance_ < x.distance_) return false;
      return x.point_data_ < y.point_data_;
    }

   private:
    Distance distance_;
    const PointData* point_data_;
  };

  // The minimum number of points that a cell must contain to enqueue it
  // rather than processing its contents immediately.
  static constexpr int kMinPointsToEnqueue = 13;

  // Default constructor; requires Init() to be called.
  S2ClosestPointQueryBase();
  ~S2ClosestPointQueryBase();

  // Convenience constructor that calls Init().
  explicit S2ClosestPointQueryBase(const Index* index);

  // S2ClosestPointQueryBase is not copyable.
  S2ClosestPointQueryBase(const S2ClosestPointQueryBase&) = delete;
  void operator=(const S2ClosestPointQueryBase&) = delete;

  // Initializes the query.
  // REQUIRES: ReInit() must be called if "index" is modified.
  void Init(const Index* index);

  // Reinitializes the query.  This method must be called whenever the
  // underlying index is modified.
  void ReInit();

  // Return a reference to the underlying S2PointIndex.
  const Index& index() const;

  // Returns the closest points to the given target that satisfy the given
  // options.  This method may be called multiple times.
  std::vector<Result> FindClosestPoints(Target* target, const Options& options);

  // This version can be more efficient when this method is called many times,
  // since it does not require allocating a new vector on each call.
  void FindClosestPoints(Target* target, const Options& options,
                         std::vector<Result>* results);

  // Convenience method that returns exactly one point.  If no points satisfy
  // the given search criteria, then a Result with distance() == Infinity()
  // and is_empty() == true is returned.
  //
  // REQUIRES: options.max_results() == 1
  Result FindClosestPoint(Target* target, const Options& options);

 private:
  using Iterator = typename Index::Iterator;

  const Options& options() const { return *options_; }
  void FindClosestPointsInternal(Target* target, const Options& options);
  void FindClosestPointsBruteForce();
  void FindClosestPointsOptimized();
  void InitQueue();
  void InitCovering();
  void AddInitialRange(S2CellId first_id, S2CellId last_id);
  void MaybeAddResult(const PointData* point_data);
  bool ProcessOrEnqueue(S2CellId id, Iterator* iter, bool seek);

  const Index* index_;
  const Options* options_;
  Target* target_;

  // True if max_error() must be subtracted from priority queue cell distances
  // in order to ensure that such distances are measured conservatively.  This
  // is true only if the target takes advantage of max_error() in order to
  // return faster results, and 0 < max_error() < distance_limit_.
  bool use_conservative_cell_distance_;

  // For the optimized algorihm we precompute the top-level S2CellIds that
  // will be added to the priority queue.  There can be at most 6 of these
  // cells.  Essentially this is just a covering of the indexed points.
  std::vector<S2CellId> index_covering_;

  // The distance beyond which we can safely ignore further candidate points.
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
  //  - Otherwise results are kept in a priority queue so that we can
  //    progressively reduce the distance limit once max_results() results
  //    have been found.
  Result result_singleton_;
  std::vector<Result> result_vector_;
  std::priority_queue<Result, absl::InlinedVector<Result, 16>> result_set_;

  // The algorithm maintains a priority queue of unprocessed S2CellIds, sorted
  // in increasing order of distance from the target.
  struct QueueEntry {
    // A lower bound on the distance from the target to "id".  This is the key
    // of the priority queue.
    Distance distance;

    // The cell being queued.
    S2CellId id;

    QueueEntry(Distance _distance, S2CellId _id)
        : distance(_distance), id(_id) {}

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

  Iterator iter_;
  std::vector<S2CellId> region_covering_;
  std::vector<S2CellId> max_distance_covering_;
  std::vector<S2CellId> intersection_with_region_;
  std::vector<S2CellId> intersection_with_max_distance_;
  const PointData* tmp_point_data_[kMinPointsToEnqueue - 1];
};


//////////////////   Implementation details follow   ////////////////////


template <class Distance> inline
S2ClosestPointQueryBaseOptions<Distance>::S2ClosestPointQueryBaseOptions() {
}

template <class Distance>
inline int S2ClosestPointQueryBaseOptions<Distance>::max_results() const {
  return max_results_;
}

template <class Distance>
inline void S2ClosestPointQueryBaseOptions<Distance>::set_max_results(
    int max_results) {
  S2_DCHECK_GE(max_results, 1);
  max_results_ = max_results;
}

template <class Distance>
inline Distance S2ClosestPointQueryBaseOptions<Distance>::max_distance()
    const {
  return max_distance_;
}

template <class Distance>
inline void S2ClosestPointQueryBaseOptions<Distance>::set_max_distance(
    Distance max_distance) {
  max_distance_ = max_distance;
}

template <class Distance>
inline typename Distance::Delta
S2ClosestPointQueryBaseOptions<Distance>::max_error() const {
  return max_error_;
}

template <class Distance>
inline void S2ClosestPointQueryBaseOptions<Distance>::set_max_error(
    Delta max_error) {
  max_error_ = max_error;
}

template <class Distance>
inline const S2Region* S2ClosestPointQueryBaseOptions<Distance>::region()
    const {
  return region_;
}

template <class Distance>
inline void S2ClosestPointQueryBaseOptions<Distance>::set_region(
    const S2Region* region) {
  region_ = region;
}

template <class Distance>
inline bool S2ClosestPointQueryBaseOptions<Distance>::use_brute_force() const {
  return use_brute_force_;
}

template <class Distance>
inline void S2ClosestPointQueryBaseOptions<Distance>::set_use_brute_force(
    bool use_brute_force) {
  use_brute_force_ = use_brute_force;
}

template <class Distance, class Data>
S2ClosestPointQueryBase<Distance, Data>::S2ClosestPointQueryBase() {
}

template <class Distance, class Data>
S2ClosestPointQueryBase<Distance, Data>::~S2ClosestPointQueryBase() {
  // Prevent inline destructor bloat by providing a definition.
}

template <class Distance, class Data>
inline S2ClosestPointQueryBase<Distance, Data>::S2ClosestPointQueryBase(
    const S2PointIndex<Data>* index) : S2ClosestPointQueryBase() {
  Init(index);
}

template <class Distance, class Data>
void S2ClosestPointQueryBase<Distance, Data>::Init(
    const S2PointIndex<Data>* index) {
  index_ = index;
  ReInit();
}

template <class Distance, class Data>
void S2ClosestPointQueryBase<Distance, Data>::ReInit() {
  iter_.Init(index_);
  index_covering_.clear();
}

template <class Distance, class Data>
inline const S2PointIndex<Data>&
S2ClosestPointQueryBase<Distance, Data>::index() const {
  return *index_;
}

template <class Distance, class Data>
inline std::vector<typename S2ClosestPointQueryBase<Distance, Data>::Result>
S2ClosestPointQueryBase<Distance, Data>::FindClosestPoints(
    Target* target, const Options& options) {
  std::vector<Result> results;
  FindClosestPoints(target, options, &results);
  return results;
}

template <class Distance, class Data>
typename S2ClosestPointQueryBase<Distance, Data>::Result
S2ClosestPointQueryBase<Distance, Data>::FindClosestPoint(
    Target* target, const Options& options) {
  S2_DCHECK_EQ(options.max_results(), 1);
  FindClosestPointsInternal(target, options);
  return result_singleton_;
}

template <class Distance, class Data>
void S2ClosestPointQueryBase<Distance, Data>::FindClosestPoints(
    Target* target, const Options& options, std::vector<Result>* results) {
  FindClosestPointsInternal(target, options);
  results->clear();
  if (options.max_results() == 1) {
    if (!result_singleton_.is_empty()) {
      results->push_back(result_singleton_);
    }
  } else if (options.max_results() == Options::kMaxMaxResults) {
    std::sort(result_vector_.begin(), result_vector_.end());
    std::unique_copy(result_vector_.begin(), result_vector_.end(),
                     std::back_inserter(*results));
    result_vector_.clear();
  } else {
    results->reserve(result_set_.size());
    for (; !result_set_.empty(); result_set_.pop()) {
      results->push_back(result_set_.top());
    }
    // The priority queue returns the largest elements first.
    std::reverse(results->begin(), results->end());
    S2_DCHECK(std::is_sorted(results->begin(), results->end()));
  }
}

template <class Distance, class Data>
void S2ClosestPointQueryBase<Distance, Data>::FindClosestPointsInternal(
    Target* target, const Options& options) {
  target_ = target;
  options_ = &options;

  distance_limit_ = options.max_distance();
  result_singleton_ = Result();
  S2_DCHECK(result_vector_.empty());
  S2_DCHECK(result_set_.empty());
  S2_DCHECK_GE(target->max_brute_force_index_size(), 0);
  if (distance_limit_ == Distance::Zero()) return;

  if (options.max_results() == Options::kMaxMaxResults &&
      options.max_distance() == Distance::Infinity() &&
      options.region() == nullptr) {
    S2_LOG(WARNING) << "Returning all points "
                    "(max_results/max_distance/region not set)";
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

  // Note that given point is processed only once (unlike S2ClosestEdgeQuery),
  // and therefore we don't need to worry about the possibility of having
  // duplicate points in the results.
  if (options.use_brute_force() ||
      index_->num_points() <= target_->max_brute_force_index_size()) {
    FindClosestPointsBruteForce();
  } else {
    FindClosestPointsOptimized();
  }
}

template <class Distance, class Data>
void S2ClosestPointQueryBase<Distance, Data>::FindClosestPointsBruteForce() {
  for (iter_.Begin(); !iter_.done(); iter_.Next()) {
    MaybeAddResult(&iter_.point_data());
  }
}

template <class Distance, class Data>
void S2ClosestPointQueryBase<Distance, Data>::FindClosestPointsOptimized() {
  InitQueue();
  while (!queue_.empty()) {
    // We need to copy the top entry before removing it, and we need to remove
    // it before adding any new entries to the queue.
    QueueEntry entry = queue_.top();
    queue_.pop();
    // Work around weird parse error in gcc 4.9 by using a local variable for
    // entry.distance.
    Distance distance = entry.distance;
    if (!(distance < distance_limit_)) {
      queue_ = CellQueue();  // Clear any remaining entries.
      break;
    }
    S2CellId child = entry.id.child_begin();
    // We already know that it has too many points, so process its children.
    // Each child may either be processed directly or enqueued again.  The
    // loop is optimized so that we don't seek unnecessarily.
    bool seek = true;
    for (int i = 0; i < 4; ++i, child = child.next()) {
      seek = ProcessOrEnqueue(child, &iter_, seek);
    }
  }
}

template <class Distance, class Data>
void S2ClosestPointQueryBase<Distance, Data>::InitQueue() {
  S2_DCHECK(queue_.empty());

  // Optimization: rather than starting with the entire index, see if we can
  // limit the search region to a small disc.  Then we can find a covering for
  // that disc and intersect it with the covering for the index.  This can
  // save a lot of work when the search region is small.
  S2Cap cap = target_->GetCapBound();
  if (options().max_results() == 1) {
    // If the user is searching for just the closest point, we can compute an
    // upper bound on search radius by seeking to the center of the target's
    // bounding cap and looking at the adjacent index points (in S2CellId
    // order).  The minimum distance to either of these points is an upper
    // bound on the search radius.
    //
    // TODO(ericv): The same strategy would also work for small values of
    // max_results() > 1, e.g. max_results() == 20, except that we would need to
    // examine more neighbors (at least 20, and preferably 20 in each
    // direction).  It's not clear whether this is a common case, though, and
    // also this would require extending MaybeAddResult() so that it can
    // remove duplicate entries.  (The points added here may be re-added by
    // ProcessOrEnqueue(), but this is okay when max_results() == 1.)
    iter_.Seek(S2CellId(cap.center()));
    if (!iter_.done()) {
      MaybeAddResult(&iter_.point_data());
    }
    if (iter_.Prev()) {
      MaybeAddResult(&iter_.point_data());
    }
    // Skip the rest of the algorithm if we found a matching point.
    if (distance_limit_ == Distance::Zero()) return;
  }
  // We start with a covering of the set of indexed points, then intersect it
  // with the given region (if any) and maximum search radius disc (if any).
  if (index_covering_.empty()) InitCovering();
  const std::vector<S2CellId>* initial_cells = &index_covering_;
  if (options().region()) {
    S2RegionCoverer coverer;
    coverer.mutable_options()->set_max_cells(4);
    coverer.GetCovering(*options().region(), &region_covering_);
    S2CellUnion::GetIntersection(index_covering_, region_covering_,
                                 &intersection_with_region_);
    initial_cells = &intersection_with_region_;
  }
  if (distance_limit_ < Distance::Infinity()) {
    S2RegionCoverer coverer;
    coverer.mutable_options()->set_max_cells(4);
    S1ChordAngle radius = cap.radius() + distance_limit_.GetChordAngleBound();
    S2Cap search_cap(cap.center(), radius);
    coverer.GetFastCovering(search_cap, &max_distance_covering_);
    S2CellUnion::GetIntersection(*initial_cells, max_distance_covering_,
                                 &intersection_with_max_distance_);
    initial_cells = &intersection_with_max_distance_;
  }
  iter_.Begin();
  for (int i = 0; i < initial_cells->size() && !iter_.done(); ++i) {
    S2CellId id = (*initial_cells)[i];
    ProcessOrEnqueue(id, &iter_, id.range_min() > iter_.id() /*seek*/);
  }
}

template <class Distance, class Data>
void S2ClosestPointQueryBase<Distance, Data>::InitCovering() {
  // Compute the "index covering", which is a small number of S2CellIds that
  // cover the indexed points.  There are two cases:
  //
  //  - If the index spans more than one face, then there is one covering cell
  // per spanned face, just big enough to cover the index cells on that face.
  //
  //  - If the index spans only one face, then we find the smallest cell "C"
  // that covers the index cells on that face (just like the case above).
  // Then for each of the 4 children of "C", if the child contains any index
  // cells then we create a covering cell that is big enough to just fit
  // those index cells (i.e., shrinking the child as much as possible to fit
  // its contents).  This essentially replicates what would happen if we
  // started with "C" as the covering cell, since "C" would immediately be
  // split, except that we take the time to prune the children further since
  // this will save work on every subsequent query.
  index_covering_.reserve(6);
  iter_.Finish();
  if (!iter_.Prev()) return;  // Empty index.
  S2CellId index_last_id = iter_.id();
  iter_.Begin();
  if (iter_.id() != index_last_id) {
    // The index has at least two cells.  Choose a level such that the entire
    // index can be spanned with at most 6 cells (if the index spans multiple
    // faces) or 4 cells (it the index spans a single face).
    int level = iter_.id().GetCommonAncestorLevel(index_last_id) + 1;

    // Visit each potential covering cell except the last (handled below).
    S2CellId last_id = index_last_id.parent(level);
    for (S2CellId id = iter_.id().parent(level);
         id != last_id; id = id.next()) {
      // Skip any covering cells that don't contain any index cells.
      if (id.range_max() < iter_.id()) continue;

      // Find the range of index cells contained by this covering cell and
      // then shrink the cell if necessary so that it just covers them.
      S2CellId cell_first_id = iter_.id();
      iter_.Seek(id.range_max().next());
      iter_.Prev();
      S2CellId cell_last_id = iter_.id();
      iter_.Next();
      AddInitialRange(cell_first_id, cell_last_id);
    }
  }
  AddInitialRange(iter_.id(), index_last_id);
}

// Adds a cell to index_covering_ that covers the given inclusive range.
//
// REQUIRES: "first" and "last" have a common ancestor.
template <class Distance, class Data>
void S2ClosestPointQueryBase<Distance, Data>::AddInitialRange(
    S2CellId first_id, S2CellId last_id) {
  // Add the lowest common ancestor of the given range.
  int level = first_id.GetCommonAncestorLevel(last_id);
  S2_DCHECK_GE(level, 0);
  index_covering_.push_back(first_id.parent(level));
}

template <class Distance, class Data>
void S2ClosestPointQueryBase<Distance, Data>::MaybeAddResult(
    const PointData* point_data) {
  Distance distance = distance_limit_;
  if (!target_->UpdateMinDistance(point_data->point(), &distance)) return;

  const S2Region* region = options().region();
  if (region && !region->Contains(point_data->point())) return;

  Result result(distance, point_data);
  if (options().max_results() == 1) {
    // Optimization for the common case where only the closest point is wanted.
    result_singleton_ = result;
    distance_limit_ = result.distance() - options().max_error();
  } else if (options().max_results() == Options::kMaxMaxResults) {
    result_vector_.push_back(result);  // Sort/unique at end.
  } else {
    // Add this point to result_set_.  Note that with the current algorithm
    // each candidate point is considered at most once (except for one special
    // case where max_results() == 1, see InitQueue for details), so we don't
    // need to worry about possibly adding a duplicate entry here.
    if (result_set_.size() >= options().max_results()) {
      result_set_.pop();  // Replace the furthest result point.
    }
    result_set_.push(result);
    if (result_set_.size() >= options().max_results()) {
      distance_limit_ = result_set_.top().distance() - options().max_error();
    }
  }
}

// Either process the contents of the given cell immediately, or add it to the
// queue to be subdivided.  If "seek" is false, then "iter" must already be
// positioned at the first indexed point within or after this cell.
//
// Returns "true" if the cell was added to the queue, and "false" if it was
// processed immediately, in which case "iter" is left positioned at the next
// cell in S2CellId order.
template <class Distance, class Data>
bool S2ClosestPointQueryBase<Distance, Data>::ProcessOrEnqueue(
    S2CellId id, Iterator* iter, bool seek) {
  if (seek) iter->Seek(id.range_min());
  if (id.is_leaf()) {
    // Leaf cells can't be subdivided.
    for (; !iter->done() && iter->id() == id; iter->Next()) {
      MaybeAddResult(&iter->point_data());
    }
    return false;  // No need to seek to next child.
  }
  S2CellId last = id.range_max();
  int num_points = 0;
  for (; !iter->done() && iter->id() <= last; iter->Next()) {
    if (num_points == kMinPointsToEnqueue - 1) {
      // This cell has too many points (including this one), so enqueue it.
      S2Cell cell(id);
      Distance distance = distance_limit_;
      // We check "region_" second because it may be relatively expensive.
      if (target_->UpdateMinDistance(cell, &distance) &&
          (!options().region() || options().region()->MayIntersect(cell))) {
        if (use_conservative_cell_distance_) {
          // Ensure that "distance" is a lower bound on distance to the cell.
          distance = distance - options().max_error();
        }
        queue_.push(QueueEntry(distance, id));
      }
      return true;  // Seek to next child.
    }
    tmp_point_data_[num_points++] = &iter->point_data();
  }
  // There were few enough points that we might as well process them now.
  for (int i = 0; i < num_points; ++i) {
    MaybeAddResult(tmp_point_data_[i]);
  }
  return false;  // No need to seek to next child.
}

#endif  // S2_S2CLOSEST_POINT_QUERY_BASE_H_
