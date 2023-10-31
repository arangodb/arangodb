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

#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {

namespace replication2 {
struct DocumentStateMethods;
namespace replicated_state::document {
struct SnapshotParams;
}
}  // namespace replication2

class RestDocumentStateHandler : public RestVocbaseBaseHandler {
 public:
  RestDocumentStateHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);
  RestStatus execute() final;
  char const* name() const final { return "RestDocumentStateHandler"; }
  RequestLane lane() const final { return RequestLane::CLIENT_SLOW; }

 private:
  RestStatus executeByMethod(replication2::DocumentStateMethods const& methods);

  RestStatus handleGetRequest(
      replication2::DocumentStateMethods const& methods);
  auto parseGetSnapshotParams()
      -> ResultT<replication2::replicated_state::document::SnapshotParams>;

  RestStatus handlePostRequest(
      replication2::DocumentStateMethods const& methods);
  auto parsePostSnapshotParams()
      -> ResultT<replication2::replicated_state::document::SnapshotParams>;

  RestStatus handleDeleteRequest(
      replication2::DocumentStateMethods const& methods);
  auto parseDeleteSnapshotParams()
      -> ResultT<replication2::replicated_state::document::SnapshotParams>;

  RestStatus processSnapshotRequest(
      replication2::DocumentStateMethods const& methods,
      replication2::LogId const& logId,
      ResultT<replication2::replicated_state::document::SnapshotParams>&&
          params);

 private:
  std::unique_ptr<VPackCustomTypeHandler> _customTypeHandler;
  VPackOptions _options;
};
}  // namespace arangodb
