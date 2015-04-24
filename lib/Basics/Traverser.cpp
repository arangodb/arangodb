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

#include "Basics/Thread.h"

using namespace std;
using namespace triagens::basics;

class Searcher : public Thread {

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
      : Thread(id), _traverser(traverser), _myInfo(myInfo), 
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

      std::lock_guard<std::mutex> guard(_myInfo._mutex);
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

      std::lock_guard<std::mutex> guard(_peerInfo._mutex);
      Traverser::Step* s = _peerInfo._pq.find(vertex);
      if (s == nullptr) {
        // Not found, nothing more to do
        return;
      }
      Traverser::EdgeWeight total = s->_weight + weight;

      // Update the highscore:
      std::lock_guard<std::mutex> guard2(_traverser->_resultMutex);
      if (!_traverser->_highscoreSet || total < _traverser->_highscore) {
        _traverser->_highscoreSet = true;
        _traverser->_highscore = total;
      }

      // Now the highscore is set!

      // Did we find a solution together with the other thread?
      if (s->_done) {
        if (total <= _traverser->_highscore) {
          _traverser->_intermediate = vertex;
          _traverser->_bingo = true;
        }
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

  public:

    virtual void run () {

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
  //Searcher backwardSearcher(this, backward, forward, target,
  //                          _backwardExpander, "Backward");
  forwardSearcher.start();
  //backwardSearcher.start();
  forwardSearcher.join();
  //backwardSearcher.join();

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

