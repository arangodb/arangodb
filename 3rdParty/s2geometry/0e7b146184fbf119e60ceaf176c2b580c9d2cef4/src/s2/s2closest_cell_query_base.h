// Copyright 2018 Google Inc. All Rights Reserved.
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
// See S2ClosestCellQueryBase (defined below) for an overview.

#ifndef S2_S2CLOSEST_CELL_QUERY_BASE_H_
#define S2_S2CLOSEST_CELL_QUERY_BASE_H_

#include <vector>

#include "s2/base/logging.h"
#include "s2/third_party/absl/container/inlined_vector.h"
#include "s2/s1chord_angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_index.h"
#include "s2/s2cell_union.h"
#include "s2/s2distance_target.h"
#include "s2/s2region_coverer.h"
#include "s2/util/gtl/btree_set.h"
#include "s2/util/gtl/dense_hash_set.h"
#include "s2/util/hash/mix.h"

// S2ClosestCellQueryBase is a templatized class for finding the closest
// (cell_id, label) pairs in an S2CellIndex to a given target.  It is not
// intended to be used directly, but rather to serve as the implementation of
// various specialized classes with more convenient APIs (such as
// S2ClosestCellQuery).  It is flexible enough so that it can be adapted to
// compute maximum distances and even potentially Hausdorff distances.
//
// By using the appropriate options, this class can answer questions such as:
//
//  - Find the minimum distance between a cell collection A and a target B.
//  - Find all cells in collection A that are within a distance D of target B.
//  - Find the k cells of collection A that are closest to a given point P.
//
// The target is any class that implements the S2DistanceTarget interface.
// There are predefined targets for points, edges, S2Cells, S2CellUnions, and
// S2ShapeIndexes (arbitrary collctions of points, polylines, and polygons).
//
// The Distance template argument is used to represent distances.  Usually it
// is a thin wrapper around S1ChordAngle, but another distance type may be
// used as long as it implements the Distance concept described in
// s2distance_targets.h.  For example this can be used to measure maximum
// distances, to get more accuracy, or to measure non-spheroidal distances.
template <class Distance>
class S2ClosestCellQueryBase {
 public:
  using Delta = typename Distance::Delta;
  using Label = S2CellIndex::Label;
  using LabelledCell = S2CellIndex::LabelledCell;

  // Options that control the set of cells returned.  Note that by default
  // *all* cells are returned, so you will always want to set either the
  // max_results() option or the max_distance() option (or both).
  //
  // This class is also available as S2ClosestCellQueryBase<Data>::Options.
  //
  // The Distance template argument is described below.
  class Options {
   public:
    Options();

    // Specifies that at most "max_results" cells should be returned.
    //
    // REQUIRES: max_results >= 1
    // DEFAULT: kMaxMaxResults
    int max_results() const;
    void set_max_results(int max_results);
    static constexpr int kMaxMaxResults = std::numeric_limits<int>::max();

    // Specifies that only cells whose distance to the target is less than
    // "max_distance" should be returned.
    //
    // Note that cells whose distance is exactly equal to "max_distance" are
    // not returned.  In most cases this doesn't matter (since distances are
    // not computed exactly in the first place), but if such cells are needed
    // then you can retrieve them by specifying "max_distance" as the next
    // largest representable Distance.  For example, if Distance is an
    // S1ChordAngle then you can specify max_distance.Successor().
    //
    // DEFAULT: Distance::Infinity()
    Distance max_distance() const;
    void set_max_distance(Distance max_distance);

    // Specifies that cells up to max_error() further away than the true
    // closest cells may be substituted in the result set, as long as such
    // cells satisfy all the remaining search criteria (such as max_distance).
    // This option only has an effect if max_results() is also specified;
    // otherwise all cells closer than max_distance() will always be returned.
    //
    // Note that this does not affect how the distance between cells is
    // computed; it simply gives the algorithm permission to stop the search
    // early as soon as the best possible improvement drops below max_error().
    //
    // This can be used to implement distance predicates efficiently.  For
    // example, to determine whether the minimum distance is less than D, the
    // IsDistanceLess() method sets max_results() == 1 and max_distance() ==
    // max_error() == D.  This causes the algorithm to terminate as soon as it
    // finds any cell whose distance is less than D, rather than continuing to
    // search for a cell that is even closer.
    //
    // DEFAULT: Distance::Delta::Zero()
    Delta max_error() const;
    void set_max_error(Delta max_error);

