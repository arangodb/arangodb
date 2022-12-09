// Copyright 2012 Google Inc. All Rights Reserved.
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

#include "s2/mutable_s2shape_index.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <utility>

#include "s2/base/casts.h"
#include "s2/base/commandlineflags.h"
#include "s2/base/spinlock.h"
#include "absl/base/attributes.h"
#include "absl/flags/flag.h"
#include "absl/utility/utility.h"
#include "s2/encoded_s2cell_id_vector.h"
#include "s2/encoded_string_vector.h"
#include "s2/r1interval.h"
#include "s2/r2.h"
#include "s2/r2rect.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"
#include "s2/s2coords.h"
#include "s2/s2edge_clipping.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2metrics.h"
#include "s2/s2padded_cell.h"
#include "s2/s2pointutil.h"
#include "s2/s2shapeutil_contains_brute_force.h"
#include "s2/util/math/mathutil.h"

using absl::make_unique;
using std::fabs;
using std::make_pair;
using std::max;
using std::min;
using std::unique_ptr;
using std::vector;

// FLAGS_s2shape_index_default_max_edges_per_cell
//
// The default maximum number of edges per cell (not counting 'long' edges).
// If a cell has more than this many edges, and it is not a leaf cell, then it
// is subdivided.  This flag can be overridden via MutableS2ShapeIndex::Options.
// Reasonable values range from 10 to about 50 or so.
S2_DEFINE_int32(
    s2shape_index_default_max_edges_per_cell, 10,
    "Default maximum number of edges per cell (not counting 'long' edges); "
    "reasonable values range from 10 to 50.  Small values makes queries "
    "faster, while large values make construction faster and use less memory.");

// FLAGS_s2shape_index_tmp_memory_budget
//
// Attempt to limit the amount of temporary memory allocated while building or
// updating a MutableS2ShapeIndex to at most this value.  This is achieved by
// splitting the updates into multiple batches when necessary.  (The memory
// required is proportional to the number of edges being updated at once.)
//
// Note that this limit is not a hard guarantee, for several reasons:
//  (1) the memory estimates are only approximations;
//  (2) all edges in a given shape are added or removed at once, so shapes
//      with huge numbers of edges may exceed the budget;
//  (3) shapes being removed are always processed in a single batch.  (This
//      could be fixed, but it seems better to keep the code simpler for now.)
S2_DEFINE_int64(
    s2shape_index_tmp_memory_budget, int64{100} << 20 /*100 MB*/,
    "Attempts to limit the amount of temporary memory used by "
    "MutableS2ShapeIndex when creating or updating very large indexes to at "
    "most this number of bytes.  If more memory than this is needed, updates "
    "will automatically be split into batches internally.");

// FLAGS_s2shape_index_cell_size_to_long_edge_ratio
//
// The maximum cell size, relative to an edge's length, for which that edge is
// considered 'long'.  Cell size is defined as the average edge length of all
// cells at a given level.  For example, a value of 2.0 means that an edge E
// is long at cell level k iff the average edge length at level k is at most
// twice the length of E.  Long edges edges are not counted towards the
// max_edges_per_cell() limit because such edges typically need to be
// propagated to several children, which increases time and memory costs
// without commensurate benefits.
S2_DEFINE_double(
    s2shape_index_cell_size_to_long_edge_ratio, 1.0,
    "The maximum cell size, relative to an edge's length, for which that "
    "edge is considered 'long'.  Long edges are not counted towards the "
    "max_edges_per_cell() limit.  The size and speed of the index are "
    "typically not very sensitive to this parameter.  Reasonable values range "
    "from 0.1 to 10, with smaller values causing more aggressive subdivision "
    "of long edges grouped closely together.");

// FLAGS_s2shape_index_min_short_edge_fraction
//
// The minimum fraction of 'short' edges that must be present in a cell in
// order for it to be subdivided.  If this parameter is non-zero then the
// total index size and construction time are guaranteed to be linear in the
// number of input edges; this prevents the worst-case quadratic space and
// time usage that can otherwise occur with certain input configurations.
// Specifically, the maximum index size is
//
//     O((c1 + c2 * (1 - f) / f) * n)
//
// where n is the number of input edges, f is this parameter value, and
// constant c2 is roughly 20 times larger than constant c1.  (The exact values
// of c1 and c2 depend on the cell_size_to_long_edge_ratio and
// max_edges_per_cell parameters and certain properties of the input geometry
// such as whether it consists of O(1) shapes, whether it includes polygons,
// and whether the polygon interiors are disjoint.)
//
// Reasonable parameter values range from 0.1 up to perhaps 0.95.  The main
// factors to consider when choosing this parameter are:
//
//  - For pathological geometry, larger values result in indexes that are
//    smaller and faster to construct but have worse query performance (due to
//    having more edges per cell).  However note that even a setting of 0.1
//    reduces the worst case by 100x compared with a setting of 0.001.
//
//  - For normal geometry, values up to about 0.8 result in indexes that are
//    virtually unchanged except for a slight increase in index construction
//    time (proportional to the parameter value f) for very large inputs.
//    With millions of edges, indexing time increases by about (15% * f),
//    e.g. a parameter value of 0.5 slows down indexing for very large inputs
//    by about 7.5%.  (Indexing time for small inputs is not affected.)
//
//  - Values larger than about 0.8 start to affect index construction even for
//    normal geometry, resulting in smaller indexes and faster construction
//    times but gradually worse query performance.
//
// Essentially this parameter provides control over a space-time tradeoff that
// largely affects only pathological geometry.  The default value of 0.2 was
// chosen to make index construction as fast as possible while still
// protecting against possible quadratic space usage.
S2_DEFINE_double(
    s2shape_index_min_short_edge_fraction, 0.2,
    "The minimum fraction of 'short' edges that must be present in a cell in "
    "order for it to be subdivided.  If this parameter is non-zero then the "
    "total index size and construction time are guaranteed to be linear in the "
    "number of input edges, where the constant of proportionality has the "
    "form (c1 + c2 * (1 - f) / f).  Reasonable values range from 0.1 to "
    "perhaps 0.95.  Values up to about 0.8 have almost no effect on 'normal' "
    "geometry except for a small increase in index construction time "
    "(proportional to f) for very large inputs.  For worst-case geometry, "
    "larger parameter values result in indexes that are smaller and faster "
    "to construct but have worse query performance (due to having more edges "
    "per cell).  Essentially this parameter provides control over a space-time "
    "tradeoff that largely affects only pathological geometry.");

// The total error when clipping an edge comes from two sources:
// (1) Clipping the original spherical edge to a cube face (the "face edge").
//     The maximum error in this step is S2::kFaceClipErrorUVCoord.
// (2) Clipping the face edge to the u- or v-coordinate of a cell boundary.
//     The maximum error in this step is S2::kEdgeClipErrorUVCoord.
// Finally, since we encounter the same errors when clipping query edges, we
// double the total error so that we only need to pad edges during indexing
// and not at query time.
const double MutableS2ShapeIndex::kCellPadding =
    2 * (S2::kFaceClipErrorUVCoord + S2::kEdgeClipErrorUVCoord);

MutableS2ShapeIndex::Options::Options()
    : max_edges_per_cell_(
          absl::GetFlag(FLAGS_s2shape_index_default_max_edges_per_cell)) {}

void MutableS2ShapeIndex::Options::set_max_edges_per_cell(
    int max_edges_per_cell) {
  max_edges_per_cell_ = max_edges_per_cell;
}

bool MutableS2ShapeIndex::Iterator::Locate(const S2Point& target) {
  return LocateImpl(target, this);
}

MutableS2ShapeIndex::CellRelation MutableS2ShapeIndex::Iterator::Locate(
    S2CellId target) {
  return LocateImpl(target, this);
}

const S2ShapeIndexCell* MutableS2ShapeIndex::Iterator::GetCell() const {
  S2_LOG(DFATAL) << "Should never be called";
  return nullptr;
}

unique_ptr<MutableS2ShapeIndex::IteratorBase>
MutableS2ShapeIndex::Iterator::Clone() const {
  return make_unique<Iterator>(*this);
}

void MutableS2ShapeIndex::Iterator::Copy(const IteratorBase& other)  {
  *this = *down_cast<const Iterator*>(&other);
}

// FaceEdge and ClippedEdge store temporary edge data while the index is being
// updated.  FaceEdge represents an edge that has been projected onto a given
// face, while ClippedEdge represents the portion of that edge that has been
// clipped to a given S2Cell.
//
// While it would be possible to combine all the edge information into one
// structure, there are two good reasons for separating it:
//
//  - Memory usage.  Separating the two classes means that we only need to
//    store one copy of the per-face data no matter how many times an edge is
//    subdivided, and it also lets us delay computing bounding boxes until
//    they are needed for processing each face (when the dataset spans
//    multiple faces).
//
//  - Performance.  UpdateEdges is significantly faster on large polygons when
//    the data is separated, because it often only needs to access the data in
//    ClippedEdge and this data is cached more successfully.

struct MutableS2ShapeIndex::FaceEdge {
  int32 shape_id;      // The shape that this edge belongs to
  int32 edge_id;       // Edge id within that shape
  int32 max_level;     // Not desirable to subdivide this edge beyond this level
  bool has_interior;   // Belongs to a shape of dimension 2.
  R2Point a, b;        // The edge endpoints, clipped to a given face
  S2Shape::Edge edge;  // The edge endpoints
};

struct MutableS2ShapeIndex::ClippedEdge {
  const FaceEdge* face_edge;  // The original unclipped edge
  R2Rect bound;               // Bounding box for the clipped portion
};

// Given a set of shapes, InteriorTracker keeps track of which shapes contain
// a particular point (the "focus").  It provides an efficient way to move the
// focus from one point to another and incrementally update the set of shapes
// which contain it.  We use this to compute which shapes contain the center
// of every S2CellId in the index, by advancing the focus from one cell center
// to the next.
//
// Initially the focus is at the start of the S2CellId space-filling curve.
// We then visit all the cells that are being added to the MutableS2ShapeIndex
// in increasing order of S2CellId.  For each cell, we draw two edges: one
// from the entry vertex to the center, and another from the center to the
// exit vertex (where "entry" and "exit" refer to the points where the
// space-filling curve enters and exits the cell).  By counting edge crossings
// we can incrementally compute which shapes contain the cell center.  Note
// that the same set of shapes will always contain the exit point of one cell
// and the entry point of the next cell in the index, because either (a) these
// two points are actually the same, or (b) the intervening cells in S2CellId
// order are all empty, and therefore there are no edge crossings if we follow
// this path from one cell to the other.
class MutableS2ShapeIndex::InteriorTracker {
 public:
  // Constructs the InteriorTracker.  You must call AddShape() for each shape
  // that will be tracked before calling MoveTo() or DrawTo().
  InteriorTracker();

  // Returns the initial focus point when the InteriorTracker is constructed
  // (corresponding to the start of the S2CellId space-filling curve).
  static S2Point Origin();

  // Returns the current focus point (see above).
  const S2Point& focus() { return b_; }

  // Returns true if any shapes are being tracked.
  bool is_active() const { return is_active_; }

  // Adds a shape whose interior should be tracked.  "contains_focus" indicates
  // whether the current focus point is inside the shape.  Alternatively, if the
  // focus point is in the process of being moved (via MoveTo/DrawTo), you can
  // also specify "contains_focus" at the old focus point and call TestEdge()
  // for every edge of the shape that might cross the current DrawTo() line.
  // This updates the state to correspond to the new focus point.
  //
  // REQUIRES: shape->dimension() == 2
  void AddShape(int32 shape_id, bool contains_focus);

