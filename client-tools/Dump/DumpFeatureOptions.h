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

struct DumpFeatureOptions {
  std::vector<std::string> collections{};
  // Collections in here, will be ignored during the dump
  std::vector<std::string> collectionsToBeIgnored{};
  std::vector<std::string> shards{};
  std::string outputPath{};
  std::string maskingsFile{};
  uint64_t docsPerBatch{1000 * 10};
  uint64_t initialChunkSize{1024 * 1024 * 8};
  uint64_t maxChunkSize{1024 * 1024 * 64};
  // actual default value depends on the number of available cores
  uint32_t threadCount{2};
  bool allDatabases{false};
  bool clusterMode{false};
  bool dumpData{true};
  bool dumpViews{true};
  bool force{false};
  bool ignoreDistributeShardsLikeErrors{false};
  bool includeSystemCollections{false};
  bool overwrite{false};
  bool progress{true};
  bool useGzipForStorage{true};
  bool useVPack{false};
  bool useParalleDump{true};
  bool splitFiles{false};
  std::uint64_t dbserverWorkerThreads{5};
  std::uint64_t dbserverPrefetchBatches{5};
  std::uint64_t localWriterThreads{5};
  std::uint64_t localNetworkThreads{4};
};

}  // namespace arangodb
