/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
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
/// TODO: 
/// startVertex: id of the startVertex, e.g. "users/foo", mandatory
/// edgeCollection: name of the collection that contains the edges, mandatory
/// filter: body (JavaScript code) of custom filter function, optional
///         function signature: (config, vertex, path) -> mixed
///         can return either undefined (=include), 'exclude', 'prune' or an array of these
///         if no filter is specified, all nodes will be included
/// minDepth: optional minDepth filter value (will be ANDed with any existing filters)
/// maxDepth: optional maxDepth filter value (will be ANDed with any existing filters)
/// visitor: body (JavaScript) code of custom visitor function, optional
///          function signature: (config, result, vertex, path) -> void
///          visitor function can do anything, but its return value is ignored. To 
///          populate a result, use the "result" variable by reference
///          example: "result.visited++; result.myVertices.push(vertex);"
/// direction: direction for traversal, optional
///            if set, must be either "outbound", "inbound", or "any"
///            if not set, the "expander" attribute must be specified
/// init: body (JavaScript) code of custom result initialisation function, optional
///       function signature: (config, result) -> void
///       initialise any values in result with what is required
///       example: "result.visited = 0; result.myVertices = [ ];"
/// expander: body (JavaScript) code of custom expander function, optional
///           must be set if "direction" attribute is not set
///           function signature: (config, vertex, path) -> array
///           expander must return an array of the connections for "vertex"
///           each connection is an object with the attributes "edge" and "vertex"
///           example: "expander": "var connections = [ ]; config.edgeCollection.outEdges(vertex).forEach(function (e) { connections.push({ vertex: e._to, edge: e }); }); return connections;"
/// strategy: traversal strategy, optional
///           can be "depthfirst" or "breadthfirst"
/// order: traversal order, optional
///        can be "preorder" or "postorder"
/// itemOrder: item iteration order, optional
///            can be "forward" or "backward"
/// uniqueness: specifies uniqueness for vertices and edges visited, optional
///             if set, must be an object like this: "uniqueness": { "vertices": "none"|"global"|path", "edges": "none"|"global"|"path" } 
///
/// the "result" object will be returned by the traversal
///
/// example:
/// POST {"edgeCollection": "e", "expander": "var connections = [ ]; config.edgeCollection.outEdges(vertex).forEach(function (e) { connections.push({ vertex: e._to, edge: e }); }); return connections;", "startVertex" : "v/jan", "filter": "return undefined;", "minDepth": 0 }
///
/// @RESTDESCRIPTION
/// TODO
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
    return badParam(req, res, "invalid startVertex");
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
    return badParam(req, res, "invalid edgeCollection");
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
