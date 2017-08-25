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
#include "Aql/AqlItemBlock.h"
#include "Aql/ClusterBlocks.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/JobQueue.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/Context.h"
#include "VocBase/ticks.h"

#include <velocypack/Dumper.h>
#include <velocypack/velocypack-aliases.h>

#ifdef USE_ENTERPRISE
#include "Enterprise/Transaction/IgnoreNoAccessAqlTransaction.h"
#endif

using namespace arangodb;
using namespace arangodb::rest;
using namespace arangodb::aql;

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

RestAqlHandler::RestAqlHandler(GeneralRequest* request,
                               GeneralResponse* response,
                               QueryRegistry* queryRegistry)
    : RestVocbaseBaseHandler(request, response),
      _context(static_cast<VocbaseContext*>(request->requestContext())),
      _vocbase(_context->vocbase()),
      _queryRegistry(queryRegistry),
      _qId(0) {
  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(_queryRegistry != nullptr);
}

// returns the queue name
size_t RestAqlHandler::queue() const { return JobQueue::AQL_QUEUE; }

bool RestAqlHandler::isDirect() const { return false; }

// POST method for /_api/aql/instantiate (internal)
// The body is a VelocyPack with attributes "plan" for the execution plan and
// "options" for the options, all exactly as in AQL_EXECUTEJSON.
void RestAqlHandler::createQueryFromVelocyPack() {
  std::shared_ptr<VPackBuilder> queryBuilder = parseVelocyPackBody();
  if (queryBuilder == nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "invalid VelocyPack plan in query";
    return;
  }
  VPackSlice querySlice = queryBuilder->slice();

  TRI_ASSERT(querySlice.isObject());

  VPackSlice plan = querySlice.get("plan");
  if (plan.isNone()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Invalid VelocyPack: \"plan\" attribute missing.";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"plan\"");
    return;
  }

  auto options = std::make_shared<VPackBuilder>(
      VPackBuilder::clone(querySlice.get("options")));

  std::string const part =
      VelocyPackHelper::getStringValue(querySlice, "part", "");

  auto planBuilder = std::make_shared<VPackBuilder>(VPackBuilder::clone(plan));
  auto query = std::make_unique<Query>(false, _vocbase, planBuilder, options,
                                      (part == "main" ? PART_MAIN : PART_DEPENDENT));
  
  try {
    query->prepare(_queryRegistry, 0);
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to instantiate the query: " << ex.what();
    generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_BAD_JSON_PLAN, ex.what());
    return;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to instantiate the query";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_BAD_JSON_PLAN);
    return;
  }

  // Now the query is ready to go, store it in the registry and return:
  double ttl = 600.0;
  bool found;
  std::string const& ttlstring = _request->header("ttl", found);

  if (found) {
    ttl = arangodb::basics::StringUtils::doubleDecimal(ttlstring);
  }

  _qId = TRI_NewTickServer();
  auto transactionContext = query->trx()->transactionContext().get();

  try {
    _queryRegistry->insert(_qId, query.get(), ttl);
    query.release();
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not keep query in registry";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "could not keep query in registry");
    return;
  }

  VPackBuilder answerBody;
  try {
    VPackObjectBuilder guard(&answerBody);
    answerBody.add("queryId",
                   VPackValue(arangodb::basics::StringUtils::itoa(_qId)));
    answerBody.add("ttl", VPackValue(ttl));
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not keep query in registry";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  sendResponse(rest::ResponseCode::ACCEPTED, answerBody.slice(), transactionContext);
}

