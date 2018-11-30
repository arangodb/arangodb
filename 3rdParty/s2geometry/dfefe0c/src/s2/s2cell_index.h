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

#ifndef S2_S2CELL_INDEX_H_
#define S2_S2CELL_INDEX_H_

#include <vector>
#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"

// S2CellIndex stores a collection of (cell_id, label) pairs.  The S2CellIds
// may be overlapping or contain duplicate values.  For example, an
// S2CellIndex could store a collection of S2CellUnions, where each
// S2CellUnion has its own label.
//
// Labels are 32-bit non-negative integers, and are typically used to map the
// results of queries back to client data structures.  Labels other than
// integers can be supported by using a ValueLexicon, which maintains a set of
// distinct labels and maps them to sequentially numbered integers.  For
// example, the following code uses strings as labels:
//
//   ValueLexicon<string> my_label_lexicon;
//   string label_str = ...;
//   cell_index.Add(cell_id, my_label_lexicon.Add(label_str));
//   ...
//   int32 label = ...;
//   string label_str = my_label_lexicon.value(label);
//
// To build an S2CellIndex, call Add() for each (cell_id, label) pair, and
// then call the Build() method.  For example:
//
//   vector<S2CellId> contents = ...;
//   for (int i = 0; i < contents.size(); ++i) {
//     index.Add(contents[i], i /*label*/);
//   }
//   index.Build();
//
// There is also a convenience method that adds an S2CellUnion:
//
//     index.Add(cell_union, label);
//
// Note that the index is not dynamic; the contents of the index cannot be
// changed once it has been built.
//
// There are several options for retrieving data from the index.  The simplest
// is to use a built-in method such as GetIntersectingLabels (which returns
// the labels of all cells that intersect a given target S2CellUnion):
//
//   vector<Label> labels = index.GetIntersectingLabels(target_union);
//
// Alternatively, you can use an external class such as S2ClosestCellQuery,
// which computes the cell(s) that are closest to a given target geometry.
// For example, here is how to find all cells that are closer than
// "distance_limit" to a given target point:
//
//   S2ClosestCellQuery query(&index);
//   query.mutable_options()->set_max_distance(distance_limit);
//   S2ClosestCellQuery::PointTarget target(target_point);
//   for (const auto& result : query.FindClosestCells(&target)) {
//     // result.distance() is the distance to the target.
//     // result.cell_id() is the indexed S2CellId.
//     // result.label() is the integer label associated with the S2CellId.
//     DoSomething(target_point, result);
//   }
//
// Finally, you can access the index contents directly.  Internally, the index
// consists of a set of non-overlapping leaf cell ranges that subdivide the
// sphere and such that each range intersects a particular set of (cell_id,
// label) pairs.  Data is accessed using the following iterator types:
//
//   RangeIterator:
//    - used to seek and iterate over the non-overlapping leaf cell ranges.
//   NonEmptyRangeIterator:
//    - like RangeIterator, but skips ranges whose contents are empty.
//   ContentsIterator:
//    - iterates over the (cell_id, label) pairs that intersect a given range.
//   CellIterator:
//    - iterates over the entire set of (cell_id, label) pairs.
//
// Note that these are low-level, efficient types intended mainly for
// implementing new query classes.  Most clients should use either the
// built-in methods such as VisitIntersectingCells and GetIntersectingLabels,
// or a helper such as S2ClosestCellQuery or S2Closest*Query::CellUnionTarget.
class S2CellIndex {
 public:
  // Labels are 32-bit non-negative integers.  To support other label types,
  // you can use ValueLexicon to map label values to integers:
  //
  //   ValueLexicon<MyLabel> my_label_lexicon;
  //   index.Add(cell_id, my_label_lexicon.Add(label));
  using Label = int32;

  // Convenience class that represents a (cell_id, label) pair.
  struct LabelledCell {
    S2CellId cell_id;
    Label label;

    LabelledCell() : cell_id(S2CellId::None()), label(-1) {}

    LabelledCell(S2CellId _cell_id, Label _label)
        : cell_id(_cell_id), label(_label) {}

    bool operator==(LabelledCell y) const {
      return cell_id == y.cell_id && label == y.label;
    }

    bool operator<(LabelledCell y) const {
      if (cell_id < y.cell_id) return true;
      if (y.cell_id < cell_id) return false;
      return label < y.label;
    }
  };

  // Default constructor.
  S2CellIndex();

  // Returns the number of (cell_id, label) pairs in the index.
  int num_cells() const;

