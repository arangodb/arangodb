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

#ifndef ARANGODB_BASICS_SHORTEST_PATH_FINDER_H
#define ARANGODB_BASICS_SHORTEST_PATH_FINDER_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"

#include <deque>
#include <stack>
#include <thread>

namespace arangodb {
namespace basics {

template <typename Key, typename Value, typename Weight>
class PriorityQueue {
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
  PriorityQueue() : _popped(0), _isHeap(false), _maxWeight(0) {}

  ~PriorityQueue() {
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
          _lookup.insert(std::make_pair(
              k, static_cast<ssize_t>(_heap.size() - 1 + _popped)));
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

  Value* find(Key const& k) {
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

template <typename VertexId, typename Path>
class PathFinder {
 protected:
  PathFinder() {
  }
public:

  virtual ~PathFinder() {}

  virtual bool shortestPath(VertexId& start, VertexId& target, Path& result) = 0;
};

template <typename VertexId, typename EdgeId, typename EdgeWeight, typename Path>
class DynamicDistanceFinder : public PathFinder<VertexId, Path> {
 public:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Step, one position with a predecessor and the edge
  //////////////////////////////////////////////////////////////////////////////

  struct Step {
   private:
    EdgeWeight _weight;

   public:
    VertexId _vertex;
    VertexId _predecessor;
    EdgeId _edge;
    bool _done;

    Step() : _done(false) {}

    Step(VertexId const& vert, VertexId const& pred, EdgeWeight weig, EdgeId const& edge)
        : _weight(weig),
          _vertex(vert),
          _predecessor(pred),
          _edge(edge),
          _done(false) {}

    EdgeWeight weight() const { return _weight; }

    void setWeight(EdgeWeight w) { _weight = w;}

    VertexId const& getKey() const { return _vertex; }
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief edge direction
  //////////////////////////////////////////////////////////////////////////////

  typedef enum { FORWARD, BACKWARD } Direction;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief callback to find neighbors
  //////////////////////////////////////////////////////////////////////////////

  typedef std::function<void(VertexId const&, std::vector<Step*>&)>
      ExpanderFunction;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief our specialization of the priority queue
  //////////////////////////////////////////////////////////////////////////////

  typedef arangodb::basics::PriorityQueue<VertexId, Step, EdgeWeight> PQueue;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief information for each thread
  //////////////////////////////////////////////////////////////////////////////

  struct ThreadInfo {
    PQueue _pq;
    arangodb::Mutex _mutex;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a Dijkstra searcher for the multi-threaded search
  //////////////////////////////////////////////////////////////////////////////

  class SearcherTwoThreads {
    DynamicDistanceFinder* _pathFinder;
    ThreadInfo& _myInfo;
    ThreadInfo& _peerInfo;
    VertexId _start;
    ExpanderFunction _expander;
    std::string _id;

   public:
    SearcherTwoThreads(DynamicDistanceFinder* pathFinder, ThreadInfo& myInfo,
                       ThreadInfo& peerInfo, VertexId& start,
                       ExpanderFunction expander, std::string const& id)
        : _pathFinder(pathFinder),
          _myInfo(myInfo),
          _peerInfo(peerInfo),
          _start(std::move(start)),
          _expander(expander),
          _id(id) {}

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Insert a neighbor to the todo list.
    ////////////////////////////////////////////////////////////////////////////////

   private:
    void insertNeighbor(Step* step, EdgeWeight newWeight) {
      MUTEX_LOCKER(locker, _myInfo._mutex);

      Step* s = _myInfo._pq.find(step->_vertex);

      // Not found, so insert it:
      if (s == nullptr) {
        step->setWeight(newWeight);
        _myInfo._pq.insert(step->_vertex, step);
        // step is consumed!
        return;
      }
      if (s->_done) {
        delete step;
        return;
      }
      if (s->weight() > newWeight) {
        s->_predecessor = step->_predecessor;
        s->_edge = step->_edge;
        _myInfo._pq.lowerWeight(s->_vertex, newWeight);
      }
      delete step;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Lookup our current vertex in the data of our peer.
    ////////////////////////////////////////////////////////////////////////////////

    void lookupPeer(VertexId& vertex, EdgeWeight weight) {
      MUTEX_LOCKER(locker, _peerInfo._mutex);

      Step* s = _peerInfo._pq.find(vertex);
      if (s == nullptr) {
        // Not found, nothing more to do
        return;
      }
      EdgeWeight total = s->weight() + weight;

      // Update the highscore:
      MUTEX_LOCKER(resultLocker, _pathFinder->_resultMutex);

      if (!_pathFinder->_highscoreSet || total < _pathFinder->_highscore) {
        _pathFinder->_highscoreSet = true;
        _pathFinder->_highscore = total;
        _pathFinder->_intermediate = vertex;
        _pathFinder->_intermediateSet = true;
      }

      // Now the highscore is set!

      // Did we find a solution together with the other thread?
      if (s->_done) {
        if (total <= _pathFinder->_highscore) {
          _pathFinder->_intermediate = vertex;
          _pathFinder->_intermediateSet = true;
        }
        // Hacki says: If the highscore was set, and even if
        // it is better than total, then this observation here
        // proves that it will never be better, so: BINGO.
        _pathFinder->_bingo = true;
        // We found a way, but somebody else found a better way, so
        // this is not the shortest path
        return;
      }

      // Did we find a solution on our own? This is for the
      // single thread case and for the case that the other
      // thread is too slow to even finish its own start vertex!
      if (s->weight() == 0) {
        // We have found the target, we have finished all
        // vertices with a smaller weight than this one (and did
        // not succeed), so this must be a best solution:
        _pathFinder->_intermediate = vertex;
        _pathFinder->_intermediateSet = true;
        _pathFinder->_bingo = true;
      }
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Search graph starting at Start following edges of the given
    /// direction only
    ////////////////////////////////////////////////////////////////////////////////

    void run() {
      try {
        VertexId v;
        Step* s;
        bool b;
        {
          MUTEX_LOCKER(locker, _myInfo._mutex);
          b = _myInfo._pq.popMinimal(v, s, true);
        }

        std::vector<Step*> neighbors;

        // Iterate while no bingo found and
        // there still is a vertex on the stack.
        while (!_pathFinder->_bingo && b) {
          neighbors.clear();
          _expander(v, neighbors);
          for (auto* neighbor : neighbors) {
            insertNeighbor(neighbor, s->weight() + neighbor->weight());
          }
          lookupPeer(v, s->weight());

          MUTEX_LOCKER(locker, _myInfo._mutex);
          Step* s2 = _myInfo._pq.find(v);
          s2->_done = true;
          b = _myInfo._pq.popMinimal(v, s, true);
        }
        // We can leave this loop only under 2 conditions:
        // 1) already bingo==true => bingo = true no effect
        // 2) This queue is empty => if there would be a
        //    path we would have found it here
        //    => No path possible. Set bingo, intermediate is empty.
        _pathFinder->_bingo = true;
      } catch (arangodb::basics::Exception const& ex) {
        _pathFinder->_resultCode = ex.code();
      } catch (std::bad_alloc const&) {
        _pathFinder->_resultCode = TRI_ERROR_OUT_OF_MEMORY;
      } catch (...) {
        _pathFinder->_resultCode = TRI_ERROR_INTERNAL;
      }
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief start and join functions
    ////////////////////////////////////////////////////////////////////////////////

   public:
    void start() { _thread = std::thread(&SearcherTwoThreads::run, this); }

    void join() { _thread.join(); }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief The thread object.
    ////////////////////////////////////////////////////////////////////////////////

   private:
    std::thread _thread;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a Dijkstra searcher for the single-threaded search
  //////////////////////////////////////////////////////////////////////////////

  class Searcher {
    DynamicDistanceFinder* _pathFinder;
    ThreadInfo& _myInfo;
    ThreadInfo& _peerInfo;
    VertexId _start;
    ExpanderFunction _expander;
    std::string _id;

   public:
    Searcher(DynamicDistanceFinder* pathFinder, ThreadInfo& myInfo, ThreadInfo& peerInfo,
             VertexId& start, ExpanderFunction expander, std::string const& id)
        : _pathFinder(pathFinder),
          _myInfo(myInfo),
          _peerInfo(peerInfo),
          _start(start),
          _expander(expander),
          _id(id) {}

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Insert a neighbor to the todo list.
    ////////////////////////////////////////////////////////////////////////////////

   private:
    void insertNeighbor(Step* step, EdgeWeight newWeight) {
      Step* s = _myInfo._pq.find(step->_vertex);

      // Not found, so insert it:
      if (s == nullptr) {
        step->setWeight(newWeight);
        _myInfo._pq.insert(step->_vertex, step);
        return;
      }
      if (!s->_done && s->weight() > newWeight) {
        s->_predecessor = step->_predecessor;
        s->_edge = step->_edge;
        _myInfo._pq.lowerWeight(s->_vertex, newWeight);
      }
      delete step;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Lookup our current vertex in the data of our peer.
    ////////////////////////////////////////////////////////////////////////////////

    void lookupPeer(VertexId& vertex, EdgeWeight weight) {
      Step* s = _peerInfo._pq.find(vertex);

      if (s == nullptr) {
        // Not found, nothing more to do
        return;
      }
      EdgeWeight total = s->weight() + weight;

      // Update the highscore:
      if (!_pathFinder->_highscoreSet || total < _pathFinder->_highscore) {
        _pathFinder->_highscoreSet = true;
        _pathFinder->_highscore = total;
        _pathFinder->_intermediate = vertex;
        _pathFinder->_intermediateSet = true;
      }

      // Now the highscore is set!

      // Did we find a solution together with the other thread?
      if (s->_done) {
        if (total <= _pathFinder->_highscore) {
          _pathFinder->_intermediate = vertex;
          _pathFinder->_intermediateSet = true;
        }
        // Hacki says: If the highscore was set, and even if
        // it is better than total, then this observation here
        // proves that it will never be better, so: BINGO.
        _pathFinder->_bingo = true;
        // We found a way, but somebody else found a better way,
        // so this is not the shortest path
        return;
      }

      // Did we find a solution on our own? This is for the
      // single thread case and for the case that the other
      // thread is too slow to even finish its own start vertex!
      if (s->weight() == 0) {
        // We have found the target, we have finished all
        // vertices with a smaller weight than this one (and did
        // not succeed), so this must be a best solution:
        _pathFinder->_intermediate = vertex;
        _pathFinder->_intermediateSet = true;
        _pathFinder->_bingo = true;
      }
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Do one step only.
    ////////////////////////////////////////////////////////////////////////////////

   public:
    bool oneStep() {
      VertexId v;
      Step* s = nullptr;
      bool b = _myInfo._pq.popMinimal(v, s, true);

      if (_pathFinder->_bingo || !b) {
        // We can leave this functino only under 2 conditions:
        // 1) already bingo==true => bingo = true no effect
        // 2) This queue is empty => if there would be a
        //    path we would have found it here
        //    => No path possible. Set bingo, intermediate is empty.
        _pathFinder->_bingo = true;
        return false;
      }

      TRI_ASSERT(s != nullptr);

      std::vector<Step*> neighbors;
      _expander(v, neighbors);
      for (Step* neighbor : neighbors) {
        insertNeighbor(neighbor, s->weight() + neighbor->weight());
      }
      lookupPeer(v, s->weight());

      Step* s2 = _myInfo._pq.find(v);
      s2->_done = true;
      return true;
    }
  };

  // -----------------------------------------------------------------------------

  DynamicDistanceFinder(DynamicDistanceFinder const&) = delete;
  DynamicDistanceFinder& operator=(DynamicDistanceFinder const&) = delete;
  DynamicDistanceFinder() = delete;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the PathFinder
  //////////////////////////////////////////////////////////////////////////////

  DynamicDistanceFinder(ExpanderFunction&& forwardExpander,
             ExpanderFunction&& backwardExpander, bool bidirectional = true)
      : _highscoreSet(false),
        _highscore(0),
        _bingo(false),
        _resultCode(TRI_ERROR_NO_ERROR),
        _intermediateSet(false),
        _intermediate(),
        _forwardExpander(forwardExpander),
        _backwardExpander(backwardExpander),
        _bidirectional(bidirectional){};

  ~DynamicDistanceFinder(){};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Find the shortest path between start and target.
  ///        Only edges having the given direction are followed.
  ///        nullptr indicates there is no path.
  //////////////////////////////////////////////////////////////////////////////

  // Caller has to free the result
  // If this returns true there is a path, if this returns false there is no
  // path
  bool shortestPath(VertexId& start, VertexId& target, Path& result) override{
    // For the result:
    result.clear();
    _highscoreSet = false;
    _highscore = 0;
    _bingo = false;
    _intermediateSet = false;

    // Forward with initialization:
    VertexId emptyVertex;
    EdgeId emptyEdge;
    ThreadInfo forward;
    forward._pq.insert(start, new Step(start, emptyVertex, 0, emptyEdge));

    // backward with initialization:
    ThreadInfo backward;
    backward._pq.insert(target, new Step(target, emptyVertex, 0, emptyEdge));

    // Now the searcher threads:
    Searcher forwardSearcher(this, forward, backward, start, _forwardExpander,
                             "Forward");
    std::unique_ptr<Searcher> backwardSearcher;
    if (_bidirectional) {
      backwardSearcher.reset(new Searcher(this, backward, forward, target,
                                          _backwardExpander, "Backward"));
    }

    TRI_IF_FAILURE("TraversalOOMInitialize") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    while (!_bingo) {
      if (!forwardSearcher.oneStep()) {
        break;
      }
      if (_bidirectional && !backwardSearcher->oneStep()) {
        break;
      }
    }

    if (!_bingo || _intermediateSet == false) {
      return false;
    }

    Step* s = forward._pq.find(_intermediate);
    result._vertices.emplace_back(_intermediate);

    // FORWARD Go path back from intermediate -> start.
    // Insert all vertices and edges at front of vector
    // Do NOT! insert the intermediate vertex
    while (!s->_predecessor.isNone()) {
      result._edges.push_front(s->_edge);
      result._vertices.push_front(s->_predecessor);
      s = forward._pq.find(s->_predecessor);
    }

    // BACKWARD Go path back from intermediate -> target.
    // Insert all vertices and edges at back of vector
    // Also insert the intermediate vertex
    s = backward._pq.find(_intermediate);
    while (!s->_predecessor.isNone()) {
      result._edges.emplace_back(s->_edge);
      result._vertices.emplace_back(s->_predecessor);
      s = backward._pq.find(s->_predecessor);
    }

    TRI_IF_FAILURE("TraversalOOMPath") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    return true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the shortest path between the start and target vertex,
  /// multi-threaded version using SearcherTwoThreads.
  //////////////////////////////////////////////////////////////////////////////

  // Caller has to free the result
  // If this returns true there is a path, if this returns false there is no
  // path

  bool shortestPathTwoThreads(VertexId& start, VertexId& target, Path& result) {
    // For the result:
    result.clear();
    _highscoreSet = false;
    _highscore = 0;
    _bingo = false;

    // Forward with initialization:
    VertexId emptyVertex;
    EdgeId emptyEdge;
    ThreadInfo forward;
    forward._pq.insert(start, new Step(start, emptyVertex, 0, emptyEdge));

    // backward with initialization:
    ThreadInfo backward;
    backward._pq.insert(target, new Step(target, emptyVertex, 0, emptyEdge));

    // Now the searcher threads:
    SearcherTwoThreads forwardSearcher(this, forward, backward, start,
                                       _forwardExpander, "Forward");
    std::unique_ptr<SearcherTwoThreads> backwardSearcher;
    if (_bidirectional) {
      backwardSearcher.reset(new SearcherTwoThreads(
          this, backward, forward, target, _backwardExpander, "Backward"));
    }

    TRI_IF_FAILURE("TraversalOOMInitialize") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    forwardSearcher.start();
    if (_bidirectional) {
      backwardSearcher->start();
    }
    forwardSearcher.join();
    if (_bidirectional) {
      backwardSearcher->join();
    }

    // check error code returned by the threads
    int res = _resultCode.load();

    if (res != TRI_ERROR_NO_ERROR) {
      // one of the threads caught an exception
      THROW_ARANGO_EXCEPTION(res);
    }

    if (!_bingo || _intermediateSet == false) {
      return false;
    }

    Step* s = forward._pq.find(_intermediate);
    result._vertices.emplace_back(_intermediate);

    // FORWARD Go path back from intermediate -> start.
    // Insert all vertices and edges at front of vector
    // Do NOT! insert the intermediate vertex
    while (!s->_predecessor.isNone()) {
      result._edges.push_front(s->_edge);
      result._vertices.push_front(s->_predecessor);
      s = forward._pq.find(s->_predecessor);
    }

    // BACKWARD Go path back from intermediate -> target.
    // Insert all vertices and edges at back of vector
    // Also insert the intermediate vertex
    s = backward._pq.find(_intermediate);
    while (!s->_predecessor.isNone()) {
      result._edges.emplace_back(s->_edge);
      result._vertices.emplace_back(s->_predecessor);
      s = backward._pq.find(s->_predecessor);
    }

    TRI_IF_FAILURE("TraversalOOMPath") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    return true;
  }

  /* Here is a proof for the correctness of this algorithm:
   *
   * Assume we are looking for a shortest path from vertex A to vertex B.
   *
   * We do Dijkstra from both sides, thread 1 from A in forward direction and
   * thread 2 from B in backward direction. That is, we administrate a (hash)
   * table of distances from A to vertices in forward direction and one of
   * distances from B to vertices in backward direction.
   *
   * We get the following guarantees:
   *
   * When thread 1 is working on a vertex X, then it knows the distance w
   * from A to X.
   *
   * When thread 2 is working on a vertex Y, then it knows the distance v
   * from Y to B.
   *
   * When thread 1 is working on a vertex X at distance w from A, then it has
   * completed the work on all vertices X' at distance < w from A.
   *
   * When thread 2 is working on a vertex Y at distance v to B, then it has
   * completed the work on all vertices X' at (backward) distance < v to B.
   *
   * This all follows from the standard Dijkstra algorithm.
   *
   * Additionally, we do the following after we complete the normal work on a
   * vertex:
   *
   * Thread 1 checks for each vertex X at distance w from A whether thread 2
   * already knows it. If so, it makes sure that the highscore and intermediate
   * are set to the total length. Thread 2 does the analogous thing.
   *
   * If Thread 1 finds that vertex X (at distance v to B, say) has already
   * been completed by thread 2, then we call bingo. Thread 2 does the
   * analogous thing.
   *
   * We need to prove that the result is a shortest path.
   *
   * Assume that there is a shortest path of length <v+w from A to B. Let X'
   * be the latest vertex on this path with distance w' < w from A and let Y'
   * be the next one on the path. Then Y' is at distance w'+z' >= w from A
   * and thus at distance v' < v to B:
   *
   *    |     >=w      |   v'<v  |
   *    |  w'<w  |  z' |         |
   *    A -----> X' -> Y' -----> B
   *
   * Therefore, X' has already been completed by thread 1 and Y' has
   * already been completed by thread 2.
   *
   * Therefore, thread 1 has (in this temporal order) done:
   *
   *   1a: discover Y' and store it in table 1 under mutex 1
   *   1b: lookup X' in thread 2's table under mutex 2
   *   1c: mark X' as complete in table 1 under mutex 1
   *
   * And thread 2 has (in this temporal order) done:
   *
   *   2a: discover X' and store it in table 2 under mutex 2
   *   2b: lookup Y' in thread 1's table under mutex 1
   *   2c: mark Y' as complete in table 2 under mutex 2
   *
   * If 1b has happened before 2a, then 1a has happened before 2a and
   * thus 2b, so thread 2 has found the highscore w'+z'+v' < v+w.
   * Otherwise, 1b has happened after 2a and thus thread 1 has found the
   * highscore.
   *
   * Thus the highscore of this shortest path has already been set and the
   * algorithm is correct.
   */

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lowest total weight for a complete path found
  //////////////////////////////////////////////////////////////////////////////

  bool _highscoreSet;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lowest total weight for a complete path found
  //////////////////////////////////////////////////////////////////////////////

  EdgeWeight _highscore;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _bingo, flag that indicates termination
  //////////////////////////////////////////////////////////////////////////////

  std::atomic<bool> _bingo;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief result code. this is used to transport errors from sub-threads to
  /// the caller thread
  //////////////////////////////////////////////////////////////////////////////

  std::atomic<int> _resultCode;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _resultMutex, this is used to protect access to the result data
  //////////////////////////////////////////////////////////////////////////////

  arangodb::Mutex _resultMutex;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _intermediate, one vertex on the shortest path found, flag
  /// indicates
  /// whether or not it was set.
  //////////////////////////////////////////////////////////////////////////////

  bool _intermediateSet;
  VertexId _intermediate;

 private:
  ExpanderFunction _forwardExpander;
  ExpanderFunction _backwardExpander;
  bool _bidirectional;
};

template <typename VertexId, typename EdgeId, typename HashFuncType, typename EqualFuncType, typename Path>
class ConstDistanceFinder : public PathFinder<VertexId, Path> {
 public:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief callback to find neighbors
  //////////////////////////////////////////////////////////////////////////////

  typedef std::function<void(VertexId& V, std::vector<EdgeId>& edges,
                             std::vector<VertexId>& neighbors)>
      ExpanderFunction;

 private:
  struct PathSnippet {
    VertexId const _pred;
    EdgeId const _path;

    PathSnippet(VertexId& pred, EdgeId& path) : _pred(pred), _path(path) {}
  };

  std::unordered_map<VertexId, PathSnippet*, HashFuncType, EqualFuncType> _leftFound;
  std::deque<VertexId> _leftClosure;

  std::unordered_map<VertexId, PathSnippet*, HashFuncType, EqualFuncType> _rightFound;
  std::deque<VertexId> _rightClosure;

  ExpanderFunction _leftNeighborExpander;
  ExpanderFunction _rightNeighborExpander;

 public:
  ConstDistanceFinder(ExpanderFunction left, ExpanderFunction right)
      : _leftNeighborExpander(left), _rightNeighborExpander(right) {}

  ~ConstDistanceFinder() {
    clearVisited();
  }

  bool shortestPath(VertexId& start, VertexId& end, Path& result) override {
    result.clear();
    // Init
    if (start == end) {
      result._vertices.emplace_back(start);
      return true;
    }
    _leftClosure.clear();
    _rightClosure.clear();
    clearVisited();

    _leftFound.emplace(start, nullptr);
    _rightFound.emplace(end, nullptr);
    _leftClosure.emplace_back(start);
    _rightClosure.emplace_back(end);

    TRI_IF_FAILURE("TraversalOOMInitialize") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    std::vector<EdgeId> edges;
    std::vector<VertexId> neighbors;
    while (!_leftClosure.empty() && !_rightClosure.empty()) {
      edges.clear();
      neighbors.clear();
      std::deque<VertexId> _nextClosure;
      if (_leftClosure.size() < _rightClosure.size()) {
        for (VertexId& v : _leftClosure) {
          _leftNeighborExpander(v, edges, neighbors);
          TRI_ASSERT(edges.size() == neighbors.size());
          for (size_t i = 0; i < neighbors.size(); ++i) {
            VertexId const n = neighbors.at(i);
            if (_leftFound.find(n) == _leftFound.end()) {
              auto leftFoundIt = _leftFound.emplace(n, new PathSnippet(v, edges.at(i))).first;
              auto rightFoundIt = _rightFound.find(n);
              if (rightFoundIt != _rightFound.end()) {
                result._vertices.emplace_back(n);
                auto it = leftFoundIt;
                VertexId next;
                while (it->second != nullptr) {
                  next = it->second->_pred;
                  result._vertices.push_front(next);
                  result._edges.push_front(it->second->_path);
                  it = _leftFound.find(next);
                }
                it = rightFoundIt;
                while (it->second != nullptr) {
                  next = it->second->_pred;
                  result._vertices.emplace_back(next);
                  result._edges.emplace_back(it->second->_path);
                  it = _rightFound.find(next);
                }

                TRI_IF_FAILURE("TraversalOOMPath") {
                  THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
                }
                return true;
              }
              _nextClosure.emplace_back(n);
            }
          }
        }
        _leftClosure = _nextClosure;
      } else {
        for (VertexId& v : _rightClosure) {
          _rightNeighborExpander(v, edges, neighbors);
          TRI_ASSERT(edges.size() == neighbors.size());
          for (size_t i = 0; i < neighbors.size(); ++i) {
            VertexId const n = neighbors.at(i);
            if (_rightFound.find(n) == _rightFound.end()) {
              auto rightFoundIt = _rightFound.emplace(n, new PathSnippet(v, edges.at(i))).first;
              auto leftFoundIt = _leftFound.find(n);
              if (leftFoundIt != _leftFound.end()) {
                result._vertices.emplace_back(n);
                auto it = leftFoundIt;
                VertexId next;
                while (it->second != nullptr) {
                  next = it->second->_pred;
                  result._vertices.push_front(next);
                  result._edges.push_front(it->second->_path);
                  it = _leftFound.find(next);
                }
                it = rightFoundIt;
                while (it->second != nullptr) {
                  next = it->second->_pred;
                  result._vertices.emplace_back(next);
                  result._edges.emplace_back(it->second->_path);
                  it = _rightFound.find(next);
                }

                TRI_IF_FAILURE("TraversalOOMPath") {
                  THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
                }
                return true;
              }
              _nextClosure.emplace_back(n);
            }
          }
        }
        _rightClosure = _nextClosure;
      }
    }
    return false;
  }

 private:
  void clearVisited() {  
    for (auto& it : _leftFound) {
      delete it.second;
    }
    _leftFound.clear();

    for (auto& it : _rightFound) {
      delete it.second;
    }
    _rightFound.clear();
  }

};

} // namespace basics
} // namespace arangodb

#endif
