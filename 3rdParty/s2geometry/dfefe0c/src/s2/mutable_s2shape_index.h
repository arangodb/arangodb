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

#ifndef S2_MUTABLE_S2SHAPE_INDEX_H_
#define S2_MUTABLE_S2SHAPE_INDEX_H_

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>


#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/base/mutex.h"
#include "s2/base/spinlock.h"
#include "s2/_fp_contract_off.h"
#include "s2/s2cell_id.h"
#include "s2/s2pointutil.h"
#include "s2/s2shape.h"
#include "s2/s2shape_index.h"
#include "s2/third_party/absl/base/macros.h"
#include "s2/third_party/absl/base/thread_annotations.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/util/gtl/btree_map.h"

// MutableS2ShapeIndex is a class for in-memory indexing of polygonal geometry.
// The objects in the index are known as "shapes", and may consist of points,
// polylines, and/or polygons, possibly overlapping.  The index makes it very
// fast to answer queries such as finding nearby shapes, measuring distances,
// testing for intersection and containment, etc.
//
// MutableS2ShapeIndex allows not only building an index, but also updating it
// incrementally by adding or removing shapes (hence its name).  It is one of
// several implementations of the S2ShapeIndex interface.  MutableS2ShapeIndex
// is designed to be compact; usually it is smaller than the underlying
// geometry being indexed.  It is capable of indexing up to hundreds of
// millions of edges.  The index is also fast to construct.
//
// There are a number of built-in classes that work with S2ShapeIndex objects.
// Generally these classes accept any collection of geometry that can be
// represented by an S2ShapeIndex, i.e. any combination of points, polylines,
// and polygons.  Such classes include:
//
// - S2ContainsPointQuery: returns the shape(s) that contain a given point.
//
// - S2ClosestEdgeQuery: returns the closest edge(s) to a given point, edge,
//                       S2CellId, or S2ShapeIndex.
//
// - S2CrossingEdgeQuery: returns the edge(s) that cross a given edge.
//
// - S2BooleanOperation: computes boolean operations such as union,
//                       and boolean predicates such as containment.
//
// - S2ShapeIndexRegion: computes approximations for a collection of geometry.
//
// - S2ShapeIndexBufferedRegion: computes approximations that have been
//                               expanded by a given radius.
//
// Here is an example showing how to build an index for a set of polygons, and
// then then determine which polygon(s) contain each of a set of query points:
//
//   void TestContainment(const vector<S2Point>& points,
//                        const vector<S2Polygon*>& polygons) {
//     MutableS2ShapeIndex index;
//     for (auto polygon : polygons) {
//       index.Add(absl::make_unique<S2Polygon::Shape>(polygon));
//     }
//     auto query = MakeS2ContainsPointQuery(&index);
//     for (const auto& point : points) {
//       for (S2Shape* shape : query.GetContainingShapes(point)) {
//         S2Polygon* polygon = polygons[shape->id()];
//         ... do something with (point, polygon) ...
//       }
//     }
//   }
//
// This example uses S2Polygon::Shape, which is one example of an S2Shape
// object.  S2Polyline and S2Loop also have nested Shape classes, and there are
// additional S2Shape types defined in *_shape.h.
//
// Internally, MutableS2ShapeIndex is essentially a map from S2CellIds to the
// set of shapes that intersect each S2CellId.  It is adaptively refined to
// ensure that no cell contains more than a small number of edges.
//
// For efficiency, updates are batched together and applied lazily on the
// first subsequent query.  Locking is used to ensure that MutableS2ShapeIndex
// has the same thread-safety properties as "vector": const methods are
// thread-safe, while non-const methods are not thread-safe.  This means that
// if one thread updates the index, you must ensure that no other thread is
// reading or updating the index at the same time.
//
// TODO(ericv): MutableS2ShapeIndex has an Encode() method that allows the
// index to be serialized.  An encoded S2ShapeIndex can be decoded either into
// its original form (MutableS2ShapeIndex) or into an EncodedS2ShapeIndex.
// The key property of EncodedS2ShapeIndex is that it can be constructed
// instantaneously, since the index is kept in its original encoded form.
// Data is decoded only when an operation needs it.  For example, to determine
// which shapes(s) contain a given query point only requires decoding the data
// in the S2ShapeIndexCell that contains that point.
class MutableS2ShapeIndex final : public S2ShapeIndex {
 private:
  using CellMap = gtl::btree_map<S2CellId, S2ShapeIndexCell*>;

