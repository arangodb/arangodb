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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"
#include "Replication2/StateMachines/Document/ReplicatedOperation.h"
#include "Replication2/StateMachines/Document/ReplicatedOperationInspectors.h"
#include "Replication2/Streams/StreamSpecification.h"
#include "Inspection/Status.h"

namespace arangodb::replication2::replicated_state {
namespace document {

/*
 * Used for transporting operations to the state machine. Does not contain any
 * logic.
 */
struct DocumentLogEntry {
  ReplicatedOperation operation;

  auto& getInnerOperation() noexcept { return operation.operation; }
  [[nodiscard]] auto const& getInnerOperation() const noexcept {
    return operation.operation;
  }

  template<typename Inspector>
  friend auto inspect(Inspector& f, DocumentLogEntry& x) {
    return f.object(x).fields(f.field("operation", x.operation));
  }
};

auto operator<<(std::ostream&, DocumentLogEntry const&) -> std::ostream&;
}  // namespace document

template<>
struct EntryDeserializer<document::DocumentLogEntry> {
  auto operator()(
      streams::serializer_tag_t<replicated_state::document::DocumentLogEntry>,
      velocypack::Slice s) const
      -> replicated_state::document::DocumentLogEntry;
};

template<>
struct EntrySerializer<document::DocumentLogEntry> {
  void operator()(
      streams::serializer_tag_t<replicated_state::document::DocumentLogEntry>,
      replicated_state::document::DocumentLogEntry const& e,
      velocypack::Builder& b) const;
};
}  // namespace arangodb::replication2::replicated_state