// POST method for /_api/aql/parse (internal)
// The body is a Json with attributes "query" for the query string,
// "parameters" for the query parameters and "options" for the options.
// This does the same as AQL_PARSE with exactly these parameters and
// does not keep the query hanging around.
void RestAqlHandler::parseQuery() {
  std::shared_ptr<VPackBuilder> bodyBuilder = parseVelocyPackBody();
  if (bodyBuilder == nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "invalid VelocyPack plan in query";
    return;
  }
  VPackSlice querySlice = bodyBuilder->slice();

  std::string const queryString =
      VelocyPackHelper::getStringValue(querySlice, "query", "");
  if (queryString.empty()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "body must be an object with attribute \"query\"";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"query\"");
    return;
  }

  auto query = std::make_unique<Query>(false, _vocbase, QueryString(queryString),
                std::shared_ptr<VPackBuilder>(), nullptr, PART_MAIN);
  QueryResult res = query->parse();
  if (res.code != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to instantiate the Query: " << res.details;
    generateError(rest::ResponseCode::BAD, res.code, res.details);
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
    sendResponse(rest::ResponseCode::OK, answerBuilder.slice(),
                 transactionContext.get());
  } catch (...) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_OUT_OF_MEMORY,
                  "out of memory");
  }
}

// POST method for /_api/aql/explain (internal)
// The body is a Json with attributes "query" for the query string,
// "parameters" for the query parameters and "options" for the options.
// This does the same as AQL_EXPLAIN with exactly these parameters and
// does not keep the query hanging around.
void RestAqlHandler::explainQuery() {
  std::shared_ptr<VPackBuilder> bodyBuilder = parseVelocyPackBody();
  if (bodyBuilder == nullptr) {
    return;
  }
  VPackSlice querySlice = bodyBuilder->slice();

  std::string queryString =
      VelocyPackHelper::getStringValue(querySlice, "query", "");
  if (queryString.empty()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "body must be an object with attribute \"query\"";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"query\"");
    return;
  }

  auto bindVars = std::make_shared<VPackBuilder>(
      VPackBuilder::clone(querySlice.get("parameters")));

  auto options = std::make_shared<VPackBuilder>(
      VPackBuilder::clone(querySlice.get("options")));

  auto query =
      std::make_unique<Query>(false, _vocbase, QueryString(queryString),
                              bindVars, options, PART_MAIN);
  QueryResult res = query->explain();
  if (res.code != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to instantiate the Query: " << res.details;
    generateError(rest::ResponseCode::BAD, res.code, res.details);
    return;
  }

  // Now prepare the answer:
  VPackBuilder answerBuilder;
  try {
    {
      VPackObjectBuilder guard(&answerBuilder);
      if (res.result != nullptr) {
        if (query->queryOptions().allPlans) {
          answerBuilder.add(VPackValue("plans"));
        } else {
          answerBuilder.add(VPackValue("plan"));
        }
        answerBuilder.add(res.result->slice());
        res.result = nullptr;
      }
    }
    sendResponse(rest::ResponseCode::OK, answerBuilder.slice(),
                 query->trx()->transactionContext().get());
  } catch (...) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_OUT_OF_MEMORY,
                  "out of memory");
  }
}

