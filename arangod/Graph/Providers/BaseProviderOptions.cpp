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
                                         std::vector<IndexAccessor> indexInfo,
                                         std::map<std::string, std::string> const& collectionToShardMap)
    : _temporaryVariable(tmpVar),
      _indexInformation(std::move(indexInfo)),
      _collectionToShardMap(collectionToShardMap) {}

aql::Variable const* BaseProviderOptions::tmpVar() const {
  return _temporaryVariable;
}

std::vector<IndexAccessor> const& BaseProviderOptions::indexInformations() const {
  return _indexInformation;
}

std::map<std::string, std::string> const& BaseProviderOptions::collectionToShardMap() const {
  return _collectionToShardMap;
}

ClusterBaseProviderOptions::ClusterBaseProviderOptions(
    std::shared_ptr<RefactoredClusterTraverserCache> cache,
    std::unordered_map<ServerID, aql::EngineId> const* engines, bool backward)
    : _cache(std::move(cache)), _engines(engines), _backward(backward) {
  TRI_ASSERT(_cache != nullptr);
  TRI_ASSERT(_engines != nullptr);
}

RefactoredClusterTraverserCache* ClusterBaseProviderOptions::getCache() {
  TRI_ASSERT(_cache != nullptr);
  return _cache.get();
}

bool ClusterBaseProviderOptions::isBackward() const { return _backward; }

std::unordered_map<ServerID, aql::EngineId> const* ClusterBaseProviderOptions::engines() const {
  TRI_ASSERT(_engines != nullptr);
  return _engines;
}