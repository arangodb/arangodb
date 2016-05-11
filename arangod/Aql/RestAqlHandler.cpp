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

#include "RestAqlHandler.h"
#include "Aql/ClusterBlocks.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionBlock.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/DispatcherThread.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "VocBase/server.h"

#include <velocypack/Dumper.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;
using namespace arangodb::aql;

using Json = arangodb::basics::Json;
using VelocyPackHelper = arangodb::basics::VelocyPackHelper;
using JsonHelper = arangodb::basics::JsonHelper;

RestAqlHandler::RestAqlHandler(arangodb::HttpRequest* request,
                               QueryRegistry* queryRegistry)
    : RestVocbaseBaseHandler(request),
      _context(static_cast<VocbaseContext*>(request->requestContext())),
      _vocbase(_context->getVocbase()),
      _queryRegistry(queryRegistry),
      _qId(0) {
  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(_queryRegistry != nullptr);
}

/// @brief returns the queue name
size_t RestAqlHandler::queue() const { return Dispatcher::AQL_QUEUE; }

bool RestAqlHandler::isDirect() const { return false; }

/// @brief POST method for /_api/aql/instantiate (internal)
/// The body is a VelocyPack with attributes "plan" for the execution plan and
/// "options" for the options, all exactly as in AQL_EXECUTEJSON.
void RestAqlHandler::createQueryFromVelocyPack() {
  std::shared_ptr<VPackBuilder> queryBuilder = parseVelocyPackBody();
  if (queryBuilder == nullptr) {
    LOG(ERR) << "invalid VelocyPack plan in query";
    return;
  }
  VPackSlice querySlice = queryBuilder->slice();

  TRI_ASSERT(querySlice.isObject());

  VPackSlice plan = querySlice.get("plan");
  if (plan.isNone()) {
    LOG(ERR) << "Invalid VelocyPack: \"plan\" attribute missing.";
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"plan\"");
    return;
  }

  auto options = std::make_shared<VPackBuilder>(
      VPackBuilder::clone(querySlice.get("options")));

  std::string const part =
      VelocyPackHelper::getStringValue(querySlice, "part", "");

  auto planBuilder = std::make_shared<VPackBuilder>(VPackBuilder::clone(plan));
  auto query = new Query(false, _vocbase, planBuilder, options,
                         (part == "main" ? PART_MAIN : PART_DEPENDENT));
  QueryResult res = query->prepare(_queryRegistry);
  if (res.code != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "failed to instantiate the query: " << res.details;

    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_QUERY_BAD_JSON_PLAN, res.details);
    delete query;
    return;
  }

  // Now the query is ready to go, store it in the registry and return:
  double ttl = 3600.0;
  bool found;
  std::string const& ttlstring = _request->header("ttl", found);

  if (found) {
    ttl = arangodb::basics::StringUtils::doubleDecimal(ttlstring);
  }

  // query id not set, now generate a new one
  _qId = TRI_NewTickServer();

  try {
    _queryRegistry->insert(_qId, query, ttl);
  } catch (...) {
    LOG(ERR) << "could not keep query in registry";

    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "could not keep query in registry");
    delete query;
    return;
  }

  VPackBuilder answerBody;
  try {
    VPackObjectBuilder guard(&answerBody);
    answerBody.add("queryId",
                   VPackValue(arangodb::basics::StringUtils::itoa(_qId)));
    answerBody.add("ttl", VPackValue(ttl));
  } catch (...) {
    LOG(ERR) << "could not keep query in registry";
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  sendResponse(GeneralResponse::ResponseCode::ACCEPTED, answerBody.slice(),
               query->trx()->transactionContext().get());
}

