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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "RefactoredTraverserCache.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Aql/TraversalStats.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringHeap.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Graph/BaseOptions.h"
#include "Graph/EdgeDocumentToken.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"
#include "VocBase/LogicalCollection.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::graph;

RefactoredTraverserCache::RefactoredTraverserCache(
    ResourceMonitor& resourceMonitor)
    : _stringHeap(
          resourceMonitor,
          4096), /* arbitrary block-size may be adjusted for performance */
      _resourceMonitor(resourceMonitor) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
}

RefactoredTraverserCache::~RefactoredTraverserCache() { clear(); }

void RefactoredTraverserCache::clear() {
  _resourceMonitor.decreaseMemoryUsage(
      _persistedStrings.size() * sizeof(arangodb::velocypack::HashedStringRef));
  _persistedStrings.clear();
  _stringHeap.clear();
}

arangodb::velocypack::HashedStringRef RefactoredTraverserCache::persistString(
    arangodb::velocypack::HashedStringRef idString) {
  auto it = _persistedStrings.find(idString);
  if (it != _persistedStrings.end()) {
    return *it;
  }
  auto res = _stringHeap.registerString(idString);

  ResourceUsageScope guard(_resourceMonitor, sizeof(res));
  _persistedStrings.emplace(res);

  guard.steal();
  return res;
}
