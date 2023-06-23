/*jshint strict: true */
'use strict';
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

const lh = require("@arangodb/testutils/replicated-logs-helper");
const request = require("@arangodb/request");
const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertNotNull} = jsunity.jsUnity.assertions;

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
  lh.checkRequestResult(res, true);
  return res.json;
};

/**
 * Verify that a key is available (or not) on a server.
 * If available is set to false, the key must not exist.
 * Otherwise, the value at that key must be equal to the given value.
 */
const localKeyStatus = function (endpoint, db, col, key, available, value) {
  return function() {
    const data = getLocalValue(endpoint, db, col, key);
    if (available === false && data.code === 404) {
      return true;
    }
    if (available === true && data._key === key) {
      if (value === undefined) {
        // We are not interested in any value, just making sure the key exists.
        return true;
      }
      if (data.value === value) {
        return true;
      }
    }
    return Error(`Wrong value returned by ${endpoint}/${db}/${col}/${key}, got: ${JSON.stringify(data)}, ` +
      `expected: ${JSON.stringify(value)}`);
  };
};

/**
 * Checks that a given key exists (or not) on all servers.
 * To check for the absence of a key, pass `null` as the value.
 */
const checkFollowersValue = function (servers, db, shardId, logId, key, value, isReplication2) {
  let localValues = {};
  for (const [serverId, endpoint] of Object.entries(servers)) {
    if (value === null) {
      // Check for absence of key
      lh.waitFor(localKeyStatus(endpoint, db, shardId, key, false));
    } else {
      // Check for key and value
      lh.waitFor(localKeyStatus(endpoint, db, shardId, key, true, value));
    }
    localValues[serverId] = getLocalValue(endpoint, db, shardId, key);
  }

  let replication2Log = '';
  if (isReplication2) {
    replication2Log = `Log entries: ${JSON.stringify(lh.dumpLogHead(logId))}`;
  }
  let extraErrorMessage = `All responses: ${JSON.stringify(localValues)}` + `\n${replication2Log}`;

  for (const [serverId, res] of Object.entries(localValues)) {
    if (value === null) {
      assertTrue(res.code === 404,
        `Expected 404 while reading key from ${serverId}/${db}/${shardId}, ` +
        `but the response was ${JSON.stringify(res)}.\n` + extraErrorMessage);
    } else {
      assertTrue(res.code === undefined,
        `Error while reading key from ${serverId}/${db}/${shardId}/${key}, ` +
        `got: ${JSON.stringify(res)}.\n` + extraErrorMessage);
      assertEqual(res.value, value,
        `Wrong value returned by ${serverId}/${db}/${shardId}, expected ${value} but ` +
        `got: ${JSON.stringify(res)}. ` + extraErrorMessage);
    }
  }

  if (value !== null) {
    // All ids and revisions should be equal
    const revs = Object.values(localValues).map(value => value._rev);
    assertTrue(revs.every((val, i, arr) => val === arr[0]), `_rev mismatch ${JSON.stringify(localValues)}` +
      `\n${replication2Log}`);

    const ids = Object.values(localValues).map(value => value._id);
    assertTrue(ids.every((val, i, arr) => val === arr[0]), `_id mismatch ${JSON.stringify(localValues)}` +
      `\n${replication2Log}`);
  }
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
  lh.checkRequestResult(res, true);
  return res.json;
};

const getAssociatedShards = function (endpoint, db, stateId) {
  let res = request.get({
    url: `${endpoint}/_db/${db}/_api/document-state/${stateId}/shards`,
  });
  lh.checkRequestResult(res, false);
  return res.json.result;
};

/**
 * Return a single array with all log entries merged together.
 */
const mergeLogs = function(logs) {
  return logs.reduce((previous, current) => previous.concat(current.head(1000)), []);
};

const getOperation = function(entry) {
  if (entry.hasOwnProperty("payload"))  {
    return entry.payload.operation;
  }
  return null;
};

