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

#ifndef ARANGOD_CLUSTER_CLUSTER_TRAVERSER_H
#define ARANGOD_CLUSTER_CLUSTER_TRAVERSER_H 1

#include "VocBase/Traverser.h"

namespace triagens {
namespace arango {
class CollectionNameResolver;
class Transaction;

namespace traverser {

class ClusterTraversalPath;

class ClusterTraverser : public Traverser {
 public:
  ClusterTraverser(
      std::vector<std::string> edgeCollections, TraverserOptions& opts,
      std::string dbname, CollectionNameResolver const* resolver,
      std::unordered_map<size_t, std::vector<TraverserExpression*>> const*
          expressions)
      : Traverser(opts, expressions),
        _edgeCols(edgeCollections),
        _dbname(dbname),
        _vertexGetter(this),
        _edgeGetter(this),
        _resolver(resolver) {}

  ~ClusterTraverser() {
    for (auto& it : _vertices) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, it.second);
    }
    for (auto& it : _edges) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, it.second);
    }
  }

  void setStartVertex(VertexId const& v) override;

  TraversalPath* next() override;

  triagens::basics::Json* edgeToJson(std::string const&) const;

  triagens::basics::Json* vertexToJson(std::string const&) const;

 private:
  
  bool vertexMatchesCondition(TRI_json_t*,
                              std::vector<TraverserExpression*> const&);

  
  class VertexGetter {
   public:
    explicit VertexGetter(ClusterTraverser* traverser)
        : _traverser(traverser) {}

    bool operator()(std::string const&, std::string const&, size_t,
                    std::string&);

   private:
    ClusterTraverser* _traverser;
  };

  class EdgeGetter {
   public:
    explicit EdgeGetter(ClusterTraverser* traverser)
        : _traverser(traverser), _continueConst(1) {}

    void operator()(std::string const&, std::vector<std::string>&, size_t*&,
                    size_t&, bool&);

   private:
    ClusterTraverser* _traverser;

    size_t _continueConst;
  };

  
  std::unordered_map<std::string, TRI_json_t*> _edges;

  std::unordered_map<std::string, TRI_json_t*> _vertices;

  std::stack<std::stack<std::string>> _iteratorCache;

  std::vector<std::string> _edgeCols;

  std::string _dbname;

  VertexGetter _vertexGetter;

  EdgeGetter _edgeGetter;

  CollectionNameResolver const* _resolver;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief internal cursor to enumerate the paths of a graph
  ////////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<triagens::basics::PathEnumerator<std::string, std::string,
                                                   size_t>> _enumerator;
};

class ClusterTraversalPath : public TraversalPath {
 public:
  ClusterTraversalPath(
      ClusterTraverser const* traverser,
      const triagens::basics::EnumeratedPath<std::string, std::string>& path)
      : _path(path), _traverser(traverser) {}

  triagens::basics::Json* pathToJson(Transaction*, CollectionNameResolver*);

  triagens::basics::Json* lastEdgeToJson(Transaction*, CollectionNameResolver*);

  triagens::basics::Json* lastVertexToJson(Transaction*,
                                           CollectionNameResolver*);

 private:
  triagens::basics::EnumeratedPath<std::string, std::string> _path;

  ClusterTraverser const* _traverser;
};

}  // traverser
}  // arango
}  // triagens

#endif