 public:
  // Options that affect construction of the MutableS2ShapeIndex.
  class Options {
   public:
    Options();

    // The maximum number of edges per cell.  If a cell has more than this
    // many edges that are not considered "long" relative to the cell size,
    // then it is subdivided.  (Whether an edge is considered "long" is
    // controlled by --s2shape_index_cell_size_to_long_edge_ratio flag.)
    //
    // Values between 10 and 50 represent a reasonable balance between memory
    // usage, construction time, and query time.  Small values make queries
    // faster, while large values make construction faster and use less memory.
    // Values higher than 50 do not save significant additional memory, and
    // query times can increase substantially, especially for algorithms that
    // visit all pairs of potentially intersecting edges (such as polygon
    // validation), since this is quadratic in the number of edges per cell.
    //
    // Note that the *average* number of edges per cell is generally slightly
    // less than half of the maximum value defined here.
    //
    // Defaults to value given by --s2shape_index_default_max_edges_per_cell.
    int max_edges_per_cell() const { return max_edges_per_cell_; }
    void set_max_edges_per_cell(int max_edges_per_cell);

   private:
    int max_edges_per_cell_;
  };

  // Creates a MutableS2ShapeIndex that uses the default option settings.
  // Option values may be changed by calling Init().
  MutableS2ShapeIndex();

  // Create a MutableS2ShapeIndex with the given options.
  explicit MutableS2ShapeIndex(const Options& options);

  ~MutableS2ShapeIndex() override;

  // Initialize a MutableS2ShapeIndex with the given options.  This method may
  // only be called when the index is empty (i.e. newly created or Reset() has
  // just been called).
  void Init(const Options& options);

  const Options& options() const { return options_; }

  // The number of distinct shape ids that have been assigned.  This equals
  // the number of shapes in the index provided that no shapes have ever been
  // removed.  (Shape ids are not reused.)
  int num_shape_ids() const override {
    return static_cast<int>(shapes_.size());
  }

  // Returns a pointer to the shape with the given id, or nullptr if the shape
  // has been removed from the index.
  S2Shape* shape(int id) const override { return shapes_[id].get(); }

  // Minimizes memory usage by requesting that any data structures that can be
  // rebuilt should be discarded.  This method invalidates all iterators.
  //
  // Like all non-const methods, this method is not thread-safe.
  void Minimize() override;

  // Appends an encoded representation of the S2ShapeIndex to "encoder".
  //
  // This method does not encode the S2Shapes in the index; it is the client's
  // responsibility to encode them separately.  For example:
  //
  //   s2shapeutil::CompactEncodeTaggedShapes(index, encoder);
  //   index.Encode(encoder);
  //
  // REQUIRES: "encoder" uses the default constructor, so that its buffer
  //           can be enlarged as necessary by calling Ensure(int).
  void Encode(Encoder* encoder) const;

  // Decodes an S2ShapeIndex, returning true on success.
  //
  // This method does not decode the S2Shape objects in the index; this is
  // the responsibility of the client-provided function "shape_factory"
  // (see s2shapeutil_coding.h).  Example usage:
  //
  //   index.Init(decoder, s2shapeutil::LazyDecodeShapeFactory(decoder));
  //
  // Note that the S2Shape vector must be encoded *before* the S2ShapeIndex in
  // this example.
  bool Init(Decoder* decoder, const ShapeFactory& shape_factory);

