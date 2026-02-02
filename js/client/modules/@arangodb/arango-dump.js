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

const internal = require('internal');

const URL = '/_api/dump';

exports.get_batch_id_from_start_response = function(response) {
  return response.headers["x-arango-dump-id"];
};

exports.start = function (options, server) {
  let url = `${URL}/start`;
  if (server !== undefined) {
    url += `?dbserver=${server}`;
  }
  return internal.arango.POST_RAW(url, options);
};
exports.next = function (dumpId, batchId, previousBatchId, server, headerOpts={}) {
  let url = `${URL}/next/${dumpId}?batchId=${batchId}`;
  if (previousBatchId !== undefined && previousBatchId !== null) {
    url += `&lastBatch=${previousBatchId}`;
  }
  if (server !== undefined) {
    url += `&dbserver=${server}`;
  }
  return internal.arango.POST_RAW(url, {}, headerOpts);
};
exports.delete = function (dumpId, server) {
  let url = `${URL}/${dumpId}`;
  if (server !== undefined) {
    url += `?dbserver=${server}`;
  }
  return internal.arango.DELETE_RAW(url);
};
