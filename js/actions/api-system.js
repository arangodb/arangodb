/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief administration actions
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
// / @author Dr. Frank Celler
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const actions = require('@arangodb/actions');
const internal = require('internal');

// //////////////////////////////////////////////////////////////////////////////
// / @brief main routing action
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '',
  prefix: true,
  isSystem: false,

  callback: function (req, res) {
    req.absoluteUrl = function (url) {
      var protocol = req.headers['x-forwarded-proto'] || req.protocol;
      var address = req.headers['x-forwarded-host'] || req.headers.host;
      if (!address) {
        address = (req.server.address === '0.0.0.0' ? 'localhost' : req.server.address) + ':' + req.server.port;
      }
      return protocol + '://' + address + '/_db/' + encodeURIComponent(req.database) + (url || req.url);
    };
    try {
      actions.routeRequest(req, res);
    } catch (e) {
      if (e instanceof internal.ArangoError &&
          e.errorNum === actions.HTTP_SERVICE_UNAVAILABLE) {
        // return any HTTP 503 response as it is
        actions.resultException(req, res, e, null, false);
        // and finish the routing
        return;
      }

      var msg = 'A runtime error occurred while executing an action\n';
      let err = e;
      while (err) {
        if (err !== e) {
          msg += '\nvia ';
        }
        if (err.stack) {
          msg += err.stack;
        }
        err = err.cause;
      }

      if (e.hasOwnProperty('route')) {
        actions.errorFunction(e.route, msg)(req, res);
      } else {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, actions.HTTP_SERVER_ERROR, msg);
      }
    }
  }
});
