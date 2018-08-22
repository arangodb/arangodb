/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief global configurations
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 triagens GmbH, Cologne, Germany
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
