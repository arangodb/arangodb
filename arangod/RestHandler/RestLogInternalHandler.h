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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {

namespace replication2 {
struct ReplicatedLogMethods;
}

class RestLogInternalHandler : public RestVocbaseBaseHandler {
 public:
  RestLogInternalHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);
  ~RestLogInternalHandler() override;

 public:
  RestStatus execute() final;
  char const* name() const final { return "RestLogInternalHandler"; }
  RequestLane lane() const final { return RequestLane::CLIENT_SLOW; }

 private:
  RestStatus handleAppendEntries();
  RestStatus handleUpdateSnapshotStatus();
};
}  // namespace arangodb