  class Iterator final : public IteratorBase {
   public:
    // Default constructor; must be followed by a call to Init().
    Iterator();

    // Constructs an iterator positioned as specified.  By default iterators
    // are unpositioned, since this avoids an extra seek in this situation
    // where one of the seek methods (such as Locate) is immediately called.
    //
    // If you want to position the iterator at the beginning, e.g. in order to
    // loop through the entire index, do this instead:
    //
    //   for (MutableS2ShapeIndex::Iterator it(&index, S2ShapeIndex::BEGIN);
    //        !it.done(); it.Next()) { ... }
    explicit Iterator(const MutableS2ShapeIndex* index,
                      InitialPosition pos = UNPOSITIONED);

    // Initializes an iterator for the given MutableS2ShapeIndex.  This method
    // may also be called in order to restore an iterator to a valid state
    // after the underlying index has been updated (although it is usually
    // easier just to declare a new iterator whenever required, since iterator
    // construction is cheap).
    void Init(const MutableS2ShapeIndex* index,
              InitialPosition pos = UNPOSITIONED);

    // Initialize an iterator for the given MutableS2ShapeIndex without
    // applying any pending updates.  This can be used to observe the actual
    // current state of the index without modifying it in any way.
    void InitStale(const MutableS2ShapeIndex* index,
                   InitialPosition pos = UNPOSITIONED);

    // Inherited non-virtual methods:
    //   S2CellId id() const;
    //   bool done() const;
    //   S2Point center() const;
    const S2ShapeIndexCell& cell() const;

    // IteratorBase API:
    void Begin() override;
    void Finish() override;
    void Next() override;
    bool Prev() override;
    void Seek(S2CellId target) override;
    bool Locate(const S2Point& target) override;
    CellRelation Locate(S2CellId target) override;

   protected:
    const S2ShapeIndexCell* GetCell() const override;
    std::unique_ptr<IteratorBase> Clone() const override;
    void Copy(const IteratorBase& other) override;

   private:
    void Refresh();  // Updates the IteratorBase fields.
    const MutableS2ShapeIndex* index_;
    CellMap::const_iterator iter_, end_;
  };

  // Takes ownership of the given shape and adds it to the index.  Also
  // assigns a unique id to the shape (shape->id()) and returns that id.
  // Shape ids are assigned sequentially starting from 0 in the order shapes
  // are added.  Invalidates all iterators and their associated data.
  int Add(std::unique_ptr<S2Shape> shape);

  // Removes the given shape from the index and return ownership to the caller.
  // Invalidates all iterators and their associated data.
  std::unique_ptr<S2Shape> Release(int shape_id);

  // Resets the index to its original state and returns ownership of all
  // shapes to the caller.  This method is much more efficient than removing
  // all shapes one at a time.
  std::vector<std::unique_ptr<S2Shape>> ReleaseAll();

  // Resets the index to its original state and deletes all shapes.  Any
  // options specified via Init() are preserved.
  void Clear();

  // Returns the number of bytes currently occupied by the index (including any
  // unused space at the end of vectors, etc). It has the same thread safety
  // as the other "const" methods (see introduction).
  size_t SpaceUsed() const override;

  // Calls to Add() and Release() are normally queued and processed on the
  // first subsequent query (in a thread-safe way).  This has many advantages,
  // the most important of which is that sometimes there *is* no subsequent
  // query, which lets us avoid building the index completely.
  //
  // This method forces any pending updates to be applied immediately.
  // Calling this method is rarely a good idea.  (One valid reason is to
  // exclude the cost of building the index from benchmark results.)
  void ForceBuild();

  // Returns true if there are no pending updates that need to be applied.
  // This can be useful to avoid building the index unnecessarily, or for
  // choosing between two different algorithms depending on whether the index
  // is available.
  //
  // The returned index status may be slightly out of date if the index was
  // built in a different thread.  This is fine for the intended use (as an
  // efficiency hint), but it should not be used by internal methods  (see
  // MaybeApplyUpdates).
  bool is_fresh() const;

