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

using namespace std;
using namespace triagens::basics;

void Traverser::insertNeighbor (ThreadInfo& info,
                            VertexId neighbor,
                            VertexId predecessor,
                            EdgeId edge,
                            EdgeWeight weight
                           ) {
  std::lock_guard<std::mutex> guard(info.mutex);
  auto it = info.lookup.find(neighbor);

  // Not found insert it
  if (it == info.lookup.end()) {
    info.lookup.emplace(
      neighbor,
      LookupInfo(weight, edge, predecessor)
    );
    info.queue.insert(
      QueueInfo(neighbor, weight)
    );
    return;
  }
  if (it->second.done) {
    return;
  }
  if (it->second.weight > weight) {
    QueueInfo q(neighbor, it->second.weight);
    info.queue.erase(q);
    q.weight = weight;
    info.queue.insert(q);
    it->second.weight = weight;
  }
};

void Traverser::lookupPeer (ThreadInfo& info,
                            VertexId& neighbor,
                            EdgeWeight& weight) {
  std::lock_guard<std::mutex> guard(info.mutex);
  auto it = info.lookup.find(neighbor);
  if (it == info.lookup.end()) {
    return;
  }
  EdgeWeight total = it->second.weight + weight;
  if (total < highscore) {
    highscore = total;
  }
  if (it->second.done && total <= highscore) {
    std::lock_guard<std::mutex> guard(resultMutex);
    intermediate = neighbor;
    bingo = true;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Search graph starting at Start following edges of the given
///        direction only
////////////////////////////////////////////////////////////////////////////////

void Traverser::searchFromVertex (
    ThreadInfo* myInfo_p,
    ThreadInfo* peerInfo_p,
    VertexId start,
    ExpanderFunction expander,
    string id
    ) {
  ThreadInfo& myInfo(*myInfo_p);
  ThreadInfo& peerInfo(*peerInfo_p);

  cout << id << ": inserting " << start << endl;
  insertNeighbor(myInfo, start, "", "", 0);
  auto nextVertexIt = myInfo.queue.begin();
  std::vector<Neighbor> neighbors;

  // Iterate while no bingo found and
  // there still is a vertex on the stack.
  while (!bingo && nextVertexIt != myInfo.queue.end()) {
    auto nextVertex = *nextVertexIt;
    cout << id << ": next " << nextVertex.vertex << endl;
    myInfo.queue.erase(nextVertexIt);
    neighbors.clear();
    expander(nextVertex.vertex, neighbors);
    for (auto& neighbor : neighbors) {
      cout << id << ": neighbor " << neighbor.neighbor << endl;
      insertNeighbor(myInfo, neighbor.neighbor, nextVertex.vertex,
                     neighbor.edge, nextVertex.weight + neighbor.weight);
    }
    lookupPeer(peerInfo, nextVertex.vertex, nextVertex.weight);
    myInfo.mutex.lock();
    // Can move nextVertexLookup up?
    auto nextVertexLookup = myInfo.lookup.find(nextVertex.vertex);

    TRI_ASSERT(nextVertexLookup != myInfo.lookup.end());
    cout << id << ": done " << nextVertexLookup->first << endl;

    nextVertexLookup->second.done = true;
    myInfo.mutex.unlock();
    nextVertexIt = myInfo.queue.begin();
  }
  // No possible path, can possibly terminate other thread
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

  std::thread forwardSearcher(&Traverser::searchFromVertex, 
                                            this, &forwardInfo, &backwardInfo, start, forwardExpander, string("X"));
  std::thread backwardSearcher(&Traverser::searchFromVertex, 
                                             this, &backwardInfo, &forwardInfo, target, backwardExpander, string("Y"));
  forwardSearcher.join();
  backwardSearcher.join();

  cout << forwardInfo.lookup.size() << backwardInfo.lookup.size() << endl;

  if (!bingo) {
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
