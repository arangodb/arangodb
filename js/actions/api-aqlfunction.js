/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief AQL user functions management
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
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangodb = require('@arangodb');
var actions = require('@arangodb/actions');
var aqlfunctions = require('@arangodb/aql/functions');

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_aqlfunction
// //////////////////////////////////////////////////////////////////////////////

function get_api_aqlfunction (req, res) {
  if (req.suffix.length !== 0) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var namespace;
  if (req.parameters.hasOwnProperty('namespace')) {
    namespace = req.parameters.namespace;
  }

  var result = aqlfunctions.toArray(namespace);
  actions.resultOk(req, res, actions.HTTP_OK, result);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_api_aqlfunction
// //////////////////////////////////////////////////////////////////////////////

function post_api_aqlfunction (req, res) {
  var json = actions.getJsonBody(req, res, actions.HTTP_BAD);

  if (json === undefined) {
    return;
  }

  var result = aqlfunctions.register(json.name, json.code, json.isDeterministic);

  actions.resultOk(req, res, result ? actions.HTTP_OK : actions.HTTP_CREATED, { });
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_delete_api_aqlfunction
// //////////////////////////////////////////////////////////////////////////////

function delete_api_aqlfunction (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var name = decodeURIComponent(req.suffix[0]);
  try {
    var g = req.parameters.group;
    if (g === 'true' || g === 'yes' || g === 'y' || g === 'on' || g === '1') {
      aqlfunctions.unregisterGroup(name);
    } else {
      aqlfunctions.unregister(name);
    }
    actions.resultOk(req, res, actions.HTTP_OK, { });
  } catch (err) {
    if (err.errorNum === arangodb.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code) {
      actions.resultNotFound(req, res, arangodb.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code);
    } else {
      throw err;
    }
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief gateway
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_api/aqlfunction',

  callback: function (req, res) {
    try {
      switch (req.requestType) {
        case actions.GET:
          get_api_aqlfunction(req, res);
          break;

        case actions.POST:
          post_api_aqlfunction(req, res);
          break;

        case actions.DELETE:
          delete_api_aqlfunction(req, res);
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});