  // Moves the focus to the given point.  This method should only be used when
  // it is known that there are no edge crossings between the old and new
  // focus locations; otherwise use DrawTo().
  void MoveTo(const S2Point& b) { b_ = b; }

  // Moves the focus to the given point.  After this method is called,
  // TestEdge() should be called with all edges that may cross the line
  // segment between the old and new focus locations.
  void DrawTo(const S2Point& b);

  // Indicates that the given edge of the given shape may cross the line
  // segment between the old and new focus locations (see DrawTo).
  // REQUIRES: shape->dimension() == 2
  inline void TestEdge(int32 shape_id, const S2Shape::Edge& edge);

  // The set of shape ids that contain the current focus.
  const ShapeIdSet& shape_ids() const { return shape_ids_; }

  // Indicates that the last argument to MoveTo() or DrawTo() was the entry
  // vertex of the given S2CellId, i.e. the tracker is positioned at the start
  // of this cell.  By using this method together with at_cellid(), the caller
  // can avoid calling MoveTo() in cases where the exit vertex of the previous
  // cell is the same as the entry vertex of the current cell.
  void set_next_cellid(S2CellId next_cellid) {
    next_cellid_ = next_cellid.range_min();
  }

  // Returns true if the focus is already at the entry vertex of the given
  // S2CellId (provided that the caller calls set_next_cellid() as each cell
  // is processed).
  bool at_cellid(S2CellId cellid) const {
    return cellid.range_min() == next_cellid_;
  }

  // Makes an internal copy of the state for shape ids below the given limit,
  // and then clear the state for those shapes.  This is used during
  // incremental updates to track the state of added and removed shapes
  // separately.
  void SaveAndClearStateBefore(int32 limit_shape_id);

  // Restores the state previously saved by SaveAndClearStateBefore().  This
  // only affects the state for shape_ids below "limit_shape_id".
  void RestoreStateBefore(int32 limit_shape_id);

  // Indicates that only some edges of the given shape are being added, and
  // therefore its interior should not be processed yet.
  int partial_shape_id() const { return partial_shape_id_; }
  void set_partial_shape_id(int shape_id) { partial_shape_id_ = shape_id; }

 private:
  // Removes "shape_id" from shape_ids_ if it exists, otherwise insert it.
  void ToggleShape(int shape_id);

  // Returns a pointer to the first entry "x" where x >= shape_id.
  ShapeIdSet::iterator lower_bound(int32 shape_id);

  bool is_active_ = false;
  S2Point a_, b_;
  S2CellId next_cellid_;
  S2EdgeCrosser crosser_;
  ShapeIdSet shape_ids_;

  // Shape ids saved by SaveAndClearStateBefore().  The state is never saved
  // recursively so we don't need to worry about maintaining a stack.
  ShapeIdSet saved_ids_;

  // As an optimization, we also save is_active_ so that RestoreStateBefore()
  // can deactivate the tracker again in the case where the shapes being added
  // and removed do not have an interior, but some existing shapes do.
  bool saved_is_active_;

  // If non-negative, indicates that only some edges of the given shape are
  // being added and therefore its interior should not be tracked yet.
  int partial_shape_id_ = -1;
};

// As shapes are added, we compute which ones contain the start of the
// S2CellId space-filling curve by drawing an edge from S2::Origin() to this
// point and counting how many shape edges cross this edge.
MutableS2ShapeIndex::InteriorTracker::InteriorTracker()
    : b_(Origin()), next_cellid_(S2CellId::Begin(S2CellId::kMaxLevel)) {
}

S2Point MutableS2ShapeIndex::InteriorTracker::Origin() {
  // The start of the S2CellId space-filling curve.
  return S2::FaceUVtoXYZ(0, -1, -1).Normalize();
}

void MutableS2ShapeIndex::InteriorTracker::AddShape(int32 shape_id,
                                                    bool contains_focus) {
  is_active_ = true;
  if (contains_focus) {
    ToggleShape(shape_id);
  }
}

void MutableS2ShapeIndex::InteriorTracker::ToggleShape(int shape_id) {
  // Since shape_ids_.size() is typically *very* small (0, 1, or 2), it turns
  // out to be significantly faster to maintain a sorted array rather than
  // using an STL set or btree_set.
  if (shape_ids_.empty()) {
    shape_ids_.push_back(shape_id);
  } else if (shape_ids_[0] == shape_id) {
    shape_ids_.erase(shape_ids_.begin());
  } else {
    ShapeIdSet::iterator pos = shape_ids_.begin();
    while (*pos < shape_id) {
      if (++pos == shape_ids_.end()) {
        shape_ids_.push_back(shape_id);
        return;
      }
    }
    if (*pos == shape_id) {
      shape_ids_.erase(pos);
    } else {
      shape_ids_.insert(pos, shape_id);
    }
  }
}