    // Specifies that cells must intersect the given S2Region.  "region" is
    // owned by the caller and must persist during the lifetime of this
    // object.  The value may be changed between calls to FindClosestPoints(),
    // or reset by calling set_region(nullptr).
    //
    // Note that if you want to set the region to a disc around a target
    // point, it is faster to use a PointTarget with set_max_distance()
    // instead.  You can also call both methods, e.g. to set a maximum
    // distance and also require that cells lie within a given rectangle.
    const S2Region* region() const;
    void set_region(const S2Region* region);

    // Specifies that distances should be computed by examining every cell
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

  // The Target class represents the geometry to which the distance is
  // measured.  For example, there can be subtypes for measuring the distance
  // to a point, an edge, or to an S2ShapeIndex (an arbitrary collection of
  // geometry).
  //
  // Implementations do *not* need to be thread-safe.  They may cache data or
  // allocate temporary data structures in order to improve performance.
  using Target = S2DistanceTarget<Distance>;

  // Each "Result" object represents a closest (cell_id, label) pair.
  class Result {
   public:
    // The default constructor yields an empty result, with a distance() of
    // Infinity() and invalid cell_id() and label() values.
    Result() : distance_(Distance::Infinity()), cell_id_(S2CellId::None()),
               label_(-1) {}

    // Constructs a Result object for the given (cell_id, label) pair.
    Result(Distance distance, S2CellId cell_id, Label label)
        : distance_(distance), cell_id_(cell_id), label_(label) {}

    // The distance from the target to this cell.
    Distance distance() const { return distance_; }

    // The cell itself.
    S2CellId cell_id() const { return cell_id_; }

    // The label associated with this S2CellId.
    Label label() const { return label_; }

    // Returns true if this Result object does not refer to any cell.
    // (The only case where an empty Result is returned is when the
    // FindClosestCell() method does not find any cells that meet the
    // specified criteria.)
    bool is_empty() const { return cell_id_ == S2CellId::None(); }

    // Returns true if two Result objects are identical.
    friend bool operator==(const Result& x, const Result& y) {
      return (x.distance_ == y.distance_ &&
              x.cell_id_ == y.cell_id_ &&
              x.label_ == y.label_);
    }

    // Compares two Result objects first by distance, then by cell_id and
    // finally by label.
    friend bool operator<(const Result& x, const Result& y) {
      if (x.distance_ < y.distance_) return true;
      if (y.distance_ < x.distance_) return false;
      if (x.cell_id_ < y.cell_id_) return true;
      if (y.cell_id_ < x.cell_id_) return false;
      return x.label_ < y.label_;
    }

    // Indicates that linear rather than binary search should be used when this
    // type is used as the key in gtl::btree data structures.
    using goog_btree_prefer_linear_node_search = std::true_type;

   private:
    Distance distance_;
    S2CellId cell_id_;
    Label label_;
  };

  // The minimum number of ranges that a cell must contain to enqueue it
  // rather than processing its contents immediately.
  static constexpr int kMinRangesToEnqueue = 6;

  // Default constructor; requires Init() to be called.
  S2ClosestCellQueryBase();
  ~S2ClosestCellQueryBase();

  // Convenience constructor that calls Init().
  explicit S2ClosestCellQueryBase(const S2CellIndex* index);

  // S2ClosestCellQueryBase is not copyable.
  S2ClosestCellQueryBase(const S2ClosestCellQueryBase&) = delete;
  void operator=(const S2ClosestCellQueryBase&) = delete;

  // Initializes the query.
  // REQUIRES: ReInit() must be called if "index" is modified.
  void Init(const S2CellIndex* index);

  // Reinitializes the query.  This method must be called whenever the
  // underlying index is modified.
  void ReInit();

  // Return a reference to the underlying S2CellIndex.
  const S2CellIndex& index() const;

  // Returns the closest (cell_id, label) pairs to the given target that
  // satisfy the given options.  This method may be called multiple times.
  std::vector<Result> FindClosestCells(Target* target, const Options& options);

  // This version can be more efficient when this method is called many times,
  // since it does not require allocating a new vector on each call.
  void FindClosestCells(Target* target, const Options& options,
                        std::vector<Result>* results);

  // Convenience method that returns exactly one (cell_id, label) pair.  If no
  // cells satisfy the given search criteria, then a Result with
  // distance() == Infinity() and is_empty() == true is returned.
  //
  // REQUIRES: options.max_results() == 1
  Result FindClosestCell(Target* target, const Options& options);

