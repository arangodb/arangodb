'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief run cluster bootstrap
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
// / @author Max Neunhoeffer
// / @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief initialize a new database
// //////////////////////////////////////////////////////////////////////////////

(function () {
  const internal = require('internal');
  global.UPGRADE_ARGS = {
    isCluster: true,
    isCoordinator: true,
    isRelaunch: false,
    upgrade: false
  };
  let result = internal.loadStartup('server/upgrade-database.js');
  
  if (result) {
    delete global.UPGRADE_TYPE;
  }

  result = global.UPGRADE_STARTED && result;
  delete global.UPGRADE_STARTED;
  delete global.UPGRADE_ARGS;

  if (!result) {
    console.error('upgrade-database.js for cluster script failed!');
  }

  global.ArangoAgency.set('Current/Foxxmaster', global.ArangoServerState.id());

  return result;
}());
