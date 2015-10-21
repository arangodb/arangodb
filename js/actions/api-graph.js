/*jshint strict: false */
/*global AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief graph api
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
/// @author Achim Brandt
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("org/arangodb/actions");
var graph = require("org/arangodb/graph-blueprint");
var arangodb = require("org/arangodb");

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief url prefix
////////////////////////////////////////////////////////////////////////////////

var GRAPH_URL_PREFIX = "_api/graph";

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

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
/// @brief returns true if a "if-match" or "if-none-match" error happens
////////////////////////////////////////////////////////////////////////////////

function matchError (req, res, doc, errorCode) {

  if (req.headers["if-none-match"] !== undefined) {
    if (doc._rev === req.headers["if-none-match"].replace(/(^["']|["']$)/g, '')) {
      // error
      res.responseCode = actions.HTTP_NOT_MODIFIED;
      res.contentType = "application/json; charset=utf-8";
      res.body = '';
      res.headers = {};
      return true;
    }
  }

  if (req.headers["if-match"] !== undefined) {
    if (doc._rev !== req.headers["if-match"].replace(/(^["']|["']$)/g, '')) {
      // error
      actions.resultError(req,
                          res,
                          actions.HTTP_PRECONDITION_FAILED,
                          errorCode,
                          "wrong revision",
                          {});
      return true;
    }
  }

  var rev = req.parameters.rev;
  if (rev !== undefined) {
    if (doc._rev !== rev) {
      // error
      actions.resultError(req,
                          res,
                          actions.HTTP_PRECONDITION_FAILED,
                          errorCode,
                          "wrong revision",
                          {});
      return true;
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   graph functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function post_graph_graph (req, res) {
  try {
    var json = actions.getJsonBody(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CREATE_GRAPH);

    if (json === undefined) {
      return;
    }

    var name = json._key;
    var vertices = json.vertices;
    var edges = json.edges;

    var waitForSync = false;
    if (req.parameters.waitForSync) {
      waitForSync = true;
    }
    var g = new graph.Graph(name, vertices, edges, waitForSync);

    if (g._properties === null) {
      throw "no properties of graph found";
    }

    var headers = {
      "Etag" : g._properties._rev
    };


    waitForSync = waitForSync || g._gdb.properties().waitForSync;
    var returnCode = waitForSync ? actions.HTTP_CREATED : actions.HTTP_ACCEPTED;
    actions.resultOk(req, res, returnCode, { "graph" : g._properties }, headers );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CREATE_GRAPH, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function get_graph_graph (req, res) {
  try {
    var g = graph_by_request(req);

    if (matchError(req, res, g._properties, arangodb.ERROR_GRAPH_INVALID_GRAPH)) {
      return;
    }

    var headers = {
      "Etag" : g._properties._rev
    };

    actions.resultOk(req, res, actions.HTTP_OK, { "graph" : g._properties }, headers );
  }
  catch (err) {
    actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_INVALID_GRAPH, err);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function delete_graph_graph (req, res) {
  var g;

  try {
    var name = req.suffix[0];
    var exists = arangodb.db._collection('_graphs').exists(name);

    try {
      g = graph_by_request(req);
      if (g === null || g === undefined) {
        throw "graph not found";
      }
    }
    catch (graphNotFound) {
      if (exists) {
        graph.Graph.drop(name);
        actions.resultOk(req, res, actions.HTTP_OK, { "deleted" : true });
      }
    }
  }
  catch (err) {
    actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_INVALID_GRAPH, err);
    return;
  }

  if (matchError(req, res, g._properties, arangodb.ERROR_GRAPH_INVALID_GRAPH)) {
    return;
  }

  var waitForSync = g._gdb.properties().waitForSync;
  if (req.parameters.waitForSync) {
    waitForSync = true;
  }

  g.drop(waitForSync);

  waitForSync = waitForSync || g._gdb.properties().waitForSync;
  var returnCode = waitForSync ? actions.HTTP_OK : actions.HTTP_ACCEPTED;

  actions.resultOk(req, res, returnCode, { "deleted" : true });
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  vertex functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function post_graph_vertex (req, res, g) {
  try {
    var json = actions.getJsonBody(req, res);
    var id;

    if (json) {
      id = json._key;
    }

    var waitForSync = g._vertices.properties().waitForSync;
    if (req.parameters.waitForSync) {
      waitForSync = true;
    }

    var v = g.addVertex(id, json, waitForSync);

    if (v === null || v._properties === undefined) {
      throw "could not create vertex";
    }

    var headers = {
      "Etag" : v._properties._rev
    };

    var returnCode = waitForSync ? actions.HTTP_CREATED : actions.HTTP_ACCEPTED;

    actions.resultOk(req, res, returnCode, { "vertex" : v._properties }, headers );
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CREATE_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function get_graph_vertex (req, res, g) {
  var v;

  try {
    v = vertex_by_request(req, g);
  }
  catch (err) {
    actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, err);
    return;
  }

  if (matchError(req, res, v._properties, arangodb.ERROR_GRAPH_INVALID_VERTEX)) {
    return;
  }

  var headers = {
    "Etag" : v._properties._rev
  };

  actions.resultOk(req, res, actions.HTTP_OK, { "vertex" : v._properties}, headers);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function delete_graph_vertex (req, res, g) {
  var v;

  try {
    v = vertex_by_request(req, g);
  }
  catch (err) {
    actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, err);
    return;
  }

  if (matchError(req, res, v._properties, arangodb.ERROR_GRAPH_INVALID_VERTEX)) {
    return;
  }

  var waitForSync = g._vertices.properties().waitForSync;
  if (req.parameters.waitForSync) {
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
    actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, err);
    return;
  }

  if (matchError(req, res, v._properties, arangodb.ERROR_GRAPH_INVALID_VERTEX)) {
    return;
  }

  try {
    var json = actions.getJsonBody(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX);

    if (json === undefined) {
      actions.resultBad(req,
                        res,
                        arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX,
                        "error in request body");
      return;
    }

    var waitForSync = g._vertices.properties().waitForSync;
    if (req.parameters.waitForSync) {
      waitForSync = true;
    }

    var shallow = json._shallowCopy;

    var id2 = null;
    if (isPatch) {
      var keepNull = req.parameters.keepNull;
      if (keepNull !== undefined || keepNull === "false") {
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
      "Etag" : result._rev
    };

    var returnCode = waitForSync ? actions.HTTP_CREATED : actions.HTTP_ACCEPTED;

    actions.resultOk(req, res, returnCode, { "vertex" : result }, headers );
  }
  catch (err2) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, err2);
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function put_graph_vertex (req, res, g) {
  update_graph_vertex(req, res, g, false);
}

////////////////////////////////////////////////////////////////////////////////
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

  throw "unknown compare function in property filter";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills a filter (throws exception)
////////////////////////////////////////////////////////////////////////////////

function process_property_filter (data, num, property, collname) {
  if (property.key !== undefined && property.compare === "HAS") {
      if (data.filter === "") { data.filter = " FILTER"; } else { data.filter += " &&";}
      data.filter += " HAS(" + collname + ", @key" + num.toString() + ") ";
      data.bindVars["key" + num.toString()] = property.key;
      return;
  }
  if (property.key !== undefined && property.compare === "HAS_NOT") {
      if (data.filter === "") { data.filter = " FILTER"; } else { data.filter += " &&";}
      data.filter += " !HAS(" + collname + ", @key" + num.toString() + ") ";
      data.bindVars["key" + num.toString()] = property.key;
      return;
  }
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
      limit = " LIMIT " + parseInt(json.limit, 10);
    }

    if (json.filter !== undefined && json.filter.properties !== undefined) {
      process_properties_filter(data, json.filter.properties, "v");
    }

    // build aql query
    var query = "FOR v IN @@vertexColl" + data.filter + limit + " RETURN v";
                                        
    var options = {
      count: json.count || false,
      batchSize: json.batchSize || 1000
    };

    var cursor = AQL_EXECUTE(query, data.bindVars, options);
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
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
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
      limit = " LIMIT " + parseInt(json.limit, 10);
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
    var query = 'FOR n IN NEIGHBORS( @@vertexColl, @@edgeColl, @id, "' + direction + '", null, ' +
                '{ includeData: true }) ' + data.filter + limit + " RETURN n.vertex ";

    var options = {
      count: json.count,
      batchSize: json.batchSize || 1000
    };

    var cursor = AQL_EXECUTE(query, data.bindVars, options);

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
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function post_graph_edge (req, res, g) {
  try {
    var json = actions.getJsonBody(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CREATE_EDGE);

    if (json === undefined) {
      actions.resultBad(req,
                        res,
                        arangodb.ERROR_GRAPH_COULD_NOT_CREATE_EDGE,
                        "error in request body");
      return;
    }

    var waitForSync = g._edges.properties().waitForSync;
    if (req.parameters.waitForSync) {
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
      "Etag" : e._properties._rev
    };

    var returnCode = waitForSync ? actions.HTTP_CREATED : actions.HTTP_ACCEPTED;

    actions.resultOk(req, res, returnCode, { "edge" : e._properties }, headers);
  }
  catch (err) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CREATE_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function get_graph_edge (req, res, g) {
  var e;

  try {
    e = edge_by_request(req, g);

    if (matchError(req, res, e._properties, arangodb.ERROR_GRAPH_INVALID_EDGE)) {
      return;
    }

    var headers = {
      "Etag" : e._properties._rev
    };

    actions.resultOk(req, res, actions.HTTP_OK, { "edge" : e._properties}, headers);
  }
  catch (err) {
    actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_INVALID_EDGE, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function delete_graph_edge (req, res, g) {
  var e;

  try {
    e = edge_by_request(req, g);
  }
  catch (err) {
    actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_INVALID_EDGE, err);
    return;
  }

  if (matchError(req, res, e._properties, arangodb.ERROR_GRAPH_INVALID_EDGE)) {
    return;
  }

  var waitForSync = g._edges.properties().waitForSync;
  if (req.parameters.waitForSync) {
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
    actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, err);
    return;
  }

  if (matchError(req, res, e._properties, arangodb.ERROR_GRAPH_INVALID_EDGE)) {
    return;
  }

  try {
    var json = actions.getJsonBody(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE);

    if (json === undefined) {
      actions.resultBad(req,
                        res,
                        arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE,
                        "error in request body");
      return;
    }

    var waitForSync = g._edges.properties().waitForSync;
    if (req.parameters.waitForSync) {
      waitForSync = true;
    }

    var shallow = json._shallowCopy;
    shallow.$label = e._properties.$label;

    var id2 = null;
    if (isPatch) {
      var keepNull = req.parameters.keepNull;
      if (keepNull !== undefined || keepNull === "false") {
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
      "Etag" : result._rev
    };

    var returnCode = waitForSync ? actions.HTTP_CREATED : actions.HTTP_ACCEPTED;

    actions.resultOk(req, res, returnCode, { "edge" : result}, headers );
  }
  catch (err2) {
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, err2);
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function put_graph_edge (req, res, g) {
  update_graph_edge (req, res, g, false);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

function patch_graph_edge (req, res, g) {
  update_graph_edge (req, res, g, true);
}

////////////////////////////////////////////////////////////////////////////////
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
      limit = " LIMIT " + parseInt(json.limit, 10);
    }

    if (json.filter !== undefined && json.filter.properties !== undefined) {
      process_properties_filter(data, json.filter.properties, "e");
    }

    if (json.filter !== undefined && json.filter.labels !== undefined) {
      process_labels_filter(data, json.filter.labels, "e");
    }

    var query = "FOR e IN @@edgeColl" + data.filter + limit + " RETURN e";

    var options = {
      count: json.count,
      batchSize: json.batchSize || 1000
    };
    var cursor = AQL_EXECUTE(query, data.bindVars, options);

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
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
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
      limit = " LIMIT " + parseInt(json.limit, 10);
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

    var options = {
      count: json.count,
      batchSize: json.batchSize || 1000
    };

    var cursor = AQL_EXECUTE(query, data.bindVars, options);

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
    actions.resultBad(req, res, arangodb.ERROR_GRAPH_INVALID_VERTEX, err);
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
    var g;

    // POST /_api/graph/<key>/...
    try {
      g = graph_by_request(req);
    }
    catch (err) {
      actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_INVALID_GRAPH, err);
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
    // GET /_api/graph: return all graphs

    actions.resultOk(req, res, actions.HTTP_OK, { "graphs" : graph.Graph.getAll() });
  }
  else if (req.suffix.length === 1) {
    // GET /_api/graph/<key>
    get_graph_graph(req, res);
  }
  else {
    var g;

    // GET /_api/graph/<key>/...
    try {
      g = graph_by_request(req);
    }
    catch (err) {
      actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_INVALID_GRAPH, err);
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
    var g;

    // PUT /_api/graph/<key>/...
    try {
      g = graph_by_request(req);
    }
    catch (err) {
      actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_INVALID_GRAPH, err);
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
    var g;

    // PATCH /_api/graph/<key>/...
    try {
      g = graph_by_request(req);
    }
    catch (err) {
      actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_INVALID_GRAPH, err);
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
    var g;

    // DELETE /_api/graph/<key>/...
    try {
      g = graph_by_request(req);
    }
    catch (err) {
      actions.resultNotFound(req, res, arangodb.ERROR_GRAPH_INVALID_GRAPH, err);
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
