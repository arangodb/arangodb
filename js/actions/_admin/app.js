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
const actions = require('@arangodb/actions');

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_admin_echo
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/echo',
  prefix: true,
  isSystem: false,

  callback: function (req, res) {
    res.responseCode = actions.HTTP_OK;
    res.contentType = 'application/json; charset=utf-8';
    req.rawRequestBody = internal.rawRequestBody(req);
    res.body = JSON.stringify(req);
  }
});
