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

const _ = require("lodash");
const LH = require("@arangodb/testutils/replicated-logs-helper");
const request = require('@arangodb/request');
const spreds = require("@arangodb/testutils/replicated-state-predicates");
const helper = require('@arangodb/test-helper');

const getLocalStatus = function (serverId, database, logId) {
  let url = LH.getServerUrl(serverId);
  const res = request.get(`${url}/_db/${database}/_api/log/${logId}/local-status`);
  LH.checkRequestResult(res);
  return res.json.result;
};

const replaceParticipant = (database, logId, oldParticipant, newParticipant) => {
  const url = LH.getServerUrl(_.sample(LH.coordinators));
  const res = request.post(
    `${url}/_db/${database}/_api/log/${logId}/participant/${oldParticipant}/replace-with/${newParticipant}`
  );
  LH.checkRequestResult(res);
  const {json: {result}} = res;
  return result;
};

/**
 * Returns the value of a key from a server.
 */
const getLocalValue = function (endpoint, db, col, key) {
  let res = request.get({
    url: `${endpoint}/_db/${db}/_api/document/${col}/${key}`,
    headers: {
      "X-Arango-Allow-Dirty-Read": true
    },
  });
  LH.checkRequestResult(res, true);
  return res.json;
};

/**
 * Returns bulk documents from a server collection.
 */
const getBulkDocuments = function (endpoint, db, col, keys) {
  let res = request.put({
    url: `${endpoint}/_db/${db}/_api/document/${col}?onlyget=true`,
    headers: {
      "X-Arango-Allow-Dirty-Read": true
    },
    body: JSON.stringify(keys)
  });
  LH.checkRequestResult(res, true);
  return res.json;
};

exports.getLocalStatus = getLocalStatus;
exports.replaceParticipant = replaceParticipant;
exports.getLocalValue = getLocalValue;
exports.getBulkDocuments = getBulkDocuments;
