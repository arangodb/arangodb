////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_GRAPH_CURSORS_REFACTORED_CLUSTER_EDGECURSOR_H
#define ARANGOD_GRAPH_CURSORS_REFACTORED_CLUSTER_EDGECURSOR_H 1

#include "Aql/Expression.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Aql/QueryContext.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/Methods.h"

#include "Graph/Providers/TypeAliases.h"
#include "Graph/Cache/RefactoredClusterTraverserCache.h"

#include <vector>

namespace arangodb {

class LocalDocumentId;

namespace aql {
class TraversalStats;
}

namespace velocypack {
class Builder;
class HashedStringRef;
}  // namespace velocypack

namespace graph {
struct IndexAccessor;

struct EdgeDocumentToken;

class RefactoredClusterEdgeCursor {
 public:

  enum Direction { FORWARD, BACKWARD };

 public:
  RefactoredClusterEdgeCursor(arangodb::transaction::Methods* trx,
                              arangodb::aql::FixedVarExpressionContext const& expressionContext,
                              RefactoredClusterTraverserCache* cache, bool backward);
  ~RefactoredClusterEdgeCursor();

  using Callback =
      std::function<void(EdgeDocumentToken&&, arangodb::velocypack::Slice, size_t)>;

 private:
  arangodb::transaction::Methods* _trx;
  arangodb::aql::FixedVarExpressionContext const& _expressionContext;

 public:
  void readAll(aql::TraversalStats& stats, Callback const& callback);
  void rearm();

 private:
  [[nodiscard]] transaction::Methods* trx() const;
  [[nodiscard]] arangodb::aql::FixedVarExpressionContext const& getExpressionContext();

 protected:
  std::vector<arangodb::velocypack::Slice> _edgeList;
  size_t _cursorPosition;
};
}  // namespace graph
}  // namespace arangodb

#endif