/// @brief POST method for /_api/aql/parse (internal)
/// The body is a Json with attributes "query" for the query string,
/// "parameters" for the query parameters and "options" for the options.
/// This does the same as AQL_PARSE with exactly these parameters and
/// does not keep the query hanging around.
void RestAqlHandler::parseQuery() {
  std::shared_ptr<VPackBuilder> bodyBuilder = parseVelocyPackBody();
  if (bodyBuilder == nullptr) {
    LOG(ERR) << "invalid VelocyPack plan in query";
    return;
  }
  VPackSlice querySlice = bodyBuilder->slice();

  std::string const queryString =
      VelocyPackHelper::getStringValue(querySlice, "query", "");
  if (queryString.empty()) {
    LOG(ERR) << "body must be an object with attribute \"query\"";
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"query\"");
    return;
  }

  auto query = new Query(false, _vocbase, queryString.c_str(),
                         queryString.size(), std::shared_ptr<VPackBuilder>(),
                         nullptr, PART_MAIN);
  QueryResult res = query->parse();
  if (res.code != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "failed to instantiate the Query: " << res.details;
    generateError(GeneralResponse::ResponseCode::BAD, res.code, res.details);
    delete query;
    return;
  }

  // Now prepare the answer:
  VPackBuilder answerBuilder;
  auto transactionContext = query->trx()->transactionContext();
  try {
    {
      VPackObjectBuilder guard(&answerBuilder);
      answerBuilder.add("parsed", VPackValue(true));
      answerBuilder.add(VPackValue("collections"));
      {
        VPackArrayBuilder arrGuard(&answerBuilder);
        for (auto const& c : res.collectionNames) {
          answerBuilder.add(VPackValue(c));
        }
      }

      answerBuilder.add(VPackValue("parameters"));
      {
        VPackArrayBuilder arrGuard(&answerBuilder);
        for (auto const& p : res.bindParameters) {
          answerBuilder.add(VPackValue(p));
        }
      }
      answerBuilder.add(VPackValue("ast"));
      answerBuilder.add(res.result->slice());
      res.result = nullptr;
    }
    sendResponse(GeneralResponse::ResponseCode::OK, answerBuilder.slice(),
                 transactionContext.get());
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_OUT_OF_MEMORY,
                  "out of memory");
  }
}

/// @brief POST method for /_api/aql/explain (internal)
/// The body is a Json with attributes "query" for the query string,
/// "parameters" for the query parameters and "options" for the options.
/// This does the same as AQL_EXPLAIN with exactly these parameters and
/// does not keep the query hanging around.
void RestAqlHandler::explainQuery() {
  std::shared_ptr<VPackBuilder> bodyBuilder = parseVelocyPackBody();
  if (bodyBuilder == nullptr) {
    return;
  }
  VPackSlice querySlice = bodyBuilder->slice();

  std::string queryString =
      VelocyPackHelper::getStringValue(querySlice, "query", "");
  if (queryString.empty()) {
    LOG(ERR) << "body must be an object with attribute \"query\"";
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"query\"");
    return;
  }

  auto bindVars = std::make_shared<VPackBuilder>(
      VPackBuilder::clone(querySlice.get("parameters")));

  auto options = std::make_shared<VPackBuilder>(
      VPackBuilder::clone(querySlice.get("options")));

  auto query = std::make_unique<Query>(false, _vocbase,
                                       queryString.c_str(), queryString.size(),
                                       bindVars, options, PART_MAIN);
  QueryResult res = query->explain();
  if (res.code != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "failed to instantiate the Query: " << res.details;
    generateError(GeneralResponse::ResponseCode::BAD, res.code, res.details);
    return;
  }

  // Now prepare the answer:
  VPackBuilder answerBuilder;
  try {
    {
      VPackObjectBuilder guard(&answerBuilder);
      if (res.result != nullptr) {
        if (query->allPlans()) {
          answerBuilder.add(VPackValue("plans"));
        } else {
          answerBuilder.add(VPackValue("plan"));
        }
        answerBuilder.add(res.result->slice());
        res.result = nullptr;
      }
    }
    sendResponse(GeneralResponse::ResponseCode::OK, answerBuilder.slice(),
                 query->trx()->transactionContext().get());
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_OUT_OF_MEMORY,
                  "out of memory");
  }
}

