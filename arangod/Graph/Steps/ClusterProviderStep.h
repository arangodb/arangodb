////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <utility>

#include "Graph/Providers/BaseStep.h"
#include "Graph/Providers/TypeAliases.h"
#include "Transaction/Methods.h"

namespace arangodb::graph {

template<class StepImpl>
class ClusterProvider;

class ClusterProviderStep
    : public arangodb::graph::BaseStep<ClusterProviderStep> {
 public:
  using EdgeType = ::arangodb::graph::EdgeType;

  class Vertex {
   public:
    explicit Vertex(VertexType v) : _vertex(std::move(v)) {}

    [[nodiscard]] VertexType const& getID() const;

    bool operator<(Vertex const& other) const noexcept {
      return _vertex < other._vertex;
    }

    bool operator>(Vertex const& other) const noexcept {
      return _vertex > other._vertex;
    }

   private:
    VertexType _vertex;
  };

  class Edge {
   public:
    explicit Edge(EdgeType tkn) : _edge(std::move(tkn)) {}
    Edge() : _edge() {}

    void addToBuilder(ClusterProvider<ClusterProviderStep>& provider,
                      arangodb::velocypack::Builder& builder) const;
    [[nodiscard]] EdgeType const& getID()
        const;  // TODO: Performance Test compare EdgeType
                // <-> EdgeDocumentToken
    [[nodiscard]] bool isValid() const;

   private:
    EdgeType _edge;
  };

  ClusterProviderStep(VertexType v, EdgeType edge, size_t prev,
                      FetchedType fetched, size_t depth, double weight);
  ClusterProviderStep(VertexType v, size_t depth, double weight = 0.0);

 private:
  ClusterProviderStep(const VertexType& v, const EdgeType& edge, size_t prev);
  ClusterProviderStep(VertexType v, EdgeType edge, size_t prev,
                      FetchedType fetched);
  ClusterProviderStep(VertexType v, EdgeType edge, size_t prev,
                      FetchedType fetched, size_t depth);

  explicit ClusterProviderStep(const VertexType& v);

 public:
  ~ClusterProviderStep();

  bool operator<(ClusterProviderStep const& other) const noexcept {
    return _vertex < other._vertex;
  }

  [[nodiscard]] Vertex const& getVertex() const { return _vertex; }
  [[nodiscard]] Edge const& getEdge() const { return _edge; }

  [[nodiscard]] std::string toString() const {
    return "<Step><Vertex>: " + _vertex.getID().toString();
  }

  bool vertexFetched() const {
    return _fetchedStatus == FetchedType::VERTEX_FETCHED ||
           _fetchedStatus == FetchedType::VERTEX_AND_EDGES_FETCHED;
  }

  bool edgeFetched() const {
    return _fetchedStatus == FetchedType::EDGES_FETCHED ||
           _fetchedStatus == FetchedType::VERTEX_AND_EDGES_FETCHED;
  }

  // todo: rename
  [[nodiscard]] bool isProcessable() const { return !isLooseEnd(); }
  [[nodiscard]] bool isLooseEnd() const {
    return _fetchedStatus == FetchedType::UNFETCHED ||
           _fetchedStatus == FetchedType::EDGES_FETCHED ||
           _fetchedStatus == FetchedType::VERTEX_FETCHED;
  }

  [[nodiscard]] VertexType getVertexIdentifier() const {
    return _vertex.getID();
  }
  [[nodiscard]] EdgeType getEdgeIdentifier() const { return _edge.getID(); }

  [[nodiscard]] std::string getCollectionName() const {
    auto collectionNameResult = extractCollectionName(_vertex.getID());
    if (collectionNameResult.fail()) {
      THROW_ARANGO_EXCEPTION(collectionNameResult.result());
    }
    return collectionNameResult.get().first;
  };

  static bool isResponsible(transaction::Methods* trx);

  friend auto operator<<(std::ostream& out, ClusterProviderStep const& step)
      -> std::ostream&;

  void setVertexFetched() {
    if (edgeFetched()) {
      _fetchedStatus = FetchedType::VERTEX_AND_EDGES_FETCHED;
    } else {
      _fetchedStatus = FetchedType::VERTEX_FETCHED;
    }
  }
  void setEdgesFetched() {
    if (vertexFetched()) {
      _fetchedStatus = FetchedType::VERTEX_AND_EDGES_FETCHED;
    } else {
      _fetchedStatus = FetchedType::EDGES_FETCHED;
    }
  }

 private:
  Vertex _vertex;
  Edge _edge;
  FetchedType _fetchedStatus;
};

}  // namespace arangodb::graph
