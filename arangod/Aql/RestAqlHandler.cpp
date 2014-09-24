////////////////////////////////////////////////////////////////////////////////
/// @brief AQL query/cursor request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestAqlHandler.h"

#include "Basics/ConditionLocker.h"
#include "Basics/StringUtils.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

#include "HttpServer/HttpServer.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "GeneralServer/GeneralServerJob.h"
#include "GeneralServer/GeneralServer.h"

#include "VocBase/server.h"
#include "V8Server/v8-vocbaseprivate.h"

#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionBlock.h"

using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::rest;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the queue
////////////////////////////////////////////////////////////////////////////////

const std::string RestAqlHandler::QUEUE_NAME = "STANDARD";

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestAqlHandler::RestAqlHandler (triagens::rest::HttpRequest* request,
                   std::pair<ApplicationV8*, QueryRegistry*>* pair)
  : RestBaseHandler(request),
    _applicationV8(pair->first),
    _context(static_cast<VocbaseContext*>(request->getRequestContext())),
    _vocbase(_context->getVocbase()),
    _queryRegistry(pair->second) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestAqlHandler::isDirect () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

std::string const& RestAqlHandler::queue () const {
  return QUEUE_NAME;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief POST method for /_api/aql/instanciate
/// The body is a JSON with attributes "plan" for the execution plan and
/// "options" for the options, all exactly as in AQL_EXECUTEJSON.
////////////////////////////////////////////////////////////////////////////////

void RestAqlHandler::createQueryFromJson () {
  Json queryJson(TRI_UNKNOWN_MEM_ZONE, parseJsonBody());
  if (queryJson.isEmpty()) {
    return;
  }
  
  TRI_vocbase_t* vocbase = GetContextVocBase();
  if (vocbase == nullptr) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_INTERNAL, "cannot get vocbase from context");
    return;
  }

  Json plan;
  Json options;

  plan = queryJson.get("plan").copy();   // cannot throw
  if (plan.isEmpty()) {
    generateError(HttpResponse::BAD, TRI_ERROR_INTERNAL,
      "body must be an object with attribute \"plan\"");
    return;
  }
  options = queryJson.get("options").copy();

  auto query = new Query(vocbase, plan, options.steal());
  QueryResult res = query->prepare();
  if (res.code != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::BAD, TRI_ERROR_QUERY_BAD_JSON_PLAN,
      res.details);
    delete query;
    return;
  }

  // Now the query is ready to go, store it in the registry and return:
  double ttl = 3600.0;
  bool found;
  char const* ttlstring = _request->header("ttl", found);
  if (found) {
    ttl = StringUtils::doubleDecimal(ttlstring);
  }
  QueryId qId = TRI_NewTickServer();
  try {
    _queryRegistry->insert(vocbase, qId, query, ttl);
  }
  catch (...) {
    generateError(HttpResponse::BAD, TRI_ERROR_INTERNAL,
        "could not keep query in registry");
    delete query;
    return;
  }

  _response = createResponse(triagens::rest::HttpResponse::ACCEPTED);
  _response->setContentType("application/json; charset=utf-8");
  Json answerBody(Json::Array, 2);
  answerBody("queryId", Json(StringUtils::itoa(qId)))
            ("ttl",     Json(ttl));
  _response->body().appendText(answerBody.toString());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief POST method for /_api/aql/parse
/// The body is a Json with attributes "query" for the query string,
/// "parameters" for the query parameters and "options" for the options.
/// This does the same as AQL_PARSE with exactly these parameters and
/// does not keep the query hanging around.
////////////////////////////////////////////////////////////////////////////////