/// @brief POST method for /_api/aql/query (internal)
/// The body is a Json with attributes "query" for the query string,
/// "parameters" for the query parameters and "options" for the options.
/// This sets up the query as as AQL_EXECUTE would, but does not use
/// the cursor API yet. Rather, the query is stored in the query registry
/// for later use by PUT requests.
void RestAqlHandler::createQueryFromString() {
  std::shared_ptr<VPackBuilder> queryBuilder = parseVelocyPackBody();
  if (queryBuilder == nullptr) {
    return;
  }
  VPackSlice querySlice = queryBuilder->slice();

  std::string const queryString =
      VelocyPackHelper::getStringValue(querySlice, "query", "");
  if (queryString.empty()) {
    LOG(ERR) << "body must be an object with attribute \"query\"";
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"query\"");
    return;
  }

  std::string const part =
      VelocyPackHelper::getStringValue(querySlice, "part", "");
  if (part.empty()) {
    LOG(ERR) << "body must be an object with attribute \"part\"";
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"part\"");
    return;
  }

  auto bindVars = std::make_shared<VPackBuilder>(
      VPackBuilder::clone(querySlice.get("parameters")));
  auto options = std::make_shared<VPackBuilder>(
      VPackBuilder::clone(querySlice.get("options")));

  auto query = new Query(false, _vocbase, queryString.c_str(),
                         queryString.size(), bindVars, options,
                         (part == "main" ? PART_MAIN : PART_DEPENDENT));
  QueryResult res = query->prepare(_queryRegistry);
  if (res.code != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "failed to instantiate the Query: " << res.details;
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_QUERY_BAD_JSON_PLAN, res.details);
    delete query;
    return;
  }

  // Now the query is ready to go, store it in the registry and return:
  double ttl = 3600.0;
  bool found;
  std::string const& ttlstring = _request->header("ttl", found);

  if (found) {
    ttl = arangodb::basics::StringUtils::doubleDecimal(ttlstring);
  }

  _qId = TRI_NewTickServer();
  try {
    _queryRegistry->insert(_qId, query, ttl);
  } catch (...) {
    LOG(ERR) << "could not keep query in registry";
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "could not keep query in registry");
    delete query;
    return;
  }

  createResponse(GeneralResponse::ResponseCode::ACCEPTED);
  _response->setContentType(HttpResponse::CONTENT_TYPE_JSON);
  arangodb::basics::Json answerBody(arangodb::basics::Json::Object, 2);
  answerBody("queryId",
             arangodb::basics::Json(arangodb::basics::StringUtils::itoa(_qId)))(
      "ttl", arangodb::basics::Json(ttl));
  _response->body().appendText(answerBody.toString());
}

