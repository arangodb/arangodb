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

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

var actions = require("actions");
var graph = require("graph");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief create a key value pair
////////////////////////////////////////////////////////////////////////////////

function postGraph(req, res) {
  try {
    var name = req.parameters['name'];
    var vertices = req.parameters['vertices'];
    var edges = req.parameters['edges'];

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
/// @brief return graph data
////////////////////////////////////////////////////////////////////////////////

function getGraph(req, res) {
  try {
    var name = req.parameters['name'];
    var g = new graph.Graph(name);

    if (g._properties == null) {
      throw "no properties of graph found";
    }
    actions.resultOk(req, res, actions.HTTP_OK, { "graph" : g._properties} );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_GRAPH, err);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief key value pair actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/graph/graph",
  context : "api",

  callback : function (req, res) {
    switch (req.requestType) {
      case (actions.POST) :
        postGraph(req, res); 
        break;

      case (actions.GET) :
        getGraph(req, res); 
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
/// @addtogroup AvocadoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add a vertex
////////////////////////////////////////////////////////////////////////////////

function postVertex(req, res) {
  try {
    // name/id of the graph
    var name = req.parameters['name'];
    
    var g = new graph.Graph(name);

    var id = req.parameters['id'];
    var v;
    if (req.requestBody != null && req.requestBody != "") {
      var body = JSON.parse(req.requestBody);      
      v = g.addVertex(id, body);      
    }
    else {
      v = g.addVertex(id);
    }

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
/// @brief add a vertex
////////////////////////////////////////////////////////////////////////////////

function getVertex(req, res) {
  try {
    // name/id of the graph
    var name = req.parameters['name'];
    
    var g = new graph.Graph(name);

    var id = req.parameters['id'];    

    var v = g.getVertex(id);

    if (v == undefined || v._properties == undefined) {
      throw "no vertex found";
    }
    
    actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : v._properties} );
  }
  catch (err) {
    actions.resultBad(req, res, actions.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

actions.defineHttp({
  url : "_api/graph/vertex",
  context : "api",

  callback : function (req, res) {
    switch (req.requestType) {
      case (actions.POST) :
        postVertex(req, res); 
        break;

      case (actions.GET) :
        getVertex(req, res); 
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
