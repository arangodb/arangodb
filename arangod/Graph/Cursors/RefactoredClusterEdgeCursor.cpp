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

#include "./RefactoredClusterEdgeCursor.h"
#include <Aql/AstNode.h>

#include "Cluster/ClusterMethods.h"
#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::graph;

RefactoredClusterEdgeCursor::RefactoredClusterEdgeCursor(
    arangodb::transaction::Methods* trx,
    arangodb::aql::FixedVarExpressionContext const& expressionContext,
    RefactoredClusterTraverserCache* cache, bool backward)
    : _trx(trx), _expressionContext(expressionContext), _cache(cache), _backward(backward) {}

RefactoredClusterEdgeCursor::~RefactoredClusterEdgeCursor() {}

#ifdef USE_ENTERPRISE
static bool CheckInaccessible(transaction::Methods* trx, VPackSlice const& edge) {
  // for skipInaccessibleCollections we need to check the edge
  // document, in that case nextWithExtra has no benefit
  TRI_ASSERT(edge.isString());
  arangodb::velocypack::StringRef str(edge);
  size_t pos = str.find('/');
  TRI_ASSERT(pos != std::string::npos);
  return trx->isInaccessibleCollection(str.substr(0, pos).toString());
}
#endif

void RefactoredClusterEdgeCursor::rearm() {
  _edgeList.clear();
  _cursorPosition = 0;
}

void RefactoredClusterEdgeCursor::readAll(aql::TraversalStats& stats, Callback const& callback) {
  for (VPackSlice const& edge : _edgeList) {
    callback(EdgeDocumentToken(edge), edge, _cursorPosition);
  }
}

arangodb::transaction::Methods* RefactoredClusterEdgeCursor::trx() const {
  return _trx;
}

arangodb::aql::FixedVarExpressionContext const& RefactoredClusterEdgeCursor::getExpressionContext() {
  return _expressionContext;
}