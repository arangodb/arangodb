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
//
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{vertex,string,required}
/// The id of the start vertex.
///
/// @RESTQUERYPARAM{direction,string,required}
/// Selects `any`, `in` or `out` direction for edges.
///
/// @RESTDESCRIPTION
/// Returns the list of edges starting or ending in the vertex identified by
/// `vertex-handle`.
///
/// @EXAMPLES
///
/// Any direction
///
/// @verbinclude rest-edge-read-edges-any
///
/// In edges
///
/// @verbinclude rest-edge-read-edges-in
///
/// Out edges
///
/// @verbinclude rest-edge-read-edges-out
////////////////////////////////////////////////////////////////////////////////

function GET_edges (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "expect GET /" + API + "/<collection-identifer>?vertex=<vertex-handle>&direction=<direction>");
    return;
  }

  var name = decodeURIComponent(req.suffix[0]);
  var id = parseInt(name) || name;
  var collection = arangodb.db._collection(id);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var vertex = req.parameters['vertex'];
  var direction = req.parameters['direction'];
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
        GET_edges(req, res);
      }
      else {
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
