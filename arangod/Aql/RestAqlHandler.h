////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "RestServer/VocbaseContext.h"

struct TRI_vocbase_t;

namespace arangodb {

namespace traverser {
class TraverserEngineRegistry;
}

namespace aql {
class Query;
class QueryRegistry;

/// @brief shard control request handler
class RestAqlHandler : public RestVocbaseBaseHandler {
 public:
  RestAqlHandler(GeneralRequest*, GeneralResponse*,
                 std::pair<QueryRegistry*, traverser::TraverserEngineRegistry*>*);

 public:
  char const* name() const override final { return "RestAqlHandler"; }
  RequestLane lane() const override final {
    return RequestLane::CLUSTER_INTERNAL;
  }
  RestStatus execute() override;
  RestStatus continueExecute() override;

 public:
  // POST method for /_api/aql/instantiate
  // The body is a VelocyPack with attributes "plan" for the execution plan and
  // "options" for the options, all exactly as in AQL_EXECUTEJSON.
  void createQueryFromVelocyPack();

  // PUT method for /_api/aql/<operation>/<queryId>, this is using
  // the part of the cursor API with side effects.
  // <operation>: can be "getSome" or "skip".
  // The body must be a Json with the following attributes:
  // For the "getSome" operation one has to give:
  //   "atMost": must be a positiv integers, the cursor returns never
  //             more than "atMost" items.
  //             The result is the JSON representation of an
  //             AqlItemBlock.
  // For the "skip" operation one has to give:
  //   "number": must be a positive integer, the cursor skips as many items,
  //             possibly exhausting the cursor.
  //             The result is a JSON with the attributes "error" (boolean),
  //             "errorMessage" (if applicable) and "exhausted" (boolean) [3.3
  //             and earlier] "done" (boolean) [3.4.0 and later] to indicate
  //             whether or not the cursor is exhausted.
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

  bool registerSnippets(arangodb::velocypack::Slice const snippets,
                        arangodb::velocypack::Slice const collections,
                        arangodb::velocypack::Slice const variables,
                        std::shared_ptr<arangodb::velocypack::Builder> options,
                        std::shared_ptr<transaction::Context> const& ctx, double const ttl,
                        bool& needToLock, arangodb::velocypack::Builder& answer);

  bool registerTraverserEngines(arangodb::velocypack::Slice const traversers,
                                std::shared_ptr<transaction::Context> const& ctx,
                                double const ttl, bool& needToLock,
                                arangodb::velocypack::Builder& answer);

  // Send slice as result with the given response type.
  void sendResponse(rest::ResponseCode, arangodb::velocypack::Slice const,
                    transaction::Context*);
  // Send slice as result with the given response type.
  void sendResponse(rest::ResponseCode, arangodb::velocypack::Slice const);

  // handle for useQuery
  RestStatus handleUseQuery(std::string const&, Query*, arangodb::velocypack::Slice const);

  // parseVelocyPackBody, returns a nullptr and produces an error
  // response if
  // parse was not successful.
  std::shared_ptr<arangodb::velocypack::Builder> parseVelocyPackBody();

 private:
  // dig out vocbase from context and query from ID, handle errors
  bool findQuery(std::string const& idString, Query*& query);

  // generate patched options with TTL extracted from request
  std::pair<double, std::shared_ptr<VPackBuilder>> getPatchedOptionsWithTTL(VPackSlice const& optionsSlice) const;

  // our query registry
  QueryRegistry* _queryRegistry;

  // our traversal engine registry
  traverser::TraverserEngineRegistry* _traverserRegistry;

  // id of current query
  QueryId _qId;
};
}  // namespace aql
}  // namespace arangodb

#endif
