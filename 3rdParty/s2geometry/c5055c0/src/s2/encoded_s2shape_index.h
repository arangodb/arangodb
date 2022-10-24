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

#ifndef S2_ENCODED_S2SHAPE_INDEX_H_
#define S2_ENCODED_S2SHAPE_INDEX_H_

#include "s2/encoded_s2cell_id_vector.h"
#include "s2/encoded_string_vector.h"
#include "s2/mutable_s2shape_index.h"

// EncodedS2ShapeIndex is an S2ShapeIndex implementation that works directly
// with encoded data.  Rather than decoding everything in advance, geometry is
// decoded incrementally (down to individual edges) as needed.  It can be
// initialized from a single block of data in nearly constant time (about 1.3
// microseconds per million edges).  This saves large amounts of memory and is
// also much faster in the common situation where geometric data is loaded
// from somewhere, decoded, and then only a single operation is performed on
// it.  It supports all S2ShapeIndex operations including boolean operations,
// measuring distances, etc.
//
// The speedups can be over 1000x for large geometric objects.  For example
// vertices and 50,000 loops.  If this geometry is represented as an
// S2Polygon, then simply decoding it takes ~250ms and building its internal
// S2ShapeIndex takes a further ~1500ms.  These times are much longer than the
// time needed for many operations, e.g. e.g. measuring the distance from the
// polygon to one of its vertices takes only about 0.001ms.
//
// If the same geometry is represented using EncodedLaxPolygonShape and
// EncodedS2ShapeIndex, initializing the index takes only 0.005ms.  The
// distance measuring operation itself takes slightly longer than before
// (0.0013ms vs. the original 0.001ms) but the overall time is now much lower
// (~0.007ms vs. 1750ms).  This is possible because the new classes decode
// data lazily (only when it is actually needed) and with fine granularity
// (down to the level of individual edges).  The overhead associated with this
// incremental decoding is small; operations are typically 25% slower compared
// to fully decoding the MutableS2ShapeIndex and its underlying shapes.
//
// EncodedS2ShapeIndex also uses less memory than MutableS2ShapeIndex.  The
// encoded data is contiguous block of memory that is typically between 4-20%
// of the original index size (see MutableS2ShapeIndex::Encode for examples).
// Constructing the EncodedS2ShapeIndex uses additional memory, but even so
// the total memory usage immediately after construction is typically 25-35%
// of the corresponding MutableS2ShapeIndex size.
//
// Note that MutableS2ShapeIndex will still be faster and use less memory if
// you need to decode the entire index.  Similarly MutableS2ShapeIndex will be
// faster if you plan to execute a large number of operations on it.  The main
// advantage of EncodedS2ShapeIndex is that it is much faster and uses less
// memory when only a small portion of the data needs to be decoded.
//
// Example code showing how to create an encoded index:
//
//   Encoder encoder;
//   s2shapeutil::CompactEncodeTaggedShapes(index, encoder);
//   index.Encode(encoder);
//   string encoded(encoder.base(), encoder.length());  // Encoded data.
//
// Example code showing how to use an encoded index:
//
//   Decoder decoder(encoded.data(), encoded.size());
//   EncodedS2ShapeIndex index;
//   index.Init(&decoder, s2shapeutil::LazyDecodeShapeFactory(&decoder));
//   S2ClosestEdgeQuery query(&index);
//   S2ClosestEdgeQuery::PointTarget target(test_point);
//   if (query.IsDistanceLessOrEqual(&target, limit)) {
//     ...
//   }
//
// Note that EncodedS2ShapeIndex does not make a copy of the encoded data, and
// therefore the client must ensure that this data outlives the
// EncodedS2ShapeIndex object.
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
// - S2ShapeIndexRegion: can be used together with S2RegionCoverer to
//                       approximate geometry as a set of S2CellIds.
//
// - S2ShapeIndexBufferedRegion: computes approximations that have been
//                               expanded by a given radius.
//
// EncodedS2ShapeIndex is thread-compatible, meaning that const methods are
// thread safe, and non-const methods are not thread safe.  The only non-const
// method is Minimize(), so if you plan to call Minimize() while other threads
// are actively using the index that you must use an external reader-writer
// lock such as absl::Mutex to guard access to it.  (There is no global state
// and therefore each index can be guarded independently.)
class EncodedS2ShapeIndex final : public S2ShapeIndex {
 public:
  using Options = MutableS2ShapeIndex::Options;
  using ShapeFactory = S2ShapeIndex::ShapeFactory;