void MutableS2ShapeIndex::InteriorTracker::DrawTo(const S2Point& b) {
  a_ = b_;
  b_ = b;
  crosser_.Init(&a_, &b_);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE  // ~1% faster
inline void MutableS2ShapeIndex::InteriorTracker::TestEdge(
    int32 shape_id, const S2Shape::Edge& edge) {
  if (crosser_.EdgeOrVertexCrossing(&edge.v0, &edge.v1)) {
    ToggleShape(shape_id);
  }
}

// Like std::lower_bound(shape_ids_.begin(), shape_ids_.end(), shape_id), but
// implemented with linear rather than binary search because the number of
// shapes being tracked is typically very small.
inline MutableS2ShapeIndex::ShapeIdSet::iterator
MutableS2ShapeIndex::InteriorTracker::lower_bound(int32 shape_id) {
  ShapeIdSet::iterator pos = shape_ids_.begin();
  while (pos != shape_ids_.end() && *pos < shape_id) { ++pos; }
  return pos;
}

void MutableS2ShapeIndex::InteriorTracker::SaveAndClearStateBefore(
    int32 limit_shape_id) {
  S2_DCHECK(saved_ids_.empty());
  ShapeIdSet::iterator limit = lower_bound(limit_shape_id);
  saved_ids_.assign(shape_ids_.begin(), limit);
  shape_ids_.erase(shape_ids_.begin(), limit);
  saved_is_active_ = is_active_;
}

void MutableS2ShapeIndex::InteriorTracker::RestoreStateBefore(
    int32 limit_shape_id) {
  shape_ids_.erase(shape_ids_.begin(), lower_bound(limit_shape_id));
  shape_ids_.insert(shape_ids_.begin(), saved_ids_.begin(), saved_ids_.end());
  saved_ids_.clear();
  is_active_ = saved_is_active_;
}

MutableS2ShapeIndex::MutableS2ShapeIndex() {
}

MutableS2ShapeIndex::MutableS2ShapeIndex(const Options& options) {
  Init(options);
}

MutableS2ShapeIndex::MutableS2ShapeIndex(MutableS2ShapeIndex&& b)
    : S2ShapeIndex(std::move(b)),
      shapes_(std::move(b.shapes_)),
      cell_map_(std::move(b.cell_map_)),
      options_(std::move(b.options_)),
      pending_additions_begin_(absl::exchange(b.pending_additions_begin_, 0)),
      pending_removals_(std::move(b.pending_removals_)),
      index_status_(b.index_status_.exchange(FRESH, std::memory_order_relaxed)),
      mem_tracker_(std::move(b.mem_tracker_)) {}

MutableS2ShapeIndex& MutableS2ShapeIndex::operator=(MutableS2ShapeIndex&& b) {
  // We need to delegate to our parent move-assignment operator since we can't
  // move any of its private state.  This is a little odd since b is in a
  // half-moved state after calling but is ultimately safe.
  S2ShapeIndex::operator=(static_cast<S2ShapeIndex&&>(b));
  shapes_ = std::move(b.shapes_);
  cell_map_ = std::move(b.cell_map_);
  options_ = std::move(b.options_);
  pending_additions_begin_ = absl::exchange(b.pending_additions_begin_, 0);
  pending_removals_ = std::move(b.pending_removals_);
  index_status_.store(
      b.index_status_.exchange(FRESH, std::memory_order_relaxed),
      std::memory_order_relaxed);
  mem_tracker_ = std::move(b.mem_tracker_);
  return *this;
}

void MutableS2ShapeIndex::Init(const Options& options) {
  S2_DCHECK(shapes_.empty());
  options_ = options;
  // Memory tracking is not affected by this method.
}

MutableS2ShapeIndex::~MutableS2ShapeIndex() {
  Clear();
}

void MutableS2ShapeIndex::set_memory_tracker(S2MemoryTracker* tracker) {
  mem_tracker_.Tally(-mem_tracker_.client_usage_bytes());
  mem_tracker_.Init(tracker);
  if (mem_tracker_.is_active()) mem_tracker_.Tally(SpaceUsed());
}

// Called to set the index status when the index needs to be rebuilt.
void MutableS2ShapeIndex::MarkIndexStale() {
  // The UPDATING status can only be changed in ApplyUpdatesThreadSafe().
  if (index_status_.load(std::memory_order_relaxed) == UPDATING) return;

  // If a memory tracking error has occurred we set the index status to FRESH
  // in order to prevent us from attempting to rebuild it.
  IndexStatus status = (shapes_.empty() || !mem_tracker_.ok()) ? FRESH : STALE;
  index_status_.store(status, std::memory_order_relaxed);
}

void MutableS2ShapeIndex::Minimize() {
  mem_tracker_.Tally(-mem_tracker_.client_usage_bytes());
  Iterator it;
  for (it.InitStale(this, S2ShapeIndex::BEGIN); !it.done(); it.Next()) {
    delete &it.cell();
  }
  cell_map_.clear();
  pending_removals_.reset();
  pending_additions_begin_ = 0;
  MarkIndexStale();
  if (mem_tracker_.is_active()) mem_tracker_.Tally(SpaceUsed());
}

int MutableS2ShapeIndex::Add(unique_ptr<S2Shape> shape) {
  // Additions are processed lazily by ApplyUpdates().  Note that in order to
  // avoid unexpected client behavior, this method continues to add shapes
  // even once the specified S2MemoryTracker limit has been exceeded.
  const int id = shapes_.size();
  shape->id_ = id;
  mem_tracker_.AddSpace(&shapes_, 1);
  shapes_.push_back(std::move(shape));
  MarkIndexStale();
  return id;
}

unique_ptr<S2Shape> MutableS2ShapeIndex::Release(int shape_id) {
  // This class updates itself lazily, because it is much more efficient to
  // process additions and removals in batches.  However this means that when
  // a shape is removed we need to make a copy of all its edges, since the
  // client is free to delete "shape" once this call is finished.

  S2_DCHECK(shapes_[shape_id] != nullptr);
  auto shape = std::move(shapes_[shape_id]);
  if (shape_id >= pending_additions_begin_) {
    // We are removing a shape that has not yet been added to the index,
    // so there is nothing else to do.
  } else {
    if (!pending_removals_) {
      if (!mem_tracker_.Tally(sizeof(*pending_removals_))) {
        Minimize();
        return shape;
      }
      pending_removals_ = make_unique<vector<RemovedShape>>();
    }
    RemovedShape removed;
    removed.shape_id = shape->id();
    removed.has_interior = (shape->dimension() == 2);
    removed.contains_tracker_origin =
        s2shapeutil::ContainsBruteForce(*shape, InteriorTracker::Origin());
    int num_edges = shape->num_edges();
    if (!mem_tracker_.AddSpace(&removed.edges, num_edges) ||
        !mem_tracker_.AddSpace(pending_removals_.get(), 1)) {
      Minimize();
      return shape;
    }
    for (int e = 0; e < num_edges; ++e) {
      removed.edges.push_back(shape->edge(e));
    }
    pending_removals_->push_back(std::move(removed));
  }
  MarkIndexStale();
  return shape;
}

vector<unique_ptr<S2Shape>> MutableS2ShapeIndex::ReleaseAll() {
  S2_DCHECK(update_state_ == nullptr);
  vector<unique_ptr<S2Shape>> result;
  result.swap(shapes_);
  Minimize();
  return result;
}

void MutableS2ShapeIndex::Clear() {
  ReleaseAll();
}

// Apply any pending updates in a thread-safe way.
void MutableS2ShapeIndex::ApplyUpdatesThreadSafe() {
  lock_.Lock();
  if (index_status_.load(std::memory_order_relaxed) == FRESH) {
    lock_.Unlock();
  } else if (index_status_.load(std::memory_order_relaxed) == UPDATING) {
    // Wait until the updating thread is finished.  We do this by attempting
    // to lock a mutex that is held by the updating thread.  When this mutex
    // is unlocked the index_status_ is guaranteed to be FRESH.
    ++update_state_->num_waiting;
    lock_.Unlock();
    update_state_->wait_mutex.Lock();
    lock_.Lock();
    --update_state_->num_waiting;
    UnlockAndSignal();  // Notify other waiting threads.
  } else {
    S2_DCHECK_EQ(STALE, index_status_);
    index_status_.store(UPDATING, std::memory_order_relaxed);
    // Allocate the extra state needed for thread synchronization.  We keep
    // the spinlock held while doing this, because (1) memory allocation is
    // fast, so the chance of a context switch while holding the lock is low;
    // (2) by far the most common situation is that there is no contention,
    // and this saves an extra lock and unlock step; (3) even in the rare case
    // where there is contention, the main side effect is that some other
    // thread will burn a few CPU cycles rather than sleeping.
    update_state_ = make_unique<UpdateState>();
    // lock_.Lock wait_mutex *before* calling Unlock() to ensure that all other
    // threads will block on it.
    update_state_->wait_mutex.Lock();
    // Release the spinlock before doing any real work.
    lock_.Unlock();
    ApplyUpdatesInternal();
    lock_.Lock();
    // index_status_ can be updated to FRESH only while locked *and* using
    // an atomic store operation, so that MaybeApplyUpdates() can check
    // whether the index is FRESH without acquiring the spinlock.
    index_status_.store(FRESH, std::memory_order_release);
    UnlockAndSignal();  // Notify any waiting threads.
  }
}

// Releases lock_ and wakes up any waiting threads by releasing wait_mutex.
// If this was the last waiting thread, also deletes update_state_.
// REQUIRES: lock_ is held.
// REQUIRES: wait_mutex is held.
inline void MutableS2ShapeIndex::UnlockAndSignal() {
  S2_DCHECK_EQ(FRESH, index_status_);
  int num_waiting = update_state_->num_waiting;
  lock_.Unlock();
  // Allow another waiting thread to proceed.  Note that no new threads can
  // start waiting because the index_status_ is now FRESH, and the caller is
  // required to prevent any new mutations from occurring while these const
  // methods are running.
  //
  // We need to unlock wait_mutex before destroying it even if there are no
  // waiting threads.
  update_state_->wait_mutex.Unlock();
  if (num_waiting == 0) {
    update_state_.reset();
  }
}

// This method updates the index by applying all pending additions and
// removals.  It does *not* update index_status_ (see ApplyUpdatesThreadSafe).
void MutableS2ShapeIndex::ApplyUpdatesInternal() {
  // Check whether we have so many edges to process that we should process
  // them in multiple batches to save memory.  Building the index can use up
  // to 20x as much memory (per edge) as the final index size.
  vector<BatchDescriptor> batches = GetUpdateBatches();
  for (const BatchDescriptor& batch : batches) {
    if (mem_tracker_.is_active()) {
      S2_DCHECK_EQ(mem_tracker_.client_usage_bytes(), SpaceUsed());  // Invariant.
    }
    vector<FaceEdge> all_edges[6];
    ReserveSpace(batch, all_edges);
    if (!mem_tracker_.ok()) return Minimize();

    InteriorTracker tracker;
    if (pending_removals_) {
      // The first batch implicitly includes all shapes being removed.
      for (const auto& pending_removal : *pending_removals_) {
        RemoveShape(pending_removal, all_edges, &tracker);
      }
      pending_removals_.reset(nullptr);
    }
    // A batch consists of zero or more full shapes followed by zero or one
    // partial shapes.  The loop below handles all such cases.
    for (auto begin = batch.begin; begin < batch.end;
         ++begin.shape_id, begin.edge_id = 0) {
      const S2Shape* shape = this->shape(begin.shape_id);
      if (shape == nullptr) continue;  // Already removed.
      int edges_end = begin.shape_id == batch.end.shape_id ? batch.end.edge_id
                                                           : shape->num_edges();
      AddShape(shape, begin.edge_id, edges_end, all_edges, &tracker);
    }
    for (int face = 0; face < 6; ++face) {
      UpdateFaceEdges(face, all_edges[face], &tracker);
      // Save memory by clearing vectors after we are done with them.
      vector<FaceEdge>().swap(all_edges[face]);
    }
    pending_additions_begin_ = batch.end.shape_id;
    if (batch.begin.edge_id > 0 && batch.end.edge_id == 0) {
      // We have just finished adding the edges of shape that was split over
      // multiple batches.  Now we need to mark the interior of the shape, if
      // any, by setting contains_center() on the appropriate index cells.
      FinishPartialShape(tracker.partial_shape_id());
    }
    if (mem_tracker_.is_active()) {
      mem_tracker_.Tally(-mem_tracker_.client_usage_bytes());
      if (!mem_tracker_.Tally(SpaceUsed())) return Minimize();
    }
  }
  // It is the caller's responsibility to update index_status_.
}

// Count the number of edges being updated, and break them into several
// batches if necessary to reduce the amount of memory needed.  (See the
// documentation for FLAGS_s2shape_index_tmp_memory_budget.)
vector<MutableS2ShapeIndex::BatchDescriptor>
MutableS2ShapeIndex::GetUpdateBatches() const {
  // Count the edges being removed and added.
  int num_edges_removed = 0;
  if (pending_removals_) {
    for (const auto& pending_removal : *pending_removals_) {
      num_edges_removed += pending_removal.edges.size();
    }
  }
  int num_edges_added = 0;
  for (int id = pending_additions_begin_; id < shapes_.size(); ++id) {
    const S2Shape* shape = this->shape(id);
    if (shape) num_edges_added += shape->num_edges();
  }
  BatchGenerator batch_gen(num_edges_removed, num_edges_added,
                           pending_additions_begin_);
  for (int id = pending_additions_begin_; id < shapes_.size(); ++id) {
    const S2Shape* shape = this->shape(id);
    if (shape) batch_gen.AddShape(id, shape->num_edges());
  }
  return batch_gen.Finish();
}

// The following memory estimates are based on heap profiling.

// The batch sizes during a given update gradually decrease as the space
// occupied by the index itself grows.  In order to do this, we need a
// conserative lower bound on how much the index grows per edge.
//
// The final size of a MutableS2ShapeIndex depends mainly on how finely the
// index is subdivided, as controlled by Options::max_edges_per_cell() and
// --s2shape_index_default_max_edges_per_cell.  For realistic values of
// max_edges_per_cell() and shapes with moderate numbers of edges, it is
// difficult to get much below 8 bytes per edge.  *The minimum possible size
// is 4 bytes per edge (to store a 32-bit edge id in an S2ClippedShape) plus
// 24 bytes per shape (for the S2ClippedShape itself plus a pointer in the
// shapes_ vector.)  Note that this value is a lower bound; a typical final
// index size is closer to 24 bytes per edge.
static constexpr size_t kFinalBytesPerEdge = 8;

// The temporary memory consists mainly of the FaceEdge and ClippedEdge
// structures plus a ClippedEdge pointer for every level of recursive
// subdivision.  This can be more than 220 bytes per edge even for typical
// geometry.  (The pathological worst case is higher, but we don't use this to
// determine the batch sizes.)
static constexpr size_t kTmpBytesPerEdge = 226;

// We arbitrarily limit the number of batches as a safety measure.  With the
// current default memory budget of 100 MB, this limit is not reached even
// when building an index of 350 million edges.
static constexpr int kMaxBatches = 100;

MutableS2ShapeIndex::BatchGenerator::BatchGenerator(int num_edges_removed,
                                                    int num_edges_added,
                                                    int shape_id_begin)
    : max_batch_sizes_(GetMaxBatchSizes(num_edges_removed, num_edges_added)),
      batch_begin_(shape_id_begin, 0),
      shape_id_end_(shape_id_begin) {
  if (max_batch_sizes_.size() > 1) {
    S2_VLOG(1) << "Removing " << num_edges_removed << ", adding "
            << num_edges_added << " edges in " << max_batch_sizes_.size()
            << " batches";
  }
  // Duplicate the last entry to simplify next_max_batch_size().
  max_batch_sizes_.push_back(max_batch_sizes_.back());

  // We process edge removals before additions, and edges are always removed
  // in a single batch.  The reasons for this include: (1) removed edges use
  // quite a bit of memory (about 50 bytes each) and this space can be freed
  // immediately when we process them in one batch; (2) removed shapes are
  // expected to be small fraction of the index size in typical use cases
  // (e.g. incremental updates of large indexes), and (3) AbsorbIndexCell()
  // uses (shape(id) == nullptr) to detect when a shape is being removed, so
  // in order to split the removed shapes into multiple batches we would need
  // a different approach (e.g., temporarily adding fake entries to shapes_
  // and restoring them back to nullptr as shapes are removed).  Removing
  // individual shapes over multiple batches would be even more work.
  batch_size_ = num_edges_removed;
}

void MutableS2ShapeIndex::BatchGenerator::AddShape(int shape_id,
                                                   int num_edges) {
  int batch_remaining = max_batch_size() - batch_size_;
  if (num_edges <= batch_remaining) {
    ExtendBatch(num_edges);
  } else if (num_edges <= next_max_batch_size()) {
    // Avoid splitting shapes across batches unnecessarily.
    FinishBatch(0, ShapeEdgeId(shape_id, 0));
    ExtendBatch(num_edges);
  } else {
    // This shape must be split across at least two batches.  We simply fill
    // each batch until the remaining edges will fit in two batches, and then
    // divide those edges such that both batches have the same amount of
    // remaining space relative to their maximum size.
    int e_begin = 0;
    while (batch_remaining + next_max_batch_size() < num_edges) {
      e_begin += batch_remaining;
      FinishBatch(batch_remaining, ShapeEdgeId(shape_id, e_begin));
      num_edges -= batch_remaining;
      batch_remaining = max_batch_size();
    }
    // Figure out how many edges to add to the current batch so that it will
    // have the same amount of remaining space as the next batch.
    int n = (num_edges + batch_remaining - next_max_batch_size()) / 2;
    FinishBatch(n, ShapeEdgeId(shape_id, e_begin + n));
    FinishBatch(num_edges - n, ShapeEdgeId(shape_id + 1, 0));
  }
  shape_id_end_ = shape_id + 1;
}

vector<MutableS2ShapeIndex::BatchDescriptor>
MutableS2ShapeIndex::BatchGenerator::Finish() {
  // We must generate at least one batch even when num_edges_removed ==
  // num_edges_added == 0, because some shapes have an interior but no edges.
  // (Specifically, the full polygon has this property.)
  if (batches_.empty() || shape_id_end_ != batch_begin_.shape_id) {
    FinishBatch(0, ShapeEdgeId(shape_id_end_, 0));
  }
  return std::move(batches_);
}

void MutableS2ShapeIndex::BatchGenerator::FinishBatch(int num_edges,
                                                      ShapeEdgeId batch_end) {
  ExtendBatch(num_edges);
  batches_.push_back(BatchDescriptor{batch_begin_, batch_end, batch_size_});
  batch_begin_ = batch_end;
  batch_index_edges_left_ -= batch_size_;
  while (batch_index_edges_left_ < 0) {
    batch_index_edges_left_ += max_batch_size();
    batch_index_ += 1;
  }
  batch_size_ = 0;
}

// Divides "num_edges" edges into batches where each batch needs about the
// same total amount of memory.  (The total memory needed by a batch consists
// of the temporary memory needed to process the edges in that batch plus the
// final representations of the edges that have already been indexed.)  It
// uses the fewest number of batches (up to kMaxBatches) such that the total
// memory usage does not exceed the combined final size of all the edges plus
// FLAGS_s2shape_index_tmp_memory_budget.  Returns a vector of sizes
// indicating the desired number of edges in each batch.
/* static */
vector<int> MutableS2ShapeIndex::BatchGenerator::GetMaxBatchSizes(
    int num_edges_removed, int num_edges_added) {
  // Check whether we can update all the edges at once.
  int num_edges_total = num_edges_removed + num_edges_added;
  const double tmp_memory_budget_bytes =
      absl::GetFlag(FLAGS_s2shape_index_tmp_memory_budget);
  if (num_edges_total * kTmpBytesPerEdge <= tmp_memory_budget_bytes) {
    return vector<int>{num_edges_total};
  }

  // Each batch is allowed to use up to "total_budget_bytes".  The memory
  // usage consists of some number of edges already added by previous batches
  // (at kFinalBytesPerEdge each), plus some number being updated in the
  // current batch (at kTmpBytesPerEdge each).  The available free space is
  // multiplied by (1 - kFinalBytesPerEdge / kTmpBytesPerEdge) after each
  // batch is processed as edges are converted into their final form.
  const double final_bytes = num_edges_added * kFinalBytesPerEdge;
  constexpr double kFinalBytesRatio = 1.0 * kFinalBytesPerEdge /
                                      kTmpBytesPerEdge;
  constexpr double kTmpSpaceMultiplier = 1 - kFinalBytesRatio;

  // The total memory budget is the greater of the final size plus the allowed
  // temporary memory, or the minimum amount of memory required to limit the
  // number of batches to "kMaxBatches".
  const double total_budget_bytes = max(
      final_bytes + tmp_memory_budget_bytes,
      final_bytes / (1 - MathUtil::IPow(kTmpSpaceMultiplier, kMaxBatches - 1)));

  // "ideal_batch_size" is the number of edges in the current batch before
  // rounding to an integer.
  double ideal_batch_size = total_budget_bytes / kTmpBytesPerEdge;

  // Removed edges are always processed in the first batch, even if this might
  // use more memory than requested (see the BatchGenerator constructor).
  vector<int> batch_sizes;
  int num_edges_left = num_edges_added;
  if (num_edges_removed > ideal_batch_size) {
    batch_sizes.push_back(num_edges_removed);
  } else {
    num_edges_left += num_edges_removed;
  }
  for (int i = 0; num_edges_left > 0; ++i) {
    int batch_size = static_cast<int>(ideal_batch_size + 1);
    batch_sizes.push_back(batch_size);
    num_edges_left -= batch_size;
    ideal_batch_size *= kTmpSpaceMultiplier;
  }
  S2_DCHECK_LE(batch_sizes.size(), kMaxBatches);
  return batch_sizes;
}

// Reserve an appropriate amount of space for the top-level face edges in the
// current batch.  This data structure uses about half of the temporary memory
// needed during index construction.  Furthermore, if the arrays are grown via
// push_back() then up to 10% of the total run time consists of copying data
// as these arrays grow, so it is worthwhile to preallocate space for them.
void MutableS2ShapeIndex::ReserveSpace(
    const BatchDescriptor& batch, vector<FaceEdge> all_edges[6]) {
  // The following accounts for the temporary space needed for everything
  // except the FaceEdge vectors (which are allocated separately below).
  int64 other_usage = batch.num_edges * (kTmpBytesPerEdge - sizeof(FaceEdge));

  // If the number of edges is relatively small, then the fastest approach is
  // to simply reserve space on every face for the maximum possible number of
  // edges.  (We use a different threshold for this calculation than for
  // deciding when to break updates into batches because the cost/benefit
  // ratio is different.  Here the only extra expense is that we need to
  // sample the edges to estimate how many edges per face there are, and
  // therefore we generally use a lower threshold.)
  const size_t kMaxCheapBytes =
      min(absl::GetFlag(FLAGS_s2shape_index_tmp_memory_budget) / 2,
          int64{30} << 20 /*30 MB*/);
  int64 face_edge_usage = batch.num_edges * (6 * sizeof(FaceEdge));
  if (face_edge_usage <= kMaxCheapBytes) {
    if (!mem_tracker_.TallyTemp(face_edge_usage + other_usage)) {
      return;
    }
    for (int face = 0; face < 6; ++face) {
      all_edges[face].reserve(batch.num_edges);
    }
    return;
  }
  // Otherwise we estimate the number of edges on each face by taking a random
  // sample.  The goal is to come up with an estimate that is fast and
  // accurate for non-pathological geometry.  If our estimates happen to be
  // wrong, the vector will still grow automatically - the main side effects
  // are that memory usage will be larger (by up to a factor of 3), and
  // constructing the index will be about 10% slower.
  //
  // Given a desired sample size, we choose equally spaced edges from
  // throughout the entire data set.  We use a Bresenham-type algorithm to
  // choose the samples.
  const int kDesiredSampleSize = 10000;
  const int sample_interval = max(1, batch.num_edges / kDesiredSampleSize);

  // Initialize "edge_id" to be midway through the first sample interval.
  // Because samples are equally spaced the actual sample size may differ
  // slightly from the desired sample size.
  int edge_id = sample_interval / 2;
  const int actual_sample_size = (batch.num_edges + edge_id) / sample_interval;
  int face_count[6] = { 0, 0, 0, 0, 0, 0 };
  if (pending_removals_) {
    for (const RemovedShape& removed : *pending_removals_) {
      edge_id += removed.edges.size();
      while (edge_id >= sample_interval) {
        edge_id -= sample_interval;
        face_count[S2::GetFace(removed.edges[edge_id].v0)] += 1;
      }
    }
  }
  for (auto begin = batch.begin; begin < batch.end;
       ++begin.shape_id, begin.edge_id = 0) {
    const S2Shape* shape = this->shape(begin.shape_id);
    if (shape == nullptr) continue;  // Already removed.
    int edges_end = begin.shape_id == batch.end.shape_id ? batch.end.edge_id
                                                         : shape->num_edges();
    edge_id += edges_end - begin.edge_id;
    while (edge_id >= sample_interval) {
      edge_id -= sample_interval;
      // For speed, we only count the face containing one endpoint of the
      // edge.  In general the edge could span all 6 faces (with padding), but
      // it's not worth the expense to compute this more accurately.
      face_count[S2::GetFace(shape->edge(edge_id + begin.edge_id).v0)] += 1;
    }
  }
  // Now given the raw face counts, compute a confidence interval such that we
  // will be unlikely to allocate too little space.  Computing accurate
  // binomial confidence intervals is expensive and not really necessary.
  // Instead we use a simple approximation:
  //  - For any face with at least 1 sample, we use at least a 4-sigma
  //    confidence interval.  (The chosen width is adequate for the worst case
  //    accuracy, which occurs when the face contains approximately 50% of the
  //    edges.)  Assuming that our sample is representative, the probability of
  //    reserving too little space is approximately 1 in 30,000.
  //  - For faces with no samples at all, we don't bother reserving space.
  //    It is quite likely that such faces are truly empty, so we save time
  //    and memory this way.  If the face does contain some edges, there will
  //    only be a few so it is fine to let the vector grow automatically.
  // On average, we reserve 2% extra space for each face that has geometry
  // (which could be up to 12% extra space overall, but typically 2%).

  // kMaxSemiWidth is the maximum semi-width over all probabilities p of a
  // 4-sigma binomial confidence interval with a sample size of 10,000.
  const double kMaxSemiWidth = 0.02;

  // First estimate the total amount of memory we are about to allocate.
  double multiplier = 1.0;
  for (int face = 0; face < 6; ++face) {
    if (face_count[face] != 0) multiplier += kMaxSemiWidth;
  }
  face_edge_usage = multiplier * batch.num_edges * sizeof(FaceEdge);
  if (!mem_tracker_.TallyTemp(face_edge_usage + other_usage)) {
    return;
  }
  const double sample_ratio = 1.0 / actual_sample_size;
  for (int face = 0; face < 6; ++face) {
    if (face_count[face] == 0) continue;
    double fraction = sample_ratio * face_count[face] + kMaxSemiWidth;
    all_edges[face].reserve(fraction * batch.num_edges);
  }
}

// Clips the edges of the given shape to the six cube faces, add the clipped
// edges to "all_edges", and start tracking its interior if necessary.
void MutableS2ShapeIndex::AddShape(
    const S2Shape* shape, int edges_begin, int edges_end,
    vector<FaceEdge> all_edges[6], InteriorTracker* tracker) const {
  // Construct a template for the edges to be added.
  FaceEdge edge;
  edge.shape_id = shape->id();
  edge.has_interior = false;
  if (shape->dimension() == 2) {
    // To add a single shape with an interior over multiple batches, we first
    // add all the edges without tracking the interior.  After all edges have
    // been added, the interior is updated in a separate step by setting the
    // contains_center() flags appropriately.
    if (edges_begin > 0 || edges_end < shape->num_edges()) {
      tracker->set_partial_shape_id(edge.shape_id);
    } else {
      edge.has_interior = true;
      tracker->AddShape(
          edge.shape_id,
          s2shapeutil::ContainsBruteForce(*shape, tracker->focus()));
    }
  }
  for (int e = edges_begin; e < edges_end; ++e) {
    edge.edge_id = e;
    edge.edge = shape->edge(e);
    edge.max_level = GetEdgeMaxLevel(edge.edge);
    AddFaceEdge(&edge, all_edges);
  }
}

void MutableS2ShapeIndex::RemoveShape(const RemovedShape& removed,
                                      vector<FaceEdge> all_edges[6],
                                      InteriorTracker* tracker) const {
  FaceEdge edge;
  edge.edge_id = -1;  // Not used or needed for removed edges.
  edge.shape_id = removed.shape_id;
  edge.has_interior = removed.has_interior;
  if (edge.has_interior) {
    tracker->AddShape(edge.shape_id, removed.contains_tracker_origin);
  }
  for (const auto& removed_edge : removed.edges) {
    edge.edge = removed_edge;
    edge.max_level = GetEdgeMaxLevel(edge.edge);
    AddFaceEdge(&edge, all_edges);
  }
}

void MutableS2ShapeIndex::FinishPartialShape(int shape_id) {
  if (shape_id < 0) return;  // The partial shape did not have an interior.
  const S2Shape* shape = this->shape(shape_id);

  // Filling in the interior of a partial shape can grow the cell_map_
  // significantly, however the new cells have just one shape and no edges.
  // The following is a rough estimate of how much extra memory is needed
  // based on experiments.  It assumes that one new cell is required for every
  // 10 shape edges, and that the cell map uses 50% more space than necessary
  // for the new entries because they are inserted between existing entries
  // (which means that the btree nodes are not full).
  if (mem_tracker_.is_active()) {
    const int64 new_usage =
        SpaceUsed() - mem_tracker_.client_usage_bytes() +
        0.1 * shape->num_edges() * (1.5 * sizeof(CellMap::value_type) +
                                    sizeof(S2ShapeIndexCell) +
                                    sizeof(S2ClippedShape));
    if (!mem_tracker_.TallyTemp(new_usage)) return;
  }

  // All the edges of the partial shape have already been indexed, now we just
  // need to set the contains_center() flags appropriately.  We use a fresh
  // InteriorTracker for this purpose since we don't want to continue tracking
  // the interior state of any other shapes in this batch.
  //
  // We have implemented this below in the simplest way possible, namely by
  // scanning through the entire index.  In theory it would be more efficient
  // to keep track of the set of index cells that were modified when the
  // partial shape's edges were added, and then visit only those cells.
  // However in practice any shape that is added over multiple batches is
  // likely to occupy most or all of the index anyway, so it is faster and
  // simpler to just iterate through the entire index.
  //
  // "tmp_edges" below speeds up large polygon index construction by 3-12%.
  vector<S2Shape::Edge> tmp_edges;  // Temporary storage.
  InteriorTracker tracker;
  tracker.AddShape(shape_id,
                   s2shapeutil::ContainsBruteForce(*shape, tracker.focus()));
  S2CellId begin = S2CellId::Begin(S2CellId::kMaxLevel);
  for (CellMap::iterator index_it = cell_map_.begin(); ; ++index_it) {
    if (!tracker.shape_ids().empty()) {
      // Check whether we need to add new cells that are entirely contained by
      // the partial shape.
      S2CellId fill_end =
          (index_it != cell_map_.end()) ? index_it->first.range_min()
                                        : S2CellId::End(S2CellId::kMaxLevel);
      if (begin != fill_end) {
        for (S2CellId cellid : S2CellUnion::FromBeginEnd(begin, fill_end)) {
          S2ShapeIndexCell* cell = new S2ShapeIndexCell;
          S2ClippedShape* clipped = cell->add_shapes(1);
          clipped->Init(shape_id, 0);
          clipped->set_contains_center(true);
          index_it = cell_map_.insert(index_it, make_pair(cellid, cell));
          ++index_it;
        }
      }
    }
    if (index_it == cell_map_.end()) break;

    // Now check whether the current index cell needs to be updated.
    S2CellId cellid = index_it->first;
    S2ShapeIndexCell* cell = index_it->second;
    int n = cell->shapes_.size();
    if (n > 0 && cell->shapes_[n - 1].shape_id() == shape_id) {
      // This cell contains edges of the partial shape.  If the partial shape
      // contains the center of this cell, we must update the index.
      S2PaddedCell pcell(cellid, kCellPadding);
      if (!tracker.at_cellid(cellid)) {
        tracker.MoveTo(pcell.GetEntryVertex());
      }
      tracker.DrawTo(pcell.GetCenter());
      S2ClippedShape* clipped = &cell->shapes_[n - 1];
      int num_edges = clipped->num_edges();
      S2_DCHECK_GT(num_edges, 0);
      for (int i = 0; i < num_edges; ++i) {
        tmp_edges.push_back(shape->edge(clipped->edge(i)));
      }
      for (const auto& edge : tmp_edges) {
        tracker.TestEdge(shape_id, edge);
      }
      if (!tracker.shape_ids().empty()) {
        // The partial shape contains the center of this index cell.
        clipped->set_contains_center(true);
      }
      tracker.DrawTo(pcell.GetExitVertex());
      for (const auto& edge : tmp_edges) {
        tracker.TestEdge(shape_id, edge);
      }
      tracker.set_next_cellid(cellid.next());
      tmp_edges.clear();

    } else if (!tracker.shape_ids().empty()) {
      // The partial shape contains the center of an existing index cell that
      // does not intersect any of its edges.
      S2ClippedShape* clipped = cell->add_shapes(1);
      clipped->Init(shape_id, 0);
      clipped->set_contains_center(true);
    }
    begin = cellid.range_max().next();
  }
}

inline void MutableS2ShapeIndex::AddFaceEdge(
    FaceEdge* edge, vector<FaceEdge> all_edges[6]) const {
  // Fast path: both endpoints are on the same face, and are far enough from
  // the edge of the face that don't intersect any (padded) adjacent face.
  int a_face = S2::GetFace(edge->edge.v0);
  if (a_face == S2::GetFace(edge->edge.v1)) {
    S2::ValidFaceXYZtoUV(a_face, edge->edge.v0, &edge->a);
    S2::ValidFaceXYZtoUV(a_face, edge->edge.v1, &edge->b);
    const double kMaxUV = 1 - kCellPadding;
    if (fabs(edge->a[0]) <= kMaxUV && fabs(edge->a[1]) <= kMaxUV &&
        fabs(edge->b[0]) <= kMaxUV && fabs(edge->b[1]) <= kMaxUV) {
      all_edges[a_face].push_back(*edge);
      return;
    }
  }
  // Otherwise we simply clip the edge to all six faces.
  for (int face = 0; face < 6; ++face) {
    if (S2::ClipToPaddedFace(edge->edge.v0, edge->edge.v1, face,
                             kCellPadding, &edge->a, &edge->b)) {
      all_edges[face].push_back(*edge);
    }
  }
}

// Returns the first level for which the given edge will be considered "long",
// i.e. it will not count towards the max_edges_per_cell() limit.
int MutableS2ShapeIndex::GetEdgeMaxLevel(const S2Shape::Edge& edge) const {
  // Compute the maximum cell edge length for which this edge is considered
  // "long".  The calculation does not need to be perfectly accurate, so we
  // use Norm() rather than Angle() for speed.
  double max_cell_edge =
      ((edge.v0 - edge.v1).Norm() *
       absl::GetFlag(FLAGS_s2shape_index_cell_size_to_long_edge_ratio));

  // Now return the first level encountered during subdivision where the
  // average cell edge length at that level is at most "max_cell_edge".
  return S2::kAvgEdge.GetLevelForMaxValue(max_cell_edge);
}

// EdgeAllocator provides temporary storage for new ClippedEdges that are
// created during indexing.  It is essentially a stack model, where edges are
// allocated as the recursion does down and freed as it comes back up.
//
// It also provides a mutable vector of FaceEdges that is used when
// incrementally updating the index (see AbsorbIndexCell).
class MutableS2ShapeIndex::EdgeAllocator {
 public:
  EdgeAllocator() : size_(0) {}

  // Return a pointer to a newly allocated edge.  The EdgeAllocator
  // retains ownership.
  ClippedEdge* NewClippedEdge() {
    if (size_ == clipped_edges_.size()) {
      clipped_edges_.emplace_back(new ClippedEdge);
    }
    return clipped_edges_[size_++].get();
  }
  // Return the number of allocated edges.
  size_t size() const { return size_; }

  // Reset the allocator to only contain the first "size" allocated edges.
  void Reset(size_t size) { size_ = size; }

  vector<FaceEdge>* mutable_face_edges() {
    return &face_edges_;
  }

 private:
  // We can't use vector<ClippedEdge> because edges are not allowed to move
  // once they have been allocated.  Instead we keep a pool of allocated edges
  // that are all deleted together at the end.
  size_t size_;
  vector<unique_ptr<ClippedEdge>> clipped_edges_;

  // On the other hand, we can use vector<FaceEdge> because they are allocated
  // only at one level during the recursion (namely, the level at which we
  // absorb an existing index cell).
  vector<FaceEdge> face_edges_;

  EdgeAllocator(const EdgeAllocator&) = delete;
  void operator=(const EdgeAllocator&) = delete;
};

// Given a face and a vector of edges that intersect that face, add or remove
// all the edges from the index.  (An edge is added if shapes_[id] is not
// nullptr, and removed otherwise.)
void MutableS2ShapeIndex::UpdateFaceEdges(int face,
                                          const vector<FaceEdge>& face_edges,
                                          InteriorTracker* tracker) {
  int num_edges = face_edges.size();
  if (num_edges == 0 && tracker->shape_ids().empty()) return;

  // Create the initial ClippedEdge for each FaceEdge.  Additional clipped
  // edges are created when edges are split between child cells.  We create
  // two arrays, one containing the edge data and another containing pointers
  // to those edges, so that during the recursion we only need to copy
  // pointers in order to propagate an edge to the correct child.
  vector<ClippedEdge> clipped_edge_storage;
  vector<const ClippedEdge*> clipped_edges;
  clipped_edge_storage.reserve(num_edges);
  clipped_edges.reserve(num_edges);
  R2Rect bound = R2Rect::Empty();
  for (int e = 0; e < num_edges; ++e) {
    ClippedEdge clipped;
    clipped.face_edge = &face_edges[e];
    clipped.bound = R2Rect::FromPointPair(face_edges[e].a, face_edges[e].b);
    clipped_edge_storage.push_back(clipped);
    clipped_edges.push_back(&clipped_edge_storage.back());
    bound.AddRect(clipped.bound);
  }
  // Construct the initial face cell containing all the edges, and then update
  // all the edges in the index recursively.
  EdgeAllocator alloc;
  S2CellId face_id = S2CellId::FromFace(face);
  S2PaddedCell pcell(face_id, kCellPadding);

  // "disjoint_from_index" means that the current cell being processed (and
  // all its descendants) are not already present in the index.  It is set to
  // true during the recursion whenever we detect that the current cell is
  // disjoint from the index.  We could save a tiny bit of work by setting
  // this flag to true here on the very first update, however currently there
  // is no easy way to check that.  (It's not sufficient to test whether
  // cell_map_.empty() or pending_additions_begin_ == 0.)
  bool disjoint_from_index = false;
  if (num_edges > 0) {
    S2CellId shrunk_id = ShrinkToFit(pcell, bound);
    if (shrunk_id != pcell.id()) {
      // All the edges are contained by some descendant of the face cell.  We
      // can save a lot of work by starting directly with that cell, but if we
      // are in the interior of at least one shape then we need to create
      // index entries for the cells we are skipping over.
      SkipCellRange(face_id.range_min(), shrunk_id.range_min(),
                    tracker, &alloc, disjoint_from_index);
      pcell = S2PaddedCell(shrunk_id, kCellPadding);
      UpdateEdges(pcell, &clipped_edges, tracker, &alloc, disjoint_from_index);
      SkipCellRange(shrunk_id.range_max().next(), face_id.range_max().next(),
                    tracker, &alloc, disjoint_from_index);
      return;
    }
  }
  // Otherwise (no edges, or no shrinking is possible), subdivide normally.
  UpdateEdges(pcell, &clipped_edges, tracker, &alloc, disjoint_from_index);
}

S2CellId MutableS2ShapeIndex::ShrinkToFit(const S2PaddedCell& pcell,
                                          const R2Rect& bound) const {
  S2CellId shrunk_id = pcell.ShrinkToFit(bound);
  if (shrunk_id != pcell.id()) {
    // Don't shrink any smaller than the existing index cells, since we need
    // to combine the new edges with those cells.  Use InitStale() to avoid
    // applying updates recursively.
    Iterator iter;
    iter.InitStale(this);
    CellRelation r = iter.Locate(shrunk_id);
    if (r == INDEXED) { shrunk_id = iter.id(); }
  }
  return shrunk_id;
}

// Skip over the cells in the given range, creating index cells if we are
// currently in the interior of at least one shape.
void MutableS2ShapeIndex::SkipCellRange(S2CellId begin, S2CellId end,
                                        InteriorTracker* tracker,
                                        EdgeAllocator* alloc,
                                        bool disjoint_from_index) {
  // If we aren't in the interior of a shape, then skipping over cells is easy.
  if (tracker->shape_ids().empty()) return;

  // Otherwise generate the list of cell ids that we need to visit, and create
  // an index entry for each one.
  for (S2CellId skipped_id : S2CellUnion::FromBeginEnd(begin, end)) {
    vector<const ClippedEdge*> clipped_edges;
    UpdateEdges(S2PaddedCell(skipped_id, kCellPadding),
                &clipped_edges, tracker, alloc, disjoint_from_index);
  }
}

// Given an edge and an interval "middle" along the v-axis, clip the edge
// against the boundaries of "middle" and add the edge to the corresponding
// children.
/* static */ ABSL_ATTRIBUTE_ALWAYS_INLINE  // ~8% faster
inline void MutableS2ShapeIndex::ClipVAxis(
    const ClippedEdge* edge,
    const R1Interval& middle,
    vector<const ClippedEdge*> child_edges[2],
    EdgeAllocator* alloc) {
  if (edge->bound[1].hi() <= middle.lo()) {
    // Edge is entirely contained in the lower child.
    child_edges[0].push_back(edge);
  } else if (edge->bound[1].lo() >= middle.hi()) {
    // Edge is entirely contained in the upper child.
    child_edges[1].push_back(edge);
  } else {
    // The edge bound spans both children.
    child_edges[0].push_back(ClipVBound(edge, 1, middle.hi(), alloc));
    child_edges[1].push_back(ClipVBound(edge, 0, middle.lo(), alloc));
  }
}

// Given a cell and a set of ClippedEdges whose bounding boxes intersect that
// cell, add or remove all the edges from the index.  Temporary space for
// edges that need to be subdivided is allocated from the given EdgeAllocator.
// "disjoint_from_index" is an optimization hint indicating that cell_map_
// does not contain any entries that overlap the given cell.
void MutableS2ShapeIndex::UpdateEdges(const S2PaddedCell& pcell,
                                      vector<const ClippedEdge*>* edges,
                                      InteriorTracker* tracker,
                                      EdgeAllocator* alloc,
                                      bool disjoint_from_index) {
  // Cases where an index cell is not needed should be detected before this.
  S2_DCHECK(!edges->empty() || !tracker->shape_ids().empty());

  // This function is recursive with a maximum recursion depth of 30
  // (S2CellId::kMaxLevel).  Note that using an explicit stack does not seem
  // to be any faster based on profiling.

  // Incremental updates are handled as follows.  All edges being added or
  // removed are combined together in "edges", and all shapes with interiors
  // are tracked using "tracker".  We subdivide recursively as usual until we
  // encounter an existing index cell.  At this point we "absorb" the index
  // cell as follows:
  //
  //   - Edges and shapes that are being removed are deleted from "edges" and
  //     "tracker".
  //   - All remaining edges and shapes from the index cell are added to
  //     "edges" and "tracker".
  //   - Continue subdividing recursively, creating new index cells as needed.
  //   - When the recursion gets back to the cell that was absorbed, we
  //     restore "edges" and "tracker" to their previous state.
  //
  // Note that the only reason that we include removed shapes in the recursive
  // subdivision process is so that we can find all of the index cells that
  // contain those shapes efficiently, without maintaining an explicit list of
  // index cells for each shape (which would be expensive in terms of memory).
  bool index_cell_absorbed = false;
  if (!disjoint_from_index) {
    // There may be existing index cells contained inside "pcell".  If we
    // encounter such a cell, we need to combine the edges being updated with
    // the existing cell contents by "absorbing" the cell.  We use InitStale()
    // to avoid applying updates recursively.
    Iterator iter;
    iter.InitStale(this);
    CellRelation r = iter.Locate(pcell.id());
    if (r == DISJOINT) {
      disjoint_from_index = true;
    } else if (r == INDEXED) {
      // Absorb the index cell by transferring its contents to "edges" and
      // deleting it.  We also start tracking the interior of any new shapes.
      AbsorbIndexCell(pcell, iter, edges, tracker, alloc);
      index_cell_absorbed = true;
      disjoint_from_index = true;
    } else {
      S2_DCHECK_EQ(SUBDIVIDED, r);
    }
  }

  // If there are existing index cells below us, then we need to keep
  // subdividing so that we can merge with those cells.  Otherwise,
  // MakeIndexCell checks if the number of edges is small enough, and creates
  // an index cell if possible (returning true when it does so).
  if (!disjoint_from_index || !MakeIndexCell(pcell, *edges, tracker)) {
    // Reserve space for the edges that will be passed to each child.  This is
    // important since otherwise the running time is dominated by the time
    // required to grow the vectors.  The amount of memory involved is
    // relatively small, so we simply reserve the maximum space for every child.
    vector<const ClippedEdge*> child_edges[2][2];  // [i][j]
    int num_edges = edges->size();
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 2; ++j) {
        child_edges[i][j].reserve(num_edges);
      }
    }

    // Remember the current size of the EdgeAllocator so that we can free any
    // edges that are allocated during edge splitting.
    size_t alloc_size = alloc->size();

    // Compute the middle of the padded cell, defined as the rectangle in
    // (u,v)-space that belongs to all four (padded) children.  By comparing
    // against the four boundaries of "middle" we can determine which children
    // each edge needs to be propagated to.
    const R2Rect& middle = pcell.middle();

    // Build up a vector edges to be passed to each child cell.  The (i,j)
    // directions are left (i=0), right (i=1), lower (j=0), and upper (j=1).
    // Note that the vast majority of edges are propagated to a single child.
    // This case is very fast, consisting of between 2 and 4 floating-point
    // comparisons and copying one pointer.  (ClipVAxis is inline.)
    for (int e = 0; e < num_edges; ++e) {
      const ClippedEdge* edge = (*edges)[e];
      if (edge->bound[0].hi() <= middle[0].lo()) {
        // Edge is entirely contained in the two left children.
        ClipVAxis(edge, middle[1], child_edges[0], alloc);
      } else if (edge->bound[0].lo() >= middle[0].hi()) {
        // Edge is entirely contained in the two right children.
        ClipVAxis(edge, middle[1], child_edges[1], alloc);
      } else if (edge->bound[1].hi() <= middle[1].lo()) {
        // Edge is entirely contained in the two lower children.
        child_edges[0][0].push_back(ClipUBound(edge, 1, middle[0].hi(), alloc));
        child_edges[1][0].push_back(ClipUBound(edge, 0, middle[0].lo(), alloc));
      } else if (edge->bound[1].lo() >= middle[1].hi()) {
        // Edge is entirely contained in the two upper children.
        child_edges[0][1].push_back(ClipUBound(edge, 1, middle[0].hi(), alloc));
        child_edges[1][1].push_back(ClipUBound(edge, 0, middle[0].lo(), alloc));
      } else {
        // The edge bound spans all four children.  The edge itself intersects
        // either three or four (padded) children.
        const ClippedEdge* left = ClipUBound(edge, 1, middle[0].hi(), alloc);
        ClipVAxis(left, middle[1], child_edges[0], alloc);
        const ClippedEdge* right = ClipUBound(edge, 0, middle[0].lo(), alloc);
        ClipVAxis(right, middle[1], child_edges[1], alloc);
      }
    }
    // Free any memory reserved for children that turned out to be empty.  This
    // step is cheap and reduces peak memory usage by about 10% when building
    // large indexes (> 10M edges).
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 2; ++j) {
        if (child_edges[i][j].empty()) {
          vector<const ClippedEdge*>().swap(child_edges[i][j]);
        }
      }
    }
    // Now recursively update the edges in each child.  We call the children in
    // increasing order of S2CellId so that when the index is first constructed,
    // all insertions into cell_map_ are at the end (which is much faster).
    for (int pos = 0; pos < 4; ++pos) {
      int i, j;
      pcell.GetChildIJ(pos, &i, &j);
      if (!child_edges[i][j].empty() || !tracker->shape_ids().empty()) {
        UpdateEdges(S2PaddedCell(pcell, i, j), &child_edges[i][j],
                    tracker, alloc, disjoint_from_index);
      }
    }
    // Free any temporary edges that were allocated during clipping.
    alloc->Reset(alloc_size);
  }
  if (index_cell_absorbed) {
    // Restore the state for any edges being removed that we are tracking.
    tracker->RestoreStateBefore(pending_additions_begin_);
  }
}

