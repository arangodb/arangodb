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

#include "Graph/Providers/BaseProviderOptions.h"

using namespace arangodb;
using namespace arangodb::graph;

BaseProviderOptions::BaseProviderOptions(aql::Variable const* tmpVar,
                                         std::vector<IndexAccessor> indexInfo)
    : _temporaryVariable(tmpVar), _indexInformation(std::move(indexInfo)) {}

aql::Variable const* BaseProviderOptions::tmpVar() const {
  return _temporaryVariable;
}

std::vector<IndexAccessor> const& BaseProviderOptions::indexInformations() const {
  return _indexInformation;
}

ClusterBaseProviderOptions::ClusterBaseProviderOptions(arangodb::aql::FixedVarExpressionContext const& expressionContext,
                                                       ClusterTraverserCache* cache)
    : _expressionCtx(expressionContext), _cache(cache) {}

ClusterTraverserCache* ClusterBaseProviderOptions::getCache() {
  TRI_ASSERT(_cache != nullptr);
  return _cache;
}

arangodb::aql::FixedVarExpressionContext const& ClusterBaseProviderOptions::getExpressionContext() {
  return _expressionCtx;
}