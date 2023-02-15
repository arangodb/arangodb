////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Pregel/GraphStore/VertexID.h"

namespace arangodb::pregel {
/// A counter for counting unique vertex IDs using a HyperLogLog sketch.
/// @author Aljoscha Krettek, Robert Metzger, Robert Waury
/// https://github.com/hideo55/cpp-HyperLogLog/blob/master/include/hyperloglog.hpp
/// https://github.com/rmetzger/spargel-closeness/blob/master/src/main/java/de/robertmetzger/HLLCounterWritable.java
struct HLLCounter {
  friend struct HLLCounterFormat;
  constexpr static int32_t NUM_BUCKETS = 64;
  constexpr static double ALPHA = 0.709;

  uint32_t getCount();
  void addNode(VertexID const& pregelId);
  void merge(HLLCounter const& counter);

 private:
  uint8_t _buckets[NUM_BUCKETS] = {0};
};

template<typename Inspector>
auto inspect(Inspector& f, HLLCounter& v) {
  // TODO: friend and implement
  return f.object(v).fields();
}

}
