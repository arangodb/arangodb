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

#pragma once

#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {

namespace replication2 {
struct PrototypeStateMethods;
}

class RestPrototypeStateHandler : public RestVocbaseBaseHandler {
 public:
  RestPrototypeStateHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);

 public:
  RestStatus execute() final;
  char const* name() const final { return "RestPrototypeStateHandler"; }
  RequestLane lane() const final { return RequestLane::CLIENT_SLOW; }

 private:
  RestStatus executeByMethod(
      replication2::PrototypeStateMethods const& methods);
  RestStatus handleGetRequest(
      replication2::PrototypeStateMethods const& methods);
  RestStatus handlePostRequest(
      replication2::PrototypeStateMethods const& methods);
  RestStatus handleDeleteRequest(
      replication2::PrototypeStateMethods const& methods);

  RestStatus handlePostInsert(
      replication2::PrototypeStateMethods const& methods,
      replication2::LogId logId, velocypack::Slice payload);
  RestStatus handlePostRetrieveMulti(
      replication2::PrototypeStateMethods const& methods, replication2::LogId,
      velocypack::Slice payload);
  RestStatus handleCreateState(
      replication2::PrototypeStateMethods const& methods,
      velocypack::Slice payload);

  RestStatus handleDeleteRemove(
      replication2::PrototypeStateMethods const& methods,
      replication2::LogId logId);
  RestStatus handleDeleteRemoveMulti(
      replication2::PrototypeStateMethods const& methods,
      replication2::LogId logId, velocypack::Slice payload);

  RestStatus handleGetEntry(replication2::PrototypeStateMethods const& methods,
                            replication2::LogId);
  RestStatus handleGetSnapshot(
      replication2::PrototypeStateMethods const& methods, replication2::LogId);
};
}  // namespace arangodb
