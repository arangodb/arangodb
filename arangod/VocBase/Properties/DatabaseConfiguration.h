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

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include "Replication2/Version.h"

namespace arangodb {

template<typename T>
class ResultT;

class DataSourceId;
struct UserInputCollectionProperties;

struct DatabaseConfiguration {
  DatabaseConfiguration(
      std::function<DataSourceId()> idGenerator,
      std::function<ResultT<UserInputCollectionProperties>(std::string const&)>
          getCollectionGroupSharding);

  bool isSystemDB = false;
  bool allowExtendedNames = false;
  bool shouldValidateClusterSettings = false;
  uint32_t maxNumberOfShards = 0;

  uint32_t minReplicationFactor = 0;
  uint32_t maxReplicationFactor = 0;
  bool enforceReplicationFactor = true;

  uint64_t defaultNumberOfShards = 1;
  uint64_t defaultReplicationFactor = 1;
  uint64_t defaultWriteConcern = 1;
  std::string defaultDistributeShardsLike = "";
  bool isOneShardDB = false;
  replication::Version replicationVersion = replication::Version::ONE;

  std::function<DataSourceId()> idGenerator;
  std::function<ResultT<UserInputCollectionProperties>(std::string const&)>
      getCollectionGroupSharding;
};
}  // namespace arangodb