/// @brief PUT method for /_api/aql/<operation>/<queryId>, (internal)
/// this is using the part of the cursor API with side effects.
/// <operation>: can be "lock" or "getSome" or "skip" or "initializeCursor" or
/// "shutdown".
/// The body must be a Json with the following attributes:
/// For the "getSome" operation one has to give:
///   "atLeast":
///   "atMost": both must be positive integers, the cursor returns never
///             more than "atMost" items and tries to return at least
///             "atLeast". Note that it is possible to return fewer than
///             "atLeast", for example if there are only fewer items
///             left. However, the implementation may return fewer items
///             than "atLeast" for internal reasons, for example to avoid
///             excessive copying. The result is the JSON representation of an
///             AqlItemBlock.
///             If "atLeast" is not given it defaults to 1, if "atMost" is not
///             given it defaults to ExecutionBlock::DefaultBatchSize.
/// For the "skipSome" operation one has to give:
///   "atLeast":
///   "atMost": both must be positive integers, the cursor skips never
///             more than "atMost" items and tries to skip at least
///             "atLeast". Note that it is possible to skip fewer than
///             "atLeast", for example if there are only fewer items
///             left. However, the implementation may skip fewer items
///             than "atLeast" for internal reasons, for example to avoid
///             excessive copying. The result is a JSON object with a
///             single attribute "skipped" containing the number of
///             skipped items.
///             If "atLeast" is not given it defaults to 1, if "atMost" is not
///             given it defaults to ExecutionBlock::DefaultBatchSize.
/// For the "skip" operation one should give:
///   "number": must be a positive integer, the cursor skips as many items,
///             possibly exhausting the cursor.
///             The result is a JSON with the attributes "error" (boolean),
///             "errorMessage" (if applicable) and "exhausted" (boolean)
///             to indicate whether or not the cursor is exhausted.
///             If "number" is not given it defaults to 1.
/// For the "initializeCursor" operation, one has to bind the following
/// attributes:
///   "items": This is a serialized AqlItemBlock with usually only one row
///            and the correct number of columns.
///   "pos":   The number of the row in "items" to take, usually 0.
/// For the "shutdown" and "lock" operations no additional arguments are
/// required and an empty JSON object in the body is OK.
/// All operations allow to set the HTTP header "Shard-ID:". If this is
/// set, then the root block of the stored query must be a ScatterBlock
/// and the shard ID is given as an additional argument to the ScatterBlock's
/// special API.
void RestAqlHandler::useQuery(std::string const& operation,
                              std::string const& idString) {
  // the PUT verb
  Query* query = nullptr;
  if (findQuery(idString, query)) {
    return;
  }

  TRI_ASSERT(_qId > 0);
  TRI_ASSERT(query->engine() != nullptr);

  std::shared_ptr<VPackBuilder> queryBuilder = parseVelocyPackBody();
  if (queryBuilder == nullptr) {
    _queryRegistry->close(_vocbase, _qId);
    return;
  }

  try {
    handleUseQuery(operation, query, queryBuilder->slice());
    if (_qId != 0) {
      try {
        _queryRegistry->close(_vocbase, _qId);
      } catch (...) {
        // ignore errors on unregistering
        // an error might occur if "shutdown" is called
      }
    }
  } catch (arangodb::basics::Exception const& ex) {
    _queryRegistry->close(_vocbase, _qId);
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR, ex.code(),
                  ex.message());
  } catch (std::exception const& ex) {
    _queryRegistry->close(_vocbase, _qId);

    LOG(ERR) << "failed during use of Query: " << ex.what();

    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_HTTP_SERVER_ERROR, ex.what());
  } catch (...) {
    _queryRegistry->close(_vocbase, _qId);
    LOG(ERR) << "failed during use of Query: Unknown exception occurred";

    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_HTTP_SERVER_ERROR, "an unknown exception occurred");
  }
}

