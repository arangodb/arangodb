////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

// TODO (COR-867): Remove this file by cutting the link to GeneralServerFeature.cpp

#include "RocksDBEngine/RocksDBRestHandlers.h"
#include "ClusterEngine/ClusterRestHandlers.h"

namespace arangodb {

// These no-op implementations are needed because of GeneralServerFeature.cpp.
// It calls registerResources for both RocksDBEngine and ClusterEngine, but
// this test only links arango_rocksdb (which lacks these methods'
// implementations).
void RocksDBRestHandlers::registerResources(rest::RestHandlerFactory*,
                                            StorageEngine&) {}
void ClusterRestHandlers::registerResources(rest::RestHandlerFactory*) {}

}  // namespace arangodb