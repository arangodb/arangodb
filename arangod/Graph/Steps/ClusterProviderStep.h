////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Graph/Types/ValidationResult.h"
#include "Transaction/Methods.h"

namespace arangodb::graph {

template<class StepImpl>
class ClusterProvider;

class ClusterProviderStep
    : public arangodb::graph::BaseStep<ClusterProviderStep> {
 public:
  using EdgeType = ::arangodb::graph::EdgeType;
  using VertexType = arangodb::velocypack::HashedStringRef;

  class Vertex {
   public:
    explicit Vertex(VertexType v) : _vertex(std::move(v)) {}

    [[nodiscard]] VertexType const& getID() const noexcept;

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
    Edge() = default;

    [[nodiscard]] EdgeType const& getID()
        const noexcept;  // TODO: Performance Test compare EdgeType
                         // <-> EdgeDocumentToken
    [[nodiscard]] bool isValid() const noexcept;

   private:
    EdgeType _edge;
  };

  ClusterProviderStep(VertexType v, EdgeType edge, size_t prev,
                      FetchedType fetched, size_t depth, double weight);
  ClusterProviderStep(VertexType v, size_t depth, double weight = 0.0);

 private:
  ClusterProviderStep(VertexType const& v, EdgeType const& edge, size_t prev);
  ClusterProviderStep(VertexType v, EdgeType edge, size_t prev,
                      FetchedType fetched);
  ClusterProviderStep(VertexType v, EdgeType edge, size_t prev,
                      FetchedType fetched, size_t depth);

  explicit ClusterProviderStep(VertexType const& v);

 public:
  ~ClusterProviderStep();

  bool operator<(ClusterProviderStep const& other) const noexcept {
    return _vertex < other._vertex;
  }

  [[nodiscard]] Vertex const& getVertex() const noexcept { return _vertex; }
  [[nodiscard]] Edge const& getEdge() const noexcept { return _edge; }

  [[nodiscard]] std::string toString() const {
    return "<Step><Vertex>: " + _vertex.getID().toString() +
           " <Depth>: " + std::to_string(getDepth()) +
           " <Weight>: " + std::to_string(getWeight());
  }

  bool vertexFetched() const noexcept {
    return _fetchedStatus == FetchedType::VERTEX_FETCHED ||
           _fetchedStatus == FetchedType::VERTEX_AND_EDGES_FETCHED;
  }

  bool edgeFetched() const noexcept {
    return _fetchedStatus == FetchedType::EDGES_FETCHED ||
           _fetchedStatus == FetchedType::VERTEX_AND_EDGES_FETCHED;
  }

  // todo: rename
  [[nodiscard]] bool isProcessable() const noexcept { return !isLooseEnd(); }
  [[nodiscard]] bool isLooseEnd() const noexcept {
    return _fetchedStatus == FetchedType::UNFETCHED ||
           _fetchedStatus == FetchedType::EDGES_FETCHED ||
           _fetchedStatus == FetchedType::VERTEX_FETCHED;
  }
  bool isUnknown() const noexcept { return _validationStatus.isUnknown(); }

  // beware: returns a *copy* of the vertex id
  [[nodiscard]] VertexType getVertexIdentifier() const {
    return _vertex.getID();
  }

  // beware: returns a *copy* of the edge id
  [[nodiscard]] EdgeType getEdgeIdentifier() const { return _edge.getID(); }

  [[nodiscard]] std::string getCollectionName() const {
    auto collectionNameResult = extractCollectionName(_vertex.getID());
    if (collectionNameResult.fail()) {
      THROW_ARANGO_EXCEPTION(collectionNameResult.result());
    }
    return collectionNameResult.get().first;
  }

  friend auto operator<<(std::ostream& out, ClusterProviderStep const& step)
      -> std::ostream&;

  void setVertexFetched() noexcept {
    if (edgeFetched()) {
      _fetchedStatus = FetchedType::VERTEX_AND_EDGES_FETCHED;
    } else {
      _fetchedStatus = FetchedType::VERTEX_FETCHED;
    }
  }

  void setEdgesFetched() noexcept {
    if (vertexFetched()) {
      _fetchedStatus = FetchedType::VERTEX_AND_EDGES_FETCHED;
    } else {
      _fetchedStatus = FetchedType::EDGES_FETCHED;
    }
  }

  void setValidationResult(ValidationResult res) noexcept {
    _validationStatus = res;
  }

 private:
  Vertex _vertex;
  Edge _edge;
  FetchedType _fetchedStatus;
  ValidationResult _validationStatus;
};

}  // namespace arangodb::graph