  // Adds the given (cell_id, label) pair to the index.  Note that the index
  // is not valid until Build() is called.
  //
  // The S2CellIds in the index may overlap (including duplicate values).
  // Duplicate (cell_id, label) pairs are also allowed, although be aware that
  // S2ClosestCellQuery will eliminate such duplicates anyway.
  //
  // REQUIRES: cell_id.is_valid()
  void Add(S2CellId cell_id, Label label);

  // Convenience function that adds a collection of cells with the same label.
  void Add(const S2CellUnion& cell_ids, Label label);

  // Constructs the index.  This method may only be called once.  No iterators
  // may be used until the index is built.
  void Build();

  // Clears the index so that it can be re-used.
  void Clear();

  // A function that is called with each (cell_id, label) pair to be visited.
  // The function may return false in order to indicate that no further
  // (cell_id, label) pairs are needed.
  using CellVisitor = std::function<bool (S2CellId cell_id, Label label)>;

  // Visits all (cell_id, label) pairs in the given index that intersect the
  // given S2CellUnion "target", terminating early if the given CellVisitor
  // function returns false (in which case VisitIntersectingCells returns false
  // as well).  Each (cell_id, label) pair in the index is visited at most
  // once.  (If the index contains duplicates, then each copy is visited.)
  bool VisitIntersectingCells(const S2CellUnion& target,
                              const CellVisitor& visitor) const;

  // Convenience function that returns the labels of all indexed cells that
  // intersect the given S2CellUnion "target".  The output contains each label
  // at most once, but is not sorted.
  std::vector<Label> GetIntersectingLabels(const S2CellUnion& target) const;

  // This version can be more efficient when it is called many times, since it
  // does not require allocating a new vector on each call.
  void GetIntersectingLabels(const S2CellUnion& target,
                             std::vector<Label>* labels) const;

 private:
  // Represents a node in the set of non-overlapping leaf cell ranges.
  struct RangeNode;

  // A special label indicating that ContentsIterator::done() is true.
  static Label constexpr kDoneContents = -1;

  // Represents a node in the (cell_id, label) tree.  Cells are organized in a
  // tree such that the ancestors of a given node contain that node.
  struct CellNode {
    S2CellId cell_id;
    Label label;
    int32 parent;

    CellNode(S2CellId _cell_id, Label _label, int32 _parent)
        : cell_id(_cell_id), label(_label), parent(_parent) {
    }
    CellNode() : cell_id(S2CellId::None()), label(kDoneContents), parent(-1) {}
  };

 public:
  class ContentsIterator;

  // An iterator that visits the entire set of indexed (cell_id, label) pairs
  // in an unspecified order.
  class CellIterator {
   public:
    // Initializes a CellIterator for the given S2CellIndex, positioned at the
    // first cell (if any).
    explicit CellIterator(const S2CellIndex* index);

    // The S2CellId of the current (cell_id, label) pair.
    // REQUIRES: !done()
    S2CellId cell_id() const;

    // The Label of the current (cell_id, label) pair.
    // REQUIRES: !done()
    Label label() const;

    // Returns the current (cell_id, label) pair.
    LabelledCell labelled_cell() const;

    // Returns true if all (cell_id, label) pairs have been visited.
    bool done() const;

    // Advances the iterator to the next (cell_id, label) pair.
    // REQUIRES: !done()
    void Next();

   private:
    // NOTE(ericv): There is a potential optimization that would require this
    // class to iterate over both cell_tree_ *and* range_nodes_.
    std::vector<CellNode>::const_iterator cell_it_, cell_end_;
  };

  // An iterator that seeks and iterates over a set of non-overlapping leaf
  // cell ranges that cover the entire sphere.  The indexed (s2cell_id, label)
  // pairs that intersect the current leaf cell range can be visited using
  // ContentsIterator (see below).
  class RangeIterator {
   public:
    // Initializes a RangeIterator for the given S2CellIndex.  The iterator is
    // initially *unpositioned*; you must call a positioning method such as
    // Begin() or Seek() before accessing its contents.
    explicit RangeIterator(const S2CellIndex* index);

    // The start of the current range of leaf S2CellIds.
    //
    // If done() is true, returns S2CellId::End(S2CellId::kMaxLevel).  This
    // property means that most loops do not need to test done() explicitly.
    S2CellId start_id() const;

    // The (non-inclusive) end of the current range of leaf S2CellIds.
    // REQUIRES: !done()
    S2CellId limit_id() const;

    // Returns true if the iterator is positioned beyond the last valid range.
    bool done() const;

    // Positions the iterator at the first range of leaf cells (if any).
    void Begin();

