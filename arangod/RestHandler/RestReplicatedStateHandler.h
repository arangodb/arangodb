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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {

namespace replication2 {
struct ReplicatedStateMethods;
}

class RestReplicatedStateHandler : public RestVocbaseBaseHandler {
 public:
  RestReplicatedStateHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);

 public:
  [[nodiscard]] auto execute() -> RestStatus final;
  [[nodiscard]] auto name() const -> char const* final {
    return "RestReplicatedStateHandler";
  }
  [[nodiscard]] auto lane() const -> RequestLane final {
    return RequestLane::CLIENT_SLOW;
  }

 private:
  [[nodiscard]] auto executeByMethod(
      replication2::ReplicatedStateMethods const& methods) -> RestStatus;
  [[nodiscard]] auto handleGetRequest(
      replication2::ReplicatedStateMethods const& methods) -> RestStatus;
  [[nodiscard]] auto handlePostRequest(
      replication2::ReplicatedStateMethods const& methods) -> RestStatus;
  [[nodiscard]] auto handleDeleteRequest(
      replication2::ReplicatedStateMethods const& methods) -> RestStatus;
};
}  // namespace arangodb
