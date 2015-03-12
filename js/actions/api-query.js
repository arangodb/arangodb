/*jshint strict: false */
/*global require, AQL_PARSE, AQL_QUERIES_CURRENT, AQL_QUERIES_SLOW, 
  AQL_QUERIES_PROPERTIES, AQL_QUERIES_KILL */

////////////////////////////////////////////////////////////////////////////////
/// @brief query actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var ArangoError = arangodb.ArangoError;

// -----------------------------------------------------------------------------
// --SECTION--                                                      HTTP methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock GetApiQueryCurrent
/// @brief returns a list of currently running AQL queries
///
/// @RESTHEADER{GET /_api/query/current, Returns the currently running AQL queries}
///
/// Returns an array containing the AQL queries currently running in the selected
/// database. Each query is a JSON object with the following attributes:
///
/// - *id*: the query's id
///
/// - *query*: the query string (potentially truncated)
///
/// - *started*: the date and time when the query was started
///
/// - *runTime*: the query's run time up to the point the list of queries was
///   queried
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned when the list of queries can be retrieved successfully.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request,
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock GetApiQuerySlow
/// @brief returns a list of slow running AQL queries
///
/// @RESTHEADER{GET /_api/query/slow, Returns the list of slow AQL queries}
///
/// Returns an array containing the last AQL queries that exceeded the slow 
/// query threshold in the selected database. 
/// The maximum amount of queries in the list can be controlled by setting
/// the query tracking property `maxSlowQueries`. The threshold for treating
/// a query as *slow* can be adjusted by setting the query tracking property
/// `slowQueryThreshold`.
///
/// Each query is a JSON object with the following attributes:
///
/// - *id*: the query's id
///
/// - *query*: the query string (potentially truncated)
///
/// - *started*: the date and time when the query was started
///
/// - *runTime*: the query's run time up to the point the list of queries was
///   queried
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned when the list of queries can be retrieved successfully.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request,
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock GetApiQueryProperties
/// @brief returns the configuration for the AQL query tracking
///
/// @RESTHEADER{GET /_api/query/properties, Returns the properties for the AQL query tracking}
///
/// Returns the current query tracking configuration. The configuration is a
/// JSON object with the following properties:
/// 
/// - *enabled*: if set to *true*, then queries will be tracked. If set to 
///   *false*, neither queries nor slow queries will be tracked.
///
/// - *trackSlowQueries*: if set to *true*, then slow queries will be tracked
///   in the list of slow queries if their runtime exceeds the value set in 
///   *slowQueryThreshold*. In order for slow queries to be tracked, the *enabled*
///   property must also be set to *true*.
/// 
/// - *maxSlowQueries*: the maximum number of slow queries to keep in the list
///   of slow queries. If the list of slow queries is full, the oldest entry in
///   it will be discarded when additional slow queries occur.
///
/// - *slowQueryThreshold*: the threshold value for treating a query as slow. A
///   query with a runtime greater or equal to this threshold value will be
///   put into the list of slow queries when slow query tracking is enabled.
///   The value for *slowQueryThreshold* is specified in seconds.
///
/// - *maxQueryStringLength*: the maximum query string length to keep in the
///   list of queries. Query strings can have arbitrary lengths, and this property
///   can be used to save memory in case very long query strings are used. The
///   value is specified in bytes.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned when the list of queries can be retrieved successfully.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request,
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function get_api_query (req, res) {
  var suffixes = [ "slow", "current", "properties" ];

  if (req.suffix.length !== 1 ||
      suffixes.indexOf(req.suffix[0]) === -1) {
    actions.resultNotFound(req,
                           res,
                           arangodb.ERROR_HTTP_NOT_FOUND,
                           arangodb.errors.ERROR_HTTP_NOT_FOUND.message);
    return;
  }

  var result;
  if (req.suffix[0] === "slow") {
    result = AQL_QUERIES_SLOW();
  }
  else if (req.suffix[0] === "current") {
    result = AQL_QUERIES_CURRENT();
  }
  else if (req.suffix[0] === "properties") {
    result = AQL_QUERIES_PROPERTIES();
  }

  if (result instanceof ArangoError) {
    actions.resultBad(req, res, result.errorNum, result.errorMessage);
    return;
  }

  actions.resultOk(req, res, actions.HTTP_OK, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock DeleteApiQuerySlow
/// @brief clears the list of slow AQL queries
///
/// @RESTHEADER{DELETE /_api/query/slow, Clears the list of slow AQL queries}
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{204}
/// The server will respond with *HTTP 200* when the list of queries was
/// cleared successfully.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock DeleteApiQueryKill
/// @brief kills an AQL query
///
/// @RESTHEADER{DELETE /_api/query/{query-id}, Kills a running AQL query}
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// The server will respond with *HTTP 200* when the query was still running when
/// the kill request was executed and the query's kill flag was set.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request.
///
/// @RESTRETURNCODE{404}
/// The server will respond with *HTTP 404* when no query with the specified
/// id was found.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function delete_api_query (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultNotFound(req,
                           res,
                           arangodb.ERROR_HTTP_NOT_FOUND,
                           arangodb.errors.ERROR_HTTP_NOT_FOUND.message);
    return;
  }

  if (req.suffix[0] === "slow") {
    // erase the slow log
    AQL_QUERIES_SLOW(true);
    actions.resultOk(req, res, actions.HTTP_NO_CONTENT);
  }
  else {
    // kill a single query
    AQL_QUERIES_KILL(req.suffix[0]);
    actions.resultOk(req, res, actions.HTTP_OK);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock PutApiQueryProperties
/// @brief changes the configuration for the AQL query tracking
///
/// @RESTHEADER{PUT /_api/query/properties, Changes the properties for the AQL query tracking}
///
/// @RESTBODYPARAM{properties,json,required}
/// The properties for query tracking in the current database. 
///
/// The properties need to be passed in the attribute *properties* in the body
/// of the HTTP request. *properties* needs to be a JSON object with the following
/// properties:
/// 
/// - *enabled*: if set to *true*, then queries will be tracked. If set to 
///   *false*, neither queries nor slow queries will be tracked.
///
/// - *trackSlowQueries*: if set to *true*, then slow queries will be tracked
///   in the list of slow queries if their runtime exceeds the value set in 
///   *slowQueryThreshold*. In order for slow queries to be tracked, the *enabled*
///   property must also be set to *true*.
/// 
/// - *maxSlowQueries*: the maximum number of slow queries to keep in the list
///   of slow queries. If the list of slow queries is full, the oldest entry in
///   it will be discarded when additional slow queries occur.
///
/// - *slowQueryThreshold*: the threshold value for treating a query as slow. A
///   query with a runtime greater or equal to this threshold value will be
///   put into the list of slow queries when slow query tracking is enabled.
///   The value for *slowQueryThreshold* is specified in seconds.
///
/// - *maxQueryStringLength*: the maximum query string length to keep in the
///   list of queries. Query strings can have arbitrary lengths, and this property
///   can be used to save memory in case very long query strings are used. The
///   value is specified in bytes.
///
/// After the properties have been changed, the current set of properties will
/// be returned in the HTTP response.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the properties were changed successfully.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request,
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function put_api_query (req, res) {

  if (req.suffix.length !== 1 ||
      req.suffix[0] !== "properties") {
    actions.resultNotFound(req,
                           res,
                           arangodb.ERROR_HTTP_NOT_FOUND,
                           arangodb.errors.ERROR_HTTP_NOT_FOUND.message);
    return;
  }

  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    return;
  }

  var result = AQL_QUERIES_PROPERTIES(json);

  if (result instanceof ArangoError) {
    actions.resultBad(req, res, result.errorNum, result.errorMessage);
    return;
  }

  actions.resultOk(req, res, actions.HTTP_OK, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_query
/// @brief parse an AQL query and return information about it
///
/// @RESTHEADER{POST /_api/query, Parse an AQL query}
///
/// @RESTBODYPARAM{query,json,required}
/// To validate a query string without executing it, the query string can be
/// passed to the server via an HTTP POST request.
///
/// The query string needs to be passed in the attribute *query* of a JSON
/// object as the body of the POST request.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the query is valid, the server will respond with *HTTP 200* and
/// return the names of the bind parameters it found in the query (if any) in
/// the *bindVars* attribute of the response. It will also return an array
/// of the collections used in the query in the *collections* attribute. 
/// If a query can be parsed successfully, the *ast* attribute of the returned
/// JSON will contain the abstract syntax tree representation of the query.
/// The format of the *ast* is subject to change in future versions of 
/// ArangoDB, but it can be used to inspect how ArangoDB interprets a given
/// query. Note that the abstract syntax tree will be returned without any
/// optimizations applied to it.
///
/// @RESTRETURNCODE{400}
/// The server will respond with *HTTP 400* in case of a malformed request,
/// or if the query contains a parse error. The body of the response will
/// contain the error details embedded in a JSON object.
///
/// @EXAMPLES
///
/// Valid query:
///
/// @EXAMPLE_ARANGOSH_RUN{RestQueryValid}
///     var url = "/_api/query";
///     var body = '{ "query" : "FOR p IN products FILTER p.name == @name LIMIT 2 RETURN p.n" }';
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Invalid query:
///
/// @EXAMPLE_ARANGOSH_RUN{RestQueryInvalid}
///     var url = "/_api/query";
///     var body = '{ "query" : "FOR p IN products FILTER p.name = @name LIMIT 2 RETURN p.n" }';
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 400);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function post_api_query (req, res) {
  if (req.suffix.length !== 0) {
    actions.resultNotFound(req,
                           res,
                           arangodb.ERROR_HTTP_NOT_FOUND,
                           arangodb.errors.ERROR_HTTP_NOT_FOUND.message);
    return;
  }

  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    return;
  }

  var result = AQL_PARSE(json.query);

  if (result instanceof ArangoError) {
    actions.resultBad(req, res, result.errorNum, result.errorMessage);
    return;
  }

  result = { 
    bindVars: result.parameters, 
    collections: result.collections,
    ast: result.ast
  };

  actions.resultOk(req, res, actions.HTTP_OK, result);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief query actions gateway
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_api/query",

  callback : function (req, res) {
    try {
      switch (req.requestType) {
        case actions.GET:
          get_api_query(req, res);
          break;
        
        case actions.PUT:
          put_api_query(req, res);
          break;

        case actions.POST:
          post_api_query(req, res);
          break;
        
        case actions.DELETE:
          delete_api_query(req, res);
          break;
        
        default:
          actions.resultUnsupported(req, res);
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
