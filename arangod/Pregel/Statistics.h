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

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <cstdint>

#include "Inspection/VPack.h"
#include "Pregel/Utils.h"
#include "Logger/LogMacros.h"

namespace arangodb {
namespace pregel {

struct MessageStats {
  size_t sendCount = 0;
  size_t receivedCount = 0;
  double superstepRuntimeSecs = 0;

  MessageStats() = default;
  MessageStats(size_t s, size_t r) : sendCount(s), receivedCount(r) {}

  void accumulate(MessageStats const& other) {
    sendCount += other.sendCount;
    receivedCount += other.receivedCount;
    superstepRuntimeSecs += other.superstepRuntimeSecs;
  }

  void reset() {
    sendCount = 0;
    receivedCount = 0;
    superstepRuntimeSecs = 0;
  }

  bool allMessagesProcessed() const { return sendCount == receivedCount; }
};
template<typename Inspector>
auto inspect(Inspector& f, MessageStats& x) {
  return f.object(x).fields(
      f.field("sendCount", x.sendCount),
      f.field("receivedCount", x.receivedCount),
      f.field("superstepRuntimeInSeconds", x.superstepRuntimeSecs));
}

struct StatsManager {
  void accumulate(MessageStats const& data) { _stats.accumulate(data); }
  void accumulateActiveCounts(uint64_t counts) { _activeCounts += counts; }

  bool allMessagesProcessed() const { return _stats.allMessagesProcessed(); }

  bool noActiveVertices() const { return _activeCounts == 0; }

  void resetActiveCount() { _activeCounts = 0; }

  void reset() { _stats.reset(); }

  void debugOutput() const {
    VPackBuilder v;
    serialize(v, _stats);
    LOG_TOPIC("26dad", TRACE, Logger::PREGEL) << v.toJson();
  }

  template<typename Inspector>
  friend auto inspect(Inspector& f, StatsManager& x);

 private:
  uint64_t _activeCounts;
  MessageStats _stats;
};

template<typename Inspector>
auto inspect(Inspector& f, StatsManager& x) {
  return f.object(x).fields(f.field("activeCounts", x._activeCounts),
                            f.field("statistics", x._stats));
}
}  // namespace pregel
}  // namespace arangodb
