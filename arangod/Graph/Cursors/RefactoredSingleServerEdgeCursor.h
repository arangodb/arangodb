////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/Expression.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Aql/InAndOutRowExpressionContext.h"
#include "Aql/QueryContext.h"
#include "Containers/FlatHashMap.h"
#include "Graph/Providers/TypeAliases.h"
#include "Indexes/IndexIterator.h"

#include <cstdint>
#include <vector>

namespace arangodb {
struct ResourceMonitor;

namespace aql {
class TraversalStats;
struct Variable;
}  // namespace aql

namespace transaction {
class Methods;
}

namespace graph {
struct IndexAccessor;

struct EdgeDocumentToken;

template<class StepType>
class SingleServerProvider;

template<class StepType>
class RefactoredSingleServerEdgeCursor {
 public:
  struct LookupInfo {
    explicit LookupInfo(IndexAccessor* accessor);

    ~LookupInfo();

    LookupInfo(LookupInfo const&) = delete;
    LookupInfo(LookupInfo&&);
    LookupInfo& operator=(LookupInfo const&) = delete;

    IndexIterator& cursor();
    aql::Expression* getExpression();

    size_t getCursorID() const;

    uint16_t coveringIndexPosition() const noexcept;

    void calculateIndexExpressions(aql::Ast* ast, aql::ExpressionContext& ctx);

    void rearmVertex(VertexType vertex, ResourceMonitor& monitor,
                     transaction::Methods* trx,
                     arangodb::aql::Variable const* tmpVar,
                     aql::TraversalStats& stats, bool useCache);

   private:
    IndexAccessor* _accessor;

    std::unique_ptr<IndexIterator> _cursor;

    uint16_t _coveringIndexPosition;
  };

  enum Direction { FORWARD, BACKWARD };

  RefactoredSingleServerEdgeCursor(
      ResourceMonitor& monitor, transaction::Methods* trx,
      arangodb::aql::Variable const* tmpVar,
      std::vector<IndexAccessor>& globalIndexConditions,
      std::unordered_map<uint64_t, std::vector<IndexAccessor>>&
          depthBasedIndexConditions,
      arangodb::aql::FixedVarExpressionContext& expressionContext,
      bool requiresFullDocument, bool useCache);

  ~RefactoredSingleServerEdgeCursor();

  using Callback = std::function<void(EdgeDocumentToken&&,
                                      arangodb::velocypack::Slice, size_t)>;

  void readAll(SingleServerProvider<StepType>& provider,
               aql::TraversalStats& stats, size_t depth,
               Callback const& callback);

  void prepareIndexExpressions(aql::Ast* ast);

  bool evaluateEdgeExpression(arangodb::aql::Expression* expression,
                              VPackSlice value);

  bool hasDepthSpecificLookup(uint64_t depth) const noexcept;

  void rearm(VertexType vertex, uint64_t depth, aql::TraversalStats& stats);

 private:
  auto getLookupInfos(uint64_t depth) -> std::vector<LookupInfo>&;

  aql::Variable const* _tmpVar;
  std::vector<LookupInfo> _lookupInfo;
  containers::FlatHashMap<uint64_t, std::vector<LookupInfo>> _depthLookupInfo;

  ResourceMonitor& _monitor;
  transaction::Methods* _trx;
  // Only works with hardcoded variables
  arangodb::aql::FixedVarExpressionContext& _expressionCtx;

  // TODO [GraphRefactor]: This is currently unused. Ticket: #GORDO-1364
  // Will be implemented in the future (Performance Optimization).
  bool const _requiresFullDocument;

  bool const _useCache;
};
}  // namespace graph
}  // namespace arangodb
