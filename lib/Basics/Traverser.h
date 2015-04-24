////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase traversals
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_TRAVERSAL_H
#define ARANGODB_TRAVERSAL_H 1

#include "Basics/Common.h"

#include <mutex>

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                               class PriorityQueue
// -----------------------------------------------------------------------------

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
      // The Value type must be copyable and should be movable. Finally,
      // the Value type must have a method getKey that returns a Key
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

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        PriorityQueue () 
          : _popped(0), _isHeap(false), _maxWeight(0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~PriorityQueue () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief empty
////////////////////////////////////////////////////////////////////////////////

        bool empty () {
          return _heap.empty();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief size
////////////////////////////////////////////////////////////////////////////////

        size_t size () {
          return _heap.size();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief insert, data will be copied, returns true, if the key did not
/// yet exist, and false, in which case nothing else is changed.
////////////////////////////////////////////////////////////////////////////////

        bool insert (Key const& k, Value const& v) {
          auto it = _lookup.find(k);
          if (it != _lookup.end()) {
            return false;
          }

          // Are we still in the simple case of a deque?
          if (! _isHeap) {
            Weight w = v.weight();
            if (w < _maxWeight) {
              // Oh dear, we have to upgrade to heap:
              _isHeap = true;
              // fall through intentionally
            }
            else {
              if (w > _maxWeight) {
                _maxWeight = w;
              }
              _heap.push_back(v);
              try {
                _lookup.insert(std::make_pair(k, 
                               static_cast<ssize_t>(_heap.size()-1 + _popped)));
              }
              catch (...) {
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
            _lookup.insert(std::make_pair(k, 
                           static_cast<ssize_t>(newpos + _popped)));
            repairUp(newpos);
          }
          catch (...) {
            _heap.pop_back();
            throw;
          }
          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup, note that the resulting pointer is only valid until the
/// the next modification of the data structure happens (insert or lowerWeight
/// or popMinimal). The weight in the Value type must not be modified other
/// than via lowerWeight, otherwise the queue order could be violated.
////////////////////////////////////////////////////////////////////////////////

        Value* lookup (Key const& k) {
          auto it = _lookup.find(k);
          if (it == _lookup.end()) {
            return nullptr;
          }
          if (it->second >= 0) {   // still in the queue
            return &(_heap[static_cast<size_t>(it->second) - _popped]);
          }
          else {  // already in the history
            return &(_history[static_cast<size_t>(-it->second) - 1]);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief erase, returns whether the key was found
////////////////////////////////////////////////////////////////////////////////

        bool lowerWeight (Key const& k, Weight newWeight) {
          if (! _isHeap) {
            _isHeap = true;
          }
          auto it = _lookup.find(k);
          if (it == _lookup.end()) {
            return false;
          }
          if (it->second >= 0) {  // still in the queue
            size_t pos = static_cast<size_t>(it->second) - _popped;
            _heap[pos].setWeight(newWeight);
            repairUp(pos);
          }
          else {  // already in the history
            size_t pos = static_cast<size_t>(-it->second) - 1;
            _history[pos].setWeight(newWeight);
          }
          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getMinimal, note that the resulting pointer is only valid until the
/// the next modification of the data structure happens (insert or lowerWeight
/// or popMinimal). The weight in the Value type must not be modified other
/// than via lowerWeight, otherwise the queue order could be violated.
////////////////////////////////////////////////////////////////////////////////

        Value* getMinimal() {
          if (_heap.empty()) {
            return nullptr;
          }
          return &(_heap[0]);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief popMinimal, returns true if something was returned and false
/// if the structure is empty. Key and Value are stored in k and v.
/// If keepForLookup is true then the Value is kept for lookup in the
/// hash table but removed from the priority queue.
////////////////////////////////////////////////////////////////////////////////

        bool popMinimal (Key& k, Value& v, bool keepForLookup = false) {
          if (_heap.empty()) {
            return false;
          }
          k = _heap[0].getKey();
          v = _heap[0];
          if (! _isHeap) {
            auto it = _lookup.find(k);
            TRI_ASSERT(it != _lookup.end());
            if (keepForLookup) {
              _history.push_back(_heap[0]);
              it->second = -static_cast<ssize_t>(_history.size());
              // Note: This is intentionally one too large to shift by 1
            }
            else {
              _lookup.erase(it);
            }
            _heap.pop_front();
            _popped++;
          }
          else {
            removeFromHeap(keepForLookup);
          }
          return true;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief swap, two positions in the heap, adjusts the _lookup table
////////////////////////////////////////////////////////////////////////////////

        void swap (size_t p, size_t q) {
          Value v(std::move(_heap[p]));
          _heap[p] = std::move(_heap[q]);
          _heap[q] = std::move(v);

          // Now fix the lookup:
          Key const& keyp(_heap[p].getKey());
          auto it = _lookup.find(keyp);
          TRI_ASSERT(it != _lookup.end());
          TRI_ASSERT(it->second - _popped == q);
          it->second = static_cast<ssize_t>(p) + _popped;

          Key const& keyq(_heap[q].getKey());
          it = _lookup.find(keyq);
          TRI_ASSERT(it != _lookup.end());
          TRI_ASSERT(it->second - _popped == p);
          it->second = static_cast<ssize_t>(q) + _popped;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief parent, find the parent node in heap
////////////////////////////////////////////////////////////////////////////////

        size_t parent (size_t pos) {
          return ((pos+1) >> 1)-1;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief lchild, find the node of the left child in the heap
////////////////////////////////////////////////////////////////////////////////

        size_t lchild (size_t pos) {
          return 2*(pos+1)-1;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief rchild, find the node of the right child in the heap
////////////////////////////////////////////////////////////////////////////////

        size_t rchild (size_t pos) {
          return 2*(pos+1);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief repairUp, fix the heap property between position pos and its parent
////////////////////////////////////////////////////////////////////////////////

        void repairUp (size_t pos) {
          while (pos > 0) {
            size_t par = parent(pos);
            Weight wpos = _heap[pos].weight();
            Weight wpar = _heap[par].weight();
            if (wpos < wpar) {
              swap(pos, par);
              pos = par;
            }
            else {
              return;
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief repairDown, fix the heap property between position pos and its
/// children.
////////////////////////////////////////////////////////////////////////////////

        void repairDown () {
          size_t pos = 0;
          while (pos < _heap.size()) {
            size_t lchi = lchild(pos);
            if (lchi >= _heap.size()) {
              return;
            }
            Weight wpos = _heap[pos].weight();
            Weight wlchi = _heap[lchi].weight();
            size_t rchi = rchild(pos);
            if (rchi >= _heap.size()) {
              if (wpos > wlchi) {
                swap(pos, lchi);
              }
              return;
            }
            Weight wrchi = _heap[rchi].weight();
            if (wlchi <= wrchi) {
              if (wpos <= wlchi) {
                return;
              }
              swap(pos, lchi);
              pos = lchi;
            }
            else {
              if (wpos <= wrchi) {
                return;
              }
              swap(pos, rchi);
              pos = rchi;
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief removeFromHeap, remove first position in the heap
////////////////////////////////////////////////////////////////////////////////

        void removeFromHeap (bool keepForLookup) {
          auto it = _lookup.find(_heap[0].getKey());
          TRI_ASSERT(it != _lookup.end());
          if (keepForLookup) {
            _history.push_back(_heap[0]);
            it->second = -static_cast<ssize_t>(_history.size());
            // Note: This is intentionally one too large to shift by 1
          }
          else {
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
          it = _lookup.find(_heap[0].getKey());
          TRI_ASSERT(it != _lookup.end());
          it->second = static_cast<ssize_t>(_popped);
          repairDown();
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                     private parts
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief _popped, number of elements that have been popped from the beginning
/// of the deque, this is necessary to interpret positions stored in the
/// unordered_map _lookup
////////////////////////////////////////////////////////////////////////////////

      private:
        size_t _popped;

////////////////////////////////////////////////////////////////////////////////
/// @brief _lookup, this provides O(1) lookup by Key
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<Key, ssize_t> _lookup;

////////////////////////////////////////////////////////////////////////////////
/// @brief _isHeap, starts as false, in which case we only use a deque,
/// if true, then _heap is an actual binary heap and we do no longer modify
/// _popped.
////////////////////////////////////////////////////////////////////////////////

        bool _isHeap;

////////////////////////////////////////////////////////////////////////////////
/// @brief _heap, the actual data
////////////////////////////////////////////////////////////////////////////////

        std::deque<Value> _heap;

////////////////////////////////////////////////////////////////////////////////
/// @brief _maxWeight, the current maximal weight ever seen
////////////////////////////////////////////////////////////////////////////////

        Weight _maxWeight;

////////////////////////////////////////////////////////////////////////////////
/// @brief _history, the actual data that is only in the key/value store
////////////////////////////////////////////////////////////////////////////////

        std::vector<Value> _history;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                   class Traverser
// -----------------------------------------------------------------------------

    class Traverser {

// -----------------------------------------------------------------------------
// --SECTION--                                                   data structures
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                             types
// -----------------------------------------------------------------------------


    public:

////////////////////////////////////////////////////////////////////////////////
/// @brief types for vertices, edges and weights
////////////////////////////////////////////////////////////////////////////////

      typedef std::string VertexId;
      typedef std::string EdgeId;
      typedef double EdgeWeight;

////////////////////////////////////////////////////////////////////////////////
/// @brief Path, type for the result
////////////////////////////////////////////////////////////////////////////////

      // Convention vertices.size() -1 === edges.size()
      // path is vertices[0] , edges[0], vertices[1] etc.
      struct Path {
        std::deque<VertexId> vertices; 
        std::deque<EdgeId> edges; 
        EdgeWeight weight;

        Path (std::deque<VertexId> vertices, std::deque<EdgeId> edges,
              EdgeWeight weight) 
          : vertices(vertices), edges(edges), weight(weight) {
        };
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief Step, one position with a predecessor and the edge
////////////////////////////////////////////////////////////////////////////////

      struct Step {
        VertexId _vertex;
        VertexId _predecessor;
        EdgeWeight _weight;
        EdgeId _edge;
        bool _done;

        Step () : _done(false) {
        }

        Step (VertexId const& vert, VertexId const& pred,
              EdgeWeight weig, EdgeId const& edge)
          : _vertex(vert), _predecessor(pred), _weight(weig), _edge(edge),
            _done(false) {
        }

        EdgeWeight weight () const {
          return _weight;
        }

        void setWeight (EdgeWeight w) {
          _weight = w;
        }

        VertexId const& getKey () const {
          return _vertex;
        }
      };

////////////////////////////////////////////////////////////////////////////////
/// @brief edge direction
////////////////////////////////////////////////////////////////////////////////

      typedef enum {FORWARD, BACKWARD} Direction;

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to find neighbours
////////////////////////////////////////////////////////////////////////////////

      typedef std::function<void(VertexId V, std::vector<Step>& result)>
              ExpanderFunction;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the Traverser
////////////////////////////////////////////////////////////////////////////////

        Traverser (ExpanderFunction forwardExpander,
                   ExpanderFunction backwardExpander) 
          : _highscoreSet(false),
            _highscore(0),
            _bingo(false),
            _intermediate(""),
            _forwardExpander(forwardExpander),
            _backwardExpander(backwardExpander) {
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~Traverser () {
        };

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Find the shortest path between start and target.
///        Only edges having the given direction are followed.
///        nullptr indicates there is no path.
////////////////////////////////////////////////////////////////////////////////

        // Caller has to free the result
        // nullptr indicates there is no path
        Path* shortestPath (
          VertexId const& start,
          VertexId const& target
        );

// -----------------------------------------------------------------------------
// --SECTION--                                                       public data
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lowest total weight for a complete path found
////////////////////////////////////////////////////////////////////////////////

        bool _highscoreSet;

////////////////////////////////////////////////////////////////////////////////
/// @brief lowest total weight for a complete path found
////////////////////////////////////////////////////////////////////////////////

        EdgeWeight _highscore;

////////////////////////////////////////////////////////////////////////////////
/// @brief _bingo, flag that indicates termination
////////////////////////////////////////////////////////////////////////////////

        std::atomic<bool> _bingo;

////////////////////////////////////////////////////////////////////////////////
/// @brief _resultMutex, this is used to protect access to the result data
////////////////////////////////////////////////////////////////////////////////

        std::mutex _resultMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief _intermediate, one vertex on the shortest path found
////////////////////////////////////////////////////////////////////////////////

        VertexId _intermediate;

// -----------------------------------------------------------------------------
// --SECTION--                                                      private data
// -----------------------------------------------------------------------------

        typedef triagens::basics::PriorityQueue<VertexId, Step, EdgeWeight>
                PQueue;

        struct ThreadInfo {
          PQueue     _pq;
          std::mutex _mutex;
        };

        ExpanderFunction _forwardExpander;
        ExpanderFunction _backwardExpander;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
