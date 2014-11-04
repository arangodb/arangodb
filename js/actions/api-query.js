/*jshint strict: false */
/*global require, AQL_PARSE */

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
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

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
/// the *bindVars* attribute of the response. It will also return a list
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
        case actions.POST:
          post_api_query(req, res);
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