    // Positions the iterator so that done() is true.
    void Finish();

    // Advances the iterator to the next range of leaf cells.
    // REQUIRES: !done()
    void Next();

    // If the iterator is already positioned at the beginning, returns false.
    // Otherwise positions the iterator at the previous entry and returns true.
    bool Prev();

    // Positions the iterator at the first range with start_id() >= target.
    // (Such an entry always exists as long as "target" is a valid leaf cell.
    // Note that it is valid to access start_id() even when done() is true.)
    //
    // REQUIRES: target.is_leaf()
    void Seek(S2CellId target);

    // Returns true if no (s2cell_id, label) pairs intersect this range.
    // Also returns true if done() is true.
    bool is_empty() const;

    // If advancing the iterator "n" times would leave it positioned on a
    // valid range, does so and returns true.  Otherwise leaves the iterator
    // unmodified and returns false.
    bool Advance(int n);

   private:
    // A special value used to indicate that the RangeIterator has not yet
    // been initialized by calling Begin() or Seek().
    std::vector<RangeNode>::const_iterator kUninitialized() const {
      // Note that since the last element of range_nodes_ is a sentinel value,
      // it_ will never legitimately be positioned at range_nodes_->end().
      return range_nodes_->end();
    }

    friend class ContentsIterator;
    const std::vector<RangeNode>* range_nodes_;
    std::vector<RangeNode>::const_iterator it_;
  };

  // Like RangeIterator, but only visits leaf cell ranges that overlap at
  // least one (cell_id, label) pair.
  class NonEmptyRangeIterator : public RangeIterator {
   public:
    // Initializes a NonEmptyRangeIterator for the given S2CellIndex.
    // The iterator is initially *unpositioned*; you must call a positioning
    // method such as Begin() or Seek() before accessing its contents.
    explicit NonEmptyRangeIterator(const S2CellIndex* index);

    // Positions the iterator at the first non-empty range of leaf cells.
    void Begin();

    // Advances the iterator to the next non-empty range of leaf cells.
    // REQUIRES: !done()
    void Next();

    // If the iterator is already positioned at the beginning, returns false.
    // Otherwise positions the iterator at the previous entry and returns true.
    bool Prev();

    // Positions the iterator at the first non-empty range with
    // start_id() >= target.
    //
    // REQUIRES: target.is_leaf()
    void Seek(S2CellId target);
  };

  // An iterator that visits the (cell_id, label) pairs that cover a set of
  // leaf cell ranges (see RangeIterator).  Note that when multiple leaf cell
  // ranges are visited, this class only guarantees that each result will be
  // reported at least once, i.e. duplicate values may be suppressed.  If you
  // want duplicate values to be reported again, be sure to call Clear() first.
  //
  // [In particular, the implementation guarantees that when multiple leaf
  // cell ranges are visited in monotonically increasing order, then each
  // (cell_id, label) pair is reported exactly once.]
  class ContentsIterator {
   public:
    // Default constructor; must be followed by a call to Init().
    ContentsIterator();

    // Convenience constructor that calls Init().
    explicit ContentsIterator(const S2CellIndex* index);

    // Initializes the iterator.  Should be followed by a call to UnionWith()
    // to visit the contents of each desired leaf cell range.
    void Init(const S2CellIndex* index);

    // Clears all state with respect to which range(s) have been visited.
    void Clear();

    // Positions the ContentsIterator at the first (cell_id, label) pair that
    // covers the given leaf cell range.  Note that when multiple leaf cell
    // ranges are visited using the same ContentsIterator, duplicate values
    // may be suppressed.  If you don't want this behavior, call Clear() first.
    void StartUnion(const RangeIterator& range);

    // The S2CellId of the current (cell_id, label) pair.
    // REQUIRES: !done()
    S2CellId cell_id() const;

    // The Label of the current (cell_id, label) pair.
    // REQUIRES: !done()
    Label label() const;

    // Returns the current (cell_id, label) pair.
    // REQUIRES: !done()
    LabelledCell labelled_cell() const;

    // Returns true if all (cell_id, label) pairs have been visited.
    bool done() const;

    // Advances the iterator to the next (cell_id, label) pair covered by the
    // current leaf cell range.
    // REQUIRES: !done()
    void Next();

   private:
    // node_.label == kDoneContents indicates that done() is true.
    void set_done() { node_.label = kDoneContents; }

    // A pointer to the cell tree itself (owned by the S2CellIndex).
    const std::vector<CellNode>* cell_tree_;

    // The value of it.start_id() from the previous call to StartUnion().
    // This is used to check whether these values are monotonically
    // increasing.
    S2CellId prev_start_id_;

