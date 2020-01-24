/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief traversal actions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangodb = require('@arangodb');
var actions = require('@arangodb/actions');
var db = require('internal').db;
var traversal = require('@arangodb/graph/traversal');
var Traverser = traversal.Traverser;
var graph = require('@arangodb/general-graph');

// //////////////////////////////////////////////////////////////////////////////
// / @brief create a "bad parameter" error
// //////////////////////////////////////////////////////////////////////////////

function badParam (req, res, message) {
  actions.resultBad(req,
    res,
    arangodb.ERROR_HTTP_BAD_PARAMETER,
    message);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief create a "not found" error
// //////////////////////////////////////////////////////////////////////////////

function notFound (req, res, code, message) {
  actions.resultNotFound(req,
    res,
    code,
    message);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_HTTP_API_TRAVERSAL
// //////////////////////////////////////////////////////////////////////////////

function post_api_traversal (req, res) {
  /* jshint evil: true */
  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    return badParam(req, res);
  }

  // check start vertex
  // -----------------------------------------

  if (json.startVertex === undefined ||
    typeof json.startVertex !== 'string') {
    return badParam(req, res, 'missing or invalid startVertex');
  }

  var doc;
  try {
    doc = db._document(json.startVertex);
  } catch (err) {
    return notFound(req, res, arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND, 'invalid startVertex');
  }

  var datasource;
  var edgeCollection;

  if (json.graphName === undefined) {
    // check edge collection
    // -----------------------------------------

    if (json.edgeCollection === undefined) {
      return badParam(req, res, 'missing graphname');
    }
    if (typeof json.edgeCollection !== 'string') {
      return notFound(req, res, 'invalid edgecollection');
    }

    try {
      edgeCollection = db._collection(json.edgeCollection);
      datasource = traversal.collectionDatasourceFactory(edgeCollection);
    } catch (ignore) {}

    if (edgeCollection === undefined ||
      edgeCollection === null) {
      return notFound(req, res, arangodb.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        'invalid edgeCollection');
    }
  } else {
    if (typeof json.graphName !== 'string' || !graph._exists(json.graphName)) {
      return notFound(req, res, arangodb.ERROR_GRAPH_NOT_FOUND,
        'invalid graphname');
    }
    datasource = traversal.generalGraphDatasourceFactory(json.graphName);
  }

  // set up filters
  // -----------------------------------------

  var filters = [];
  if (json.minDepth !== undefined) {
    filters.push(traversal.minDepthFilter);
  }
  if (json.maxDepth !== undefined) {
    filters.push(traversal.maxDepthFilter);
  }
  if (json.filter) {
    try {
      filters.push(new Function('config', 'vertex', 'path', json.filter));
    } catch (err3) {
      return badParam(req, res, 'invalid filter function');
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
      visitor = new Function('config', 'result', 'vertex', 'path', 'connected', json.visitor);
    } catch (err4) {
      return badParam(req, res, 'invalid visitor function');
    }
  } else {
    visitor = traversal.trackingVisitor;
  }

  // set up expander
  // -----------------------------------------

  var expander;

  if (json.direction !== undefined) {
    expander = json.direction;
  } else if (json.expander !== undefined) {
    try {
      expander = new Function('config', 'vertex', 'path', json.expander);
    } catch (err5) {
      return badParam(req, res, 'invalid expander function');
    }
  }

  if (expander === undefined) {
    return badParam(req, res, 'missing or invalid expander');
  }

  // set up sort
  // -----------------------------------------

  var sort;

  if (json.sort !== undefined) {
    try {
      sort = new Function('l', 'r', json.sort);
    } catch (err6) {
      return badParam(req, res, 'invalid sort function');
    }
  }

  // assemble config object
  // -----------------------------------------

  var config = {
    params: json,
    datasource: datasource,
    strategy: json.strategy,
    order: json.order,
    itemOrder: json.itemOrder,
    expander: expander,
    sort: sort,
    visitor: visitor,
    filter: filters,
    minDepth: json.minDepth,
    maxDepth: json.maxDepth,
    maxIterations: json.maxIterations,
    uniqueness: json.uniqueness
  };

  if (edgeCollection !== undefined) {
    config.edgeCollection = edgeCollection;
  }

  // assemble result object
  // -----------------------------------------

  var result = {
    visited: {
      vertices: [],
      paths: []
    }
  };

  if (json.init !== undefined) {
    try {
      var init = new Function('result', json.init);
      init(result);
    } catch (err7) {
      return badParam(req, res, 'invalid init function');
    }
  }

  // run the traversal
  // -----------------------------------------

  var traverser;
  try {
    traverser = new Traverser(config);
    traverser.traverse(result, doc);
    actions.resultOk(req, res, actions.HTTP_OK, { result: result });
  } catch (err8) {
    if (traverser === undefined) {
      // error during traversal setup
      return badParam(req, res, err8);
    }
    actions.resultException(req, res, err8, undefined, false);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief gateway
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_api/traversal',
  isSystem: false,

  callback: function (req, res) {
    try {
      switch (req.requestType) {
        case actions.POST:
          post_api_traversal(req, res);
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    } catch (err) {
      actions.resultException(req, res, err);
    }
  }
});
