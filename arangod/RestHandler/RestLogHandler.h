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

class RestLogHandler : public RestVocbaseBaseHandler {
 public:
  RestLogHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);

 public:
  auto executeAsync() -> futures::Future<futures::Unit> override;
  char const* name() const final { return "RestLogHandler"; }
  RequestLane lane() const final { return RequestLane::CLIENT_SLOW; }

 private:
  auto executeByMethod(replication2::ReplicatedLogMethods const& methods)
      -> async<void>;
  auto handleGetRequest(replication2::ReplicatedLogMethods const& methods)
      -> async<void>;
  auto handlePostRequest(replication2::ReplicatedLogMethods const& methods)
      -> async<void>;
  auto handleDeleteRequest(replication2::ReplicatedLogMethods const& methods)
      -> async<void>;

  auto handlePost(replication2::ReplicatedLogMethods const& methods,
                  velocypack::Slice specSlice) -> async<void>;
  auto handlePostInsert(replication2::ReplicatedLogMethods const& methods,
                        replication2::LogId logId, velocypack::Slice payload)
      -> async<void>;
  auto handlePostPing(replication2::ReplicatedLogMethods const& methods,
                      replication2::LogId logId, velocypack::Slice payload)
      -> async<void>;
  auto handlePostInsertMulti(replication2::ReplicatedLogMethods const& methods,
                             replication2::LogId logId,
                             velocypack::Slice payload) -> async<void>;
  auto handlePostRelease(replication2::ReplicatedLogMethods const& methods,
                         replication2::LogId logId) -> async<void>;
  auto handlePostCompact(replication2::ReplicatedLogMethods const& methods,
                         replication2::LogId logId) -> async<void>;

  auto handleGet(replication2::ReplicatedLogMethods const& methods)
      -> async<void>;
  auto handleGetPoll(replication2::ReplicatedLogMethods const& methods,
                     replication2::LogId) -> async<void>;
  auto handleGetHead(replication2::ReplicatedLogMethods const& methods,
                     replication2::LogId) -> async<void>;
  auto handleGetTail(replication2::ReplicatedLogMethods const& methods,
                     replication2::LogId) -> async<void>;
  auto handleGetSlice(replication2::ReplicatedLogMethods const& methods,
                      replication2::LogId) -> async<void>;
  auto handleGetLocalStatus(replication2::ReplicatedLogMethods const& methods,
                            replication2::LogId) -> async<void>;
  auto handleGetGlobalStatus(replication2::ReplicatedLogMethods const& methods,
                             replication2::LogId) -> async<void>;
  auto handleGetCollectionStatus(
      replication2::ReplicatedLogMethods const& methods, CollectionID cid)
      -> async<void>;
  auto handleGetLog(replication2::ReplicatedLogMethods const& methods,
                    replication2::LogId) -> async<void>;
  auto handleGetEntry(replication2::ReplicatedLogMethods const& methods,
                      replication2::LogId) -> async<void>;
};
}  // namespace arangodb
