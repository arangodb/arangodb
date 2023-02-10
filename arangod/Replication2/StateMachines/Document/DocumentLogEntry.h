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
#include "Replication2/Streams/StreamSpecification.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "Inspection/Status.h"

#include <velocypack/SharedSlice.h>

#include <string>
#include <string_view>

namespace arangodb::replication2::replicated_state {
namespace document {

enum class OperationType {
  kInsert,
  kUpdate,
  kReplace,
  kRemove,
  kTruncate,
  kCommit,
  kAbort,
  kAbortAllOngoingTrx,
  kIntermediateCommit,
};

template<class Inspector>
auto inspect(Inspector& f, OperationType& p) {
  return f.enumeration(p).values(
      OperationType::kInsert, "Insert",                          //
      OperationType::kUpdate, "Update",                          //
      OperationType::kReplace, "Replace",                        //
      OperationType::kRemove, "Remove",                          //
      OperationType::kTruncate, "Truncate",                      //
      OperationType::kCommit, "Commit",                          //
      OperationType::kAbort, "Abort",                            //
      OperationType::kAbortAllOngoingTrx, "AbortAllOngoingTrx",  //
      OperationType::kIntermediateCommit, "IntermediateCommit"   //
  );
}

auto fromDocumentOperation(TRI_voc_document_operation_e& op) noexcept
    -> OperationType;

struct DocumentLogEntry {
  std::string shardId;
  OperationType operation;
  velocypack::SharedSlice data;
  TransactionId tid;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, DocumentLogEntry& p) {
    return f.object(p).fields(
        f.field("shardId", p.shardId), f.field("operation", p.operation),
        f.field("data", p.data).fallback(velocypack::SharedSlice{}),
        f.field("tid", p.tid));
  }
};

auto operator<<(std::ostream& os, DocumentLogEntry entry) -> std::ostream&;
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
