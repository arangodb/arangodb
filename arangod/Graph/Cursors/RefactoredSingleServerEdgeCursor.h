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

#pragma once

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/Expression.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Aql/QueryContext.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/Methods.h"

// Note: only used for NonConstExpressionContainer
// Could be extracted to it's own file.
#include "Aql/OptimizerUtils.h"

#include "Graph/Providers/TypeAliases.h"

#include <vector>

namespace arangodb {

class LocalDocumentId;

namespace aql {
class TraversalStats;
}  // namespace aql

namespace velocypack {
class Builder;
class HashedStringRef;
}  // namespace velocypack

namespace graph {
struct IndexAccessor;

struct EdgeDocumentToken;

template<class StepType>
class SingleServerProvider;

template<class StepType>
class RefactoredSingleServerEdgeCursor {
 public:
  struct LookupInfo {
    LookupInfo(IndexAccessor* accessor);

    ~LookupInfo();

    LookupInfo(LookupInfo const&) = delete;
    LookupInfo(LookupInfo&&);
    LookupInfo& operator=(LookupInfo const&) = delete;

    void rearmVertex(VertexType vertex, transaction::Methods* trx,
                     arangodb::aql::Variable const* tmpVar);

    IndexIterator& cursor();
    aql::Expression* getExpression();

    size_t getCursorID() const;

    void calculateIndexExpressions(aql::Ast* ast, aql::ExpressionContext& ctx);

   private:
    IndexAccessor* _accessor;

    std::unique_ptr<IndexIterator> _cursor;
  };

  enum Direction { FORWARD, BACKWARD };

 public:
  RefactoredSingleServerEdgeCursor(
      transaction::Methods* trx, arangodb::aql::Variable const* tmpVar,
      std::vector<IndexAccessor>& globalIndexConditions,
      std::unordered_map<uint64_t, std::vector<IndexAccessor>>&
          depthBasedIndexConditions,
      arangodb::aql::FixedVarExpressionContext& expressionContext,
      bool requiresFullDocument);

  ~RefactoredSingleServerEdgeCursor();

  using Callback = std::function<void(EdgeDocumentToken&&,
                                      arangodb::velocypack::Slice, size_t)>;

 private:
  aql::Variable const* _tmpVar;
  std::vector<LookupInfo> _lookupInfo;
  std::unordered_map<uint64_t, std::vector<LookupInfo>> _depthLookupInfo;

  transaction::Methods* _trx;
  arangodb::aql::FixedVarExpressionContext& _expressionCtx;
  bool _requiresFullDocument;

 public:
  void readAll(SingleServerProvider<StepType>& provider,
               aql::TraversalStats& stats, size_t depth,
               Callback const& callback);

  void rearm(VertexType vertex, uint64_t depth);

  void prepareIndexExpressions(aql::Ast* ast);

  bool evaluateEdgeExpression(arangodb::aql::Expression* expression,
                              VPackSlice value);

 private:
  auto getLookupInfos(uint64_t depth) -> std::vector<LookupInfo>&;
};
}  // namespace graph
}  // namespace arangodb