// POST method for /_api/aql/query (internal)
// The body is a Json with attributes "query" for the query string,
// "parameters" for the query parameters and "options" for the options.
// This sets up the query as as AQL_EXECUTE would, but does not use
// the cursor API yet. Rather, the query is stored in the query registry
// for later use by PUT requests.
void RestAqlHandler::createQueryFromString() {
  std::shared_ptr<VPackBuilder> queryBuilder = parseVelocyPackBody();
  if (queryBuilder == nullptr) {
    return;
  }
  VPackSlice querySlice = queryBuilder->slice();

  std::string const queryString =
      VelocyPackHelper::getStringValue(querySlice, "query", "");
  if (queryString.empty()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "body must be an object with attribute \"query\"";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"query\"");
    return;
  }

  std::string const part =
      VelocyPackHelper::getStringValue(querySlice, "part", "");
  if (part.empty()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "body must be an object with attribute \"part\"";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "body must be an object with attribute \"part\"");
    return;
  }

  auto bindVars = std::make_shared<VPackBuilder>(
      VPackBuilder::clone(querySlice.get("parameters")));
  auto options = std::make_shared<VPackBuilder>(
      VPackBuilder::clone(querySlice.get("options")));

  auto query = std::make_unique<Query>(false, _vocbase, QueryString(queryString),
                         bindVars, options,
                         (part == "main" ? PART_MAIN : PART_DEPENDENT));
  
  try {
    query->prepare(_queryRegistry, 0);
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to instantiate the query: " << ex.what();
    generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_BAD_JSON_PLAN, ex.what());
    return;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to instantiate the query";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_BAD_JSON_PLAN);
    return;
  }

  // Now the query is ready to go, store it in the registry and return:
  double ttl = 600.0;
  bool found;
  std::string const& ttlstring = _request->header("ttl", found);

  if (found) {
    ttl = arangodb::basics::StringUtils::doubleDecimal(ttlstring);
  }

  auto transactionContext = query->trx()->transactionContext().get();
  _qId = TRI_NewTickServer();

  try {
    _queryRegistry->insert(_qId, query.get(), ttl);
    query.release();
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not keep query in registry";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "could not keep query in registry");
    return;
  }

  VPackBuilder answerBody;
  try {
    VPackObjectBuilder guard(&answerBody);
    answerBody.add("queryId",
                   VPackValue(arangodb::basics::StringUtils::itoa(_qId)));
    answerBody.add("ttl", VPackValue(ttl));
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not keep query in registry";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  sendResponse(rest::ResponseCode::ACCEPTED, answerBody.slice(), transactionContext);
}

// PUT method for /_api/aql/<operation>/<queryId>, (internal)
// this is using the part of the cursor API with side effects.
// <operation>: can be "lock" or "getSome" or "skip" or "initializeCursor" or
// "shutdown".
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
//             If "atLeast" is not given it defaults to 1, if "atMost" is not
//             given it defaults to ExecutionBlock::DefaultBatchSize.
// For the "skipSome" operation one has to give:
//   "atLeast":
//   "atMost": both must be positive integers, the cursor skips never
//             more than "atMost" items and tries to skip at least
//             "atLeast". Note that it is possible to skip fewer than
//             "atLeast", for example if there are only fewer items
//             left. However, the implementation may skip fewer items
//             than "atLeast" for internal reasons, for example to avoid
//             excessive copying. The result is a JSON object with a
//             single attribute "skipped" containing the number of
//             skipped items.
//             If "atLeast" is not given it defaults to 1, if "atMost" is not
//             given it defaults to ExecutionBlock::DefaultBatchSize.
// For the "skip" operation one should give:
//   "number": must be a positive integer, the cursor skips as many items,
//             possibly exhausting the cursor.
//             The result is a JSON with the attributes "error" (boolean),
//             "errorMessage" (if applicable) and "exhausted" (boolean)
//             to indicate whether or not the cursor is exhausted.
//             If "number" is not given it defaults to 1.
// For the "initializeCursor" operation, one has to bind the following
// attributes:
//   "items": This is a serialized AqlItemBlock with usually only one row
//            and the correct number of columns.
//   "pos":   The number of the row in "items" to take, usually 0.
// For the "shutdown" and "lock" operations no additional arguments are
// required and an empty JSON object in the body is OK.
// All operations allow to set the HTTP header "Shard-ID:". If this is
// set, then the root block of the stored query must be a ScatterBlock
// and the shard ID is given as an additional argument to the ScatterBlock's
// special API.
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
    generateError(rest::ResponseCode::SERVER_ERROR, ex.code(),
                  ex.message());
  } catch (std::exception const& ex) {
    _queryRegistry->close(_vocbase, _qId);

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed during use of Query: " << ex.what();

    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_HTTP_SERVER_ERROR, ex.what());
  } catch (...) {
    _queryRegistry->close(_vocbase, _qId);
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed during use of Query: Unknown exception occurred";

    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_HTTP_SERVER_ERROR, "an unknown exception occurred");
  }
}

