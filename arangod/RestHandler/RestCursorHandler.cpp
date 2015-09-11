////////////////////////////////////////////////////////////////////////////////
/// @brief cursor request handler
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestCursorHandler.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/MutexLocker.h"
#include "Basics/ScopeGuard.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "V8Server/ApplicationV8.h"

using namespace triagens::arango;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestCursorHandler::RestCursorHandler (HttpRequest* request,
                                      std::pair<triagens::arango::ApplicationV8*, triagens::aql::QueryRegistry*>* pair) 
  : RestVocbaseBaseHandler(request),
    _applicationV8(pair->first),
    _queryRegistry(pair->second),
    _queryLock(),
    _query(nullptr),
    _queryKilled(false) {

}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestCursorHandler::execute () {
  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  if (type == HttpRequest::HTTP_REQUEST_POST) {
    createCursor();
    return status_t(HANDLER_DONE);
  }

  if (type == HttpRequest::HTTP_REQUEST_PUT) {
    modifyCursor();
    return status_t(HANDLER_DONE);
  }

  if (type == HttpRequest::HTTP_REQUEST_DELETE) {
    deleteCursor();
    return status_t(HANDLER_DONE);
  }
   
  generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED); 
  return status_t(HANDLER_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestCursorHandler::cancel () {
  return cancelQuery();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief processes the query and returns the results/cursor
/// this method is also used by derived classes
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::processQuery (TRI_json_t const* json) {
  if (! TRI_IsObjectJson(json)) {
    generateError(HttpResponse::BAD, TRI_ERROR_QUERY_EMPTY);
    return;
  }

  auto const* queryString = TRI_LookupObjectJson(json, "query");

  if (! TRI_IsStringJson(queryString)) {
    generateError(HttpResponse::BAD, TRI_ERROR_QUERY_EMPTY);
    return;
  }

  auto const* bindVars = TRI_LookupObjectJson(json, "bindVars");

  if (bindVars != nullptr) {
    if (! TRI_IsObjectJson(bindVars) && 
        ! TRI_IsNullJson(bindVars)) {
      generateError(HttpResponse::BAD, TRI_ERROR_TYPE_ERROR, "expecting object for <bindVars>");
      return;
    }
  }
  
  auto options = buildOptions(json);

  triagens::aql::Query query(_applicationV8, 
                             false, 
                             _vocbase, 
                             queryString->_value._string.data,
                             static_cast<size_t>(queryString->_value._string.length - 1),
                             (bindVars != nullptr ? TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, bindVars) : nullptr),
                             TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, options.json()), 
                             triagens::aql::PART_MAIN);

  registerQuery(&query); 
  auto queryResult = query.execute(_queryRegistry);
  unregisterQuery(); 

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        (queryResult.code == TRI_ERROR_QUERY_KILLED && wasCanceled())) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  TRI_ASSERT(TRI_IsArrayJson(queryResult.json));
 
  { 
    _response = createResponse(HttpResponse::CREATED);
    _response->setContentType("application/json; charset=utf-8");

    // build "extra" attribute
    triagens::basics::Json extra(triagens::basics::Json::Object, 3); 

    if (queryResult.stats != nullptr) {
      extra.set("stats", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, queryResult.stats, triagens::basics::Json::AUTOFREE));
      queryResult.stats = nullptr;
    }
    if (queryResult.profile != nullptr) {
      extra.set("profile", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, queryResult.profile, triagens::basics::Json::AUTOFREE));
      queryResult.profile = nullptr;
    }
    if (queryResult.warnings == nullptr) {
      extra.set("warnings", triagens::basics::Json(triagens::basics::Json::Array));
    }
    else {
      extra.set("warnings", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, queryResult.warnings, triagens::basics::Json::AUTOFREE));
      queryResult.warnings = nullptr;
    }


    size_t batchSize = triagens::basics::JsonHelper::getNumericValue<size_t>(options.json(), "batchSize", 1000);
    size_t const n = TRI_LengthArrayJson(queryResult.json);

    if (n <= batchSize) {
      // result is smaller than batchSize and will be returned directly. no need to create a cursor

      triagens::basics::Json result(triagens::basics::Json::Object, 7);
      result.set("result", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, queryResult.json, triagens::basics::Json::AUTOFREE));
      queryResult.json = nullptr;

      result.set("hasMore", triagens::basics::Json(false));

      if (triagens::basics::JsonHelper::getBooleanValue(options.json(), "count", false)) {
        result.set("count", triagens::basics::Json(static_cast<double>(n)));
      }
    
      result.set("cached", triagens::basics::Json(queryResult.cached));
      result.set("extra", extra);
      result.set("error", triagens::basics::Json(false));
      result.set("code", triagens::basics::Json(static_cast<double>(_response->responseCode())));

      result.dump(_response->body());
      return;
    }
      
    // result is bigger than batchSize, and a cursor will be created
    auto cursors = static_cast<triagens::arango::CursorRepository*>(_vocbase->_cursorRepository);
    TRI_ASSERT(cursors != nullptr);

    double ttl = triagens::basics::JsonHelper::getNumericValue<double>(options.json(), "ttl", 30);
    bool count = triagens::basics::JsonHelper::getBooleanValue(options.json(), "count", false);
    
    // steal the query JSON, cursor will take over the ownership
    auto j = queryResult.json;
    triagens::arango::JsonCursor* cursor = cursors->createFromJson(j, batchSize, extra.steal(), ttl, count, queryResult.cached); 
    queryResult.json = nullptr;

    try {
      _response->body().appendChar('{');
      cursor->dump(_response->body());
      _response->body().appendText(",\"error\":false,\"code\":");
      _response->body().appendInteger(static_cast<uint32_t>(_response->responseCode()));
      _response->body().appendChar('}');

      cursors->release(cursor);
    }
    catch (...) {
      cursors->release(cursor);
      throw;
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief register the currently running query
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::registerQuery (triagens::aql::Query* query) {
  MUTEX_LOCKER(_queryLock);

  TRI_ASSERT(_query == nullptr);
  _query = query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the currently running query
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::unregisterQuery () {
  MUTEX_LOCKER(_queryLock);

  _query = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel the currently running query
////////////////////////////////////////////////////////////////////////////////

bool RestCursorHandler::cancelQuery () {
  MUTEX_LOCKER(_queryLock);

  if (_query != nullptr) {
    _query->killed(true);
    _queryKilled = true;
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query was canceled
////////////////////////////////////////////////////////////////////////////////

bool RestCursorHandler::wasCanceled () {
  MUTEX_LOCKER(_queryLock);
  return _queryKilled;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build options for the query as JSON
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json RestCursorHandler::buildOptions (TRI_json_t const* json) const {
  auto getAttribute = [&json] (char const* name) {
    return TRI_LookupObjectJson(json, name);
  };

  triagens::basics::Json options(triagens::basics::Json::Object, 3);

  auto attribute = getAttribute("count");
  options.set("count", triagens::basics::Json(TRI_IsBooleanJson(attribute) ? attribute->_value._boolean : false));

  attribute = getAttribute("batchSize");
  options.set("batchSize", triagens::basics::Json(TRI_IsNumberJson(attribute) ? attribute->_value._number : 1000.0));

  if (TRI_IsNumberJson(attribute) && static_cast<size_t>(attribute->_value._number) == 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TYPE_ERROR, "expecting non-zero value for <batchSize>");
  }

  attribute = getAttribute("cache");
  if (TRI_IsBooleanJson(attribute)) {
    options.set("cache", triagens::basics::Json(attribute->_value._boolean));
  }

  attribute = getAttribute("options");

  if (TRI_IsObjectJson(attribute)) {
    size_t const n = TRI_LengthVector(&attribute->_value._objects);

    for (size_t i = 0; i < n; i += 2) {
      auto key   = static_cast<TRI_json_t const*>(TRI_AtVector(&attribute->_value._objects, i));
      auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&attribute->_value._objects, i + 1));

      if (! TRI_IsStringJson(key) || value == nullptr) {
        continue;
      }

      auto keyName = key->_value._string.data;

      if (strcmp(keyName, "count") != 0 && 
          strcmp(keyName, "batchSize") != 0) { 

        if (strcmp(keyName, "cache") == 0 && options.has("cache")) {
          continue;
        }

        options.set(keyName, triagens::basics::Json(
          TRI_UNKNOWN_MEM_ZONE, 
          TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, value),
          triagens::basics::Json::NOFREE
        ));
      }
    }
  }

  if (! options.has("ttl")) {
    attribute = getAttribute("ttl");
    options.set("ttl", triagens::basics::Json(TRI_IsNumberJson(attribute) ? attribute->_value._number : 30.0));
  }

  return options;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the "extra" attribute values from the result.
/// note that the "extra" object will take ownership from the result for 
/// several values
////////////////////////////////////////////////////////////////////////////////
      
triagens::basics::Json RestCursorHandler::buildExtra (triagens::aql::QueryResult& queryResult) const {
  // build "extra" attribute
  triagens::basics::Json extra(triagens::basics::Json::Object); 
 
  if (queryResult.stats != nullptr) {
    extra.set("stats", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, queryResult.stats, triagens::basics::Json::AUTOFREE));
    queryResult.stats = nullptr;
  }
  if (queryResult.profile != nullptr) {
    extra.set("profile", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, queryResult.profile, triagens::basics::Json::AUTOFREE));
    queryResult.profile = nullptr;
  }
  if (queryResult.warnings == nullptr) {
    extra.set("warnings", triagens::basics::Json(triagens::basics::Json::Array));
  }
  else {
    extra.set("warnings", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, queryResult.warnings, triagens::basics::Json::AUTOFREE));
    queryResult.warnings = nullptr;
  }

  return extra;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_cursor
/// @brief create a cursor and return the first results
///
/// @RESTHEADER{POST /_api/cursor, Create cursor}
///
/// A JSON object describing the query and query parameters.
///
/// @RESTBODYPARAM{query,string,required,string}
/// contains the query string to be executed
///
/// @RESTBODYPARAM{count,boolean,optional,}
/// indicates whether the number of documents in the result set should be returned in
/// the "count" attribute of the result.
/// Calculating the "count" attribute might in the future have a performance
/// impact for some queries so this option is turned off by default, and "count"
/// is only returned when requested.
///
/// @RESTBODYPARAM{batchSize,integer,optional,int64}
/// maximum number of result documents to be transferred from
/// the server to the client in one roundtrip. If this attribute is
/// not set, a server-controlled default value will be used. A *batchSize* value of
/// *0* is disallowed.
///
/// @RESTBODYPARAM{ttl,integer,optional,int64}
/// The time-to-live for the cursor (in seconds). The cursor will be
/// removed on the server automatically after the specified amount of time. This
/// is useful to ensure garbage collection of cursors that are not fully fetched
/// by clients. If not set, a server-defined value will be used.
///
/// @RESTBODYPARAM{cache,boolean,optional,}
/// flag to determine whether the AQL query cache
/// shall be used. If set to *false*, then any query cache lookup will be skipped
/// for the query. If set to *true*, it will lead to the query cache being checked
/// for the query if the query cache mode is either *on* or *demand*.
///
/// @RESTBODYPARAM{bindVars,array,optional,object}
/// list of bind parameter objects.
///
/// @RESTBODYPARAM{options,object,optional,JSF_post_api_cursor_opts}
/// key/value object with extra options for the query.
///
/// @RESTSTRUCT{fullCount,JSF_post_api_cursor_opts,boolean,optional,}
/// if set to *true* and the query contains a *LIMIT* clause, then the
/// result will contain an extra attribute *extra* with a sub-attribute *fullCount*.
/// This sub-attribute will contain the number of documents in the result before the
/// last LIMIT in the query was applied. It can be used to count the number of documents that
/// match certain filter criteria, but only return a subset of them, in one go.
/// It is thus similar to MySQL's *SQL_CALC_FOUND_ROWS* hint. Note that setting the option
/// will disable a few LIMIT optimizations and may lead to more documents being processed,
/// and thus make queries run longer. Note that the *fullCount* sub-attribute will only
/// be present in the result if the query has a LIMIT clause and the LIMIT clause is
/// actually used in the query.
///
/// @RESTSTRUCT{maxPlans,JSF_post_api_cursor_opts,integer,optional,int64}
/// limits the maximum number of plans that are created by the AQL query optimizer.
///
/// @RESTSTRUCT{optimizer.rules,JSF_post_api_cursor_opts,array,optional,string}
/// a list of to-be-included or to-be-excluded optimizer rules
/// can be put into this attribute, telling the optimizer to include or exclude
/// specific rules. To disable a rule, prefix its name with a `-`, to enable a rule, prefix it
/// with a `+`. There is also a pseudo-rule `all`, which will match all optimizer rules.
///
/// @RESTSTRUCT{profile,JSF_post_api_cursor_opts,boolean,optional,}
/// if set to *true*, then the additional query profiling information
/// will be returned in the *extra.stats* return attribute if the query result is not
/// served from the query cache.
///
/// @RESTDESCRIPTION
/// The query details include the query string plus optional query options and
/// bind parameters. These values need to be passed in a JSON representation in
/// the body of the POST request.
///
/// If the result set can be created by the server, the server will respond with
/// *HTTP 201*. The body of the response will contain a JSON object with the
/// result set.
///
/// The returned JSON object has the following attributes:
///
/// - *error*: boolean flag to indicate that an error occurred (*false*
///   in this case)
///
/// - *code*: the HTTP status code
///
/// - *result*: an array of result documents (might be empty if query has no results)
///
/// - *hasMore*: a boolean indicator whether there are more results
///   available for the cursor on the server
///
/// - *count*: the total number of result documents available (only
///   available if the query was executed with the *count* attribute set)
///
/// - *id*: id of temporary cursor created on the server (optional, see above)
///
/// - *extra*: an optional JSON object with extra information about the query result
///   contained in its *stats* sub-attribute. For data-modification queries, the 
///   *extra.stats* sub-attribute will contain the number of modified documents and 
///   the number of documents that could not be modified
///   due to an error (if *ignoreErrors* query option is specified)
///
/// - *cached*: a boolean flag indicating whether the query result was served 
///   from the query cache or not. If the query result is served from the query
///   cache, the *extra* return attribute will not contain any *stats* sub-attribute
///   and no *profile* sub-attribute.
///
/// If the JSON representation is malformed or the query specification is
/// missing from the request, the server will respond with *HTTP 400*.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - *error*: boolean flag to indicate that an error occurred (*true* in this case)
///
/// - *code*: the HTTP status code
///
/// - *errorNum*: the server error number
///
/// - *errorMessage*: a descriptive error message
///
/// If the query specification is complete, the server will process the query. If an
/// error occurs during query processing, the server will respond with *HTTP 400*.
/// Again, the body of the response will contain details about the error.
///
/// A list of query errors can be found (../ArangoErrors/README.md) here.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the result set can be created by the server.
///
/// @RESTRETURNCODE{400}
/// is returned if the JSON representation is malformed or the query specification is
/// missing from the request.
///
/// @RESTRETURNCODE{404}
/// The server will respond with *HTTP 404* in case a non-existing collection is
/// accessed in the query.
///
/// @RESTRETURNCODE{405}
/// The server will respond with *HTTP 405* if an unsupported HTTP method is used.
///
/// @EXAMPLES
///
/// Execute a query and extract the result in a single go
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorForLimitReturnSingle}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     db.products.save({"hello1":"world1"});
///     db.products.save({"hello2":"world1"});
///
///     var url = "/_api/cursor";
///     var body = {
///       query: "FOR p IN products LIMIT 2 RETURN p",
///       count: true,
///       batchSize: 2
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Execute a query and extract a part of the result
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorForLimitReturn}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     db.products.save({"hello1":"world1"});
///     db.products.save({"hello2":"world1"});
///     db.products.save({"hello3":"world1"});
///     db.products.save({"hello4":"world1"});
///     db.products.save({"hello5":"world1"});
///
///     var url = "/_api/cursor";
///     var body = {
///       query: "FOR p IN products LIMIT 5 RETURN p",
///       count: true,
///       batchSize: 2
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using the query option "fullCount"
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorOption}
///     var url = "/_api/cursor";
///     var body = {
///       query: "FOR i IN 1..1000 FILTER i > 500 LIMIT 10 RETURN i",
///       count: true,
///       options: {
///         fullCount: true
///       }
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Enabling and disabling optimizer rules
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorOptimizerRules}
///     var url = "/_api/cursor";
///     var body = {
///       query: "FOR i IN 1..10 LET a = 1 LET b = 2 FILTER a + b == 3 RETURN i",
///       count: true,
///       options: {
///         maxPlans: 1,
///         optimizer: {
///           rules: [ "-all", "+remove-unnecessary-filters" ]
///         }
///       }
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Execute a data-modification query and retrieve the number of
/// modified documents
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorDeleteQuery}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     db.products.save({"hello1":"world1"});
///     db.products.save({"hello2":"world1"});
///
///     var url = "/_api/cursor";
///     var body = {
///       query: "FOR p IN products REMOVE p IN products"
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///     assert(JSON.parse(response.body).extra.stats.writesExecuted === 2);
///     assert(JSON.parse(response.body).extra.stats.writesIgnored === 0);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Execute a data-modification query with option *ignoreErrors*
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorDeleteIgnore}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     db.products.save({ _key: "foo" });
///
///     var url = "/_api/cursor";
///     var body = {
///       query: "REMOVE 'bar' IN products OPTIONS { ignoreErrors: true }"
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///     assert(JSON.parse(response.body).extra.stats.writesExecuted === 0);
///     assert(JSON.parse(response.body).extra.stats.writesIgnored === 1);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Bad query - Missing body
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorMissingBody}
///     var url = "/_api/cursor";
///
///     var response = logCurlRequest('POST', url, '');
///
///     assert(response.code === 400);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Bad query - Unknown collection
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorCreateCursorUnknownCollection}
///     var url = "/_api/cursor";
///     var body = {
///       query: "FOR u IN unknowncoll LIMIT 2 RETURN u",
///       count: true,
///       batchSize: 2
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Bad query - Execute a data-modification query that attempts to remove a non-existing
/// document
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorDeleteQueryFail}
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     db.products.save({ _key: "bar" });
///
///     var url = "/_api/cursor";
///     var body = {
///       query: "REMOVE 'foo' IN products"
///     };
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::createCursor () {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/cursor");
    return;
  }

  try { 
    std::unique_ptr<TRI_json_t> json(parseJsonBody());

    if (json.get() == nullptr) {
      return;
    }

    processQuery(json.get());
  }  
  catch (triagens::basics::Exception const& ex) {
    unregisterQuery(); 
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  }
  catch (std::bad_alloc const&) {
    unregisterQuery(); 
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
  }
  catch (std::exception const& ex) {
    unregisterQuery(); 
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL, ex.what());
  }
  catch (...) {
    unregisterQuery(); 
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_cursor_identifier
/// @brief return the next results from an existing cursor
///
/// @RESTHEADER{PUT /_api/cursor/{cursor-identifier}, Read next batch from cursor}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{cursor-identifier,string,required}
/// The name of the cursor
///
/// @RESTDESCRIPTION
///
/// If the cursor is still alive, returns an object with the following
/// attributes:
///
/// - *id*: the *cursor-identifier*
/// - *result*: a list of documents for the current batch
/// - *hasMore*: *false* if this was the last batch
/// - *count*: if present the total number of elements
///
/// Note that even if *hasMore* returns *true*, the next call might
/// still return no documents. If, however, *hasMore* is *false*, then
/// the cursor is exhausted.  Once the *hasMore* attribute has a value of
/// *false*, the client can stop.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// The server will respond with *HTTP 200* in case of success.
///
/// @RESTRETURNCODE{400}
/// If the cursor identifier is omitted, the server will respond with *HTTP 404*.
///
/// @RESTRETURNCODE{404}
/// If no cursor with the specified identifier can be found, the server will respond
/// with *HTTP 404*.
///
/// @EXAMPLES
///
/// Valid request for next batch
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorForLimitReturnCont}
///     var url = "/_api/cursor";
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     db.products.save({"hello1":"world1"});
///     db.products.save({"hello2":"world1"});
///     db.products.save({"hello3":"world1"});
///     db.products.save({"hello4":"world1"});
///     db.products.save({"hello5":"world1"});
///
///     var url = "/_api/cursor";
///     var body = {
///       query: "FOR p IN products LIMIT 5 RETURN p",
///       count: true,
///       batchSize: 2
///     };
///     var response = logCurlRequest('POST', url, body);
///
///     var body = response.body.replace(/\\/g, '');
///     var _id = JSON.parse(body).id;
///     response = logCurlRequest('PUT', url + '/' + _id, '');
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Missing identifier
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorMissingCursorIdentifier}
///     var url = "/_api/cursor";
///
///     var response = logCurlRequest('PUT', url, '');
///
///     assert(response.code === 400);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Unknown identifier
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorInvalidCursorIdentifier}
///     var url = "/_api/cursor/123123";
///
///     var response = logCurlRequest('PUT', url, '');
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::modifyCursor () {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/cursor/<cursor-id>");
    return;
  }
  
  std::string const& id = suffix[0];

  auto cursors = static_cast<triagens::arango::CursorRepository*>(_vocbase->_cursorRepository);
  TRI_ASSERT(cursors != nullptr);

  auto cursorId = static_cast<triagens::arango::CursorId>(triagens::basics::StringUtils::uint64(id));
  bool busy;
  auto cursor = cursors->find(cursorId, busy);

  if (cursor == nullptr) {
    if (busy) {
      generateError(HttpResponse::responseCode(TRI_ERROR_CURSOR_BUSY), TRI_ERROR_CURSOR_BUSY);
    }
    else {
      generateError(HttpResponse::responseCode(TRI_ERROR_CURSOR_NOT_FOUND), TRI_ERROR_CURSOR_NOT_FOUND);
    }
    return;
  }

  try {
    _response = createResponse(HttpResponse::OK);
    _response->setContentType("application/json; charset=utf-8");

    _response->body().appendChar('{');
    cursor->dump(_response->body());
    _response->body().appendText(",\"error\":false,\"code\":");
    _response->body().appendInteger(static_cast<uint32_t>(_response->responseCode()));
    _response->body().appendChar('}');

    cursors->release(cursor);
  }
  catch (triagens::basics::Exception const& ex) {
    cursors->release(cursor);

    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  }
  catch (...) {
    cursors->release(cursor);

    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_cursor_delete
/// @brief dispose an existing cursor
///
/// @RESTHEADER{DELETE /_api/cursor/{cursor-identifier}, Delete cursor}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{cursor-identifier,string,required}
/// The id of the cursor
///
/// @RESTDESCRIPTION
/// Deletes the cursor and frees the resources associated with it.
///
/// The cursor will automatically be destroyed on the server when the client has
/// retrieved all documents from it. The client can also explicitly destroy the
/// cursor at any earlier time using an HTTP DELETE request. The cursor id must
/// be included as part of the URL.
///
/// Note: the server will also destroy abandoned cursors automatically after a
/// certain server-controlled timeout to avoid resource leakage.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{202}
/// is returned if the server is aware of the cursor.
///
/// @RESTRETURNCODE{404}
/// is returned if the server is not aware of the cursor. It is also
/// returned if a cursor is used after it has been destroyed.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestCursorDelete}
///     var url = "/_api/cursor";
///     var cn = "products";
///     db._drop(cn);
///     db._create(cn);
///
///     db.products.save({"hello1":"world1"});
///     db.products.save({"hello2":"world1"});
///     db.products.save({"hello3":"world1"});
///     db.products.save({"hello4":"world1"});
///     db.products.save({"hello5":"world1"});
///
///     var url = "/_api/cursor";
///     var body = {
///       query: "FOR p IN products LIMIT 5 RETURN p",
///       count: true,
///       batchSize: 2
///     };
///     var response = logCurlRequest('POST', url, body);
///     logJsonResponse(response);
///     var body = response.body.replace(/\\/g, '');
///     var _id = JSON.parse(body).id;
///     response = logCurlRequest('DELETE', url + '/' + _id);
///
///     assert(response.code === 202);
///   ~ db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestCursorHandler::deleteCursor () {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/cursor/<cursor-id>");
    return;
  }
  
  std::string const& id = suffix[0];

  auto cursors = static_cast<triagens::arango::CursorRepository*>(_vocbase->_cursorRepository);
  TRI_ASSERT(cursors != nullptr);
 
  auto cursorId = static_cast<triagens::arango::CursorId>(triagens::basics::StringUtils::uint64(id));
  bool found = cursors->remove(cursorId);

  if (! found) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }

  _response = createResponse(HttpResponse::ACCEPTED);
  _response->setContentType("application/json; charset=utf-8");
   
  triagens::basics::Json json(triagens::basics::Json::Object);
  json.set("id", triagens::basics::Json(id)); // id as a string! 
  json.set("error", triagens::basics::Json(false)); 
  json.set("code", triagens::basics::Json(static_cast<double>(_response->responseCode())));

  json.dump(_response->body());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
