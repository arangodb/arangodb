////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_GRAPH_SHORTEST_PATH_PRIORITY_QUEUE_H
#define ARANGODB_GRAPH_SHORTEST_PATH_PRIORITY_QUEUE_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace graph {

template <typename Key, typename Value, typename Weight>
class ShortestPathPriorityQueue {
  // This class implements a data structure that is a key/value
  // store with the additional property that every Value has a
  // positive Weight (provided by the weight() and setWeight(w)
  // methods), which is a numerical type, and for which operator<
  // is defined. With respect to this weight the data structure
  // is at the same time a priority queue in that it is possible
  // to ask for (one of) the value(s) with the smallest weight and
  // remove this efficiently.
  // All methods only work with pointers to Values for efficiency
  // reasons. This class deletes all Value* that are stored on
  // destruction.
  // The Value type must have a method getKey that returns a Key
  // const&.
  // This data structure makes the following complexity promises
  // (amortized), where n is the number of key/value pairs stored
  // in the queue:
  //   insert:                  O(log(n))   (but see below)
  //   lookup value by key:     O(1)
  //   get smallest:            O(1)
  //   get and erase smallest:  O(log(n))   (but see below)
  //   lower weight by key      O(log(n))   (but see below)
  // Additionally, if we only ever insert pairs whose value is not
  // smaller than any other value that is already in the structure,
  // and if we do not use lower weight by key, then we even get:
  //   insert:                  O(1)
  //   get and erase smallest:  O(1)
  // With the "get and erase smallest" operation one has the option
  // of retaining the erased value in the key/value store. It can then
  // still be looked up but will no longer be considered for the
  // priority queue.

 public:
  ShortestPathPriorityQueue() : _popped(0), _isHeap(false), _maxWeight(0) {}

