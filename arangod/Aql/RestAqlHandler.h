////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_REST_AQL_HANDLER_H
#define ARANGOD_AQL_REST_AQL_HANDLER_H 1

#include "Aql/types.h"
#include "Basics/Common.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {
class Query;
class QueryRegistry;
enum class SerializationFormat;

/// @brief shard control request handler
class RestAqlHandler : public RestVocbaseBaseHandler {
 public:
  RestAqlHandler(application_features::ApplicationServer&, GeneralRequest*,
                 GeneralResponse*, QueryRegistry*);

 public:
  char const* name() const override final { return "RestAqlHandler"; }
  RequestLane lane() const override final { return RequestLane::CLUSTER_AQL; }
  RestStatus execute() override;
  RestStatus continueExecute() override;
  void shutdownExecute(bool isFinalized) noexcept override;

  class Route {
   public:
    static auto execute() -> const char* { return "/_api/aql/execute"; }
  };
  
  // PUT method for /_api/aql/<operation>/<queryId>, this is using
  // the part of the cursor API with side effects.
  // <operation>: can be "execute", "getSome", "skipSome" "initializeCursor" or
  //              "shutdown".
  //              "getSome" and "skipSome" are only used pre-3.7 and can be
  //              removed in 3.8.
  // The body must be a Json with the following attributes:
  // For the "execute" operation one has to give:
  //   "callStack": an array of objects, each with the following attributes:
  //     "offset": a non-negative integer
  //     "limit": either a non-negative integer, or the string "infinity"
  //     "limitType: string or null, either "soft" or "hard"; set iff limit is
  //     not infinity "fullCount": a boolean
  //   The result is an object with the attributes
  //     "code": integer, error code.
  //        If there was no error:
  //     "result": an object with the following attributes:
  //       "state": string, either "hasMore" or "done"
  //       "skipped": non-negative integer
  //       "block": serialized AqlItemBlock, or null when no rows are returned.
  // For the "getSome" operation one has to give:
  //   "atMost": must be a positive integer, the cursor returns never
  //             more than "atMost" items. Defaults to
  //             ExecutionBlock::DefaultBatchSize.
  //   The result is the JSON representation of an AqlItemBlock.
  // For the "skipSome" operation one has to give:
  //   "atMost": must be a positive integer, the cursor skips never
  //             more than "atMost" items. The result is a JSON object with a
  //             single attribute "skipped" containing the number of
  //             skipped items.
  //             If "atMost" is not given it defaults to
  //             ExecutionBlock::DefaultBatchSize.
  // For the "initializeCursor" operation, one has to bind the following
  // attributes:
  //   "items": This is a serialized AqlItemBlock with usually only one row
  //            and the correct number of columns.
  //   "pos":   The number of the row in "items" to take, usually 0.
  // For the "shutdown" operation no additional arguments are
  // required and an empty JSON object in the body is OK.
  // All operations allow to set the HTTP header "x-shard-id:". If this is
  // set, then the root block of the stored query must be a ScatterBlock
  // and the shard ID is given as an additional argument to the ScatterBlock's
  // special API.
  RestStatus useQuery(std::string const& operation, std::string const& idString);

 private:
  // POST method for /_api/aql/setup (internal)
  // Only available on DBServers in the Cluster.
  // This route sets-up all the query engines required
  // for a complete query on this server.
  // Furthermore it directly locks all shards for this query.
  // So after this route the query is ready to go.
  // NOTE: As this Route LOCKS the collections, the caller
  // is responsible to destroy those engines in a timely
  // manner, if the engines are not called for a period
  // of time, they will be garbage-collected and unlocked.
  // The body is a VelocyPack with the following layout:
  //  {
  //    lockInfo: {
  //      READ: [<collections to read-lock],
  //      WRITE: [<collections to write-lock]
  //    },
  //    options: { < query options > },
  //    snippets: {
  //      <queryId: {nodes: [ <nodes>]}>
  //    },
  //    variables: [ <variables> ]
  //  }

  void setupClusterQuery();

  // handle for useQuery
  RestStatus handleUseQuery(std::string const&, arangodb::velocypack::Slice const);
  
  // handle query finalization for all engines
  RestStatus handleFinishQuery(std::string const& idString);

 private:
  // dig out vocbase from context and query from ID, handle errors
  ExecutionEngine* findEngine(std::string const& idString);

  // our query registry
  QueryRegistry* _queryRegistry;

  aql::ExecutionEngine* _engine;

  // id of current query
  QueryId _qId;
};
}  // namespace aql
}  // namespace arangodb

#endif
