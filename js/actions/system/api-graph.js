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

(function() {
  var actions = require("org/arangodb/actions");
  var graph = require("org/arangodb/graph");
  var ArangoError = require("org/arangodb/arango-error").ArangoError; 

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
      
    if (g._properties == null) {
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

    if (vertex == undefined || vertex._properties == undefined) {
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

    if (edge == undefined || edge._properties == undefined) {
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

  function POST_graph_graph (req, res) {
    try {    
      var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_GRAPH);
      
      if (json === undefined) {
        return;
      }
      
      var name = json['_key'];
      var vertices = json['vertices'];
      var edges = json['edges'];
      
      var g = new graph.Graph(name, vertices, edges);
      
      if (g._properties == null) {
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

  function GET_graph_graph (req, res) {
    try {
      var g = graph_by_request(req);
      actions.resultOk(req, res, actions.HTTP_OK, { "graph" : g._properties} );
    }
    catch (err) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
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

  function DELETE_graph_graph (req, res) {
    try {    
      var g = graph_by_request(req);      
      g.drop();      
      actions.resultOk(req, res, actions.HTTP_OK, { "deleted" : true} );
    }
    catch (err) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
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

  function POST_graph_vertex (req, res, g) {
    try {
      var json = actions.getJsonBody(req, res);
      var id = undefined;

      if (json) {
        id = json["_key"];
      }

      var v = g.addVertex(id, json);      

      if (v == undefined || v._properties == undefined) {
        throw "could not create vertex";
      }

      actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : v._properties } );
    }
    catch (err) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_VERTEX, err);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief get vertex properties
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

  function GET_graph_vertex (req, res, g) {
    try {    
      var v = vertex_by_request(req, g);
      actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : v._properties} );
    }
    catch (err) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
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

  function DELETE_graph_vertex (req, res, g) {
    try {    
      var v = vertex_by_request(req, g);
      g.removeVertex(v);

      actions.resultOk(req, res, actions.HTTP_OK, { "deleted" : true} );
    }
    catch (err) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
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

  function PUT_graph_vertex (req, res, g) {
    try {    
      var v = vertex_by_request(req, g);

      var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX);

      if (json === undefined) {
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
/// @brief returns the compare operator (throws exception)
////////////////////////////////////////////////////////////////////////////////

  function process_property_compare (compare) {
    if (compare == undefined) {
      return "==";
    }
    
    switch (compare) {
      case ("==") :
        return compare;
        break;
      case ("!=") :
        return compare;
        break;
      case ("<") :
        return compare;
        break;
      case (">") :
        return compare;
        break;
      case (">=") :
        return compare;
        break;
      case ("<=") :
        return compare;
        break;
    }        
    throw "unknow compare function in property filter"
  }
  
////////////////////////////////////////////////////////////////////////////////
/// @brief fills a filter (throws exception)
////////////////////////////////////////////////////////////////////////////////
  
  function process_property_filter (data, num, property, collname) {
    if (property.key != undefined && property.value != undefined) {      
        if (data.filter == "") { data.filter = " FILTER"; } else { data.filter += " &&";}
        data.filter += " " + collname + "[@key" + num.toString() + "] " + 
              process_property_compare(property.compare) + " @value" + num.toString();
        data.bindVars["key" + num.toString()] = property.key;
        data.bindVars["value" + num.toString()] = property.value; 
        return;
    }
    
    throw "error in property filter"
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief fills a properties filter
////////////////////////////////////////////////////////////////////////////////

  function process_properties_filter (data, properties, collname) {
    if (properties instanceof Array) {
      for (var i = 0;  i < properties.length;  ++i) {            
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

  function process_labels_filter (data, labels) {
    if (labels instanceof Array) {
      // filter edge labels
      if (labels != undefined && labels instanceof Array) {
        if (data.edgeFilter == "") { data.edgeFilter = " FILTER"; } else { data.edgeFilter += " &&";}
        data.edgeFilter += ' e["$label"] IN @labels';
        data.bindVars["labels"] = labels;
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief get vertices of a graph
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

  function POST_graph_all_vertices (req, res, g) {
    var json = actions.getJsonBody(req, res);

    if (json === undefined) {
      json = {};
    }    
    
    try {    
      var data = {
        'filter' : '',
        'bindVars' : { '@vertexColl' : g._vertices.name() }
      }

      if (json.filter != undefined && json.filter.properties != undefined) {
        process_properties_filter(data, json.filter.properties, "v");        
      }
      
      // build aql query
      var query = "FOR v IN @@vertexColl" + data.filter + " RETURN v";

      var cursor = AHUACATL_RUN(query, 
                            data.bindVars, 
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
/// Select all vertices
///
/// @verbinclude api-graph-get-vertex-vertices
///
////////////////////////////////////////////////////////////////////////////////

  function POST_graph_vertex_vertices (req, res, g) {
    var json = actions.getJsonBody(req, res);

    if (json === undefined) {
      json = {};
    }    
    
    try {    
      var v = vertex_by_request(req, g);

      var data = {
        'filter' : '',
        'edgeFilter' : '',
        'bindVars' : { 
           '@vertexColl' : g._vertices.name(),
           '@edgeColl' : g._edges.name(),
           'id' : v._id
        }
      }

      var direction = "all";
      if (json.filter != undefined && json.filter.direction != undefined) {
        if (json.filter.direction == "in") {
          direction = "in";
        }
        else if (json.filter.direction == "out") {
          direction = "out";
        }
      }

      // get inbound neighbors
      if (direction == "in") {
        data.edgeFilter = "FILTER e._to == @id";
        data.filter = "FILTER e._from == v._id";
      }
      // get outbound neighbors
      else if (direction == "out") {
        data.edgeFilter = "FILTER e._from == @id";
        data.filter = "FILTER e._to == v._id";
      }
      // get all neighbors
      else {          
        data.filter = "FILTER ((e._from == @id && e._to == v._id) || (e._to == @id && e._from == v._id))";
      }

      if (json.filter != undefined && json.filter.properties != undefined) {
        process_properties_filter(data, json.filter.properties, "v");
      }
      
      if (json.filter != undefined && json.filter.labels != undefined) {
        process_labels_filter(data, json.filter.labels);
      }

      // build aql query
      var query = "FOR e IN @@edgeColl " + data.edgeFilter + " FOR v IN @@vertexColl " + data.filter + " RETURN v";

      var cursor = AHUACATL_RUN(query, 
                            data.bindVars, 
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

  function POST_graph_edge (req, res, g) {
    try {
      var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_EDGE);

      if (json === undefined) {
        return;
      }

      var id = json["_key"];    
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

  function GET_graph_edge (req, res, g) {
    try {    
      var e = edge_by_request(req, g);

      actions.resultOk(req, res, actions.HTTP_OK, { "edge" : e._properties} );
    }
    catch (err) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_EDGE, err);
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

  function DELETE_graph_edge (req, res, g) {
    try {    
      var e = edge_by_request(req, g);
      g.removeEdge(e);
      actions.resultOk(req, res, actions.HTTP_OK, { "deleted" : true} );
    }
    catch (err) {
      actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_EDGE, err);
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

  function PUT_graph_edge(req, res, g) {
    try {    
      var e = edge_by_request(req, g);

      var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE);

      if (json === undefined) {
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

  function POST_graph_all_edges (req, res, g) {
    var json = actions.getJsonBody(req, res);

    if (json === undefined) {
      json = {};
    }
        
    try {    
      
      var data = {
        'filter' : '',
        'edgeFilter' : '',
        'bindVars' : { 
           '@edgeColl' : g._edges.name()
        }
      }
      
      if (json.filter != undefined && json.filter.properties != undefined) {
        process_properties_filter(data, json.filter.properties, "e");
        data.edgeFilter = data.filter;
      }
      
      if (json.filter != undefined && json.filter.labels != undefined) {
        process_labels_filter(data, json.filter.labels);
      }

      var query = "FOR e IN @@edgeColl" + data.edgeFilter + " RETURN e";

      var cursor = AHUACATL_RUN(query, 
                            data.bindVars, 
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

  function POST_graph_vertex_edges (req, res, g) {
    var json = actions.getJsonBody(req, res);

    if (json === undefined) {
      json = {};
    }
        
    try {    
      var v = vertex_by_request(req, g);

      var data = {
        'filter' : '',
        'edgeFilter' : '',
        'bindVars' : { 
           '@edgeColl' : g._edges.name(),
           'id' : v._id
        }
      }

      var direction = "all";
      if (json.filter != undefined && json.filter.direction != undefined) {
        if (json.filter.direction == "in") {
          direction = "in";
        }
        else if (json.filter.direction == "out") {
          direction = "out";
        }
      }

      // get inbound neighbors
      if (direction == "in") {
        data.edgeFilter = "FILTER e._to == @id";
      }
      // get outbound neighbors
      else if (direction == "out") {
        data.edgeFilter = "FILTER e._from == @id";
      }
      // get all neighbors
      else {          
        data.edgeFilter = "FILTER (e._from == @id || e._to == @id)";
      }

      if (json.filter != undefined && json.filter.properties != undefined) {
        data.filter = data.edgeFilter;
        process_properties_filter(data, json.filter.properties, "e");
        data.edgeFilter = data.filter;
      }
      
      if (json.filter != undefined && json.filter.labels != undefined) {
        process_labels_filter(data, json.filter.labels);
      }

      var query = "FOR e IN @@edgeColl " + data.edgeFilter + " RETURN e";

      var cursor = AHUACATL_RUN(query, 
                            data.bindVars, 
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
      actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief handle POST /_api/graph/<...>/vertices
////////////////////////////////////////////////////////////////////////////////

  function POST_graph_vertices (req, res, g) {
    if (req.suffix.length > 2) {
      POST_graph_vertex_vertices (req, res, g);
    }
    else {
      POST_graph_all_vertices (req, res, g);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief handle POST /_api/graph/<...>/edges
////////////////////////////////////////////////////////////////////////////////

  function POST_graph_edges (req, res, g) {
    if (req.suffix.length > 2) {
      POST_graph_vertex_edges (req, res, g);
    }
    else {
      POST_graph_all_edges (req, res, g);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief handle post requests
////////////////////////////////////////////////////////////////////////////////
  
function POST_graph(req, res) { 
  if (req.suffix.length == 0) {
    // POST /_api/graph
    POST_graph_graph(req, res);
  }
  else if (req.suffix.length > 1) {
    // POST /_api/graph/<key>/...
    try {
      var g = graph_by_request(req);
      
      switch (req.suffix[1]) {
        case ("vertex") :
          POST_graph_vertex(req, res, g); 
          break;

        case ("edge") :
          POST_graph_edge(req, res, g); 
          break;

        case ("vertices") :
          POST_graph_vertices(req, res, g); 
          break;

        case ("edges") :
          POST_graph_edges(req, res, g); 
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
  
function GET_graph(req, res) {
  if (req.suffix.length == 0) {
    // GET /_api/graph
    actions.resultUnsupported(req, res);
  }
  else if (req.suffix.length == 1) {
    // GET /_api/graph/<key>
    GET_graph_graph(req, res); 
  }
  else {
    // GET /_api/graph/<key>/...
    try {
      var g = graph_by_request(req);
      
      switch (req.suffix[1]) {
        case ("vertex") :
          GET_graph_vertex(req, res, g); 
          break;

        case ("edge") :
          GET_graph_edge(req, res, g); 
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
  
function PUT_graph(req, res) {
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
          PUT_graph_vertex(req, res, g);
          break;

        case ("edge") :
          PUT_graph_edge(req, res, g); 
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
//
////////////////////////////////////////////////////////////////////////////////
/// @brief handle delete requests
////////////////////////////////////////////////////////////////////////////////
  
function DELETE_graph(req, res) {
  if (req.suffix.length == 0) {
    // DELETE /_api/graph
    actions.resultUnsupported(req, res);
  }
  else if (req.suffix.length == 1) {
    // DELETE /_api/graph/<key>
    DELETE_graph_graph(req, res); 
  }
  else {
    // DELETE /_api/graph/<key>/...
    try {
      var g = graph_by_request(req);
      
      switch (req.suffix[1]) {
        case ("vertex") :
          DELETE_graph_vertex(req, res, g); 
          break;

        case ("edge") :
          DELETE_graph_edge(req, res, g); 
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
            POST_graph(req, res); 
            break;

          case (actions.DELETE) :
            DELETE_graph(req, res); 
            break;

          case (actions.GET) :
            GET_graph(req, res); 
            break;

          case (actions.PUT) :
            PUT_graph(req, res); 
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

})();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
