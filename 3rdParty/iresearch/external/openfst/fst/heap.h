
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2005-2010 Google, Inc.
// All Rights Reserved.
// Author: Johan Schalkwyk (johans@google.com)
//
// \file
// Implementation of a heap as in STL, but allows tracking positions
// in heap using a key. The key can be used to do an in-place update of
// values in the heap.

#ifndef FST_LIB_HEAP_H__
#define FST_LIB_HEAP_H__

#include <vector>
using std::vector;
#include <functional>

#include <fst/compat.h>
namespace fst {

//
// \class Heap
// \brief A templated heap implementation that support in-place update
//        of values.
//
// The templated heap implementation is a little different from the
// STL priority_queue and the *_heap operations in STL. This heap
// supports indexing of values in the heap via an associated key.
//
// Each value is internally associated with a key which is returned
// to the calling functions on heap insert. This key can be used
// to later update the specific value in the heap.
//
// \param T the element type of the hash, can be POD, Data or Ptr to Data
// \param Compare Comparison class for determiningg min-heapness.
// \param  whether heap top should be max or min element w.r.t. Compare
//

static const int kNoKey = -1;
template <class T, class Compare, bool max>
class Heap {
 public:

  // Initialize with a specific comparator
  Heap(Compare comp) : comp_(comp), size_(0) { }

  // Create a heap with initial size of internal arrays of 0
  Heap() : size_(0) { }

  ~Heap() { }

  // Insert a value into the heap
  int Insert(const T& val) {
    if (size_ < A_.size()) {
      A_[size_] = val;
      pos_[key_[size_]] = size_;
    } else {
      A_.push_back(val);
      pos_.push_back(size_);
      key_.push_back(size_);
    }

    ++size_;
    return Insert(val, size_ - 1);
  }

  // Update a value at position given by the key. The pos array is first
  // indexed by the key. The position gives the position in the heap array.
  // Once we have the position we can then use the standard heap operations
  // to calculate the parent and child positions.
  void Update(int key, const T& val) {
    int i = pos_[key];
    if (Better(val, A_[Parent(i)])) {
      Insert(val, i);
    } else {
      A_[i] = val;
      Heapify(i);
    }
  }

  // Return the greatest (max=true) / least (max=false) value w.r.t.
  // from the heap.
  T Pop() {
    T top = A_[0];

    Swap(0, size_-1);
    size_--;
    Heapify(0);
    return top;
  }

  // Return the greatest (max=true) / least (max=false) value w.r.t.
  // comp object from the heap.
  T Top() const {
    return A_[0];
  }

  // Check if the heap is empty
  bool Empty() const {
    return size_ == 0;
  }

  void Clear() {
    size_ = 0;
  }


  //
  // The following protected routines are used in a supportive role
  // for managing the heap and keeping the heap properties.
  //
 private:
  // Compute left child of parent
  int Left(int i) {
    return 2*(i+1)-1;   // 0 -> 1, 1 -> 3
  }

  // Compute right child of parent
  int Right(int i) {
    return 2*(i+1);     // 0 -> 2, 1 -> 4
  }

  // Given a child compute parent
  int Parent(int i) {
    return (i-1)/2;     // 1 -> 0, 2 -> 0,  3 -> 1,  4-> 1
  }

  // Swap a child, parent. Use to move element up/down tree.
  // Note a little tricky here. When we swap we need to swap:
  //   the value
  //   the associated keys
  //   the position of the value in the heap
  void Swap(int j, int k) {
    int tkey = key_[j];
    pos_[key_[j] = key_[k]] = j;
    pos_[key_[k] = tkey]    = k;

    T val  = A_[j];
    A_[j]  = A_[k];
    A_[k]  = val;
  }

  // Returns the greater (max=true) / least (max=false) of two
  // elements.
  bool Better(const T& x, const T& y) {
    return max ? comp_(y, x) : comp_(x, y);
  }

  // Heapify subtree rooted at index i.
  void Heapify(int i) {
    int l = Left(i);
    int r = Right(i);
    int largest;

    if (l < size_ && Better(A_[l], A_[i]) )
      largest = l;
    else
      largest = i;

    if (r < size_ && Better(A_[r], A_[largest]) )
      largest = r;

    if (largest != i) {
      Swap(i, largest);
      Heapify(largest);
    }
  }


  // Insert (update) element at subtree rooted at index i
  int Insert(const T& val, int i) {
    int p;
    while (i > 0 && !Better(A_[p = Parent(i)], val)) {
      Swap(i, p);
      i = p;
    }

    return key_[i];
  }

 private:
  Compare comp_;

  vector<int> pos_;
  vector<int> key_;
  vector<T>   A_;
  int  size_;

  // DISALLOW_COPY_AND_ASSIGN(Heap);
};

}  // namespace fst

#endif  // FST_LIB_HEAP_H__