const getOperationType = function(entry) {
  const op = getOperation(entry);
  if (op === null) {
    return null;
  }
  return op.type;
};

const getOperationPayload = function(entry) {
  const op = getOperation(entry);
  if (op === null) {
    return null;
  }
  return op.payload;
};

/**
 * Returns the first entry with the same key and type as provided.
 * If no key is provided, all entries of the specified type are returned.
 */
const getDocumentEntries = function (entries, type, key) {
  if (key === undefined) {
    let matchingType = [];
    for (const entry of entries) {
      if (getOperationType(entry) === type) {
        matchingType.push(entry);
      }
    }
    return matchingType;
  }

  for (const entry of entries) {
    if (getOperationType(entry) === type) {
      let op = getOperation(entry);
      // replication entries can contain an array of documents (batch op)
      if (Array.isArray(op.payload)) {
        // in this case try to find the document in the batch
        let res = op.payload.filter((doc) => doc._key === key);
        if (res.length === 1) {
          return entry;
        }
      } else if (op.payload._key === key) {
        // single document operation was replicated
        return entry;
      }
    }
  }
  return null;
};

/**
 * Check if all the documents are in the logs and have the provided type.
 */
const searchDocs = function(logs, docs, opType) {
  let allEntries = mergeLogs(logs);
  for (const doc of docs) {
    let entry = getDocumentEntries(allEntries, opType, doc._key);
    assertNotNull(entry);
    assertEqual(getOperationType(entry), opType, `Dumping combined log entries: ${JSON.stringify(allEntries)}`);
  }
};

/**
 * Unroll all array entries from all logs and optionally filter by the name attribute.
 */
const getArrayElements = function(logs, opType, name) {
  let entries = mergeLogs(logs).filter(entry => getOperationType(entry) === opType
    && Array.isArray(getOperationPayload(entry)))
    .reduce((previous, current) => previous.concat(getOperationPayload(current)), []);
  if (name === undefined) {
    return entries;
  }
  return entries.filter(entry => entry.name === name);
};

const startSnapshot = function(endpoint, db, logId, follower, rebootId) {
  return request.post(`${endpoint}/_db/${db}/_api/document-state/${logId}/snapshot/start`,
    {body: {serverId: follower, rebootId: rebootId}, json: true});
};

const getSnapshotStatus = function (endpoint, db, logId, snapshotId) {
  return request.get(`${endpoint}/_db/${db}/_api/document-state/${logId}/snapshot/status/${snapshotId}`);
};

const allSnapshotsStatus = function (endpoint, db, logId) {
  return request.get(`${endpoint}/_db/${db}/_api/document-state/${logId}/snapshot/status`);
};

const getNextSnapshotBatch = function (endpoint, db, logId, snapshotId) {
  return request.post(`${endpoint}/_db/${db}/_api/document-state/${logId}/snapshot/next/${snapshotId}`);
};

const finishSnapshot = function (endpoint, db, logId, snapshotId) {
  return request.delete(`${endpoint}/_db/${db}/_api/document-state/${logId}/snapshot/finish/${snapshotId}`);
};

exports.getLocalValue = getLocalValue;
exports.localKeyStatus = localKeyStatus;
exports.checkFollowersValue = checkFollowersValue;
exports.getBulkDocuments = getBulkDocuments;
exports.mergeLogs = mergeLogs;
exports.getOperation = getOperation;
exports.getOperationType = getOperationType;
exports.getOperationPayload = getOperationPayload;
exports.getDocumentEntries = getDocumentEntries;
exports.searchDocs = searchDocs;
exports.getArrayElements = getArrayElements;
exports.getAssociatedShards = getAssociatedShards;
exports.startSnapshot = startSnapshot;
exports.getSnapshotStatus = getSnapshotStatus;
exports.getNextSnapshotBatch = getNextSnapshotBatch;
exports.finishSnapshot = finishSnapshot;
exports.allSnapshotsStatus = allSnapshotsStatus;
