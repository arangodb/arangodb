/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief administration actions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
// / @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const console = require('console');
const actions = require('@arangodb/actions');

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_admin_echo
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/echo',
  prefix: true,

  callback: function (req, res) {
    res.responseCode = actions.HTTP_OK;
    res.contentType = 'application/json; charset=utf-8';
    req.rawRequestBody = internal.rawRequestBody(req);
    res.body = JSON.stringify(req);
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_admin_time
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/time',
  prefix: false,

  callback: function (req, res) {
    actions.resultOk(req, res, actions.HTTP_OK, { time: internal.time() });
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_admin_execute
// //////////////////////////////////////////////////////////////////////////////

if (global.ALLOW_ADMIN_EXECUTE) {
  actions.defineHttp({
    url: '_admin/execute',
    prefix: false,

    callback: function (req, res) {
      /* jshint evil: true */
      var body = req.requestBody;
      var result;

      console.warn("about to execute: '%s'", body);

      if (body !== '') {
        result = eval('(function() {' + body + '}());');
      }

      if (req.parameters.hasOwnProperty('returnAsJSON') &&
        req.parameters.returnAsJSON === 'true') {
        actions.resultOk(req, res, actions.HTTP_OK, result);
      } else {
        actions.resultOk(req, res, actions.HTTP_OK, JSON.stringify(result));
      }
    }
  });
}

delete global.ALLOW_ADMIN_EXECUTE;
