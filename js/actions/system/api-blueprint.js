////////////////////////////////////////////////////////////////////////////////
/// @brief blueprint graph api
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

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

var actions = require("actions");
var graph = require("graph");

var MY_URL = "_api/blueprint";
var MY_CONTEXT = "api";

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

shallowCopy = function (props) {
  var shallow,
    key;

  shallow = {};

  for (key in props) {
    if (props.hasOwnProperty(key) && key[0] !== '_' && key[0] !== '$') {
      shallow[key] = props[key];
    }
  }

  return shallow;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief create graph
///
/// @REST{POST /_api/blueprint/graph}
/// {"name":"...","verticesName":"...","edgesName":"..."}
///
////////////////////////////////////////////////////////////////////////////////

function postGraph(req, res) {
  try {    
    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_GRAPH);
      
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
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_GRAPH, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get graph
///
/// @REST{GET /_api/blueprint/graph/@FA{graph-identifier}}
///
////////////////////////////////////////////////////////////////////////////////

function getGraph(req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, "graph not found");
    return;
  }

  try {    
    var id = req.suffix[0];
    if (req.suffix.length > 1) {
      id += "/" + req.suffix[1];
    }
    
    var g = new graph.Graph(id);

    if (g._properties == null) {
      throw "no properties of graph found";
    }
    
    actions.resultOk(req, res, actions.HTTP_OK, { "graph" : g._properties} );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete graph
///
/// @REST{DELETE /_api/blueprint/graph/@FA{graph-identifier}}
///
////////////////////////////////////////////////////////////////////////////////

function deleteGraph(req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, "graph not found");
    return;
  }

  try {    
    var id = req.suffix[0];
    if (req.suffix.length > 1) {
      id += "/" + req.suffix[1];
    }
    
    var g = new graph.Graph(id);

    if (g._properties == null) {
      throw "no properties of graph found";
    }
    
    actions.resultOk(req, res, actions.HTTP_OK, { "graph" : g._properties} );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : MY_URL + "/graph",
  context : MY_CONTEXT,

  callback : function (req, res) {
    switch (req.requestType) {
      case (actions.POST) :
        postGraph(req, res); 
        break;

      case (actions.GET) :
        getGraph(req, res); 
        break;

      case (actions.DELETE) :
        deleteGraph(req, res); 
        break;

      default:
        actions.resultUnsupported(req, res);
    }
  }
});


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create vertex
///
/// @REST{POST /_api/blueprint/vertex?graph=@FA{graph-identifier}}
/// {"$id":"...","key1":"...","key2":"..."}
///
////////////////////////////////////////////////////////////////////////////////

function postVertex(req, res) {
  try {
    // name/id of the graph
    var name = req.parameters['graph'];    
    if (name == undefined) {
      throw "missing graph name";
    }
    var g = new graph.Graph(name);

    var json = actions.getJsonBody(req, res);
    var id = undefined;
    
    if (json) {
      id = json["$id"];
    }

    var v = g.addVertex(id, body);      

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
/// @brief get vertex
///
/// @REST{GET /_api/blueprint/vertex/@FA{vertex-identifier}?graph=@FA{graph-identifier}}
///
////////////////////////////////////////////////////////////////////////////////

function getVertex(req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, "vertex not found");
    return;
  }

  try {    
    // name/id of the graph
    var name = req.parameters['graph'];    
    if (name == undefined) {
      throw "missing graph name";
    }    
    var g = new graph.Graph(name);

    var id = req.suffix[0];
    if (req.suffix.length > 1) {
      id += "/" + req.suffix[1];
    }
    
    var v = g.getVertex(id);

    if (v == undefined || v._properties == undefined) {
      throw "no vertex found for: " + id;
    }
    
    actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : v._properties} );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete vertex
///
/// @REST{DELETE /_api/blueprint/vertex/@FA{vertex-identifier}?graph=@FA{graph-identifier}}
///
////////////////////////////////////////////////////////////////////////////////

