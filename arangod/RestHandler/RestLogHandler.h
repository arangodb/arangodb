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
  RestStatus execute() final;
  char const* name() const final { return "RestLogHandler"; }
  RequestLane lane() const final { return RequestLane::CLIENT_SLOW; }

 private:
  RestStatus executeByMethod(replication2::ReplicatedLogMethods const& methods);
  RestStatus handleGetRequest(
      replication2::ReplicatedLogMethods const& methods);
  RestStatus handlePostRequest(
      replication2::ReplicatedLogMethods const& methods);
  RestStatus handleDeleteRequest(
      replication2::ReplicatedLogMethods const& methods);

  RestStatus handlePost(replication2::ReplicatedLogMethods const& methods,
                        velocypack::Slice specSlice);
  RestStatus handlePostInsert(replication2::ReplicatedLogMethods const& methods,
                              replication2::LogId logId,
                              velocypack::Slice payload);
  RestStatus handlePostPing(replication2::ReplicatedLogMethods const& methods,
                            replication2::LogId logId,
                            velocypack::Slice payload);
  RestStatus handlePostInsertMulti(
      replication2::ReplicatedLogMethods const& methods,
      replication2::LogId logId, velocypack::Slice payload);
  RestStatus handlePostRelease(
      replication2::ReplicatedLogMethods const& methods,
      replication2::LogId logId);
  RestStatus handlePostCompact(
      replication2::ReplicatedLogMethods const& methods,
      replication2::LogId logId);

  RestStatus handleGet(replication2::ReplicatedLogMethods const& methods);
  RestStatus handleGetPoll(replication2::ReplicatedLogMethods const& methods,
                           replication2::LogId);
  RestStatus handleGetHead(replication2::ReplicatedLogMethods const& methods,
                           replication2::LogId);
  RestStatus handleGetTail(replication2::ReplicatedLogMethods const& methods,
                           replication2::LogId);
  RestStatus handleGetSlice(replication2::ReplicatedLogMethods const& methods,
                            replication2::LogId);
  RestStatus handleGetLocalStatus(
      replication2::ReplicatedLogMethods const& methods, replication2::LogId);
  RestStatus handleGetGlobalStatus(
      replication2::ReplicatedLogMethods const& methods, replication2::LogId);
  RestStatus handleGetCollectionStatus(
      replication2::ReplicatedLogMethods const& methods, CollectionID cid);
  RestStatus handleGetLog(replication2::ReplicatedLogMethods const& methods,
                          replication2::LogId);
  RestStatus handleGetEntry(replication2::ReplicatedLogMethods const& methods,
                            replication2::LogId);
};
}  // namespace arangodb
