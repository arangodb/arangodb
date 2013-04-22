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
/// @REST{POST /_api/graph}
///
////////////////////////////////////////////////////////////////////////////////
///
/// Creates a new graph.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - @LIT{_key}: The name of the new graph.
/// - @LIT{vertices}: The name of the vertices collection.
/// - @LIT{edges}: The name of the egde collection.
///
/// Returns an object with an attribute @LIT{graph} containing a
/// list of all graph properties.
///
/// @EXAMPLES
///
/// @verbinclude api-graph-create-graph
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

    var g = new graph.Graph(name, vertices, edges);

    if (g._properties === null) {
      throw "no properties of graph found";
    }

    actions.resultOk(req, res, actions.HTTP_CREATED, { "graph" : g._properties } );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_GRAPH, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get graph properties
///
/// @RESTHEADER{GET /_api/graph,get graph properties}
///
/// @REST{GET /_api/graph/@FA{graph-name}}
///
////////////////////////////////////////////////////////////////////////////////
///
/// Returns an object with an attribute @LIT{graph} containing a
/// list of all graph properties.
//
/// @EXAMPLES
///
/// get graph by name
///
/// @verbinclude api-graph-get-graph
///
////////////////////////////////////////////////////////////////////////////////

function get_graph_graph (req, res) {
  try {
    var g = graph_by_request(req);
    actions.resultOk(req, res, actions.HTTP_OK, { "graph" : g._properties} );
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a graph
///
/// @RESTHEADER{DELETE /_api/graph,delete graph}
///
/// @REST{DELETE /_api/graph/@FA{graph-name}}
///
/// Deletes graph, edges and vertices
///
/// @EXAMPLES
///
/// @verbinclude api-graph-delete-graph
////////////////////////////////////////////////////////////////////////////////

function delete_graph_graph (req, res) {
  try {
    var g = graph_by_request(req);
    g.drop();
    actions.resultOk(req, res, actions.HTTP_OK, { "deleted" : true} );
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
  }
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
/// @brief creates a blueprint graph vertex
///
/// @RESTHEADER{POST /_api/graph/@FA{graph-name}/vertex,create vertex}
///
/// @REST{POST /_api/graph/@FA{graph-name}/vertex}
///
////////////////////////////////////////////////////////////////////////////////
///
/// Creates a vertex in a graph.
///
/// The call expects a JSON hash array as body with the vertex properties:
///
/// - @LIT{_key}: The name of the vertex (optional).
/// - further optional attributes.
///
/// Returns an object with an attribute @LIT{vertex} containing a
/// list of all vertex properties.
///
/// @EXAMPLES
///
/// @verbinclude api-graph-create-vertex
////////////////////////////////////////////////////////////////////////////////

function post_graph_vertex (req, res, g) {
  try {
    var json = actions.getJsonBody(req, res);
    var id;

    if (json) {
      id = json._key;
    }

    var v = g.addVertex(id, json);

    if (v === null || v._properties === undefined) {
      throw "could not create vertex";
    }

    actions.resultOk(req, res, actions.HTTP_CREATED, { "vertex" : v._properties } );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the vertex properties
///
/// @RESTHEADER{GET /_api/graph/@FA{graph-name}/vertex,get vertex}
///
/// @REST{GET /_api/graph/@FA{graph-name}/vertex/@FA{vertex-name}}
///
/// Returns an object with an attribute @LIT{vertex} containing a
/// list of all vertex properties.
///
/// @EXAMPLES
///
/// get vertex properties by name
///
/// @verbinclude api-graph-get-vertex
///
////////////////////////////////////////////////////////////////////////////////

function get_graph_vertex (req, res, g) {
  try {
    var v = vertex_by_request(req, g);
    actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : v._properties} );
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete vertex
///
/// @RESTHEADER{DELETE /_api/graph/@FA{graph-name}/vertex,delete vertex}
///
/// @REST{DELETE /_api/graph/@FA{graph-name}/vertex/@FA{vertex-name}}
///
/// Deletes vertex and all in and out edges of the vertex
///
/// @EXAMPLES
///
/// @verbinclude api-graph-delete-vertex
////////////////////////////////////////////////////////////////////////////////

function delete_graph_vertex (req, res, g) {
  try {
    var v = vertex_by_request(req, g);
    g.removeVertex(v);

    actions.resultOk(req, res, actions.HTTP_OK, { "deleted" : true} );
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a vertex
///
/// @RESTHEADER{PUT /_api/graph/@FA{graph-name}/vertex,update vertex}
///
/// @REST{PUT /_api/graph/@FA{graph-name}/vertex/@FA{vertex-name}}
///
////////////////////////////////////////////////////////////////////////////////
///
/// Replaces the vertex properties.
///
/// The call expects a JSON hash array as body with the new vertex properties.
///
/// Returns an object with an attribute @LIT{vertex} containing a
/// list of all vertex properties.
///
/// @EXAMPLES
///
/// @verbinclude api-graph-change-vertex
////////////////////////////////////////////////////////////////////////////////

function put_graph_vertex (req, res, g) {
  var v = null;

  try {
    v = vertex_by_request(req, g);
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, err);
    return;
  }

  try {
    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX);

    if (json === undefined) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, "error in request body");
      return;
    }

    var shallow = json.shallowCopy;

    var id2 = g._vertices.replace(v._properties, shallow);
    var result = g._vertices.document(id2);

    actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : result} );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a vertex
///
/// @RESTHEADER{PATCH /_api/graph/@FA{graph-name}/vertex,update vertex}
///
/// @REST{PATCH /_api/graph/@FA{graph-name}/vertex/@FA{vertex-name}}
///
////////////////////////////////////////////////////////////////////////////////
///
/// Partially updates the vertex properties.
///
/// The call expects a JSON hash array as body with the properties to patch.
///
/// Setting an attribute value to @LIT{null} in the patch document will cause a value 
/// of @LIT{null} be saved for the attribute by default. If the intention is to 
/// delete existing attributes with the patch command, the URL parameter 
/// @LIT{keepNull} can be used with a value of @LIT{false}. 
/// This will modify the behavior of the patch command to remove any attributes 
/// from the existing document that are contained in the patch document 
/// with an attribute value of @LIT{null}.
//
/// Returns an object with an attribute @LIT{vertex} containing a
/// list of all vertex properties.
///
/// @EXAMPLES
///
/// @verbinclude api-graph-changep-vertex
////////////////////////////////////////////////////////////////////////////////

function patch_graph_vertex (req, res, g) {
  var v = null;

  try {
    v = vertex_by_request(req, g);
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, err);
    return;
  }

  try {
    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX);

    if (json === undefined) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, "error in request body");
      return;
    }
    
    var keepNull = req.parameters['keepNull'];
    if (keepNull != undefined || keepNull == "false") {
      keepNull = false;
    }
    else {
      keepNull = true;
    }

    var id2 = g._vertices.update(v._properties, json, true, keepNull);
    var result = g._vertices.document(id2);

    actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : result } );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, err);
  }
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
/// @RESTHEADER{POST /_api/graph/@FA{graph-name}/vertices,get vertices}
///
/// @REST{POST /_api/graph/@FA{graph-name}/vertices}
///
/// Returns a cursor.
///
/// The call expects a JSON hash array as body to filter the result:
///
/// - @LIT{batchSize}: the batch size of the returned cursor
/// - @LIT{limit}: limit the result size
/// - @LIT{count}: return the total number of results (default "false")
/// - @LIT{filter}: a optional filter
///
/// The attributes of filter
/// - @LIT{properties}: filter by an array of vertex properties
///
/// The attributes of a property filter
/// - @LIT{key}: filter the result vertices by a key value pair
/// - @LIT{value}: the value of the @LIT{key}
/// - @LIT{compare}: a compare operator
//
/// @EXAMPLES
///
/// Select all vertices
///
/// @verbinclude api-graph-get-vertices
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
    if (cursor instanceof ArangoError) {
      actions.resultBad(req, res, cursor.errorNum, cursor.errorMessage);
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
/// @RESTHEADER{POST /_api/graph/@FA{graph-name}/vertices/@FA{vertice-name},get vertices}
///
/// @REST{POST /_api/graph/@FA{graph-name}/vertices/@FA{vertice-name}}
///
/// Returns a cursor.
///
/// The call expects a JSON hash array as body to filter the result:
///
/// - @LIT{batchSize}: the batch size of the returned cursor
/// - @LIT{limit}: limit the result size
/// - @LIT{count}: return the total number of results (default "false")
/// - @LIT{filter}: a optional filter
///
/// The attributes of filter
/// - @LIT{direction}: Filter for inbound (value "in") or outbound (value "out")
///   neighbors. Default value is "any".
/// - @LIT{labels}: filter by an array of edge labels (empty array means no restriction)
/// - @LIT{properties}: filter neighbors by an array of edge properties
///
/// The attributes of a property filter
/// - @LIT{key}: filter the result vertices by a key value pair
/// - @LIT{value}: the value of the @LIT{key}
/// - @LIT{compare}: a compare operator
///
/// @EXAMPLES
///
/// Select all vertices
///
/// @verbinclude api-graph-get-vertex-vertices
///
/// Select vertices by direction and property filter
///
/// @verbinclude api-graph-get-vertex-vertices2
///
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
    if (cursor instanceof ArangoError) {
      actions.resultBad(req, res, cursor.errorNum, cursor.errorMessage);
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
/// @RESTHEADER{POST /_api/graph/@FA{graph-name}/edge,create edge}
///
/// @REST{POST /_api/graph/@FA{graph-name}/edge}
///
////////////////////////////////////////////////////////////////////////////////
///
/// Creates an edge in a graph.
///
/// The call expects a JSON hash array as body with the edge properties:
///
/// - @LIT{_key}: The name of the edge.
/// - @LIT{_from}: The name of the from vertex.
/// - @LIT{_to}: The name of the to vertex.
/// - @LIT{$label}: A label for the edge (optional).
/// - further optional attributes.
///
/// Returns an object with an attribute @LIT{edge} containing the
/// list of all edge properties.
///
/// @EXAMPLES
///
/// @verbinclude api-graph-create-edge
////////////////////////////////////////////////////////////////////////////////

function post_graph_edge (req, res, g) {
  try {
    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_EDGE);

    if (json === undefined) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_EDGE, "error in request body");
      return;
    }

    var id = json._key;
    var out = g.getVertex(json._from);
    var ine = g.getVertex(json._to);
    var label = json.$label;

    var e = g.addEdge(out, ine, id, label, json);

    if (e === null || e._properties === undefined) {
      throw "could not create edge";
    }

    actions.resultOk(req, res, actions.HTTP_CREATED, { "edge" : e._properties } );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get edge properties
///
/// @RESTHEADER{GET /_api/graph/@FA{graph-name}/edge,get edge}
///
/// @REST{GET /_api/graph/@FA{graph-name}/edge/@FA{edge-name}}
///
/// Returns an object with an attribute @LIT{edge} containing a
/// list of all edge properties.
///
/// @EXAMPLES
///
/// @verbinclude api-graph-get-edge
////////////////////////////////////////////////////////////////////////////////

function get_graph_edge (req, res, g) {
  try {
    var e = edge_by_request(req, g);

    actions.resultOk(req, res, actions.HTTP_OK, { "edge" : e._properties} );
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an edge
///
/// @RESTHEADER{DELETE /_api/graph/@FA{graph-name}/edge,delete edge}
///
/// @REST{DELETE /_api/graph/@FA{graph-name}/edge/@FA{edge-name}}
///
/// Deletes an edge of the graph
///
/// @EXAMPLES
///
/// @verbinclude api-graph-delete-edge
////////////////////////////////////////////////////////////////////////////////

function delete_graph_edge (req, res, g) {
  try {
    var e = edge_by_request(req, g);
    g.removeEdge(e);
    actions.resultOk(req, res, actions.HTTP_OK, { "deleted" : true} );
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_INVALID_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an edge
///
/// @RESTHEADER{PUT /_api/graph/@FA{graph-name}/edge,update edge}
///
/// @REST{PUT /_api/graph/@FA{graph-name}/edge/@FA{edge-name}}
///
////////////////////////////////////////////////////////////////////////////////
///
/// Replaces the optional edge properties.
///
/// The call expects a JSON hash array as body with the new edge properties.
///
/// Returns an object with an attribute @LIT{edge} containing a
/// list of all edge properties.
///
/// @EXAMPLES
///
/// @verbinclude api-graph-change-edge
////////////////////////////////////////////////////////////////////////////////

function put_graph_edge (req, res, g) {
  var e = null;

  try {
    e = edge_by_request(req, g);
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, err);
    return;
  }
  
  try {
    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE);

    if (json === undefined) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, "error in request body");
      return;
    }

    var shallow = json.shallowCopy;
    shallow.$label = e._properties.$label;

    var id2 = g._edges.replace(e._properties, shallow);
    var result = g._edges.document(id2);

    actions.resultOk(req, res, actions.HTTP_OK, { "edge" : result} );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an edge
///
/// @RESTHEADER{PATCH /_api/graph/@FA{graph-name}/edge,update edge}
///
/// @REST{PATCH /_api/graph/@FA{graph-name}/edge/@FA{vertex-name}}
///
////////////////////////////////////////////////////////////////////////////////
///
/// Partially updates the edge properties.
///
/// The call expects a JSON hash array as body with the properties to patch.
///
/// Setting an attribute value to @LIT{null} in the patch document will cause a value 
/// of @LIT{null} be saved for the attribute by default. If the intention is to 
/// delete existing attributes with the patch command, the URL parameter 
/// @LIT{keepNull} can be used with a value of @LIT{false}. 
/// This will modify the behavior of the patch command to remove any attributes 
/// from the existing document that are contained in the patch document 
/// with an attribute value of @LIT{null}.
//
/// Returns an object with an attribute @LIT{edge} containing a
/// list of all edge properties.
///
/// @EXAMPLES
///
/// @verbinclude api-graph-changep-edge
////////////////////////////////////////////////////////////////////////////////

function patch_graph_edge (req, res, g) {
  var e = null;

  try {
    e = edge_by_request(req, g);
  }
  catch (err) {
    actions.resultNotFound(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, err);
    return;
  }
  
  try {
    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE);

    if (json === undefined) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, "error in request body");
      return;
    }
    
    var keepNull = req.parameters['keepNull'];
    if (keepNull != undefined || keepNull == "false") {
      keepNull = false;
    }
    else {
      keepNull = true;
    }
    
    var shallow = json.shallowCopy;
    shallow.$label = e._properties.$label;

    var id2 = g._edges.update(e._properties, shallow, true, keepNull);
    var result = g._edges.document(id2);

    actions.resultOk(req, res, actions.HTTP_OK, { "edge" : result} );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get edges of a graph
///
/// @RESTHEADER{POST /_api/graph/@FA{graph-name}/edges,get edges}
///
/// @REST{POST /_api/graph/@FA{graph-name}/edges}
///
/// Returns a cursor.
///
/// The call expects a JSON hash array as body to filter the result:
///
/// - @LIT{batchSize}: the batch size of the returned cursor
/// - @LIT{limit}: limit the result size
/// - @LIT{count}: return the total number of results (default "false")
/// - @LIT{filter}: a optional filter
///
/// The attributes of filter
/// - @LIT{labels}: filter by an array of edge labels
/// - @LIT{properties}: filter by an array of edge properties
///
/// The attributes of a property filter
/// - @LIT{key}: filter the result edges by a key value pair
/// - @LIT{value}: the value of the @LIT{key}
/// - @LIT{compare}: a compare operator
///
/// @EXAMPLES
///
/// Select all edges
///
/// @verbinclude api-graph-get-edges
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
    if (cursor instanceof ArangoError) {
      actions.resultBad(req, res, cursor.errorNum, cursor.errorMessage);
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
/// @RESTHEADER{POST /_api/graph/@FA{graph-name}/edges/@FA{vertex-name},get edges}
///
/// @REST{POST /_api/graph/@FA{graph-name}/edges/@FA{vertex-name}}
///
/// Returns a cursor.
///
/// The call expects a JSON hash array as body to filter the result:
///
/// - @LIT{batchSize}: the batch size of the returned cursor
/// - @LIT{limit}: limit the result size
/// - @LIT{count}: return the total number of results (default "false")
/// - @LIT{filter}: a optional filter
///
/// The attributes of filter
/// - @LIT{direction}: Filter for inbound (value "in") or outbound (value "out")
///   neighbors. Default value is "any".
/// - @LIT{labels}: filter by an array of edge labels
/// - @LIT{properties}: filter neighbors by an array of properties
///
/// The attributes of a property filter
/// - @LIT{key}: filter the result vertices by a key value pair
/// - @LIT{value}: the value of the @LIT{key}
/// - @LIT{compare}: a compare operator
///
/// @EXAMPLES
///
/// Select all edges
///
/// @verbinclude api-graph-get-vertex-edges
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
    if (cursor instanceof ArangoError) {
      actions.resultBad(req, res, cursor.errorNum, cursor.errorMessage);
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
    catch (err) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
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
    catch (err) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
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
    catch (err) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
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
    // PATCh /_api/graph/<key>/...
    try {
      var g = graph_by_request(req);

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
    catch (err) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
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
    catch (err) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
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
      actions.resultException(req, res, err);
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
