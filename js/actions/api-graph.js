/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, module */

////////////////////////////////////////////////////////////////////////////////
/// @brief graph api
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
/// @author Achim Brandt
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("org/arangodb/actions");
var graph = require("org/arangodb/graph");
var internal = require("internal");

var ArangoError = require("org/arangodb").ArangoError;
var QUERY = require("internal").AQL_QUERY;

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief url prefix
////////////////////////////////////////////////////////////////////////////////

var GRAPH_URL_PREFIX = "_api/graph";

////////////////////////////////////////////////////////////////////////////////
/// @brief context
////////////////////////////////////////////////////////////////////////////////

var GRAPH_CONTEXT = "api";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get graph by request parameter (throws exception)
////////////////////////////////////////////////////////////////////////////////

function graph_by_request (req) {
  var key = req.suffix[0];

  var g = new graph.Graph(key);

  if (g._properties === null) {
    throw "no graph found for: " + key;
    }

    return g;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get vertex by request (throws exception)
  ////////////////////////////////////////////////////////////////////////////////

  function vertex_by_request (req, g) {
    if (req.suffix.length < 3) {
      throw "no vertex found";
    }

    var key = req.suffix[2];
    if (req.suffix.length > 3) {
      key += "/" + req.suffix[3];
    }

    var vertex = g.getVertex(key);

    if (vertex === null || vertex._properties === undefined) {
      throw "no vertex found for: " + key;
    }

    return vertex;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get edge by request (throws exception)
  ////////////////////////////////////////////////////////////////////////////////

  function edge_by_request (req, g) {
    if (req.suffix.length < 3) {
      throw "no edge found";
    }

    var key = req.suffix[2];
    if (req.suffix.length > 3) {
      key += "/" + req.suffix[3];
    }
    var edge = g.getEdge(key);

    if (edge === null || edge._properties === undefined) {
      throw "no edge found for: " + key;
    }

    return edge;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief returns true if a "if-match" or "if-none-match" errer happens
  ////////////////////////////////////////////////////////////////////////////////

  function matchError (req, res, doc, errorCode) {  

    if (req.headers["if-none-match"] != undefined) {
      if (doc._rev === req.headers["if-none-match"]) {
        // error      
        res.responseCode = actions.HTTP_NOT_MODIFIED;
        res.contentType = "application/json; charset=utf-8";
        res.body = '';
        res.headers = {};      
        return true;
      }
    }  
    
    if (req.headers["if-match"] != undefined) {
      if (doc._rev !== req.headers["if-match"]) {
        // error
        actions.resultError(req, res, actions.HTTP_PRECONDITION_FAILED, errorCode, "wrong revision", {});
        return true;
      }
    }  
    
    var rev = req.parameters['rev'];
    if (rev != undefined) {
      if (doc._rev !== rev) {
        // error
      actions.resultError(req, res, actions.HTTP_PRECONDITION_FAILED, errorCode, "wrong revision", {});
      return true;
    }
  }  
  
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   graph functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a graph
///
/// @RESTHEADER{POST /_api/graph,create graph}
///
/// @RESTQUERYPARAMETERS
/// 
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been sync to disk.
///
/// @RESTBODYPARAM{graph,json,required}
/// The call expects a JSON hash array as body with the following attributes:
/// `_key`: The name of the new graph.
/// `vertices`: The name of the vertices collection.
/// `edges`: The name of the egde collection.
///
/// @RESTDESCRIPTION
/// Creates a new graph.
///
/// Returns an object with an attribute `graph` containing a
/// list of all graph properties.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// is returned if the graph was created sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the graph was created sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{400}
/// is returned if it failed.
/// The response body contains an error document in this case.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphPostGraph}
///     var url = "/_api/graph/";
///     var response = logCurlRequest('POST', url, {"_key" : "graph", "vertices" : "vertices", "edges" : "edges"});
/// 
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function post_graph_graph (req, res) {
  try {
    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_GRAPH);

    if (json === undefined) {
      return;
    }

    var name = json._key;
    var vertices = json.vertices;
    var edges = json.edges;

    var waitForSync = false;
    if (req.parameters['waitForSync']) {
      waitForSync = true;
    }

    var g = new graph.Graph(name, vertices, edges, waitForSync);

    if (g._properties === null) {
      throw "no properties of graph found";
    }

    var headers = {
      "Etag" :  g._properties._rev
    }

    waitForSync = waitForSync || g._gdb.properties().waitForSync;
    var returnCode = waitForSync ? actions.HTTP_CREATED : actions.HTTP_ACCEPTED;

    actions.resultOk(req, res, returnCode, { "graph" : g._properties }, headers );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_GRAPH, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get graph properties
///
/// @RESTHEADER{GET /_api/graph/{graph-name},get graph properties}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-None-Match,string,optional}
/// If the "If-None-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has a different revision than the
/// given etag. Otherwise a `HTTP 304` is returned.
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has the same revision ad the
/// given etag. Otherwise a `HTTP 412` is returned. As an alternative
/// you can supply the etag in an attribute `rev` in the URL.
///
/// @RESTDESCRIPTION
///
/// Returns an object with an attribute `graph` containing a
/// list of all graph properties.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the graph was found
///
/// @RESTRETURNCODE{404}
/// is returned if the graph was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{304}
/// "If-None-Match" header is given and the current graph has not a different 
/// version
///
/// @RESTRETURNCODE{412}
/// "If-Match" header or `rev` is given and the current graph has 
/// a different version
///
/// @EXAMPLES
///
/// get graph by name
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphGetGraph}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var url = "/_api/graph/graph";
///     var response = logCurlRequest('GET', url);
/// 
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function get_graph_graph (req, res) {
  try {
    var g = graph_by_request(req);
    
    if (matchError(req, res, g._properties, actions.ERROR_GRAPH_INVALID_GRAPH)) {
      return;
    } 
    
    var headers = {
      "Etag" :  g._properties._rev
    }

    actions.resultOk(req, res, actions.HTTP_OK, { "graph" : g._properties}, headers );
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a graph
///
/// @RESTHEADER{DELETE /_api/graph/{graph-name},delete graph}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has the same revision ad the
/// given etag. Otherwise a `HTTP 412` is returned. As an alternative
/// you can supply the etag in an attribute `rev` in the URL.
///
/// @RESTDESCRIPTION
/// Deletes graph, edges and vertices
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the graph was deleted and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the graph was deleted and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{404}
/// is returned if the graph was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{412}
/// "If-Match" header or `rev` is given and the current graph has 
/// a different version
///
/// @EXAMPLES
///
/// delete graph by name
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphDeleteGraph}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var url = "/_api/graph/graph";
///     var response = logCurlRequest('DELETE', url);
/// 
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function delete_graph_graph (req, res) {
  try {
    var g = graph_by_request(req);
    if (g === null || g === undefined) {
      throw "graph not found";
    }
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
    return;
  }
    
  if (matchError(req, res, g._properties, actions.ERROR_GRAPH_INVALID_GRAPH)) {
    return;
  } 
    
  var waitForSync = g._gdb.properties().waitForSync;
  if (req.parameters['waitForSync']) {
    waitForSync = true;
  }
    
  g.drop(waitForSync);

  waitForSync = waitForSync || g._gdb.properties().waitForSync;
  var returnCode = waitForSync ? actions.HTTP_OK : actions.HTTP_ACCEPTED;

  actions.resultOk(req, res, returnCode, { "deleted" : true });
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  vertex functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a graph vertex
///
/// @RESTHEADER{POST /_api/graph/`graph-name`/vertex,create vertex}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTQUERYPARAMETERS
/// 
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been sync to disk.
///
/// @RESTBODYPARAM{vertex,json,required}
/// The call expects a JSON hash array as body with the vertex properties:
/// - `_key`: The name of the vertex (optional).
/// - further optional attributes.
///
/// @RESTDESCRIPTION
/// Creates a vertex in a graph.
///
/// Returns an object with an attribute `vertex` containing a
/// list of all vertex properties.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// is returned if the graph was created sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the graph was created sucessfully and `waitForSync` was
/// `false`.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphCreateVertex}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var url = "/_api/graph/graph/vertex";
///     var response = logCurlRequest('POST', url, {"_key" : "v1", "optional1" : "val1" });
/// 
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function post_graph_vertex (req, res, g) {
  try {
    var json = actions.getJsonBody(req, res);
    var id;

    if (json) {
      id = json._key;
    }

    var waitForSync = g._vertices.properties().waitForSync;
    if (req.parameters['waitForSync']) {
      waitForSync = true;
    }
    
    var v = g.addVertex(id, json, waitForSync);

    if (v === null || v._properties === undefined) {
      throw "could not create vertex";
    }

    var headers = {
      "Etag" :  v._properties._rev
    }
    
    var returnCode = waitForSync ? actions.HTTP_CREATED : actions.HTTP_ACCEPTED;
    
    actions.resultOk(req, res, returnCode, { "vertex" : v._properties }, headers );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the vertex properties
///
/// @RESTHEADER{GET /_api/graph/`graph-name`/vertex,get vertex}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTQUERYPARAMETERS
/// 
/// @RESTQUERYPARAM{rev,string,optional}
/// Revision of a vertex
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-None-Match,string,optional}
/// If the "If-None-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has a different revision than the
/// given etag. Otherwise a `HTTP 304` is returned.
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has the same revision ad the
/// given etag. Otherwise a `HTTP 412` is returned. As an alternative
/// you can supply the etag in an attribute `rev` in the URL.
///
/// @RESTDESCRIPTION
/// Returns an object with an attribute `vertex` containing a
/// list of all vertex properties.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the graph was found
///
/// @RESTRETURNCODE{304}
/// "If-Match" header is given and the current graph has not a different 
/// version
///
/// @RESTRETURNCODE{404}
/// is returned if the graph or vertex was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{412}
/// "If-None-Match" header or `rev` is given and the current graph has 
/// a different version
///
/// @EXAMPLES
///
/// get vertex properties by name
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphGetVertex}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     g.addVertex("v1", {"optional1" : "val1" });
///     var url = "/_api/graph/graph/vertex/v1";
///     var response = logCurlRequest('GET', url);
/// 
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function get_graph_vertex (req, res, g) {
  try {
    var v = vertex_by_request(req, g);
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
    return;
  }
 
  if (matchError(req, res, v._properties, actions.ERROR_GRAPH_INVALID_VERTEX)) {
    return;
  } 
 
  var headers = {
    "Etag" :  v._properties._rev
  }
    
  actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : v._properties}, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete vertex
///
/// @RESTHEADER{DELETE /_api/graph/`graph-name`/vertex,delete vertex}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTQUERYPARAMETERS
/// 
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been sync to disk.
///
/// @RESTQUERYPARAM{rev,string,optional}
/// Revision of a vertex
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has the same revision ad the
/// given etag. Otherwise a `HTTP 412` is returned. As an alternative
/// you can supply the etag in an attribute `rev` in the URL.
///
/// @RESTDESCRIPTION
/// Deletes vertex and all in and out edges of the vertex
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the vertex was deleted and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the vertex was deleted and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{404}
/// is returned if the graph or the vertex was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{412}
/// "If-Match" header or `rev` is given and the current vertex has 
/// a different version
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphDeleteVertex}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     g.addVertex("v1", {"optional1" : "val1" });
///     var url = "/_api/graph/graph/vertex/v1";
///     var response = logCurlRequest('DELETE', url);
/// 
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function delete_graph_vertex (req, res, g) {
  try {
    var v = vertex_by_request(req, g);
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
    return;
  }
    
  if (matchError(req, res, v._properties, actions.ERROR_GRAPH_INVALID_VERTEX)) {
    return;
  } 

  var waitForSync = g._vertices.properties().waitForSync;
  if (req.parameters['waitForSync']) {
    waitForSync = true;
  }

  g.removeVertex(v, waitForSync);

  var returnCode = waitForSync ? actions.HTTP_OK : actions.HTTP_ACCEPTED;

  actions.resultOk(req, res, returnCode, { "deleted" : true });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update (PUT or PATCH) a vertex
////////////////////////////////////////////////////////////////////////////////

function update_graph_vertex (req, res, g, isPatch) {
  var v = null;

  try {
    v = vertex_by_request(req, g);
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, err);
    return;
  }

  if (matchError(req, res, v._properties, actions.ERROR_GRAPH_INVALID_VERTEX)) {
    return;
  } 

  try {
    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX);

    if (json === undefined) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, "error in request body");
      return;
    }

    var waitForSync = g._vertices.properties().waitForSync;
    if (req.parameters['waitForSync']) {
      waitForSync = true;
    }
    
    var shallow = json._shallowCopy;

    var id2 = null;
    if (isPatch) {
      var keepNull = req.parameters['keepNull'];
      if (keepNull != undefined || keepNull == "false") {
        keepNull = false;
      }
      else {
        keepNull = true;
      }

      id2 = g._vertices.update(v._properties, json, true, keepNull, waitForSync);      
    }
    else {
      id2 = g._vertices.replace(v._properties, shallow, true, waitForSync);      
    }

    var result = g._vertices.document(id2);

    var headers = {
      "Etag" :  result._rev
    }

    var returnCode = waitForSync ? actions.HTTP_CREATED : actions.HTTP_ACCEPTED;
    
    actions.resultOk(req, res, returnCode, { "vertex" : result }, headers );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, err);
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief updates a vertex
///
/// @RESTHEADER{PUT /_api/graph/`graph-name`/vertex,update vertex}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTQUERYPARAMETERS
/// 
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until vertex has been sync to disk.
///
/// @RESTQUERYPARAM{rev,string,optional}
/// Revision of a vertex
///
/// @RESTBODYPARAM{vertex,json,required}
/// The call expects a JSON hash array as body with the new vertex properties.
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{if-match,string,optional}
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is updated, if it has the same revision ad the
/// given etag. Otherwise a `HTTP 412` is returned. As an alternative
/// you can supply the etag in an attribute `rev` in the URL.
///
/// @RESTDESCRIPTION
/// Replaces the vertex properties.
///
/// Returns an object with an attribute `vertex` containing a
/// list of all vertex properties.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// is returned if the vertex was updated sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the vertex was updated sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{404}
/// is returned if the graph or the vertex was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{412}
/// "If-Match" header or `rev` is given and the current vertex has 
/// a different version
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphChangeVertex}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     g.addVertex("v1", {"optional1" : "val1" });
///     var url = "/_api/graph/graph/vertex/v1";
///     var response = logCurlRequest('PUT', url, { "optional1" : "val2" });
/// 
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function put_graph_vertex (req, res, g) {
  update_graph_vertex(req, res, g, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a vertex
///
/// @RESTHEADER{PATCH /_api/graph/`graph-name`/vertex,update vertex}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTQUERYPARAMETERS
/// 
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until vertex has been sync to disk.
///
/// @RESTQUERYPARAM{rev,string,optional}
/// Revision of a vertex
///
/// @RESTQUERYPARAM{keepNull,boolean,optional}
/// Modify the behavior of the patch command to remove any attribute
///
/// @RESTBODYPARAM{graph,json,required}
/// The call expects a JSON hash array as body with the properties to patch.
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{if-match,string,optional}
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is updated, if it has the same revision ad the
/// given etag. Otherwise a `HTTP 412` is returned. As an alternative
/// you can supply the etag in an attribute `rev` in the URL.
///
/// @RESTDESCRIPTION
/// Partially updates the vertex properties.
///
/// Setting an attribute value to `null` in the patch document will cause a value 
/// of `null` be saved for the attribute by default. If the intention is to 
/// delete existing attributes with the patch command, the URL parameter 
/// `keepNull` can be used with a value of `false`. 
/// This will modify the behavior of the patch command to remove any attributes 
/// from the existing document that are contained in the patch document 
/// with an attribute value of `null`.
//
/// Returns an object with an attribute `vertex` containing a
/// list of all vertex properties.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// is returned if the vertex was updated sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the vertex was updated sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{404}
/// is returned if the graph or the vertex was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{412}
/// "If-Match" header or `rev` is given and the current vertex has 
/// a different version
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphChangepVertex}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     g.addVertex("v1", { "optional1" : "val1" });
///     var url = "/_api/graph/graph/vertex/v1";
///     var response = logCurlRequest('PATCH', url, { "optional1" : "vertexPatch" });
/// 
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///     var response = logCurlRequest('PATCH', url, { "optional1" : null });
/// 
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function patch_graph_vertex (req, res, g) {
  update_graph_vertex(req, res, g, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the compare operator (throws exception)
////////////////////////////////////////////////////////////////////////////////

function process_property_compare (compare) {
  if (compare === undefined) {
    return "==";
  }

  switch (compare) {
    case ("==") :
      return compare;

    case ("!=") :
      return compare;

    case ("<") :
      return compare;

    case (">") :
      return compare;

    case (">=") :
      return compare;

    case ("<=") :
      return compare;
  }

  throw "unknow compare function in property filter";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills a filter (throws exception)
////////////////////////////////////////////////////////////////////////////////

function process_property_filter (data, num, property, collname) {
  if (property.key !== undefined && property.value !== undefined) {
      if (data.filter === "") { data.filter = " FILTER"; } else { data.filter += " &&";}
      data.filter += " " + collname + "[@key" + num.toString() + "] " +
            process_property_compare(property.compare) + " @value" + num.toString();
      data.bindVars["key" + num.toString()] = property.key;
      data.bindVars["value" + num.toString()] = property.value;
      return;
  }

  throw "error in property filter";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills a properties filter
////////////////////////////////////////////////////////////////////////////////

function process_properties_filter (data, properties, collname) {
  var i;

  if (properties instanceof Array) {
    for (i = 0;  i < properties.length;  ++i) {
      process_property_filter(data, i, properties[i], collname);
    }
  }
  else if (properties instanceof Object) {
    process_property_filter(data, 0, properties, collname);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills a labels filter
////////////////////////////////////////////////////////////////////////////////

function process_labels_filter (data, labels, collname) {

  // filter edge labels
  if (labels !== undefined && labels instanceof Array && labels.length > 0) {
    if (data.filter === "") { data.filter = " FILTER"; } else { data.filter += " &&";}
    data.filter += ' ' + collname + '["$label"] IN @labels';
    data.bindVars.labels = labels;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the vertices of a graph
///
/// @RESTHEADER{POST /_api/graph/`graph-name`/vertices,get vertices}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTBODYPARAM{filter,json,required}
/// The call expects a JSON hash array as body to filter the result:
///
/// @RESTDESCRIPTION
/// Returns a cursor.
///
/// The call expects a JSON hash array as body to filter the result:
///
/// - `batchSize`: the batch size of the returned cursor
/// - `limit`: limit the result size
/// - `count`: return the total number of results (default "false")
/// - `filter`: a optional filter
///
/// The attributes of filter
/// - `properties`: filter by an array of vertex properties
///
/// The attributes of a property filter
/// - `key`: filter the result vertices by a key value pair
/// - `value`: the value of the `key`
/// - `compare`: a compare operator
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// is returned if the cursor was created
///
/// @EXAMPLES
///
/// Select all vertices
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphGetVertices}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     g.addVertex("v1", { "optional1" : "val1" });
///     g.addVertex("v2", { "optional1" : "val1" });
///     g.addVertex("v3", { "optional1" : "val1" });
///     g.addVertex("v4", { "optional1" : "val1" });
///     g.addVertex("v5", { "optional1" : "val1" });
///     var url = "/_api/graph/graph/vertices";
///     var response = logCurlRequest('POST', url, {"batchSize" : 100 });
/// 
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function post_graph_all_vertices (req, res, g) {
  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    json = {};
  }

  try {
    var data = {
      'filter': '',
      'bindVars': { '@vertexColl' : g._vertices.name() }
    };

    var limit = "";
    if (json.limit !== undefined) {
      limit = " LIMIT " + parseInt(json.limit);
    }

    if (json.filter !== undefined && json.filter.properties !== undefined) {
      process_properties_filter(data, json.filter.properties, "v");
    }

    // build aql query
    var query = "FOR v IN @@vertexColl" + data.filter + limit + " RETURN v";

    var cursor = QUERY(query,
                          data.bindVars,
                          (json.count !== undefined ? json.count : false),
                          json.batchSize,
                          (json.batchSize === undefined));

    // error occurred
    if (cursor instanceof Error) {
      actions.resultBad(req, res, cursor);
      return;
    }

    // this might dispose or persist the cursor
    actions.resultCursor(req, res, cursor, actions.HTTP_CREATED,
                         { countRequested: json.count ? true : false });
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get neighbors of a vertex
///
/// @RESTHEADER{POST /_api/graph/`graph-name`/vertices/`vertice-name`,get vertices}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTBODYPARAM{graph,json,required}
/// The call expects a JSON hash array as body to filter the result:
///
/// @RESTDESCRIPTION
/// Returns a cursor.
///
/// The call expects a JSON hash array as body to filter the result:
///
/// - `batchSize`: the batch size of the returned cursor
/// - `limit`: limit the result size
/// - `count`: return the total number of results (default "false")
/// - `filter`: a optional filter
///
/// The attributes of filter
/// - `direction`: Filter for inbound (value "in") or outbound (value "out")
///   neighbors. Default value is "any".
/// - `labels`: filter by an array of edge labels (empty array means no restriction)
/// - `properties`: filter neighbors by an array of edge properties
///
/// The attributes of a property filter
/// - `key`: filter the result vertices by a key value pair
/// - `value`: the value of the `key`
/// - `compare`: a compare operator
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// is returned if the cursor was created
///
/// @EXAMPLES
///
/// Select all vertices
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphGetVertexVertices}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex("v1", { "optional1" : "val1" });
///     var v2 = g.addVertex("v2", { "optional1" : "val1" });
///     var v3 = g.addVertex("v3", { "optional1" : "val1" });
///     var v4 = g.addVertex("v4", { "optional1" : "val1" });
///     var v5 = g.addVertex("v5", { "optional1" : "val1" });
///     g.addEdge(v1,v5, 1);
///     g.addEdge(v1,v2, 2);
///     g.addEdge(v1,v3, 3);
///     g.addEdge(v2,v4, 4);
///     var url = "/_api/graph/graph/vertices/v2";
///     var body = '{"batchSize" : 100, "filter" : {"direction" : "any", "properties":';
///     body += '[] }}';
///     var response = logCurlRequest('POST', url, body);
/// 
///     assert(response.code === 201);
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Select vertices by direction and property filter
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphGetVertexVertices2}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex("v1", { "optional1" : "val1" });
///     var v2 = g.addVertex("v2", { "optional1" : "val2" });
///     var v3 = g.addVertex("v3", { "optional1" : "val2" });
///     var v4 = g.addVertex("v4", { "optional1" : "val2" });
///     var v5 = g.addVertex("v5", { "optional1" : "val1" });
///     g.addEdge(v1, v5, 1);
///     g.addEdge(v2, v1, 2);
///     g.addEdge(v1, v3, 3);
///     g.addEdge(v4, v2, 4);
///     var url = "/_api/graph/graph/vertices/v2";
///     var body = '{"batchSize" : 100, "filter" : {"direction" : "out", "properties":';
///     body += '[ { "key": "optional1", "value": "val2", "compare" : "==" }, ] }}';
///     var response = logCurlRequest('POST', url, body);
/// 
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function post_graph_vertex_vertices (req, res, g) {
  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    json = {};
  }

  try {
    var v = vertex_by_request(req, g);

    var data = {
      'filter' : '',
      'bindVars' : {
         '@vertexColl' : g._vertices.name(),
         '@edgeColl' : g._edges.name(),
         'id' : v._properties._id
      }
    };

    var limit = "";
    if (json.limit !== undefined) {
      limit = " LIMIT " + parseInt(json.limit);
    }

    var direction = "any";
    if (json.filter !== undefined && json.filter.direction !== undefined) {
      if (json.filter.direction === "in") {
        direction = "inbound";
      }
      else if (json.filter.direction === "out") {
        direction = "outbound";
      }
    }
    
    if (json.filter !== undefined && json.filter.properties !== undefined) {
      process_properties_filter(data, json.filter.properties, "n.edge");
    }

    if (json.filter !== undefined && json.filter.labels !== undefined) {
      process_labels_filter(data, json.filter.labels, "n.edge");
    }

    // build aql query
    var query = 'FOR n IN NEIGHBORS( @@vertexColl, @@edgeColl, @id, "' + direction + '") ' + 
            data.filter + limit + " RETURN n.vertex ";
    
    var cursor = QUERY(query,
                          data.bindVars,
                          (json.count !== undefined ? json.count : false),
                          json.batchSize,
                          (json.batchSize === undefined));

    // error occurred
    if (cursor instanceof Error) {
      actions.resultBad(req, res, cursor);
      return;
    }

    // this might dispose or persist the cursor
    actions.resultCursor(req,
                         res,
                         cursor,
                         actions.HTTP_CREATED,
                         { countRequested: json.count ? true : false });
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an edge
///
/// @RESTHEADER{POST /_api/graph/`graph-name`/edge,create edge}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTQUERYPARAMETERS
/// 
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until edge has been sync to disk.
///
/// @RESTBODYPARAM{edge,json,required}
/// The call expects a JSON hash array as body with the edge properties:
///
/// @RESTDESCRIPTION
/// Creates an edge in a graph.
///
/// The call expects a JSON hash array as body with the edge properties:
///
/// - `_key`: The name of the edge.
/// - `_from`: The name of the from vertex.
/// - `_to`: The name of the to vertex.
/// - `$label`: A label for the edge (optional).
/// - further optional attributes.
///
/// Returns an object with an attribute `edge` containing the
/// list of all edge properties.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// is returned if the edge was created sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the edge was created sucessfully and `waitForSync` was
/// `false`.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphCreateEdge}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var url = "/_api/graph/graph/edge";
///     g.addVertex("vert1");
///     g.addVertex("vert2");
///     var body = {"_key" : "edge1", "_from" : "vert2", "_to" : "vert1", "optional1" : "val1"};
///     var response = logCurlRequest('POST', url, body);
/// 
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function post_graph_edge (req, res, g) {
  try {
    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_EDGE);

    if (json === undefined) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_EDGE, "error in request body");
      return;
    }

    var waitForSync = g._edges.properties().waitForSync;
    if (req.parameters['waitForSync']) {
      waitForSync = true;
    }
    
    var id = json._key;
    var out = g.getVertex(json._from);
    var ine = g.getVertex(json._to);
    var label = json.$label;

    var e = g.addEdge(out, ine, id, label, json, waitForSync);

    if (e === null || e._properties === undefined) {
      throw "could not create edge";
    }

    var headers = {
      "Etag" :  e._properties._rev
    }
    
    var returnCode = waitForSync ? actions.HTTP_CREATED : actions.HTTP_ACCEPTED;

    actions.resultOk(req, res, returnCode, { "edge" : e._properties }, headers);
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get edge properties
///
/// @RESTHEADER{GET /_api/graph/`graph-name`/edge,get edge}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTQUERYPARAMETERS
/// 
/// @RESTQUERYPARAM{rev,string,optional}
/// Revision of an edge
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{if-none-match,string,optional}
/// If the "If-None-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has a different revision than the
/// given etag. Otherwise a `HTTP 304` is returned.
///
/// @RESTHEADERPARAM{if-match,string,optional}
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has the same revision ad the
/// given etag. Otherwise a `HTTP 412` is returned. As an alternative
/// you can supply the etag in an attribute `rev` in the URL.
///
/// @RESTDESCRIPTION
/// Returns an object with an attribute `edge` containing a
/// list of all edge properties.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the edge was found
///
/// @RESTRETURNCODE{304}
/// "If-Match" header is given and the current edge has not a different 
/// version
///
/// @RESTRETURNCODE{404}
/// is returned if the graph or edge was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{412}
/// "If-None-Match" header or `rev` is given and the current edge has 
/// a different version
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphGetEdge}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var url = "/_api/graph/graph/edge/edge1";
///     var v1 = g.addVertex("vert1");
///     var v2 = g.addVertex("vert2");
///     g.addEdge(v1, v2, "edge1", { "optional1" : "val1" }); 
///     var response = logCurlRequest('GET', url);
/// 
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function get_graph_edge (req, res, g) {
  try {
    var e = edge_by_request(req, g);

    if (matchError(req, res, e._properties, actions.ERROR_GRAPH_INVALID_EDGE)) {
      return;
    } 
 
    var headers = {
      "Etag" :  e._properties._rev
    }
 
    actions.resultOk(req, res, actions.HTTP_OK, { "edge" : e._properties}, headers);
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an edge
///
/// @RESTHEADER{DELETE /_api/graph/`graph-name`/edge,delete edge}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTQUERYPARAMETERS
/// 
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until edge has been sync to disk.
///
/// @RESTQUERYPARAM{rev,string,optional}
/// Revision of an edge
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{if-match,string,optional}
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has the same revision ad the
/// given etag. Otherwise a `HTTP 412` is returned. As an alternative
/// you can supply the etag in an attribute `rev` in the URL.
///
/// @RESTDESCRIPTION
/// Deletes an edge of the graph
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the edge was deletd sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the edge was deleted sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{404}
/// is returned if the graph or the edge was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{412}
/// "If-Match" header or `rev` is given and the current edge has 
/// a different version
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphDeleteEdge}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex("vert1");
///     var v2 = g.addVertex("vert2");
///     g.addEdge(v1, v2, "edge1", { "optional1" : "val1" }); 
///     var url = "/_api/graph/graph/edge/edge1";
///     var response = logCurlRequest('DELETE', url);
/// 
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function delete_graph_edge (req, res, g) {
  try {
    var e = edge_by_request(req, g);
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_EDGE, err);
    return;
  }

  if (matchError(req, res, e._properties, actions.ERROR_GRAPH_INVALID_EDGE)) {
    return;
  } 
 
  var waitForSync = g._edges.properties().waitForSync;
  if (req.parameters['waitForSync']) {
    waitForSync = true;
  }
    
  g.removeEdge(e, waitForSync);

  var returnCode = waitForSync ? actions.HTTP_OK : actions.HTTP_ACCEPTED;

  actions.resultOk(req, res, returnCode, { "deleted" : true });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update (PUT or PATCH) an edge
////////////////////////////////////////////////////////////////////////////////

function update_graph_edge (req, res, g, isPatch) {
  var e = null;

  try {
    e = edge_by_request(req, g);
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, err);
    return;
  }

  if (matchError(req, res, e._properties, actions.ERROR_GRAPH_INVALID_EDGE)) {
    return;
  } 
  
  try {
    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE);

    if (json === undefined) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, "error in request body");
      return;
    }

    var waitForSync = g._edges.properties().waitForSync;
    if (req.parameters['waitForSync']) {
      waitForSync = true;
    }
    
    var shallow = json._shallowCopy;
    shallow.$label = e._properties.$label;
    
    var id2 = null;
    if (isPatch) {
      var keepNull = req.parameters['keepNull'];
      if (keepNull != undefined || keepNull == "false") {
        keepNull = false;
      }
      else {
        keepNull = true;
      }
    
      id2 = g._edges.update(e._properties, shallow, true, keepNull, waitForSync);      
    }
    else {
      id2 = g._edges.replace(e._properties, shallow, true, waitForSync);      
    }

    var result = g._edges.document(id2);

    var headers = {
      "Etag" :  result._rev
    }
 
    var returnCode = waitForSync ? actions.HTTP_CREATED : actions.HTTP_ACCEPTED;
    
    actions.resultOk(req, res, returnCode, { "edge" : result}, headers );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, err);
  }  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an edge
///
/// @RESTHEADER{PUT /_api/graph/`graph-name`/edge,update edge}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTQUERYPARAMETERS
/// 
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until edge has been sync to disk.
///
/// @RESTQUERYPARAM{rev,string,optional}
/// Revision of an edge
///
/// @RESTBODYPARAM{edge,json,required}
/// The call expects a JSON hash array as body with the new edge properties.
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{if-match,string,optional}
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has the same revision ad the
/// given etag. Otherwise a `HTTP 412` is returned. As an alternative
/// you can supply the etag in an attribute `rev` in the URL.
///
/// @RESTDESCRIPTION
/// Replaces the optional edge properties.
///
/// The call expects a JSON hash array as body with the new edge properties.
///
/// Returns an object with an attribute `edge` containing a
/// list of all edge properties.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// is returned if the edge was updated sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the edge was updated sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{404}
/// is returned if the graph or the edge was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{412}
/// "If-Match" header or `rev` is given and the current edge has 
/// a different version
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphChangeEdge}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex("vert1");
///     var v2 = g.addVertex("vert2");
///     g.addEdge(v1, v2, "edge1", { "optional1" : "val1" }); 
///     var url = "/_api/graph/graph/edge/edge1";
///     var response = logCurlRequest('PUT', url, { "optional1" : "val2" });
/// 
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function put_graph_edge (req, res, g) {
  update_graph_edge (req, res, g, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an edge
///
/// @RESTHEADER{PATCH /_api/graph/`graph-name`/edge,update edge}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTQUERYPARAMETERS
/// 
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until edge has been sync to disk.
///
/// @RESTQUERYPARAM{rev,string,optional}
/// Revision of an edge
///
/// @RESTQUERYPARAM{keepNull,boolean,optional}
/// Modify the behavior of the patch command to remove any attribute
///
/// @RESTBODYPARAM{edge-properties,json,required}
/// The call expects a JSON hash array as body with the properties to patch.
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{if-match,string,optional}
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has the same revision ad the
/// given etag. Otherwise a `HTTP 412` is returned. As an alternative
/// you can supply the etag in an attribute `rev` in the URL.
///
/// @RESTDESCRIPTION
/// Partially updates the edge properties.
///
/// Setting an attribute value to `null` in the patch document will cause a value 
/// of `null` be saved for the attribute by default. If the intention is to 
/// delete existing attributes with the patch command, the URL parameter 
/// `keepNull` can be used with a value of `false`. 
/// This will modify the behavior of the patch command to remove any attributes 
/// from the existing document that are contained in the patch document 
/// with an attribute value of `null`.
///
/// Returns an object with an attribute `edge` containing a
/// list of all edge properties.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// is returned if the edge was updated sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the edge was updated sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{404}
/// is returned if the graph or the edge was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{412}
/// "If-Match" header or `rev` is given and the current edge has 
/// a different version
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphChangepEdge}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex("vert1");
///     var v2 = g.addVertex("vert2");
///     g.addEdge(v1, v2, "edge1", { "optional1" : "val1" }); 
///     var url = "/_api/graph/graph/edge/edge1";
///     var response = logCurlRequest('PATCH', url, { "optional3" : "val3" });
/// 
///     assert(response.code === 202);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function patch_graph_edge (req, res, g) {
  update_graph_edge (req, res, g, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get edges of a graph
///
/// @RESTHEADER{POST /_api/graph/`graph-name`/edges,get edges}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTBODYPARAM{edge-properties,json,required}
/// The call expects a JSON hash array as body to filter the result:
///
/// @RESTDESCRIPTION
/// Returns a cursor.
///
/// The call expects a JSON hash array as body to filter the result:
///
/// - `batchSize`: the batch size of the returned cursor
/// - `limit`: limit the result size
/// - `count`: return the total number of results (default "false")
/// - `filter`: a optional filter
///
/// The attributes of filter
/// - `labels`: filter by an array of edge labels
/// - `properties`: filter by an array of edge properties
///
/// The attributes of a property filter
/// - `key`: filter the result edges by a key value pair
/// - `value`: the value of the `key`
/// - `compare`: a compare operator
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// is returned if the cursor was created
///
/// @EXAMPLES
///
/// Select all edges
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphGetEdges}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex("v1", { "optional1" : "val1" });
///     var v2 = g.addVertex("v2", { "optional1" : "val1" });
///     var v3 = g.addVertex("v3", { "optional1" : "val1" });
///     var v4 = g.addVertex("v4", { "optional1" : "val1" });
///     var v5 = g.addVertex("v5", { "optional1" : "val1" });
///     g.addEdge(v1, v2, "edge1", { "optional1" : "val1" }); 
///     g.addEdge(v1, v3, "edge2", { "optional1" : "val1" }); 
///     g.addEdge(v2, v4, "edge3", { "optional1" : "val1" }); 
///     g.addEdge(v1, v5, "edge4", { "optional1" : "val1" }); 
///     var url = "/_api/graph/graph/edges";
///     var response = logCurlRequest('POST', url, {"batchSize" : 100 });
/// 
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function post_graph_all_edges (req, res, g) {
  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    json = {};
  }

  try {

    var data = {
      'filter' : '',
      'bindVars' : {
         '@edgeColl' : g._edges.name()
      }
    };

    var limit = "";
    if (json.limit !== undefined) {
      limit = " LIMIT " + parseInt(json.limit);
    }

    if (json.filter !== undefined && json.filter.properties !== undefined) {
      process_properties_filter(data, json.filter.properties, "e");
    }

    if (json.filter !== undefined && json.filter.labels !== undefined) {
      process_labels_filter(data, json.filter.labels, "e");
    }

    var query = "FOR e IN @@edgeColl" + data.filter + limit + " RETURN e";

    var cursor = QUERY(query,
                          data.bindVars,
                          (json.count !== undefined ? json.count : false),
                          json.batchSize,
                          (json.batchSize === undefined));

    // error occurred
    if (cursor instanceof Error) {
      actions.resultBad(req, res, cursor);
      return;
    }

    // this might dispose or persist the cursor
    actions.resultCursor(req,
                         res,
                         cursor,
                         actions.HTTP_CREATED,
                         { countRequested: json.count ? true : false });
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get edges of a vertex
///
/// @RESTHEADER{POST /_api/graph/`graph-name`/edges/`vertex-name`,get edges}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{graph-name,string,required}
/// The name of the graph
///
/// @RESTURLPARAM{vertex-name,string,required}
/// The name of the vertex
///
/// @RESTBODYPARAM{edge-properties,json,required}
/// The call expects a JSON hash array as body to filter the result:
///
/// @RESTDESCRIPTION
///
/// Returns a cursor.
///
/// The call expects a JSON hash array as body to filter the result:
///
/// - `batchSize`: the batch size of the returned cursor
/// - `limit`: limit the result size
/// - `count`: return the total number of results (default "false")
/// - `filter`: a optional filter
///
/// The attributes of filter
/// - `direction`: Filter for inbound (value "in") or outbound (value "out")
///   neighbors. Default value is "any".
/// - `labels`: filter by an array of edge labels
/// - `properties`: filter neighbors by an array of properties
///
/// The attributes of a property filter
/// - `key`: filter the result vertices by a key value pair
/// - `value`: the value of the `key`
/// - `compare`: a compare operator
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{201}
/// is returned if the cursor was created
///
/// @EXAMPLES
///
/// Select all edges
///
/// @EXAMPLE_ARANGOSH_RUN{RestGraphGetVertexEdges}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex("v1", { "optional1" : "val1" });
///     var v2 = g.addVertex("v2", { "optional1" : "val1" });
///     var v3 = g.addVertex("v3", { "optional1" : "val1" });
///     var v4 = g.addVertex("v4", { "optional1" : "val1" });
///     var v5 = g.addVertex("v5", { "optional1" : "val1" });
///     g.addEdge(v1, v2, "edge1", { "optional1" : "val1" }); 
///     g.addEdge(v1, v3, "edge2", { "optional1" : "val1" }); 
///     g.addEdge(v2, v4, "edge3", { "optional1" : "val1" }); 
///     g.addEdge(v1, v5, "edge4", { "optional1" : "val1" }); 
///     var url = "/_api/graph/graph/edges/v2";
///     var body = '{"batchSize" : 100, "filter" : { "direction" : "any" }}';
///     var response = logCurlRequest('POST', url, body);
/// 
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function post_graph_vertex_edges (req, res, g) {
  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    json = {};
  }

  try {
    var v = vertex_by_request(req, g);

    var data = {
      'filter' : '',
      'bindVars' : {
         '@edgeColl' : g._edges.name(),
         'id' : v._properties._id
      }
    };

    var limit = "";
    if (json.limit !== undefined) {
      limit = " LIMIT " + parseInt(json.limit);
    }

    var direction = "any";
    if (json.filter !== undefined && json.filter.direction !== undefined) {
      if (json.filter.direction === "in") {
        direction = "inbound";
      }
      else if (json.filter.direction === "out") {
        direction = "outbound";
      }
    }

    if (json.filter !== undefined && json.filter.properties !== undefined) {
      process_properties_filter(data, json.filter.properties, "e");
    }

    if (json.filter !== undefined && json.filter.labels !== undefined) {
      process_labels_filter(data, json.filter.labels, "e");
    }

    var query = 'FOR e in EDGES( @@edgeColl , @id , "' + direction + '") ' 
            + data.filter + limit + " RETURN e";

    var cursor = QUERY(query,
                       data.bindVars,
                       (json.count !== undefined ? json.count : false),
                       json.batchSize,
                       (json.batchSize === undefined));

    // error occurred
    if (cursor instanceof Error) {
      actions.resultException(req, res, cursor, undefined, false);
      return;
    }

    // this might dispose or persist the cursor
    actions.resultCursor(req,
                         res,
                         cursor,
                         actions.HTTP_CREATED,
                         { countRequested: json.count ? true : false });
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle POST /_api/graph/<...>/vertices
////////////////////////////////////////////////////////////////////////////////

function post_graph_vertices (req, res, g) {
  if (req.suffix.length > 2) {
    post_graph_vertex_vertices (req, res, g);
  }
  else {
    post_graph_all_vertices (req, res, g);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle POST /_api/graph/<...>/edges
////////////////////////////////////////////////////////////////////////////////

function post_graph_edges (req, res, g) {
  if (req.suffix.length > 2) {
    post_graph_vertex_edges (req, res, g);
  }
  else {
    post_graph_all_edges (req, res, g);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle post requests
////////////////////////////////////////////////////////////////////////////////

function post_graph (req, res) {
  if (req.suffix.length === 0) {
    // POST /_api/graph
    post_graph_graph(req, res);
  }
  else if (req.suffix.length > 1) {
    // POST /_api/graph/<key>/...
    try {
      var g = graph_by_request(req);
    }
    catch (err) {
      actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
      return;
    }

    switch (req.suffix[1]) {
      case ("vertex") :
        post_graph_vertex(req, res, g);
        break;

      case ("edge") :
        post_graph_edge(req, res, g);
        break;

      case ("vertices") :
        post_graph_vertices(req, res, g);
        break;

      case ("edges") :
        post_graph_edges(req, res, g);
        break;

     default:
        actions.resultUnsupported(req, res);
     }

  }
  else {
    actions.resultUnsupported(req, res);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle get requests
////////////////////////////////////////////////////////////////////////////////

function get_graph (req, res) {
  if (req.suffix.length === 0) {
    // GET /_api/graph
    actions.resultUnsupported(req, res);
  }
  else if (req.suffix.length === 1) {
    // GET /_api/graph/<key>
    get_graph_graph(req, res);
  }
  else {
    // GET /_api/graph/<key>/...
    try {
      var g = graph_by_request(req);
    }
    catch (err) {
      actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
      return;
    }

    switch (req.suffix[1]) {
      case ("vertex") :
        get_graph_vertex(req, res, g);
        break;

      case ("edge") :
        get_graph_edge(req, res, g);
        break;

      default:
        actions.resultUnsupported(req, res);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle put requests
////////////////////////////////////////////////////////////////////////////////

function put_graph (req, res) {
  if (req.suffix.length < 2) {
    // PUT /_api/graph
    actions.resultUnsupported(req, res);
  }
  else {
    // PUT /_api/graph/<key>/...
    try {
      var g = graph_by_request(req);
    }
    catch (err) {
      actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
      return;
    }

    switch (req.suffix[1]) {
      case ("vertex") :
        put_graph_vertex(req, res, g);
        break;

      case ("edge") :
        put_graph_edge(req, res, g);
        break;

      default:
        actions.resultUnsupported(req, res);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle patch requests
////////////////////////////////////////////////////////////////////////////////

function patch_graph (req, res) {
  if (req.suffix.length < 2) {
    // PATCH /_api/graph
    actions.resultUnsupported(req, res);
  }
  else {
    // PATCH /_api/graph/<key>/...
    try {
      var g = graph_by_request(req);
    }
    catch (err) {
      actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
      return;
    }

    switch (req.suffix[1]) {
      case ("vertex") :
        patch_graph_vertex(req, res, g);
        break;

      case ("edge") :
        patch_graph_edge(req, res, g);
        break;

      default:
        actions.resultUnsupported(req, res);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle delete requests
////////////////////////////////////////////////////////////////////////////////

function delete_graph (req, res) {
  if (req.suffix.length === 0) {
    // DELETE /_api/graph
    actions.resultUnsupported(req, res);
  }
  else if (req.suffix.length === 1) {
    // DELETE /_api/graph/<key>
    delete_graph_graph(req, res);
  }
  else {
    // DELETE /_api/graph/<key>/...
    try {
      var g = graph_by_request(req);
    }
    catch (err) {
      actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
      return;
    }

    switch (req.suffix[1]) {
      case ("vertex") :
        delete_graph_vertex(req, res, g);
        break;

      case ("edge") :
        delete_graph_edge(req, res, g);
        break;

     default:
        actions.resultUnsupported(req, res);
     }

  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actions gateway
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : GRAPH_URL_PREFIX,
  context : GRAPH_CONTEXT,

  callback : function (req, res) {
    try {
      switch (req.requestType) {
        case (actions.POST) :
          post_graph(req, res);
          break;

        case (actions.DELETE) :
          delete_graph(req, res);
          break;

        case (actions.GET) :
          get_graph(req, res);
          break;

        case (actions.PUT) :
          put_graph(req, res);
          break;

        case (actions.PATCH) :
          patch_graph(req, res);
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
