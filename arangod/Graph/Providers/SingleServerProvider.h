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
#include "Graph/Providers/BaseStep.h"
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

struct SingleServerProvider {
  using VertexType = arangodb::velocypack::StringRef;
  using VertexRef = arangodb::velocypack::StringRef; // TODO: Change back to HashRef

  enum Direction { FORWARD, BACKWARD };  // TODO check
  enum class LooseEndBehaviour { NEVER, ALLWAYS };

  class Step : public arangodb::graph::BaseStep<Step> {
   public:
    class Vertex {
     public:
      explicit Vertex(VertexType v) : _vertex(v){};

      void addToBuilder(arangodb::velocypack::Builder& builder) const;
      VertexType getId() const;  // TODO hashed?
      VertexType data() const { return _vertex; }

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

    Step(VertexType v);
    Step(VertexType v, size_t prev, bool isLooseEnd);
    ~Step();

    bool operator<(Step const& other) const noexcept {
      return _vertex < other._vertex;
    }

    Vertex getVertex() const { return _vertex; }

    bool isLooseEnd() const { return _isLooseEnd; }
    void setLooseEnd(bool isLooseEnd) { _isLooseEnd = false; }

   private:
    Vertex _vertex;
    bool _isLooseEnd;
  };

 public:
  SingleServerProvider(arangodb::transaction::Methods* trx,
                       arangodb::aql::QueryContext* queryContext);
  ~SingleServerProvider();

  auto startVertex(arangodb::velocypack::StringRef vertex) -> Step;
  auto fetch(std::vector<Step*> const& looseEnds)
      -> futures::Future<std::vector<Step*>>;                           // rocks
  auto expand(Step const& from, size_t previous) -> std::vector<Step>;  // index

 private:
  void activateCache(bool enableDocumentCache);

  std::unique_ptr<RefactoredSingleServerEdgeCursor> buildCursor();
  [[nodiscard]] transaction::Methods* trx() const;           // TODO nodiscard?
  [[nodiscard]] arangodb::aql::QueryContext* query() const;  // TODO nodiscard?

 private:
  std::unique_ptr<RefactoredSingleServerEdgeCursor> _cursor;
  // We DO take responsibility for the Cache (TODO?)
  arangodb::transaction::Methods* _trx;
  arangodb::aql::QueryContext* _query;
  RefactoredTraverserCache _cache;
};
}  // namespace graph
}  // namespace arangodb

#endif