/// @brief GET method for /_api/aql/<operation>/<queryId>, (internal)
/// this is using
/// the part of the cursor API without side effects. The operation must
/// be one of "count", "remaining" and "hasMore". The result is a Json
/// with, depending on the operation, the following attributes:
///   for "count": the result has the attributes "error" (set to false)
///                and "count" set to the total number of documents.
///   for "remaining": the result has the attributes "error" (set to false)
///                and "remaining" set to the total number of documents.
///   for "hasMore": the result has the attributes "error" (set to false)
///                  and "hasMore" set to a boolean value.
/// Note that both "count" and "remaining" may return "unknown" if the
/// internal cursor API returned -1.
void RestAqlHandler::getInfoQuery(std::string const& operation,
                                  std::string const& idString) {
  // the GET verb

  bool found;
  std::string shardId;
  std::string const& shardIdCharP = _request->header("shard-id", found);

  if (found && !shardIdCharP.empty()) {
    shardId = shardIdCharP;
  }

  Query* query = nullptr;
  if (findQuery(idString, query)) {
    return;
  }

  TRI_ASSERT(_qId > 0);

  arangodb::basics::Json answerBody(arangodb::basics::Json::Object, 2);

  TRI_ASSERT(query->engine() != nullptr);

  try {
    int64_t number;
    if (operation == "count") {
      number = query->engine()->count();
      if (number == -1) {
        answerBody("count", arangodb::basics::Json("unknown"));
      } else {
        answerBody("count",
                   arangodb::basics::Json(static_cast<double>(number)));
      }
    } else if (operation == "remaining") {
      if (shardId.empty()) {
        number = query->engine()->remaining();
      } else {
        auto block = static_cast<BlockWithClients*>(query->engine()->root());
        if (block->getPlanNode()->getType() != ExecutionNode::SCATTER &&
            block->getPlanNode()->getType() != ExecutionNode::DISTRIBUTE) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
        }
        number = block->remainingForShard(shardId);
      }
      if (number == -1) {
        answerBody("remaining", arangodb::basics::Json("unknown"));
      } else {
        answerBody("remaining",
                   arangodb::basics::Json(static_cast<double>(number)));
      }
    } else if (operation == "hasMore") {
      bool hasMore;
      if (shardId.empty()) {
        hasMore = query->engine()->hasMore();
      } else {
        auto block = static_cast<BlockWithClients*>(query->engine()->root());
        if (block->getPlanNode()->getType() != ExecutionNode::SCATTER &&
            block->getPlanNode()->getType() != ExecutionNode::DISTRIBUTE) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
        }
        hasMore = block->hasMoreForShard(shardId);
      }

      answerBody("hasMore", arangodb::basics::Json(hasMore));
    } else {
      _queryRegistry->close(_vocbase, _qId);
      LOG(ERR) << "referenced query not found";
      generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                    TRI_ERROR_HTTP_NOT_FOUND);
      return;
    }
  } catch (arangodb::basics::Exception const& ex) {
    _queryRegistry->close(_vocbase, _qId);
    LOG(ERR) << "failed during use of query: " << ex.message();
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR, ex.code(),
                  ex.message());
  } catch (std::exception const& ex) {
    _queryRegistry->close(_vocbase, _qId);

    LOG(ERR) << "failed during use of query: " << ex.what();

    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_HTTP_SERVER_ERROR, ex.what());
  } catch (...) {
    _queryRegistry->close(_vocbase, _qId);

    LOG(ERR) << "failed during use of query: Unknown exception occurred";

    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_HTTP_SERVER_ERROR, "an unknown exception occurred");
  }

  _queryRegistry->close(_vocbase, _qId);

  createResponse(GeneralResponse::ResponseCode::OK);
  _response->setContentType(HttpResponse::CONTENT_TYPE_JSON);
  answerBody("error", arangodb::basics::Json(false));
  _response->body().appendText(answerBody.toString());
}

