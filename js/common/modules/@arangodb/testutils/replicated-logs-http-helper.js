/*jshint strict: true */
'use strict';
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
const lh = require("@arangodb/testutils/replicated-logs-helper");
const request = require('@arangodb/request');


exports.localStatus = function (server, database, logId) {
  let url = `${lh.getServerUrl(server)}/_db/${database}/_api/log/${logId}/local-status`;
  return request.get(url).json;
};

exports.head = function (server, database, logId, limit) {
  let url = `${lh.getServerUrl(server)}/_db/${database}/_api/log/${logId}/head`;
  if (limit !== undefined) {
    url += `?limit=${limit}`;
  }
  return request.get(url).json;
};

exports.tail = function (server, database, logId, limit) {
  let url = `${lh.getServerUrl(server)}/_db/${database}/_api/log/${logId}/tail`;
  if (limit !== undefined) {
    url += `?limit=${limit}`;
  }
  return request.get(url).json;
};

exports.slice = function (server, database, logId, start, stop) {
  let url = `${lh.getServerUrl(server)}/_db/${database}/_api/log/${logId}/slice?start=${start}&stop=${stop}`;
  return request.get(url).json;
};

exports.at = function (server, database, logId, index) {
  let url = `${lh.getServerUrl(server)}/_db/${database}/_api/log/${logId}/entry/${index}`;
  return request.get(url).json;
};

exports.listLogs = function (server, database) {
  let url = `${lh.getServerUrl(server)}/_db/${database}/_api/log`;
  return request.get(url).json;
};
