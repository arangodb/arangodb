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
// //////////////////////////////////////////////////////////////////////////////

const db = require('internal').db;
const arangosh = require('@arangodb/arangosh');
const IM = global.instanceManager;
const internal = require('internal');

const ACTIVITY_REGISTRY_URL = '/_admin/activity-registry';

// get snapshot from coordinator or single server
exports.get_snapshot = function () {
  return arangosh.checkRequestResult(db._connection.GET(ACTIVITY_REGISTRY_URL));
}

exports.get_snapshot_from_server = function (server) {
  IM.rememberConnection();
  IM.arangods.filter((x) => x.id == server)[0].connect();
  const result = arangosh.checkRequestResult(internal.arango.GET_RAW(ACTIVITY_REGISTRY_URL)).parsedBody;
  IM.reconnectMe()
  return result;
}
