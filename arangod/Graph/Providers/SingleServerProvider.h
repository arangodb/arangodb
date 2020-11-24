////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_GRAPH_PROVIDERS_SINGLESERVERPROVIDER_H
#define ARANGOD_GRAPH_PROVIDERS_SINGLESERVERPROVIDER_H 1

#include "Graph/Cache/RefactoredTraverserCache.h"
#include "Graph/Cursors/RefactoredSingleServerEdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/Providers/BaseStep.h"
#include "Graph/Providers/TypeAliases.h"

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

// Todo this needs to be placed in a more public space.
// It is not depending on this provider
struct IndexAccessor {
  IndexAccessor(transaction::Methods::IndexHandle idx, aql::AstNode* condition,
                std::optional<size_t> memberToUpdate);

  aql::AstNode* getCondition() const;
  transaction::Methods::IndexHandle indexHandle() const;
  std::optional<size_t> getMemberToUpdate() const;

 private:
  transaction::Methods::IndexHandle _idx;
  aql::AstNode* _indexCondition;
  std::optional<size_t> _memberToUpdate;
};

struct BaseProviderOptions {
 public:
  BaseProviderOptions(aql::Variable const* tmpVar, std::vector<IndexAccessor> indexInfo);

  aql::Variable const* tmpVar() const;
  std::vector<IndexAccessor> const& indexInformations() const;

 private:
  // The temporary Variable used in the Indexes
  aql::Variable const* _temporaryVariable;
  // One entry per collection, ShardTranslation needs
  // to be done by Provider
  std::vector<IndexAccessor> _indexInformation;
};

// TODO we need to control from the outside if and which ports of the vertex
// data should be returned THis is most-likely done via Template Parameter like
// this: template<ProduceVertexData>
struct SingleServerProvider {
  enum Direction { FORWARD, BACKWARD };  // TODO check
  enum class LooseEndBehaviour { NEVER, ALLWAYS };

  class Step : public arangodb::graph::BaseStep<Step> {
   public:
    class Vertex {
     public:
      explicit Vertex(VertexType v) : _vertex(v){};

      // NUr implementiert wenn baseOptions>produceVertices()
      // Sonst add `null`.

      void addToBuilder(SingleServerProvider& provider, arangodb::velocypack::Builder& builder) const;

      VertexType data() const;

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

      void addToBuilder(arangodb::velocypack::Builder& builder) const;

     private:
      EdgeDocumentToken _token;
    };

    Step(VertexType v);
    Step(VertexType v, EdgeDocumentToken edge, size_t prev);
    ~Step();

    bool operator<(Step const& other) const noexcept {
      return _vertex < other._vertex;
    }

    Vertex getVertex() const { return _vertex; }
    std::optional<Edge> getEdge() const { return _edge; }

    std::string toString() const {
      return "<Step><Vertex>: " + _vertex.data().toString();
    }
    bool isProcessable() const { return !isLooseEnd(); }
    bool isLooseEnd() const { return false; }

    friend auto operator<<(std::ostream& out, Step const& step) -> std::ostream&;
    
   private:
    Vertex _vertex;
    std::optional<Edge> _edge;
  };

 public:
  SingleServerProvider(arangodb::aql::QueryContext& queryContext, BaseProviderOptions opts);
  SingleServerProvider(SingleServerProvider const&) = delete;
  SingleServerProvider(SingleServerProvider&&) = default;
  ~SingleServerProvider() = default;

  SingleServerProvider& operator=(SingleServerProvider const&) = delete;
  SingleServerProvider& operator=(SingleServerProvider&&) = default;

  auto startVertex(VertexType vertex) -> Step;
  auto fetch(std::vector<Step*> const& looseEnds)
      -> futures::Future<std::vector<Step*>>;                           // rocks
  auto expand(Step const& from, size_t previous) -> std::vector<Step>;  // index

  void insertVertexIntoResult(VertexType vertex, arangodb::velocypack::Builder& builder);
  
  void destroyEngines() {}; // TODO: remove after refactor

  [[nodiscard]] transaction::Methods* trx();

 private:
  void activateCache(bool enableDocumentCache);

  std::unique_ptr<RefactoredSingleServerEdgeCursor> buildCursor();

  [[nodiscard]] arangodb::aql::QueryContext* query() const;

 private:
  std::unique_ptr<RefactoredSingleServerEdgeCursor> _cursor;
  // Unique_ptr to have this class movable, and to keep reference of trx()
  // alive
  std::unique_ptr<arangodb::transaction::Methods> _trx;
  arangodb::aql::QueryContext* _query;
  // We DO take responsibility for the Cache (TODO?)
  RefactoredTraverserCache _cache;

  BaseProviderOptions _opts;
};
}  // namespace graph
}  // namespace arangodb

#endif
