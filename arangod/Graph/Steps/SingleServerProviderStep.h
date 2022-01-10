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

#include "Transaction/Methods.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/Providers/BaseStep.h"
#include "Graph/Providers/TypeAliases.h"

#pragma once

namespace arangodb {
namespace graph {

template<class StepType>
class SingleServerProvider;

class SingleServerProviderStep
    : public arangodb::graph::BaseStep<SingleServerProviderStep> {
 public:
  using StepType = EdgeDocumentToken;

 public:
  class Vertex {
   public:
    explicit Vertex(VertexType v) : _vertex(v) {}

    VertexType const& getID() const;

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
    explicit Edge(EdgeDocumentToken tkn) noexcept : _token(std::move(tkn)) {}
    Edge() noexcept : _token() {}

    void addToBuilder(SingleServerProvider<SingleServerProviderStep>& provider,
                      arangodb::velocypack::Builder& builder) const;
    StepType const& getID() const;
    bool isValid() const;

   private:
    EdgeDocumentToken _token;
  };

  SingleServerProviderStep(VertexType v);
  SingleServerProviderStep(VertexType v, EdgeDocumentToken edge, size_t prev);
  SingleServerProviderStep(VertexType v, EdgeDocumentToken edge, size_t prev,
                           size_t depth, double weight, size_t);
  SingleServerProviderStep(VertexType v, size_t depth, double weight = 0.0);
  ~SingleServerProviderStep();

  bool operator<(SingleServerProviderStep const& other) const noexcept {
    return _vertex < other._vertex;
  }

  Vertex const& getVertex() const { return _vertex; }
  Edge const& getEdge() const { return _edge; }

  std::string toString() const {
    return "<Step><Vertex>: " + _vertex.getID().toString() +
           ", <Edge>: " + std::to_string(_edge.getID().localDocumentId().id());
  }
  bool isProcessable() const { return !isLooseEnd(); }
  bool isLooseEnd() const { return false; }

  ::arangodb::graph::VertexType getVertexIdentifier() const {
    return _vertex.getID();
  }

  StepType getEdgeIdentifier() const { return _edge.getID(); }

  std::string getCollectionName() const {
    auto collectionNameResult = extractCollectionName(_vertex.getID());
    if (collectionNameResult.fail()) {
      THROW_ARANGO_EXCEPTION(collectionNameResult.result());
    }
    return collectionNameResult.get().first;
  };

  bool isResponsible(transaction::Methods* trx) const;

  friend auto operator<<(std::ostream& out,
                         SingleServerProviderStep const& step) -> std::ostream&;

 private:
  Vertex _vertex;
  Edge _edge;
};
}  // namespace graph
}  // namespace arangodb
