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

#include "s2/s2cell_index.h"

using std::vector;

using Label = S2CellIndex::Label;

void S2CellIndex::RangeIterator::Seek(S2CellId target) {
  S2_DCHECK(target.is_leaf());
  it_ = std::upper_bound(range_nodes_->begin(), range_nodes_->end(),
                         target) - 1;
}

void S2CellIndex::ContentsIterator::StartUnion(const RangeIterator& range) {
  if (range.start_id() < prev_start_id_) {
    node_cutoff_ = -1;  // Can't automatically eliminate duplicates.
  }
  prev_start_id_ = range.start_id();

  // TODO(ericv): Since RangeNode only uses 12 of its 16 bytes, we could add a
  // "label" field without using any extra space.  Then we could store a leaf
  // node of cell_tree_ directly in each RangeNode, where the cell_id is
  // implicitly defined as the one that covers the current leaf cell range.
  // This would save quite a bit of space; e.g. if the given cells are
  // non-overlapping, then cell_tree_ would be empty (since every node is a
  // leaf node and could therefore be stored directly in a RangeNode).  It
  // would also be faster because cell_tree_ would rarely be accessed.
  int contents = range.it_->contents;
  if (contents <= node_cutoff_) {
    set_done();
  } else {
    node_ = (*cell_tree_)[contents];
  }

  // When visiting ancestors, we can stop as soon as the node index is smaller
  // than any previously visited node index.  Because indexes are assigned
  // using a preorder traversal, such nodes are guaranteed to have already
  // been reported.
  next_node_cutoff_ = contents;
}

S2CellIndex::S2CellIndex() {
}

void S2CellIndex::Add(const S2CellUnion& cell_ids, Label label) {
  for (S2CellId cell_id : cell_ids) {
    Add(cell_id, label);
  }
}

void S2CellIndex::Build() {
  // To build the cell tree and leaf cell ranges, we maintain a stack of
  // (cell_id, label) pairs that contain the current leaf cell.  This class
  // represents an instruction to push or pop a (cell_id, label) pair.
  //
  // If label >= 0, the (cell_id, label) pair is pushed on the stack.
  // If cell_id == S2CellId::Sentinel(), a pair is popped from the stack.
  // Otherwise the stack is unchanged but a RangeNode is still emitted.
  struct Delta {
    S2CellId start_id, cell_id;
    Label label;

    Delta(S2CellId _start_id, S2CellId _cell_id, Label _label)
        : start_id(_start_id), cell_id(_cell_id), label(_label) {}

    // Deltas are sorted first by start_id, then in reverse order by cell_id,
    // and then by label.  This is necessary to ensure that (1) larger cells
    // are pushed on the stack before smaller cells, and (2) cells are popped
    // off the stack before any new cells are added.
    bool operator<(const Delta& y) const {
      if (start_id < y.start_id) return true;
      if (y.start_id < start_id) return false;
      if (y.cell_id < cell_id) return true;
      if (cell_id < y.cell_id) return false;
      return label < y.label;
    }
  };

  vector<Delta> deltas;
  deltas.reserve(2 * cell_tree_.size() + 2);
  // Create two deltas for each (cell_id, label) pair: one to add the pair to
  // the stack (at the start of its leaf cell range), and one to remove it from
  // the stack (at the end of its leaf cell range).
  for (const CellNode& node : cell_tree_) {
    deltas.push_back(Delta(node.cell_id.range_min(), node.cell_id, node.label));
    deltas.push_back(Delta(node.cell_id.range_max().next(),
                           S2CellId::Sentinel(), -1));
  }
  // We also create two special deltas to ensure that a RangeNode is emitted at
  // the beginning and end of the S2CellId range.
  deltas.push_back(
      Delta(S2CellId::Begin(S2CellId::kMaxLevel), S2CellId::None(), -1));
  deltas.push_back(
      Delta(S2CellId::End(S2CellId::kMaxLevel), S2CellId::None(), -1));
  std::sort(deltas.begin(), deltas.end());

  // Now walk through the deltas to build the leaf cell ranges and cell tree
  // (which is essentially a permanent form of the "stack" described above).
  cell_tree_.clear();
  range_nodes_.reserve(deltas.size());
  int contents = -1;
  for (int i = 0; i < deltas.size(); ) {
    S2CellId start_id = deltas[i].start_id;
    // Process all the deltas associated with the current start_id.
    for (; i < deltas.size() && deltas[i].start_id == start_id; ++i) {
      if (deltas[i].label >= 0) {
        cell_tree_.push_back({deltas[i].cell_id, deltas[i].label, contents});
        contents = cell_tree_.size() - 1;
      } else if (deltas[i].cell_id == S2CellId::Sentinel()) {
        contents = cell_tree_[contents].parent;
      }
    }
    range_nodes_.push_back({start_id, contents});
  }
}

vector<Label> S2CellIndex::GetIntersectingLabels(const S2CellUnion& target)
    const {
  vector<Label> labels;
  GetIntersectingLabels(target, &labels);
  return labels;
}

void S2CellIndex::GetIntersectingLabels(const S2CellUnion& target,
                                        std::vector<Label>* labels) const {
  labels->clear();
  VisitIntersectingCells(target, [labels](S2CellId cell_id, Label label) {
      labels->push_back(label);
      return true;
    });
  std::sort(labels->begin(), labels->end());
  labels->erase(std::unique(labels->begin(), labels->end()), labels->end());
}