// Given an edge, clip the given endpoint (lo=0, hi=1) of the u-axis so that
// it does not extend past the given value.
/* static */
const MutableS2ShapeIndex::ClippedEdge*
MutableS2ShapeIndex::ClipUBound(const ClippedEdge* edge, int u_end, double u,
                                EdgeAllocator* alloc) {
  // First check whether the edge actually requires any clipping.  (Sometimes
  // this method is called when clipping is not necessary, e.g. when one edge
  // endpoint is in the overlap area between two padded child cells.)
  if (u_end == 0) {
    if (edge->bound[0].lo() >= u) return edge;
  } else {
    if (edge->bound[0].hi() <= u) return edge;
  }
  // We interpolate the new v-value from the endpoints of the original edge.
  // This has two advantages: (1) we don't need to store the clipped endpoints
  // at all, just their bounding box; and (2) it avoids the accumulation of
  // roundoff errors due to repeated interpolations.  The result needs to be
  // clamped to ensure that it is in the appropriate range.
  const FaceEdge& e = *edge->face_edge;
  double v = edge->bound[1].Project(
      S2::InterpolateDouble(u, e.a[0], e.b[0], e.a[1], e.b[1]));

  // Determine which endpoint of the v-axis bound to update.  If the edge
  // slope is positive we update the same endpoint, otherwise we update the
  // opposite endpoint.
  int v_end = u_end ^ ((e.a[0] > e.b[0]) != (e.a[1] > e.b[1]));
  return UpdateBound(edge, u_end, u, v_end, v, alloc);
}

