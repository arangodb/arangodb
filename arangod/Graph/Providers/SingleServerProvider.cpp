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

#include "./SingleServerProvider.h"

#include "Aql/QueryContext.h"
#include "Graph/Cursors/RefactoredSingleServerEdgeCursor.h"

#include "Futures/Future.h"
#include "Futures/Utilities.h"

#include <vector>

using namespace arangodb;
using namespace arangodb::graph;

SingleServerProvider::SingleServerProvider(arangodb::transaction::Methods* trx,
                                           arangodb::aql::QueryContext* queryContext)
    : _trx(trx), _query(queryContext) {
  buildCursor();
}

SingleServerProvider::~SingleServerProvider() = default;

auto SingleServerProvider::startVertex(arangodb::velocypack::StringRef vertex,
                                       bool lazy) -> Step {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Start Vertex:" << vertex;
  clearCursor(vertex);
  if (!lazy) {
    // get data
    // return step
  }
  // if lazy return loosEnd step
  return Step();  // TODO
}

auto SingleServerProvider::fetch(std::vector<Step*> const& looseEnds)
    -> futures::Future<std::vector<Step*>> {
  LOG_TOPIC("78156", TRACE, Logger::GRAPHS)
      << "<MockGraphProvider> Fetching...";
  std::vector<Step*> result{};
  result.reserve(looseEnds.size());
  // for (auto* s : looseEnds) {
  // Get data
  // // TODO
  return futures::makeFuture(std::move(result));
}

std::unique_ptr<RefactoredSingleServerEdgeCursor> SingleServerProvider::buildCursor() {
  return std::make_unique<RefactoredSingleServerEdgeCursor>(trx(), query());
}

void SingleServerProvider::clearCursor(arangodb::velocypack::StringRef vertex) {
  _cursor->rearm(vertex, 0);
}

arangodb::transaction::Methods* SingleServerProvider::trx() const {
  return _trx;
}

arangodb::aql::QueryContext* SingleServerProvider::query() const {
  return _query;
}