// GET method for /_api/aql/<operation>/<queryId>, (internal)
// this is using
// the part of the cursor API without side effects. The operation must
// be one of "count", "remaining" and "hasMore". The result is a Json
// with, depending on the operation, the following attributes:
//   for "count": the result has the attributes "error" (set to false)
//                and "count" set to the total number of documents.
//   for "remaining": the result has the attributes "error" (set to false)
//                and "remaining" set to the total number of documents.
//   for "hasMore": the result has the attributes "error" (set to false)
//                  and "hasMore" set to a boolean value.
// Note that both "count" and "remaining" may return "unknown" if the
// internal cursor API returned -1.

void RestAqlHandler::getInfoQuery(std::string const& operation,
                                  std::string const& idString) {
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

  VPackBuilder answerBody;
  try {
    VPackObjectBuilder guard(&answerBody);

    int64_t number;

    if (operation == "count") {
      number = query->engine()->count();
      if (number == -1) {
        answerBody.add("count", VPackValue("unknown"));
      } else {
        answerBody.add("count", VPackValue(number));
      }
    } else if (operation == "remaining") {
      if (shardId.empty()) {
        number = query->engine()->remaining();
      } else {
        auto block = static_cast<BlockWithClients*>(query->engine()->root());
        if (block->getPlanNode()->getType() != ExecutionNode::SCATTER &&
            block->getPlanNode()->getType() != ExecutionNode::DISTRIBUTE) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected node type");
        }
        number = block->remainingForShard(shardId);
      }
      if (number == -1) {
        answerBody.add("remaining", VPackValue("unknown"));
      } else {
        answerBody.add("remaining", VPackValue(number));
      }
    } else if (operation == "hasMore") {
      bool hasMore;
      if (shardId.empty()) {
        hasMore = query->engine()->hasMore();
      } else {
        auto block = static_cast<BlockWithClients*>(query->engine()->root());
        if (block->getPlanNode()->getType() != ExecutionNode::SCATTER &&
            block->getPlanNode()->getType() != ExecutionNode::DISTRIBUTE) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected node type");
        }
        hasMore = block->hasMoreForShard(shardId);
      }

      answerBody.add("hasMore", VPackValue(hasMore));
    } else {
      _queryRegistry->close(_vocbase, _qId);
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "referenced query not found";
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_HTTP_NOT_FOUND);
      return;
    }

    answerBody.add("error", VPackValue(false));
  } catch (arangodb::basics::Exception const& ex) {
    _queryRegistry->close(_vocbase, _qId);
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed during use of query: " << ex.message();
    generateError(rest::ResponseCode::SERVER_ERROR, ex.code(),
                  ex.message());
  } catch (std::exception const& ex) {
    _queryRegistry->close(_vocbase, _qId);

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed during use of query: " << ex.what();

    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_HTTP_SERVER_ERROR, ex.what());
  } catch (...) {
    _queryRegistry->close(_vocbase, _qId);

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed during use of query: Unknown exception occurred";

    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_HTTP_SERVER_ERROR, "an unknown exception occurred");
  }

  _queryRegistry->close(_vocbase, _qId);

  sendResponse(rest::ResponseCode::OK, answerBody.slice(),
               query->trx()->transactionContext().get());
}