function deleteVertex(req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, "vertex not found");
    return;
  }

  try {    
    // name/id of the graph
    var name = req.parameters['graph'];    
    if (name == undefined) {
      throw "missing graph name";
    }
    var g = new graph.Graph(name);

    var id = req.suffix[0];
    if (req.suffix.length > 1) {
      id += "/" + req.suffix[1];
    }
    
    var v = g.getVertex(id);

    if (v == undefined || v._properties == undefined) {
      throw "no vertex found for: " + id;
    }
    
    g.removeVertex(v);
    
    actions.resultOk(req, res, actions.HTTP_OK, { "deleted" : true} );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief change vertex
///
/// @REST{PUT /_api/blueprint/vertex/@FA{vertex-identifier}?graph=@FA{graph-identifier}}
/// {"$id":"...","key1":"...","key2":"..."}
///
////////////////////////////////////////////////////////////////////////////////

function putVertex(req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, "vertex not found");
    return;
  }

  try {    
    // name/id of the graph
    var name = req.parameters['graph'];    
    if (name == undefined) {
      throw "missing graph name";
    }
    var g = new graph.Graph(name);

    var id = req.suffix[0];
    if (req.suffix.length > 1) {
      id += "/" + req.suffix[1];
    }
    
    var v = g.getVertex(id);

    if (v == undefined || v._properties == undefined) {
      throw "no vertex found for: " + id;
    }
    
    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX);
      
    if (json === undefined) {
      return;
    }

    var shallow = shallowCopy(json);
    shallow.$id = v._properties.$id;

    var id2 = g._vertices.replace(v._properties, shallow);
    var result = g._vertices.document(id2);

    actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : result} );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : MY_URL + "/vertex",
  context : MY_CONTEXT,

  callback : function (req, res) {
    switch (req.requestType) {
      case (actions.POST) :
        postVertex(req, res); 
        break;

      case (actions.GET) :
        getVertex(req, res); 
        break;

      case (actions.PUT) :
        putVertex(req, res); 
        break;

      default:
        actions.resultUnsupported(req, res);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get vertices
///
/// @REST{GET /_api/blueprint/vertices?graph=@FA{graph-identifier}}
///
////////////////////////////////////////////////////////////////////////////////

function getVertices(req, res) {
  try {    
    // name/id of the graph
    var name = req.parameters['graph'];    
    if (name == undefined) {
      throw "missing graph name";
    }
    var g = new graph.Graph(name);
    
    var v = g.getVertices();

    var result = {};
    result["vertices"] = [];
    
    while (v.hasNext()) {
      var e = v.next();
      result["vertices"].push(e._properties);
    }
    
    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : MY_URL + "/vertices",
  context : MY_CONTEXT,

  callback : function (req, res) {
    switch (req.requestType) {
      case (actions.GET) :
        getVertices(req, res); 
        break;

      default:
        actions.resultUnsupported(req, res);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create edge
///
/// @REST{POST /_api/blueprint/edge?graph=@FA{graph-identifier}}
/// {"$id":"...","_from":"...","_to":"...","$label":"...","data1":{...}}
///
////////////////////////////////////////////////////////////////////////////////

function postEdge(req, res) {
  try {
    // name/id of the graph
    var name = req.parameters['graph'];    
    if (name == undefined) {
      throw "missing graph name";
    }
    var g = new graph.Graph(name);

    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CREATE_EDGE);
      
    if (json === undefined) {
      return;
    }
    
    var id = json["$id"];    
    var out = g.getVertex(json["_from"]);
    var ine = g.getVertex(json["_to"]);
    var label = json["$label"];    

    var e = g.addEdge(id, out, ine, label, json);      

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
/// @brief get edge
///
/// @REST{GET /_api/blueprint/edge/@FA{edge-identifier}?graph=@FA{graph-identifier}}
///
////////////////////////////////////////////////////////////////////////////////

function getEdge(req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_EDGE, "edge not found");
    return;
  }

  try {    
    // name/id of the graph
    var name = req.parameters['graph'];    
    if (name == undefined) {
      throw "missing graph name";
    }
    var g = new graph.Graph(name);

    var id = req.suffix[0];
    if (req.suffix.length > 1) {
      id += "/" + req.suffix[1];
    }
        
    var e = new graph.Edge(g, id);

    if (e == undefined || e._properties == undefined) {
      throw "no edge found for: " + id;
    }
    
    actions.resultOk(req, res, actions.HTTP_OK, { "edge" : e._properties} );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete edge
///
/// @REST{DELETE /_api/blueprint/edge/@FA{edge-identifier}?graph=@FA{graph-identifier}}
///
////////////////////////////////////////////////////////////////////////////////

function deleteEdge(req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_EDGE, "edge not found");
    return;
  }

  try {    
    // name/id of the graph
    var name = req.parameters['graph'];    
    if (name == undefined) {
      throw "missing graph name";
    }
    var g = new graph.Graph(name);

    var id = req.suffix[0];
    if (req.suffix.length > 1) {
      id += "/" + req.suffix[1];
    }
    
    var e = new graph.Edge(g, id);

    if (e == undefined || e._properties == undefined) {
      throw "no edge found for: " + id;
    }
    
    g.removeEdge(e);
    
    actions.resultOk(req, res, actions.HTTP_OK, { "deleted" : true} );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update edge data
///
/// @REST{PUT /_api/blueprint/edge/@FA{edge-identifier}?graph=@FA{graph-identifier}}
/// {"$id":"...","_from":"...","_to":"...","$label":"...","data1":{...}}
///
////////////////////////////////////////////////////////////////////////////////

function putEdge(req, res) {
  if (req.suffix.length < 1) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, "edge not found");
    return;
  }

  try {    
    // name/id of the graph
    var name = req.parameters['graph'];    
    if (name == undefined) {
      throw "missing graph name";
    }
    var g = new graph.Graph(name);

    var id = req.suffix[0];
    if (req.suffix.length > 1) {
      id += "/" + req.suffix[1];
    }
    
    var e = new graph.Edge(g, id);

    if (e == undefined || e._properties == undefined) {
      throw "no edge found for: " + id;
    }
    
    var json = actions.getJsonBody(req, res, actions.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE);
      
    if (json === undefined) {
      return;
    }

    var shallow = shallowCopy(json);
    shallow.$id = e._properties.$id;
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
/// @brief actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : MY_URL + "/edge",
  context : MY_CONTEXT,

  callback : function (req, res) {
    switch (req.requestType) {
      case (actions.POST) :
        postEdge(req, res); 
        break;

      case (actions.GET) :
        getEdge(req, res); 
        break;

      case (actions.DELETE) :
        deleteEdge(req, res); 
        break;

      case (actions.PUT) :
        putEdge(req, res); 
        break;

      default:
        actions.resultUnsupported(req, res);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get edges
///
/// Returns a list of all edges of a @FA{graph}
///
/// @REST{GET /_api/blueprint/edges?graph=@FA{graph-identifier}}
///
/// Returns a list of edges of a @FA{vertex}
///
/// @REST{GET /_api/blueprint/edges?graph=@FA{graph-identifier}&vertex=@FA{vertex-identifier}}
///
/// Returns a list of outbound edges of a @FA{vertex}
///
/// @REST{GET /_api/blueprint/edges?graph=@FA{graph-identifier}&vertex=@FA{vertex-identifier}&type=out}
///
/// Returns a list of inbound edges of a @FA{vertex}
///
/// @REST{GET /_api/blueprint/edges?graph=@FA{graph-identifier}&vertex=@FA{vertex-identifier}&type=in}
///
////////////////////////////////////////////////////////////////////////////////

function getEdges(req, res) {
  try {    
    // name/id of the graph
    var name = req.parameters['graph'];
    if (name == undefined) {
      throw "missing graph name";
    }
    
    var g = new graph.Graph(name);
    
    var result = {
      "edges" : []
    }
    
    var vertex = req.parameters['vertex'];
    if (vertex == undefined) {
      var e = g.getEdges();
      
      while (e.hasNext()) {
        result.edges.push(e.next()._properties);
      }    
    }
    else {
      var v = g.getVertex(vertex);

      if (v == undefined || v._properties == undefined) {
        throw "no vertex found for: " + vertex;
      }
      
      var type = req.parameters['type'];
      if (type === "in") {
        result.edges = g._edges.inEdges(v._id);
      }
      else if (type === "out") {
        result.edges = g._edges.outEdges(v._id);
      }
      else {
        result.edges = g._edges.edges(v._id);
      }      
    }
    
    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : MY_URL + "/edges",
  context : MY_CONTEXT,

  callback : function (req, res) {
    switch (req.requestType) {
      case (actions.GET) :
        getEdges(req, res); 
        break;

      default:
        actions.resultUnsupported(req, res);
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