// Given an edge, clip the given endpoint (lo=0, hi=1) of the v-axis so that
// it does not extend past the given value.
/* static */
const MutableS2ShapeIndex::ClippedEdge*
MutableS2ShapeIndex::ClipVBound(const ClippedEdge* edge, int v_end, double v,
                                EdgeAllocator* alloc) {
  // See comments in ClipUBound.
  if (v_end == 0) {
    if (edge->bound[1].lo() >= v) return edge;
  } else {
    if (edge->bound[1].hi() <= v) return edge;
  }
  const FaceEdge& e = *edge->face_edge;
  double u = edge->bound[0].Project(
      S2::InterpolateDouble(v, e.a[1], e.b[1], e.a[0], e.b[0]));
  int u_end = v_end ^ ((e.a[0] > e.b[0]) != (e.a[1] > e.b[1]));
  return UpdateBound(edge, u_end, u, v_end, v, alloc);
}

// Given an edge and two bound endpoints that need to be updated, allocate and
// return a new edge with the updated bound.
/* static */
inline const MutableS2ShapeIndex::ClippedEdge*
MutableS2ShapeIndex::UpdateBound(const ClippedEdge* edge, int u_end, double u,
                                 int v_end, double v, EdgeAllocator* alloc) {
  ClippedEdge* clipped = alloc->NewClippedEdge();
  clipped->face_edge = edge->face_edge;
  clipped->bound[0][u_end] = u;
  clipped->bound[1][v_end] = v;
  clipped->bound[0][1-u_end] = edge->bound[0][1-u_end];
  clipped->bound[1][1-v_end] = edge->bound[1][1-v_end];
  S2_DCHECK(!clipped->bound.is_empty());
  S2_DCHECK(edge->bound.Contains(clipped->bound));
  return clipped;
}