 private:
  using CellIterator = S2CellIndex::CellIterator;
  using ContentsIterator = S2CellIndex::ContentsIterator;
  using NonEmptyRangeIterator = S2CellIndex::NonEmptyRangeIterator;
  using RangeIterator = S2CellIndex::RangeIterator;

  const Options& options() const { return *options_; }
  void FindClosestCellsInternal(Target* target, const Options& options);
  void FindClosestCellsBruteForce();
  void FindClosestCellsOptimized();
  void InitQueue();
  void InitCovering();
  void AddInitialRange(S2CellId first_id, S2CellId last_id);
  void MaybeAddResult(S2CellId cell_id, Label label);
  bool ProcessOrEnqueue(S2CellId id, NonEmptyRangeIterator* iter, bool seek);
  void AddRange(const RangeIterator& range);

  const S2CellIndex* index_;
  const Options* options_;
  Target* target_;

  // True if max_error() must be subtracted from priority queue cell distances
  // in order to ensure that such distances are measured conservatively.  This
  // is true only if the target takes advantage of max_error() in order to
  // return faster results, and 0 < max_error() < distance_limit_.
  bool use_conservative_cell_distance_;

  // For the optimized algorithm we precompute the top-level S2CellIds that
  // will be added to the priority queue.  There can be at most 6 of these
  // cells.  Essentially this is just a covering of the indexed cells.
  std::vector<S2CellId> index_covering_;

  // The distance beyond which we can safely ignore further candidate cells.
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
  //  - If max_results() == kMaxMaxResults, results are appended to
  //    result_vector_ and sorted/uniqued at the end.
  //
  //  - Otherwise results are kept in a btree_set so that we can progressively
  //    reduce the distance limit once max_results() results have been found.
  //    (A priority queue is not sufficient because we need to be able to
  //    check whether a candidate cell is already in the result set.)
  //
  // TODO(ericv): Check whether it would be faster to use avoid_duplicates_
  // when result_set_ is used so that we could use a priority queue instead.
  Result result_singleton_;
  std::vector<Result> result_vector_;
  gtl::btree_set<Result> result_set_;

  // When the results are stored in a btree_set (see above), usually
  // duplicates can be removed simply by inserting candidate cells in the
  // current result set.  However this is not true if Options::max_error() > 0
  // and the Target subtype takes advantage of this by returning suboptimal
  // distances.  This is because when UpdateMinDistance() is called with
  // different "min_dist" parameters (i.e., the distance to beat), the
  // implementation may return a different distance for the same cell.  Since
  // the btree_set is keyed by (distance, cell_id, label) this can create
  // duplicate results.
  //
  // The flag below is true when duplicates must be avoided explicitly.  This
  // is achieved by maintaining a separate set keyed by (cell_id, label) only,
  // and checking whether each edge is in that set before computing the
  // distance to it.
  //
  // TODO(ericv): Check whether it is faster to avoid duplicates by default
  // (even when Options::max_results() == 1), rather than just when we need to.
  bool avoid_duplicates_;
  struct LabelledCellHash {
    size_t operator()(LabelledCell x) const {
      HashMix mix(x.cell_id.id());
      mix.Mix(x.label);
      return mix.get();
    }
  };
  gtl::dense_hash_set<LabelledCell, LabelledCellHash> tested_cells_;

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

  // Used to iterate over the contents of an S2CellIndex range.  It is defined
  // here to take advantage of the fact that when multiple ranges are visited
  // in increasing order, duplicates can automatically be eliminated.
  S2CellIndex::ContentsIterator contents_it_;

  // Temporaries, defined here to avoid multiple allocations / initializations.