  // Creates an index that must be initialized by calling Init().
  EncodedS2ShapeIndex();

  ~EncodedS2ShapeIndex() override;

  // Initializes the EncodedS2ShapeIndex, returning true on success.
  //
  // This method does not decode the S2Shape objects in the index; this is
  // the responsibility of the client-provided function "shape_factory"
  // (see s2shapeutil_coding.h).  Example usage:
  //
  //   index.Init(decoder, s2shapeutil::LazyDecodeShapeFactory(decoder));
  //
  // Note that the encoded shape vector must *precede* the encoded S2ShapeIndex
  // in the Decoder's data buffer in this example.
  bool Init(Decoder* decoder, const ShapeFactory& shape_factory);

  const Options& options() const { return options_; }

  // The number of distinct shape ids in the index.  This equals the number of
  // shapes in the index provided that no shapes have ever been removed.
  // (Shape ids are not reused.)
  int num_shape_ids() const override { return shapes_.size(); }

  // Return a pointer to the shape with the given id, or nullptr if the shape
  // has been removed from the index.
  S2Shape* shape(int id) const override;

  // Minimizes memory usage by requesting that any data structures that can be
  // rebuilt should be discarded.  This method invalidates all iterators.
  //
  // Like all non-const methods, this method is not thread-safe.
  void Minimize() override;

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
    //   for (EncodedS2ShapeIndex::Iterator it(&index, S2ShapeIndex::BEGIN);
    //        !it.done(); it.Next()) { ... }
    explicit Iterator(const EncodedS2ShapeIndex* index,
                      InitialPosition pos = UNPOSITIONED);

    // Initializes an iterator for the given EncodedS2ShapeIndex.
    void Init(const EncodedS2ShapeIndex* index,
              InitialPosition pos = UNPOSITIONED);

    // Inherited non-virtual methods:
    //   S2CellId id() const;
    //   const S2ShapeIndexCell& cell() const;
    //   bool done() const;
    //   S2Point center() const;

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
    const EncodedS2ShapeIndex* index_;
    int32 cell_pos_;  // Current position in the vector of index cells.
    int32 num_cells_;
  };

  // Returns the number of bytes currently occupied by the index (including any
  // unused space at the end of vectors, etc). It has the same thread safety
  // as the other "const" methods (see introduction).
  size_t SpaceUsed() const override;

 protected:
  std::unique_ptr<IteratorBase> NewIterator(InitialPosition pos) const override;

 private:
  friend class Iterator;

  // Returns a value indicating that a shape has not been decoded yet.
  inline static S2Shape* kUndecodedShape() {
    return reinterpret_cast<S2Shape*>(1);
  }

  // Like std::atomic<S2Shape*>, but defaults to kUndecodedShape().
  class AtomicShape : public std::atomic<S2Shape*> {
   public:
    AtomicShape() : std::atomic<S2Shape*>(kUndecodedShape()) {}
  };

  S2Shape* GetShape(int id) const;
  const S2ShapeIndexCell* GetCell(int i) const;
  bool cell_decoded(int i) const;
  void set_cell_decoded(int i) const;
  int max_cell_cache_size() const;

  std::unique_ptr<ShapeFactory> shape_factory_;

  // The options specified for this index.
  Options options_;

  // A vector containing all shapes in the index.  Initially all shapes are
  // set to kUndecodedShape(); as shapes are decoded, they are added to the
  // vector using std::atomic::compare_exchange_strong.
  mutable std::vector<AtomicShape> shapes_;

  // A vector containing the S2CellIds of each cell in the index.
  s2coding::EncodedS2CellIdVector cell_ids_;

  // A vector containing the encoded contents of each cell in the index.
  s2coding::EncodedStringVector encoded_cells_;

  // A raw array containing the decoded contents of each cell in the index.
  // Initially all values are *uninitialized memory*.  The cells_decoded_
  // field below keeps track of which elements are present.
  mutable std::unique_ptr<S2ShapeIndexCell*[]> cells_;

  // A bit vector indicating which elements of cells_ have been decoded.
  // All other elements of cells_ contain uninitialized (random) memory.
  mutable std::vector<std::atomic<uint64>> cells_decoded_;

  // In order to minimize destructor time when very few cells of a large
  // S2ShapeIndex are needed, we keep track of the indices of the first few
  // cells to be decoded.  This lets us avoid scanning the cells_decoded_
  // vector when the number of cells decoded is very small.
  mutable std::vector<int> cell_cache_;

  // Protects all updates to cells_ and cells_decoded_.
  mutable SpinLock cells_lock_;

  EncodedS2ShapeIndex(const EncodedS2ShapeIndex&) = delete;
  void operator=(const EncodedS2ShapeIndex&) = delete;
};