// Absorb an index cell by transferring its contents to "edges" and/or
// "tracker", and then delete this cell from the index.  If "edges" includes
// any edges that are being removed, this method also updates their
// InteriorTracker state to correspond to the exit vertex of this cell, and
// saves the InteriorTracker state by calling SaveAndClearStateBefore().  It
// is the caller's responsibility to restore this state by calling
// RestoreStateBefore() when processing of this cell is finished.
void MutableS2ShapeIndex::AbsorbIndexCell(const S2PaddedCell& pcell,
                                          const Iterator& iter,
                                          vector<const ClippedEdge*>* edges,
                                          InteriorTracker* tracker,
                                          EdgeAllocator* alloc) {
  S2_DCHECK_EQ(pcell.id(), iter.id());

  // When we absorb a cell, we erase all the edges that are being removed.
  // However when we are finished with this cell, we want to restore the state
  // of those edges (since that is how we find all the index cells that need
  // to be updated).  The edges themselves are restored automatically when
  // UpdateEdges returns from its recursive call, but the InteriorTracker
  // state needs to be restored explicitly.
  //
  // Here we first update the InteriorTracker state for removed edges to
  // correspond to the exit vertex of this cell, and then save the
  // InteriorTracker state.  This state will be restored by UpdateEdges when
  // it is finished processing the contents of this cell.  (Note in the test
  // below that removed edges are always sorted before added edges.)
  if (tracker->is_active() && !edges->empty() &&
      is_shape_being_removed((*edges)[0]->face_edge->shape_id)) {
    // We probably need to update the InteriorTracker.  ("Probably" because
    // it's possible that all shapes being removed do not have interiors.)
    if (!tracker->at_cellid(pcell.id())) {
      tracker->MoveTo(pcell.GetEntryVertex());
    }
    tracker->DrawTo(pcell.GetExitVertex());
    tracker->set_next_cellid(pcell.id().next());
    for (const ClippedEdge* edge : *edges) {
      const FaceEdge* face_edge = edge->face_edge;
      if (!is_shape_being_removed(face_edge->shape_id)) {
        break;  // All shapes being removed come first.
      }
      if (face_edge->has_interior) {
        tracker->TestEdge(face_edge->shape_id, face_edge->edge);
      }
    }
  }
  // Save the state of the edges being removed so that it can be restored when
  // we are finished processing this cell and its children.  Below we not only
  // remove those edges but also add new edges whose state only needs to be
  // tracked within this subtree.  We don't need to save the state of the
  // edges being added because they aren't being removed from "edges" and will
  // therefore be updated normally as we visit this cell and its children.
  tracker->SaveAndClearStateBefore(pending_additions_begin_);

  // Create a FaceEdge for each edge in this cell that isn't being removed.
  vector<FaceEdge>* face_edges = alloc->mutable_face_edges();
  face_edges->clear();
  bool tracker_moved = false;
  const S2ShapeIndexCell& cell = iter.cell();
  for (int s = 0; s < cell.num_clipped(); ++s) {
    const S2ClippedShape& clipped = cell.clipped(s);
    int shape_id = clipped.shape_id();
    const S2Shape* shape = this->shape(shape_id);
    if (shape == nullptr) continue;  // This shape is being removed.
    int num_edges = clipped.num_edges();

    // If this shape has an interior, start tracking whether we are inside the
    // shape.  UpdateEdges() wants to know whether the entry vertex of this
    // cell is inside the shape, but we only know whether the center of the
    // cell is inside the shape, so we need to test all the edges against the
    // line segment from the cell center to the entry vertex.
    FaceEdge edge;
    edge.shape_id = shape_id;
    edge.has_interior = (shape->dimension() == 2 &&
                         shape_id != tracker->partial_shape_id());
    if (edge.has_interior) {
      tracker->AddShape(shape_id, clipped.contains_center());
      // There might not be any edges in this entire cell (i.e., it might be
      // in the interior of all shapes), so we delay updating the tracker
      // until we see the first edge.
      if (!tracker_moved && num_edges > 0) {
        tracker->MoveTo(pcell.GetCenter());
        tracker->DrawTo(pcell.GetEntryVertex());
        tracker->set_next_cellid(pcell.id());
        tracker_moved = true;
      }
    }
    for (int i = 0; i < num_edges; ++i) {
      int e = clipped.edge(i);
      edge.edge_id = e;
      edge.edge = shape->edge(e);
      edge.max_level = GetEdgeMaxLevel(edge.edge);
      if (edge.has_interior) tracker->TestEdge(shape_id, edge.edge);
      if (!S2::ClipToPaddedFace(edge.edge.v0, edge.edge.v1, pcell.id().face(),
                                kCellPadding, &edge.a, &edge.b)) {
        S2_LOG(DFATAL) << "Invariant failure in MutableS2ShapeIndex";
      }
      face_edges->push_back(edge);
    }
  }
  // Now create a ClippedEdge for each FaceEdge, and put them in "new_edges".
  vector<const ClippedEdge*> new_edges;
  for (const FaceEdge& face_edge : *face_edges) {
    ClippedEdge* clipped = alloc->NewClippedEdge();
    clipped->face_edge = &face_edge;
    clipped->bound = S2::GetClippedEdgeBound(face_edge.a, face_edge.b,
                                                     pcell.bound());
    new_edges.push_back(clipped);
  }
  // Discard any edges from "edges" that are being removed, and append the
  // remainder to "new_edges".  (This keeps the edges sorted by shape id.)
  for (int i = 0; i < edges->size(); ++i) {
    const ClippedEdge* clipped = (*edges)[i];
    if (!is_shape_being_removed(clipped->face_edge->shape_id)) {
      new_edges.insert(new_edges.end(), edges->begin() + i, edges->end());
      break;
    }
  }
  // Update the edge list and delete this cell from the index.
  edges->swap(new_edges);
  cell_map_.erase(pcell.id());
  delete &cell;
}

