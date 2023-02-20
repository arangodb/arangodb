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

#include "Replication2/StateMachines/Document/DocumentLogEntry.h"

#include "Inspection/VPack.h"

using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;

auto document::fromDocumentOperation(TRI_voc_document_operation_e& op) noexcept
    -> OperationType {
  switch (op) {
    case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      return OperationType::kInsert;
    case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
      return OperationType::kUpdate;
    case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
      return OperationType::kReplace;
    case TRI_VOC_DOCUMENT_OPERATION_REMOVE:
      return OperationType::kRemove;
    default:
      ADB_PROD_ASSERT(false) << "Unexpected document operation " << op;
  }
  return {};  // should never be reached, but compiler complains about missing
              // return
}

auto document::operator<<(std::ostream& os, DocumentLogEntry entry)
    -> std::ostream& {
  VPackBuilder b;
  velocypack::serialize(b, entry);
  return os << b.toJson();
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
