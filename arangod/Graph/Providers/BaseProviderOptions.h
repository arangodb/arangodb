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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_PROVIDER_BASEPROVIDEROPTIONS_H
#define ARANGOD_GRAPH_PROVIDER_BASEPROVIDEROPTIONS_H 1

#include "Transaction/Methods.h"

#include <Aql/FixedVarExpressionContext.h>
#include <Graph/ClusterTraverserCache.h>
#include <optional>
#include <vector>

namespace arangodb {

namespace aql {
class QueryContext;
}

namespace graph {

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

struct ClusterBaseProviderOptions {
 public:
  ClusterBaseProviderOptions(arangodb::aql::FixedVarExpressionContext expressionContext, ClusterTraverserCache* cache);

  arangodb::aql::FixedVarExpressionContext const& getExpressionContext() const;
  ClusterTraverserCache* getCache();

 private:
  arangodb::aql::FixedVarExpressionContext _expressionCtx;
  ClusterTraverserCache* _cache;
};

}  // namespace graph
}  // namespace arangodb

#endif