 protected:
  std::unique_ptr<IteratorBase> NewIterator(InitialPosition pos) const override;

 private:
  friend class EncodedS2ShapeIndex;
  friend class Iterator;
  friend class MutableS2ShapeIndexTest;
  friend class S2Stats;

  struct BatchDescriptor;
  struct ClippedEdge;
  class EdgeAllocator;
  struct FaceEdge;
  class InteriorTracker;
  struct RemovedShape;

  using ShapeIdSet = std::vector<int>;

  // When adding a new encoding, be aware that old binaries will not be able
  // to decode it.
  static const unsigned char kCurrentEncodingVersionNumber = 0;

  // Internal methods are documented with their definitions.
  bool is_first_update() const;
  bool is_shape_being_removed(int shape_id) const;
  void MaybeApplyUpdates() const;
  void ApplyUpdatesThreadSafe();
  void ApplyUpdatesInternal();
  void GetUpdateBatches(std::vector<BatchDescriptor>* batches) const;
  static void GetBatchSizes(int num_items, int max_batches,
                            double final_bytes_per_item,
                            double high_water_bytes_per_item,
                            double preferred_max_bytes_per_batch,
                            std::vector<int>* batch_sizes);
  void ReserveSpace(const BatchDescriptor& batch,
                    std::vector<FaceEdge> all_edges[6]) const;
  void AddShape(int id, std::vector<FaceEdge> all_edges[6],
                InteriorTracker* tracker) const;
  void RemoveShape(const RemovedShape& removed,
                   std::vector<FaceEdge> all_edges[6],
                   InteriorTracker* tracker) const;
  void AddFaceEdge(FaceEdge* edge, std::vector<FaceEdge> all_edges[6]) const;
  void UpdateFaceEdges(int face, const std::vector<FaceEdge>& face_edges,
                       InteriorTracker* tracker);
  S2CellId ShrinkToFit(const S2PaddedCell& pcell, const R2Rect& bound) const;
  void SkipCellRange(S2CellId begin, S2CellId end, InteriorTracker* tracker,
                     EdgeAllocator* alloc, bool disjoint_from_index);
  void UpdateEdges(const S2PaddedCell& pcell,
                   std::vector<const ClippedEdge*>* edges,
                   InteriorTracker* tracker, EdgeAllocator* alloc,
                   bool disjoint_from_index);
  void AbsorbIndexCell(const S2PaddedCell& pcell,
                       const Iterator& iter,
                       std::vector<const ClippedEdge*>* edges,
                       InteriorTracker* tracker,
                       EdgeAllocator* alloc);
  int GetEdgeMaxLevel(const S2Shape::Edge& edge) const;
  static int CountShapes(const std::vector<const ClippedEdge*>& edges,
                         const ShapeIdSet& cshape_ids);
  bool MakeIndexCell(const S2PaddedCell& pcell,
                     const std::vector<const ClippedEdge*>& edges,
                     InteriorTracker* tracker);
  static void TestAllEdges(const std::vector<const ClippedEdge*>& edges,
                           InteriorTracker* tracker);
  inline static const ClippedEdge* UpdateBound(const ClippedEdge* edge,
                                               int u_end, double u,
                                               int v_end, double v,
                                               EdgeAllocator* alloc);
  static const ClippedEdge* ClipUBound(const ClippedEdge* edge,
                                       int u_end, double u,
                                       EdgeAllocator* alloc);
  static const ClippedEdge* ClipVBound(const ClippedEdge* edge,
                                       int v_end, double v,
                                       EdgeAllocator* alloc);
  static void ClipVAxis(const ClippedEdge* edge, const R1Interval& middle,
                        std::vector<const ClippedEdge*> child_edges[2],
                        EdgeAllocator* alloc);