//////////////////   Implementation details follow   ////////////////////


inline EncodedS2ShapeIndex::Iterator::Iterator() : index_(nullptr) {
}

inline EncodedS2ShapeIndex::Iterator::Iterator(
    const EncodedS2ShapeIndex* index, InitialPosition pos) {
  Init(index, pos);
}

inline void EncodedS2ShapeIndex::Iterator::Init(
    const EncodedS2ShapeIndex* index, InitialPosition pos) {
  index_ = index;
  num_cells_ = index->cell_ids_.size();
  cell_pos_ = (pos == BEGIN) ? 0 : num_cells_;
  Refresh();
}

inline void EncodedS2ShapeIndex::Iterator::Refresh() {
  if (cell_pos_ == num_cells_) {
    set_finished();
  } else {
    // It's faster to initialize the cell to nullptr even if it has already
    // been decoded, since algorithms frequently don't need it (i.e., based on
    // the S2CellId they might not need to look at the cell contents).
    set_state(index_->cell_ids_[cell_pos_], nullptr);
  }
}

inline void EncodedS2ShapeIndex::Iterator::Begin() {
  cell_pos_ = 0;
  Refresh();
}

inline void EncodedS2ShapeIndex::Iterator::Finish() {
  cell_pos_ = num_cells_;
  Refresh();
}

inline void EncodedS2ShapeIndex::Iterator::Next() {
  S2_DCHECK(!done());
  ++cell_pos_;
  Refresh();
}

inline bool EncodedS2ShapeIndex::Iterator::Prev() {
  if (cell_pos_ == 0) return false;
  --cell_pos_;
  Refresh();
  return true;
}

inline void EncodedS2ShapeIndex::Iterator::Seek(S2CellId target) {
  cell_pos_ = index_->cell_ids_.lower_bound(target);
  Refresh();
}

inline std::unique_ptr<EncodedS2ShapeIndex::IteratorBase>
EncodedS2ShapeIndex::NewIterator(InitialPosition pos) const {
  return absl::make_unique<Iterator>(this, pos);
}

inline S2Shape* EncodedS2ShapeIndex::shape(int id) const {
  S2Shape* shape = shapes_[id].load(std::memory_order_acquire);
  if (shape != kUndecodedShape()) return shape;
  return GetShape(id);
}

// Returns true if the given cell has already been decoded.
inline bool EncodedS2ShapeIndex::cell_decoded(int i) const {
  // cell_decoded(i) uses acquire/release synchronization (see .cc file).
  uint64 group_bits = cells_decoded_[i >> 6].load(std::memory_order_acquire);
  return (group_bits & (1ULL << (i & 63))) != 0;
}

// Marks the given cell as having been decoded.
// REQUIRES: cells_lock_ is held
inline void EncodedS2ShapeIndex::set_cell_decoded(int i) const {
  // We use memory_order_release for the store operation below to ensure that
  // cells_decoded(i) sees the most recent value, however we can use
  // memory_order_relaxed for the load because cells_lock_ is held.
  std::atomic<uint64>* group = &cells_decoded_[i >> 6];
  uint64 bits = group->load(std::memory_order_relaxed);
  group->store(bits | 1ULL << (i & 63), std::memory_order_release);
}

inline int EncodedS2ShapeIndex::max_cell_cache_size() const {
  // The cell cache is sized so that scanning decoded_cells_ in the destructor
  // costs about 30 cycles per decoded cell in the worst case.  (This overhead
  // is acceptable relative to the other costs of decoding each cell.)
  //
  // For example, if there are 65,536 cells then we won't need to scan
  // encoded_cells_ unless we decode at least (65536/2048) == 32 cells.  It
  // takes about 1 cycle per 64 cells to scan encoded_cells_, so that works
  // out to (65536/64) == 1024 cycles.  However this cost is amortized over
  // the 32 cells decoded, which works out to 32 cycles per cell.
  return cell_ids_.size() >> 11;
}

#endif  // S2_ENCODED_S2SHAPE_INDEX_H_
