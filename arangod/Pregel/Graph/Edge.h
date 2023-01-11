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
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <string>

#include "VertexID.h"

namespace arangodb::pregel {

template<typename V, typename E>
class GraphStore;

// header entry for the edge file
template<typename E>
// cppcheck-suppress noConstructor
class Edge {
  template<typename V, typename E2>
  friend class GraphStore;

  // these members are initialized by the GraphStore
  char* _toKey;              // uint64_t
  uint16_t _toKeyLength;     // uint16_t
  PregelShard _targetShard;  // uint16_t

  E _data;

 public:
  [[nodiscard]] std::string_view toKey() const {
    return {_toKey, _toKeyLength};
  }
  E& data() noexcept { return _data; }
  [[nodiscard]] PregelShard targetShard() const noexcept {
    return _targetShard;
  }
};

}  // namespace arangodb::pregel
