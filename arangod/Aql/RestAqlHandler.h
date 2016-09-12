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

#include "Basics/Common.h"
#include "Aql/QueryRegistry.h"
#include "Aql/types.h"
#include "RestHandler/RestVocbaseBaseHandler.h"
#include "RestServer/VocbaseContext.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {

/// @brief shard control request handler
class RestAqlHandler : public RestVocbaseBaseHandler {
 public:
  RestAqlHandler(GeneralRequest*, GeneralResponse*, QueryRegistry*);

 public:
  bool isDirect() const override;
  size_t queue() const override;
  status execute() override;

 public:
  // POST method for /_api/aql/instantiate
  // The body is a VelocyPack with attributes "plan" for the execution plan and
  // "options" for the options, all exactly as in AQL_EXECUTEJSON.
  void createQueryFromVelocyPack();

  // POST method for /_api/aql/parse
  // The body is a Json with attributes "query" for the query string,
  // "parameters" for the query parameters and "options" for the options.
  // This does the same as AQL_PARSE with exactly these parameters and
  // does not keep the query hanging around.
  void parseQuery();

  // POST method for /_api/aql/explain
  // The body is a Json with attributes "query" for the query string,
  // "parameters" for the query parameters and "options" for the options.
  // This does the same as AQL_EXPLAIN with exactly these parameters and
  // does not keep the query hanging around.
  void explainQuery();

  // POST method for /_api/aql/query
  // The body is a Json with attributes "query" for the query string,
  // "parameters" for the query parameters and "options" for the options.
  // This sets up the query as as AQL_EXECUTE would, but does not use
  // the cursor API yet. Rather, the query is stored in the query registry
  // for later use by PUT requests.
  void createQueryFromString();

  // PUT method for /_api/aql/<operation>/<queryId>, this is using
  // the part of the cursor API with side effects.
  // <operation>: can be "getSome" or "skip".
  // The body must be a Json with the following attributes:
  // For the "getSome" operation one has to give:
  //   "atLeast":
  //   "atMost": both must be positive integers, the cursor returns never
  //             more than "atMost" items and tries to return at least
  //             "atLeast". Note that it is possible to return fewer than
  //             "atLeast", for example if there are only fewer items
  //             left. However, the implementation may return fewer items
  //             than "atLeast" for internal reasons, for example to avoid
  //             excessive copying. The result is the JSON representation of an
  //             AqlItemBlock.
  // For the "skip" operation one has to give:
  //   "number": must be a positive integer, the cursor skips as many items,
  //             possibly exhausting the cursor.
  //             The result is a JSON with the attributes "error" (boolean),
  //             "errorMessage" (if applicable) and "exhausted" (boolean)
  //             to indicate whether or not the cursor is exhausted.
  void useQuery(std::string const& operation, std::string const& idString);

  // GET method for /_api/aql/<queryId>
  void getInfoQuery(std::string const& operation, std::string const& idString);

 private:
  // Send slice as result with the given response type.
  void sendResponse(rest::ResponseCode,
                    arangodb::velocypack::Slice const, TransactionContext*);

  // handle for useQuery
  void handleUseQuery(std::string const&, Query*,
                      arangodb::velocypack::Slice const);

  // parseVelocyPackBody, returns a nullptr and produces an error
  // response if
  // parse was not successful.
  std::shared_ptr<arangodb::velocypack::Builder> parseVelocyPackBody();

 private:
  // dig out vocbase from context and query from ID, handle errors
  bool findQuery(std::string const& idString, Query*& query);

  // name of the queue
  static std::string const QUEUE_NAME;

  // request context
  VocbaseContext* _context;

  // the vocbase
  TRI_vocbase_t* _vocbase;

  // our query registry
  QueryRegistry* _queryRegistry;

  // id of current query
  QueryId _qId;
};
}
}

#endif
