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

#include "s2/encoded_s2shape_index.h"

#include <memory>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/mutable_s2shape_index.h"

using absl::make_unique;
using std::unique_ptr;
using std::vector;

bool EncodedS2ShapeIndex::Iterator::Locate(const S2Point& target) {
  return LocateImpl(target, this);
}

EncodedS2ShapeIndex::CellRelation EncodedS2ShapeIndex::Iterator::Locate(
    S2CellId target) {
  return LocateImpl(target, this);
}

unique_ptr<EncodedS2ShapeIndex::IteratorBase>
EncodedS2ShapeIndex::Iterator::Clone() const {
  return make_unique<Iterator>(*this);
}

void EncodedS2ShapeIndex::Iterator::Copy(const IteratorBase& other)  {
  *this = *down_cast<const Iterator*>(&other);
}


S2Shape* EncodedS2ShapeIndex::GetShape(int id) const {
  // This method is called when a shape has not been decoded yet.
  unique_ptr<S2Shape> shape = (*shape_factory_)[id];
  if (shape) shape->id_ = id;
  S2Shape* expected = kUndecodedShape();
  if (shapes_[id].compare_exchange_strong(expected, shape.get(),
                                          std::memory_order_relaxed)) {
    return shape.release();  // Ownership has been transferred to shapes_.
  }
  return shapes_[id].load(std::memory_order_relaxed);
}
inline const S2ShapeIndexCell* EncodedS2ShapeIndex::GetCell(int i) const {
  // This method is called by Iterator::cell() when the cell has not been
  // decoded yet.  For thread safety, we first decode the cell and then assign
  // it atomically using a compare-and-swap operation.
  auto cell = make_unique<S2ShapeIndexCell>();
  Decoder decoder = encoded_cells_.GetDecoder(i);
  if (cell->Decode(num_shape_ids(), &decoder)) {
    S2ShapeIndexCell* expected = nullptr;
    if (cells_[i].compare_exchange_strong(expected, cell.get(),
                                          std::memory_order_relaxed)) {
      return cell.release();  // Ownership has been transferred to cells_.
    }
  }
  return cells_[i].load(std::memory_order_relaxed);
}

const S2ShapeIndexCell* EncodedS2ShapeIndex::Iterator::GetCell() const {
  return index_->GetCell(cell_pos_);
}

EncodedS2ShapeIndex::EncodedS2ShapeIndex() {
}

EncodedS2ShapeIndex::~EncodedS2ShapeIndex() {
  // Optimization notes:
  //
  //  - For large S2ShapeIndexes where very few cells need to be decoded,
  //    initialization/destruction of the cells_ vector can dominate benchmark
  //    times.  (In theory this could also happen with shapes_.)
  //
  //  - Although Minimize() does more than required (by resetting the vector
  //    elements to their default values), this does not affect benchmarks.
  //
  //  - The time required to initialize and destroy cells_ and/or shapes_
  //    could be avoided by using more complex data structures, at the expense
  //    of increasing access times.  One simple idea would be to use
  //    uninitialized memory for the atomic pointers, plus one bit per pointer
  //    indicating whether it has been initialized yet.  This would reduce the
  //    linear initialization/destruction costs by a factor of 64.
  Minimize();
}

bool EncodedS2ShapeIndex::Init(Decoder* decoder,
                               const ShapeFactory& shape_factory) {
  Minimize();
  uint64 max_edges_version;
  if (!decoder->get_varint64(&max_edges_version)) return false;
  int version = max_edges_version & 3;
  if (version != MutableS2ShapeIndex::kCurrentEncodingVersionNumber) {
    return false;
  }
  options_.set_max_edges_per_cell(max_edges_version >> 2);

  // AtomicShape is a subtype of std::atomic<S2Shape*> that changes the
  // default constructor value to kUndecodedShape().  This saves the effort of
  // initializing all the elements twice.
  shapes_ = std::vector<AtomicShape>(shape_factory.size());
  shape_factory_ = shape_factory.Clone();
  if (!cell_ids_.Init(decoder)) return false;

  // The cells_ elements are default-initialized to nullptr.
  cells_ = std::vector<std::atomic<S2ShapeIndexCell*>>(cell_ids_.size());
  return encoded_cells_.Init(decoder);
}

void EncodedS2ShapeIndex::Minimize() {
  for (auto& atomic_shape : shapes_) {
    S2Shape* shape = atomic_shape.load(std::memory_order_relaxed);
    if (shape != kUndecodedShape() && shape != nullptr) {
      atomic_shape.store(kUndecodedShape(), std::memory_order_relaxed);
      delete shape;
    }
  }
  for (auto& atomic_cell : cells_) {
    S2ShapeIndexCell* cell = atomic_cell.load(std::memory_order_relaxed);
    if (cell != nullptr) {
      atomic_cell.store(nullptr, std::memory_order_relaxed);
      delete cell;
    }
  }
}

size_t EncodedS2ShapeIndex::SpaceUsed() const {
  // TODO(ericv): Add SpaceUsed() method to S2Shape base class,and Include
  // memory owned by the allocated S2Shapes (here and in S2ShapeIndex).
  size_t size = sizeof(*this);
  size += shapes_.capacity() * sizeof(std::atomic<S2Shape*>);
  size += cells_.capacity() * sizeof(std::atomic<S2ShapeIndexCell*>);
  return size;
}