  std::vector<S2CellId> max_distance_covering_;
  std::vector<S2CellId> intersection_with_max_distance_;
  const LabelledCell* tmp_range_data_[kMinRangesToEnqueue - 1];
};


//////////////////   Implementation details follow   ////////////////////


template <class Distance>
inline S2ClosestCellQueryBase<Distance>::Options::Options() {
}

template <class Distance>
inline int S2ClosestCellQueryBase<Distance>::Options::max_results() const {
  return max_results_;
}

template <class Distance>
inline void S2ClosestCellQueryBase<Distance>::Options::set_max_results(
    int max_results) {
  S2_DCHECK_GE(max_results, 1);
  max_results_ = max_results;
}

template <class Distance>
inline Distance S2ClosestCellQueryBase<Distance>::Options::max_distance()
    const {
  return max_distance_;
}

template <class Distance>
inline void S2ClosestCellQueryBase<Distance>::Options::set_max_distance(
    Distance max_distance) {
  max_distance_ = max_distance;
}

template <class Distance>
inline typename Distance::Delta
S2ClosestCellQueryBase<Distance>::Options::max_error() const {
  return max_error_;
}

template <class Distance>
inline void S2ClosestCellQueryBase<Distance>::Options::set_max_error(
    Delta max_error) {
  max_error_ = max_error;
}

template <class Distance>
inline const S2Region* S2ClosestCellQueryBase<Distance>::Options::region()
    const {
  return region_;
}

template <class Distance>
inline void S2ClosestCellQueryBase<Distance>::Options::set_region(
    const S2Region* region) {
  region_ = region;
}

template <class Distance>
inline bool S2ClosestCellQueryBase<Distance>::Options::use_brute_force() const {
  return use_brute_force_;
}

template <class Distance>
inline void S2ClosestCellQueryBase<Distance>::Options::set_use_brute_force(
    bool use_brute_force) {
  use_brute_force_ = use_brute_force;
}

template <class Distance>
S2ClosestCellQueryBase<Distance>::S2ClosestCellQueryBase()
    : tested_cells_(1) /* expected_max_elements*/ {
  tested_cells_.set_empty_key(LabelledCell(S2CellId::None(), -1));
}

template <class Distance>
S2ClosestCellQueryBase<Distance>::~S2ClosestCellQueryBase() {
  // Prevent inline destructor bloat by providing a definition.
}

template <class Distance>
inline S2ClosestCellQueryBase<Distance>::S2ClosestCellQueryBase(
    const S2CellIndex* index) : S2ClosestCellQueryBase() {
  Init(index);
}

template <class Distance>
void S2ClosestCellQueryBase<Distance>::Init(
    const S2CellIndex* index) {
  index_ = index;
  contents_it_.Init(index);
  ReInit();
}

template <class Distance>
void S2ClosestCellQueryBase<Distance>::ReInit() {
  index_covering_.clear();
}

template <class Distance>
inline const S2CellIndex&
S2ClosestCellQueryBase<Distance>::index() const {
  return *index_;
}

template <class Distance>
inline std::vector<typename S2ClosestCellQueryBase<Distance>::Result>
S2ClosestCellQueryBase<Distance>::FindClosestCells(
    Target* target, const Options& options) {
  std::vector<Result> results;
  FindClosestCells(target, options, &results);
  return results;
}

template <class Distance>
typename S2ClosestCellQueryBase<Distance>::Result
S2ClosestCellQueryBase<Distance>::FindClosestCell(
    Target* target, const Options& options) {
  S2_DCHECK_EQ(options.max_results(), 1);
  FindClosestCellsInternal(target, options);
  return result_singleton_;
}

template <class Distance>
void S2ClosestCellQueryBase<Distance>::FindClosestCells(
    Target* target, const Options& options, std::vector<Result>* results) {
  FindClosestCellsInternal(target, options);
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
    results->assign(result_set_.begin(), result_set_.end());
    result_set_.clear();
  }
}

template <class Distance>
void S2ClosestCellQueryBase<Distance>::FindClosestCellsInternal(
    Target* target, const Options& options) {
  target_ = target;
  options_ = &options;

  tested_cells_.clear();
  contents_it_.Clear();
  distance_limit_ = options.max_distance();
  result_singleton_ = Result();
  S2_DCHECK(result_vector_.empty());
  S2_DCHECK(result_set_.empty());
  S2_DCHECK_GE(target->max_brute_force_index_size(), 0);
  if (distance_limit_ == Distance::Zero()) return;

  if (options.max_results() == Options::kMaxMaxResults &&
      options.max_distance() == Distance::Infinity() &&
      options.region() == nullptr) {
    S2_LOG(WARNING) << "Returning all cells "
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
  // max_error() only affects the algorithm once at least max_edges() edges
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

  // Use the brute force algorithm if the index is small enough.
  if (options.use_brute_force() ||
      index_->num_cells() <= target_->max_brute_force_index_size()) {
    avoid_duplicates_ = false;
    FindClosestCellsBruteForce();
  } else {
    // If the target takes advantage of max_error() then we need to avoid
    // duplicate edges explicitly.  (Otherwise it happens automatically.)
    avoid_duplicates_ = (target_uses_max_error && options.max_results() > 1);
    FindClosestCellsOptimized();
  }
}

template <class Distance>
void S2ClosestCellQueryBase<Distance>::FindClosestCellsBruteForce() {
  for (CellIterator it(index_); !it.done(); it.Next()) {
    MaybeAddResult(it.cell_id(), it.label());
  }
}

template <class Distance>
void S2ClosestCellQueryBase<Distance>::FindClosestCellsOptimized() {
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
    // We already know that it has too many cells, so process its children.
    // Each child may either be processed directly or enqueued again.  The
    // loop is optimized so that we don't seek unnecessarily.
    bool seek = true;
    NonEmptyRangeIterator range(index_);
    for (int i = 0; i < 4; ++i, child = child.next()) {
      seek = ProcessOrEnqueue(child, &range, seek);
    }
  }
}

