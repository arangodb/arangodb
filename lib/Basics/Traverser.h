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

namespace triagens {
  namespace basics {
// -----------------------------------------------------------------------------
// --SECTION--                                                   class Traverser
// -----------------------------------------------------------------------------

    class Traverser {

// -----------------------------------------------------------------------------
// --SECTION--                                                   data structures
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                              path
// -----------------------------------------------------------------------------


    public:

      typedef std::string VertexId;
      typedef std::string EdgeId;
      typedef double EdgeWeight;

      // Convention vertices.size() -1 === edges.size()
      // path is vertices[0] , edges[0], vertices[1] etc.
      struct Path {
        std::vector<VertexId> vertices; 
        std::vector<EdgeId> edges; 
        EdgeWeight weight;

        Path (
          std::vector<VertexId> vertices,
          std::vector<EdgeId> edges,
          EdgeWeight weight
        ) : vertices(vertices),
            edges(edges),
            weight(weight) {
        };
      };

      struct Neighbor {
        VertexId neighbor;
        EdgeId edge;
        EdgeWeight weight;

        Neighbor (
          VertexId neighbor,
          EdgeId edge,
          EdgeWeight weight
        ) : neighbor(neighbor),
            edge(edge),
            weight(weight) {
        };
      };

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief edge direction
      ////////////////////////////////////////////////////////////////////////////////
      typedef enum {FORWARD, BACKWARD} Direction;

      typedef std::function<void(VertexId source, Direction dir, std::vector<Neighbor>& result)>
              ExpanderFunction;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the Traverser
////////////////////////////////////////////////////////////////////////////////

        Traverser (ExpanderFunction const& n) : _neighborFunction(n) {
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////
        ~Traverser ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief Find the shortest path between start and target.
///        Only edges having the given direction are followed.
///        nullptr indicates there is no path.
////////////////////////////////////////////////////////////////////////////////

        // Caller has to free the result
        // nullptr indicates there is no path
        Path* ShortestPath (
          VertexId const& start,
          VertexId const& target
        );

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Function to compute all neighbors of a given vertex
////////////////////////////////////////////////////////////////////////////////
      private:

        std::atomic<EdgeWeight> highscore;
        std::atomic<bool> bingo;
        std::mutex resultMutex;
        VertexId intermediate;

        struct LookupInfo {
          EdgeWeight weight;
          bool done;
          EdgeId edge;
          VertexId predecessor;

          LookupInfo (
            EdgeWeight weight,
            EdgeId edge,
            VertexId predecessor
          ) : weight(weight),
              done(false),
              edge(edge),
              predecessor(predecessor) {
          };
        };

        struct QueueInfo {
          EdgeWeight weight;
          VertexId vertex;

          QueueInfo (
            VertexId vertex,
            EdgeWeight weight
          ) : weight(weight),
              vertex(vertex) {
          };

          friend bool operator< (QueueInfo const& a, QueueInfo const& b) {
            if (a.weight == b.weight) {
              return a.vertex < b.vertex;
            }
            return a.weight < b.weight;
          };

        };

        // TODO: Destructor?!
        struct ThreadInfo {
          std::unordered_map<VertexId, LookupInfo>& lookup;
          std::set<QueueInfo, std::less<QueueInfo>>& queue;
          std::mutex& mutex;

          ThreadInfo (
            std::unordered_map<VertexId, LookupInfo>& lookup,
            std::set<QueueInfo, std::less<QueueInfo>>& queue,
            std::mutex& mutex
          ) : lookup(lookup),
              queue(queue),
              mutex(mutex) {
          };
        };


        ExpanderFunction const& _neighborFunction;

        // Are they needed anyway??
        // ShortestPath will create these variables
        std::unordered_map<VertexId, LookupInfo> _forwardLookup;
        std::set<QueueInfo, std::less<QueueInfo>> _forwardQueue;
        std::mutex _forwardMutex;

        std::unordered_map<VertexId, LookupInfo> _backwardLookup;
        std::set<QueueInfo, std::less<QueueInfo>> _backwardQueue;
        std::mutex _backwardMutex;


        void insertNeighbor ( ThreadInfo& info,
                              VertexId neighbor,
                              VertexId predecessor,
                              EdgeId edge,
                              EdgeWeight weight
                            );

        void lookupPeer ( ThreadInfo& info,
                          VertexId neighbor,
                          EdgeWeight weight
                        );
        void searchFromVertex ( ThreadInfo myInfo,
                                ThreadInfo peerInfo,
                                VertexId start,
                                Direction dir
                              );
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