// executes the handler
RestStatus RestAqlHandler::execute() {
  // std::cout << "GOT INCOMING REQUEST: " <<
  // GeneralRequest::translateMethod(_request->requestType()) << ",
  // " << arangodb::ServerState::instance()->getId() << ": " <<
  // _request->fullUrl() << ": " << _request->body() << "\n\n";
  std::vector<std::string> const& suffixes = _request->suffixes();

  // extract the sub-request type
  rest::RequestType type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case rest::RequestType::POST: {
      if (suffixes.size() != 1) {
        generateError(rest::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_NOT_FOUND);
      } else if (suffixes[0] == "instantiate") {
        createQueryFromVelocyPack();
      } else if (suffixes[0] == "parse") {
        parseQuery();
      } else if (suffixes[0] == "explain") {
        explainQuery();
      } else if (suffixes[0] == "query") {
        createQueryFromString();
      } else {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Unknown API";
        generateError(rest::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_NOT_FOUND);
      }
      break;
    }
    case rest::RequestType::PUT: {
      if (suffixes.size() != 2) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unknown PUT API";
        generateError(rest::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_NOT_FOUND);
      } else {
        useQuery(suffixes[0], suffixes[1]);
      }
      break;
    }
    case rest::RequestType::GET: {
      if (suffixes.size() != 2) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Unknown GET API";
        generateError(rest::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_NOT_FOUND);
      } else {
        getInfoQuery(suffixes[0], suffixes[1]);
      }
      break;
    }
    case rest::RequestType::DELETE_REQ:
    case rest::RequestType::HEAD:
    case rest::RequestType::PATCH:
    case rest::RequestType::OPTIONS:
    case rest::RequestType::VSTREAM_CRED:
    case rest::RequestType::VSTREAM_REGISTER:
    case rest::RequestType::VSTREAM_STATUS:
    case rest::RequestType::ILLEGAL: {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_NOT_IMPLEMENTED, "illegal method for /_api/aql");
      break;
    }
  }

  // std::cout << "REQUEST HANDLING DONE: " <<
  // arangodb::ServerState::instance()->getId() << ": " <<
  // _request->fullUrl() << ": " << _response->responseCode() << ",
  // CONTENT-LENGTH: " << _response->contentLength() << "\n";

  return RestStatus::DONE;
}

// dig out the query from ID, handle errors
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
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_QUERY_NOT_FOUND);
    return true;
  }

  TRI_ASSERT(_qId > 0);

  return false;
}

