/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief managing edges
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");

var API = "/_api/edges";

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get edges
///
/// @RESTHEADER{GET /_api/edges/{collection-id},reads in- or outbound edges}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{collection-id,string,required}
/// The id of the collection.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{vertex,string,required}
/// The id of the start vertex.
///
/// @RESTQUERYPARAM{direction,string,optional}
/// Selects `in` or `out` direction for edges. If not set, any edges are
/// returned.
///
/// @RESTDESCRIPTION
/// Returns the list of edges starting or ending in the vertex identified by
/// `vertex-handle`.
///
/// @EXAMPLES
///
/// Any direction
///
/// @EXAMPLE_ARANGOSH_RUN{RestEdgesReadEdgesAny}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex(1);
///     var v2 = g.addVertex(2);
///     var v3 = g.addVertex(3);
///     var v4 = g.addVertex(4);
///     g.addEdge(v1, v3, 5, "v1 -> v3");
///     g.addEdge(v2, v1, 6, "v2 -> v1");
///     g.addEdge(v4, v1, 7, "v4 -> v1");
///
///     var url = "/_api/edges/edges?vertex=vertices/1";
///     var response = logCurlRequest('GET', url);
/// 
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// In edges
///
/// @EXAMPLE_ARANGOSH_RUN{RestEdgesReadEdgesIn}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex(1);
///     var v2 = g.addVertex(2);
///     var v3 = g.addVertex(3);
///     var v4 = g.addVertex(4);
///     g.addEdge(v1, v3, 5, "v1 -> v3");
///     g.addEdge(v2, v1, 6, "v2 -> v1");
///     g.addEdge(v4, v1, 7, "v4 -> v1");
///
///     var url = "/_api/edges/edges?vertex=vertices/1&direction=in";
///     var response = logCurlRequest('GET', url);
/// 
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop("edges");
///     db._drop("vertices");
///     db._graphs.remove("graph");
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Out edges
///
/// @EXAMPLE_ARANGOSH_RUN{RestEdgesReadEdgesOut}
///     var Graph = require("org/arangodb/graph").Graph;
///     var g = new Graph("graph", "vertices", "edges");
///     var v1 = g.addVertex(1);
///     var v2 = g.addVertex(2);
///     var v3 = g.addVertex(3);
///     var v4 = g.addVertex(4);
///     g.addEdge(v1, v3, 5, "v1 -> v3");
///     g.addEdge(v2, v1, 6, "v2 -> v1");
///     g.addEdge(v4, v1, 7, "v4 -> v1");
///
///     var url = "/_api/edges/edges?vertex=vertices/1&direction=out";
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

function get_edges (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, 
                      res, 
                      arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "expect GET /" + API + 
                      "/<collection-identifer>?vertex=<vertex-handle>&direction=<direction>");
    return;
  }

  var name = decodeURIComponent(req.suffix[0]);
  var id = parseInt(name, 10) || name;
  var collection = arangodb.db._collection(id);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var vertex = req.parameters.vertex;
  var direction = req.parameters.direction;
  var e;

  if (direction === null || direction === undefined || direction === "" || direction === "any") {
    e = collection.edges(vertex);
  }
  else if (direction === "in") {
    e = collection.inEdges(vertex);
  }
  else if (direction === "out") {
    e = collection.outEdges(vertex);
  }
  else {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "<direction> must be any, in, or out, not: " + JSON.stringify(direction));
    return;
  }

  var result = { edges : e };

  actions.resultOk(req, res, actions.HTTP_OK, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads or creates a collection
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API,
  context : "api",

  callback : function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        get_edges(req, res);
      }
      else {
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

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
