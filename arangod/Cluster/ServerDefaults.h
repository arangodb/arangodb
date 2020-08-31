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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_SERVERDEFAULTS_H
#define ARANGOD_CLUSTER_SERVERDEFAULTS_H

#include <cstdint>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

struct ServerDefaults {
  uint64_t numberOfShards = 1;
  uint64_t replicationFactor = 1;
  uint64_t writeConcern = 1;

  explicit ServerDefaults(application_features::ApplicationServer& server);
};

}  // namespace arangodb

#endif  // ARANGOD_CLUSTER_SERVERDEFAULTS_H
