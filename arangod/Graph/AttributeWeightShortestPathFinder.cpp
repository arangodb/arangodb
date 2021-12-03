////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "AttributeWeightShortestPathFinder.h"

#include "Basics/Exceptions.h"
#include "Basics/ResourceUsage.h"
#include "Basics/tryEmplaceHelper.h"
#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Graph/TraverserCache.h"
#include "Transaction/Helpers.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

AttributeWeightShortestPathFinder::Step::Step(std::string_view vert, std::string_view pred,
                                              double weig, EdgeDocumentToken&& edge)
    : _weight(weig), _vertex(vert), _predecessor(pred), _edge(std::move(edge)), _done(false) {}

AttributeWeightShortestPathFinder::Searcher::Searcher(
    AttributeWeightShortestPathFinder* pathFinder, ThreadInfo& myInfo,
    ThreadInfo& peerInfo, std::string_view start, bool backward)
    : _pathFinder(pathFinder),
      _myInfo(myInfo),
      _peerInfo(peerInfo),
      _start(start),
      _backward(backward) {}

void AttributeWeightShortestPathFinder::Searcher::insertNeighbor(std::unique_ptr<Step>&& step,
                                                                 double newWeight) {
  Step* s = _myInfo._pq.find(step->_vertex);

  // Not found, so insert it:
  if (s == nullptr) {
    step->setWeight(newWeight);
    _myInfo._pq.insert(step->_vertex, std::move(step));
    return;
  }
  if (!s->_done && s->weight() > newWeight) {
    s->_predecessor = step->_predecessor;
    std::swap(s->_edge, step->_edge);
    _myInfo._pq.lowerWeight(s->_vertex, newWeight);
  }
}

void AttributeWeightShortestPathFinder::Searcher::lookupPeer(std::string_view vertex,
                                                             double weight) {
  Step* s = _peerInfo._pq.find(vertex);

  if (s == nullptr) {
    // Not found, nothing more to do
    return;
  }
  double total = s->weight() + weight;

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

bool AttributeWeightShortestPathFinder::Searcher::oneStep() {
  std::string_view v;
  // Do not steal responsibility.
  Step* s = nullptr;
  bool b = _myInfo._pq.popMinimal(v, s);

  if (_pathFinder->_bingo || !b) {
    // We can leave this function only under 2 conditions:
    // 1) already bingo==true => bingo = true no effect
    // 2) This queue is empty => if there would be a
    //    path we would have found it here
    //    => No path possible. Set bingo, intermediate is empty.
    _pathFinder->_bingo = true;
    return false;
  }

  TRI_ASSERT(s != nullptr);

  _neighbors.clear();
  // populates _neighbors
  _pathFinder->expandVertex(_backward, v, _neighbors);

  for (auto& neighbor : _neighbors) {
    insertNeighbor(std::move(neighbor), s->weight() + neighbor->weight());
  }
  _neighbors.clear();
  // All neighbors are moved out.
  lookupPeer(v, s->weight());

  Step* s2 = _myInfo._pq.find(v);
  TRI_ASSERT(s2 != nullptr);
  if (s2 != nullptr) {
    s2->_done = true;
  }
  return true;
}

AttributeWeightShortestPathFinder::AttributeWeightShortestPathFinder(ShortestPathOptions& options)
    : ShortestPathFinder(options),
      _resourceMonitor(options.resourceMonitor()),
      _highscoreSet(false),
      _highscore(0),
      _bingo(false),
      _intermediateSet(false),
      _intermediate() {
  // cppcheck-suppress *
  _forwardCursor = _options.buildCursor(false);
  // cppcheck-suppress *
  _backwardCursor = _options.buildCursor(true);
}

AttributeWeightShortestPathFinder::~AttributeWeightShortestPathFinder() {
  // required for memory usage tracking
  clearCandidates();
}

void AttributeWeightShortestPathFinder::clearCandidates() noexcept {
  _resourceMonitor.decreaseMemoryUsage(_candidates.size() * candidateMemoryUsage());
  _candidates.clear();
}

void AttributeWeightShortestPathFinder::clear() {
  options().cache()->clear();
  _highscoreSet = false;
  _highscore = 0;
  _bingo = false;
  _intermediateSet = false;
  _intermediate = std::string_view{};
  clearCandidates();
}

bool AttributeWeightShortestPathFinder::shortestPath(arangodb::velocypack::Slice st,
                                                     arangodb::velocypack::Slice ta,
                                                     ShortestPathResult& result) {
  // For the result:
  result.clear();
  _highscoreSet = false;
  _highscore = 0;
  _bingo = false;
  _intermediateSet = false;

  std::string_view start =
      _options.cache()->persistString(st.stringView());
  std::string_view target =
      _options.cache()->persistString(ta.stringView());

  // Forward with initialization:
  std::string_view emptyVertex;
  ThreadInfo forward;
  forward._pq.insert(start, std::make_unique<Step>(start, emptyVertex, 0,
                                                   EdgeDocumentToken()));

  // backward with initialization:
  ThreadInfo backward;
  backward._pq.insert(target, std::make_unique<Step>(target, emptyVertex, 0,
                                                     EdgeDocumentToken()));

  // Now the searcher threads:
  Searcher forwardSearcher(this, forward, backward, start, false);
  std::unique_ptr<Searcher> backwardSearcher;
  if (_options.bidirectional) {
    backwardSearcher = std::make_unique<Searcher>(this, backward, forward, target, true);
  }

  TRI_IF_FAILURE("TraversalOOMInitialize") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  int counter = 0;

  while (!_bingo) {
    if (!forwardSearcher.oneStep()) {
      break;
    }
    if (_options.bidirectional && !backwardSearcher->oneStep()) {
      break;
    }

    if (++counter == 10) {
      // check for abortion
      options().isQueryKilledCallback();
      counter = 0;
    }
  }

  if (!_bingo || _intermediateSet == false) {
    return false;
  }

  Step* s = forward._pq.find(_intermediate);
  
  // track memory usage for result buildup.
  ResourceUsageScope guard(_resourceMonitor);

  result._vertices.emplace_back(_intermediate);

  // FORWARD Go path back from intermediate -> start.
  // Insert all vertices and edges at front of vector
  // Do NOT! insert the intermediate vertex
  while (true) {
    if (s == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "did not find required shortest path vertex");
    }

    if (s->_predecessor.empty()) {
      break;
    }

    guard.increase(arangodb::graph::ShortestPathResult::resultItemMemoryUsage());

    result._edges.push_front(std::move(s->_edge));
    result._vertices.push_front(s->_predecessor);
    s = forward._pq.find(s->_predecessor);
  }

  // BACKWARD Go path back from intermediate -> target.
  // Insert all vertices and edges at back of vector
  // Also insert the intermediate vertex
  s = backward._pq.find(_intermediate);

  while (true) {
    if (s == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "did not find required shortest path vertex");
    }

    if (s->_predecessor.empty()) {
      break;
    }
    
    guard.increase(arangodb::graph::ShortestPathResult::resultItemMemoryUsage());

    result._edges.emplace_back(std::move(s->_edge));
    result._vertices.emplace_back(s->_predecessor);
    s = backward._pq.find(s->_predecessor);
  }

  TRI_IF_FAILURE("TraversalOOMPath") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  _options.fetchVerticesCoordinator(result._vertices);
  // we intentionally don't commit the memory usage to the _resourceMonitor here.
  return true;
}

