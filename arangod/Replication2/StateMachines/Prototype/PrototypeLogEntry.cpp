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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "PrototypeLogEntry.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::prototype;

auto PrototypeLogEntry::getType() noexcept -> std::string_view {
  return std::visit(
      overload{
          [](PrototypeLogEntry::DeleteOperation const& o) { return kDelete; },
          [](PrototypeLogEntry::InsertOperation const& o) { return kInsert; },
      },
      op);
}

auto replicated_state::EntryDeserializer<
    replicated_state::prototype::PrototypeLogEntry>::
operator()(
    streams::serializer_tag_t<replicated_state::prototype::PrototypeLogEntry>,
    velocypack::Slice s) const
    -> replicated_state::prototype::PrototypeLogEntry {
  return deserialize<prototype::PrototypeLogEntry>(s);
}

void replicated_state::EntrySerializer<
    replicated_state::prototype::PrototypeLogEntry>::
operator()(
    streams::serializer_tag_t<replicated_state::prototype::PrototypeLogEntry>,
    prototype::PrototypeLogEntry const& e, velocypack::Builder& b) const {
  serialize(b, e);
}