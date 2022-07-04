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

#include "DocumentLogEntry.h"
#include "Inspection/VPack.h"

namespace {
auto const String_Insert = std::string_view{"Insert"};
auto const String_Update = std::string_view{"Update"};
auto const String_Replace = std::string_view{"Replace"};
auto const String_Remove = std::string_view{"Remove"};
auto const String_Truncate = std::string_view{"Truncate"};
auto const String_Commit = std::string_view{"Commit"};
auto const String_Abort = std::string_view{"Abort"};
}  // namespace

using namespace arangodb::replication2::replicated_state;
using namespace arangodb::replication2::replicated_state::document;

auto document::fromDocumentOperation(TRI_voc_document_operation_e& op) noexcept
    -> OperationType {
  switch (op) {
    case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      return kInsert;
    case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
      return kUpdate;
    case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
      return kReplace;
    case TRI_VOC_DOCUMENT_OPERATION_REMOVE:
      return kRemove;
    default:
      ADB_PROD_ASSERT(false) << "Unexpected document operation " << op;
  }
  return {};  // should never be reached, but compiler complains about missing
              // return
}

auto document::to_string(OperationType op) noexcept -> std::string_view {
  switch (op) {
    case kInsert:
      return String_Insert;
    case kUpdate:
      return String_Update;
    case kReplace:
      return String_Replace;
    case kRemove:
      return String_Remove;
    case kTruncate:
      return String_Truncate;
    case kCommit:
      return String_Commit;
    case kAbort:
      return String_Abort;
    default:
      ADB_PROD_ASSERT(false) << "Unexpected operation " << op;
  }
  return {};  // should never be reached, but compiler complains about missing
              // return
}

auto OperationStringTransformer::toSerialized(OperationType source,
                                              std::string& target) const
    -> inspection::Status {
  target = to_string(source);
  return {};
}

auto OperationStringTransformer::fromSerialized(std::string const& source,
                                                OperationType& target) const
    -> inspection::Status {
  if (source == String_Insert) {
    target = OperationType::kInsert;
  } else if (source == String_Update) {
    target = OperationType::kUpdate;
  } else if (source == String_Replace) {
    target = OperationType::kReplace;
  } else if (source == String_Remove) {
    target = OperationType::kRemove;
  } else if (source == String_Truncate) {
    target = OperationType::kTruncate;
  } else if (source == String_Commit) {
    target = OperationType::kCommit;
  } else if (source == String_Abort) {
    target = OperationType::kAbort;
  } else {
    return inspection::Status{"Invalid operation " + std::string{source}};
  }
  return {};
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