template <class Distance>
void S2ClosestCellQueryBase<Distance>::InitQueue() {
  S2_DCHECK(queue_.empty());

  // Optimization: rather than starting with the entire index, see if we can
  // limit the search region to a small disc.  Then we can find a covering for
  // that disc and intersect it with the covering for the index.  This can
  // save a lot of work when the search region is small.
  S2Cap cap = target_->GetCapBound();
  if (options().max_results() == 1) {
    // If the user is searching for just the closest cell, we can compute an
    // upper bound on search radius by seeking to the center of the target's
    // bounding cap and looking at the contents of that leaf cell range.  If
    // the range intersects any cells, then the distance is zero.  Otherwise
    // we can still look at the two neighboring ranges, and use the minimum
    // distance to any cell in those ranges as an upper bound on the search
    // radius.  These cells may wind up being processed twice, but in general
    // this is still faster.
    //
    // First check the range containing or immediately following "center".
    NonEmptyRangeIterator range(index_);
    S2CellId target(cap.center());
    range.Seek(target);
    AddRange(range);
    if (distance_limit_ == Distance::Zero()) return;

    // If the range immediately follows "center" (rather than containing it),
    // then check the previous non-empty range as well.
    if (range.start_id() > target && range.Prev()) {
      AddRange(range);
      if (distance_limit_ == Distance::Zero()) return;
    }
  }

  // We start with a covering of the set of indexed cells, then intersect it
  // with the maximum search radius disc (if any).
  //
  // Note that unlike S2ClosestPointQuery, we can't also intersect with the
  // given region (if any).  This is because the index cells in the result are
  // only required to intersect the region.  This means that an index cell that
  // intersects the region's covering may be much closer to the target than the
  // covering itself, which means that we cannot use the region's covering to
  // restrict the search.
  //
  // TODO(ericv): If this feature becomes important, this could be fixed by
  // (1) computing a covering of the region, (2) looking up any index cells
  // that contain each covering cell by seeking to covering_cell.range_min(),
  // (3) replacing each covering cell by the largest such cell (if any), and
  // (4) normalizing the result.
  if (index_covering_.empty()) InitCovering();
  const std::vector<S2CellId>* initial_cells = &index_covering_;
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
  NonEmptyRangeIterator range(index_);
  for (int i = 0; i < initial_cells->size(); ++i) {
    S2CellId id = (*initial_cells)[i];
    bool seek = (i == 0) || id.range_min() >= range.limit_id();
    ProcessOrEnqueue(id, &range, seek);
    if (range.done()) break;
  }
}

template <class Distance>
void S2ClosestCellQueryBase<Distance>::InitCovering() {
  // Compute the "index covering", which is a small number of S2CellIds that
  // cover the indexed cells.  There are two cases:
  //
  //  - If the index spans more than one face, then there is one covering cell
  // per spanned face, just big enough to cover the indexed cells on that face.
  //
  //  - If the index spans only one face, then we find the smallest cell "C"
  // that covers the indexed cells on that face (just like the case above).
  // Then for each of the 4 children of "C", if the child contains any index
  // cells then we create a covering cell that is big enough to just fit
  // those indexed cells (i.e., shrinking the child as much as possible to fit
  // its contents).  This essentially replicates what would happen if we
  // started with "C" as the covering cell, since "C" would immediately be
  // split, except that we take the time to prune the children further since
  // this will save work on every subsequent query.
  index_covering_.reserve(6);
  NonEmptyRangeIterator it(index_), last(index_);
  it.Begin();
  last.Finish();
  if (!last.Prev()) return;  // Empty index.
  S2CellId index_last_id = last.limit_id().prev();
  if (it.start_id() != last.start_id()) {
    // The index contains at least two distinct S2CellIds (because otherwise
    // there would only be one non-empty range).  Choose a level such that the
    // entire index can be spanned with at most 6 cells (if the index spans
    // multiple faces) or 4 cells (it the index spans a single face).
    int level = it.start_id().GetCommonAncestorLevel(index_last_id) + 1;

    // Visit each potential covering cell except the last (handled below).
    S2CellId start_id = it.start_id().parent(level);
    S2CellId last_id = index_last_id.parent(level);
    for (S2CellId id = start_id; id != last_id; id = id.next()) {
      // Skip any covering cells that don't contain an indexed range.
      if (id.range_max() < it.start_id()) continue;

      // Find the indexed range contained by this covering cell and then
      // shrink the cell if necessary so that it just covers this range.
      S2CellId cell_first_id = it.start_id();
      it.Seek(id.range_max().next());
      // Find the last leaf cell covered by the previous non-empty range.
      last = it;
      last.Prev();
      AddInitialRange(cell_first_id, last.limit_id().prev());
    }
  }
  AddInitialRange(it.start_id(), index_last_id);
}

