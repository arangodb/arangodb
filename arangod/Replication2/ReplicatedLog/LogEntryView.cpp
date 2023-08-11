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

#include "LogEntryView.h"

#include <Basics/StaticStrings.h>

#include <velocypack/Builder.h>

namespace arangodb::replication2 {

LogEntryView::LogEntryView(LogIndex index, LogPayload const& payload) noexcept
    : _index(index), _payload(payload.slice()) {}

LogEntryView::LogEntryView(LogIndex index, velocypack::Slice payload) noexcept
    : _index(index), _payload(payload) {}

auto LogEntryView::logIndex() const noexcept -> LogIndex { return _index; }

auto LogEntryView::logPayload() const noexcept -> velocypack::Slice {
  return _payload;
}

void LogEntryView::toVelocyPack(velocypack::Builder& builder) const {
  auto og = velocypack::ObjectBuilder(&builder);
  builder.add(StaticStrings::LogIndex, velocypack::Value(_index));
  builder.add(StaticStrings::Payload, _payload);
}

auto LogEntryView::fromVelocyPack(velocypack::Slice slice) -> LogEntryView {
  return {slice.get(StaticStrings::LogIndex).extract<LogIndex>(),
          slice.get(StaticStrings::Payload)};
}

auto LogEntryView::clonePayload() const -> LogPayload {
  return LogPayload::createFromSlice(_payload);
}

}  // namespace arangodb::replication2