/// @brief executes the handler
arangodb::rest::HttpHandler::status_t RestAqlHandler::execute() {
  // std::cout << "GOT INCOMING REQUEST: " <<
  // arangodb::rest::HttpRequest::translateMethod(_request->requestType()) << ",
  // " << arangodb::ServerState::instance()->getId() << ": " <<
  // _request->fullUrl() << ": " << _request->body() << "\n\n";

  std::vector<std::string> const& suffix = _request->suffix();

  // extract the sub-request type
  GeneralRequest::RequestType type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case GeneralRequest::RequestType::POST: {
      if (suffix.size() != 1) {
        generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_NOT_FOUND);
      } else if (suffix[0] == "instantiate") {
        createQueryFromVelocyPack();
      } else if (suffix[0] == "parse") {
        parseQuery();
      } else if (suffix[0] == "explain") {
        explainQuery();
      } else if (suffix[0] == "query") {
        createQueryFromString();
      } else {
        LOG(ERR) << "Unknown API";
        generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_NOT_FOUND);
      }
      break;
    }
    case GeneralRequest::RequestType::PUT: {
      if (suffix.size() != 2) {
        LOG(ERR) << "unknown PUT API";
        generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_NOT_FOUND);
      } else {
        useQuery(suffix[0], suffix[1]);
      }
      break;
    }
    case GeneralRequest::RequestType::GET: {
      if (suffix.size() != 2) {
        LOG(ERR) << "Unknown GET API";
        generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_NOT_FOUND);
      } else {
        getInfoQuery(suffix[0], suffix[1]);
      }
      break;
    }
    case GeneralRequest::RequestType::DELETE_REQ:
    case GeneralRequest::RequestType::HEAD:
    case GeneralRequest::RequestType::PATCH:
    case GeneralRequest::RequestType::OPTIONS:
    case GeneralRequest::RequestType::VSTREAM_CRED:
    case GeneralRequest::RequestType::VSTREAM_REGISTER:
    case GeneralRequest::RequestType::VSTREAM_STATUS:
    case GeneralRequest::RequestType::ILLEGAL: {
      generateError(GeneralResponse::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_NOT_IMPLEMENTED, "illegal method for /_api/aql");
      break;
    }
  }

  // std::cout << "REQUEST HANDLING DONE: " <<
  // arangodb::ServerState::instance()->getId() << ": " <<
  // _request->fullUrl() << ": " << _response->responseCode() << ",
  // CONTENT-LENGTH: " << _response->contentLength() << "\n";

  return status_t(HANDLER_DONE);
}

/// @brief dig out the query from ID, handle errors
bool RestAqlHandler::findQuery(std::string const& idString, Query*& query) {
  _qId = arangodb::basics::StringUtils::uint64(idString);
  query = nullptr;

  // sleep for 10ms each time, wait for at most 30 seconds...
  static int64_t const SingleWaitPeriod = 10 * 1000;
  static int64_t const MaxIterations =
      static_cast<int64_t>(30.0 * 1000000.0 / (double)SingleWaitPeriod);

  int64_t iterations = 0;

  // probably need to cycle here until we can get hold of the query
  while (++iterations < MaxIterations) {
    try {
      query = _queryRegistry->open(_vocbase, _qId);
      // we got the query (or it was not found - at least no one else
      // can now have access to the same query)
      break;
    } catch (...) {
      // we can only get here if the query is currently used by someone
      // else. in this case we sleep for a while and re-try
      usleep(SingleWaitPeriod);
    }
  }

  if (query == nullptr) {
    _qId = 0;
    generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                  TRI_ERROR_QUERY_NOT_FOUND);
    return true;
  }

  TRI_ASSERT(_qId > 0);

  return false;
}

