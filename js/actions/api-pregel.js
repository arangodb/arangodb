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
var graph_module = require('@arangodb/general-graph');

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
  if (json.vertexCollections && typeof json.vertexCollections === 'array'
      && json.edgeCollection && typeof json.edgeCollection === 'string') {
    
    var executionNum = db._pregel(json.vertexCollections,
                                  json.edgeCollection,
                                  algorithm,
                                  params);
    // TODO response
  } else if (json.graphName && typeof json.graphName === 'string') {
    var graph = graph_module._graph(json.graphName);
    if (graph) {
      var vertexCollections = [];
      var vcs = graph._vertecCollections();
      for (var i = 0; i < fcs.length; i++) {
        vertexCollections.push(vcs[i].name());
      }
      var edges = graph._edgeCollections();
      if (edges.length > 0) {
        var edgeCollection = [0].name()
        var executionNum = db._pregel(vertexCollections,
                                      edgeCollection,
                                      algorithm,
                                      params);
        
        
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
  
}

function delete_api_pregel(req, res) {

}

// //////////////////////////////////////////////////////////////////////////////
// / @brief gateway
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_api/pregel',

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
});
