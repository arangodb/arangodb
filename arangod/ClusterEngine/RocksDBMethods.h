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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_ENGINE_ROCKSDB_METHODS_H
#define ARANGOD_CLUSTER_ENGINE_ROCKSDB_METHODS_H 1

#include "Basics/Result.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace rocksdb {
////////////////////////////////////////////////////////////////////////////////
/// @brief recalculate collection count on all DBServers
////////////////////////////////////////////////////////////////////////////////

Result recalculateCountsOnAllDBServers(application_features::ApplicationServer& server,
                                       std::string const& dbname,
                                       std::string const& collname);

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for estimator sync on all DBServers
////////////////////////////////////////////////////////////////////////////////

Result waitForEstimatorSync();
}  // namespace rocksdb
}  // namespace arangodb

#endif