void RestAqlHandler::parseQuery () {
  _response = createResponse(triagens::rest::HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  Json answerBody(Json::Array, 1);
  answerBody("bla", Json("Hallo"));
  _response->body().appendText(answerBody.toString());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief POST method for /_api/aql/explain
/// The body is a Json with attributes "query" for the query string,
/// "parameters" for the query parameters and "options" for the options.
/// This does the same as AQL_EXPLAIN with exactly these parameters and
/// does not keep the query hanging around.
////////////////////////////////////////////////////////////////////////////////

void RestAqlHandler::explainQuery () {
  Json queryJson(TRI_UNKNOWN_MEM_ZONE, parseJsonBody());
  if (queryJson.isEmpty()) {
    return;
  }
  
  TRI_vocbase_t* vocbase = GetContextVocBase();
  if (vocbase == nullptr) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_INTERNAL, "cannot get vocbase from context");
    return;
  }

  std::string queryString;
  Json parameters;
  Json options;

  queryString = JsonHelper::getStringValue(queryJson.json(), "query", "");
  if (queryString.empty()) {
    generateError(HttpResponse::BAD, TRI_ERROR_INTERNAL,
      "body must be an object with attribute \"query\"");
    return;
  }
  parameters = queryJson.get("parameters").copy();  // cannot throw
  options = queryJson.get("options").copy();        // cannot throw

  auto query = new Query(vocbase, queryString.c_str(), queryString.size(),
                         parameters.steal(), options.steal());
  QueryResult res = query->explain();
  if (res.code != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::BAD, res.code, res.details);
    delete query;
    return;
  }

  // Now prepare the answer:
  Json answerBody(Json::Array, 1);
  if (res.json != nullptr) {
    if (query->allPlans()) {
      answerBody("plans", Json(res.zone, res.json));
    }
    else {
      answerBody("plan", Json(res.zone, res.json));
    }
  }
  res.json = nullptr;

  _response = createResponse(triagens::rest::HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  _response->body().appendText(answerBody.toString());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief POST method for /_api/aql/query
/// The body is a Json with attributes "query" for the query string,
/// "parameters" for the query parameters and "options" for the options.
/// This sets up the query as as AQL_EXECUTE would, but does not use
/// the cursor API yet. Rather, the query is stored in the query registry
/// for later use by PUT requests.
////////////////////////////////////////////////////////////////////////////////

void RestAqlHandler::createQueryFromString () {
  _response = createResponse(triagens::rest::HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  Json answerBody(Json::Array, 1);
  answerBody("bla", Json("Hallo"));
  _response->body().appendText(answerBody.toString());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief DELETE method for /_api/aql/<queryId>
/// The query specified by <queryId> is deleted.
////////////////////////////////////////////////////////////////////////////////

void RestAqlHandler::deleteQuery (std::string const& idString) {
  // the DELETE verb

  QueryId qId;
  TRI_vocbase_t* vocbase;
  Query* query = nullptr;
  if (findQuery(idString, qId, vocbase, query)) {
    return;
  }

  _queryRegistry->destroy(vocbase, qId);

  _response = createResponse(triagens::rest::HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  Json answerBody(Json::Array, 2);
  answerBody("error", Json(false))
            ("queryId", Json(idString));
  _response->body().appendText(answerBody.toString());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief PUT method for /_api/aql/<operation>/<queryId>, this is using 
/// the part of the cursor API with side effects.
/// <operation>: can be "getSome" or "skip".
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
/// For the "skip" operation one should give:
///   "number": must be a positive integer, the cursor skips as many items,
///             possibly exhausting the cursor.
///             The result is a JSON with the attributes "error" (boolean),
///             "errorMessage" (if applicable) and "exhausted" (boolean)
///             to indicate whether or not the cursor is exhausted.
///             If "number" is not given it defaults to 1.
////////////////////////////////////////////////////////////////////////////////

void RestAqlHandler::useQuery (std::string const& operation,
                               std::string const& idString) {
  // the PUT verb

  QueryId qId;
  TRI_vocbase_t* vocbase;
  Query* query = nullptr;
  if (findQuery(idString, qId, vocbase, query)) {
    return;
  }

  TRI_ASSERT(query->engine() != nullptr);

  Json queryJson(TRI_UNKNOWN_MEM_ZONE, parseJsonBody());
  if (queryJson.isEmpty()) {
    _queryRegistry->close(vocbase, qId);
    return;
  }

  Json answerBody(Json::Array, 2);

  if (operation == "getSome") {
    auto atLeast = JsonHelper::getNumericValue<uint64_t>(queryJson.json(),
                                                         "atLeast", 1);
    auto atMost = JsonHelper::getNumericValue<uint64_t>(queryJson.json(),
                               "atMost", ExecutionBlock::DefaultBatchSize);
    std::unique_ptr<AqlItemBlock> items(query->engine()->getSome(atLeast, atMost));
    if (items.get() == nullptr) {
      answerBody("items", Json(Json::Null));
    }
    else {
      try {
        answerBody("items", items->toJson(query->trx()));
      }
      catch (...) {
        _queryRegistry->close(vocbase, qId);
        generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR,
                      "cannot transform AqlItemBlock to Json");
        return;
      }
    }
  }
  else if (operation == "skip") {
    auto number = JsonHelper::getNumericValue<uint64_t>(queryJson.json(),
                                                         "number", 1);
    try {
      bool exhausted = query->engine()->skip(number);
      answerBody("exhausted", Json(exhausted));
    }
    catch (...) {
      _queryRegistry->close(vocbase, qId);
      generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR,
                    "skip lead to an exception");
      return;
    }
  }
  else {
    _queryRegistry->close(vocbase, qId);
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return;
  }

  _queryRegistry->close(vocbase, qId);

  _response = createResponse(triagens::rest::HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  answerBody("error", Json(false));
  _response->body().appendText(answerBody.toString());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief GET method for /_api/aql/<operation>/<queryId>, this is using
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
////////////////////////////////////////////////////////////////////////////////

void RestAqlHandler::getInfoQuery (std::string const& operation,
                                   std::string const& idString) {
  // the GET verb

  QueryId qId;
  TRI_vocbase_t* vocbase;
  Query* query = nullptr;
  if (findQuery(idString, qId, vocbase, query)) {
    return;
  }

  Json answerBody(Json::Array, 2);

  TRI_ASSERT(query->engine() != nullptr);

  int64_t number;
  if (operation == "count") {
    number = query->engine()->count();
    if (number == -1) {
      answerBody("count", Json("unknown"));
    }
    else {
      answerBody("count", Json(static_cast<double>(number)));
    }
  }
  else if (operation == "remaining") {
    number = query->engine()->remaining();
    if (number == -1) {
      answerBody("remaining", Json("unknown"));
    }
    else {
      answerBody("remaining", Json(static_cast<double>(number)));
    }
  }
  else if (operation == "hasMore") {
    bool hasMore = query->engine()->hasMore();
    answerBody("hasMore", Json(hasMore));
  }
  else {
    _queryRegistry->close(vocbase, qId);
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return;
  }

  _queryRegistry->close(vocbase, qId);

  _response = createResponse(triagens::rest::HttpResponse::OK);
  _response->setContentType("application/json; charset=utf-8");
  answerBody("error", Json(false));
  _response->body().appendText(answerBody.toString());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the handler
////////////////////////////////////////////////////////////////////////////////

triagens::rest::HttpHandler::status_t RestAqlHandler::execute () {
  auto context = _applicationV8->enterContext("STANDARD", _vocbase,
                                              false, false);
  if (nullptr == context) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "cannot enter V8 context");
  }
  else {
    std::vector<std::string> const& suffix = _request->suffix();

    // extract the sub-request type
    HttpRequest::HttpRequestType type = _request->requestType();

    // execute one of the CRUD methods
    switch (type) {
      case HttpRequest::HTTP_REQUEST_POST: {
        if (suffix.size() != 1) {
          generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
        }
        else if (suffix[0] == "instanciate") {
          createQueryFromJson(); 
        }
        else if (suffix[0] == "parse") {
          parseQuery();
        }
        else if (suffix[0] == "explain") {
          explainQuery();
        }
        else if (suffix[0] == "query") {
          createQueryFromString();
        }
        else {
          generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
        }
        break;
      }
      case HttpRequest::HTTP_REQUEST_DELETE: {
        if (suffix.size() != 1) {
          generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
        }
        else {
          deleteQuery(suffix[0]);
        }
        break;
      }
      case HttpRequest::HTTP_REQUEST_PUT: {
        if (suffix.size() != 2) {
          generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
        }
        else {
          useQuery(suffix[0], suffix[1]);
        }
        break;
      }
      case HttpRequest::HTTP_REQUEST_GET: {
        if (suffix.size() != 2) {
          generateError(HttpResponse::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
        }
        else {
          getInfoQuery(suffix[0], suffix[1]);
        }
        break;
      }
      case HttpRequest::HTTP_REQUEST_HEAD:
      case HttpRequest::HTTP_REQUEST_PATCH:
      case HttpRequest::HTTP_REQUEST_OPTIONS:
      case HttpRequest::HTTP_REQUEST_ILLEGAL: {
        generateError(HttpResponse::METHOD_NOT_ALLOWED, 
                      TRI_ERROR_NOT_IMPLEMENTED,
                      "illegal method for /_api/aql");
        break;
      }
    }

  }

  _applicationV8->exitContext(context);

  return status_t(HANDLER_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dig out vocbase from context and query from ID, handle errors
////////////////////////////////////////////////////////////////////////////////

bool RestAqlHandler::findQuery (std::string const& idString,
                                QueryId& qId,
                                TRI_vocbase_t*& vocbase,
                                Query*& query) {
  // get the vocbase from the context:
  vocbase = GetContextVocBase();
  if (vocbase == nullptr) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_INTERNAL, "cannot get vocbase from context");
    return true;
  }

  qId = StringUtils::uint64(idString);

  query = nullptr;

  try {
    query = _queryRegistry->open(vocbase, qId);
  }
  catch (...) {
    generateError(HttpResponse::FORBIDDEN, TRI_ERROR_QUERY_IN_USE);
    return true;
  }

  if (query == nullptr) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_QUERY_NOT_FOUND);
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// parseJsonBody
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* RestAqlHandler::parseJsonBody () {
  char* errmsg = nullptr;
  TRI_json_t* json = _request->toJson(&errmsg);

  if (json == nullptr) {
    if (errmsg == nullptr) {
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_CORRUPTED_JSON,
                    "cannot parse json object");
    }
    else {
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_CORRUPTED_JSON,
                    errmsg);

      TRI_FreeString(TRI_CORE_MEM_ZONE, errmsg);
    }

    return nullptr;
  }

  TRI_ASSERT(errmsg == nullptr);

  if (! TRI_IsArrayJson(json)) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body of request must be a JSON array");
    return nullptr;
  }

  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