// Adds a cell to index_covering_ that covers the given inclusive range.
//
// REQUIRES: "first" and "last" have a common ancestor.
template <class Distance>
void S2ClosestCellQueryBase<Distance>::AddInitialRange(
    S2CellId first_id, S2CellId last_id) {
  // Add the lowest common ancestor of the given range.
  int level = first_id.GetCommonAncestorLevel(last_id);
  S2_DCHECK_GE(level, 0);
  index_covering_.push_back(first_id.parent(level));
}

// TODO(ericv): Consider having this method return false when distance_limit_
// is reduced to zero, and terminating any calling loops early.
template <class Distance>
void S2ClosestCellQueryBase<Distance>::MaybeAddResult(S2CellId cell_id,
                                                      Label label) {
  if (avoid_duplicates_ &&
      !tested_cells_.insert(LabelledCell(cell_id, label)).second) {
    return;
  }

  // TODO(ericv): It may be relatively common to add the same S2CellId
  // multiple times with different labels.  This could be optimized by
  // remembering the last "cell_id" argument and its distance.  However this
  // may not be beneficial when Options::max_results() == 1, for example.
  S2Cell cell(cell_id);
  Distance distance = distance_limit_;
  if (!target_->UpdateMinDistance(cell, &distance)) return;

  const S2Region* region = options().region();
  if (region && !region->MayIntersect(cell)) return;

  Result result(distance, cell_id, label);
  if (options().max_results() == 1) {
    // Optimization for the common case where only the closest cell is wanted.
    result_singleton_ = result;
    distance_limit_ = result.distance() - options().max_error();
  } else if (options().max_results() == Options::kMaxMaxResults) {
    result_vector_.push_back(result);  // Sort/unique at end.
  } else {
    // Add this cell to result_set_.  Note that even if we already have enough
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

// Either process the contents of the given cell immediately, or add it to the
// queue to be subdivided.  If "seek" is false, then "iter" must be positioned
// at the first non-empty range (if any) with start_id() >= id.range_min().
//
// Returns "true" if the cell was added to the queue, and "false" if it was
// processed immediately, in which case "iter" is positioned at the first
// non-empty range (if any) with start_id() > id.range_max().
template <class Distance>
bool S2ClosestCellQueryBase<Distance>::ProcessOrEnqueue(
    S2CellId id, NonEmptyRangeIterator* iter, bool seek) {
  if (seek) iter->Seek(id.range_min());
  S2CellId last = id.range_max();
  if (iter->start_id() > last) {
    return false;  // No need to seek to next child.
  }
  // If this cell intersects at least "kMinRangesToEnqueue" leaf cell ranges
  // (including ranges whose contents are empty), then enqueue it.  We test
  // this by advancing (n - 1) ranges and checking whether that range also
  // intersects this cell.
  RangeIterator max_it = *iter;
  if (max_it.Advance(kMinRangesToEnqueue - 1) && max_it.start_id() <= last) {
    // This cell intersects at least kMinRangesToEnqueue ranges, so enqueue it.
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
  // There were few enough ranges that we might as well process them now.
  for (; iter->start_id() <= last; iter->Next()) {
    AddRange(*iter);
  }
  return false;  // No need to seek to next child.
}

template <class Distance>
void S2ClosestCellQueryBase<Distance>::AddRange(const RangeIterator& range) {
  for (contents_it_.StartUnion(range);
       !contents_it_.done(); contents_it_.Next()) {
    MaybeAddResult(contents_it_.cell_id(), contents_it_.label());
  }
}

#endif  // S2_S2CLOSEST_CELL_QUERY_BASE_H_