void AttributeWeightShortestPathFinder::inserter(
    std::vector<std::unique_ptr<Step>>& result,
    std::string_view s, std::string_view t,
    double currentWeight, EdgeDocumentToken&& edge) {
  
  ResourceUsageScope guard(_resourceMonitor, candidateMemoryUsage());

  auto [cand, emplaced] =
      _candidates.try_emplace(t, arangodb::lazyConstruct([&] {
                                   result.emplace_back(
                                     std::make_unique<Step>(t, s, currentWeight,
                                                            std::move(edge)));
                                   return result.size() - 1;
                              }));
  if (emplaced) {
    // new candidate created. now candiates are responsible for memory usage tracking
    guard.steal();
  } else {
    // Compare weight
    auto& old = result[cand->second];
    auto oldWeight = old->weight();
    if (currentWeight < oldWeight) {
      old->setWeight(currentWeight);
      old->_predecessor = s;
      old->_edge = std::move(edge);
    }
  }
}

void AttributeWeightShortestPathFinder::expandVertex(
    bool backward, std::string_view vertex,
    std::vector<std::unique_ptr<Step>>& result) {
  TRI_ASSERT(result.empty());

  EdgeCursor* cursor = backward ? _backwardCursor.get() : _forwardCursor.get();
  cursor->rearm(vertex, 0);

  clearCandidates();
  cursor->readAll([&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorIdx) -> void {
    if (edge.isString()) {
      VPackSlice doc = _options.cache()->lookupToken(eid);
      double currentWeight = _options.weightEdge(doc);
      std::string_view other =
          _options.cache()->persistString(edge.stringView());
      if (other.compare(vertex) != 0) {
        inserter(result, vertex, other, currentWeight, std::move(eid));
      } else {
        inserter(result, other, vertex, currentWeight, std::move(eid));
      }
    } else {
      std::string_view fromTmp(
          transaction::helpers::extractFromFromDocument(edge).stringView());
      std::string_view toTmp(transaction::helpers::extractToFromDocument(edge).stringView());
      std::string_view from = _options.cache()->persistString(fromTmp);
      std::string_view to = _options.cache()->persistString(toTmp);
      double currentWeight = _options.weightEdge(edge);
      if (from == vertex) {
        inserter(result, from, to, currentWeight, std::move(eid));
      } else {
        inserter(result, to, from, currentWeight, std::move(eid));
      }
    }
  });
  
  clearCandidates();
}

size_t AttributeWeightShortestPathFinder::candidateMemoryUsage() const noexcept {
  return 16 /*arbitrary overhead*/ + 
         sizeof(decltype(_candidates)::key_type) +
         sizeof(decltype(_candidates)::value_type);
}
