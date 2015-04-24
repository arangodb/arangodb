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
      : Thread(id), _traverser(traverser), _myInfo(myInfo), _peerInfo(peerInfo), 
        _start(start), _expander(expander), _id(id) {
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Insert a neighbor to the todo list.
////////////////////////////////////////////////////////////////////////////////

  private:

    void insertNeighbor (Traverser::ThreadInfo& info,
                         Traverser::VertexId& neighbor,
                         Traverser::VertexId& predecessor,
                         Traverser::EdgeId& edge,
                         Traverser::EdgeWeight weight) {

      std::lock_guard<std::mutex> guard(info.mutex);
      auto it = info.lookup.find(neighbor);

      // Not found, so insert it:
      if (it == info.lookup.end()) {
        info.lookup.emplace(
          neighbor,
          Traverser::LookupInfo(weight, edge, predecessor)
        );
        info.queue.insert(
          Traverser::QueueInfo(neighbor, weight)
        );
        return;
      }
      if (it->second.done) {
        return;
      }
      if (it->second.weight > weight) {
        Traverser::QueueInfo q(neighbor, it->second.weight);
        info.queue.erase(q);
        q.weight = weight;
        info.queue.insert(q);
        it->second.weight = weight;
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Lookup a neighbor in the list of our peer.
////////////////////////////////////////////////////////////////////////////////

    void lookupPeer (Traverser::ThreadInfo& info,
                     Traverser::VertexId& neighbor,
                     Traverser::EdgeWeight weight) {

      std::lock_guard<std::mutex> guard(info.mutex);
      auto it = info.lookup.find(neighbor);
      if (it == info.lookup.end()) {
        return;
      }
      Traverser::EdgeWeight total = it->second.weight + weight;
      if (total < _traverser->highscore) {
        _traverser->highscore = total;
      }
      if (it->second.done && total <= _traverser->highscore) {
        std::lock_guard<std::mutex> guard(_traverser->resultMutex);
        _traverser->intermediate = neighbor;
        _traverser->bingo = true;
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Search graph starting at Start following edges of the given
/// direction only
////////////////////////////////////////////////////////////////////////////////

  public:

    virtual void run () {

      cout << _id << ": inserting " << _start << endl;
      string empty;
      insertNeighbor(_myInfo, _start, empty, empty, 0);
      auto nextVertexIt = _myInfo.queue.begin();
      std::vector<Traverser::Neighbor> neighbors;

      // Iterate while no bingo found and
      // there still is a vertex on the stack.
      while (!_traverser->bingo && nextVertexIt != _myInfo.queue.end()) {
        auto nextVertex = *nextVertexIt;
        cout << _id << ": next " << nextVertex.vertex << endl;
        _myInfo.queue.erase(nextVertexIt);
        neighbors.clear();
        _expander(nextVertex.vertex, neighbors);
        for (auto& neighbor : neighbors) {
          cout << _id << ": neighbor " << neighbor.neighbor << endl;
          insertNeighbor(_myInfo, neighbor.neighbor, nextVertex.vertex,
                         neighbor.edge, nextVertex.weight + neighbor.weight);
        }
        lookupPeer(_peerInfo, nextVertex.vertex, nextVertex.weight);
        _myInfo.mutex.lock();
        // Can move nextVertexLookup up?
        auto nextVertexLookup = _myInfo.lookup.find(nextVertex.vertex);

        TRI_ASSERT(nextVertexLookup != _myInfo.lookup.end());
        cout << _id << ": done " << nextVertexLookup->first << endl;

        nextVertexLookup->second.done = true;
        _myInfo.mutex.unlock();
        nextVertexIt = _myInfo.queue.begin();
      }
      _traverser->bingo = true;
      // No possible path, can possibly terminate other thread
    }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the shortest path between the start and target vertex.
////////////////////////////////////////////////////////////////////////////////

Traverser::Path* Traverser::ShortestPath (VertexId const& start,
                                          VertexId const& target) {

  std::vector<VertexId> r_vertices;
  std::vector<VertexId> r_edges;
  highscore = 1e50;
  bingo = false;

  // Forward
  _forwardLookup.clear();
  _forwardQueue.clear();
  ThreadInfo forwardInfo(_forwardLookup, _forwardQueue, _forwardMutex);

  _backwardLookup.clear();
  _backwardQueue.clear();
  ThreadInfo backwardInfo(_backwardLookup, _backwardQueue, _backwardMutex);

  Searcher forwardSearcher(this, forwardInfo, backwardInfo, start,
                           forwardExpander, "X");
  Searcher backwardSearcher(this, backwardInfo, forwardInfo, target,
                            backwardExpander, "Y");
  forwardSearcher.start();
  backwardSearcher.start();
  forwardSearcher.join();
  backwardSearcher.join();

  cout << forwardInfo.lookup.size() << backwardInfo.lookup.size() << endl;

  if (!bingo || intermediate == "") {
    return nullptr;
  }

  auto pathLookup = _forwardLookup.find(intermediate);
  // FORWARD Go path back from intermediate -> start.
  // Insert all vertices and edges at front of vector
  // Do NOT! insert the intermediate vertex
  TRI_ASSERT(pathLookup != _forwardLookup.end());
  if (pathLookup->second.predecessor != "") {
    r_edges.insert(r_edges.begin(), pathLookup->second.edge);
    pathLookup = _forwardLookup.find(pathLookup->second.predecessor);
  }
  while (pathLookup->second.predecessor != "") {
    r_edges.insert(r_edges.begin(), pathLookup->second.edge);
    r_vertices.insert(r_vertices.begin(), pathLookup->first);
    pathLookup = _forwardLookup.find(pathLookup->second.predecessor);
  }
  r_vertices.insert(r_vertices.begin(), pathLookup->first);

  // BACKWARD Go path back from intermediate -> target.
  // Insert all vertices and edges at back of vector
  // Also insert the intermediate vertex
  pathLookup = _backwardLookup.find(intermediate);
  TRI_ASSERT(pathLookup != _backwardLookup.end());
  while (pathLookup->second.predecessor != "") {
    r_vertices.push_back(pathLookup->first);
    r_edges.push_back(pathLookup->second.edge);
    pathLookup = _backwardLookup.find(pathLookup->second.predecessor);
  }
  r_vertices.push_back(pathLookup->first);
  Path* res = new Path(r_vertices, r_edges, highscore);
  return res;
};