/// @brief handle for useQuery
void RestAqlHandler::handleUseQuery(std::string const& operation, Query* query,
                                    VPackSlice const querySlice) {
  bool found;
  std::string shardId;
  std::string const& shardIdCharP = _request->header("shard-id", found);

  if (found && !shardIdCharP.empty()) {
    shardId = shardIdCharP;
  }

  VPackBuilder answerBuilder;
  auto transactionContext = query->trx()->transactionContext();
  try {
    {
      VPackObjectBuilder guard(&answerBuilder);
      if (operation == "lock") {
        // Mark current thread as potentially blocking:
        auto currentThread = arangodb::rest::DispatcherThread::current();

        if (currentThread != nullptr) {
          currentThread->block();
        }
        int res = TRI_ERROR_INTERNAL;
        try {
          res = query->trx()->lockCollections();
        } catch (...) {
          LOG(ERR) << "lock lead to an exception";
          if (currentThread != nullptr) {
            currentThread->unblock();
          }
          generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                        TRI_ERROR_HTTP_SERVER_ERROR,
                        "lock lead to an exception");
          return;
        }
        if (currentThread != nullptr) {
          currentThread->unblock();
        }
        answerBuilder.add("error", VPackValue(res != TRI_ERROR_NO_ERROR));
        answerBuilder.add("code", VPackValue(res));
      } else if (operation == "getSome") {
        auto atLeast =
            VelocyPackHelper::getNumericValue<size_t>(querySlice, "atLeast", 1);
        auto atMost = VelocyPackHelper::getNumericValue<size_t>(
            querySlice, "atMost", ExecutionBlock::DefaultBatchSize());
        std::unique_ptr<AqlItemBlock> items;
        if (shardId.empty()) {
          items.reset(query->engine()->getSome(atLeast, atMost));
        } else {
          auto block = static_cast<BlockWithClients*>(query->engine()->root());
          if (block->getPlanNode()->getType() != ExecutionNode::SCATTER &&
              block->getPlanNode()->getType() != ExecutionNode::DISTRIBUTE) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
          }
          items.reset(block->getSomeForShard(atLeast, atMost, shardId));
        }
        if (items.get() == nullptr) {
          answerBuilder.add("exhausted", VPackValue(true));
          answerBuilder.add("error", VPackValue(false));
          answerBuilder.add(VPackValue("stats"));
          query->getStats(answerBuilder);
        } else {
          try {
            items->toVelocyPack(query->trx(), answerBuilder);
            answerBuilder.add(VPackValue("stats"));
            query->getStats(answerBuilder);
          } catch (...) {
            LOG(ERR) << "cannot transform AqlItemBlock to VelocyPack";
            generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                          TRI_ERROR_HTTP_SERVER_ERROR,
                          "cannot transform AqlItemBlock to VelocyPack");
            return;
          }
        }
      } else if (operation == "skipSome") {
        auto atLeast =
            VelocyPackHelper::getNumericValue<size_t>(querySlice, "atLeast", 1);
        auto atMost = VelocyPackHelper::getNumericValue<size_t>(
            querySlice, "atMost", ExecutionBlock::DefaultBatchSize());
        size_t skipped;
        try {
          if (shardId.empty()) {
            skipped = query->engine()->skipSome(atLeast, atMost);
          } else {
            auto block =
                static_cast<BlockWithClients*>(query->engine()->root());
            if (block->getPlanNode()->getType() != ExecutionNode::SCATTER &&
                block->getPlanNode()->getType() != ExecutionNode::DISTRIBUTE) {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
            }
            skipped = block->skipSomeForShard(atLeast, atMost, shardId);
          }
        } catch (...) {
          LOG(ERR) << "skipSome lead to an exception";
          generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                        TRI_ERROR_HTTP_SERVER_ERROR,
                        "skipSome lead to an exception");
          return;
        }
        answerBuilder.add("skipped", VPackValue(static_cast<double>(skipped)));
        answerBuilder.add("error", VPackValue(false));
        answerBuilder.add(VPackValue("stats"));
        query->getStats(answerBuilder);
      } else if (operation == "skip") {
        auto number =
            VelocyPackHelper::getNumericValue<size_t>(querySlice, "number", 1);
        try {
          bool exhausted;
          if (shardId.empty()) {
            exhausted = query->engine()->skip(number);
          } else {
            auto block =
                static_cast<BlockWithClients*>(query->engine()->root());
            if (block->getPlanNode()->getType() != ExecutionNode::SCATTER &&
                block->getPlanNode()->getType() != ExecutionNode::DISTRIBUTE) {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
            }
            exhausted = block->skipForShard(number, shardId);
          }
          answerBuilder.add("exhausted", VPackValue(exhausted));
          answerBuilder.add("error", VPackValue(false));
          answerBuilder.add(VPackValue("stats"));
          query->getStats(answerBuilder);
        } catch (...) {
          LOG(ERR) << "skip lead to an exception";
          generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                        TRI_ERROR_HTTP_SERVER_ERROR,
                        "skip lead to an exception");
          return;
        }
      } else if (operation == "initializeCursor") {
        auto pos =
            VelocyPackHelper::getNumericValue<size_t>(querySlice, "pos", 0);
        std::unique_ptr<AqlItemBlock> items;
        int res;
        try {
          if (VelocyPackHelper::getBooleanValue(querySlice, "exhausted",
                                                true)) {
            res = query->engine()->initializeCursor(nullptr, 0);
          } else {
            items.reset(new AqlItemBlock(querySlice.get("items")));
            res = query->engine()->initializeCursor(items.get(), pos);
          }
        } catch (...) {
          LOG(ERR) << "initializeCursor lead to an exception";
          generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                        TRI_ERROR_HTTP_SERVER_ERROR,
                        "initializeCursor lead to an exception");
          return;
        }
        answerBuilder.add("error", VPackValue(res != TRI_ERROR_NO_ERROR));
        answerBuilder.add("code", VPackValue(static_cast<double>(res)));
        answerBuilder.add(VPackValue("stats"));
        query->getStats(answerBuilder);
      } else if (operation == "shutdown") {
        int res = TRI_ERROR_INTERNAL;
        int errorCode = VelocyPackHelper::getNumericValue<int>(
            querySlice, "code", TRI_ERROR_INTERNAL);
        try {
          res = query->engine()->shutdown(
              errorCode);  // pass errorCode to shutdown

          // return statistics
          answerBuilder.add(VPackValue("stats"));
          query->getStats(answerBuilder);

          // return warnings if present
          query->addWarningsToVelocyPackObject(answerBuilder);

          // delete the query from the registry
          _queryRegistry->destroy(_vocbase, _qId, errorCode);
          _qId = 0;
        } catch (...) {
          LOG(ERR) << "shutdown lead to an exception";
          generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                        TRI_ERROR_HTTP_SERVER_ERROR,
                        "shutdown lead to an exception");
          return;
        }
        answerBuilder.add("error", VPackValue(res != TRI_ERROR_NO_ERROR));
        answerBuilder.add("code", VPackValue(res));
      } else {
        LOG(ERR) << "Unknown operation!";
        generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_NOT_FOUND);
        return;
      }
    }
    sendResponse(GeneralResponse::ResponseCode::OK, answerBuilder.slice(),
                 transactionContext.get());
  } catch (arangodb::basics::Exception const& e) {
    generateError(GeneralResponse::ResponseCode::BAD, e.code());
    return;
  } catch (...) {
    LOG(ERR) << "OUT OF MEMORY when handling query.";
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
}

