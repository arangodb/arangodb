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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <atomic>

#include <Inspection/VPack.h>
#include <Inspection/Transformers.h>

#include <Pregel/Common.h>

#include "StaticStrings.h"

namespace arangodb::pregel {

struct GraphStoreStats {
  std::size_t numberVerticesLoaded{0};
  std::size_t vertexStorageBytes{0};
  std::size_t vertexStorageBytesUsed{0};
  std::size_t vertexKeyStorageBytes{0};
  std::size_t vertexKeyStorageBytesUsed{0};

  std::size_t numberEdgesLoaded{0};
  std::size_t edgeStorageBytes{0};
  std::size_t edgeStorageBytesUsed{0};
  std::size_t edgeKeyStorageBytes{0};
  std::size_t edgeKeyStorageBytesUsed{0};
};

struct AtomicGraphStoreStats {
  std::atomic<std::size_t> numberVerticesLoaded{0};
  std::atomic<std::size_t> vertexStorageBytes{0};
  std::atomic<std::size_t> vertexStorageBytesUsed{0};
  std::atomic<std::size_t> vertexKeyStorageBytes{0};
  std::atomic<std::size_t> vertexKeyStorageBytesUsed{0};

  std::atomic<std::size_t> numberEdgesLoaded{0};
  std::atomic<std::size_t> edgeStorageBytes{0};
  std::atomic<std::size_t> edgeStorageBytesUsed{0};
  std::atomic<std::size_t> edgeKeyStorageBytes{0};
  std::atomic<std::size_t> edgeKeyStorageBytesUsed{0};

  auto snapshot() -> GraphStoreStats {
    return GraphStoreStats{
        .numberVerticesLoaded = numberVerticesLoaded.load(),
        .vertexStorageBytes = vertexStorageBytes.load(),
        .vertexStorageBytesUsed = vertexStorageBytesUsed.load(),
        .vertexKeyStorageBytes = vertexKeyStorageBytes.load(),
        .vertexKeyStorageBytesUsed = vertexKeyStorageBytesUsed.load(),
        .numberEdgesLoaded = numberEdgesLoaded.load(),
        .edgeStorageBytes = edgeStorageBytes.load(),
        .edgeStorageBytesUsed = edgeStorageBytesUsed.load(),
        .edgeKeyStorageBytes = edgeKeyStorageBytes.load(),
        .edgeKeyStorageBytesUsed = edgeKeyStorageBytesUsed.load()};
  }
};

template<typename Inspector>
auto inspect(Inspector& f, GraphStoreStats& x) {
  return f.object(x).fields(
      f.field(static_strings::numberVerticesLoaded, x.numberVerticesLoaded),
      f.field(static_strings::vertexStorageBytes, x.vertexStorageBytes),
      f.field(static_strings::vertexStorageBytesUsed, x.vertexStorageBytesUsed),
      f.field(static_strings::vertexKeyStorageBytes, x.vertexKeyStorageBytes),
      f.field(static_strings::vertexKeyStorageBytesUsed,
              x.vertexKeyStorageBytesUsed),
      f.field(static_strings::numberEdgesLoaded, x.numberEdgesLoaded),
      f.field(static_strings::edgeStorageBytes, x.edgeStorageBytes),
      f.field(static_strings::edgeStorageBytesUsed, x.edgeStorageBytesUsed),
      f.field(static_strings::edgeKeyStorageBytes, x.edgeKeyStorageBytes),
      f.field(static_strings::edgeKeyStorageBytesUsed,
              x.edgeKeyStorageBytesUsed));
}

}  // namespace arangodb::pregel