  ~ShortestPathPriorityQueue() {
    for (Value* v : _heap) {
      delete v;
    }
    for (Value* v : _history) {
      delete v;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief empty
  //////////////////////////////////////////////////////////////////////////////

  bool empty() { return _heap.empty(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief size
  //////////////////////////////////////////////////////////////////////////////

  size_t size() { return _heap.size(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief insert, data will be copied, returns true, if the key did not
  /// yet exist, and false, in which case nothing else is changed.
  //////////////////////////////////////////////////////////////////////////////

  bool insert(Key const& k, Value* v) {
    auto it = _lookup.find(k);
    if (it != _lookup.end()) {
      return false;
    }

    // Are we still in the simple case of a deque?
    if (!_isHeap) {
      Weight w = v->weight();
      if (w < _maxWeight) {
        // Oh dear, we have to upgrade to heap:
        _isHeap = true;
        // fall through intentionally
      } else {
        if (w > _maxWeight) {
          _maxWeight = w;
        }
        _heap.push_back(v);
        try {
          _lookup.insert(std::make_pair(k, static_cast<ssize_t>(_heap.size() - 1 + _popped)));
        } catch (...) {
          _heap.pop_back();
          throw;
        }
        return true;
      }
    }
    // If we get here, we have to insert into a proper binary heap:
    _heap.push_back(v);
    try {
      size_t newpos = _heap.size() - 1;
      _lookup.insert(std::make_pair(k, static_cast<ssize_t>(newpos + _popped)));
      repairUp(newpos);
    } catch (...) {
      _heap.pop_back();
      throw;
    }
    return true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find, note that the resulting pointer is only valid until the
  /// the next modification of the data structure happens (insert or lowerWeight
  /// or popMinimal). The weight in the Value type must not be modified other
  /// than via lowerWeight, otherwise the queue order could be violated.
  //////////////////////////////////////////////////////////////////////////////

  Value* find(Key const& k) const {
    auto it = _lookup.find(k);

    if (it == _lookup.end()) {
      return nullptr;
    }

    if (it->second >= 0) {  // still in the queue
      return _heap.at(static_cast<size_t>(it->second) - _popped);
    } else {  // already in the history
      return _history.at(static_cast<size_t>(-it->second) - 1);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief erase, returns whether the key was found
  //////////////////////////////////////////////////////////////////////////////

  bool lowerWeight(Key const& k, Weight newWeight) {
    if (!_isHeap) {
      _isHeap = true;
    }
    auto it = _lookup.find(k);
    if (it == _lookup.end()) {
      return false;
    }
    if (it->second >= 0) {  // still in the queue
      size_t pos = static_cast<size_t>(it->second) - _popped;
      _heap[pos]->setWeight(newWeight);
      repairUp(pos);
    } else {  // already in the history
      size_t pos = static_cast<size_t>(-it->second) - 1;
      _history[pos]->setWeight(newWeight);
    }
    return true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief getMinimal, note that the resulting pointer is only valid until the
  /// the next modification of the data structure happens (insert or lowerWeight
  /// or popMinimal). The weight in the Value type must not be modified other
  /// than via lowerWeight, otherwise the queue order could be violated.
  //////////////////////////////////////////////////////////////////////////////

  Value* getMinimal() {
    if (_heap.empty()) {
      return nullptr;
    }
    return _heap[0];
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief popMinimal, returns true if something was returned and false
  /// if the structure is empty. Key and Value are stored in k and v.
  /// If keepForLookup is true then the Value is kept for lookup in the
  /// hash table but removed from the priority queue.
  //////////////////////////////////////////////////////////////////////////////

  bool popMinimal(Key& k, Value*& v, bool keepForLookup = false) {
    if (_heap.empty()) {
      return false;
    }
    k = _heap[0]->getKey();
    v = _heap[0];
    if (!_isHeap) {
      auto it = _lookup.find(k);
      TRI_ASSERT(it != _lookup.end());
      if (keepForLookup) {
        _history.push_back(_heap[0]);
        it->second = -static_cast<ssize_t>(_history.size());
        // Note: This is intentionally one too large to shift by 1
      } else {
        _lookup.erase(it);
      }
      _heap.pop_front();
      _popped++;
    } else {
      removeFromHeap(keepForLookup);
    }
    return true;
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief swap, two positions in the heap, adjusts the _lookup table
  //////////////////////////////////////////////////////////////////////////////

  void swap(size_t p, size_t q) {
    Value* v = _heap[p];
    _heap[p] = _heap[q];
    _heap[q] = v;

    // Now fix the lookup:
    Key const& keyp(_heap[p]->getKey());
    auto it = _lookup.find(keyp);
    TRI_ASSERT(it != _lookup.end());
    TRI_ASSERT(it->second - _popped == q);
    it->second = static_cast<ssize_t>(p) + static_cast<ssize_t>(_popped);

    Key const& keyq(_heap[q]->getKey());
    it = _lookup.find(keyq);
    TRI_ASSERT(it != _lookup.end());
    TRI_ASSERT(it->second - _popped == p);
    it->second = static_cast<ssize_t>(q) + static_cast<ssize_t>(_popped);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief parent, find the parent node in heap
  //////////////////////////////////////////////////////////////////////////////

  size_t parent(size_t pos) { return ((pos + 1) >> 1) - 1; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lchild, find the node of the left child in the heap
  //////////////////////////////////////////////////////////////////////////////

  size_t lchild(size_t pos) { return 2 * (pos + 1) - 1; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief rchild, find the node of the right child in the heap
  //////////////////////////////////////////////////////////////////////////////

  size_t rchild(size_t pos) { return 2 * (pos + 1); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief repairUp, fix the heap property between position pos and its parent
  //////////////////////////////////////////////////////////////////////////////

  void repairUp(size_t pos) {
    while (pos > 0) {
      size_t par = parent(pos);
      Weight wpos = _heap[pos]->weight();
      Weight wpar = _heap[par]->weight();
      if (wpos < wpar) {
        swap(pos, par);
        pos = par;
      } else {
        return;
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief repairDown, fix the heap property between position pos and its
  /// children.
  //////////////////////////////////////////////////////////////////////////////

  void repairDown() {
    size_t pos = 0;
    while (pos < _heap.size()) {
      size_t lchi = lchild(pos);
      if (lchi >= _heap.size()) {
        return;
      }
      Weight wpos = _heap[pos]->weight();
      Weight wlchi = _heap[lchi]->weight();
      size_t rchi = rchild(pos);
      if (rchi >= _heap.size()) {
        if (wpos > wlchi) {
          swap(pos, lchi);
        }
        return;
      }
      Weight wrchi = _heap[rchi]->weight();
      if (wlchi <= wrchi) {
        if (wpos <= wlchi) {
          return;
        }
        swap(pos, lchi);
        pos = lchi;
      } else {
        if (wpos <= wrchi) {
          return;
        }
        swap(pos, rchi);
        pos = rchi;
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removeFromHeap, remove first position in the heap
  //////////////////////////////////////////////////////////////////////////////

  void removeFromHeap(bool keepForLookup) {
    auto it = _lookup.find(_heap[0]->getKey());
    TRI_ASSERT(it != _lookup.end());
    if (keepForLookup) {
      _history.push_back(_heap[0]);
      it->second = -static_cast<ssize_t>(_history.size());
      // Note: This is intentionally one too large to shift by 1
    } else {
      _lookup.erase(it);
    }
    if (_heap.size() == 1) {
      _heap.clear();
      _popped = 0;
      _isHeap = false;
      _maxWeight = 0;
      return;
    }
    // Move one in front:
    _heap[0] = _heap.back();
    _heap.pop_back();
    it = _lookup.find(_heap[0]->getKey());
    TRI_ASSERT(it != _lookup.end());
    it->second = static_cast<ssize_t>(_popped);
    repairDown();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _popped, number of elements that have been popped from the
  /// beginning
  /// of the deque, this is necessary to interpret positions stored in the
  /// unordered_map _lookup
  //////////////////////////////////////////////////////////////////////////////

 private:
  size_t _popped;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _lookup, this provides O(1) lookup by Key
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<Key, ssize_t> _lookup;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _isHeap, starts as false, in which case we only use a deque,
  /// if true, then _heap is an actual binary heap and we do no longer modify
  /// _popped.
  //////////////////////////////////////////////////////////////////////////////

  bool _isHeap;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _heap, the actual data
  //////////////////////////////////////////////////////////////////////////////

  std::deque<Value*> _heap;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _maxWeight, the current maximal weight ever seen
  //////////////////////////////////////////////////////////////////////////////

  Weight _maxWeight;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _history, the actual data that is only in the key/value store
  //////////////////////////////////////////////////////////////////////////////

  std::vector<Value*> _history;
};

}  // namespace graph
}  // namespace arangodb
#endif
