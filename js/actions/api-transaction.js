/* jshint strict: false */
/* global TRANSACTION */

// //////////////////////////////////////////////////////////////////////////////
// / @brief transaction actions
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_api_transaction
// //////////////////////////////////////////////////////////////////////////////

function post_api_transaction (req, res) {
  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var result = TRANSACTION(json);

  actions.resultOk(req, res, actions.HTTP_OK, { result: result });
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief gateway
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_api/transaction',

  callback: function (req, res) {
    try {
      switch (req.requestType) {
        case actions.POST:
          post_api_transaction(req, res);
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    } catch (err) {
      actions.resultException(req, res, err);
    }
  }
});