    // The maximum index within the cell_tree_ vector visited during the
    // previous call to StartUnion().  This is used to eliminate duplicate
    // values when StartUnion() is called multiple times.
    int32 node_cutoff_;

    // The maximum index within the cell_tree_ vector visited during the
    // current call to StartUnion().  This is used to update node_cutoff_.
    int32 next_node_cutoff_;

    // A copy of the current node in the cell tree.
    CellNode node_;
  };

 private:
  friend class CellIterator;
  friend class RangeIterator;
  friend class ContentsIterator;

  // A tree of (cell_id, label) pairs such that if X is an ancestor of Y, then
  // X.cell_id contains Y.cell_id.  The contents of a given range of leaf
  // cells can be represented by pointing to a node of this tree.
  std::vector<CellNode> cell_tree_;

  // A RangeNode represents a range of leaf S2CellIds.  The range starts at
  // "start_id" (a leaf cell) and ends at the "start_id" field of the next
  // RangeNode.  "contents" points to the node of cell_tree_ representing the
  // cells that overlap this range.
  struct RangeNode {
    S2CellId start_id;  // First leaf cell contained by this range.
    int32 contents;     // Contents of this node (an index within cell_tree_).

    RangeNode(S2CellId _start_id, int32 _contents)
        : start_id(_start_id), contents(_contents) {
    }

    // Comparison operator needed for std::upper_bound().
    friend bool operator<(S2CellId x, const RangeNode& y) {
      return x < y.start_id;
    }
  };
  // The last element of range_nodes_ is a sentinel value, which is necessary
  // in order to represent the range covered by the previous element.
  std::vector<RangeNode> range_nodes_;

  S2CellIndex(const S2CellIndex&) = delete;
  void operator=(const S2CellIndex&) = delete;
};
std::ostream& operator<<(std::ostream& os, S2CellIndex::LabelledCell x);

//////////////////   Implementation details follow   ////////////////////


inline S2CellIndex::CellIterator::CellIterator(const S2CellIndex* index)
    : cell_it_(index->cell_tree_.begin()),
      cell_end_(index->cell_tree_.end()) {
  S2_DCHECK(!index->range_nodes_.empty()) << "Call Build() first.";
}

inline S2CellId S2CellIndex::CellIterator::cell_id() const {
  S2_DCHECK(!done());
  return cell_it_->cell_id;
}

inline S2CellIndex::Label S2CellIndex::CellIterator::label() const {
  S2_DCHECK(!done());
  return cell_it_->label;
}

inline S2CellIndex::LabelledCell S2CellIndex::CellIterator::labelled_cell()
    const {
  S2_DCHECK(!done());
  return LabelledCell(cell_it_->cell_id, cell_it_->label);
}

inline bool S2CellIndex::CellIterator::done() const {
  return cell_it_ == cell_end_;
}

inline void S2CellIndex::CellIterator::Next() {
  S2_DCHECK(!done());
  ++cell_it_;
}

inline S2CellIndex::RangeIterator::RangeIterator(const S2CellIndex* index)
    : range_nodes_(&index->range_nodes_), it_() {
  S2_DCHECK(!range_nodes_->empty()) << "Call Build() first.";
  if (google::DEBUG_MODE) it_ = kUninitialized();  // See done().
}

inline S2CellId S2CellIndex::RangeIterator::start_id() const {
  return it_->start_id;
}

inline S2CellId S2CellIndex::RangeIterator::limit_id() const {
  S2_DCHECK(!done());
  return (it_ + 1)->start_id;
}

inline bool S2CellIndex::RangeIterator::done() const {
  S2_DCHECK(it_ != kUninitialized()) << "Call Begin() or Seek() first.";

  // Note that the last element of range_nodes_ is a sentinel value.
  return it_ >= range_nodes_->end() - 1;
}

inline void S2CellIndex::RangeIterator::Begin() {
  it_ = range_nodes_->begin();
}

inline void S2CellIndex::RangeIterator::Finish() {
  // Note that the last element of range_nodes_ is a sentinel value.
  it_ = range_nodes_->end() - 1;
}

inline void S2CellIndex::RangeIterator::Next() {
  S2_DCHECK(!done());
  ++it_;
}

inline bool S2CellIndex::RangeIterator::is_empty() const {
  return it_->contents == kDoneContents;
}

inline bool S2CellIndex::RangeIterator::Advance(int n) {
  // Note that the last element of range_nodes_ is a sentinel value.
  if (it_ + n >= range_nodes_->end() - 1) return false;
  it_ += n;
  return true;
}