  // The amount by which cells are "padded" to compensate for numerical errors
  // when clipping line segments to cell boundaries.
  static const double kCellPadding;

  // The shapes in the index, accessed by their shape id.  Removed shapes are
  // replaced by nullptr pointers.
  std::vector<std::unique_ptr<S2Shape>> shapes_;

  // A map from S2CellId to the set of clipped shapes that intersect that
  // cell.  The cell ids cover a set of non-overlapping regions on the
  // sphere.  Note that this field is updated lazily (see below).  Const
  // methods *must* call MaybeApplyUpdates() before accessing this field.
  // (The easiest way to achieve this is simply to use an Iterator.)
  CellMap cell_map_;

  // The options supplied for this index.
  Options options_;

  // The id of the first shape that has been queued for addition but not
  // processed yet.
  int pending_additions_begin_ = 0;

  // The representation of an edge that has been queued for removal.
  struct RemovedShape {
    int32 shape_id;
    bool has_interior;  // Belongs to a shape of dimension 2.
    bool contains_tracker_origin;
    std::vector<S2Shape::Edge> edges;
  };

  // The set of shapes that have been queued for removal but not processed
  // yet.  Note that we need to copy the edge data since the caller is free to
  // destroy the shape once Release() has been called.  This field is present
  // only when there are removed shapes to process (to save memory).
  std::unique_ptr<std::vector<RemovedShape>> pending_removals_;

  // Additions and removals are queued and processed on the first subsequent
  // query.  There are several reasons to do this:
  //
  //  - It is significantly more efficient to process updates in batches.
  //  - Often the index will never be queried, in which case we can save both
  //    the time and memory required to build it.  Examples:
  //     + S2Loops that are created simply to pass to an S2Polygon.  (We don't
  //       need the S2Loop index, because S2Polygon builds its own index.)
  //     + Applications that load a database of geometry and then query only
  //       a small fraction of it.
  //     + Applications that only read and write geometry (Decode/Encode).
  //
  // The main drawback is that we need to go to some extra work to ensure that
  // "const" methods are still thread-safe.  Note that the goal is *not* to
  // make this class thread-safe in general, but simply to hide the fact that
  // we defer some of the indexing work until query time.
  //
  // The textbook approach to this problem would be to use a mutex and a
  // condition variable.  Unfortunately pthread mutexes are huge (40 bytes).
  // Instead we use spinlock (which is only 4 bytes) to guard a few small
  // fields representing the current update status, and only create additional
  // state while the update is actually occurring.
  mutable SpinLock lock_;

  enum IndexStatus {
    STALE,     // There are pending updates.
    UPDATING,  // Updates are currently being applied.
    FRESH,     // There are no pending updates.
  };
  // Reads and writes to this field are guarded by "lock_".
  std::atomic<IndexStatus> index_status_;

  // UpdateState holds temporary data related to thread synchronization.  It
  // is only allocated while updates are being applied.
  struct UpdateState {
    // This mutex is used as a condition variable.  It is locked by the
    // updating thread for the entire duration of the update; other threads
    // lock it in order to wait until the update is finished.
    absl::Mutex wait_mutex;

    // The number of threads currently waiting on "wait_mutex_".  The
    // UpdateState can only be freed when this number reaches zero.
    //
    // Reads and writes to this field are guarded by "lock_".
    int num_waiting;

    UpdateState() : num_waiting(0) {
    }

    ~UpdateState() {
      S2_DCHECK_EQ(0, num_waiting);
    }
  };
  std::unique_ptr<UpdateState> update_state_;

  // Documented in the .cc file.
  void UnlockAndSignal()
      UNLOCK_FUNCTION(lock_)
      UNLOCK_FUNCTION(update_state_->wait_mutex);

  MutableS2ShapeIndex(const MutableS2ShapeIndex&) = delete;
  void operator=(const MutableS2ShapeIndex&) = delete;
};


