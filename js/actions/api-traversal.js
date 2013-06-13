/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, evil: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief traversal actions
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var db = require("internal").db;
var traversal = require("org/arangodb/graph/traversal");
var Traverser = traversal.Traverser;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a "bad parameter" error
////////////////////////////////////////////////////////////////////////////////
    
function badParam (req, res, message) {
  actions.resultBad(req, 
                    res, 
                    arangodb.ERROR_HTTP_BAD_PARAMETER, 
                    message);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a "not found" error
////////////////////////////////////////////////////////////////////////////////
    
function notFound (req, res, code, message) {
  actions.resultNotFound(req, 
                         res, 
                         code,
                         message);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate and translate the argument given in value
////////////////////////////////////////////////////////////////////////////////
  
function validateArg (value, map) {
  if (value === null || value === undefined) {
    var m;
    // use first key from map
    for (m in map) {
      if (map.hasOwnProperty(m)) {
        value = m;
        break;
      }
    }
  }

  if (typeof value === 'string') {
    value = value.toLowerCase().replace(/-/, "");
    if (map[value] !== null) {
      return map[value];
    }
  }

  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a server-side traversal
///
/// @RESTHEADER{POST /_api/traversal,executes a traversal}
///
/// @RESTBODYPARAM{body,string,required}
///
/// @RESTDESCRIPTION
/// Starts a traversal starting from a given vertex and following.
/// edges contained in a given edgeCollection. The request must
/// contain the following attributes.
///  
/// - `startVertex`: id of the startVertex, e.g. `"users/foo"`.
/// 
/// - `edgeCollection`: name of the collection that contains the edges.
///
/// - `filter` (optional, default is to include all nodes):
///         body (JavaScript code) of custom filter function
///         function signature: (config, vertex, path) -> mixed
///         can return four different string values:
///         - `"exclude"` -> this vertex will not be visited.
///         - `"prune"` -> the edges of this vertex will not be followed.
///         - `""` or `undefined` -> visit the vertex and follow it's edges.
///         - `Array` -> containing any combination of the above.
///             If there is at least one `"exclude"` or `"prune"` respectivly
///             is contained, it's effect will occur.
/// - `minDepth` (optional, ANDed with any existing filters): visits only nodes in at least the given depth
/// - `maxDepth` (optional, ANDed with any existing filters): visits only nodes in at most the given depth
/// - `visitor` (optional): body (JavaScript) code of custom visitor function
///          function signature: (config, result, vertex, path) -> void
///          visitor function can do anything, but its return value is ignored. To 
///          populate a result, use the `result` variable by reference
/// - `direction` (optional): direction for traversal
///            - *if set*, must be either `"outbound"`, `"inbound"`, or `"any"`
///            - *if not set*, the `expander` attribute must be specified
/// - `init` (optional): body (JavaScript) code of custom result initialisation function
///       function signature: (config, result) -> void
///       initialise any values in result with what is required
/// - `expander` (optional): body (JavaScript) code of custom expander function
///           *must* be set if `direction` attribute is *not* set
///           function signature: (config, vertex, path) -> array
///           expander must return an array of the connections for `vertex`
///           each connection is an object with the attributes `edge` and `vertex`
/// - `strategy` (optional): traversal strategy
///           can be `"depthfirst"` or `"breadthfirst"`
/// - `order` (optional): traversal order
///        can be `"preorder"` or `"postorder"`
/// - `itemOrder` (optional): item iteration order
///            can be `"forward"` or `"backward"`
/// - `uniqueness` (optional): specifies uniqueness for vertices and edges visited
///             if set, must be an object like this:
///             `"uniqueness": {"vertices": "none"|"global"|path", "edges": "none"|"global"|"path"}` 
///
///
/// If the Traversal is successfully executed `HTTP 200` will be returned.
/// Additionally the `result` object will be returned by the traversal.
/// 
/// For successful traversals, the returned JSON object has the 
/// following properties:
///
/// - `error`: boolean flag to indicate if an error occurred (`false`
///   in this case)
///
/// - `code`: the HTTP status code
///
/// - `result`: the return value of the traversal
///
/// If the traversal specification is either missing or malformed, the server
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
/// example:
/// POST {"edgeCollection": "e", "expander": "var connections = [ ]; config.edgeCollection.outEdges(vertex).forEach(function (e) { connections.push({ vertex: e._to, edge: e }); }); return connections;", "startVertex" : "v/jan", "filter": "return undefined;", "minDepth": 0 }
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the traversal is fully executed
/// `HTTP 200` will be returned.
///
/// @RESTRETURNCODE{400}
/// If the traversal specification is either missing or malformed, the server
/// will respond with `HTTP 400`.
///
/// @EXAMPLES
///
/// In the following examples the underlying graph will contain five persons
/// `Alice`, `Bob`, `Charlie`, `Dave` and `Eve`.
/// We will have the following directed relations:
///     - `Alice` knows `Bob`
///     - `Bob` knows `Charlie`
///     - `Bob` knows `Dave`
///     - `Eve` knows `Alice`
///     - `Eve` knows `Bob`
///
/// The starting vertex will always be Alice.
///
/// Follow only outbound edges:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalOutbound}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"direction" : "outbound"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Follow only inbound edges:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalInbound}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"direction" : "inbound"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Follow any direction of edges:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalAny}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"direction" : "any"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Excluding `Charlie` and `Bob`:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalFilterExclude}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"direction" : "outbound", ';
///         body += '"filter" : "if (vertex.name === \\"Bob\\" || ';
///         body += 'vertex.name === \\"Charlie\\") {';
///         body += 'return \\"exclude\\";'
///         body += '}'
///         body += 'return;"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Do not follow edges from `Bob`:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalFilterPrune}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"direction" : "outbound", ';
///         body += '"filter" : "if (vertex.name === \\"Bob\\") {';
///         body += 'return \\"prune\\";'
///         body += '}'
///         body += 'return;"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Visit only nodes in a depth of at least 2:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalMinDepth}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"direction" : "outbound", ';
///         body += '"minDepth" : 2}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Visit only nodes in a depth of at most 1:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalMaxDepth}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"direction" : "outbound", ';
///         body += '"maxDepth" : 1}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Count all visited nodes and return a list of nodes only:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalVisitorCountAndList}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"direction" : "outbound", ';
///         body += '"init" : "result.visited = 0; result.myVertices = [ ];", ';
///         body += '"visitor" : "result.visited++; result.myVertices.push(vertex);"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Expand only inbound edges of `Alice` and outbound edges of `Eve`:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalVisitorExpander}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"expander" : ';
///         body += '"var connections = [ ];';
///         body += 'if (vertex.name === \\"Alice\\") {';
///         body += 'config.edgeCollection.inEdges(vertex).forEach(function (e) {';
///         body += 'connections.push({ vertex: require(\\"internal\\").db._document(e._from), edge: e });';
///         body += '});';
///         body += '}';
///         body += 'if (vertex.name === \\"Eve\\") {';
///         body += 'config.edgeCollection.outEdges(vertex).forEach(function (e) {';
///         body += 'connections.push({ vertex: require(\\"internal\\").db._document(e._to), edge: e });';
///         body += '});';
///         body += '}';
///         body += 'return connections;"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Follow the `depthfirst` strategy:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalDepthFirst}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"direction" : "any", ';
///         body += '"strategy" : "depthfirst"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using `postorder` ordering:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalPostorder}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"direction" : "any", ';
///         body += '"order" : "postorder"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using `backward` item-ordering:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalBackwardItemOrder}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"direction" : "any", ';
///         body += '"itemOrder" : "backward"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Edges should only be included once globally,
/// but nodes are included every time they are visited:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalEdgeUniqueness}
///     var cv = "persons";
///     var ce = "knows";
///     db._drop(cv);
///     db._drop(ce);
///     var users = db._create(cv);
///     var knows = db._createEdgeCollection(ce);
///     var a = users.save({name: "Alice"})._id;
///     var b = users.save({name: "Bob"})._id;
///     var c = users.save({name: "Charlie"})._id;
///     var d = users.save({name: "Dave"})._id;
///     var e = users.save({name: "Eve"})._id;
///     knows.save(a, b, {});
///     knows.save(b, c, {});
///     knows.save(b, d, {});
///     knows.save(e, a, {});
///     knows.save(e, b, {});
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"edgeCollection" : "' + knows.name() + '", ';
///         body += '"direction" : "any", ';
///         body += '"uniqueness" : {"vertices": "none", "edges": "global"}}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cv);
///     db._drop(ce);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function post_api_traversal(req, res) {
  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    return badParam(req, res);
  }

  // check start vertex
  // -----------------------------------------

  if (json.startVertex === undefined || 
      typeof json.startVertex !== "string") {
    return badParam(req, res, "missing or invalid startVertex");
  }

  var doc;
  try {
    doc = db._document(json.startVertex);
  }
  catch (err) {
    return notFound(req, res, arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND, "invalid startVertex");
  }
  
  // check edge collection
  // -----------------------------------------

  if (json.edgeCollection === undefined || 
      typeof json.edgeCollection !== "string") {
    return badParam(req, res, "missing or invalid edgeCollection");
  }

  var edgeCollection;
  try {
    edgeCollection = db._collection(json.edgeCollection);
  }
  catch (err2) {
  }

  if (edgeCollection === undefined || edgeCollection == null) {
    return notFound(req, res, arangodb.ERROR_ARANGO_COLLECTION_NOT_FOUND, "invalid edgeCollection");
  }
  
  // set up filters
  // -----------------------------------------
  
  var filters = [ ];
  if (json.minDepth !== undefined) {
    filters.push(traversal.minDepthFilter);
  }
  if (json.maxDepth !== undefined) {
    filters.push(traversal.maxDepthFilter);
  }
  if (json.filter) {
    try {
      filters.push(new Function('config', 'vertex', 'path', json.filter));
    }
    catch (err3) {
      return badParam(req, res, "invalid filter function");
    }
  }

  // if no filter given, use the default filter (does nothing)
  if (filters.length === 0) {
    filters.push(traversal.visitAllFilter);
  }
  
  // set up visitor
  // -----------------------------------------

  var visitor;

  if (json.visitor !== undefined) {
    try {
      visitor = new Function('config', 'result', 'vertex', 'path', json.visitor);
    }
    catch (err4) {
      return badParam(req, res, "invalid visitor function");
    }
  }
  else {
    visitor = traversal.trackingVisitor;
  }

  // set up expander
  // -----------------------------------------

  var expander;

  if (json.direction !== undefined) {
    expander = validateArg(json.direction, {
      'outbound': traversal.outboundExpander,
      'inbound': traversal.inboundExpander,
      'any': traversal.anyExpander
    });
  }
  else if (json.expander !== undefined) {
    try {
      expander = new Function('config', 'vertex', 'path', json.expander);
    }
    catch (err5) {
      return badParam(req, res, "invalid expander function");
    }
  }

  if (expander === undefined) {
    return badParam(req, res, "missing or invalid expander");
  }

  
  // assemble config object
  // -----------------------------------------

  var config = {
    params: json,
    edgeCollection: edgeCollection,
    datasource: traversal.collectionDatasourceFactory(edgeCollection),
    strategy: validateArg(json.strategy, {
      'depthfirst': Traverser.DEPTH_FIRST,
      'breadthfirst': Traverser.BREADTH_FIRST
    }),
    order: validateArg(json.order, {
      'preorder': Traverser.PRE_ORDER,
      'postorder': Traverser.POST_ORDER
    }),
    itemOrder: validateArg(json.itemOrder, {
      'forward': Traverser.FORWARD,
      'backward': Traverser.BACKWARD
    }),
    expander: expander,
    visitor: visitor,
    filter: filters,
    minDepth: json.minDepth || 0,
    maxDepth: json.maxDepth,
    uniqueness: {
      vertices: validateArg(json.uniqueness && json.uniqueness.vertices, {
        'global': Traverser.UNIQUE_GLOBAL,
        'none': Traverser.UNIQUE_NONE,
        'path': Traverser.UNIQUE_PATH
      }),
      edges: validateArg(json.uniqueness && json.uniqueness.edges, {
        'global': Traverser.UNIQUE_GLOBAL,
        'none': Traverser.UNIQUE_NONE,
        'path': Traverser.UNIQUE_PATH
      })
    }
  };
  
  // assemble result object
  // -----------------------------------------

  var result = { 
    visited: {
      vertices: [ ],
      paths: [ ]
    }
  };

  if (json.init !== undefined) {
    try {
      var init = new Function('result', json.init);
      init(result);
    } 
    catch (err6) {
      return badParam(req, res, "invalid init function");
    }
  }
  
  // run the traversal
  // -----------------------------------------

  var traverser = new Traverser(config);
  try {
    traverser.traverse(result, doc);
    actions.resultOk(req, res, actions.HTTP_OK, { result : result });
  }
  catch (err7) {
    actions.resultException(req, res, err7, undefined, false);
  }
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
  url : "_api/traversal",
  context : "api",

  callback : function (req, res) {
    try {
      switch (req.requestType) {
        case actions.POST: 
          post_api_traversal(req, res); 
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
