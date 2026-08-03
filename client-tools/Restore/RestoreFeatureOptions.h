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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace arangodb {

struct RestoreFeatureOptions {
  std::vector<std::string> collections{};
  std::vector<std::string> views{};
  std::string inputPath{};
  uint64_t chunkSize{1024 * 1024 * 8};
  uint64_t defaultNumberOfShards{1};     // deprecated
  uint64_t defaultReplicationFactor{1};  // deprecated
  uint64_t maxUnusedBufferSize{1024 * 1024 * 512};
  std::vector<std::string> numberOfShards;
  std::vector<std::string> replicationFactor;
  std::vector<std::string> writeConcern;
  uint32_t threadCount{2};
  uint32_t initialConnectRetries{3};
  bool clusterMode{false};
  bool createDatabase{false};
  bool force{false};
  bool forceSameDatabase{false};
  bool allDatabases{false};
  bool ignoreDistributeShardsLikeErrors{false};
  bool importData{true};
  bool importStructure{true};
  bool includeSystemCollections{false};
  bool overwrite{true};
  bool enableRevisionTrees{true};
  bool continueRestore{false};
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  bool failOnUpdateContinueFile{false};
#endif
  bool cleanupDuplicateAttributes{false};
  bool progress{true};
};

}  // namespace arangodb
