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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>

#include "Network/ConnectionPool.h"

namespace arangodb {

struct NetworkOptions {
  // Network flight control constants
  static constexpr std::uint64_t MaxAllowedInFlight = 65536;
  static constexpr std::uint64_t MinAllowedInFlight = 64;

  explicit NetworkOptions(network::ConnectionPool::Config const& config);

  enum class CompressionType { kNone, kDeflate, kGzip, kLz4, kAuto };

  std::string protocol;
  uint64_t maxOpenConnections = 0;
  uint64_t idleTtlMilli = 0;
  uint32_t numIOThreads = 0;
  bool verifyHosts = false;
  uint64_t maxInFlight = 0;
  uint64_t compressRequestThreshold = 200;

  // note: we cannot use any compression method by default here for the
  // 3.12 release because that could cause upgrades from 3.11 to 3.12
  // to break. for example, if we enable compression here and during the
  // upgrade the 3.12 servers could pick it up and send compressed
  // requests to 3.11 servers which cannot handle them.
  // we should set the compression type "auto" for future releases though
  // to save some traffic.
  CompressionType compressionType = CompressionType::kNone;
  std::string compressionTypeLabel = "none";
};

}  // namespace arangodb