// Attempt to build an index cell containing the given edges, and return true
// if successful.  (Otherwise the edges should be subdivided further.)
bool MutableS2ShapeIndex::MakeIndexCell(const S2PaddedCell& pcell,
                                        const vector<const ClippedEdge*>& edges,
                                        InteriorTracker* tracker) {
  if (edges.empty() && tracker->shape_ids().empty()) {
    // No index cell is needed.  (In most cases this situation is detected
    // before we get to this point, but this can happen when all shapes in a
    // cell are removed.)
    return true;
  }

  // We can show using amortized analysis that the total index size is
  //
  //     O(c1 * n + c2 * (1 - f) / f * n)
  //
  // where n is the number of input edges (and where we also count an "edge"
  // for each shape with an interior but no edges), f is the value of
  // FLAGS_s2shape_index_min_short_edge_fraction, and c1 and c2 are constants
  // where c2 is about 20 times larger than c1.
  //
  // First observe that the space used by a MutableS2ShapeIndex is
  // proportional to the space used by all of its index cells, and the space
  // used by an S2ShapeIndexCell is proportional to the number of edges that
  // intersect that cell plus the number of shapes that contain the entire
  // cell ("containing shapes").  Define an "index entry" as an intersecting
  // edge or containing shape stored by an index cell.  Our goal is then to
  // bound the number of index entries.
  //
  // We divide the index entries into two groups.  An index entry is "short"
  // if it represents an edge that was considered short in that index cell's
  // parent, and "long" otherwise.  (Note that the long index entries also
  // include the containing shapes mentioned above.)  We then bound the
  // maximum number of both types of index entries by associating them with
  // edges that were considered short in those index cells' parents.
  //
  // First consider the short index entries for a given edge E.  Let S be the
  // set of index cells that intersect E and where E was considered short in
  // those index cells' parents.  Since E was short in each parent cell, the
  // width of those parent cells is at least some fraction "g" of E's length
  // (as controlled by FLAGS_s2shape_index_cell_size_to_long_edge_ratio).
  // Therefore the minimum width of each cell in S is also at least some
  // fraction of E's length (i.e., g / 2).  This implies that there are at most
  // a constant number c1 of such cells, since they all intersect E and do not
  // overlap, which means that there are at most (c1 * n) short entries in
  // total.
  //
  // With index_cell_size_to_long_edge_ratio = 1.0 (the default value), it can
  // be shown that c1 = 10.  In other words, it is not possible for a given
  // edge to intersect more than 10 index cells where it was considered short
  // in those cells' parents.  The value of c1 can be reduced as low c1 = 4 by
  // increasing index_cell_size_to_long_edge_ratio to about 3.1.  (The reason
  // the minimum value is 3.1 rather than 2.0 is that this ratio is defined in
  // terms of the average edge length of cells at a given level, rather than
  // their minimum width, and 2 * (S2::kAvgEdge / S2::kMinWidth) ~= 3.1.)
  //
  // Next we consider the long index entries.  Let c2 be the maximum number of
  // index cells where a given edge E was considered short in those cells'
  // parents.  (Unlike the case above, we do not require that these cells
  // intersect E.)  Because the minimum width of each parent cell is at least
  // some fraction of E's length and the parent cells at a given level do not
  // overlap, there can be at most a small constant number of index cells at
  // each level where E is considered short in those cells' parents.  For
  // example, consider a very short edge E that intersects the midpoint of a
  // cell edge at level 0.  There are 16 cells at level 30 where E was
  // considered short in the parent cell, 12 cells at each of levels 29..2, and
  // 4 cells at levels 1 and 0 (pretending that all 6 face cells share a common
  // "parent").  This yields a total of c2 = 360 index cells.  This is actually
  // the worst case for index_cell_size_to_long_edge_ratio >= 3.1; with the
  // default value of 1.0 it is possible to have a few more index cells at
  // levels 29 and 30, for a maximum of c2 = 366 index cells.
  //
  // The code below subdivides a given cell only if
  //
  //     s > f * (s + l)
  //
  // where "f" is the min_short_edge_fraction parameter, "s" is the number of
  // short edges that intersect the cell, and "l" is the number of long edges
  // that intersect the cell plus an upper bound on the number of shapes that
  // contain the entire cell.  (It is an upper bound rather than an exact count
  // because we use the number of shapes that contain an arbitrary vertex of
  // the cell.)  Note that the number of long index entries in each child of
  // this cell is at most "l" because no child intersects more edges than its
  // parent or is entirely contained by more shapes than its parent.
  //
  // The inequality above can be rearranged to give
  //
  //    l < s * (1 - f) / f
  //
  // This says that each long index entry in a child cell can be associated
  // with at most (1 - f) / f edges that were considered short when the parent
  // cell was subdivided.  Furthermore we know that there are at most c2 index
  // cells where a given edge was considered short in the parent cell.  Since
  // there are only n edges in total, this means that the maximum number of
  // long index entries is at most
  //
  //    c2 * (1 - f) / f * n
  //
  // and putting this together with the result for short index entries gives
  // the desired bound.
  //
  // There are a variety of ways to make this bound tighter, e.g. when "n" is
  // relatively small.  For example when the indexed geometry satisfies the
  // requirements of S2BooleanOperation (i.e., shape interiors are disjoint)
  // and the min_short_edge_fraction parameter is not too large, then the
  // constant c2 above is only about half as big (i.e., c2 ~= 180).  This is
  // because the worst case under these circumstances requires having many
  // shapes whose interiors overlap.

  // Continue subdividing if the proposed index cell would contain too many
  // edges that are "short" relative to its size (as controlled by the
  // FLAGS_s2shape_index_cell_size_to_long_edge_ratio parameter).  Usually "too
  // many" means more than options_.max_edges_per_cell(), but this value might
  // be increased if the cell has a lot of long edges and/or containing shapes.
  // This strategy ensures that the total index size is linear (see above).
  if (edges.size() > options_.max_edges_per_cell()) {
    int max_short_edges =
        max(options_.max_edges_per_cell(),
            static_cast<int>(
                absl::GetFlag(FLAGS_s2shape_index_min_short_edge_fraction) *
                (edges.size() + tracker->shape_ids().size())));
    int count = 0;
    for (const ClippedEdge* edge : edges) {
      count += (pcell.level() < edge->face_edge->max_level);
      if (count > max_short_edges) return false;
    }
  }

  // Possible optimization: Continue subdividing as long as exactly one child
  // of "pcell" intersects the given edges.  This can be done by finding the
  // bounding box of all the edges and calling ShrinkToFit():
  //
  // S2CellId cellid = pcell.ShrinkToFit(GetRectBound(edges));
  //
  // Currently this is not beneficial; it slows down construction by 4-25%
  // (mainly computing the union of the bounding rectangles) and also slows
  // down queries (since more recursive clipping is required to get down to
  // the level of a spatial index cell).  But it may be worth trying again
  // once "contains_center" is computed and all algorithms are modified to
  // take advantage of it.

  // We update the InteriorTracker as follows.  For every S2Cell in the index
  // we construct two edges: one edge from entry vertex of the cell to its
  // center, and one from the cell center to its exit vertex.  Here "entry"
  // and "exit" refer the S2CellId ordering, i.e. the order in which points
  // are encountered along the S2 space-filling curve.  The exit vertex then
  // becomes the entry vertex for the next cell in the index, unless there are
  // one or more empty intervening cells, in which case the InteriorTracker
  // state is unchanged because the intervening cells have no edges.

  // Shift the InteriorTracker focus point to the center of the current cell.
  if (tracker->is_active() && !edges.empty()) {
    if (!tracker->at_cellid(pcell.id())) {
      tracker->MoveTo(pcell.GetEntryVertex());
    }
    tracker->DrawTo(pcell.GetCenter());
    TestAllEdges(edges, tracker);
  }
  // Allocate and fill a new index cell.  To get the total number of shapes we
  // need to merge the shapes associated with the intersecting edges together
  // with the shapes that happen to contain the cell center.
  const ShapeIdSet& cshape_ids = tracker->shape_ids();
  int num_shapes = CountShapes(edges, cshape_ids);
  S2ShapeIndexCell* cell = new S2ShapeIndexCell;
  S2ClippedShape* base = cell->add_shapes(num_shapes);

  // To fill the index cell we merge the two sources of shapes: "edge shapes"
  // (those that have at least one edge that intersects this cell), and
  // "containing shapes" (those that contain the cell center).  We keep track
  // of the index of the next intersecting edge and the next containing shape
  // as we go along.  Both sets of shape ids are already sorted.
  int enext = 0;
  ShapeIdSet::const_iterator cnext = cshape_ids.begin();
  for (int i = 0; i < num_shapes; ++i) {
    S2ClippedShape* clipped = base + i;
    int eshape_id = num_shape_ids(), cshape_id = eshape_id;  // Sentinels
    if (enext != edges.size()) {
      eshape_id = edges[enext]->face_edge->shape_id;
    }
    if (cnext != cshape_ids.end()) {
      cshape_id = *cnext;
    }
    int ebegin = enext;
    if (cshape_id < eshape_id) {
      // The entire cell is in the shape interior.
      clipped->Init(cshape_id, 0);
      clipped->set_contains_center(true);
      ++cnext;
    } else {
      // Count the number of edges for this shape and allocate space for them.
      while (enext < edges.size() &&
             edges[enext]->face_edge->shape_id == eshape_id) {
        ++enext;
      }
      clipped->Init(eshape_id, enext - ebegin);
      for (int e = ebegin; e < enext; ++e) {
        clipped->set_edge(e - ebegin, edges[e]->face_edge->edge_id);
      }
      if (cshape_id == eshape_id) {
        clipped->set_contains_center(true);
        ++cnext;
      }
    }
  }
  // UpdateEdges() visits cells in increasing order of S2CellId, so during
  // initial construction of the index all insertions happen at the end.  It
  // is much faster to give an insertion hint in this case.  Otherwise the
  // hint doesn't do much harm.  With more effort we could provide a hint even
  // during incremental updates, but this is probably not worth the effort.
  cell_map_.insert(cell_map_.end(), make_pair(pcell.id(), cell));

  // Shift the InteriorTracker focus point to the exit vertex of this cell.
  if (tracker->is_active() && !edges.empty()) {
    tracker->DrawTo(pcell.GetExitVertex());
    TestAllEdges(edges, tracker);
    tracker->set_next_cellid(pcell.id().next());
  }
  return true;
}

