////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CLUSTER_CLUSTER_TRAVERSER_H
#define ARANGOD_CLUSTER_CLUSTER_TRAVERSER_H 1

#include "VocBase/Traverser.h"

namespace arangodb {
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
  }

  void setStartVertex(arangodb::velocypack::Slice const&) override;

  TraversalPath* next() override;

  arangodb::basics::Json* edgeToJson(std::string const&) const;

  arangodb::basics::Json* vertexToJson(std::string const&) const;

 private:
  bool vertexMatchesCondition(arangodb::velocypack::Slice const&,
                              std::vector<TraverserExpression*> const&);

  class VertexGetter {
   public:
    explicit VertexGetter(ClusterTraverser* traverser)
        : _traverser(traverser) {}

    bool operator()(arangodb::velocypack::Slice const&, arangodb::velocypack::Slice const&, size_t,
                    arangodb::velocypack::Slice&);

   private:
    ClusterTraverser* _traverser;
  };

  class EdgeGetter {
   public:
    explicit EdgeGetter(ClusterTraverser* traverser)
        : _traverser(traverser), _continueConst(1) {}

    void operator()(arangodb::velocypack::Slice const&,
                    std::vector<arangodb::velocypack::Slice>&, size_t*&,
                    size_t&, bool&);

   private:
    ClusterTraverser* _traverser;

    size_t _continueConst;
  };

  std::unordered_map<arangodb::velocypack::Slice, arangodb::velocypack::Slice> _edges;

  std::unordered_map<arangodb::velocypack::Slice, arangodb::velocypack::Slice> _vertices;

  std::stack<std::stack<arangodb::velocypack::Slice>> _iteratorCache;

  std::vector<std::string> _edgeCols;

  std::string _dbname;

  VertexGetter _vertexGetter;

  EdgeGetter _edgeGetter;

  CollectionNameResolver const* _resolver;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal cursor to enumerate the paths of a graph
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<arangodb::basics::PathEnumerator<arangodb::velocypack::Slice, arangodb::velocypack::Slice,
                                                   size_t>> _enumerator;
};

class ClusterTraversalPath : public TraversalPath {
 public:
  ClusterTraversalPath(
      ClusterTraverser const* traverser,
      const arangodb::basics::EnumeratedPath<arangodb::velocypack::Slice, arangodb::velocypack::Slice>& path)
      : _path(path), _traverser(traverser) {}

  arangodb::basics::Json* pathToJson(Transaction*, CollectionNameResolver*);

  arangodb::basics::Json* lastEdgeToJson(Transaction*, CollectionNameResolver*);

  arangodb::basics::Json* lastVertexToJson(Transaction*,
                                           CollectionNameResolver*);

 private:
  arangodb::basics::EnumeratedPath<arangodb::velocypack::Slice, arangodb::velocypack::Slice> _path;

  ClusterTraverser const* _traverser;
};

}  // traverser
}  // triagens

#endif