inline S2CellIndex::NonEmptyRangeIterator::NonEmptyRangeIterator(
    const S2CellIndex* index)
    : RangeIterator(index) {
}

inline void S2CellIndex::NonEmptyRangeIterator::Begin() {
  RangeIterator::Begin();
  while (is_empty() && !done()) RangeIterator::Next();
}

inline void S2CellIndex::NonEmptyRangeIterator::Next() {
  do {
    RangeIterator::Next();
  } while (is_empty() && !done());
}

inline bool S2CellIndex::NonEmptyRangeIterator::Prev() {
  while (RangeIterator::Prev()) {
    if (!is_empty()) return true;
  }
  // Return the iterator to its original position.
  if (is_empty() && !done()) Next();
  return false;
}

inline void S2CellIndex::NonEmptyRangeIterator::Seek(S2CellId target) {
  RangeIterator::Seek(target);
  while (is_empty() && !done()) RangeIterator::Next();
}

inline bool S2CellIndex::RangeIterator::Prev() {
  if (it_ == range_nodes_->begin()) return false;
  --it_;
  return true;
}

inline S2CellIndex::ContentsIterator::ContentsIterator()
    : cell_tree_(nullptr) {
}

inline S2CellIndex::ContentsIterator::ContentsIterator(
    const S2CellIndex* index) {
    Init(index);
}

inline void S2CellIndex::ContentsIterator::Init(const S2CellIndex* index) {
  cell_tree_ = &index->cell_tree_;
  Clear();
}

inline void S2CellIndex::ContentsIterator::Clear() {
  prev_start_id_ = S2CellId::None();
  node_cutoff_ = -1;
  next_node_cutoff_ = -1;
  set_done();
}

inline S2CellId S2CellIndex::ContentsIterator::cell_id() const {
  S2_DCHECK(!done());
  return node_.cell_id;
}

inline S2CellIndex::Label S2CellIndex::ContentsIterator::label() const {
  S2_DCHECK(!done());
  return node_.label;
}

inline S2CellIndex::LabelledCell S2CellIndex::ContentsIterator::labelled_cell()
    const {
  S2_DCHECK(!done());
  return LabelledCell(node_.cell_id, node_.label);
}

inline bool S2CellIndex::ContentsIterator::done() const {
  return node_.label == kDoneContents;
}

inline void S2CellIndex::ContentsIterator::Next() {
  S2_DCHECK(!done());
  if (node_.parent <= node_cutoff_) {
    // We have already processed this node and its ancestors.
    node_cutoff_ = next_node_cutoff_;
    set_done();
  } else {
    node_ = (*cell_tree_)[node_.parent];
  }
}

inline int S2CellIndex::num_cells() const {
  return cell_tree_.size();
}

inline void S2CellIndex::Add(S2CellId cell_id, Label label) {
  S2_DCHECK(cell_id.is_valid());
  S2_DCHECK_GE(label, 0);
  cell_tree_.push_back(CellNode(cell_id, label, -1));
}

inline void S2CellIndex::Clear() {
  cell_tree_.clear();
  range_nodes_.clear();
}

inline bool S2CellIndex::VisitIntersectingCells(
    const S2CellUnion& target, const CellVisitor& visitor) const {
  if (target.empty()) return true;
  auto it = target.begin();
  ContentsIterator contents(this);
  RangeIterator range(this);
  range.Begin();
  do {
    if (range.limit_id() <= it->range_min()) {
      range.Seek(it->range_min());  // Only seek when necessary.
    }
    for (; range.start_id() <= it->range_max(); range.Next()) {
      for (contents.StartUnion(range); !contents.done(); contents.Next()) {
        if (!visitor(contents.cell_id(), contents.label())) {
          return false;
        }
      }
    }
    // Check whether the next target cell is also contained by the leaf cell
    // range that we just processed.  If so, we can skip over all such cells
    // using binary search.  This speeds up benchmarks by between 2x and 10x
    // when the average number of intersecting cells is small (< 1).
    if (++it != target.end() && it->range_max() < range.start_id()) {
      // Skip to the first target cell that extends past the previous range.
      it = std::lower_bound(it + 1, target.end(), range.start_id());
      if ((it - 1)->range_max() >= range.start_id()) --it;
    }
  } while (it != target.end());
  return true;
}

inline std::ostream& operator<<(std::ostream& os,
                                S2CellIndex::LabelledCell x) {
  return os << "(" << x.cell_id << ", " << x.label << ")";
}

#endif  // S2_S2CELL_INDEX_H_
