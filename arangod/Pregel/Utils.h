////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <cstdint>

#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;
namespace pregel {

class WorkerConfig;

class Utils {
  Utils() = delete;

 public:
  // constants
  static std::string const apiPrefix;

  static std::string const executionNumberKey;
  static std::string const senderKey;

  static std::string const threshold;
  static std::string const maxNumIterations;
  static std::string const maxGSS;
  static std::string const useMemoryMapsKey;
  static std::string const parallelismKey;

  static std::string const globalSuperstepKey;
  static std::string const phaseFirstStepKey;
  static std::string const aggregatorValuesKey;

  static size_t const batchOfVerticesStoredBeforeUpdatingStatus;
  static size_t const batchOfVerticesProcessedBeforeUpdatingStatus;

  static int64_t countDocuments(TRI_vocbase_t* vocbase,
                                std::string const& collection);

  static ErrorCode resolveShard(ClusterInfo& ci, WorkerConfig const* config,
                                std::string const& collectionName,
                                std::string const& shardKey,
                                std::string_view vertexKey,
                                std::string& responsibleShard);
};
}  // namespace pregel
}  // namespace arangodb
