/* jshint strict: false */
/* global ArangoServerState */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
// / @author Andreas Streichardt
// //////////////////////////////////////////////////////////////////////////////

exports.setup = function() {
  global.ArangoServerState.enableSyncReplicationDebug();
  ArangoServerState.setRole('PRIMARY');
  global.ArangoAgency.set = function() { return true; };
  global.ArangoAgency.write = function() { return true; };
  global.ArangoAgency.increaseVersion = function() { return true; };
  global.ArangoAgency.get = function() { return true; };
  global.ArangoAgency.lockRead = function() { return true; };
  global.ArangoAgency.lockWrite = function() { return true; };
  global.ArangoAgency.unlockRead = function() { return true; };
  global.ArangoAgency.unlockWrite = function() { return true; };
};
