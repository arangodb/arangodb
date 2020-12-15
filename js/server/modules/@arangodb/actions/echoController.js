/* jshint strict: false, unused: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief example echo controller
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2012 triagens GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var actions = require('@arangodb/actions');

exports.head = function (req, res, options, next) {
  res.responseCode = actions.HTTP_OK;
  res.contentType = 'application/json; charset=utf-8';
  res.body = '';
};

exports['do'] = function (req, res, options, next) {
  res.responseCode = actions.HTTP_OK;
  res.contentType = 'application/json; charset=utf-8';
  res.body = JSON.stringify({ 'request': req, 'options': options });
};
