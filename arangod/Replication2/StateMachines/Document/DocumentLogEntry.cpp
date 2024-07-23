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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentLogEntry.h"

#include "Inspection/VPack.h"

using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;

auto document::operator<<(std::ostream& ostream, DocumentLogEntry const& entry)
    -> std::ostream& {
  return ostream << velocypack::serialize(entry).toJson();
}

auto EntryDeserializer<DocumentLogEntry>::operator()(
    streams::serializer_tag_t<DocumentLogEntry>, velocypack::Slice s) const
    -> DocumentLogEntry {
  return arangodb::velocypack::deserialize<DocumentLogEntry>(s);
}

void EntrySerializer<DocumentLogEntry>::operator()(
    streams::serializer_tag_t<DocumentLogEntry>, DocumentLogEntry const& e,
    velocypack::Builder& b) const {
  serialize(b, e);
}