// handle for useQuery
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
        int res = TRI_ERROR_INTERNAL;

        {
          JobGuard guard(SchedulerFeature::SCHEDULER);
          guard.block();

          try {
            res = query->trx()->lockCollections();
          } catch (...) {
            generateError(rest::ResponseCode::SERVER_ERROR,
                          TRI_ERROR_HTTP_SERVER_ERROR,
                          "lock lead to an exception");
            return;
          }
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
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected node type");
          }
          items.reset(block->getSomeForShard(atLeast, atMost, shardId));
        }
        if (items.get() == nullptr) {
          answerBuilder.add("exhausted", VPackValue(true));
          answerBuilder.add("error", VPackValue(false));
        } else {
          try {
            items->toVelocyPack(query->trx(), answerBuilder);
          } catch (...) {
            generateError(rest::ResponseCode::SERVER_ERROR,
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
              THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected node type");
            }
            skipped = block->skipSomeForShard(atLeast, atMost, shardId);
          }
        } catch (...) {
          generateError(rest::ResponseCode::SERVER_ERROR,
                        TRI_ERROR_HTTP_SERVER_ERROR,
                        "skipSome lead to an exception");
          return;
        }
        answerBuilder.add("skipped", VPackValue(static_cast<double>(skipped)));
        answerBuilder.add("error", VPackValue(false));
      } else if (operation == "skip") {
        auto number =
            VelocyPackHelper::getNumericValue<size_t>(querySlice, "number", 1);
        try {
          bool exhausted;
          if (shardId.empty()) {
            size_t numActuallySkipped = 0;
            exhausted = query->engine()->skip(number, numActuallySkipped);
          } else {
            auto block =
                static_cast<BlockWithClients*>(query->engine()->root());
            if (block->getPlanNode()->getType() != ExecutionNode::SCATTER &&
                block->getPlanNode()->getType() != ExecutionNode::DISTRIBUTE) {
              THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected node type");
            }
            exhausted = block->skipForShard(number, shardId);
          }
          answerBuilder.add("exhausted", VPackValue(exhausted));
          answerBuilder.add("error", VPackValue(false));
        } catch (...) {
          generateError(rest::ResponseCode::SERVER_ERROR,
                        TRI_ERROR_HTTP_SERVER_ERROR,
                        "skip lead to an exception");
          return;
        }
      } else if (operation == "initialize") {
        int res;
        try {
          res = query->engine()->initialize();
        } catch (arangodb::basics::Exception const& ex) {
          generateError(rest::ResponseCode::SERVER_ERROR, ex.code(), "initialize lead to an exception: " + ex.message());
          return;
        } catch (...) {
          generateError(rest::ResponseCode::SERVER_ERROR,
                        TRI_ERROR_HTTP_SERVER_ERROR,
                        "initialize lead to an exception");
          return;
        }
        answerBuilder.add("error", VPackValue(res != TRI_ERROR_NO_ERROR));
        answerBuilder.add("code", VPackValue(static_cast<double>(res)));
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
            items.reset(new AqlItemBlock(query->resourceMonitor(), querySlice.get("items")));
            res = query->engine()->initializeCursor(items.get(), pos);
          }
        } catch (arangodb::basics::Exception const& ex) {
          generateError(rest::ResponseCode::SERVER_ERROR, ex.code(), "initializeCursor lead to an exception: " + ex.message());
          return;
        } catch (...) {
          generateError(rest::ResponseCode::SERVER_ERROR,
                        TRI_ERROR_HTTP_SERVER_ERROR,
                        "initializeCursor lead to an exception");
          return;
        }
        answerBuilder.add("error", VPackValue(res != TRI_ERROR_NO_ERROR));
        answerBuilder.add("code", VPackValue(static_cast<double>(res)));
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

          // return the query to the registry
          _queryRegistry->close(_vocbase, _qId);

          // delete the query from the registry
          _queryRegistry->destroy(_vocbase, _qId, errorCode);
          _qId = 0;
        } catch (arangodb::basics::Exception const& ex) {
          generateError(rest::ResponseCode::SERVER_ERROR, ex.code(), "shutdown lead to an exception: " + ex.message());
          return;
        } catch (...) {
          generateError(rest::ResponseCode::SERVER_ERROR,
                        TRI_ERROR_HTTP_SERVER_ERROR,
                        "shutdown lead to an exception");
          return;
        }
        answerBuilder.add("error", VPackValue(res != TRI_ERROR_NO_ERROR));
        answerBuilder.add("code", VPackValue(res));
      } else {
        generateError(rest::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_NOT_FOUND);
        return;
      }
    }
    sendResponse(rest::ResponseCode::OK, answerBuilder.slice(),
                 transactionContext.get());
  } catch (arangodb::basics::Exception const& e) {
    generateError(rest::ResponseCode::BAD, e.code());
    return;
  } catch (...) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
}

// extract the VelocyPack from the request
std::shared_ptr<VPackBuilder> RestAqlHandler::parseVelocyPackBody() {
  try {
    std::shared_ptr<VPackBuilder> body =
        _request->toVelocyPackBuilderPtr();
    if (body == nullptr) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot parse json object";
      generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_HTTP_CORRUPTED_JSON, "cannot parse json object");
    }
    VPackSlice tmp = body->slice();
    if (!tmp.isObject()) {
      // Validate the input has correct format.
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "body of request must be a VelocyPack object";
      generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "body of request must be a VelcoyPack object");
      return nullptr;
    }
    return body;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot parse json object";
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_CORRUPTED_JSON, "cannot parse json object");
  }
  return nullptr;
}

// Send slice as result with the given response type.
void RestAqlHandler::sendResponse(
    rest::ResponseCode code, VPackSlice const slice,
    transaction::Context* transactionContext) {
  resetResponse(code);
  writeResult(slice, *(transactionContext->getVPackOptionsForDump()));
}