// Call tracker->TestEdge() on all edges from shapes that have interiors.
/* static */
void MutableS2ShapeIndex::TestAllEdges(const vector<const ClippedEdge*>& edges,
                                       InteriorTracker* tracker) {
  for (const ClippedEdge* edge : edges) {
    const FaceEdge* face_edge = edge->face_edge;
    if (face_edge->has_interior) {
      tracker->TestEdge(face_edge->shape_id, face_edge->edge);
    }
  }
}

// Return the number of distinct shapes that are either associated with the
// given edges, or that are currently stored in the InteriorTracker.
/* static */
int MutableS2ShapeIndex::CountShapes(const vector<const ClippedEdge*>& edges,
                                     const ShapeIdSet& cshape_ids) {
  int count = 0;
  int last_shape_id = -1;
  ShapeIdSet::const_iterator cnext = cshape_ids.begin();  // Next shape
  for (const ClippedEdge* edge : edges) {
    if (edge->face_edge->shape_id != last_shape_id) {
      ++count;
      last_shape_id = edge->face_edge->shape_id;
      // Skip over any containing shapes up to and including this one,
      // updating "count" appropriately.
      for (; cnext != cshape_ids.end(); ++cnext) {
        if (*cnext > last_shape_id) break;
        if (*cnext < last_shape_id) ++count;
      }
    }
  }
  // Count any remaining containing shapes.
  count += (cshape_ids.end() - cnext);
  return count;
}

size_t MutableS2ShapeIndex::SpaceUsed() const {
  size_t size = sizeof(*this);
  size += shapes_.capacity() * sizeof(unique_ptr<S2Shape>);
  // cell_map_ itself is already included in sizeof(*this).
  size += cell_map_.bytes_used() - sizeof(cell_map_);
  size += cell_map_.size() * sizeof(S2ShapeIndexCell);
  Iterator it;
  for (it.InitStale(this, S2ShapeIndex::BEGIN); !it.done(); it.Next()) {
    const S2ShapeIndexCell& cell = it.cell();
    size += cell.shapes_.capacity() * sizeof(S2ClippedShape);
    for (int s = 0; s < cell.num_clipped(); ++s) {
      const S2ClippedShape& clipped = cell.clipped(s);
      if (!clipped.is_inline()) {
        size += clipped.num_edges() * sizeof(int32);
      }
    }
  }
  if (pending_removals_ != nullptr) {
    size += sizeof(*pending_removals_);
    size += pending_removals_->capacity() * sizeof(RemovedShape);
    for (const RemovedShape& removed : *pending_removals_) {
      size += removed.edges.capacity() * sizeof(S2Shape::Edge);
    }
  }
  return size;
}

void MutableS2ShapeIndex::Encode(Encoder* encoder) const {
  // The version number is encoded in 2 bits, under the assumption that by the
  // time we need 5 versions the first version can be permanently retired.
  // This only saves 1 byte, but that's significant for very small indexes.
  encoder->Ensure(Varint::kMax64);
  uint64 max_edges = options_.max_edges_per_cell();
  encoder->put_varint64(max_edges << 2 | kCurrentEncodingVersionNumber);

  // The index will be built anyway when we iterate through it, but building
  // it in advance lets us size the cell_ids vector correctly.
  ForceBuild();
  vector<S2CellId> cell_ids;
  cell_ids.reserve(cell_map_.size());
  s2coding::StringVectorEncoder encoded_cells;
  for (Iterator it(this, S2ShapeIndex::BEGIN); !it.done(); it.Next()) {
    cell_ids.push_back(it.id());
    it.cell().Encode(num_shape_ids(), encoded_cells.AddViaEncoder());
  }
  s2coding::EncodeS2CellIdVector(cell_ids, encoder);
  encoded_cells.Encode(encoder);
}

bool MutableS2ShapeIndex::Init(Decoder* decoder,
                               const ShapeFactory& shape_factory) {
  Clear();
  uint64 max_edges_version;
  if (!decoder->get_varint64(&max_edges_version)) return false;
  int version = max_edges_version & 3;
  if (version != kCurrentEncodingVersionNumber) return false;
  options_.set_max_edges_per_cell(max_edges_version >> 2);
  uint32 num_shapes = shape_factory.size();
  shapes_.reserve(num_shapes);
  for (int shape_id = 0; shape_id < num_shapes; ++shape_id) {
    auto shape = shape_factory[shape_id];
    if (shape) shape->id_ = shape_id;
    shapes_.push_back(std::move(shape));
  }

  s2coding::EncodedS2CellIdVector cell_ids;
  s2coding::EncodedStringVector encoded_cells;
  if (!cell_ids.Init(decoder)) return false;
  if (!encoded_cells.Init(decoder)) return false;

  for (int i = 0; i < cell_ids.size(); ++i) {
    S2CellId id = cell_ids[i];
    S2ShapeIndexCell* cell = new S2ShapeIndexCell;
    Decoder decoder = encoded_cells.GetDecoder(i);
    if (!cell->Decode(num_shapes, &decoder)) return false;
    cell_map_.insert(cell_map_.end(), make_pair(id, cell));
  }
  return true;
}
