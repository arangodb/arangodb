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
  using VertexType = arangodb::velocypack::HashedStringRef;
  using VertexRef = arangodb::velocypack::HashedStringRef;

  enum Direction { FORWARD, BACKWARD };  // TODO check
  enum class LooseEndBehaviour { NEVER, ALLWAYS };

  class Step : public arangodb::graph::BaseStep<Step> {
   public:
    class Vertex {
     public:
      explicit Vertex(VertexType v) : _vertex(v){};

      void addToBuilder(arangodb::velocypack::Builder& builder) const;
      VertexRef getId() const;
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

    Step();
    Step(VertexType v, size_t prev, bool isLooseEnd);
    ~Step();

    bool operator<(Step const& other) const noexcept {
      return _vertex < other._vertex;
    }

    Vertex getVertex() const { return _vertex; }

    bool isLooseEnd() const { return _isLooseEnd; }

   private:
    Vertex _vertex;
    bool _isLooseEnd;
  };

 public:
  SingleServerProvider(arangodb::transaction::Methods* trx,
                       arangodb::aql::QueryContext* queryContext);
  ~SingleServerProvider();

  auto startVertex(arangodb::velocypack::StringRef vertex, bool lazy = false) -> Step;
  auto fetch(std::vector<Step*> const& looseEnds)
      -> futures::Future<std::vector<Step*>>;                           // rocks
  auto expand(Step const& from, size_t previous) -> std::vector<Step>;  // index

 private:
  std::unique_ptr<RefactoredSingleServerEdgeCursor> buildCursor();
  void clearCursor(arangodb::velocypack::StringRef vertex);
  transaction::Methods* trx() const;
  arangodb::aql::QueryContext* query() const;

 private:
  std::unique_ptr<RefactoredSingleServerEdgeCursor> _cursor;

 protected:
  arangodb::transaction::Methods* _trx;
  arangodb::aql::QueryContext* _query;
};
}  // namespace graph
}  // namespace arangodb

#endif
