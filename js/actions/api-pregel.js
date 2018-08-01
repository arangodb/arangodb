/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
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
// / @author Simon Grätzer
// / @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangodb = require('@arangodb');
var actions = require('@arangodb/actions');
var internal = require('internal');
var db = internal.db;

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


function post_api_pregel (req, res) {
  /* jshint evil: true */
  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    return badParam(req, res);
  }

  // check start vertex
  // -----------------------------------------
  if (!json.algorithm || typeof json.algorithm !== 'string') {
    return notFound(req, res, 'invalid algorithm');
  }

  var params = json.params || {};
  var executionNum;
  if (json.vertexCollections && json.vertexCollections instanceof Array
      && json.edgeCollections && typeof json.edgeCollection === 'string') {

    executionNum = db._pregelStart(json.algorithm,
                                   json.vertexCollections,
                                   json.edgeCollections,
                                   params);
    actions.resultOk(req, res, actions.HTTP_OK, executionNum);

  } else if (json.graphName &&  typeof json.graphName === 'string') {
    var graph_module;
    if (internal.isEnterprise()) {
      graph_module = require('@arangodb/smart-graph');
    } else {
      graph_module = require('@arangodb/general-graph');
    }
    var graph = graph_module._graph(json.graphName);
    if (graph) {
      var vertexCollections = [];
      var vcs = graph._vertexCollections(true);
      for (var key in vcs) {
        vertexCollections.push(vcs[key].name());
      }
      var edges = graph._edgeCollections();
      if (edges.length > 0) {
        var edgeCollections = edges.map(e => e.name());
        executionNum = db._pregelStart(json.algorithm, vertexCollections,
                                       edgeCollections, params);
        actions.resultOk(req, res, actions.HTTP_OK, executionNum);

      } else {
        return badParam(req, res, "No edge collection specified");
      }
    } else {
      return notFound(req, res, arangodb.ERROR_GRAPH_NOT_FOUND,
                      'invalid graphname');
    }
  } else {
    return badParam(req, res, "invalid parameters, either collections or graph name");
  }
}

function get_api_pregel(req, res) {
  if (req.suffix.length === 1) {
    var executionNum = decodeURIComponent(req.suffix[0]);
    var status = db._pregelStatus(executionNum);
    actions.resultOk(req, res, actions.HTTP_OK, status);
  } else {
    return badParam(req, res, "invalid parameters, specify execution number");
  }
}

function delete_api_pregel(req, res) {
  if (req.suffix.length === 1) {
    var executionNum = decodeURIComponent(req.suffix[0]);
    var status = db._pregelCancel(executionNum);
    actions.resultOk(req, res, actions.HTTP_OK, "");
  } else {
    return badParam(req, res, "invalid parameters, specify execution number");
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief gateway
// //////////////////////////////////////////////////////////////////////////////

/*actions.defineHttp({
  url: '_api/control_pregel',

  callback: function (req, res) {
    try {
      switch (req.requestType) {
        case actions.POST:
          post_api_pregel(req, res);
          break;

        case actions.GET:
          get_api_pregel(req, res);
          break;

        case actions.DELETE:
          delete_api_pregel(req, res);
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    } catch (err) {
      actions.resultException(req, res, err);
    }
  }
});*/
