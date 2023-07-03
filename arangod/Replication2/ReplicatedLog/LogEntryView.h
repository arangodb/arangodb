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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <velocypack/Slice.h>

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/LogPayload.h"
#include "Replication2/ReplicatedLog/TypedLogIterator.h"

namespace arangodb::replication2 {

// A log entry as visible to the user of a replicated log.
// Does thus always contain a payload: only internal log entries are without
// payload, which aren't visible to the user. User-defined log entries always
// contain a payload.
// The term is not of interest, and therefore not part of this struct.
// Note that when these entries are visible, they are already committed.
// It does not own the payload, so make sure it is still valid when using it.
class LogEntryView {
 public:
  LogEntryView() = delete;
  LogEntryView(LogIndex index, LogPayload const& payload) noexcept;
  LogEntryView(LogIndex index, velocypack::Slice payload) noexcept;

  [[nodiscard]] auto logIndex() const noexcept -> LogIndex;
  [[nodiscard]] auto logPayload() const noexcept -> velocypack::Slice;
  [[nodiscard]] auto clonePayload() const -> LogPayload;

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> LogEntryView;

 private:
  LogIndex _index;
  velocypack::Slice _payload;
};

using LogIterator = TypedLogIterator<LogEntryView>;
using LogRangeIterator = TypedLogRangeIterator<LogEntryView>;

}  // namespace arangodb::replication2
