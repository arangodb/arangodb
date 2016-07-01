/* jshint strict: false */
/* global AQL_EXPLAIN */

// //////////////////////////////////////////////////////////////////////////////
// / @brief query explain actions
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

var actions = require('@arangodb/actions');
var ERRORS = require('internal').errors;

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_api_explain
// //////////////////////////////////////////////////////////////////////////////

function post_api_explain (req, res) {
  if (req.suffix.length !== 0) {
    actions.resultNotFound(req,
      res,
      ERRORS.errors.ERROR_HTTP_NOT_FOUND.code,
      ERRORS.errors.ERROR_HTTP_NOT_FOUND.message);
    return;
  }

  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    return;
  }

  var result = AQL_EXPLAIN(json.query, json.bindVars, json.options || { });

  if (result instanceof Error) {
    actions.resultException(req, res, result, undefined, false);
    return;
  }

  if (result.hasOwnProperty('plans')) {
    result = {
      plans: result.plans,
      warnings: result.warnings,
      stats: result.stats
    };
  } else {
    result = {
      plan: result.plan,
      warnings: result.warnings,
      stats: result.stats,
      cacheable: result.cacheable
    };
  }
  actions.resultOk(req, res, actions.HTTP_OK, result);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief explain gateway
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_api/explain',

  callback: function (req, res) {
    try {
      switch (req.requestType) {
        case actions.POST:
          post_api_explain(req, res);
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});
