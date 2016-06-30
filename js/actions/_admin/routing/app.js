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

var internal = require('internal');
var console = require('console');

var actions = require('@arangodb/actions');

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_admin_routing_reloads
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/routing/reload',
  prefix: false,

  callback: function (req, res) {
    internal.executeGlobalContextFunction('reloadRouting');
    console.debug('about to flush the routing cache');
    actions.resultOk(req, res, actions.HTTP_OK);
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the current routing information
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/routing/routes',
  prefix: false,

  callback: function (req, res) {
    actions.resultOk(req, res, actions.HTTP_OK, actions.routingTree());
  }
});
