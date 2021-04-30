////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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
#ifndef TESTS_MOCK_GRAPH_PROVIDER_H
#define TESTS_MOCK_GRAPH_PROVIDER_H

#include <numeric>
#include <ostream>
#include <unordered_map>
#include <vector>

#include "./MockGraph.h"
#include "Aql/TraversalStats.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Transaction/Methods.h"

#include "Graph/Providers/BaseStep.h"

#include <velocypack/HashedStringRef.h>

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

namespace tests {
namespace graph {
class MockGraphProvider {
  using VertexType = arangodb::velocypack::HashedStringRef;
  using EdgeType = MockGraph::EdgeDef;

 public:
  enum class LooseEndBehaviour { NEVER, ALWAYS };

  class Step : public arangodb::graph::BaseStep<Step> {
   public:
    class Vertex {
     public:
      explicit Vertex(VertexType v) : _vertex(v) { _depth = 0; };
      explicit Vertex(VertexType v, size_t depth) : _vertex(v), _depth(depth){};

      VertexType getID() const { return _vertex; }
      size_t getDepth() const { return _depth; }

      // Make the set work on the VertexRef attribute only
      bool operator<(Vertex const& other) const noexcept {
        return _vertex < other._vertex;
      }

      bool operator>(Vertex const& other) const noexcept {
        return !operator<(other);
      }

     private:
      VertexType _vertex;
      size_t _depth;
    };

    class Edge {
     public:
      Edge(EdgeType e) : _edge(e){};

      std::string toString() const {
        return "Edge - _from: " + _edge._from + ", _to: " + _edge._to;
      }

      EdgeType getEdge() const { return _edge; }
      bool isValid() const {
        if (_edge._from.empty() && _edge._to.empty()) {
          return false;
        }
        return true;
      };

     private:
      EdgeType _edge;
    };

    Step(VertexType v, bool isProcessable);
    Step(size_t prev, VertexType v, EdgeType e, bool isProcessable);
    Step(size_t prev, VertexType v, EdgeType e, bool isProcessable, size_t depth);
    ~Step();

    bool operator<(Step const& other) const noexcept {
      return _vertex < other._vertex;
    }

    std::string toString() const {
      if (_edge.isValid()) {
        return "<Step><Vertex>: " + _vertex.getID().toString() +
               ", <Edge>:" + _edge.toString() +
               ", previous: " + basics::StringUtils::itoa(getPrevious());
      } else {
        return "<Step><Vertex>: " + _vertex.getID().toString() +
               ", previous: " + basics::StringUtils::itoa(getPrevious());
      }
    }

    Vertex getVertex() const {
      /*if (!isProcessable()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "Accessing vertex (" + _vertex.data().toString() +
                                           "), before fetching it");
      }*/
      return _vertex;
    }

    Edge getEdge() const {
      /*if (!isProcessable()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "Accessing edge (" + _edge.toString() +
                                           "), before fetching it");
      }*/
      return _edge;
    }

    VertexType getVertexIdentifier() const { return getVertex().getID(); }
    size_t getDepth() const { return getVertex().getDepth(); }

    bool isProcessable() const { return _isProcessable; }

    bool isLooseEnd() const { return !isProcessable(); }

    void resolve() {
      TRI_ASSERT(!isProcessable());
      _isProcessable = true;
    }

    friend auto operator<<(std::ostream& out, Step const& step) -> std::ostream&;

   private:
    Vertex _vertex;
    Edge _edge;
    bool _isProcessable;
  };

  MockGraphProvider() = delete;
  MockGraphProvider(MockGraph const& data, arangodb::aql::QueryContext& queryContext,
                    LooseEndBehaviour looseEnds, bool reverse = false);
  MockGraphProvider(MockGraphProvider const&) = delete;  // TODO: check "Rule of 5"
  MockGraphProvider(MockGraphProvider&&) = default;
  ~MockGraphProvider();

  MockGraphProvider& operator=(MockGraphProvider const&) = delete;
  MockGraphProvider& operator=(MockGraphProvider&&) = default;

  void destroyEngines(){};
  auto startVertex(VertexType vertex) -> Step;
  auto fetch(std::vector<Step*> const& looseEnds) -> futures::Future<std::vector<Step*>>;
  auto expand(Step const& from, size_t previous) -> std::vector<Step>;
  auto expand(Step const& from, size_t previous, std::function<void(Step)> callback) -> void;

  void addVertexToBuilder(Step::Vertex const& vertex, arangodb::velocypack::Builder& builder);
  void addEdgeToBuilder(Step::Edge const& edge, arangodb::velocypack::Builder& builder);

  [[nodiscard]] transaction::Methods* trx();

  aql::TraversalStats stealStats();

 private:
  auto decideProcessable() const -> bool;

 private:
  std::unordered_map<std::string, std::vector<MockGraph::EdgeDef>> _fromIndex;
  std::unordered_map<std::string, std::vector<MockGraph::EdgeDef>> _toIndex;
  arangodb::transaction::Methods _trx;
  bool _reverse;
  LooseEndBehaviour _looseEnds;
  arangodb::aql::TraversalStats _stats;
};
}  // namespace graph
}  // namespace tests
}  // namespace arangodb

#endif
