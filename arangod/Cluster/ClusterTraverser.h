////////////////////////////////////////////////////////////////////////////////
/// @brief Cluster Traverser
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

#ifndef ARANGODB_CLUSTER_CLUSTERTRAVERSER_H
#define ARANGODB_CLUSTER_CLUSTERTRAVERSER_H 1

#include "VocBase/Traverser.h"

namespace triagens {
  namespace arango {
    namespace traverser {

// -----------------------------------------------------------------------------
// --SECTION--                                            class ClusterTraverser
// -----------------------------------------------------------------------------

      class ClusterTraverser : public Traverser {
        
        public:

          ClusterTraverser (
            std::vector<std::string> edgeCollections,
            TraverserOptions& opts,
            std::string dbname
          ) : Traverser(opts),
            _edgeCols(edgeCollections),
            _dbname(dbname),
            _vertexGetter(&_edges),
            _edgeGetter(this) {
          }

          ~ClusterTraverser () {
          }

          void setStartVertex (VertexId& v) override;

          const TraversalPath* next () override;

        private:

// -----------------------------------------------------------------------------
// --SECTION--                                                   private classes
// -----------------------------------------------------------------------------

          class VertexGetter {

            public:

              VertexGetter (std::unordered_map<std::string, TRI_json_t*> const* edges)
                : _edges(edges) {
                }

              std::string operator() (std::string const&, std::string const&);

            private:

              std::unordered_map<std::string, TRI_json_t*> const* _edges;

          };

          class EdgeGetter {

            public:

              EdgeGetter (ClusterTraverser* traverser)
                : _traverser(traverser) {
                }

              void operator() (std::string const&,
                               std::vector<std::string>&,
                               size_t*&,
                               size_t&,
                               bool&);

            private:

              ClusterTraverser* _traverser;


          };

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

          std::unordered_map<std::string, TRI_json_t*> _edges;

          std::unordered_map<std::string, TRI_json_t*> _vertices;

          std::stack<std::stack<std::string>> _iteratorCache;

          std::vector<std::string> _edgeCols;

          std::string _dbname;

          VertexGetter _vertexGetter;

          EdgeGetter _edgeGetter;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal cursor to enumerate the paths of a graph
////////////////////////////////////////////////////////////////////////////////

          std::unique_ptr<triagens::basics::PathEnumerator<std::string,
                                                           std::string,
                                                           size_t>> _enumerator;

      };

      class ClusterTraversalPath : public TraversalPath {

        public:

          ClusterTraversalPath (ClusterTraverser* traverser, const triagens::basics::EnumeratedPath<std::string, std::string>& path) {
          }

          triagens::basics::Json* pathToJson (Transaction*,
                                              CollectionNameResolver*) const;

          triagens::basics::Json* lastEdgeToJson (Transaction*,
                                                  CollectionNameResolver*) const;

          triagens::basics::Json* lastVertexToJson (Transaction*,
                                                    CollectionNameResolver*) const;

      };



    } // traverser
  } // arango
} // triagens

#endif
