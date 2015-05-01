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
/// @author Max Neunhoeffer
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Traverser.h"

#include <thread>

using namespace std;
using namespace triagens::basics;

class Searcher {

    Traverser* _traverser;
    Traverser::ThreadInfo& _myInfo;
    Traverser::ThreadInfo& _peerInfo;
    Traverser::VertexId _start;
    Traverser::ExpanderFunction _expander;
    string _id;

  public:

    Searcher (Traverser* traverser, Traverser::ThreadInfo& myInfo, 
              Traverser::ThreadInfo& peerInfo, Traverser::VertexId start,
              Traverser::ExpanderFunction expander, string id)
      : _traverser(traverser), _myInfo(myInfo), 
        _peerInfo(peerInfo), _start(start), _expander(expander), _id(id) {
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Insert a neighbor to the todo list.
////////////////////////////////////////////////////////////////////////////////

  private:

    void insertNeighbor (Traverser::VertexId& neighbor,
                         Traverser::VertexId& predecessor,
                         Traverser::EdgeId& edge,
                         Traverser::EdgeWeight weight) {

      // std::lock_guard<std::mutex> guard(_myInfo._mutex);
      Traverser::Step* s = _myInfo._pq.find(neighbor);

      // Not found, so insert it:
      if (s == nullptr) {
        _myInfo._pq.insert(neighbor, 
                           Traverser::Step(neighbor, predecessor, 
                                           weight, edge));
        return;
      }
      if (s->_done) {
        return;
      }
      if (s->_weight > weight) {
        _myInfo._pq.lowerWeight(neighbor, weight);
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Lookup our current vertex in the data of our peer.
////////////////////////////////////////////////////////////////////////////////

    void lookupPeer (Traverser::VertexId& vertex,
                     Traverser::EdgeWeight weight) {

      //std::lock_guard<std::mutex> guard(_peerInfo._mutex);
      Traverser::Step* s = _peerInfo._pq.find(vertex);
      if (s == nullptr) {
        // Not found, nothing more to do
        return;
      }
      Traverser::EdgeWeight total = s->_weight + weight;

      // Update the highscore:
      //std::lock_guard<std::mutex> guard2(_traverser->_resultMutex);
      if (!_traverser->_highscoreSet || total < _traverser->_highscore) {
        _traverser->_highscoreSet = true;
        _traverser->_highscore = total;
        _traverser->_intermediate = vertex;
      }

      // Now the highscore is set!

      // Did we find a solution together with the other thread?
      if (s->_done) {
        if (total <= _traverser->_highscore) {
          _traverser->_intermediate = vertex;
        }
        // Hacki says: If the highscore was set, and even if it is better
        // than total, then this observation here proves that it will never
        // be better, so: BINGO.
        _traverser->_bingo = true;
        // We found a way, but somebody else found a better way, so
        // this is not the shortest path
        return;
      }

      // Did we find a solution on our own? This is for the single thread
      // case and for the case that the other thread is too slow to even
      // finish its own start vertex!
      if (s->_weight == 0) {
        // We have found the target, we have finished all vertices with
        // a smaller weight than this one (and did not succeed), so this
        // must be a best solution:
        _traverser->_intermediate = vertex;
        _traverser->_bingo = true;
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Search graph starting at Start following edges of the given
/// direction only
////////////////////////////////////////////////////////////////////////////////

#if 0
    void run () {

      Traverser::VertexId v;
      Traverser::Step s;
      bool b = _myInfo._pq.popMinimal(v, s, true);
      
      std::vector<Traverser::Step> neighbors;

      // Iterate while no bingo found and
      // there still is a vertex on the stack.
      while (!_traverser->_bingo && b) {
        neighbors.clear();
        _expander(v, neighbors);
        for (auto& neighbor : neighbors) {
          insertNeighbor(neighbor._vertex, v, neighbor._edge, 
                         s._weight + neighbor._weight);
        }
        lookupPeer(v, s._weight);

        std::lock_guard<std::mutex> guard(_myInfo._mutex);
        Traverser::Step* s2 = _myInfo._pq.find(v);
        s2->_done = true;
        b = _myInfo._pq.popMinimal(v, s, true);
      }
      // We can leave this loop only under 2 conditions:
      // 1) already bingo==true => bingo = true no effect
      // 2) This queue is empty => if there would be a
      //    path we would have found it here
      //    => No path possible. Set bingo, intermediate is empty.
      _traverser->_bingo = true;
    }
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief Do one step only.
////////////////////////////////////////////////////////////////////////////////

  public:

    bool oneStep () {

      Traverser::VertexId v;
      Traverser::Step s;
      bool b = _myInfo._pq.popMinimal(v, s, true);
      
      std::vector<Traverser::Step> neighbors;

      if (_traverser->_bingo || ! b) {
        // We can leave this functino only under 2 conditions:
        // 1) already bingo==true => bingo = true no effect
        // 2) This queue is empty => if there would be a
        //    path we would have found it here
        //    => No path possible. Set bingo, intermediate is empty.
        _traverser->_bingo = true;
        return false;
      }

      _expander(v, neighbors);
      for (auto& neighbor : neighbors) {
        insertNeighbor(neighbor._vertex, v, neighbor._edge, 
                       s._weight + neighbor._weight);
      }
      lookupPeer(v, s._weight);

      //std::lock_guard<std::mutex> guard(_myInfo._mutex);
      Traverser::Step* s2 = _myInfo._pq.find(v);
      s2->_done = true;
    }

    // std::thread _thread;

  public:

    void start () {
      // _thread = std::thread(&Searcher::run, this);
    }

    void join () {
      // _thread.join();
    }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the shortest path between the start and target vertex.
////////////////////////////////////////////////////////////////////////////////

Traverser::Path* Traverser::shortestPath (VertexId const& start,
                                          VertexId const& target) {

  // For the result:
  std::deque<VertexId> r_vertices;
  std::deque<VertexId> r_edges;
  _highscoreSet = false;
  _highscore = 0;
  _bingo = false;

  // Forward with initialization:
  string empty;
  ThreadInfo forward;
  forward._pq.insert(start, Step(start, empty, 0, empty));

  // backward with initialization:
  ThreadInfo backward;
  backward._pq.insert(target, Step(target, empty, 0, empty));

  // Now the searcher threads:
  Searcher forwardSearcher(this, forward, backward, start,
                           _forwardExpander, "Forward");
  std::unique_ptr<Searcher> backwardSearcher;
  if (_bidirectional) {
    backwardSearcher.reset(new Searcher(this, backward, forward, target,
                                        _backwardExpander, "Backward"));
  }
  while (! _bingo) {
    if (! forwardSearcher.oneStep()) {
      break;
    }
    if (! backwardSearcher->oneStep()) {
      break;
    }
  }

  if (!_bingo || _intermediate == "") {
    return nullptr;
  }

  Step* s = forward._pq.find(_intermediate);
  r_vertices.push_back(_intermediate);

  // FORWARD Go path back from intermediate -> start.
  // Insert all vertices and edges at front of vector
  // Do NOT! insert the intermediate vertex
  while (s->_predecessor != "") {
    r_edges.push_front(s->_edge);
    r_vertices.push_front(s->_predecessor);
    s = forward._pq.find(s->_predecessor);
  }

  // BACKWARD Go path back from intermediate -> target.
  // Insert all vertices and edges at back of vector
  // Also insert the intermediate vertex
  s = backward._pq.find(_intermediate);
  while (s->_predecessor != "") {
    r_edges.push_back(s->_edge);
    r_vertices.push_back(s->_predecessor);
    s = backward._pq.find(s->_predecessor);
  }
  return new Path(r_vertices, r_edges, _highscore);
};

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

