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

class MockGraphProviderOptions {
 public:
  enum class LooseEndBehaviour { NEVER, ALWAYS };
  MockGraphProviderOptions(MockGraph const& data, LooseEndBehaviour looseEnds,
                           bool reverse = false)
      : _data(data), _looseEnds(looseEnds), _reverse(reverse) {}
  ~MockGraphProviderOptions() = default;

  LooseEndBehaviour looseEnds() const { return _looseEnds; }
  MockGraph const& data() const { return _data; }
  bool reverse() const { return _reverse; }

 private:
  MockGraph const& _data;
  LooseEndBehaviour _looseEnds;
  bool _reverse;
  ;
};

class MockGraphProvider {
  using VertexType = arangodb::velocypack::HashedStringRef;
  using EdgeType = MockGraph::EdgeDef;

 public:
  using Options = MockGraphProviderOptions;
  using LooseEndBehaviour = typename MockGraphProviderOptions::LooseEndBehaviour;

  class Step : public arangodb::graph::BaseStep<Step> {
   public:
    class Vertex {
     public:
      explicit Vertex(VertexType v) : _vertex(v){};

      VertexType getID() const { return _vertex; }

      // Make the set work on the VertexRef attribute only
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
    Step(size_t prev, VertexType v, bool isProcessable, size_t depth);
    Step(size_t prev, VertexType v, EdgeType e, bool isProcessable, size_t depth);
    ~Step() = default;

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

    bool isResponsible(transaction::Methods* trx) const { return true; }

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

    std::string getCollectionName() const {
      auto collectionNameResult = extractCollectionName(_vertex.getID());
      if (collectionNameResult.fail()) {
        THROW_ARANGO_EXCEPTION(collectionNameResult.result());
      }
      return collectionNameResult.get().first;
    };

    void setLocalSchreierIndex(size_t index) {
      TRI_ASSERT(index != std::numeric_limits<size_t>::max());
      TRI_ASSERT(!hasLocalSchreierIndex());
      _localSchreierIndex = index;
    }

    bool hasLocalSchreierIndex() const {
      return _localSchreierIndex != std::numeric_limits<size_t>::max();
    }

    std::size_t getLocalSchreierIndex() const { return _localSchreierIndex; }

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
    size_t _localSchreierIndex;
  };

  MockGraphProvider() = delete;
  MockGraphProvider(arangodb::aql::QueryContext& queryContext, Options opts,
                    arangodb::ResourceMonitor& resourceMonitor);

  MockGraphProvider(MockGraphProvider const&) = delete;  // TODO: check "Rule of 5"
  MockGraphProvider(MockGraphProvider&&) = default;
  ~MockGraphProvider();

  MockGraphProvider& operator=(MockGraphProvider const&) = delete;
  MockGraphProvider& operator=(MockGraphProvider&&) = default;

  void destroyEngines(){};
  auto startVertex(VertexType vertex, size_t depth = 0, double weight = 0.0) -> Step;
  auto fetch(std::vector<Step*> const& looseEnds) -> futures::Future<std::vector<Step*>>;
  auto expand(Step const& from, size_t previous) -> std::vector<Step>;
  auto expand(Step const& from, size_t previous, std::function<void(Step)> callback) -> void;
  auto clear() -> void;

  void addVertexToBuilder(Step::Vertex const& vertex, arangodb::velocypack::Builder& builder);
  void addEdgeToBuilder(Step::Edge const& edge, arangodb::velocypack::Builder& builder);

  void prepareIndexExpressions(aql::Ast* ast);

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
