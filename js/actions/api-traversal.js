/*jshint strict: false */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief traversal actions
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var db = require("internal").db;
var traversal = require("org/arangodb/graph/traversal");
var Traverser = traversal.Traverser;
var graph = require("org/arangodb/general-graph");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

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
/// @brief execute a server-side traversal
/// @startDocuBlock JSF_HTTP_API_TRAVERSAL
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
/// - *startVertex*: id of the startVertex, e.g. *"users/foo"*.
///
/// - *edgeCollection*: (optional) name of the collection that contains the edges.
///
/// - *graphName*: (optional) name of the graph that contains the edges.
///         Either *edgeCollection* or *graphName* has to be given.
///         In case both values are set the *graphName* is prefered.
///
/// - *filter* (optional, default is to include all nodes):
///         body (JavaScript code) of custom filter function
///         function signature: (config, vertex, path) -> mixed
///         can return four different string values:
///         - *"exclude"* -> this vertex will not be visited.
///         - *"prune"* -> the edges of this vertex will not be followed.
///         - *""* or *undefined* -> visit the vertex and follow it's edges.
///         - *Array* -> containing any combination of the above.
///             If there is at least one *"exclude"* or *"prune"* respectivly
///             is contained, it's effect will occur.
///
/// - *minDepth* (optional, ANDed with any existing filters):
///    visits only nodes in at least the given depth
///
/// - *maxDepth* (optional, ANDed with any existing filters):
///    visits only nodes in at most the given depth
///
/// - *visitor* (optional): body (JavaScript) code of custom visitor function
///          function signature: (config, result, vertex, path) -> void
///          visitor function can do anything, but its return value is ignored. To
///          populate a result, use the *result* variable by reference
///
/// - *direction* (optional): direction for traversal
///   - *if set*, must be either *"outbound"*, *"inbound"*, or *"any"*
///   - *if not set*, the *expander* attribute must be specified
///
/// - *init* (optional): body (JavaScript) code of custom result initialisation function
///       function signature: (config, result) -> void
///       initialise any values in result with what is required
///
/// - *expander* (optional): body (JavaScript) code of custom expander function
///           *must* be set if *direction* attribute is **not** set
///           function signature: (config, vertex, path) -> array
///           expander must return an array of the connections for *vertex*
///           each connection is an object with the attributes *edge* and *vertex*
/// - *sort* (optional): body (JavaScript) code of a custom comparison function
///           for the edges. The signature of this function is
///           (l, r) -> integer (where l and r are edges) and must
///           return -1 if l is smaller than, +1 if l is greater than,
///           and 0 if l and r are equal. The reason for this is the
///           following: The order of edges returned for a certain
///           vertex is undefined. This is because there is no natural
///           order of edges for a vertex with multiple connected edges.
///           To explicitly define the order in which edges on the
///           vertex are followed, you can specify an edge comparator
///           function with this attribute. Note that the value here has
///           to be a string to conform to the JSON standard, which in
///           turn is parsed as function body on the server side. Furthermore
///           note that this attribute is only used for the standard
///           expanders. If you use your custom expander you have to
///           do the sorting yourself within the expander code.
///
/// - *strategy* (optional): traversal strategy
///           can be *"depthfirst"* or *"breadthfirst"*
///
/// - *order* (optional): traversal order
///        can be *"preorder"* or *"postorder"*
///
/// - *itemOrder* (optional): item iteration order
///            can be *"forward"* or *"backward"*
///
/// - *uniqueness* (optional): specifies uniqueness for vertices and edges visited
///             if set, must be an object like this:
///             *"uniqueness": {"vertices": "none"|"global"|path", "edges": "none"|"global"|"path"}*
///
/// - *maxIterations* (optional): Maximum number of iterations in each traversal. This number can be
///    set to prevent endless loops in traversal of cyclic graphs. When a traversal performs
///    as many iterations as the *maxIterations* value, the traversal will abort with an
///    error. If *maxIterations* is not set, a server-defined value may be used.
///
///
/// If the Traversal is successfully executed *HTTP 200* will be returned.
/// Additionally the *result* object will be returned by the traversal.
///
/// For successful traversals, the returned JSON object has the
/// following properties:
///
/// - *error*: boolean flag to indicate if an error occurred (*false*
///   in this case)
///
/// - *code*: the HTTP status code
///
/// - *result*: the return value of the traversal
///
/// If the traversal specification is either missing or malformed, the server
/// will respond with *HTTP 400*.
///
/// The body of the response will then contain a JSON object with additional error
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
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the traversal is fully executed
/// *HTTP 200* will be returned.
///
/// @RESTRETURNCODE{400}
/// If the traversal specification is either missing or malformed, the server
/// will respond with *HTTP 400*.
///
/// @RESTRETURNCODE{404}
/// The server will responded with *HTTP 404* if the specified edge collection
/// does not exist, or the specified start vertex cannot be found.
///
/// @RESTRETURNCODE{500}
/// The server will responded with *HTTP 500* when an error occurs inside the
/// traversal or if a traversal performs more than *maxIterations* iterations.
///
/// *Examples*
///
/// In the following examples the underlying graph will contain five persons
/// *Alice*, *Bob*, *Charlie*, *Dave* and *Eve*.
/// We will have the following directed relations:
///     - *Alice* knows *Bob*
///     - *Bob* knows *Charlie*
///     - *Bob* knows *Dave*
///     - *Eve* knows *Alice*
///     - *Eve* knows *Bob*
///
/// The starting vertex will always be Alice.
///
/// Follow only outbound edges:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalOutbound}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"graphName" : "' + g.__name + '", ';
///         body += '"direction" : "outbound"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Follow only inbound edges:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalInbound}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"graphName" : "' + g.__name + '", ';
///         body += '"direction" : "inbound"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Follow any direction of edges:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalAny}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = {
///       startVertex: a,
///       graphName: g.__name,
///       direction: "any",
///       uniqueness: {
///         vertices: "none",
///         edges: "global"
///       }
///     };
///
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Excluding *Charlie* and *Bob*:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalFilterExclude}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"graphName" : "' + g.__name + '", ';
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
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Do not follow edges from *Bob*:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalFilterPrune}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"graphName" : "' + g.__name + '", ';
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
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Visit only nodes in a depth of at least 2:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalMinDepth}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"graphName" : "' + g.__name + '", ';
///         body += '"direction" : "outbound", ';
///         body += '"minDepth" : 2}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Visit only nodes in a depth of at most 1:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalMaxDepth}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"graphName" : "' + g.__name + '", ';
///         body += '"direction" : "outbound", ';
///         body += '"maxDepth" : 1}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Count all visited nodes and return a list of nodes only:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalVisitorCountAndList}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = '{ "startVertex": "' + a + '", ';
///         body += '"graphName" : "' + g.__name + '", ';
///         body += '"direction" : "outbound", ';
///         body += '"init" : "result.visited = 0; result.myVertices = [ ];", ';
///         body += '"visitor" : "result.visited++; result.myVertices.push(vertex);"}';
///
///     var response = logCurlRequest('POST', url, body);
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Expand only inbound edges of *Alice* and outbound edges of *Eve*:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalVisitorExpander}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = {
///       startVertex: a,
///       graphName: g.__name,
///       expander: "var connections = [ ];" +
///                 "if (vertex.name === \"Alice\") {" +
///                 "config.datasource.getInEdges(vertex).forEach(function (e) {" +
///                 "connections.push({ " +
///                 "vertex: require(\"internal\").db._document(e._from), " +
///                 "edge: e" +
///                 "});" +
///                 "});" +
///                 "}" +
///                 "if (vertex.name === \"Eve\") {" +
///                 "config.datasource.getOutEdges(vertex).forEach(function (e) {" +
///                 "connections.push({" +
///                 "vertex: require(\"internal\").db._document(e._to), " +
///                 "edge: e" +
///                 "});" +
///                 "});" +
///                 "}" +
///                 "return connections;"
///     };
///
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Follow the *depthfirst* strategy:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalDepthFirst}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = {
///       startVertex: a,
///       graphName: g.__name,
///       direction: "any",
///       strategy: "depthfirst"
///     };
///
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using *postorder* ordering:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalPostorder}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = {
///       startVertex: a,
///       graphName: g.__name,
///       direction: "any",
///       order: "postorder"
///     };
///
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using *backward* item-ordering:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalBackwardItemOrder}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = {
///       startVertex: a,
///       graphName: g.__name,
///       direction: "any",
///       itemOrder: "backward"
///     };
///
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Edges should only be included once globally,
/// but nodes are included every time they are visited:
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalEdgeUniqueness}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var url = "/_api/traversal";
///     var body = {
///       startVertex: a,
///       graphName: g.__name,
///       direction: "any",
///       uniqueness: {
///         vertices: "none",
///         edges: "global"
///       }
///     };
///
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// If the underlying graph is cyclic, *maxIterations* should be set:
///
/// The underlying graph has two vertices *Alice* and *Bob*.
/// With the directed edges:
/// - *Alice* knows *Bob*
/// _ *Bob* knows *Alice*
///
///
/// @EXAMPLE_ARANGOSH_RUN{RestTraversalMaxIterations}
///     var examples = require("org/arangodb/graph-examples/example-graph.js");
///     var g = examples.loadGraph("knows_graph");
///     var a = g.persons.document("alice")._id;
///     var b = g.persons.document("bob")._id;
///     g.knows.truncate();
///     g.knows.save(a, b, {});
///     g.knows.save(b, a, {});
///     var url = "/_api/traversal";
///     var body = {
///       startVertex: a,
///       graphName: g.__name,
///       direction: "any",
///       uniqueness: {
///         vertices: "none",
///         edges: "none"
///       },
///       maxIterations: 5
///     };
///
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
///     assert(response.code === 500);
///
///     logJsonResponse(response);
///     examples.dropGraph("knows_graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function post_api_traversal(req, res) {
  /*jshint evil: true */
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

  var datasource;
  var edgeCollection;

  if (json.graphName === undefined) {
    // check edge collection
    // -----------------------------------------

    if (json.edgeCollection === undefined) {
      return badParam(req, res, arangodb.ERROR_GRAPH_NOT_FOUND,
        "missing graphname");
    }
    if (typeof json.edgeCollection !== "string") {
      return notFound(req, res, "invalid edgecollection");
    }

    try {
      edgeCollection = db._collection(json.edgeCollection);
      datasource = traversal.collectionDatasourceFactory(edgeCollection);
    }
    catch (ignore) {
    }

    if (edgeCollection === undefined ||
        edgeCollection === null) {
        return notFound(req, res, arangodb.ERROR_ARANGO_COLLECTION_NOT_FOUND,
          "invalid edgeCollection");
    }

  } else {
    if (typeof json.graphName !== "string" || !graph._exists(json.graphName)) {
      return notFound(req, res, arangodb.ERROR_GRAPH_NOT_FOUND,
        "invalid graphname");
    }
    datasource = traversal.generalGraphDatasourceFactory(json.graphName);
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
    expander = json.direction;
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

  // set up sort
  // -----------------------------------------

  var sort;

  if (json.sort !== undefined) {
    try {
      sort = new Function('l', 'r', json.sort);
    }
    catch (err6) {
      return badParam(req, res, "invalid sort function");
    }
  }

  // assemble config object
  // -----------------------------------------

  var config = {
    params: json,
    datasource: datasource,
    strategy: json.strategy,
    order: json.order,
    itemOrder: json.itemOrder,
    expander: expander,
    sort: sort,
    visitor: visitor,
    filter: filters,
    minDepth: json.minDepth,
    maxDepth: json.maxDepth,
    maxIterations: json.maxIterations,
    uniqueness: json.uniqueness
  };

  if (edgeCollection !== undefined) {
    config.edgeCollection = edgeCollection;
  }

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
    catch (err7) {
      return badParam(req, res, "invalid init function");
    }
  }

  // run the traversal
  // -----------------------------------------

  var traverser;
  try {
    traverser = new Traverser(config);
    traverser.traverse(result, doc);
    actions.resultOk(req, res, actions.HTTP_OK, { result : result });
  }
  catch (err8) {
    if (traverser === undefined) {
      // error during traversal setup
      return badParam(req, res, err8);
    }
    actions.resultException(req, res, err8, undefined, false);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gateway
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/traversal",

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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