/// @brief extract the VelocyPack from the request
std::shared_ptr<VPackBuilder> RestAqlHandler::parseVelocyPackBody() {
  try {
    std::shared_ptr<VPackBuilder> body =
        _request->toVelocyPack(&VPackOptions::Defaults);
    if (body == nullptr) {
      LOG(ERR) << "cannot parse json object";
      generateError(GeneralResponse::ResponseCode::BAD,
                    TRI_ERROR_HTTP_CORRUPTED_JSON, "cannot parse json object");
    }
    VPackSlice tmp = body->slice();
    if (!tmp.isObject()) {
      // Validate the input has correct format.
      LOG(ERR) << "body of request must be a VelocyPack object";
      generateError(GeneralResponse::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "body of request must be a VelcoyPack object");
      return nullptr;
    }
    return body;
  } catch (...) {
    LOG(ERR) << "cannot parse json object";
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_CORRUPTED_JSON, "cannot parse json object");
  }
  return nullptr;
}

/// @brief Send slice as result with the given response type.
void RestAqlHandler::sendResponse(
    GeneralResponse::ResponseCode code, VPackSlice const slice,
    arangodb::TransactionContext* transactionContext) {
  // TODO: use RestBaseHandler
  createResponse(code);
  _response->setContentType(HttpResponse::CONTENT_TYPE_JSON);
  arangodb::basics::VPackStringBufferAdapter buffer(
      _response->body().stringBuffer());
  VPackDumper dumper(&buffer, transactionContext->getVPackOptions());
  try {
    dumper.dump(slice);
  } catch (...) {
    generateError(GeneralResponse::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_INTERNAL, "cannot generate output");
  }
}
