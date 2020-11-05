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
  enum Direction { FORWARD, BACKWARD };

  using VertexType = arangodb::velocypack::HashedStringRef;
  using Step = arangodb::velocypack::HashedStringRef;
  // using EdgeType = MockGraph::EdgeDef;
  using VertexRef = arangodb::velocypack::HashedStringRef;

 public:
  SingleServerProvider(arangodb::transaction::Methods* trx);
  ~SingleServerProvider();

  auto startVertex(Step vertex) -> Step;
  auto fetch(std::vector<Step> const& looseEnds)
      -> futures::Future<std::vector<Step>>;                            // rocks
  auto expand(Step const& from, size_t previous) -> std::vector<Step>;  // index

 private:
  std::unique_ptr<RefactoredSingleServerEdgeCursor> _cursor;

 protected:
  arangodb::transaction::Methods* _trx;
  arangodb::aql::QueryContext* _query;

 private:
  std::unique_ptr<RefactoredSingleServerEdgeCursor> buildCursor();
  void clearCursor(Step vertex);
  transaction::Methods* trx() const;
  arangodb::aql::QueryContext* query() const;
};
}  // namespace graph
}  // namespace arangodb

#endif
