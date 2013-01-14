////////////////////////////////////////////////////////////////////////////////
/// @brief blueprints graph api
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

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var graph = require("org/arangodb/graph");

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

var BLUEPRINTS_URL = "_api/blueprints";

////////////////////////////////////////////////////////////////////////////////
/// @brief context
////////////////////////////////////////////////////////////////////////////////

var BLUEPRINTS_CONTEXT = "api";

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

function blueprints_graph_by_request_parameter (req) {
  var name = req.parameters['graph'];    

  if (name == undefined) {
    throw "missing graph name";
  }

  return new graph.Graph(name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get graph by request parameter (throws exception)
////////////////////////////////////////////////////////////////////////////////

function blueprints_graph_by_request (req) {
  var id = req.suffix[0];

  if (req.suffix.length > 1) {
    id += "/" + req.suffix[1];
  }

  var g = new graph.Graph(id);

  if (g._properties == null) {
    throw "no graph found for: " + id;
  }

  return g;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get vertex by request (throws exception)
////////////////////////////////////////////////////////////////////////////////

function blueprints_vertex_by_request (graph, req) {
  var id = req.suffix[0];

  if (req.suffix.length > 1) {
    id += "/" + req.suffix[1];
  }

  var vertex = graph.getVertex(id);

  if (vertex == undefined || vertex._properties == undefined) {
    throw "no vertex found for: " + id;
  }

  return vertex;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get edge by request (throws exception)
////////////////////////////////////////////////////////////////////////////////

function blueprints_edge_by_request (graph, req) {
  var id = req.suffix[0];
  if (req.suffix.length > 1) {
    id += "/" + req.suffix[1];
  }

  var edge = graph.getEdge(id);

  if (edge == undefined || edge._properties == undefined) {
    throw "no edge found for: " + id;
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
/// @brief create a blueprint graph
///
/// @RESTHEADER{POST /_api/blueprints/graph,create graph}
///
/// @REST{POST /_api/blueprints/graph}
///
////////////////////////////////////////////////////////////////////////////////
///
/// Creates a new graph.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - @LIT{name}: The identifier or name of the new graph.
/// - @LIT{verticesName}: The name of the vertices collection.
/// - @LIT{edgesName}: The name of the egge collection.
///
/// Returns an object with an attribute @LIT{graph} containing a 
/// list of all graph properties.
///
/// @EXAMPLES
///
/// @verbinclude api-blueprints-create-graph
////////////////////////////////////////////////////////////////////////////////

function POST_blueprints_graph (req, res) {
  try {    
    var json = actions.getJsonBody(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CREATE_GRAPH);

    if (json === undefined) {
      return;
    }

    var name = json['name'];
    var vertices = json['verticesName'];
    var edges = json['edgesName'];

    var g = new graph.Graph(name, vertices, edges);

    if (g._properties == null) {
      throw "no properties of graph found";
    }

    actions.resultOk(req, res, actions.HTTP_OK, { "graph" : g._properties} );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CREATE_GRAPH, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get blueprint graph properties
///
/// @RESTHEADER{GET /_api/blueprints/graph,get graph properties}
///
/// @REST{GET /_api/blueprints/graph/@FA{graph-identifier}}
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
/// @verbinclude api-blueprints-get-graph
/// 
/// get graph by document id
///
/// @verbinclude api-blueprints-get-graph-by-id
////////////////////////////////////////////////////////////////////////////////

function GET_blueprints_graph (req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_GRAPH, "graph not found");
    return;
  }

  try {    
    var g = blueprints_graph_by_request(req);

    actions.resultOk(req, res, actions.HTTP_OK, { "graph" : g._properties} );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_GRAPH, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a blueprint graph
///
/// @RESTHEADER{DELETE /_api/blueprints/graph,delete graph}
///
/// @REST{DELETE /_api/blueprints/graph/@FA{graph-identifier}}
///
/// Deletes graph, edges and vertices
///
/// @EXAMPLES
///
/// @verbinclude api-blueprints-delete-graph
////////////////////////////////////////////////////////////////////////////////

function DELETE_blueprints_graph (req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_GRAPH, "graph not found");
    return;
  }

  try {    
    var g = blueprints_graph_by_request(req);

    g.drop();

    actions.resultOk(req, res, actions.HTTP_OK, { "deleted" : true} );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_GRAPH, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : BLUEPRINTS_URL + "/graph",
  context : BLUEPRINTS_CONTEXT,

  callback : function (req, res) {
    try {
      switch (req.requestType) {
	case (actions.POST) :
	  POST_blueprints_graph(req, res); 
	  break;

	case (actions.GET) :
	  GET_blueprints_graph(req, res); 
	  break;

	case (actions.DELETE) :
	  DELETE_blueprints_graph(req, res); 
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
// --SECTION--                                                  vertex functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a blueprint graph vertex
///
/// @RESTHEADER{POST /_api/blueprints/vertex,create vertex}
///
/// @REST{POST /_api/blueprints/vertex?graph=@FA{graph-identifier}}
///
////////////////////////////////////////////////////////////////////////////////
///
/// Creates a vertex in a graph.
///
/// The call expects a JSON hash array as body with the vertex properties:
///
/// - @LIT{$id}: The identifier or name of the vertex (optional).
/// - further optional attributes.
///
/// Returns an object with an attribute @LIT{vertex} containing a
/// list of all vertex properties.
///
/// @EXAMPLES
///
/// @verbinclude api-blueprints-create-vertex
////////////////////////////////////////////////////////////////////////////////

function POST_blueprints_vertex (req, res) {
  try {
    var g = blueprints_graph_by_request_parameter(req);
    var json = actions.getJsonBody(req, res);
    var id = undefined;

    if (json) {
      id = json["$id"];
    }

    var v = g.addVertex(id, json);      

    if (v == undefined || v._properties == undefined) {
      throw "could not create vertex";
    }

    actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : v._properties } );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CREATE_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get vertex properties
///
/// @RESTHEADER{GET /_api/blueprints/vertex,get vertex}
///
/// @REST{GET /_api/blueprints/vertex/@FA{vertex-identifier}?graph=@FA{graph-identifier}}
///
/// Returns an object with an attribute @LIT{vertex} containing a
/// list of all vertex properties.
///
/// @EXAMPLES
///
/// get vertex properties by $id
///
/// @verbinclude api-blueprints-get-vertex
///
/// get vertex properties by document id
///
/// @verbinclude api-blueprints-get-vertex-by-id
////////////////////////////////////////////////////////////////////////////////

function GET_blueprints_vertex (req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, "vertex not found");
    return;
  }

  try {    
    var g = blueprints_graph_by_request_parameter(req);
    var v = blueprints_vertex_by_request(g, req);

    actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : v._properties} );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete vertex
///
/// @RESTHEADER{DELETE /_api/blueprints/vertex,delete vertex}
///
/// @REST{DELETE /_api/blueprints/vertex/@FA{vertex-identifier}?graph=@FA{graph-identifier}}
///
/// Deletes vertex and all in and out edges of the vertex
///
/// @EXAMPLES
///
/// @verbinclude api-blueprints-delete-vertex
////////////////////////////////////////////////////////////////////////////////

function DELETE_blueprints_vertex (req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, "vertex not found");
    return;
  }

  try {    
    var g = blueprints_graph_by_request_parameter(req);
    var v = blueprints_vertex_by_request(g, req);

    g.removeVertex(v);

    actions.resultOk(req, res, actions.HTTP_OK, { "deleted" : true} );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a vertex
///
/// @RESTHEADER{PUT /_api/blueprints/vertex,update vertex}
///
/// @REST{PUT /_api/blueprints/vertex/@FA{vertex-identifier}?graph=@FA{graph-identifier}}
///
////////////////////////////////////////////////////////////////////////////////
///
/// Replaces the vertex properties.
///
/// The call expects a JSON hash array as body with the new vertex properties:
///
/// - @LIT{$id}: The identifier or name of the vertex (not changeable).
///
/// - further optional attributes.
///
/// Returns an object with an attribute @LIT{vertex} containing a
/// list of all vertex properties.
///
/// @EXAMPLES
///
/// @verbinclude api-blueprints-change-vertex
////////////////////////////////////////////////////////////////////////////////

function PUT_blueprints_vertex (req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, "vertex not found");
    return;
  }

  try {    
    var g = blueprints_graph_by_request_parameter(req);
    var v = blueprints_vertex_by_request(g, req);

    var json = actions.getJsonBody(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX);

    if (json === undefined) {
      return;
    }

    var shallow = json.shallowCopy;
    shallow.$id = v._properties.$id;

    var id2 = g._vertices.replace(v._properties, shallow);
    var result = g._vertices.document(id2);

    actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : result} );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : BLUEPRINTS_URL + "/vertex",
  context : BLUEPRINTS_CONTEXT,

  callback : function (req, res) {
    try {
      switch (req.requestType) {
	case (actions.POST) :
	  POST_blueprints_vertex(req, res); 
	  break;

	case (actions.DELETE) :
	  DELETE_blueprints_vertex(req, res); 
	  break;

	case (actions.GET) :
	  GET_blueprints_vertex(req, res); 
	  break;

	case (actions.PUT) :
	  PUT_blueprints_vertex(req, res); 
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
// --SECTION--                                                vertices functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get graph vertices
///
/// @RESTHEADER{POST /_api/blueprints/vertices,get vertices}
///
/// @REST{POST /_api/blueprints/vertices?graph=@FA{graph-identifier}}
///
/// Returns a cursor.
///
/// The call expects a JSON hash array as body to filter the edges:
///
/// - @LIT{vertex}: the identifier or name of a vertex. This selects inbound and 
///   outbound neighbors of a vertex. If a vertex is given the edge direction
///   can be filterd by @LIT{direction} and labels of edges can be filterd by 
///   @LIT{labels}.
/// - @LIT{direction}: Filter for inbound (value "in") or outbound (value "out") 
///   neighbors. Default value is "any".
/// - @LIT{key}: filter the result vertices by a key value pair
/// - @LIT{value}: the value of the @LIT{key}
/// - @LIT{labels}: filter by an array of edge labels
/// - @LIT{batchSize}: the batch size of the returned cursor
///
/// @EXAMPLES
///
/// Select all vertices
///
/// @verbinclude api-blueprints-get-vertices
///
/// Select of all neighbors of a vertex.
///
/// @verbinclude api-blueprints-get-vertices2
///
/// Select of all outbound neighbors of a vertex.
///
/// @verbinclude api-blueprints-get-outbound-vertices
//
/// Select of all neighbors of a vertex by a edge label.
///
/// @verbinclude api-blueprints-get-vertices-label
///
/// Select of vertices by key value
///
/// @verbinclude api-blueprints-get-vertices-key
///
////////////////////////////////////////////////////////////////////////////////

function POST_blueprints_vertices (req, res) {
  if (req.suffix.length != 0) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, "vertex not found");
    return;
  }    

  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    json = {};
  }


  try {    
    var g = blueprints_graph_by_request_parameter(req);
    var selectEdge = "";
    var vertexFilter = "";
    var edgeFilter = "";
    var bindVars = {"@vertexColl" : g._properties.verticesName };          

    if (json.vertex != undefined) {
      // get neighbors
      var v = g.getVertex(json.vertex);

      if (v == undefined || v._properties == undefined) {
	actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, "vertex not found");
	return;          
      }
      selectEdge = "FOR e IN @@edgeColl";

      // get inbound neighbors
      if (json.direction == "in") {
	edgeFilter = " FILTER e._to == @id";
	vertexFilter = " FILTER e._from == v._id";
      }
      // get outbound neighbors
      else if (json.direction == "out") {
	edgeFilter = " FILTER e._from == @id";
	vertexFilter = " FILTER e._to == v._id";
      }
      // get all neighbors
      else {          
	vertexFilter = " FILTER ((e._from == @id && e._to == v._id) || (e._to == @id && e._from == v._id))";
      }

      bindVars["@edgeColl"] = g._properties.edgesName;
      bindVars["id"] = v._id;

      // filter edge labels
      if (json.labels != undefined && json.labels instanceof Array) {
	if (edgeFilter == "") { edgeFilter = " FILTER"; } else { edgeFilter += " &&";}
	edgeFilter += ' e["$label"] IN @labels';
	bindVars["labels"] = json.labels;
      }

    }

    // filter key/value pairs labels
    if (json.key != undefined) {
      // get all with key=value
      if (vertexFilter == "") { vertexFilter = " FILTER"; } else { vertexFilter += " &&";}
      vertexFilter += " v[@key] == @value";        
      bindVars["key"] = json.key;
      bindVars["value"] = json.value;
    }

    // build aql query
    var query = selectEdge + edgeFilter + " FOR v IN @@vertexColl" + vertexFilter + " RETURN v";

    var cursor = AHUACATL_RUN(query, 
			  bindVars, 
			  (json.count != undefined ? json.count : false),
			  json.batchSize, 
			  (json.batchSize == undefined));  

    // error occurred
    if (cursor instanceof ArangoError) {
      actions.resultBad(req, res, cursor.errorNum, cursor.errorMessage);
      return;
    }

    // this might dispose or persist the cursor
    actions.resultCursor(req, res, cursor, actions.HTTP_CREATED, { countRequested: json.count ? true : false });      
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : BLUEPRINTS_URL + "/vertices",
  context : BLUEPRINTS_CONTEXT,

  callback : function (req, res) {
    try {
      switch (req.requestType) {
	case (actions.POST) :
	  POST_blueprints_vertices(req, res); 
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
// --SECTION--                                                    edge functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a blueprint graph edge
///
/// @RESTHEADER{POST /_api/blueprints/edge,create edge}
///
/// @REST{POST /_api/blueprints/edge?graph=@FA{graph-identifier}}
///
////////////////////////////////////////////////////////////////////////////////
///
/// Creates an edge in a graph.
///
/// The call expects a JSON hash array as body with the edge properties:
///
/// - @LIT{$id}: The identifier or name of the edge.
/// - @LIT{_from}: The identifier or name of the from vertex.
/// - @LIT{_to}: The identifier or name of the to vertex.
/// - @LIT{$label}: A label for the edge (optional).
/// - further optional attributes.
///
/// Returns an object with an attribute @LIT{edge} containing the
/// list of all edge properties.
///
/// @EXAMPLES
///
/// @verbinclude api-blueprints-create-edge
////////////////////////////////////////////////////////////////////////////////

function POST_blueprints_edge (req, res) {
  try {
    var g = blueprints_graph_by_request_parameter(req);
    var json = actions.getJsonBody(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CREATE_EDGE);

    if (json === undefined) {
      return;
    }

    var id = json["$id"];    
    var out = g.getVertex(json["_from"]);
    var ine = g.getVertex(json["_to"]);
    var label = json["$label"];    

    var e = g.addEdge(out, ine, id, label, json);      

    if (e == undefined || e._properties == undefined) {
      throw "could not create edge";
    }

    actions.resultOk(req, res, actions.HTTP_OK, { "edge" : e._properties } );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CREATE_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get edge properties
///
/// @RESTHEADER{GET /_api/blueprints/edge,get edge}
///
/// @REST{GET /_api/blueprints/edge/@FA{edge-identifier}?graph=@FA{graph-identifier}}
///
/// Returns an object with an attribute @LIT{edge} containing a
/// list of all edge properties.
///
/// @EXAMPLES
///
/// @verbinclude api-blueprints-get-edge
////////////////////////////////////////////////////////////////////////////////

function GET_blueprints_edge (req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_EDGE, "edge not found");
    return;
  }

  try {    
    var g = blueprints_graph_by_request_parameter(req);
    var e = blueprints_edge_by_request(g, req);

    actions.resultOk(req, res, actions.HTTP_OK, { "edge" : e._properties} );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an edge
///
/// @RESTHEADER{DELETE /_api/blueprints/edge,delete edge}
///
/// @REST{DELETE /_api/blueprints/edge/@FA{edge-identifier}?graph=@FA{graph-identifier}}
///
/// Deletes an edges of the graph
///
/// @EXAMPLES
///
/// @verbinclude api-blueprints-delete-edge
////////////////////////////////////////////////////////////////////////////////

function DELETE_blueprints_edge (req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_EDGE, "edge not found");
    return;
  }

  try {    
    var g = blueprints_graph_by_request_parameter(req);
    var e = blueprints_edge_by_request(g, req);

    g.removeEdge(e);

    actions.resultOk(req, res, actions.HTTP_OK, { "deleted" : true} );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an edge
///
/// @RESTHEADER{PUT /_api/blueprints/edge,update edge}
///
/// @REST{PUT /_api/blueprints/edge/@FA{edge-identifier}?graph=@FA{graph-identifier}}
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
/// @verbinclude api-blueprints-change-edge
////////////////////////////////////////////////////////////////////////////////

function PUT_blueprints_edge(req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, "edge not found");
    return;
  }

  try {    
    var g = blueprints_graph_by_request_parameter(req);
    var e = blueprints_edge_by_request(g, req);

    var json = actions.getJsonBody(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE);

    if (json === undefined) {
      return;
    }

    var shallow = json.shallowCopy;
    shallow.$id = e._properties.$id;
    shallow.$label = e._properties.$label;

    var id2 = g._edges.replace(e._properties, shallow);
    var result = g._edges.document(id2);

    actions.resultOk(req, res, actions.HTTP_OK, { "edge" : result} );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : BLUEPRINTS_URL + "/edge",
  context : BLUEPRINTS_CONTEXT,

  callback : function (req, res) {
    try {
      switch (req.requestType) {
	case (actions.POST) :
	  POST_blueprints_edge(req, res); 
	  break;

	case (actions.GET) :
	  GET_blueprints_edge(req, res); 
	  break;

	case (actions.DELETE) :
	  DELETE_blueprints_edge(req, res); 
	  break;

	case (actions.PUT) :
	  PUT_blueprints_edge(req, res); 
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
// --SECTION--                                                   edges functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get graph edges
///
/// @RESTHEADER{POST /_api/blueprints/edges,get edges}
///
/// @REST{POST /_api/blueprints/edges?graph=@FA{graph-identifier}}
/// 
/// Returns a cursor.
/// 
/// The call expects a JSON hash array as body to filter the edges:
///
/// - @LIT{vertex}: the identifier or name of a vertex. This selects inbound and 
///   outbound edges of a vertex. If a vertex is given the edge direction
///   can be filterd by @LIT{direction}.
/// - @LIT{direction}: Filter for inbound (value "in") or outbound (value "out") 
///   edges. Default value is "any".
/// - @LIT{labels}: filter by an array of edge labels
/// - @LIT{key}: filter the by a key value pair
/// - @LIT{value}: the value of the @LIT{key}
/// - @LIT{batchSize}: the batch size of the returned cursor
///
/// @EXAMPLES
///
/// Select all edges
///
/// @verbinclude api-blueprints-get-edges
///
/// Select of all inbound and outbound edges of a vertex.
///
/// @verbinclude api-blueprints-get-edges-by-vertex
///
/// Select of all outbound edges of a vertex.
///
/// @verbinclude api-blueprints-get-out-edges-by-vertex
///
/// Select of all edges of a vertex by a label.
///
/// @verbinclude api-blueprints-get-in-edges-by-vertex
////////////////////////////////////////////////////////////////////////////////

function POST_blueprints_edges (req, res) {
  if (req.suffix.length != 0) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, "edges not found");
    return;
  }    

  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    json = {};
  }

  try {    
    var g = blueprints_graph_by_request_parameter(req);
    var filter = "";
    var bindVars = {"@edgeColl" : g._properties.edgesName};

    if (json.vertex != undefined) {
      // get edges of a vertex
      var v = g.getVertex(json.vertex);

      if (v == undefined || v._properties == undefined) {
	actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, "vertex not found");
	return;          
      }

      if (json.direction == "in") {
	filter = " FILTER e._to == @id ";
      }
      else if (json.direction == "out") {
	filter = " FILTER e._from == @id ";
      }
      else {
	filter = " FILTER (e._from == @id || e._to == @id)";
      }

      bindVars["id"] = v._id;
    }

    if (json.key != undefined) {
      // get all with key=value
      if (filter == "") {filter = " FILTER";} else {filter += " &&";}
      filter += " e[@key] == @value";
      bindVars["key"] = json.key;
      bindVars["value"] = json.value;
    }

    if (json.labels != undefined && json.labels instanceof Array) {
      // get all with $lable=value
      if (filter == "") {filter = " FILTER";} else {filter += " &&";}
      filter += ' e["$label"] IN @labels';
      bindVars["labels"] = json.labels;
    }

    var query = "FOR e IN @@edgeColl" + filter + " RETURN e";

    var cursor = AHUACATL_RUN(query, 
			  bindVars, 
			  (json.count != undefined ? json.count : false),
			  json.batchSize, 
			  (json.batchSize == undefined));  

    // error occurred
    if (cursor instanceof ArangoError) {
      actions.resultBad(req, res, cursor.errorNum, cursor.errorMessage);
      return;
    }

    // this might dispose or persist the cursor
    actions.resultCursor(req, res, cursor, actions.HTTP_CREATED, { countRequested: json.count ? true : false });      
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}
////////////////////////////////////////////////////////////////////////////////
/// @brief actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : BLUEPRINTS_URL + "/edges",
  context : BLUEPRINTS_CONTEXT,

  callback : function (req, res) {
    try {
      switch (req.requestType) {
	case (actions.POST) :
	  POST_blueprints_edges(req, res); 
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
