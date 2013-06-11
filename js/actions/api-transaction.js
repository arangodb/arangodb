/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, TRANSACTION */

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a server-side transaction
///
/// @RESTHEADER{POST /_api/transaction,executes a transaction}
///
/// @RESTBODYPARAM{body,string,required}
/// Contains the `collections` and `action`.
///
/// @RESTDESCRIPTION
///
/// The transaction description must be passed in the body of the POST request.
///
/// The following attributes must be specified inside the JSON object:
///
/// - `collections`: contains the list of collections to be used in the
///   transaction (mandatory). `collections` must be a JSON array that can
///   have the optional sub-attributes `read` and `write`. `read`
///   and `write` must each be either lists of collections names or strings
///   with a single collection name. 
///
/// - `action`: the actual transaction operations to be executed, in the
///   form of stringified Javascript code. The code will be executed on server
///   side, with late binding. It is thus critical that the code specified in
///   `action` properly sets up all the variables it needs. 
///   If the code specified in `action` ends with a return statement, the
///   value returned will also be returned by the REST API in the `result`
///   attribute if the transaction committed successfully.
///
/// The following optional attributes may also be specified in the request:
///
/// - `waitForSync`: an optional boolean flag that, if set, will force the 
///   transaction to write all data to disk before returning.
///
/// - `lockTimeout`: an optional numeric value that can be used to set a
///   timeout for waiting on collection locks. If not specified, a default 
///   value will be used. Setting `lockTimeout` to `0` will make ArangoDB 
///   not time out waiting for a lock.
///
/// - `params`: optional arguments passed to `action`.
///
/// If the transaction is fully executed and committed on the server, 
/// `HTTP 200` will be returned. Additionally, the return value of the 
/// code defined in `action` will be returned in the `result` attribute.
/// 
/// For successfully committed transactions, the returned JSON object has the 
/// following properties:
///
/// - `error`: boolean flag to indicate if an error occurred (`false`
///   in this case)
///
/// - `code`: the HTTP status code
///
/// - `result`: the return value of the transaction
///
/// If the transaction specification is either missing or malformed, the server
/// will respond with `HTTP 400`.
///
/// The body of the response will then contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - `error`: boolean flag to indicate that an error occurred (`true` in this case)
///
/// - `code`: the HTTP status code
///
/// - `errorNum`: the server error number
///
/// - `errorMessage`: a descriptive error message
///
/// If a transaction fails to commit, either by an exception thrown in the 
/// `action` code, or by an internal error, the server will respond with 
/// an error. 
/// Any other errors will be returned with any of the return codes
/// `HTTP 400`, `HTTP 409`, or `HTTP 500`.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the transaction is fully executed and committed on the server, 
/// `HTTP 200` will be returned.
///
/// @RESTRETURNCODE{400}
/// If the transaction specification is either missing or malformed, the server
/// will respond with `HTTP 400`.
///
/// @RESTRETURNCODE{500}
/// Exceptions thrown by users will make the server respond with a return code of 
/// `HTTP 500` 
///
/// @EXAMPLES
///
/// Executing a transaction on a single collection:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTransactionSingle}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn);
///     var url = "/_api/transaction";
///     var body = '{ "collections": { "write" : "products" }, ';
///         body += '"action" : "function () { ';
///         body += 'var db = require(\\"internal\\").db;';
///         body += 'db.products.save({});';
///         body += 'return db.products.count(); }" }';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Executing a transaction using multiple collections:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTransactionMulti}
///     var cn1 = "materials";
///     db._drop(cn1);
///     var materials = db._create(cn1);
///     var cn2 = "products";
///     db._drop(cn2);
///     var products = db._create(cn2);
///     products.save({ "a": 1});
///     materials.save({"b": 1});
///     var url = "/_api/transaction";
///     var body = '{ "collections": { "write" : ["products", ';
///         body += '"materials" ] }, ';
///         body += '"action" : "function () { ';
///         body += 'var db = require(\\"internal\\").db;';
///         body += 'db.products.save({});';
///         body += 'db.materials.save({});';
///         body += 'return \\"worked!\\"; }" }';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn1);
///     db._drop(cn2);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Aborting a transaction due to an internal error:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTransactionAbortInternal}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn);
///     var url = "/_api/transaction";
///     var body = '{ "collections": { "write" : "products" }, ';
///         body += '"action" : "function () { ';
///         body += 'var db = require(\\"internal\\").db;';
///         body += 'db.products.save({_key: \\"abc\\"});';
///         body += 'db.products.save({_key: \\"abc\\"});';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 400);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Aborting a transaction by throwing an exception:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTransactionAbort}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": 1});
///     var url = "/_api/transaction";
///     var body = '{ "collections": { "read" : "products" }, ';
///         body += '"action" : "function () { throw \\"doh!\\"; }" }';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 500);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function post_api_transaction(req, res) {
  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var result = TRANSACTION(json);

  actions.resultOk(req, res, actions.HTTP_OK, { "result" : result });
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/transaction",
  context : "api",

  callback : function (req, res) {
    try {
      switch (req.requestType) {
        case actions.POST: 
          post_api_transaction(req, res); 
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    }
    catch (err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