//////////////////   Implementation details follow   ////////////////////


inline MutableS2ShapeIndex::Iterator::Iterator() : index_(nullptr) {
}

inline MutableS2ShapeIndex::Iterator::Iterator(
    const MutableS2ShapeIndex* index, InitialPosition pos) {
  Init(index, pos);
}

inline void MutableS2ShapeIndex::Iterator::Init(
    const MutableS2ShapeIndex* index, InitialPosition pos) {
  index->MaybeApplyUpdates();
  InitStale(index, pos);
}

inline void MutableS2ShapeIndex::Iterator::InitStale(
    const MutableS2ShapeIndex* index, InitialPosition pos) {
  index_ = index;
  end_ = index_->cell_map_.end();
  if (pos == BEGIN) {
    iter_ = index_->cell_map_.begin();
  } else {
    iter_ = end_;
  }
  Refresh();
}

inline const S2ShapeIndexCell& MutableS2ShapeIndex::Iterator::cell() const {
  // Since MutableS2ShapeIndex always sets the "cell_" field, we can skip the
  // logic in the base class that conditionally calls GetCell().
  return *raw_cell();
}

inline void MutableS2ShapeIndex::Iterator::Refresh() {
  if (iter_ == end_) {
    set_finished();
  } else {
    set_state(iter_->first, iter_->second);
  }
}

inline void MutableS2ShapeIndex::Iterator::Begin() {
  // Make sure that the index has not been modified since Init() was called.
  S2_DCHECK(index_->is_fresh());
  iter_ = index_->cell_map_.begin();
  Refresh();
}

inline void MutableS2ShapeIndex::Iterator::Finish() {
  iter_ = end_;
  Refresh();
}

inline void MutableS2ShapeIndex::Iterator::Next() {
  S2_DCHECK(!done());
  ++iter_;
  Refresh();
}

inline bool MutableS2ShapeIndex::Iterator::Prev() {
  if (iter_ == index_->cell_map_.begin()) return false;
  --iter_;
  Refresh();
  return true;
}

inline void MutableS2ShapeIndex::Iterator::Seek(S2CellId target) {
  iter_ = index_->cell_map_.lower_bound(target);
  Refresh();
}

inline std::unique_ptr<MutableS2ShapeIndex::IteratorBase>
MutableS2ShapeIndex::NewIterator(InitialPosition pos) const {
  return absl::make_unique<Iterator>(this, pos);
}

inline bool MutableS2ShapeIndex::is_fresh() const {
  return index_status_.load(std::memory_order_relaxed) == FRESH;
}

// Return true if this is the first update to the index.
inline bool MutableS2ShapeIndex::is_first_update() const {
  // Note that it is not sufficient to check whether cell_map_ is empty, since
  // entries are added during the update process.
  return pending_additions_begin_ == 0;
}

// Given that the given shape is being updated, return true if it is being
// removed (as opposed to being added).
inline bool MutableS2ShapeIndex::is_shape_being_removed(int shape_id) const {
  // All shape ids being removed are less than all shape ids being added.
  return shape_id < pending_additions_begin_;
}

// Ensure that any pending updates have been applied.  This method must be
// called before accessing the cell_map_ field, even if the index_status_
// appears to be FRESH, because a memory barrier is required in order to
// ensure that all the index updates are visible if the updates were done in
// another thread.
inline void MutableS2ShapeIndex::MaybeApplyUpdates() const {
  // To avoid acquiring and releasing the spinlock on every query, we use
  // atomic operations when testing whether the status is FRESH and when
  // updating the status to be FRESH.  This guarantees that any thread that
  // sees a status of FRESH will also see the corresponding index updates.
  if (index_status_.load(std::memory_order_acquire) != FRESH) {
    const_cast<MutableS2ShapeIndex*>(this)->ApplyUpdatesThreadSafe();
  }
}

#endif  // S2_MUTABLE_S2SHAPE_INDEX_H_
