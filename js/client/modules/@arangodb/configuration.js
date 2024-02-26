/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler, Lucas Dohmen
// / @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var arangosh = require('@arangodb/arangosh');

// //////////////////////////////////////////////////////////////////////////////
// / @brief the notification configuration
// //////////////////////////////////////////////////////////////////////////////

exports.notifications = {};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the versions notification configuration
// //////////////////////////////////////////////////////////////////////////////

exports.notifications.versions = function () {
  var db = internal.db;

  var uri = '_admin/configuration/notifications/versions';

  var requestResult = db._connection.GET(uri);
  return arangosh.checkRequestResult(requestResult);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets the versions notification collections
// //////////////////////////////////////////////////////////////////////////////

exports.notifications.setVersions = function (data) {
  var db = internal.db;

  var uri = '_admin/configuration/notifications/versions';

  var requestResult = db._connection.PUT(uri, data);
  return arangosh.checkRequestResult(requestResult);
};
