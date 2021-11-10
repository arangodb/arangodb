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

#pragma once

#include <velocypack/StringRef.h>

#include <memory>

#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathFinder.h"
#include "Graph/ShortestPathPriorityQueue.h"

namespace arangodb {
struct ResourceMonitor;

namespace graph {
class EdgeCursor;
struct EdgeDocumentToken;
struct ShortestPathOptions;

class AttributeWeightShortestPathFinder : public ShortestPathFinder {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Step, one position with a predecessor and the edge
  //////////////////////////////////////////////////////////////////////////////

  struct Step {
   private:
    double _weight;

   public:
    arangodb::velocypack::StringRef _vertex;
    arangodb::velocypack::StringRef _predecessor;
    arangodb::graph::EdgeDocumentToken _edge;
    bool _done;

    Step(arangodb::velocypack::StringRef const& vert,
         arangodb::velocypack::StringRef const& pred, double weig,
         EdgeDocumentToken&& edge);

    double weight() const { return _weight; }

    void setWeight(double w) { _weight = w; }

    arangodb::velocypack::StringRef const& getKey() const { return _vertex; }
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief edge direction
  //////////////////////////////////////////////////////////////////////////////

  typedef enum { FORWARD, BACKWARD } Direction;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief our specialization of the priority queue
  //////////////////////////////////////////////////////////////////////////////

  typedef arangodb::graph::ShortestPathPriorityQueue<
      arangodb::velocypack::StringRef, Step, double>
      PQueue;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief information for each thread
  //////////////////////////////////////////////////////////////////////////////

  struct ThreadInfo {
    PQueue _pq;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a Dijkstra searcher for the single-threaded search
  //////////////////////////////////////////////////////////////////////////////

  class Searcher {
   public:
    Searcher(AttributeWeightShortestPathFinder* pathFinder, ThreadInfo& myInfo,
             ThreadInfo& peerInfo, arangodb::velocypack::StringRef const& start,
             bool backward);

   public:
    //////////////////////////////////////////////////////////////////////////////
    /// @brief Do one step only.
    //////////////////////////////////////////////////////////////////////////////

    bool oneStep();

   private:
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Insert a neighbor to the todo list.
    ////////////////////////////////////////////////////////////////////////////////

    void insertNeighbor(std::unique_ptr<Step>&& step, double newWeight);

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Lookup our current vertex in the data of our peer.
    ////////////////////////////////////////////////////////////////////////////////

    void lookupPeer(arangodb::velocypack::StringRef& vertex, double weight);

   private:
    AttributeWeightShortestPathFinder* _pathFinder;
    ThreadInfo& _myInfo;
    ThreadInfo& _peerInfo;
    arangodb::velocypack::StringRef _start;
    bool _backward;
    /// @brief temp value, which is used only in Searcher::oneStep() and
    /// recycled.
    std::vector<std::unique_ptr<Step>> _neighbors;
  };

  // -----------------------------------------------------------------------------

  AttributeWeightShortestPathFinder(AttributeWeightShortestPathFinder const&) =
      delete;

  AttributeWeightShortestPathFinder& operator=(
      AttributeWeightShortestPathFinder const&) = delete;
  AttributeWeightShortestPathFinder() = delete;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the PathFinder
  //////////////////////////////////////////////////////////////////////////////

  explicit AttributeWeightShortestPathFinder(ShortestPathOptions& options);

  ~AttributeWeightShortestPathFinder();

  void clear() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Find the shortest path between start and target.
  ///        Only edges having the given direction are followed.
  ///        nullptr indicates there is no path.
  //////////////////////////////////////////////////////////////////////////////

  // Caller has to free the result
  // If this returns true there is a path, if this returns false there is no
  // path
  bool shortestPath(arangodb::velocypack::Slice const& start,
                    arangodb::velocypack::Slice const& target,
                    arangodb::graph::ShortestPathResult& result) override;

 private:
  void inserter(std::vector<std::unique_ptr<Step>>& result,
                arangodb::velocypack::StringRef const& s,
                arangodb::velocypack::StringRef const& t, double currentWeight,
                graph::EdgeDocumentToken&& edge);

  void expandVertex(bool backward,
                    arangodb::velocypack::StringRef const& source,
                    std::vector<std::unique_ptr<Step>>& result);

  void clearCandidates() noexcept;
  size_t candidateMemoryUsage() const noexcept;

  arangodb::ResourceMonitor& _resourceMonitor;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lowest total weight for a complete path found
  //////////////////////////////////////////////////////////////////////////////

  bool _highscoreSet;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lowest total weight for a complete path found
  //////////////////////////////////////////////////////////////////////////////

  double _highscore;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _bingo, flag that indicates termination
  //////////////////////////////////////////////////////////////////////////////

  std::atomic<bool> _bingo;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief _intermediate, one vertex on the shortest path found, flag
  /// indicates
  /// whether or not it was set.
  //////////////////////////////////////////////////////////////////////////////

  bool _intermediateSet;
  arangodb::velocypack::StringRef _intermediate;

  /// @brief temporary value, which is going to be populate in  inserter,
  /// and recycled between calls
  std::unordered_map<arangodb::velocypack::StringRef, size_t> _candidates;

  std::unique_ptr<EdgeCursor> _forwardCursor;
  std::unique_ptr<EdgeCursor> _backwardCursor;
};

}  // namespace graph
}  // namespace arangodb
