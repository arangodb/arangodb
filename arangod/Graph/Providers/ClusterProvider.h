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
/// @author Heiko Kernbach
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_PROVIDERS_CLUSTER_PROVIDER_H
#define ARANGOD_GRAPH_PROVIDERS_CLUSTER_PROVIDER_H 1

#include "Graph/EdgeDocumentToken.h"
#include "Graph/Providers/BaseProviderOptions.h"
#include "Graph/Providers/BaseStep.h"
#include "Graph/Providers/TypeAliases.h"

#include "Aql/TraversalStats.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StringHeap.h"

#include "Transaction/Methods.h"

#include <vector>

namespace arangodb {

namespace futures {
template <typename T>
class Future;
}

namespace aql {
class QueryContext;
}

namespace velocypack {
class Builder;
class HashedStringRef;
}  // namespace velocypack

namespace graph {

// TODO: we need to control from the outside if and which parts of the vertex - (will be implemented in the future via template parameters)
// data should be returned THis is most-likely done via Template Parameter like
// this: template<ProduceVertexData>
struct ClusterProvider {
  class Step : public arangodb::graph::BaseStep<Step> {
   public:
    class Vertex {
     public:
      explicit Vertex(VertexType v) : _vertex(v){};

      VertexType const& getID() const;

      bool operator<(Vertex const& other) const noexcept {
        return _vertex < other._vertex;
      }

      bool operator>(Vertex const& other) const noexcept {
        return !operator<(other);
      }

     private:
      VertexType _vertex;
    };

    class Edge {
     public:
      explicit Edge(EdgeDocumentToken tkn) : _token(std::move(tkn)) {}
      explicit Edge() : _token() { _token = EdgeDocumentToken(); }

      void addToBuilder(ClusterProvider& provider,
                        arangodb::velocypack::Builder& builder) const;
      EdgeDocumentToken const& getID() const;
      bool isValid() const;

     private:
      EdgeDocumentToken _token;
    };

    Step(VertexType v);
    Step(VertexType v, EdgeDocumentToken edge, size_t prev);
    ~Step();

    bool operator<(Step const& other) const noexcept {
      return _vertex < other._vertex;
    }

    Vertex const& getVertex() const { return _vertex; }
    Edge const& getEdge() const { return _edge; }

    std::string toString() const {
      return "<Step><Vertex>: " + _vertex.getID().toString();
    }
    bool isProcessable() const { return !isLooseEnd(); }
    bool isLooseEnd() const { return false; }

    VertexType getVertexIdentifier() const { return _vertex.getID(); }

    friend auto operator<<(std::ostream& out, Step const& step) -> std::ostream&;

   private:
    Vertex _vertex;
    Edge _edge;
  };

 public:
  ClusterProvider(arangodb::aql::QueryContext& queryContext, BaseProviderOptions opts,
                       arangodb::ResourceMonitor& resourceMonitor);
  ClusterProvider(ClusterProvider const&) = delete;
  ClusterProvider(ClusterProvider&&) = default;
  ~ClusterProvider() = default;

  ClusterProvider& operator=(ClusterProvider const&) = delete;

  auto startVertex(VertexType vertex) -> Step;
  auto fetch(std::vector<Step*> const& looseEnds)
      -> futures::Future<std::vector<Step*>>;                           // rocks
  auto expand(Step const& from, size_t previous,
              std::function<void(Step)> const& callback) -> void;  // index

  void insertEdgeIntoResult(EdgeDocumentToken edge, arangodb::velocypack::Builder& builder);

  void addVertexToBuilder(Step::Vertex const& vertex, arangodb::velocypack::Builder& builder);
  void addEdgeToBuilder(Step::Edge const& edge, arangodb::velocypack::Builder& builder);

  void destroyEngines(){};

  [[nodiscard]] transaction::Methods* trx();
  arangodb::ResourceMonitor* resourceMonitor();

  aql::TraversalStats stealStats();

 private:
  void activateCache(bool enableDocumentCache);

  // std::unique_ptr<RefactoredSingleServerEdgeCursor> buildCursor();

  [[nodiscard]] arangodb::aql::QueryContext* query() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Persist the given id string. The return value is guaranteed to
  ///        stay valid as long as this cache is valid
  //////////////////////////////////////////////////////////////////////////////
  arangodb::velocypack::HashedStringRef persistString(arangodb::velocypack::HashedStringRef idString);

 private:
  // Unique_ptr to have this class movable, and to keep reference of trx()
  // alive - Note: _trx must be first here because it is used in _cursor
  std::unique_ptr<arangodb::transaction::Methods> _trx;

  // std::unique_ptr<RefactoredSingleServerEdgeCursor> _cursor; // TODO REMOVE

  arangodb::aql::QueryContext* _query;

  arangodb::ResourceMonitor* _resourceMonitor;

  // RefactoredTraverserCache _cache;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Set of all strings persisted in the stringHeap. So we can save some
  ///        memory by not storing them twice.
  //////////////////////////////////////////////////////////////////////////////
  std::unordered_set<arangodb::velocypack::HashedStringRef> _persistedStrings;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Stringheap to take care of _id strings, s.t. they stay valid
  ///        during the entire traversal.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::StringHeap _stringHeap;

  BaseProviderOptions _opts;

  arangodb::aql::TraversalStats _stats;
};
}  // namespace graph
}  // namespace arangodb

#endif
